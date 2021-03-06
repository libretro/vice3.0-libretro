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
#include "uistatusbar.h"
#include "videoarch.h"
#include "joystick.h"
#include "archdep.h"

#include "libretro-core.h"
#include "libretro-graph.h"

extern unsigned int mouse_value[2 + 1];
extern unsigned int vice_led_state[3];
extern unsigned int opt_joyport_type;
extern unsigned int opt_autoloadwarp;
extern unsigned int retro_warpmode;
extern int retro_warp_mode_enabled();
extern int RGB(int r, int g, int b);

/* ----------------------------------------------------------------- */
/* static functions/variables */

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#if defined(__XVIC__)
extern int vic20mem_forced;
#endif

#define STATUSBAR_RESOLUTION_POS    28
#define STATUSBAR_MODEL_POS         45

#define STATUSBAR_JOY_POS           0
#define STATUSBAR_TAPE_POS          56
#define STATUSBAR_DRIVE_POS         57
#define STATUSBAR_DRIVE8_TRACK_POS  59
#define STATUSBAR_DRIVE9_TRACK_POS  59
#define STATUSBAR_DRIVE10_TRACK_POS 59
#define STATUSBAR_DRIVE11_TRACK_POS 59
#define STATUSBAR_PAUSE_POS         61
#define STATUSBAR_SPEED_POS         61
#define MAX_STATUSBAR_LEN           64

unsigned char statusbar_text[MAX_STATUSBAR_LEN] = {0};

static unsigned char* joystick_value_human(char val, int vice_device)
{
    static unsigned char str[6] = {0};
    sprintf(str, "%3s", "   ");

    if (val & 0x01) /* UP */
        str[1] = 30;

    else if (val & 0x02) /* DOWN */
        str[1] = 28;

    if (val & 0x04) /* LEFT */
        str[0] = 27;

    else if (val & 0x08) /* RIGHT */
        str[2] = 29;

    str[1] = (val & 0x10) ? (str[1] | 0x80) : str[1];

    if (vice_device > 0)
    {
        str[1] = (val & 0x10) ? ('L' | 0x80) : str[1];
        str[1] = (val & 0x20) ? ('R' | 0x80) : str[1];
        str[1] = (val & 0x40) ? ('M' | 0x80) : str[1];
    }

    return str;
}

