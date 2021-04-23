/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2020 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

#include "SDL_vitavideo.h"
#include "SDL_vitamessagebox.h"
#include "SDL_render_vita_gxm_tools.h"
#include <psp2/message_dialog.h>


void SDL_VITA_ShowMessageBox(const char *messageText)
{
    SceMsgDialogParam param;
    SceMsgDialogUserMessageParam msgParam;

    SceInt32 init_result;
    SDL_bool setup_minimal_gxm = SDL_FALSE;

    SDL_memset(&param, 0, sizeof(param));
    sceMsgDialogParamInit(&param);
    param.mode = SCE_MSG_DIALOG_MODE_USER_MSG;
    SDL_memset(&msgParam, 0, sizeof(msgParam));
    msgParam.msg = (const SceChar8*)messageText;
    msgParam.buttonType = SCE_MSG_DIALOG_BUTTON_TYPE_OK;
    param.userMsgParam = &msgParam;

    init_result = sceMsgDialogInit(&param);

    // Setup display if it hasn't been initialized before
    if (init_result == SCE_COMMON_DIALOG_ERROR_GXM_IS_UNINITIALIZED)
    {
        gxm_minimal_init_for_common_dialog();
        init_result = sceMsgDialogInit(&param);
        setup_minimal_gxm = SDL_TRUE;
    }

    gxm_init_for_common_dialog();

    if (init_result >= 0)
    {
        while (sceMsgDialogGetStatus() == SCE_COMMON_DIALOG_STATUS_RUNNING)
        {
            gxm_swap_for_common_dialog();
        }
        sceMsgDialogTerm();
    }

    gxm_term_for_common_dialog();

    if (setup_minimal_gxm)
    {
        gxm_minimal_term_for_common_dialog();
    }
}

/* vi: set ts=4 sw=4 expandtab: */
