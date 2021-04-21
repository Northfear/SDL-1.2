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
#include "SDL_config.h"

#if SDL_THREAD_VITA

#include "SDL_thread.h"
#include "SDL_systhread_c.h"

#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/error.h>

struct SDL_mutex {
	SceUID uid;
};

/* Create a mutex */
SDL_mutex *SDL_CreateMutex(void)
{
    SDL_mutex *mutex;

    /* Allocate mutex memory */
    mutex = (SDL_mutex *) SDL_malloc(sizeof(*mutex));
    if (mutex) {

        mutex->uid =  sceKernelCreateMutex("SDL mutex",
            SCE_KERNEL_MUTEX_ATTR_RECURSIVE,
            0,
            NULL
        );

        if (mutex->uid <= 0) {
            printf("Error creating mutex: %x\n", mutex->uid);
            SDL_OutOfMemory(); // TODO: proper error
        }
    } else {
        SDL_OutOfMemory();
    }
    return mutex;
}

/* Free the mutex */
void SDL_DestroyMutex(SDL_mutex *mutex)
{
    if (mutex) {
        sceKernelDeleteMutex(mutex->uid);
        SDL_free(mutex);
    }
}

/* Lock the mutex */
int SDL_mutexP(SDL_mutex *mutex)
{
#if SDL_THREADS_DISABLED
    return 0;
#else
    SceInt32 res = 0;
    if (mutex == NULL) {
        SDL_SetError("Passed a NULL mutex");
        return -1;
    }

    res = sceKernelLockMutex(mutex->uid, 1, NULL);
    if (res != SCE_KERNEL_OK) {
        SDL_SetError("Error trying to lock mutex: %x", res);
        return -1;
    }

    return 0;
#endif /* SDL_THREADS_DISABLED */
}

/* Unlock the mutex */
int SDL_mutexV(SDL_mutex *mutex)
{
#if SDL_THREADS_DISABLED
    return 0;
#else
    SceInt32 res = 0;

    if (mutex == NULL) {
        SDL_SetError("Passed a NULL mutex");
        return -1;
    }

    res = sceKernelUnlockMutex(mutex->uid, 1);
    if (res != 0) {
        SDL_SetError("Error trying to unlock mutex: %x", res);
        return -1;
    }

    return 0;
#endif /* SDL_THREADS_DISABLED */
}

#endif // SDL_THREAD_VITA
