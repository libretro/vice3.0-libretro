/*
 * uistatusbar.c - SDL statusbar.
 *
 * Written by
 *  Hannu Nuotio <hannu.nuotio@tut.fi>
 *
 * Based on code by
 *  Andreas Boose <viceteam@t-online.de>
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

#include <stdio.h>

#include "resources.h"
#include "types.h"
#include "ui.h"
#include "uiapi.h"
//#include "uimenu.h"
#include "uistatusbar.h"
#include "videoarch.h"

#include "libretro.h"
#include "libretro-core.h"

#include "joystick.h"

/* ----------------------------------------------------------------- */
/* static functions/variables */

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#define MAX_STATUSBAR_LEN           47
#define STATUSBAR_JOY_POS           0
#define STATUSBAR_DRIVE_POS         38
#define STATUSBAR_DRIVE8_TRACK_POS  40
#define STATUSBAR_DRIVE9_TRACK_POS  40
#define STATUSBAR_DRIVE10_TRACK_POS 40
#define STATUSBAR_DRIVE11_TRACK_POS 40
#define STATUSBAR_TAPE_POS          32
#define STATUSBAR_PAUSE_POS         43
#define STATUSBAR_SPEED_POS         44

static char statusbar_text[MAX_STATUSBAR_LEN] = "                                              ";

static int pitch;
static int draw_offset;



static int tape_counter = 0;
static int tape_enabled = 0;
static int tape_motor = 0;
static int tape_control = 0;

static void display_tape(void)
{
    int len;

    if (tape_enabled) {
        len = sprintf(&(statusbar_text[STATUSBAR_TAPE_POS]), "%c%03d%c", (tape_motor) ? '*' : ' ', tape_counter, " >f<R"[tape_control]);
    } else {
        len = sprintf(&(statusbar_text[STATUSBAR_TAPE_POS]), "     ");
    }
    statusbar_text[STATUSBAR_TAPE_POS + len] = ' ';

    if (uistatusbar_state & UISTATUSBAR_ACTIVE) {
        uistatusbar_state |= UISTATUSBAR_REPAINT;
    }
}

char* joystick_value_human(char val)
{
    static char str[3];
    switch(val) {
        default:
            sprintf(str, "%3s", "   ");
            break;
        case 1:
        case 17:
            sprintf(str, "%3s", " ^ ");
            break;
        case 2:
        case 18:
            sprintf(str, "%3s", " v ");
            break;
        case 4:
        case 20:
            sprintf(str, "%3s", "<  ");
            break;
        case 5:
        case 21:
            sprintf(str, "%3s", "<^ ");
            break;
        case 6:
        case 22:
            sprintf(str, "%3s", "<v ");
            break;
        case 8:
        case 24:
            sprintf(str, "%3s", "  >");
            break;
        case 9:
        case 25:
            sprintf(str, "%3s", " ^>");
            break;
        case 10:
        case 26:
            sprintf(str, "%3s", " v>");
            break;
    }

    str[1] = (val >= 16 ) ? (str[1] | 0x80) : str[1];
    return str;
}

extern unsigned int cur_port;

static void display_joyport(void)
{
    int len;
    char tmpstr[25];
    char joy1[2];
    char joy2[2];
    sprintf(joy1, "%s", "1");
    sprintf(joy2, "%s", "2");
    if(cur_port == 1)
        joy1[0] = (joy1[0] | 0x80);
    else if(cur_port == 2)
        joy2[0] = (joy2[0] | 0x80);
    
    sprintf(tmpstr, "J%s%3s ", joy1, joystick_value_human(joystick_value[1]));
    sprintf(tmpstr + strlen(tmpstr), "J%s%3s ", joy2, joystick_value_human(joystick_value[2]));
    sprintf(tmpstr + strlen(tmpstr), "J%d%3s ", 3, joystick_value_human(joystick_value[3]));
    sprintf(tmpstr + strlen(tmpstr), "J%d%3s", 4, joystick_value_human(joystick_value[4]));

    len = sprintf(&(statusbar_text[STATUSBAR_JOY_POS]), "%s", tmpstr);
    statusbar_text[STATUSBAR_JOY_POS + len] = ' ';

    if (uistatusbar_state & UISTATUSBAR_ACTIVE) {
        uistatusbar_state |= UISTATUSBAR_REPAINT;
    }
}


static int per = 0;
static int fps = 0;
static int warp = 0;
static int paused = 0;

static void display_speed(void)
{
    int len;
    //char sep = paused ? ('P' | 0x80) : warp ? ('W' | 0x80) : ' ';
    char fps_str[3];
    sprintf(fps_str, "%2d", fps);
    if (warp)
    {
        fps_str[0] = (fps_str[0] | 0x80);
        fps_str[1] = (fps_str[1] | 0x80);
    }

    len = sprintf(&(statusbar_text[STATUSBAR_SPEED_POS]), "%2s", fps_str);
    //statusbar_text[STATUSBAR_SPEED_POS + len] = ' '; /* No end separator for last element */

    if (uistatusbar_state & UISTATUSBAR_ACTIVE) {
        uistatusbar_state |= UISTATUSBAR_REPAINT;
    }
}

