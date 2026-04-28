#pragma once

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef	HAVE_FCNTL_H
#include <fcntl.h>
#endif	/* HAVE_FCNTL_H */

#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#endif /* HAVE_INTTYPES_H */

#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif /* HAVE_LIMITS_H */

#ifdef HAVE_STDBOOL_H
#include <stdbool.h>
#else
#define bool int
#define true 1
#define false 0
#endif /* !HAVE_STDBOOL_H */

#ifdef HAVE_STDDEF_H
#include <stddef.h>
#endif /* HAVE_STDDEF_H */

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif /* HAVE_STDINT_H */

#ifdef	HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif	/* HAVE_SYS_TYPES_H */

#ifdef	HAVE_SYS_TIME_H
#include <sys/time.h>
#endif	/* HAVE_SYS_TIME_H */

#ifdef	HAVE_UNISTD_H
#include <unistd.h>
#endif	/* HAVE_UNISTD_H */

#ifdef _WIN32
    /* The windows header defines min and max macros that interfere with std::min and
     * std::max, so we need to define NOMINMAX before including it.
     * NOTE: GoogleTest's header files also include windows.h, include of <gtest/gtest.h>
     * must come after this header to avoid compilation errors.
     */
    #define NOMINMAX
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #define PATH_MAX MAX_PATH
#else
    #ifdef __linux__
    #include <linux/limits.h>
    #endif /* __linux__ */
#endif

/* dynamic memory allocations */
#include "malloc.h"
#define ALLOCATE(type, tag, desc) ((type *)DXALLOC(sizeof(type), tag, desc))
#define CALLOCATE(num, type, tag, desc) ((type *)DXALLOC(sizeof(type[1]) * (num), tag, desc))
#define RESIZE(ptr, num, type, tag, desc) ((type *)DREALLOC((void *)ptr, sizeof(type) * (num), tag, desc))

/* standard C library portability wrappers */
#include "port/wrapper.h"

#ifndef NO_STEM

#ifdef _WIN32
    #define STDIN_FILENO _fileno(stdin)
    #define STDOUT_FILENO _fileno(stdout)
    #include <io.h>
    #define FILE_OPEN       _open
    #define FILE_FDOPEN     _fdopen
    #define FILE_CLOSE      _close
    #define FILE_READ       _read
    #define FILE_WRITE      _write
    #include <direct.h>
    #define CHDIR           _chdir
#else   /* !_WIN32 */
    #define FILE_OPEN   open
    #define FILE_FDOPEN fdopen
    #define FILE_CLOSE  close
    #define FILE_READ   read
    #define FILE_WRITE  write
    #define CHDIR       chdir
#endif  /* !_WIN32 */

#include "port/byte_code.h"
#include "port/debug.h"
#include "stem.h"

#endif /* NO_STEM */
