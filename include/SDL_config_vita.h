/*
    SDL - Simple DirectMedia Layer
    Copyright (C) 1997-2012 Sam Lantinga

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

    Sam Lantinga
    slouken@libsdl.org
*/

#ifndef _SDL_CONFIG_VITA_h
#define _SDL_CONFIG_VITA_h

#include "SDL_platform.h"

/* This is the minimal configuration that can be used to build SDL */

#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>

#define __VITA__ 1

#ifdef VITA_HW_ACCEL
#ifndef VITA_BLIT_HW
#define VITA_BLIT_HW 1
#endif
#ifndef VITA_BLIT_HW_A
#define VITA_BLIT_HW_A 1
#endif
#ifndef VITA_FILL_HW
#define VITA_FILL_HW 1
#endif
#else
#undef VITA_BLIT_HW
#undef VITA_BLIT_HW_A
#undef VITA_FILL_HW
#define VITA_BLIT_HW 0
#define VITA_BLIT_HW_A 0
#define VITA_FILL_HW 0
#endif

typedef enum {
    VITA_MEM_VRAM, // CDRAM
    VITA_MEM_RAM, // USER_RW_UNCACHE RAM
    VITA_MEM_PHYCONT, // PHYCONT_NC RAM
    VITA_MEM_RAM_CACHED // USER_RW
} VitaMemType;

#ifdef __cplusplus
extern "C" {
#endif
// custom ps vita functions
void SDL_VITA_SetVideoModeScaling(int x, int y, float w, float h);
void SDL_VITA_SetVideoModeBilinear(int enable_bilinear);
void SDL_VITA_SetVideoModeSync(int enable_vsync);
void SDL_VITA_SetTextureAllocMemblockType(VitaMemType type);
void SDL_VITA_ShowScreenKeyboard(const char *initialText, bool clearText);
void SDL_VITA_HideScreenKeyboard();
void SDL_VITA_ShowMessageBox(const char *messageText);
#ifdef __cplusplus
}
#endif

typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef signed short int16_t;
typedef unsigned short uint16_t;
typedef unsigned int size_t;

#define SDL_HAS_64BIT_TYPE 1

#define HAVE_GCC_ATOMICS    1

#define HAVE_ALLOCA_H       1
#define HAVE_SYS_TYPES_H    1
#define HAVE_STDIO_H    1
#define STDC_HEADERS    1
#define HAVE_STRING_H   1
#define HAVE_INTTYPES_H 1
#define HAVE_STDINT_H   1
#define HAVE_CTYPE_H    1
#define HAVE_MATH_H 1
#define HAVE_SIGNAL_H   1

/* C library functions */
#define HAVE_MALLOC 1
#define HAVE_CALLOC 1
#define HAVE_REALLOC    1
#define HAVE_FREE   1
#define HAVE_ALLOCA 1
#define HAVE_GETENV 1
#define HAVE_SETENV 1
#define HAVE_PUTENV 1
#define HAVE_SETENV 1
#define HAVE_UNSETENV   1
#define HAVE_QSORT  1
#define HAVE_ABS    1
#define HAVE_BCOPY  1
#define HAVE_MEMSET 1
#define HAVE_MEMCPY 1
#define HAVE_MEMMOVE    1
#define HAVE_MEMCMP 1
#define HAVE_STRLEN 1
#define HAVE_STRLCPY    1
#define HAVE_STRLCAT    1
#define HAVE_STRDUP 1
#define HAVE_STRCHR 1
#define HAVE_STRRCHR    1
#define HAVE_STRSTR 1
#define HAVE_STRTOL 1
#define HAVE_STRTOUL    1
#define HAVE_STRTOLL    1
#define HAVE_STRTOULL   1
#define HAVE_STRTOD 1
#define HAVE_ATOI   1
#define HAVE_ATOF   1
#define HAVE_STRCMP 1
#define HAVE_STRNCMP    1
#define HAVE_STRCASECMP 1
#define HAVE_STRNCASECMP 1
#define HAVE_VSSCANF 1
#define HAVE_VSNPRINTF  1
#define HAVE_M_PI   1
#define HAVE_ATAN   1
#define HAVE_ATAN2  1
#define HAVE_ACOS  1
#define HAVE_ASIN  1
#define HAVE_CEIL   1
#define HAVE_COPYSIGN   1
#define HAVE_COS    1
#define HAVE_COSF   1
#define HAVE_FABS   1
#define HAVE_FLOOR  1
#define HAVE_LOG    1
#define HAVE_POW    1
#define HAVE_SCALBN 1
#define HAVE_SIN    1
#define HAVE_SINF   1
#define HAVE_SQRT   1
#define HAVE_SQRTF  1
#define HAVE_TAN    1
#define HAVE_TANF   1
#define HAVE_SETJMP 1
#define HAVE_NANOSLEEP  1
/* #define HAVE_SIGACTION    1 */

/* VITA isn't that sophisticated */
#define LACKS_SYS_MMAN_H 1

/* Enable the stub cdrom driver (src/cdrom/dummy/\*.c) */
#define SDL_CDROM_DISABLED	1

/* Enable the stub shared object loader (src/loadso/dummy/\*.c) */
#define SDL_LOADSO_DISABLED	1

/* Enable the vita joystick driver (src/joystick/vita/\*.c) */
#define SDL_JOYSTICK_VITA	1

/* Enable the vita audio driver (src/audio/vita/\*.c) */
#define SDL_AUDIO_DRIVER_VITA	1

/* Enable the vita thread support (src/thread/vita/\*.c) */
#define SDL_THREAD_VITA	1

/* Enable the vita timer support (src/timer/vita/\*.c) */
#define SDL_TIMER_VITA	1

/* Enable the vita video driver (src/video/vita/\*.c) */
#define SDL_VIDEO_DRIVER_VITA	1

/* enable optimized blitters */
#define SDL_ARM_SIMD_BLITTERS 1
#define SDL_ARM_NEON_BLITTERS 1

#endif /* _SDL_CONFIG_VITA_h */