static void display_joyport(void)
{
    unsigned char tmpstr[25] = {0};
    unsigned int width_offset = 0;

    int zoomed_centering = (retrow - zoomed_width + width_offset) / 12;
#if defined(__XVIC__)
    /* PAL offset */
    zoomed_centering -= 5;
    /* NTSC offset */
    if (retrow == 400)
       zoomed_centering += 4;
#endif
    if (zoomed_centering > STATUSBAR_RESOLUTION_POS)
       zoomed_centering = 0;

#if !defined(__XPET__) && !defined(__XCBM2__) && !defined(__XVIC__)
    unsigned char joy1[2];
    unsigned char joy2[2];
    sprintf(joy1, "%s", "1");
    sprintf(joy2, "%s", "2");

    /* Mouse */
    if (opt_joyport_type > 2 && cur_port == 1)
        sprintf(tmpstr, "M%s%3s ", joy1, joystick_value_human(mouse_value[1], 1));
    /* Paddles */
    else if (opt_joyport_type == 2 && cur_port == 1)
        sprintf(tmpstr, "P%s%3s ", joy1, joystick_value_human(mouse_value[1], 1));
    /* Joystick */
    else
        sprintf(tmpstr, "J%s%3s ", joy1, joystick_value_human(joystick_value[1], 0));

    /* Mouse */
    if (opt_joyport_type > 2 && cur_port == 2)
        sprintf(tmpstr + strlen(tmpstr), "M%s%3s ", joy2, joystick_value_human(mouse_value[2], 1));
    /* Paddles */
    else if (opt_joyport_type == 2 && cur_port == 2)
        sprintf(tmpstr + strlen(tmpstr), "P%s%3s ", joy2, joystick_value_human(mouse_value[2], 1));
    /* Joystick */
    else
        sprintf(tmpstr + strlen(tmpstr), "J%s%3s ", joy2, joystick_value_human(joystick_value[2], 0));
#elif defined(__XVIC__)
    char joy1[2];
    sprintf(joy1, "%s", "1");

    /* Mouse */
    if (opt_joyport_type > 2)
       sprintf(tmpstr, "M%s%3s ", joy1, joystick_value_human(mouse_value[1], 1));
    /* Paddles */
    else if (opt_joyport_type == 2)
       sprintf(tmpstr, "P%s%3s ", joy1, joystick_value_human(mouse_value[1], 1));
    /* Joystick */
    else
       sprintf(tmpstr, "J%s%3s ", joy1, joystick_value_human(joystick_value[1], 0));
#endif

    if (vice_opt.UserportJoyType != -1)
    {
        sprintf(tmpstr + strlen(tmpstr), "J%d%3s ", 3, joystick_value_human(joystick_value[3], 0));
        sprintf(tmpstr + strlen(tmpstr), "J%d%3s ", 4, joystick_value_human(joystick_value[4], 0));
    }
    else
    {
        sprintf(tmpstr + strlen(tmpstr), "%5s", "");
        sprintf(tmpstr + strlen(tmpstr), "%5s", "");
    }

    if (opt_statusbar & STATUSBAR_BASIC)
        snprintf(tmpstr, sizeof(tmpstr), "%24s", "");

    sprintf(&statusbar_text[STATUSBAR_JOY_POS], "%-55s", tmpstr);

    if (opt_statusbar & STATUSBAR_BASIC)
        return;

    sprintf(&statusbar_text[STATUSBAR_RESOLUTION_POS-zoomed_centering], "%dx%d", zoomed_width, zoomed_height);

#if defined(__X64__) || defined(__X64SC__) || defined(__XSCPU64__)
    /* Model */
    unsigned model = vice_opt.Model;
    if (request_model_set > -1 && request_model_set != model)
        model = request_model_set;

    tmpstr[0] = '\0';
    switch (model)
    {
        case C64MODEL_C64_PAL:
        case C64MODEL_C64_PAL_N:
        case C64MODEL_C64_OLD_PAL:
        case C64MODEL_C64_NTSC:
        case C64MODEL_C64_OLD_NTSC:
            strcpy(tmpstr, "C64"); break;
        case C64MODEL_C64C_PAL:
        case C64MODEL_C64C_NTSC:
            strcpy(tmpstr, "C64C"); break;
        case C64MODEL_C64SX_PAL:
        case C64MODEL_C64SX_NTSC:
            strcpy(tmpstr, "SX-64"); break;
        case C64MODEL_PET64_PAL:
        case C64MODEL_PET64_NTSC:
            strcpy(tmpstr, "PET64"); break;
        case C64MODEL_C64_GS:
            strcpy(tmpstr, "C64GS"); break;
        case C64MODEL_C64_JAP:
            strcpy(tmpstr, "C64JP"); break;
        case C64MODEL_ULTIMAX:
            strcpy(tmpstr, "MAX"); break;
    }
    sprintf(&statusbar_text[STATUSBAR_MODEL_POS-zoomed_centering], "%-5s", tmpstr);

    /* Memory */
    unsigned memory = 0;
#if defined(__XSCPU64__)
    memory = vice_opt.SIMMSize * 1024;
#else
    memory = vice_opt.REUsize;
#endif

    tmpstr[0] = '\0';
    snprintf(tmpstr, sizeof(tmpstr), "%5dkB", memory);
    sprintf(&statusbar_text[STATUSBAR_MODEL_POS-9-zoomed_centering], "%7s", tmpstr);
#elif defined(__XVIC__)
    /* Model */
    unsigned model = vice_opt.Model;
    if (request_model_set > -1 && request_model_set != model)
        model = request_model_set;

    tmpstr[0] = '\0';
    switch (model)
    {
        case VIC20MODEL_VIC20_PAL:
        case VIC20MODEL_VIC20_NTSC:
            strcpy(tmpstr, "VIC20"); break;
        case VIC20MODEL_VIC21:
            strcpy(tmpstr, "VIC21"); break;
    }
    sprintf(&statusbar_text[STATUSBAR_MODEL_POS-zoomed_centering], "%-5s", tmpstr);

    /* Memory */
    unsigned memory = 0;
    memory = (vic20mem_forced > -1) ? vic20mem_forced : vice_opt.VIC20Memory;
    if (!memory && vice_opt.Model == VIC20MODEL_VIC21)
        memory = 3;

    tmpstr[0] = '\0';
    int vic20mems[6]  = {0, 3, 8, 16, 24, 35};
    snprintf(tmpstr, sizeof(tmpstr), "%2dkB", vic20mems[memory]);
    sprintf(&statusbar_text[STATUSBAR_MODEL_POS-8-zoomed_centering], "%5s", tmpstr);
#endif

    if (uistatusbar_state & UISTATUSBAR_ACTIVE) {
        uistatusbar_state |= UISTATUSBAR_REPAINT;
    }
}

