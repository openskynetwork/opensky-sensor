/* Copyright (c) 2015-2017 OpenSky Network <contact@opensky-network.org> */

#include <windows.h>
#include <unistd.h>
#include <stdlib.h>
#include "mmap.h"
#include "../log.h"

struct MMAP {
	size_t len;
	void * ptr;
};

struct MMAP * MMAP_open(const char * prefix, const char * file)
{
	/* open input file */
	HANDLE fd = CreateFile(file, GENERIC_READ, 0, NULL, OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL, NULL);
	if (fd == NULL) {
		LOG_logf(LOG_LEVEL_ERROR, prefix, "Could not open '%s'", file);
		return NULL;
	}

	DWORD fileSize = GetFileSize(fd,  NULL);

	/* mmap input file and close file descriptor */
	HANDLE hMap = CreateFileMapping(fd, NULL, PAGE_READONLY, 0, fileSize, NULL);
	CloseHandle(fd);
	if (hMap == NULL)
		goto nommap;
	void * ptr = MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, fileSize);
	CloseHandle(hMap);
	if (ptr == NULL) {
nommap:
		LOG_logf(LOG_LEVEL_ERROR, prefix, "Could not mmap '%s'", file);
		return NULL;
	}

	struct MMAP * map = malloc(sizeof *map);
	if (map == NULL)
		LOG_errno(LOG_LEVEL_EMERG, prefix, "Allocation failed");

	map->ptr = ptr;
	map->len = fileSize;

	return map;
}

void MMAP_close(struct MMAP * map)
{
	/* unmap file */
	UnmapViewOfFile(map->ptr);

	free(map);
}

const void * MMAP_getPtr(const struct MMAP * map)
{
	return map->ptr;
}

size_t MMAP_getSize(const struct MMAP * map)
{
	return map->len;
}
