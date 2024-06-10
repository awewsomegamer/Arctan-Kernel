/**
 * @file acpi.h
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
#ifndef ARC_ARCH_X86_64_ACPI_H
#define ARC_ARCH_X86_64_ACPI_H

#include <stdint.h>
#include <stddef.h>

#define ARC_DRI_ACPI  0x03
#define ARC_DRI_IRSDT 0x00
#define ARC_DRI_IAPIC 0x01
#define ARC_DRI_IFADT 0x02
#define ARC_DRI_IDSDT 0x03
#define ARC_DRI_IHPET 0x04

struct ARC_RSDTBaseEntry {
	char signature[4];
	uint32_t length;
	uint8_t revision;
	uint8_t checksum;
	uint8_t OEMID[6];
	uint8_t OEMTID[8];
	uint32_t OEMREV;
	char creator_id[4];
	uint32_t creator_rev;
}__attribute__((packed));

int Arc_ChecksumACPI(void *data, size_t length);
int Arc_InitializeACPI(uint64_t rsdp_ptr);

#endif