static int per = 0;
static int fps = 0;
static char fps_str[3] = {0};
static int warp = 0;
static int paused = 0;

static void display_speed(void)
{
    sprintf(&statusbar_text[STATUSBAR_SPEED_POS], "%2s", fps_str);

    if (uistatusbar_state & UISTATUSBAR_ACTIVE) {
        uistatusbar_state |= UISTATUSBAR_REPAINT;
    }
}

unsigned int imagename_timer = 0;
static int drive_enabled = 0;
static int drive_empty = 0;
static int drive_pwm = 0;

void display_current_image(const char *image, bool inserted)
{
    static char imagename[RETRO_PATH_MAX] = {0};
    static char imagename_prev[RETRO_PATH_MAX] = {0};

    if (strcmp(image, ""))
    {
        drive_empty = (inserted) ? 0 : 1;
        snprintf(imagename, sizeof(imagename), "%.56s", image);
        snprintf(imagename_prev, sizeof(imagename_prev), "%.56s", imagename);
    }
    else
    {
        drive_empty = 1;
        if (strcmp(imagename_prev, ""))
            snprintf(imagename, sizeof(imagename), "%.56s", imagename_prev);
    }

    if (strcmp(imagename, ""))
    {
        snprintf(&statusbar_text[STATUSBAR_JOY_POS], sizeof(statusbar_text), "%2s%-54s", "  ", imagename);
        imagename_timer = 150;

        if (inserted)
            statusbar_text[0] = (8 | 0x80);
        else if (!strcmp(image, ""))
            statusbar_text[0] = (9 | 0x80);
    }

    if (drive_empty)
    {
        statusbar_text[STATUSBAR_DRIVE8_TRACK_POS] = ' ';
        statusbar_text[STATUSBAR_DRIVE8_TRACK_POS + 1] = ' ';
    }
    else if (drive_enabled)
    {
        statusbar_text[STATUSBAR_DRIVE8_TRACK_POS] = '0';
        statusbar_text[STATUSBAR_DRIVE8_TRACK_POS + 1] = '0';
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
    if (fps > 999) {
        fps /= 1000;
        fps = (fps > 9) ? 9 : fps;
        sprintf(fps_str, "%dK", fps);
    } else if (fps > 99) {
        fps /= 100;
        fps = (fps > 9) ? 9 : fps;
        sprintf(fps_str, "%dC", fps);
    } else {
        sprintf(fps_str, "%2d", fps);
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

/* Drive related UI */
void ui_enable_drive_status(ui_drive_enable_t state, int *drive_led_color)
{
    int drive_number;
    int drive_state = (int)state;
    drive_enabled = drive_state;

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
    if (drive_empty)
        return;

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
    drive_pwm = pwm1;
    vice_led_state[1] = (drive_pwm > 1) ? 1 : 0;
    return;

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
#ifdef SDL_DEBUG
    fprintf(stderr, "%s\n", __func__);
#endif
}

/* Tape related UI */

static int tape_counter = 0;
static int tape_enabled = 0;
static int tape_motor = 0;
static int tape_control = 0;

static void display_tape(void)
{
    char tape_chars[5] = {23, 20, 22, 21, 'R'};

    if (drive_enabled)
        return;

    if (tape_enabled)
        vice_led_state[2] = (tape_control && tape_motor) ? 1 : 0;

    if (tape_enabled && (opt_autoloadwarp & AUTOLOADWARP_TAPE || retro_warp_mode_enabled()) && !retro_warpmode)
    {
        if (tape_control && tape_motor && !retro_warp_mode_enabled())
        {
            resources_set_int("WarpMode", 1);
#if 0
            printf("Tape Warp ON\n");
#endif
        }
        else if ((!tape_control || !tape_motor) && retro_warp_mode_enabled() || !(opt_autoloadwarp & AUTOLOADWARP_TAPE))
        {
            resources_set_int("WarpMode", 0);
#if 0
            printf("Tape Warp OFF\n");
#endif
        }
    }

    if (tape_enabled)
        sprintf(&(statusbar_text[STATUSBAR_TAPE_POS]), "%c%03d", tape_chars[tape_control], tape_counter);
    else
        sprintf(&(statusbar_text[STATUSBAR_TAPE_POS]), "    ");

    if (uistatusbar_state & UISTATUSBAR_ACTIVE) {
        uistatusbar_state |= UISTATUSBAR_REPAINT;
    }
}

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

void uistatusbar_draw(void)
{
    unsigned int i = 0;
    unsigned char c = 0;
    unsigned char s[2] = {0};

    unsigned int char_width = 6;
    unsigned int char_offset = 1;
    int x = 0, y = 0;

    unsigned int color_f, color_b;
    unsigned int color_black, color_white,
            color_red, color_greenb, color_green, color_greend,
            color_brown, color_brownd;
    color_black  = 0;
    color_white  = RGB(255, 255, 255);
    color_red    = RGB(204,   0,   0);
    color_greenb = RGB(  0, 204,   0);
    color_green  = RGB(  0,  85,   0);
    color_greend = RGB(  0,  34,   0);
    color_brown  = RGB(143, 140, 129);
    color_brownd = RGB( 89,  79,  78);
    color_f      = color_white;
    color_b      = color_black;

    /* Statusbar position */
    x = 1;
    if (opt_statusbar & STATUSBAR_TOP)
        y = zoomed_YS_offset + 1;
    else
        y = zoomed_height + zoomed_YS_offset - (char_width + 1) - 1;

    /* Statusbar background */
    int bkg_x = retroXS_offset + x - 1;
    int bkg_y = y - 1;
    int max_width = zoomed_width;
    int bkg_width = max_width;
    int bkg_height = (char_width + 1) + 2;

    /* Right alignment offset */
    int x_align_offset = 2;

    /* Basic mode statusbar background */
    if (opt_statusbar & STATUSBAR_BASIC && imagename_timer == 0)
    {
        if (drive_enabled)
            bkg_width = (char_width * 5) - x_align_offset + 1;
        else if (tape_enabled)
            bkg_width = (char_width * 8) - x_align_offset - 4;
        else
            bkg_width = (char_width * 3) - x_align_offset - 1;

        bkg_x = retroXS_offset + x + max_width - bkg_width - 1;
    }

    draw_fbox(bkg_x, bkg_y, bkg_width, bkg_height, 0, GRAPH_ALPHA_100);

    if (imagename_timer == 0)
        display_joyport();

    for (i = 0; i < MAX_STATUSBAR_LEN; ++i)
    {
        c = statusbar_text[i];
        if (c == 0)
            continue;
        
        /* Default background */
        color_b = color_black;

        /* Drive/tape LED color */
        if (i >= STATUSBAR_TAPE_POS && i < STATUSBAR_SPEED_POS)
        {
            color_f = color_black;

            if (tape_enabled)
            {
                if (tape_motor)
                    color_b = color_brown;
                else
                    color_b = color_brownd;
            }
        }

        /* Drive loading */
        if ((i == STATUSBAR_DRIVE8_TRACK_POS || i == STATUSBAR_DRIVE8_TRACK_POS + 1) && drive_enabled)
        {
            color_b = color_green;

            if (drive_pwm > 1)
                color_b = color_greenb;
            else if (drive_empty)
                color_b = color_greend;

            if (opt_statusbar & STATUSBAR_MINIMAL)
                c = ' ';
        }
        /* Power LED color */
        else if (i == STATUSBAR_SPEED_POS || i == STATUSBAR_SPEED_POS + 1)
        {
            color_f = color_black;
            color_b = color_red;

            if (opt_statusbar & STATUSBAR_MINIMAL)
                c = ' ';
        }

        /* Right alignment for tape/drive/power */
        int x_align = retroXS_offset;
        if (i >= STATUSBAR_TAPE_POS)
            x_align = retroXS_offset + x_align_offset + zoomed_width - (MAX_STATUSBAR_LEN * char_width);

        if (drive_enabled)
        {
            if (i == STATUSBAR_DRIVE8_TRACK_POS || i == STATUSBAR_DRIVE8_TRACK_POS + 1)
                x_align -= 2;
        }
        else if (tape_enabled)
        {
            if (i >= STATUSBAR_TAPE_POS && i < STATUSBAR_SPEED_POS - 4)
                x_align = x_align - 3 + char_width;
            else if (i >= STATUSBAR_TAPE_POS && i < STATUSBAR_SPEED_POS - 1)
                x_align = x_align - 2 + char_width;
        }

        int x_char = x + char_offset + x_align + (i * char_width);

        /* Output */
        snprintf(s, sizeof(s), "%c", c);
        draw_text(x_char, y, color_f, color_b, GRAPH_ALPHA_100, GRAPH_BG_ALL, 1, 1, 10, s);
    }
}