/* ----------------------------------------------------------------- */
/* ui.h */

void ui_display_speed(float percent, float framerate, int warp_flag)
{
    per = (int)(percent + .5);
    if (per > 999) {
        per = 999;
    }

    fps = (int)(framerate + .5);
    if (fps > 99) {
        fps = 99;
    }

    warp = warp_flag;

    display_speed();
}

void ui_display_paused(int flag)
{
    paused = flag;

    display_speed();
}

/* ----------------------------------------------------------------- */
/* uiapi.h */

/* Display a mesage without interrupting emulation */
void ui_display_statustext(const char *text, int fade_out)
{
#ifdef SDL_DEBUG
    fprintf(stderr, "%s: \"%s\", %i\n", __func__, text, fade_out);
#endif
}

/* Drive related UI.  */
void ui_enable_drive_status(ui_drive_enable_t state, int *drive_led_color)
{
    int drive_number;
    int drive_state = (int)state;

    for (drive_number = 0; drive_number < 4; ++drive_number) {
        if (drive_state & 1) {
            ui_display_drive_led(drive_number, 0, 0);
        } else {
            statusbar_text[STATUSBAR_DRIVE_POS + drive_number] = ' ';
        }
        drive_state >>= 1;
    }

    if (uistatusbar_state & UISTATUSBAR_ACTIVE) {
        uistatusbar_state |= UISTATUSBAR_REPAINT;
    }
}

void ui_display_drive_track(unsigned int drive_number, unsigned int drive_base, unsigned int half_track_number)
{
    unsigned int track_number = half_track_number / 2;

#ifdef SDL_DEBUG
    fprintf(stderr, "%s\n", __func__);
#endif

    switch (drive_number) {
        case 1:
            statusbar_text[STATUSBAR_DRIVE9_TRACK_POS] = (track_number / 10) + '0';
            statusbar_text[STATUSBAR_DRIVE9_TRACK_POS + 1] = (track_number % 10) + '0';
            break;
        case 2:
            statusbar_text[STATUSBAR_DRIVE10_TRACK_POS] = (track_number / 10) + '0';
            statusbar_text[STATUSBAR_DRIVE10_TRACK_POS + 1] = (track_number % 10) + '0';
            break;
        case 3:
            statusbar_text[STATUSBAR_DRIVE11_TRACK_POS] = (track_number / 10) + '0';
            statusbar_text[STATUSBAR_DRIVE11_TRACK_POS + 1] = (track_number % 10) + '0';
            break;
        default:
        case 0:
            statusbar_text[STATUSBAR_DRIVE8_TRACK_POS] = (track_number / 10) + '0';
            statusbar_text[STATUSBAR_DRIVE8_TRACK_POS + 1] = (track_number % 10) + '0';
            break;
    }

    if (uistatusbar_state & UISTATUSBAR_ACTIVE) {
        uistatusbar_state |= UISTATUSBAR_REPAINT;
    }
}

/* The pwm value will vary between 0 and 1000.  */
void ui_display_drive_led(int drive_number, unsigned int pwm1, unsigned int led_pwm2)
{
    char c;

#ifdef SDL_DEBUG
    fprintf(stderr, "%s: drive %i, pwm1 = %i, led_pwm2 = %u\n", __func__, drive_number, pwm1, led_pwm2);
#endif

    c = "8901"[drive_number] | ((pwm1 > 500) ? 0x80 : 0);
    statusbar_text[STATUSBAR_DRIVE_POS + (drive_number * 5)] = c;
    statusbar_text[STATUSBAR_DRIVE_POS + (drive_number * 5) + 1] = 'T';

    if (uistatusbar_state & UISTATUSBAR_ACTIVE) {
        uistatusbar_state |= UISTATUSBAR_REPAINT;
    }
}

void ui_display_drive_current_image(unsigned int drive_number, const char *image)
{
printf("d%d -> %s\n",drive_number, image);
#ifdef SDL_DEBUG
    fprintf(stderr, "%s\n", __func__);
#endif
}

/* Tape related UI */

void ui_set_tape_status(int tape_status)
{
    tape_enabled = tape_status;

    display_tape();
}

void ui_display_tape_motor_status(int motor)
{
    tape_motor = motor;

    display_tape();
}

void ui_display_tape_control_status(int control)
{
    tape_control = control;

    display_tape();
}

void ui_display_tape_counter(int counter)
{
    if (tape_counter != counter) {
        display_tape();
    }

    tape_counter = counter;
}

void ui_display_tape_current_image(const char *image)
{
#ifdef SDL_DEBUG
    fprintf(stderr, "%s: %s\n", __func__, image);
#endif
}

/* Recording UI */
void ui_display_playback(int playback_status, char *version)
{
#ifdef SDL_DEBUG
    fprintf(stderr, "%s: %i, \"%s\"\n", __func__, playback_status, version);
#endif
}

void ui_display_recording(int recording_status)
{
#ifdef SDL_DEBUG
    fprintf(stderr, "%s: %i\n", __func__, recording_status);
#endif
}

