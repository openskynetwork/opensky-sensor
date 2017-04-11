/* Copyright (c) 2015-2017 OpenSky Network <contact@opensky-network.org> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
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
	int fd = open(file, O_RDONLY | O_CLOEXEC);
	if (fd < 0) {
		LOG_errno(LOG_LEVEL_ERROR, prefix, "Could not open '%s'", file);
		return NULL;
	}

	/* stat input file for its size */
	struct stat st;
	if (fstat(fd, &st) < 0) {
		close(fd);
		LOG_errno(LOG_LEVEL_ERROR, prefix, "Could not stat '%s'", file);
		return NULL;
	}

	/* mmap input file and close file descriptor */
	void * ptr = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
	close(fd);
	if (ptr == MAP_FAILED) {
		LOG_errno(LOG_LEVEL_ERROR, prefix, "Could not mmap '%s'", file);
		return NULL;
	}

	struct MMAP * map = malloc(sizeof *map);
	if (map == NULL)
		LOG_errno(LOG_LEVEL_EMERG, prefix, "Allocation failed");

	map->ptr = ptr;
	map->len = st.st_size;

	return map;
}

void MMAP_close(struct MMAP * map)
{
	/* unmap file */
	munmap(map->ptr, map->len);

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
