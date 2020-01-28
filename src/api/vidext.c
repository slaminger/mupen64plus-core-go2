/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus-core - api/vidext.c                                       *
 *   Mupen64Plus homepage: https://mupen64plus.org/                        *
 *   Copyright (C) 2009 Richard Goedeken                                   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       * 
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.          *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
                       
/* This file contains the Core video extension functions which will be exported
 * outside of the core library.
 */

#include <SDL.h>
#include <stdlib.h>
#include <string.h>

#define M64P_CORE_PROTOTYPES 1
#include "osal/preproc.h"
#include "../osd/osd.h"
#include "callbacks.h"
#include "m64p_types.h"
#include "m64p_vidext.h"
#include "vidext.h"

#include <display.h>
#include <drm/drm_fourcc.h>

#if SDL_VERSION_ATLEAST(2,0,0)
    #ifndef USE_GLES
    static int l_ForceCompatibilityContext = 1;
    #endif
#include "vidext_sdl2_compat.h"
#endif

/* local variables */
static m64p_video_extension_functions l_ExternalVideoFuncTable = {12, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
static int l_VideoExtensionActive = 0;
static int l_VideoOutputActive = 0;
static int l_Fullscreen = 0;
static SDL_Surface *l_pScreen = NULL;
static go2_display_t* display = NULL;
static go2_presenter_t* presenter = NULL;
static go2_context_t* context = NULL;


/* global function for use by frontend.c */
m64p_error OverrideVideoFunctions(m64p_video_extension_functions *VideoFunctionStruct)
{
    /* check input data */
    if (VideoFunctionStruct == NULL)
        return M64ERR_INPUT_ASSERT;
    if (VideoFunctionStruct->Functions < 12)
        return M64ERR_INPUT_INVALID;

    /* disable video extension if any of the function pointers are NULL */
    if (VideoFunctionStruct->VidExtFuncInit == NULL ||
        VideoFunctionStruct->VidExtFuncQuit == NULL ||
        VideoFunctionStruct->VidExtFuncListModes == NULL ||
        VideoFunctionStruct->VidExtFuncSetMode == NULL ||
        VideoFunctionStruct->VidExtFuncGLGetProc == NULL ||
        VideoFunctionStruct->VidExtFuncGLSetAttr == NULL ||
        VideoFunctionStruct->VidExtFuncGLGetAttr == NULL ||
        VideoFunctionStruct->VidExtFuncGLSwapBuf == NULL ||
        VideoFunctionStruct->VidExtFuncSetCaption == NULL ||
        VideoFunctionStruct->VidExtFuncToggleFS == NULL ||
        VideoFunctionStruct->VidExtFuncResizeWindow == NULL ||
        VideoFunctionStruct->VidExtFuncGLGetDefaultFramebuffer == NULL)
    {
        l_ExternalVideoFuncTable.Functions = 12;
        memset(&l_ExternalVideoFuncTable.VidExtFuncInit, 0, 12 * sizeof(void *));
        l_VideoExtensionActive = 0;
        return M64ERR_SUCCESS;
    }

    /* otherwise copy in the override function pointers */
    memcpy(&l_ExternalVideoFuncTable, VideoFunctionStruct, sizeof(m64p_video_extension_functions));
    l_VideoExtensionActive = 1;
    return M64ERR_SUCCESS;
}

int VidExt_InFullscreenMode(void)
{
    return l_Fullscreen;
}

int VidExt_VideoRunning(void)
{
    return l_VideoOutputActive;
}

/* video extension functions to be called by the video plugin */
EXPORT m64p_error CALL VidExt_Init(void)
{
    /* call video extension override if necessary */
    if (l_VideoExtensionActive)
        return (*l_ExternalVideoFuncTable.VidExtFuncInit)();

#if SDL_VERSION_ATLEAST(2,0,0)
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
#endif

    if (SDL_InitSubSystem(SDL_INIT_EVENTS) == -1)
    {
        DebugMessage(M64MSG_ERROR, "SDL video subsystem init failed: %s", SDL_GetError());
        return M64ERR_SYSTEM_FAIL;
    }

    //kms_window = kms_window_create();
    display = go2_display_create();
    presenter = go2_presenter_create(display, DRM_FORMAT_RGB565, 0xff080808);

    go2_context_attributes_t attr;
    attr.major = 2;
    attr.minor = 0;
    attr.red_bits = 8;
    attr.green_bits = 8;
    attr.blue_bits = 8;
    attr.alpha_bits = 8;
    attr.depth_bits = 24;
    attr.stencil_bits = 8;

    //context = go2_context_create(display, 480, 320, &attr);
    context = go2_context_create(display, 320, 240, &attr);
    go2_context_make_current(context);

    return M64ERR_SUCCESS;
}

EXPORT m64p_error CALL VidExt_Quit(void)
{
    /* call video extension override if necessary */
    if (l_VideoExtensionActive)
    {
        m64p_error rval = (*l_ExternalVideoFuncTable.VidExtFuncQuit)();
        if (rval == M64ERR_SUCCESS)
        {
            l_VideoOutputActive = 0;
            StateChanged(M64CORE_VIDEO_MODE, M64VIDEO_NONE);
        }
        return rval;
    }

//     if (!SDL_WasInit(SDL_INIT_VIDEO))
//         return M64ERR_NOT_INIT;

//     SDL_ShowCursor(SDL_ENABLE);
// #if SDL_VERSION_ATLEAST(2,0,0)
//     SDL2_DestroyWindow();
// #endif
//     SDL_QuitSubSystem(SDL_INIT_VIDEO);

    //kms_window_destory(kms_window);

    go2_context_destroy(context);
    context = NULL;

    go2_presenter_destroy(presenter);
    presenter = NULL;

    go2_display_destroy(display);
    display = NULL;

    //kms_window = NULL;

    l_pScreen = NULL;
    l_VideoOutputActive = 0;
    StateChanged(M64CORE_VIDEO_MODE, M64VIDEO_NONE);

    return M64ERR_SUCCESS;
}

EXPORT m64p_error CALL VidExt_ListFullscreenModes(m64p_2d_size *SizeArray, int *NumSizes)
{
    const SDL_VideoInfo *videoInfo;
    unsigned int videoFlags;
    SDL_Rect **modes;
    int i;

    /* call video extension override if necessary */
    if (l_VideoExtensionActive)
        return (*l_ExternalVideoFuncTable.VidExtFuncListModes)(SizeArray, NumSizes);

    // if (!SDL_WasInit(SDL_INIT_VIDEO))
    //     return M64ERR_NOT_INIT;

    // /* get a list of SDL video modes */
    // videoFlags = SDL_OPENGL | SDL_FULLSCREEN;

    // if ((videoInfo = SDL_GetVideoInfo()) == NULL)
    // {
    //     DebugMessage(M64MSG_ERROR, "SDL_GetVideoInfo query failed: %s", SDL_GetError());
    //     return M64ERR_SYSTEM_FAIL;
    // }

    // if(videoInfo->hw_available)
    //     videoFlags |= SDL_HWSURFACE;
    // else
    //     videoFlags |= SDL_SWSURFACE;

    // modes = SDL_ListModes(NULL, videoFlags);

    // if (modes == (SDL_Rect **) 0 || modes == (SDL_Rect **) -1)
    // {
    //     DebugMessage(M64MSG_WARNING, "No fullscreen SDL video modes available");
    //     *NumSizes = 0;
    //     return M64ERR_SUCCESS;
    // }

    // i = 0;
    // while (i < *NumSizes && modes[i] != NULL)
    // {
    //     SizeArray[i].uiWidth  = modes[i]->w;
    //     SizeArray[i].uiHeight = modes[i]->h;
    //     i++;
    // }

    // *NumSizes = i;

    //if (!kms_window)
    if (!display)
        return M64ERR_NOT_INIT;

    SizeArray[0].uiWidth = 480; //kms_window->width;
    SizeArray[0].uiHeight = 320; //kms_window->height;

    *NumSizes = 1;

    return M64ERR_SUCCESS;
}

EXPORT m64p_error CALL VidExt_SetVideoMode(int Width, int Height, int BitsPerPixel, m64p_video_mode ScreenMode, m64p_video_flags Flags)
{
    const SDL_VideoInfo *videoInfo;
    int videoFlags = 0;

    /* call video extension override if necessary */
    if (l_VideoExtensionActive)
    {
        m64p_error rval = (*l_ExternalVideoFuncTable.VidExtFuncSetMode)(Width, Height, BitsPerPixel, ScreenMode, Flags);
        l_Fullscreen = (rval == M64ERR_SUCCESS && ScreenMode == M64VIDEO_FULLSCREEN);
        l_VideoOutputActive = (rval == M64ERR_SUCCESS);
        if (l_VideoOutputActive)
        {
            StateChanged(M64CORE_VIDEO_MODE, ScreenMode);
            StateChanged(M64CORE_VIDEO_SIZE, (Width << 16) | Height);
        }
        return rval;
    }

    // if (!SDL_WasInit(SDL_INIT_VIDEO))
    //     return M64ERR_NOT_INIT;

    // /* Get SDL video flags to use */
    // if (ScreenMode == M64VIDEO_WINDOWED)
    // {
    //     videoFlags = SDL_OPENGL;
    //     if (Flags & M64VIDEOFLAG_SUPPORT_RESIZING)
    //         videoFlags |= SDL_RESIZABLE;
    // }
    // else if (ScreenMode == M64VIDEO_FULLSCREEN)
    // {
    //     videoFlags = SDL_OPENGL | SDL_FULLSCREEN;
    // }
    // else
    // {
    //     return M64ERR_INPUT_INVALID;
    // }

    // if ((videoInfo = SDL_GetVideoInfo()) == NULL)
    // {
    //     DebugMessage(M64MSG_ERROR, "SDL_GetVideoInfo query failed: %s", SDL_GetError());
    //     return M64ERR_SYSTEM_FAIL;
    // }
    // if (videoInfo->hw_available)
    //     videoFlags |= SDL_HWSURFACE;
    // else
    //     videoFlags |= SDL_SWSURFACE;

    // /* set the mode */
    // if (BitsPerPixel > 0)
    //     DebugMessage(M64MSG_INFO, "Setting %i-bit video mode: %ix%i", BitsPerPixel, Width, Height);
    // else
    //     DebugMessage(M64MSG_INFO, "Setting video mode: %ix%i", Width, Height);

    // l_pScreen = SDL_SetVideoMode(Width, Height, BitsPerPixel, videoFlags);
    // if (l_pScreen == NULL)
    // {
    //     DebugMessage(M64MSG_ERROR, "SDL_SetVideoMode failed: %s", SDL_GetError());
    //     return M64ERR_SYSTEM_FAIL;
    // }

    // SDL_ShowCursor(SDL_DISABLE);

    //if (!kms_window)
    if (!display)
        return M64ERR_NOT_INIT;

    l_Fullscreen = (ScreenMode == M64VIDEO_FULLSCREEN);
    l_VideoOutputActive = 1;
    StateChanged(M64CORE_VIDEO_MODE, ScreenMode);
    StateChanged(M64CORE_VIDEO_SIZE, (Width << 16) | Height);
    return M64ERR_SUCCESS;
}

EXPORT m64p_error CALL VidExt_ResizeWindow(int Width, int Height)
{
    const SDL_VideoInfo *videoInfo;
    int videoFlags = 0;

    /* call video extension override if necessary */
    if (l_VideoExtensionActive)
    {
        m64p_error rval;
        // shut down the OSD
        osd_exit();
        // re-create the OGL context
        rval = (*l_ExternalVideoFuncTable.VidExtFuncResizeWindow)(Width, Height);
        if (rval == M64ERR_SUCCESS)
        {
            StateChanged(M64CORE_VIDEO_SIZE, (Width << 16) | Height);
            // re-create the On-Screen Display
            osd_init(Width, Height);
        }
        return rval;
    }

    // if (!l_VideoOutputActive || !SDL_WasInit(SDL_INIT_VIDEO))
    //     return M64ERR_NOT_INIT;

    if (l_Fullscreen)
    {
        DebugMessage(M64MSG_ERROR, "VidExt_ResizeWindow() called in fullscreen mode.");
        return M64ERR_INVALID_STATE;
    }

    // /* Get SDL video flags to use */
    // videoFlags = SDL_OPENGL | SDL_RESIZABLE;
    // if ((videoInfo = SDL_GetVideoInfo()) == NULL)
    // {
    //     DebugMessage(M64MSG_ERROR, "SDL_GetVideoInfo query failed: %s", SDL_GetError());
    //     return M64ERR_SYSTEM_FAIL;
    // }
    // if (videoInfo->hw_available)
    //     videoFlags |= SDL_HWSURFACE;
    // else
    //     videoFlags |= SDL_SWSURFACE;

    // destroy the On-Screen Display
    osd_exit();

    // /* set the re-sizing the screen will create a new OpenGL context */
    // l_pScreen = SDL_SetVideoMode(Width, Height, 0, videoFlags);
    // if (l_pScreen == NULL)
    // {
    //     DebugMessage(M64MSG_ERROR, "SDL_SetVideoMode failed: %s", SDL_GetError());
    //     return M64ERR_SYSTEM_FAIL;
    // }

    StateChanged(M64CORE_VIDEO_SIZE, (Width << 16) | Height);
    // re-create the On-Screen Display
    osd_init(Width, Height);
    return M64ERR_SUCCESS;
}

EXPORT m64p_error CALL VidExt_SetCaption(const char *Title)
{
    /* call video extension override if necessary */
    if (l_VideoExtensionActive)
        return (*l_ExternalVideoFuncTable.VidExtFuncSetCaption)(Title);

    // if (!SDL_WasInit(SDL_INIT_VIDEO))
    //     return M64ERR_NOT_INIT;

    // SDL_WM_SetCaption(Title, "M64+ Video");

    return M64ERR_SUCCESS;
}

EXPORT m64p_error CALL VidExt_ToggleFullScreen(void)
{
    /* call video extension override if necessary */
    if (l_VideoExtensionActive)
    {
        m64p_error rval = (*l_ExternalVideoFuncTable.VidExtFuncToggleFS)();
        if (rval == M64ERR_SUCCESS)
        {
            l_Fullscreen = !l_Fullscreen;
            StateChanged(M64CORE_VIDEO_MODE, l_Fullscreen ? M64VIDEO_FULLSCREEN : M64VIDEO_WINDOWED);
        }
        return rval;
    }

    // if (!SDL_WasInit(SDL_INIT_VIDEO))
    //     return M64ERR_NOT_INIT;

    // /* TODO:
    //  * SDL_WM_ToggleFullScreen doesn't work under Windows and others
    //  * (see http://wiki.libsdl.org/moin.cgi/FAQWindows for explanation).
    //  * Instead, we should call SDL_SetVideoMode with the SDL_FULLSCREEN flag.
    //  * (see http://sdl.beuc.net/sdl.wiki/SDL_SetVideoMode), but on Windows
    //  * this resets the OpenGL context and video plugins don't support it yet.
    //  * Uncomment the next line to test it: */
    // //return VidExt_SetVideoMode(l_pScreen->w, l_pScreen->h, l_pScreen->format->BitsPerPixel, l_Fullscreen ? M64VIDEO_WINDOWED : M64VIDEO_FULLSCREEN);
    // if (SDL_WM_ToggleFullScreen(l_pScreen) == 1)
    // {
    //     l_Fullscreen = !l_Fullscreen;
    //     StateChanged(M64CORE_VIDEO_MODE, l_Fullscreen ? M64VIDEO_FULLSCREEN : M64VIDEO_WINDOWED);
    //     return M64ERR_SUCCESS;
    // }

    // return M64ERR_SYSTEM_FAIL;

    return M64ERR_SUCCESS;
}

EXPORT m64p_function CALL VidExt_GL_GetProcAddress(const char* Proc)
{
    /* call video extension override if necessary */
    if (l_VideoExtensionActive)
        return (*l_ExternalVideoFuncTable.VidExtFuncGLGetProc)(Proc);

    // if (!SDL_WasInit(SDL_INIT_VIDEO))
    //     return NULL;

/* WARN: assume cast to m64p_function is supported by platform and disable warning accordingly */
OSAL_WARNING_PUSH
OSAL_NO_WARNING_FPTR_VOIDP_CAST
    //return (m64p_function)SDL_GL_GetProcAddress(Proc);
    return (m64p_function)eglGetProcAddress(Proc);
OSAL_WARNING_POP
}

typedef struct {
    m64p_GLattr m64Attr;
    SDL_GLattr sdlAttr;
} GLAttrMapNode;

static const GLAttrMapNode GLAttrMap[] = {
        { M64P_GL_DOUBLEBUFFER, SDL_GL_DOUBLEBUFFER },
        { M64P_GL_BUFFER_SIZE,  SDL_GL_BUFFER_SIZE },
        { M64P_GL_DEPTH_SIZE,   SDL_GL_DEPTH_SIZE },
        { M64P_GL_RED_SIZE,     SDL_GL_RED_SIZE },
        { M64P_GL_GREEN_SIZE,   SDL_GL_GREEN_SIZE },
        { M64P_GL_BLUE_SIZE,    SDL_GL_BLUE_SIZE },
        { M64P_GL_ALPHA_SIZE,   SDL_GL_ALPHA_SIZE },
#if !SDL_VERSION_ATLEAST(1,3,0)
        { M64P_GL_SWAP_CONTROL, SDL_GL_SWAP_CONTROL },
#endif
        { M64P_GL_MULTISAMPLEBUFFERS, SDL_GL_MULTISAMPLEBUFFERS },
        { M64P_GL_MULTISAMPLESAMPLES, SDL_GL_MULTISAMPLESAMPLES }
#if SDL_VERSION_ATLEAST(2,0,0)
       ,{ M64P_GL_CONTEXT_MAJOR_VERSION, SDL_GL_CONTEXT_MAJOR_VERSION },
        { M64P_GL_CONTEXT_MINOR_VERSION, SDL_GL_CONTEXT_MINOR_VERSION },
        { M64P_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_MASK }
#endif
};
static const int mapSize = sizeof(GLAttrMap) / sizeof(GLAttrMapNode);

EXPORT m64p_error CALL VidExt_GL_SetAttribute(m64p_GLattr Attr, int Value)
{
    int i;

    /* call video extension override if necessary */
    if (l_VideoExtensionActive)
        return (*l_ExternalVideoFuncTable.VidExtFuncGLSetAttr)(Attr, Value);

//     if (!SDL_WasInit(SDL_INIT_VIDEO))
//         return M64ERR_NOT_INIT;

// #if SDL_VERSION_ATLEAST(1,3,0)
//     if (Attr == M64P_GL_SWAP_CONTROL)
//     {
//         if (SDL_GL_SetSwapInterval(Value) != 0)
//             return M64ERR_SYSTEM_FAIL;
//         return M64ERR_SUCCESS;
//     }
// #endif

//     /* translate the GL context type mask if necessary */
// #if SDL_VERSION_ATLEAST(2,0,0)
//     if (Attr == M64P_GL_CONTEXT_PROFILE_MASK)
//     {
//         switch (Value)
//         {
//             case M64P_GL_CONTEXT_PROFILE_CORE:
//                 Value = SDL_GL_CONTEXT_PROFILE_CORE;
// #ifndef USE_GLES
//                 l_ForceCompatibilityContext = 0;
// #endif
//                 break;
//             case M64P_GL_CONTEXT_PROFILE_COMPATIBILITY:
//                 Value = SDL_GL_CONTEXT_PROFILE_COMPATIBILITY;
//                 break;
//             case M64P_GL_CONTEXT_PROFILE_ES:
//                 Value = SDL_GL_CONTEXT_PROFILE_ES;
//                 break;
//             default:
//                 Value = 0;
//         }
//     }
// #endif

    // for (i = 0; i < mapSize; i++)
    // {
    //     if (GLAttrMap[i].m64Attr == Attr)
    //     {
    //         if (SDL_GL_SetAttribute(GLAttrMap[i].sdlAttr, Value) != 0)
    //             return M64ERR_SYSTEM_FAIL;
            return M64ERR_SUCCESS;
    //     }
    // }

    // return M64ERR_INPUT_INVALID;
}

EXPORT m64p_error CALL VidExt_GL_GetAttribute(m64p_GLattr Attr, int *pValue)
{
    int i;

    /* call video extension override if necessary */
    if (l_VideoExtensionActive)
        return (*l_ExternalVideoFuncTable.VidExtFuncGLGetAttr)(Attr, pValue);

    if (!SDL_WasInit(SDL_INIT_VIDEO))
        return M64ERR_NOT_INIT;

#if SDL_VERSION_ATLEAST(1,3,0)
    if (Attr == M64P_GL_SWAP_CONTROL)
    {
        *pValue = SDL_GL_GetSwapInterval();
        return M64ERR_SUCCESS;
    }
#endif

//     for (i = 0; i < mapSize; i++)
//     {
//         if (GLAttrMap[i].m64Attr == Attr)
//         {
//             int NewValue = 0;
//             if (SDL_GL_GetAttribute(GLAttrMap[i].sdlAttr, &NewValue) != 0)
//                 return M64ERR_SYSTEM_FAIL;
//             /* translate the GL context type mask if necessary */
// #if SDL_VERSION_ATLEAST(2,0,0)
//             if (Attr == M64P_GL_CONTEXT_PROFILE_MASK)
//             {
//                 switch (NewValue)
//                 {
//                     case SDL_GL_CONTEXT_PROFILE_CORE:
//                         NewValue = M64P_GL_CONTEXT_PROFILE_CORE;
//                         break;
//                     case SDL_GL_CONTEXT_PROFILE_COMPATIBILITY:
//                         NewValue = M64P_GL_CONTEXT_PROFILE_COMPATIBILITY;
//                         break;
//                     case SDL_GL_CONTEXT_PROFILE_ES:
//                         NewValue = M64P_GL_CONTEXT_PROFILE_ES;
//                         break;
//                     default:
//                         NewValue = 0;
//                 }
//             }
// #endif
//             *pValue = NewValue;
//             return M64ERR_SUCCESS;
//         }
//     }

    return M64ERR_INPUT_INVALID;
}

EXPORT m64p_error CALL VidExt_GL_SwapBuffers(void)
{
    /* call video extension override if necessary */
    if (l_VideoExtensionActive)
        return (*l_ExternalVideoFuncTable.VidExtFuncGLSwapBuf)();

    // if (!SDL_WasInit(SDL_INIT_VIDEO))
    //     return M64ERR_NOT_INIT;

    // SDL_GL_SwapBuffers();

    //if (!kms_window)
    if (!display)
        return M64ERR_NOT_INIT;

    //kms_window_swap_buffers2(kms_window);
    go2_context_swap_buffers(context);

    go2_surface_t* surface = go2_context_surface_lock(context);
    // go2_presenter_post(presenter,
    //             surface,
    //             0, 0, 480, 320,
    //             0, 0, 320, 480,
    //             GO2_ROTATION_DEGREES_270);
    go2_presenter_post(presenter,
            surface,
            0, 0, 320, 240,
            0, 27, 320, 426,
            GO2_ROTATION_DEGREES_270);

    go2_context_surface_unlock(context, surface);

    return M64ERR_SUCCESS;
}

EXPORT uint32_t CALL VidExt_GL_GetDefaultFramebuffer(void)
{
    if (l_VideoExtensionActive)
        return (*l_ExternalVideoFuncTable.VidExtFuncGLGetDefaultFramebuffer)();

    return 0;
}
