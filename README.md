# SDL 1.2 GXM port for PS Vita

## Features

- 1.2.16 version bump
- Support of 8/15/16/24/32 bit surfaces
- Hardware accelerated blits and fills
- Front touchpad support (Mouse emulation. Works with scaled/centered images)
- Custom functions for on screen keyboard and message box (Backported from SDL2 Vita port)

## Build instructions

To build and install SDL 1.2 use the following command

```make -f Makefile.vita install```

To enable hardware acceleration use ```VITA_HW_ACCEL=1``` flag

```make -f Makefile.vita VITA_HW_ACCEL=1 install```

You can choose which functions are accelerated by using the following flags together with ```VITA_HW_ACCEL=1```.

`VITA_BLIT_HW=1` Enables accelerated blits (enabled by default).

`VITA_BLIT_HW_A=1` Enables accelerated alpha blits (somewhat limited functionality, disabled by default).

`VITA_FILL_HW=1` Enables accelerated fills (enabled by default).

## Custom functions

This SDL 1.2 port contains few custom functions that extend its functionality and allow some fine-tuning.

```void SDL_VITA_SetVideoModeScaling(int x, int y, float w, float h);```

Sets position of rendering surface and it's dimension.

```void SDL_VITA_SetVideoModeBilinear(int enable_bilinear);```

Enables or disables bilinear filtering on scaled surfaces.

```void SDL_VITA_SetVideoModeSync(int enable_vsync);```

Enables or disables vsync.

```void SDL_VITA_SetWaitGxmFinish(int gxm_wait);```

Enables or disables usage of ```sceGxmFinish``` during screen flip. In most of the cases it's safe to disable this. Doing so will improve performance a bit (but it might result in visual bugs in some games).

```void SDL_VITA_SetTextureAllocMemblockType(SceKernelMemBlockType type);```

Sets type of memory block for all new hardware surface allocations. ```SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW``` is the default one. Depending on a game ```SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE``` or ```SCE_KERNEL_MEMBLOCK_TYPE_USER_RW``` might provide a bit better (or worse) performance. Set memblock type before display/surface creation.

```void SDL_VITA_ShowScreenKeyboard(const char *initialText, bool clearText);```

Opens on-screen keyboard with ```initialText``` pre-filled. Input is done with emulation of key events, so YMMV. When ```clearText``` is set to ```true``` SDL also emulates DEL and BACKSPACE button presses to (hopefully) clear the old text. Maximal number of SDL events is 128, so with ```clearText``` set to ```true``` 64 is allocated to clear the old text (32 backspace, 32 del) and 64 is used for the rest of the emulated input. Maximum of ~128 input characters is supported with ```clearText``` set to ```false```, but you'll have to clear the input field inside the game yourself.

```void SDL_VITA_HideScreenKeyboard();```

Hides on-screen keyboard.

```void SDL_VITA_ShowMessageBox(const char *messageText);```

Displays message box with one button and pre-defined text.

## Performance considerations

Mixed usage of ```SDL_SWSURFACE``` and ```SDL_HWSURFACE``` (for screen/surfaces) might result in decreased performance.

Set ```SDL_VITA_SetWaitGxmFinish(0)``` unless you experience visual bugs (tearing, flickering, disappearing elements).

Hardware surfaces with memblock type ```SCE_KERNEL_MEMBLOCK_TYPE_USER_RW``` or ```SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE``` can provide better performance is case of big number of CPU read/writes on the surface.

Generally performance of ```SDL_HWSURFACE``` is somewhat faster with hardware acceleration enabled.

Alpha blits are disabled by default. They are functional with 32 bpp surfaces, but `SDL_SetAlpha` is currently not implemented and blits may produce a bit different results than expected in some cases.

HW acceleration is disabled with 8-bit surfaces.

## Thanks to:
- isage for [SDL2 gxm port](https://github.com/isage/SDL-mirror)
- xerpi for [libvita2d](https://github.com/xerpi/libvita2d)
- xerpi, Cpasjuste and rsn8887 for [original PS Vita SDL port](https://github.com/rsn8887/SDL-Vita/tree/SDL12)


# DEPRECATED

The 1.2 branch of SDL is deprecated. While we occasionally collect fixes
in revision control, there has not been a formal release since 2012, and
we have no intention to do future releases, either.

Current development is happening in SDL 2.0.x, which gets regular
releases and can be found at:

https://github.com/libsdl-org/SDL

Thanks!



# Simple DirectMedia Layer (SDL) Version 1.2

https://www.libsdl.org/

This is the Simple DirectMedia Layer, a general API that provides low
level access to audio, keyboard, mouse, joystick, 3D hardware via OpenGL,
and 2D framebuffer across multiple platforms.

The current version supports Linux, Windows CE/95/98/ME/XP/Vista, BeOS,
MacOS Classic, Mac OS X, FreeBSD, NetBSD, OpenBSD, BSD/OS, Solaris, IRIX,
and QNX.  The code contains support for Dreamcast, Atari, AIX, OSF/Tru64,
RISC OS, SymbianOS, Nintendo DS, and OS/2, but these are not officially
supported.

SDL is written in C, but works with C++ natively, and has bindings to
several other languages, including Ada, C#, Eiffel, Erlang, Euphoria,
Guile, Haskell, Java, Lisp, Lua, ML, Objective C, Pascal, Perl, PHP,
Pike, Pliant, Python, Ruby, and Smalltalk.

This library is distributed under GNU LGPL version 2, which can be
found in the file  "COPYING".  This license allows you to use SDL
freely in commercial programs as long as you link with the dynamic
library.

The best way to learn how to use SDL is to check out the header files in
the "include" subdirectory and the programs in the "test" subdirectory.
The header files and test programs are well commented and always up to date.
More documentation is available in HTML format in "docs/index.html".

The test programs in the "test" subdirectory are in the public domain.

Enjoy!

Sam Lantinga (slouken@libsdl.org)

