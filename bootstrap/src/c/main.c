#include "include/multiboot2.h"
#include "include/interface.h"
#include "include/global.h"
#include "include/gdt.h"
#include "include/elf.h"

#include <cpuid.h>

uint8_t *tags = NULL;
uint8_t *tags_end = NULL;
uint32_t cur_tag_sz = 0;

const char *kernel_module_name = "arctan-module.kernel.efi";

uint32_t kernel_phys_start = 0x0;
uint32_t kernel_phys_end = 0x0;
uint64_t *kernel_info = NULL;

uint64_t mem_phys_first_free = 0x0;
uint64_t size_phys_first_free = 0x0;
uint32_t bootstrap_start = 0x0;
extern uint8_t __BOOTSTRAP_END__; // Address should be aligned to 0x1000

struct multiboot_tag_framebuffer *framebuffer_tag = NULL;

const char *mem_types[] = {
	[MULTIBOOT_MEMORY_AVAILABLE] = "Available",
	[MULTIBOOT_MEMORY_ACPI_RECLAIMABLE] = "ACPI Reclaimable",
	[MULTIBOOT_MEMORY_BADRAM] = "Bad",
	[MULTIBOOT_MEMORY_NVS] = "NVS",
	[MULTIBOOT_MEMORY_RESERVED] = "Reserved",
};

uint64_t pml4[512] __attribute__((aligned(0x1000))); // PML4 Entries
// Paging tables (Higher Half)
// These should be temporary, to jump to 64-bit mode
uint64_t pml3_kernel[512] __attribute__((aligned(0x1000))); // Page Directory Pointer Entries * Cast this to a uint32_t * so we can create a PML3 table when we enter
uint64_t pml2_kernel[512] __attribute__((aligned(0x1000))); // Page Directory Entries
uint64_t pml1_kernel[512] __attribute__((aligned(0x1000))); // Page Table Entries

// Paging tables (Bootstrapper)
uint64_t pml3_boot[512] __attribute__((aligned(0x1000))); // Page Directory Pointer Entries
uint64_t pml2_boot[512] __attribute__((aligned(0x1000))); // Page Directory Entries
uint64_t pml1_boot[512] __attribute__((aligned(0x1000))); // Page Table Entries

void cpu_checks() {
	// Preform checks
	uint32_t __eax, __ebx, __ecx, __edx;

	__cpuid(0x80000001, __eax, __ebx, __ecx, __edx);

	// Check for LM
	if (((__edx >> 29) & 1) == 0) {
		printf(" CPU not up to scratch! 0x%X 0x%X 0x%X 0x%X (0x80000001)\n", __eax, __ebx, __ecx, __edx);
		__asm__("hlt");
	}

	__cpuid(1, __eax, __ebx, __ecx, __edx);

	// Check for PAE
	if (((__edx >> 6) & 1) == 0) {
		printf(" CPU not up to scratch! 0x%X 0x%X 0x%X 0x%X (0x01)\n", __eax, __ebx, __ecx, __edx);
		__asm__("hlt");
	}
}

void read_tags(uint8_t *boot_info) {
	uint32_t total_size = *(uint32_t *)(boot_info);
	tags_end = boot_info + total_size;
	tags = boot_info + 8;

	int tag = 0;
	do {
		tag = *(uint32_t*)(tags);
		cur_tag_sz = ALIGN(*(uint32_t *)(tags + 4), 8); 

		switch (tag) {
		case MULTIBOOT_TAG_TYPE_BOOT_LOADER_NAME: {
			printf("Bootloader: %s\n", tags + 8);
			
			break;
		}

		case MULTIBOOT_TAG_TYPE_BASIC_MEMINFO: {
			struct multiboot_tag_basic_meminfo *info = (struct multiboot_tag_basic_meminfo *)tags;
			
			printf("Basic Memory Info:\n	Lower: 0x%X KiB\n	Upper: 0x%X KiB\n", info->mem_lower, info->mem_upper);

			break;
		}

		case MULTIBOOT_TAG_TYPE_MODULE: {
			struct multiboot_tag_module *info = (struct multiboot_tag_module *)tags;

			printf("Module: \"%s\" (0x%X -> 0x%X)\n", info->cmdline, info->mod_start, info->mod_end);

			if (strcmp(info->cmdline, kernel_module_name) == 0) {
				printf("! FOUND KERNEL !\n");

				kernel_phys_start = info->mod_start;
				kernel_phys_end = info->mod_end;

				kernel_info = load_elf(info->mod_start);
				printf("%s\n", ((kernel_info[0] != 0) ? "Parsed Kernel ELF!" : "Failed to parse Kernel ELF"));
			}

			break;
		}

		// SAVE THIS STRUCTURE
		// PASS IT TO 64-BIT KERNEL
		case MULTIBOOT_TAG_TYPE_MMAP: {
			struct multiboot_tag_mmap *info = (struct multiboot_tag_mmap *)tags;

			printf("Detailed Memory Map (Version: %d):\n", info->entry_version);

			int i = 1;
			for (uint8_t *entry_base = tags + 16; entry_base < tags + cur_tag_sz; entry_base += info->entry_size, i++) {
				struct multiboot_mmap_entry *entry = (struct multiboot_mmap_entry *)entry_base;

				if (entry->type == MULTIBOOT_MEMORY_AVAILABLE && entry->len > size_phys_first_free) {
					mem_phys_first_free = entry->addr;
					size_phys_first_free = entry->len;
				}

				printf("\tEntry %d: @ 0x%8X%8X, 0x%8X%8X B, Type: %s (%d)\n", i, (uint32_t)(entry->addr >> 32), (uint32_t)entry->addr, (uint32_t)(entry->len >> 32), (uint32_t)entry->len, mem_types[entry->type], entry->type);
			}

			break;
		}

		case MULTIBOOT_TAG_TYPE_LOAD_BASE_ADDR: {
			struct multiboot_tag_load_base_addr *info = (struct multiboot_tag_load_base_addr *)tags;

			bootstrap_start = info->load_base_addr;

			printf("Entry: 0x%X, 0x%X\n", info->load_base_addr , &__BOOTSTRAP_END__);

			break;
		}

		case MULTIBOOT_TAG_TYPE_FRAMEBUFFER: {
			if (framebuffer_tag == NULL) {
				framebuffer_tag = (struct multiboot_tag_framebuffer *)tags;
				tags = boot_info + 8;

				continue;
			}
		}
		}

		tags += cur_tag_sz;
	} while (tag);
}

