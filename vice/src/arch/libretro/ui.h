/*
 * ui.h - RETRO user interface.
 *
 */

#ifndef _UI_RETRO_H
#define _UI_RETRO_H

#include "vice.h"

#include "types.h"
#include "uiapi.h"

extern int ui_vblank_sync_enabled();

extern void ui_exit(void);
extern void ui_display_speed(float percent, float framerate, int warp_flag);
extern void ui_display_paused(int flag);
extern void ui_dispatch_next_event(void);
extern void ui_dispatch_events(void);
extern void ui_error_string(const char *text);
extern void ui_check_mouse_cursor(void);

int  ui_emulation_is_paused(void);
void ui_pause_emulation(int flag);
#endif

