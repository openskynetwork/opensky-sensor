/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#ifndef _HAVE_PORT_MMAP_H
#define _HAVE_PORT_MMAP_H

#ifdef __cplusplus
extern "C" {
#endif

struct MMAP;

struct MMAP * MMAP_open(const char * prefix, const char * file);

void MMAP_close(struct MMAP * map);

const void * MMAP_getPtr(const struct MMAP * map);

size_t MMAP_getSize(const struct MMAP * map);

#ifdef __cplusplus
}
#endif

#endif

