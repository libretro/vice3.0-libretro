/*
 * vsyncarch.c - End-of-frame handling for Unix
 *
 * Written by
 *  Dag Lem <resid@nimrod.no>
 *
 * This file is part of VICE, the Versatile Commodore Emulator.
 * See README for copyright notice.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 *  02111-1307  USA.
 *
 */


#include "vice.h"

#include "kbdbuf.h"
#include "ui.h"
#include "uistatusbar.h"
#include "vsyncapi.h"
#include "videoarch.h"
#include "video.h"
#include "resources.h"

#include "libretro-core.h"

#if defined(VITA)
#include <psp2/kernel/threadmgr.h>
#endif

extern void retro_poll_event();
extern void app_vkb_handle();

extern struct video_canvas_s *RCANVAS;

#include <time.h>

//#define TICKSPERSECOND  1000000L     /* Microseconds resolution. */
#ifdef HAVE_NANOSLEEP
#define TICKSPERSECOND  1000000000L  /* Nanoseconds resolution. */
#define TICKSPERMSEC    1000000L
#define TICKSPERUSEC    1000L
#define TICKSPERNSEC    1L
#else
#define TICKSPERSECOND  1000000L     /* Microseconds resolution. */
#define TICKSPERMSEC    1000L
#define TICKSPERUSEC    1L
#endif


/* hook to ui event dispatcher */
static void_hook_t ui_dispatch_hook;

/* ------------------------------------------------------------------------- */

/* Number of timer units per second. */
unsigned long vsyncarch_frequency(void)
{
    /* Microseconds resolution. */
    return TICKSPERSECOND;
}

/* Get time in timer units. */
unsigned long vsyncarch_gettime(void)
{
    /* Microseconds resolution. */
    return GetTicks();
}

void vsyncarch_init(void)
{
    (void)vsync_set_event_dispatcher(ui_dispatch_events);
}

/* Display speed (percentage) and frame rate (frames per second). */
void vsyncarch_display_speed(double speed, double frame_rate, int warp_enabled)
{
    ui_display_speed((float)speed, (float)frame_rate, warp_enabled);
}

/* Sleep a number of timer units. */
void vsyncarch_sleep(unsigned long delay)
{
    // NOTE: Cores should not sleep. vsyncarch_sleep is only used by vice to
    // keep a stable 50Hz refresh rate, which for libretro is the responsibility
    // of the frontend.
}

void vsyncarch_presync(void)
{
    kbdbuf_flush();
    retro_poll_event();

#if defined(__VIC20__)
        RCANVAS->videoconfig->rendermode = VIDEO_RENDER_RGB_1X1;
#endif
	video_canvas_render(RCANVAS,(BYTE *)bmp,
			retroW,retroH,
                        retroXS,retroYS,
                        0,0,//xi, yi,
                        retrow*PITCH,8*PITCH);
                        
    if (uistatusbar_state & UISTATUSBAR_ACTIVE) {
        uistatusbar_draw();
    }

    cpuloop=0;

}

void_hook_t vsync_set_event_dispatcher(void_hook_t hook)
{
    void_hook_t t = ui_dispatch_hook;
    ui_dispatch_hook = hook;
    return t;
}

void vsyncarch_postsync(void)
{
    /* Dispatch all the pending UI events.  */
    ui_dispatch_events();
}