void ui_display_event_time(unsigned int current, unsigned int total)
{
#ifdef SDL_DEBUG
    fprintf(stderr, "%s: %i, %i\n", __func__, current, total);
#endif
}

/* Joystick UI */
void ui_display_joyport(BYTE *joyport)
{
#ifdef SDL_DEBUG
    fprintf(stderr, "%s: %02x %02x %02x %02x %02x\n", __func__, joyport[0], joyport[1], joyport[2], joyport[3], joyport[4]);
#endif
}

/* Volume UI */
void ui_display_volume(int vol)
{
#ifdef SDL_DEBUG
    fprintf(stderr, "%s: %i\n", __func__, vol);
#endif
}

/* ----------------------------------------------------------------- */
/* resources */

static int statusbar_enabled;

static int set_statusbar(int val, void *param)
{
    statusbar_enabled = val ? 1 : 0;

    if (statusbar_enabled) {
        uistatusbar_open();
    } else {
        uistatusbar_close();
    }

    return 0;
}

static const resource_int_t resources_int[] = {
    { "SDLStatusbar", 0, RES_EVENT_NO, NULL,
      &statusbar_enabled, set_statusbar, NULL },
    RESOURCE_INT_LIST_END
};

int uistatusbar_init_resources(void)
{
#ifdef SDL_DEBUG
    fprintf(stderr, "%s\n", __func__);
#endif
    return resources_register_int(resources_int);
}


/* ----------------------------------------------------------------- */
/* uistatusbar.h */

int uistatusbar_state = 0;

void uistatusbar_open(void)
{
    uistatusbar_state = UISTATUSBAR_ACTIVE | UISTATUSBAR_REPAINT;
}

void uistatusbar_close(void)
{
    uistatusbar_state = UISTATUSBAR_REPAINT;
}

#include "keyboard.h"
extern unsigned int cur_port;


#include "RSDL_wrapper.h"
#include "libretro-core.h"

#ifdef M16B
extern void Retro_Draw_string(RSDL_Surface *surface, signed short int x, signed short int y, const  char *string,unsigned short maxstrlen,unsigned short xscale, unsigned short yscale, unsigned short fg, unsigned short bg);
#else
extern void Retro_Draw_string(RSDL_Surface *surface, signed short int x, signed short int y, const  char *string,unsigned short maxstrlen,unsigned short xscale, unsigned short yscale, unsigned  fg, unsigned  bg);
#endif

RSDL_Surface fake;

void uistatusbar_draw(void)
{
    int i;
    BYTE c;//, color_f, color_b;
#ifdef M16B
unsigned short int color_f, color_b;
    color_f = 0xffff;
    color_b = 0x0001;
#else
unsigned int color_f, color_b;
    color_f = 0xffffffff;
    color_b = 0x00000000;
#endif
    unsigned int line;
    unsigned int char_width;
    unsigned int char_offset;

  //  color_f = 0xff;
  //  color_b = 0;

    pitch =PITCH;

    char tmpstr[512];

    fake.pixels=&bmp[0];
    fake.h=retroh;
    fake.w=retrow;
    fake.clip_rect.h=retroh;
    fake.clip_rect.w=retrow;
    fake.clip_rect.x=0;
    fake.clip_rect.y=0;

    /* Statusbar location with or without borders */
    int x, y;
    int border = 0;
#if !defined(__PET__) && !defined(__CBM2__)
#if defined(__VIC20__)
    resources_get_int("VICBorderMode", &border);
#elif defined(__PLUS4__)
    resources_get_int("TEDBorderMode", &border);
#else
    resources_get_int("VICIIBorderMode", &border);
#endif
#endif

    /* 0 : normal, 1: full, 2: debug, 3: none */
    /* Placement on bottom + inside VIC */
    switch(border) {
        default:
        case 0:
            x=32;
            y=227;/* Below VIC: 236 */
            if(retro_get_region() == RETRO_REGION_NTSC) {
                y=215;
            }
            break;
        case 3: 
            x=0;
            y=192;
            break;
    }

    display_joyport();

    for (i = 0; i < MAX_STATUSBAR_LEN; ++i) {
        c = statusbar_text[i];

        if (c == 0) {
            break;
        }
        
        /* Trickery to balance uneven character width with VIC area width */
        char_width = 7;
        char_offset = (MAX_STATUSBAR_LEN - i <= 4) ? -2 : 0;

        if (c & 0x80) {
            sprintf(tmpstr,"%c",c&0x7f);
            Retro_Draw_string(&fake, x+char_offset+i*char_width, y, tmpstr,1,1,1, color_b, color_f);
            //  uistatusbar_putchar((BYTE)(c & 0x7f), i, 0, color_b, color_f);
        } else {
            sprintf(tmpstr,"%c",c);
            Retro_Draw_string(&fake, x+char_offset+i*char_width, y, tmpstr,1,1,1, color_f, color_b);
            //  uistatusbar_putchar(c, i, 0, color_f, color_b);
        }
    }
}