int helper(uint8_t *boot_info, uint32_t magic) {
	if (magic != 0x36D76289)
		return 1;

	cpu_checks();
	install_gdt();
	read_tags(boot_info);
	
	// Assert the kernel exists (should not be located at 0x0000)
	ASSERT(kernel_phys_start != 0)
	ASSERT((kernel_phys_start & 0xFFF) == 0) // Ensure kernel is page aligned
	ASSERT(kernel_phys_end != 0)

	// Assert we have some free memory to map
	ASSERT(mem_phys_first_free != 0)
	ASSERT(size_phys_first_free != 0)

	printf("%dx%dx%d %8X%8X(%d)\n", framebuffer_tag->common.framebuffer_width, framebuffer_tag->common.framebuffer_height, framebuffer_tag->common.framebuffer_bpp, (uint32_t)(framebuffer_tag->common.framebuffer_addr >> 32), (uint32_t)(framebuffer_tag->common.framebuffer_addr), 0, framebuffer_tag->common.framebuffer_type);

	printf("All is well, kernel module is located at 0x%8X.\nGoing to poke into free RAM at 0x%8X.\n", (uint32_t)kernel_phys_start, (uint32_t)mem_phys_first_free);

	printf("PML4[%X] PML3_kernel[%X] PML2_kernel[%X]\n", (kernel_info[1] >> 39) & 0x1FF, (kernel_info[1] >> 30) & 0x1FF, (kernel_info[1] >> 21) & 0x1FF);

	pml4       [(kernel_info[1] >> 39) & 0x1FF] = (uintptr_t)pml3_kernel | 3; // Address of next entry | RW | P 
	pml3_kernel[(kernel_info[1] >> 30) & 0x1FF] = (uintptr_t)pml2_kernel | 3; // Address of next entry | RW | P
	pml2_kernel[(kernel_info[1] >> 21) & 0x1FF] = (uintptr_t)pml2_kernel | 3; // Address of next entry | RW | P

	// TODO, ensure that we are not relying on a single
	// long section of memory.
	if ((kernel_phys_start >= mem_phys_first_free) && ((kernel_phys_end - kernel_phys_start) < size_phys_first_free)) {
		// Kernel is located in our desired free memory
		uint64_t phys_addr = kernel_phys_start + kernel_info[0];
		for (int i = 0; i < 512; i++) {
			pml1_kernel[i] = phys_addr | 3; // Address | RW | P

			if (i == 0)
				printf("%8X\n", phys_addr);

			phys_addr += 0x1000; // Goto next page
		}

		printf("Ideal conditions met, kernel mapped to %8X%8X. %d bytes available\n", (uint32_t)(kernel_info[1] >> 32), (uint32_t)kernel_info[1], 512 * 0x1000);
	} else {
		// Kernel is not located in our desired free memory
		uint64_t phys_addr = kernel_phys_start + kernel_info[0];
		size_t kernel_size_pages = ALIGN((kernel_phys_end - kernel_phys_start), 0x1000) >> 12;

		int i = 0;
		for (; i < kernel_size_pages; i++) {
			pml1_kernel[i] = phys_addr | 3; // Address | RW | P
			phys_addr += 0x1000; // Goto next page
		}

		phys_addr = mem_phys_first_free;
		int ram_page = i;
		for (; i < 512; i++) {
			pml1_kernel[i] = phys_addr | 3; // Address | RW | P
			phys_addr += 0x1000; // Goto next page
		}

		printf("Kernel is independent from free memory. Mapping kernel to 0x%8X%8X for %d pages. Mapping 0x%8X%8X to 0x%8X%8X.\n", (uint32_t)(kernel_info[1] >> 32), (uint32_t)kernel_info[1],
																	        kernel_size_pages,
																		(uint32_t)(mem_phys_first_free >> 32),
																		(uint32_t)(mem_phys_first_free),
																		(uint32_t)((kernel_info[1] + ram_page * 0x1000) >> 32),
																		(uint32_t)(kernel_info[1] + ram_page * 0x1000));
	}

	pml4     [0] = (uintptr_t)pml3_boot | 3; // Address of next entry | RW | P 
	pml3_boot[0] = (uintptr_t)pml2_boot | 3; // Address of next entry | RW | P
	pml2_boot[0] = (uintptr_t)pml1_boot | 3; // Address of next entry | RW | P

	for (int i = 0; i < 512; i++)
		pml1_boot[i] = ((i) << 12) | 3; // RW | P

	printf("Identity mapped the bootstrapper.\n");

	return 0;
}