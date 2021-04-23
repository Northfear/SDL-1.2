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

/* Being a null driver, there's no event stream. We just define stubs for
   most of the API. */

#include "SDL.h"
#include "../../events/SDL_sysevents.h"
#include "../../events/SDL_events_c.h"

#include "SDL_vitavideo.h"
#include "SDL_vitaevents_c.h"
#include "SDL_vitakeyboard_c.h"
#include "SDL_vitamouse_c.h"
#include "SDL_vitatouch.h"

#include <stdbool.h>
#include <psp2/types.h>
#include <psp2/ime_dialog.h>


SDL_bool screenKeyboardActive = SDL_FALSE;
bool clearTextRequired = true;
SceWChar16 ime_buffer[SCE_IME_DIALOG_MAX_TEXT_LENGTH];

static void utf8_to_utf16(const uint8_t *src, uint16_t *dst)
{
    int i;
    for (i = 0; src[i];) {
        if ((src[i] & 0xE0) == 0xE0) {
            *(dst++) = ((src[i] & 0x0F) << 12) | ((src[i + 1] & 0x3F) << 6) | (src[i + 2] & 0x3F);
            i += 3;
        } else if ((src[i] & 0xC0) == 0xC0) {
            *(dst++) = ((src[i] & 0x1F) << 6) | (src[i + 1] & 0x3F);
            i += 2;
        } else {
            *(dst++) = src[i];
            i += 1;
        }
    }

    *dst = '\0';
}

static void utf16_to_utf8(const uint16_t *src, uint8_t *dst) {
    int i;
    for (i = 0; src[i]; i++) {
        if ((src[i] & 0xFF80) == 0) {
            *(dst++) = src[i] & 0xFF;
        } else if((src[i] & 0xF800) == 0) {
            *(dst++) = ((src[i] >> 6) & 0xFF) | 0xC0;
            *(dst++) = (src[i] & 0x3F) | 0x80;
        } else if((src[i] & 0xFC00) == 0xD800 && (src[i + 1] & 0xFC00) == 0xDC00) {
            *(dst++) = (((src[i] + 64) >> 8) & 0x3) | 0xF0;
            *(dst++) = (((src[i] >> 2) + 16) & 0x3F) | 0x80;
            *(dst++) = ((src[i] >> 4) & 0x30) | 0x80 | ((src[i + 1] << 2) & 0xF);
            *(dst++) = (src[i + 1] & 0x3F) | 0x80;
            i += 1;
        } else {
            *(dst++) = ((src[i] >> 12) & 0xF) | 0xE0;
            *(dst++) = ((src[i] >> 6) & 0x3F) | 0x80;
            *(dst++) = (src[i] & 0x3F) | 0x80;
        }
    }

    *dst = '\0';
}

#if !defined(SCE_IME_LANGUAGE_ENGLISH_US)
#define SCE_IME_LANGUAGE_ENGLISH_US SCE_IME_LANGUAGE_ENGLISH
#endif

void SDL_VITA_ShowScreenKeyboard(const char *initialText, bool clearText)
{
    SceWChar16 title[SCE_IME_DIALOG_MAX_TITLE_LENGTH];
    SceWChar16 text[SCE_IME_DIALOG_MAX_TEXT_LENGTH];
    SDL_memset(&title, 0, sizeof(title));
    SDL_memset(&text, 0, sizeof(text));
    utf8_to_utf16((const uint8_t *)initialText, text);

    SceInt32 res;

    SceImeDialogParam param;
    sceImeDialogParamInit(&param);

    param.supportedLanguages = SCE_IME_LANGUAGE_ENGLISH_US;
    param.languagesForced = SCE_FALSE;
    param.type = SCE_IME_TYPE_DEFAULT;
    param.option = 0;
    param.textBoxMode = SCE_IME_DIALOG_TEXTBOX_MODE_WITH_CLEAR;
    param.maxTextLength = SCE_IME_DIALOG_MAX_TEXT_LENGTH;

    param.title = title;
    param.initialText = text;
    param.inputTextBuffer = ime_buffer;

    res = sceImeDialogInit(&param);
    if (res < 0) {
        SDL_SetError("Failed to init IME dialog");
        return;
    }

    clearTextRequired = clearText;
    screenKeyboardActive = SDL_TRUE;
}

void SDL_VITA_HideScreenKeyboard()
{
    SceCommonDialogStatus dialogStatus = sceImeDialogGetStatus();

    switch (dialogStatus) {
        default:
        case SCE_COMMON_DIALOG_STATUS_NONE:
        case SCE_COMMON_DIALOG_STATUS_RUNNING:
                break;
        case SCE_COMMON_DIALOG_STATUS_FINISHED:
                sceImeDialogTerm();
                break;
    }

    screenKeyboardActive = SDL_FALSE;
}

void VITA_PumpEvents(_THIS)
{
    VITA_PollKeyboard();
    VITA_PollMouse();
    VITA_PollTouch();

    if (screenKeyboardActive == SDL_TRUE) {
        // update IME status. Terminate, if finished
        SceCommonDialogStatus dialogStatus = sceImeDialogGetStatus();
         if (dialogStatus == SCE_COMMON_DIALOG_STATUS_FINISHED) {
            uint8_t utf8_buffer[SCE_IME_DIALOG_MAX_TEXT_LENGTH];

            SceImeDialogResult result;
            SDL_memset(&result, 0, sizeof(SceImeDialogResult));
            sceImeDialogGetResult(&result);

            // Convert UTF16 to UTF8
            utf16_to_utf8(ime_buffer, utf8_buffer);

            // clear anything that could possibly be there in the most brutal way possible
            // MAXEVENTS is 128, so let's just do 32 backspaces and dels and leave the rest 64 for text and anything else
            // idealy it should be done in application itself
            if (clearTextRequired) {
                for (int i = 0; i < 32; i++) {
                    SDL_Event ev_bksp;
                    ev_bksp.type = SDL_KEYDOWN;
                    ev_bksp.key.state = SDL_PRESSED;
                    ev_bksp.key.keysym.mod = KMOD_NONE;
                    ev_bksp.key.keysym.sym = SDLK_BACKSPACE;
                    SDL_PushEvent(&ev_bksp);

                    SDL_Event ev_del;
                    ev_del.type = SDL_KEYDOWN;
                    ev_del.key.state = SDL_PRESSED;
                    ev_del.key.keysym.mod = KMOD_NONE;
                    ev_del.key.keysym.sym = SDLK_DELETE;
                    SDL_PushEvent(&ev_del);
                }
            }

            // emulate key presses. callback functions? ain't nobody got time for that
            for (int i = 0; utf8_buffer[i]; i++) {
                SDL_Event ev;
                ev.type = SDL_KEYDOWN;
                ev.key.state = SDL_PRESSED;
                ev.key.keysym.mod = KMOD_NONE;
                ev.key.keysym.sym = SDLK_UNKNOWN;
                ev.key.keysym.unicode = utf8_buffer[i];
                SDL_PushEvent(&ev);
            }

            sceImeDialogTerm();
            screenKeyboardActive = SDL_FALSE;
        }
    }
}

void VITA_InitOSKeymap(_THIS)
{
    /* do nothing. */
}

/* end of SDL_vitaevents.c ... */
