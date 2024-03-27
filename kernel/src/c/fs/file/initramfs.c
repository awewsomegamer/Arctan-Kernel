/**
 * @file initramfs.c
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
#include <abi-bits/errno.h>
#include "lib/perms.h"
#include <fs/vfs.h>
#include <mm/allocator.h>
#include <lib/resource.h>
#include <global.h>
#include <time.h>
#include <util.h>
#include <sys/stat.h>

#define ARC_NAME_OFFSET (sizeof(struct ARC_HeaderCPIO))
#define ARC_NAME_SIZE(header) (header->namesize + (header->namesize & 1))
#define ARC_DATA_OFFSET(header) (ARC_NAME_OFFSET + ARC_NAME_SIZE(header))
#define ARC_DATA_SIZE(header) (((header->filesize[0] << 16) | header->filesize[1]) + (((header->filesize[0] << 16) | header->filesize[1]) & 1))

struct ARC_HeaderCPIO {
	uint16_t magic;
	uint16_t device;
	uint16_t inode;
	uint16_t mode;
	uint16_t uid;
	uint16_t gid;
	uint16_t nlink;
	uint16_t rdev;
	uint16_t mod_time[2];
	uint16_t namesize;
	uint16_t filesize[2];
}__attribute__((packed));

void *Arc_FindFileInitramfs(void *fs, char *filename) {
	if (fs == NULL || filename == NULL) {
		ARC_DEBUG(ERR, "Either fs %p or filename %p is NULL\n", fs, filename);
		return NULL;
	}

	struct ARC_HeaderCPIO *header = (struct ARC_HeaderCPIO *)fs;
	uint64_t offset = 0;

	while (header->magic == 0070707) {
		char *name = ((char *)header) + ARC_NAME_OFFSET;

		if (strcmp(name, filename) != 0) {
			goto next;
		}

		ARC_DEBUG(INFO, "Found file \"%s\"\n", name);

		return (void *)header;

		next:;
		offset += ARC_DATA_OFFSET(header) + ARC_DATA_SIZE(header);
		header = (struct ARC_HeaderCPIO *)(fs + offset);

	}

	ARC_DEBUG(ERR, "Could not find file \"%s\"\n", filename);

	return NULL;
}

int initramfs_internal_stat(struct ARC_HeaderCPIO *header, struct stat *stat) {
	stat->st_uid = header->uid;
	stat->st_gid = header->gid;
	stat->st_mode = header->mode;
	stat->st_dev = header->device;
	stat->st_ino = header->inode;
	stat->st_nlink = header->nlink;
	stat->st_rdev = header->rdev;
	stat->st_size = ARC_DATA_SIZE(header);
	stat->st_mtim.tv_nsec = 0;
	stat->st_mtim.tv_sec = (header->mod_time[0] << 16) | header->mod_time[1];

	return 0;
}

int initramfs_empty() {
	return 0;
}

int initramfs_init(struct ARC_Resource *res, void *args) {
	res->driver_state = args;

	return 0;
}

int initramfs_uninit() {
	return 0;
}

int initramfs_open(struct ARC_Resource *res, char *path, int flags, uint32_t mode) {
	if (Arc_CheckCurPerms(mode) != 0) {
		return EPERM;
	}

	struct ARC_VFSFile *spec = res->vfs_state;

	struct ARC_HeaderCPIO *header = Arc_FindFileInitramfs(res->driver_state, path);

	if (header == NULL) {
		ARC_DEBUG(ERR, "Failed to open file\n");

		return 1;
	}

	spec->size = (header->filesize[0] << 16) | header->filesize[1];
	spec->address = (void *)header;

	initramfs_internal_stat(header, &spec->stat);

	return 0;
}

int initramfs_read(void *buffer, size_t size, size_t count, struct ARC_Resource *res) {
	struct ARC_VFSFile *spec = res->vfs_state;

	if (spec->address == NULL) {
		return 0;
	}

	struct ARC_HeaderCPIO *header = (struct ARC_HeaderCPIO *)spec->address;

	uint8_t *data = (uint8_t *)(spec->address + ARC_DATA_OFFSET(header));

	// Copy file data to buffer
	for (size_t i = 0; i < size * count; i++) {
		uint8_t value = 0;

		if (i + spec->offset < spec->size) {
			value = *((uint8_t *)(data + spec->offset + i));
		}

		*((uint8_t *)(buffer + i)) = value;
	}

	return count;
}

int initramfs_write() {
	ARC_DEBUG(ERR, "Read only file system\n");

	return 1;
}

int initramfs_seek(struct ARC_Resource *res, long offset, int whence) {
	struct ARC_VFSFile *spec = res->vfs_state;

	switch (whence) {
	case ARC_VFS_SEEK_SET: {
		spec->offset = offset;

		return 0;
	}

	case ARC_VFS_SEEK_CUR: {
		spec->offset += offset;

		if (spec->offset >= spec->size) {
			spec->offset = spec->size;
		}

		return 0;
	}

	case ARC_VFS_SEEK_END: {
		spec->offset = spec->size - offset - 1;

		if (spec->offset < 0) {
			spec->offset = 0;
		}

		return 0;
	}
	}

	return 0;
}

int initramfs_stat(struct ARC_Resource *res, char *filename, struct stat *stat) {
	if (res == NULL || filename == NULL || stat == NULL) {
		return 1;
	}

	struct ARC_HeaderCPIO *header = Arc_FindFileInitramfs(res->driver_state, filename);

	if (header == NULL) {
		return 1;
	}

	return initramfs_internal_stat(header, stat);
}

struct ARC_SuperDriverDef initramfs_super_spec = {
	.create = initramfs_empty,
	.remove = initramfs_empty,
	.link = initramfs_empty,
	.rename = initramfs_empty,
	.stat = initramfs_stat,
};

ARC_REGISTER_DRIVER(0, initramfs_super) = {
	.index = 0,
	.init = initramfs_init,
	.uninit = initramfs_uninit,
	.open = initramfs_open,
	.close = initramfs_empty,
	.read = initramfs_read,
	.write = initramfs_write,
	.seek = initramfs_seek,
	.identifer = ARC_DRIVER_IDEN_SUPER,
	.driver = (void *)&initramfs_super_spec,
};

ARC_REGISTER_DRIVER(0, initramfs_file) = {
	.index = 1,
	.init = initramfs_init,
	.uninit = initramfs_uninit,
	.open = initramfs_open,
	.close = initramfs_empty,
	.read = initramfs_read,
	.write = initramfs_write,
	.seek = initramfs_seek,
};
