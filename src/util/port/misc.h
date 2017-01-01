/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#ifndef _HAVE_PORT_MISC_H
#define _HAVE_PORT_MISC_H

#ifdef HAVE_FEATURES_H
#include <features.h>
#endif

#ifdef __GNU_LIBRARY__
#if __GLIBC__ >= 2 && __GLIBC_MINOR__ >= 1
#define PRI_SIZE_T "zu"
#define PRI_SSIZE_T "zd"
#else
#error Use a newer glibc
#endif

#elif defined(__WINDOWS__) || defined(__MINGW32__)
#define PRI_SIZE_T "Iu"
#define PRI_SSIZE_T "Id"
#else
#error Unsupported system
#endif

#endif
