/**
 * @file apic.c
 *
 * @author awewsomegamer <awewsomegamer@gmail.com>
 *
 * @LICENSE
 * Arctan - Operating System Kernel
 * Copyright (C) 2023-2024 awewsomegamer
 *
 * This file is part of Arctan.
 *
 * Arctan is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * @DESCRIPTION
*/
#include <arch/x86-64/apic/apic.h>
#include <arch/x86-64/apic/lapic.h>
#include <mm/slab.h>
#include <fs/vfs.h>
#include <global.h>

#define ENTRY_TYPE_LAPIC               0x00
#define ENTRY_TYPE_IOAPIC              0x01
#define ENTRY_TYPE_INT_OVERRIDE_SRC    0x02
#define ENTRY_TYPE_NMI_SOURCE          0x03
#define ENTRY_TYPE_LAPIC_NMI           0x04
#define ENTRY_TYPE_LAPIC_ADDR_OVERRIDE 0x05
#define ENTRY_TYPE_IOSAPIC             0x06
#define ENTRY_TYPE_LSAPIC              0x07
#define ENTRY_TYPE_PIS                 0x08
#define ENTRY_TYPE_Lx2APIC             0x09
#define ENTRY_TYPE_Lx2APIC_NMI         0x0A
#define ENTRY_TYPE_GICC                0x0B
#define ENTRY_TYPE_GICD                0x0C
#define ENTRY_TYPE_GIC_MSI             0x0D
#define ENTRY_TYPE_GICR                0x0E
#define ENTRY_TYPE_ITS                 0x0F
#define ENTRY_TYPE_MP_WAKEUP           0x10

int Arc_InitAPIC() {
	struct ARC_File *apic = NULL;
	Arc_OpenVFS("/dev/acpi/rsdt/apic", 0, 0, 0, (void *)&apic);

	if (apic == NULL) {
		return -1;
	}

	uint8_t data[2] = { 0 };

	while (Arc_ReadVFS(data, 1, 2, apic) > 0) {
		switch (data[0]) {
		case ENTRY_TYPE_LAPIC: {
			ARC_DEBUG(INFO, "LAPIC found\n");
			break;
		}

		case ENTRY_TYPE_IOAPIC: {
			ARC_DEBUG(INFO, "IOAPIC found\n");
			break;
		}

		case ENTRY_TYPE_INT_OVERRIDE_SRC: {
			ARC_DEBUG(INFO, "Interrupt Source Override found\n");
			break;
		}

		case ENTRY_TYPE_LAPIC_NMI: {
			ARC_DEBUG(INFO, "LAPIC NMI found\n");
			break;
		}

		default: {
			ARC_DEBUG(INFO, "Unhandled MADT entry of type %d\n", data[0]);
			break;
		}
		}

		Arc_SeekVFS(apic, data[1] - 2, ARC_VFS_SEEK_CUR);
	}

	Arc_CloseVFS(apic);

	Arc_InitLAPIC();
	return 0;
}
