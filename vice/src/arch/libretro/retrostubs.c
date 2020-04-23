#include "libretro.h"
#include "libretro-core.h"
#include "vkbd.h"

#include "joystick.h"
#include "keyboard.h"
#include "machine.h"
#include "mouse.h"
#include "resources.h"
#include "autostart.h"
#include "datasette.h"

#include "kbd.h"
#include "mousedrv.h"
#include "archdep.h"

extern retro_input_poll_t input_poll_cb;
extern retro_input_state_t input_state_cb;

extern void emu_reset(void);
extern unsigned int vice_devices[5];

#ifdef POINTER_DEBUG
int pointer_x = 0;
int pointer_y = 0;
#endif
int last_pointer_x = 0;
int last_pointer_y = 0;

// VKBD starting point: 10x3 == f7
int vkey_pos_x = 10;
int vkey_pos_y = 3;
int vkbd_x_min = 0;
int vkbd_x_max = 0;
int vkbd_y_min = 0;
int vkbd_y_max = 0;

int vkflag[8] = {0};

// Mouse speed flags
#define MOUSE_SPEED_SLOWER 1
#define MOUSE_SPEED_FASTER 2
// Mouse speed multipliers
#define MOUSE_SPEED_SLOW 5
#define MOUSE_SPEED_FAST 2

//EMU FLAGS
int NPAGE=-1;
int SHOWKEY=-1,SHOWKEYTRANS=1;
int SHIFTON=-1;
int TABON=-1;
char core_key_state[512];
char core_old_key_state[512];
bool num_locked = false;
unsigned int statusbar;
unsigned int warpmode;
unsigned int datasette_hotkeys;
unsigned int cur_port=2;
static int cur_port_prev=-1;
extern int cur_port_locked;
extern int mapper_keys[38];					// Bruno total number of hot key binds - default 36
extern unsigned int opt_retropad_options;
extern unsigned int opt_joyport_type;
static int opt_joyport_type_prev = -1;
extern unsigned int opt_dpadmouse_speed;
extern unsigned int opt_analogmouse;
extern unsigned int opt_analogmouse_deadzone;
extern float opt_analogmouse_speed;
unsigned int mouse_value[2]={0};
unsigned int mouse_speed[2]={0};

extern unsigned int zoom_mode_id;
extern unsigned int opt_zoom_mode_id;

extern unsigned int zoom_mode_count; // number of modes to cycle - starts at 0

extern unsigned int bcrop_horiz_mode;
extern unsigned int bcrop_horiz_mode_count; // number of modes to cycle - starts at 0

extern int RETROKEYRAHKEYPAD;
extern int RETROKEYBOARDPASSTHROUGH;
extern bool retro_load_ok;

int turbo_fire_button_disabled=-1;
int turbo_fire_button=-1;
unsigned int turbo_pulse=2;
unsigned int turbo_state[5]={0};
unsigned int turbo_toggle[5]={0};

enum EMU_FUNCTIONS
{
    EMU_VKBD = 0,
    EMU_STATUSBAR,
    EMU_JOYPORT,
    EMU_RESET,
    EMU_ZOOM_MODE,
    EMU_HORIZ_CROP_CYCLE,
    EMU_VERT_CROP_CYCLE,
    EMU_WARP,
    EMU_DATASETTE_HOTKEYS,
    EMU_DATASETTE_STOP,
    EMU_DATASETTE_START,
    EMU_DATASETTE_FORWARD,
    EMU_DATASETTE_REWIND,
    EMU_DATASETTE_RESET,
    EMU_FUNCTION_COUNT
};

/* VKBD_MIN_HOLDING_TIME: Hold a direction longer than this and automatic movement sets in */
/* VKBD_MOVE_DELAY: Delay between automatic movement from button to button */
#define VKBD_MIN_HOLDING_TIME 200
#define VKBD_MOVE_DELAY 50
bool let_go_of_direction = true;
long last_move_time = 0;
long last_press_time = 0;

/* VKBD_STICKY_HOLDING_TIME: Button press longer than this triggers sticky key */
#define VKBD_STICKY_HOLDING_TIME 1000
int let_go_of_button = 1;
long last_press_time_button = 0;
int vkey_pressed=-1;
int vkey_sticky=-1;
int vkey_sticky1=-1;
int vkey_sticky2=-1;

void emu_function(int function)
{
    switch (function)
    {
        case EMU_VKBD:
            SHOWKEY=-SHOWKEY;
            break;
        case EMU_STATUSBAR:
            statusbar = (statusbar) ? 0 : 1;
            resources_set_int("SDLStatusbar", statusbar);
            break;
        case EMU_JOYPORT:
            cur_port_locked = 1;
            cur_port++;
            if (cur_port>2) cur_port = 1;
            break;
        case EMU_RESET:
            emu_reset();
            break;
        case EMU_ZOOM_MODE:
            if (zoom_mode_id == 0 && opt_zoom_mode_id == 0)
                break;
            if (zoom_mode_id > 0)
                zoom_mode_id = 0;
            else if (zoom_mode_id == 0)
                zoom_mode_id = opt_zoom_mode_id;
            break;

        case EMU_HORIZ_CROP_CYCLE:							// Bruno New Cycle Horizontal Crop Modes

			bcrop_horiz_mode = bcrop_horiz_mode + 1;

			if (bcrop_horiz_mode > bcrop_horiz_mode_count)
			{
				bcrop_horiz_mode = 0;
			}
            break;

        case EMU_VERT_CROP_CYCLE:							// Bruno New Cycle Vertical Crop Modes

			zoom_mode_id = zoom_mode_id + 1;

			if (zoom_mode_id > zoom_mode_count)
			{
				zoom_mode_id = 0;
			}
            break;

        case EMU_WARP:
            warpmode = (warpmode) ? 0 : 1;
            resources_set_int("WarpMode", warpmode);
            break;

        case EMU_DATASETTE_HOTKEYS:
            datasette_hotkeys = (datasette_hotkeys) ? 0 : 1;
            break;
        case EMU_DATASETTE_STOP:
            datasette_control(DATASETTE_CONTROL_STOP);
            break;
        case EMU_DATASETTE_START:
            datasette_control(DATASETTE_CONTROL_START);
            break;
        case EMU_DATASETTE_FORWARD:
            datasette_control(DATASETTE_CONTROL_FORWARD);
            break;
        case EMU_DATASETTE_REWIND:
            datasette_control(DATASETTE_CONTROL_REWIND);
            break;
        case EMU_DATASETTE_RESET:
            datasette_control(DATASETTE_CONTROL_RESET);
            break;
    } 
}

void Keymap_KeyUp(int symkey)
{
    /* Num lock ..? */
    if (symkey == RETROK_NUMLOCK)
        num_locked = false;
    /* Prevent LShift keyup if ShiftLock is on */
    else if (symkey == RETROK_LSHIFT)
    {
        if (SHIFTON == -1)
            kbd_handle_keyup(symkey);
    }
    else 
        kbd_handle_keyup(symkey);
}

void Keymap_KeyDown(int symkey)
{
    /* Num lock ..? */
    if (symkey == RETROK_NUMLOCK)
        num_locked = true;
    /* CapsLock / ShiftLock */
    else if (symkey == RETROK_CAPSLOCK)
    {
        if (SHIFTON == 1)
            kbd_handle_keyup(RETROK_LSHIFT);
        else
            kbd_handle_keydown(RETROK_LSHIFT);
        SHIFTON=-SHIFTON;
    }
    /* Cursor keys */
    else if (symkey == RETROK_UP || symkey == RETROK_DOWN || symkey == RETROK_LEFT || symkey == RETROK_RIGHT)
    {
        /* Cursors will not move if CTRL (Tab) actually is pressed, so we need to fake keyup */
        if (TABON == 1)
            kbd_handle_keyup(RETROK_TAB);
            kbd_handle_keydown(symkey);
    }
    else
        kbd_handle_keydown(symkey);
}

void process_key(int disable_physical_cursor_keys)
{
   int i;

   for (i=0; i<320; i++)
      core_key_state[i]=input_state_cb(0, RETRO_DEVICE_KEYBOARD, 0,i) ? 0x80: 0;

   if (memcmp(core_key_state, core_old_key_state, sizeof(core_key_state)))
   {
      for (i=0; i<320; i++)
      {
         if (core_key_state[i] && core_key_state[i]!=core_old_key_state[i])
         {	
            if (i==RETROK_LALT)
               continue;
            else if (i==RETROK_TAB) /* CTRL (Tab) acts as a keyboard enabler */
               TABON=1;
            else if (i==RETROK_CAPSLOCK) /* Allow CapsLock while SHOWKEY */
               ;
            else if (disable_physical_cursor_keys && (i == RETROK_DOWN || i == RETROK_UP || i == RETROK_LEFT || i == RETROK_RIGHT))
               continue;
            else if (SHOWKEY==1)
               continue;
            Keymap_KeyDown(i);
         }
         else if (!core_key_state[i] && core_key_state[i] != core_old_key_state[i])
         {
            if (i==RETROK_LALT)
               continue;
            else if (i==RETROK_TAB)
               TABON=-1;
            else if (i==RETROK_CAPSLOCK)
               ;
            else if (disable_physical_cursor_keys && (i == RETROK_DOWN || i == RETROK_UP || i == RETROK_LEFT || i == RETROK_RIGHT))
               continue;
            //else if (SHOWKEY==1) /* We need to allow keyup while SHOWKEY to prevent the summoning key from staying down, if keyboard is used as a RetroPad */
            //   continue;
            Keymap_KeyUp(i);
         }
      }
   }
   memcpy(core_old_key_state, core_key_state, sizeof(core_key_state));
}

void update_input(int disable_physical_cursor_keys)
{
    //   RETRO        B    Y    SLT  STA  UP   DWN  LEFT RGT  A    X    L    R    L2   R2   L3   R3  LR  LL  LD  LU  RR  RL  RD  RU
    //   INDEX        0    1    2    3    4    5    6    7    8    9    10   11   12   13   14   15  16  17  18  19  20  21  22  23

    static long now = 0;
    static long last_vkey_pressed_time = 0;
    static int last_vkey_pressed = -1;
    static int vkey_sticky1_release = 0;
    static int vkey_sticky2_release = 0;

    static int i = 0, j = 0, mk = 0;
    static int LX = 0, LY = 0, RX = 0, RY = 0;
    static int threshold = 20000;
    static int jbt[2][24] = {0};
    static int kbt[EMU_FUNCTION_COUNT] = {0};
    
    if (!retro_load_ok)
        return;
    now = GetTicks() / 1000;

    if (vkey_sticky && last_vkey_pressed != -1 && last_vkey_pressed > 0)
    {
        if (vkey_sticky1 > -1 && vkey_sticky1 != last_vkey_pressed)
        {
            if (vkey_sticky2 > -1 && vkey_sticky2 != last_vkey_pressed)
                kbd_handle_keyup(vkey_sticky2);
            vkey_sticky2 = last_vkey_pressed;
        }
        else
            vkey_sticky1 = last_vkey_pressed;
    }

    /* Keyup only after button is up */
    if (last_vkey_pressed != -1 && vkflag[4] != 1)
    {
        if (vkey_pressed == -1 && last_vkey_pressed >= 0 && last_vkey_pressed != vkey_sticky1 && last_vkey_pressed != vkey_sticky2)
            kbd_handle_keyup(last_vkey_pressed);

        last_vkey_pressed = -1;
    }

    if (vkey_sticky1_release)
    {
        vkey_sticky1_release=0;
        vkey_sticky1=-1;
        kbd_handle_keyup(vkey_sticky1);
    }
    if (vkey_sticky2_release)
    {
        vkey_sticky2_release=0;
        vkey_sticky2=-1;
        kbd_handle_keyup(vkey_sticky2);
    }

    input_poll_cb();

    /* Iterate hotkeys, skip Datasette hotkeys if Datasette hotkeys are disabled or if VKBD is on */
    int i_last = (datasette_hotkeys && SHOWKEY==-1) ? EMU_DATASETTE_RESET : EMU_DATASETTE_HOTKEYS;  // bruno

    for (i = 0; i <= i_last; i++)
    {
        mk = i + 24; /* Skip RetroPad mappings from mapper_keys */

        /* Key down */
        if (input_state_cb(0, RETRO_DEVICE_KEYBOARD, 0, mapper_keys[mk]) && kbt[i]==0 && mapper_keys[mk]!=0)
        {
            kbt[i]=1;
            switch(mk)
            {
                case 24:
                    emu_function(EMU_VKBD);
                    break;
                case 25:
                    emu_function(EMU_STATUSBAR);
                    break;
                case 26:
                    emu_function(EMU_JOYPORT);
                    break;
                case 27:
                    emu_function(EMU_RESET);
                    break;
                case 28:
                    emu_function(EMU_ZOOM_MODE);
                    break;
                case 29:
                    emu_function(EMU_WARP);
                    break;
                case 30:									// horiz crop mode cycle
                    emu_function(EMU_HORIZ_CROP_CYCLE);
                    break;
                case 31:
                    emu_function(EMU_VERT_CROP_CYCLE);		// vert crop mode cycle
                    break;
                case 32:
                    emu_function(EMU_DATASETTE_HOTKEYS);
                    break;
                case 33:
                    emu_function(EMU_DATASETTE_STOP);
                    break;
                case 34:
                    emu_function(EMU_DATASETTE_START);
                    break;
                case 35:
                    emu_function(EMU_DATASETTE_FORWARD);
                    break;
                case 36:
                    emu_function(EMU_DATASETTE_REWIND);
                    break;
                case 37:
                    emu_function(EMU_DATASETTE_RESET);
                    break;


            }
        }
        /* Key up */
        else if (!input_state_cb(0, RETRO_DEVICE_KEYBOARD, 0, mapper_keys[mk]) && kbt[i]==1 && mapper_keys[mk]!=0)
        {
            kbt[i]=0;
            switch(mk)
            {
                case 29:
                    emu_function(EMU_WARP);
                    break;
            }
        }
    }

    /* The check for kbt[i] here prevents the hotkey from generating C64 key events */
    /* SHOWKEY check is now in process_key() to allow certain keys while SHOWKEY */
    int processkey=1;
    for (i = 0; i < (sizeof(kbt)/sizeof(kbt[0])); i++)
    {
        if (kbt[i] == 1)
        {
            processkey=0;
            break;
        }
    }

    if (processkey && disable_physical_cursor_keys != 2)
        process_key(disable_physical_cursor_keys);

    /* RetroPad hotkeys for ports 1 & 2 */
    for (j = 0; j < 2; j++)
    {
        if (vice_devices[j] == RETRO_DEVICE_JOYPAD)
        {
            LX = input_state_cb(j, RETRO_DEVICE_ANALOG, 0, 0);
            LY = input_state_cb(j, RETRO_DEVICE_ANALOG, 0, 1);
            RX = input_state_cb(j, RETRO_DEVICE_ANALOG, 1, 0);
            RY = input_state_cb(j, RETRO_DEVICE_ANALOG, 1, 1);

            // No left analog remappings with non-joysticks
            if (opt_joyport_type > 1)
                LX = LY = 0;

            for (i = 0; i < 24; i++)
            {
                int just_pressed = 0;
                int just_released = 0;
                if ((i < 4 || i > 7) && i < 16) /* Remappable RetroPad buttons excluding D-Pad */
                {
                    /* Skip VKBD keys if VKBD is visible */
                    if (SHOWKEY==1 && (i==RETRO_DEVICE_ID_JOYPAD_B || i==RETRO_DEVICE_ID_JOYPAD_A || i==RETRO_DEVICE_ID_JOYPAD_Y || i==RETRO_DEVICE_ID_JOYPAD_START))
                        continue;

                    if (input_state_cb(j, RETRO_DEVICE_JOYPAD, 0, i) && jbt[j][i]==0 && i!=turbo_fire_button)
                        just_pressed = 1;
                    else if (!input_state_cb(j, RETRO_DEVICE_JOYPAD, 0, i) && jbt[j][i]==1 && i!=turbo_fire_button)
                        just_released = 1;
                }
                else if (i >= 16) /* Remappable RetroPad analog stick directions */
                {
                    switch (i)
                    {
                        case 16: /* LR */
                            if (LX > threshold && jbt[j][i] == 0) just_pressed = 1;
                            else if (LX < threshold && jbt[j][i] == 1) just_released = 1;
                            break;
                        case 17: /* LL */
                            if (LX < -threshold && jbt[j][i] == 0) just_pressed = 1;
                            else if (LX > -threshold && jbt[j][i] == 1) just_released = 1;
                            break;
                        case 18: /* LD */
                            if (LY > threshold && jbt[j][i] == 0) just_pressed = 1;
                            else if (LY < threshold && jbt[j][i] == 1) just_released = 1;
                            break;
                        case 19: /* LU */
                            if (LY < -threshold && jbt[j][i] == 0) just_pressed = 1;
                            else if (LY > -threshold && jbt[j][i] == 1) just_released = 1;
                            break;
                        case 20: /* RR */
                            if (RX > threshold && jbt[j][i] == 0) just_pressed = 1;
                            else if (RX < threshold && jbt[j][i] == 1) just_released = 1;
                            break;
                        case 21: /* RL */
                            if (RX < -threshold && jbt[j][i] == 0) just_pressed = 1;
                            else if (RX > -threshold && jbt[j][i] == 1) just_released = 1;
                            break;
                        case 22: /* RD */
                            if (RY > threshold && jbt[j][i] == 0) just_pressed = 1;
                            else if (RY < threshold && jbt[j][i] == 1) just_released = 1;
                            break;
                        case 23: /* RU */
                            if (RY < -threshold && jbt[j][i] == 0) just_pressed = 1;
                            else if (RY > -threshold && jbt[j][i] == 1) just_released = 1;
                            break;
                        default:
                            break;
                    }
                }

                if (just_pressed)
                {
                    jbt[j][i] = 1;
                    if (mapper_keys[i] == 0) /* Unmapped, e.g. set to "---" in core options */
                        continue;

                    if (mapper_keys[i] == mapper_keys[24]) /* Virtual keyboard */
                        emu_function(EMU_VKBD);
                    else if (mapper_keys[i] == mapper_keys[25]) /* Statusbar */
                        emu_function(EMU_STATUSBAR);
                    else if (mapper_keys[i] == mapper_keys[26]) /* Switch joyport */
                        emu_function(EMU_JOYPORT);
                    else if (mapper_keys[i] == mapper_keys[27]) /* Reset */
                        emu_function(EMU_RESET);
                    else if (mapper_keys[i] == mapper_keys[28]) /* Toggle zoom mode */
                        emu_function(EMU_ZOOM_MODE);
                    else if (mapper_keys[i] == mapper_keys[29]) /* Hold warp mode */
                        emu_function(EMU_WARP);
                    else if (mapper_keys[i] == mapper_keys[30]) /* Cycle Horiz Crop modes */
                        emu_function(EMU_HORIZ_CROP_CYCLE);
                    else if (mapper_keys[i] == mapper_keys[31]) /* Cycle Vert Crop modes */
                        emu_function(EMU_VERT_CROP_CYCLE);
                    else if (mapper_keys[i] == mapper_keys[32]) /* Datasette hotkeys toggle */
                        emu_function(EMU_DATASETTE_HOTKEYS);
                    else if (datasette_hotkeys && mapper_keys[i] == mapper_keys[33]) /* Datasette stop */
                        emu_function(EMU_DATASETTE_STOP);
                    else if (datasette_hotkeys && mapper_keys[i] == mapper_keys[34]) /* Datasette start */
                        emu_function(EMU_DATASETTE_START);
                    else if (datasette_hotkeys && mapper_keys[i] == mapper_keys[35]) /* Datasette forward */
                        emu_function(EMU_DATASETTE_FORWARD);
                    else if (datasette_hotkeys && mapper_keys[i] == mapper_keys[36]) /* Datasette rewind */
                        emu_function(EMU_DATASETTE_REWIND);
                    else if (datasette_hotkeys && mapper_keys[i] == mapper_keys[37]) /* Datasette reset */
                        emu_function(EMU_DATASETTE_RESET);
                    else if (mapper_keys[i] == -5) /* Mouse speed slower */
                        mouse_speed[j] |= MOUSE_SPEED_SLOWER;
                    else if (mapper_keys[i] == -6) /* Mouse speed faster */
                        mouse_speed[j] |= MOUSE_SPEED_FASTER;
                    else if (mapper_keys[i] == -11) /* Virtual keyboard */
                        emu_function(EMU_VKBD);
                    else if (mapper_keys[i] == -12) /* Statusbar */
                        emu_function(EMU_STATUSBAR);
                    else
                        Keymap_KeyDown(mapper_keys[i]);
                }
                else if (just_released)
                {
                    jbt[j][i] = 0;
                    if (mapper_keys[i] == 0) /* Unmapped, e.g. set to "---" in core options */
                        continue;

                    if (mapper_keys[i] == mapper_keys[24])
                        ; /* nop */
                    else if (mapper_keys[i] == mapper_keys[25])
                        ; /* nop */
                    else if (mapper_keys[i] == mapper_keys[26])
                        ; /* nop */
                    else if (mapper_keys[i] == mapper_keys[27])
                        ; /* nop */
                    else if (mapper_keys[i] == mapper_keys[28])
                        ; /* nop */
                    else if (mapper_keys[i] == mapper_keys[29])
                        emu_function(EMU_WARP);
                    else if (mapper_keys[i] == mapper_keys[30])		// horiz crop mode cycle
                        ; /* nop */
                    else if (mapper_keys[i] == mapper_keys[31])		// vert crop mode cycle
                        ; /* nop */
                    else if (mapper_keys[i] == mapper_keys[32])
                        ; /* nop */
                    else if (datasette_hotkeys && mapper_keys[i] == mapper_keys[33])
                        ; /* nop */
                    else if (datasette_hotkeys && mapper_keys[i] == mapper_keys[34])
                        ; /* nop */
                    else if (datasette_hotkeys && mapper_keys[i] == mapper_keys[35])
                        ; /* nop */
                    else if (datasette_hotkeys && mapper_keys[i] == mapper_keys[36])
                        ; /* nop */
                    else if (datasette_hotkeys && mapper_keys[i] == mapper_keys[37])
                        ; /* nop */
                    else if (mapper_keys[i] == -5) /* Mouse speed slower */
                        mouse_speed[j] &= ~MOUSE_SPEED_SLOWER;
                    else if (mapper_keys[i] == -6) /* Mouse speed faster */
                        mouse_speed[j] &= ~MOUSE_SPEED_FASTER;
                    else if (mapper_keys[i] == -11) /* Virtual keyboard */
                        ; /* nop */
                    else if (mapper_keys[i] == -12) /* Statusbar */
                        ; /* nop */
                    else
                        Keymap_KeyUp(mapper_keys[i]);
                }
            } /* for i */
        } /* if vice_devices[0]==joypad */
    } /* for j */

    /* Virtual keyboard for ports 1 & 2 */
    if (SHOWKEY==1)
    {
        if (vkflag[4]==0) /* Allow directions when key is not pressed */
        {
            if (vkflag[0]==0 && (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP) || input_state_cb(1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP)))
                vkflag[0]=1;
            else if (vkflag[0]==1 && (!input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP) && !input_state_cb(1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP)))
                vkflag[0]=0;

            if (vkflag[1]==0 && (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN) || input_state_cb(1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN)))
                vkflag[1]=1;
            else if (vkflag[1]==1 && (!input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN) && !input_state_cb(1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN)))
                vkflag[1]=0;

            if (vkflag[2]==0 && (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT) || input_state_cb(1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT)))
                vkflag[2]=1;
            else if (vkflag[2]==1 && (!input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT) && !input_state_cb(1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT)))
                vkflag[2]=0;

            if (vkflag[3]==0 && (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT) || input_state_cb(1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT)))
                vkflag[3]=1;
            else if (vkflag[3]==1 && (!input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT) && !input_state_cb(1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT)))
                vkflag[3]=0;
        }
        else /* Release all directions when key is pressed */
        {
            vkflag[0]=0;
            vkflag[1]=0;
            vkflag[2]=0;
            vkflag[3]=0;
        }

        if (vkflag[0] || vkflag[1] || vkflag[2] || vkflag[3])
        {
            if (let_go_of_direction)
                /* just pressing down */
                last_press_time = now;

            if ((now - last_press_time > VKBD_MIN_HOLDING_TIME
              && now - last_move_time > VKBD_MOVE_DELAY)
              || let_go_of_direction)
            {
                last_move_time = now;

                if (vkflag[0])
                    vkey_pos_y -= 1;
                else if (vkflag[1])
                    vkey_pos_y += 1;

                if (vkflag[2])
                    vkey_pos_x -= 1;
                else if (vkflag[3])
                    vkey_pos_x += 1;
            }
            let_go_of_direction = false;
        }
        else
            let_go_of_direction = true;

        if (vkey_pos_x < 0)
            vkey_pos_x=NPLGN-1;
        else if (vkey_pos_x > NPLGN-1)
            vkey_pos_x=0;
        if (vkey_pos_y < 0)
            vkey_pos_y=NLIGN-1;
        else if (vkey_pos_y > NLIGN-1)
            vkey_pos_y=0;

        /* Absolute pointer */
        int p_x = input_state_cb(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_X);
        int p_y = input_state_cb(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_Y);

        if (p_x != 0 && p_y != 0 && (p_x != last_pointer_x || p_y != last_pointer_y))
        {
            int px = (int)((p_x + 0x7fff) * retroW / 0xffff);
            int py = (int)((p_y + 0x7fff) * retroH / 0xffff);
            last_pointer_x = p_x;
            last_pointer_y = p_y;
#ifdef POINTER_DEBUG
            pointer_x = px;
            pointer_y = py;
#endif
            if (px >= vkbd_x_min && px <= vkbd_x_max && py >= vkbd_y_min && py <= vkbd_y_max)
            {
                float vkey_width = (float)(vkbd_x_max - vkbd_x_min) / NPLGN;
                vkey_pos_x = ((px - vkbd_x_min) / vkey_width);

                float vkey_height = (float)(vkbd_y_max - vkbd_y_min) / NLIGN;
                vkey_pos_y = ((py - vkbd_y_min) / vkey_height);

                vkey_pos_x = (vkey_pos_x < 0) ? 0 : vkey_pos_x;
                vkey_pos_x = (vkey_pos_x > NPLGN-1) ? NPLGN-1 : vkey_pos_x;
                vkey_pos_y = (vkey_pos_y < 0) ? 0 : vkey_pos_y;
                vkey_pos_y = (vkey_pos_y > NLIGN-1) ? NLIGN-1 : vkey_pos_y;

#ifdef POINTER_DEBUG
                printf("px:%d py:%d (%d,%d) vkey:%dx%d\n", p_x, p_y, px, py, vkey_pos_x, vkey_pos_y);
#endif
            }
        }

        /* Press Return, RetroPad Start */
        i=RETRO_DEVICE_ID_JOYPAD_START;
        if (vkflag[7]==0 && (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, i) || input_state_cb(1, RETRO_DEVICE_JOYPAD, 0, i)))
        {
            vkflag[7]=1;
            Keymap_KeyDown(RETROK_RETURN);
        }
        else if (vkflag[7]==1 && (!input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, i) && !input_state_cb(1, RETRO_DEVICE_JOYPAD, 0, i)))
        {
            vkflag[7]=0;
            Keymap_KeyUp(RETROK_RETURN);
        }

        /* ShiftLock, RetroPad Y */
        i=RETRO_DEVICE_ID_JOYPAD_Y;
        if (vkflag[6]==0 && (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, i) || input_state_cb(1, RETRO_DEVICE_JOYPAD, 0, i)))
        {
            vkflag[6]=1;
            Keymap_KeyDown(RETROK_CAPSLOCK);
            Keymap_KeyUp(RETROK_CAPSLOCK);
        }
        else if (vkflag[6]==1 && (!input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, i) && !input_state_cb(1, RETRO_DEVICE_JOYPAD, 0, i)))
        {
            vkflag[6]=0;
        }

        /* Transparency toggle, RetroPad A */
        i=RETRO_DEVICE_ID_JOYPAD_A;
        if (vkflag[5]==0 && (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, i) || input_state_cb(1, RETRO_DEVICE_JOYPAD, 0, i)))
        {
            vkflag[5]=1;
            SHOWKEYTRANS=-SHOWKEYTRANS;
        }
        else if (vkflag[5]==1 && (!input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, i) && !input_state_cb(1, RETRO_DEVICE_JOYPAD, 0, i)))
        {
            vkflag[5]=0;
        }

        /* Key press, RetroPad B joyports 1+2 / Keyboard Enter / Pointer */
        i=RETRO_DEVICE_ID_JOYPAD_B;
        if (vkflag[4]==0 && (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, i) || input_state_cb(1, RETRO_DEVICE_JOYPAD, 0, i) || input_state_cb(0, RETRO_DEVICE_KEYBOARD, 0, RETROK_RETURN) || input_state_cb(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_PRESSED)))
        {
            vkey_pressed = check_vkey(vkey_pos_x, vkey_pos_y);
            vkflag[4]=1;

            if (vkey_pressed != -1 && last_vkey_pressed == -1)
            {
                switch (vkey_pressed)
                {
                    case -2:
                        emu_function(EMU_RESET);
                        break;
                    case -3:
                        emu_function(EMU_STATUSBAR);
                        break;
                    case -4:
                        emu_function(EMU_JOYPORT);
                        break;
                    case -5: /* sticky shift */
                        Keymap_KeyDown(RETROK_CAPSLOCK);
                        Keymap_KeyUp(RETROK_CAPSLOCK);
                        break;
                    case -20:
                        if (turbo_fire_button_disabled == -1 && turbo_fire_button == -1)
                            break;
                        else if (turbo_fire_button_disabled != -1 && turbo_fire_button != -1)
                            turbo_fire_button_disabled = -1;

                        if (turbo_fire_button_disabled != -1)
                        {
                            turbo_fire_button = turbo_fire_button_disabled;
                            turbo_fire_button_disabled = -1;
                        }
                        else
                        {
                            turbo_fire_button_disabled = turbo_fire_button;
                            turbo_fire_button = -1;
                        }
                        break;

                    case -11:
                        emu_function(EMU_DATASETTE_STOP);
                        break;
                    case -12:
                        emu_function(EMU_DATASETTE_START);
                        break;
                    case -13:
                        emu_function(EMU_DATASETTE_FORWARD);
                        break;
                    case -14:
                        emu_function(EMU_DATASETTE_REWIND);
                        break;
                    case -15:
                        emu_function(EMU_DATASETTE_RESET);
                        break;

                    default:
                        if (vkey_pressed == vkey_sticky1)
                            vkey_sticky1_release = 1;
                        if (vkey_pressed == vkey_sticky2)
                            vkey_sticky2_release = 1;
                        kbd_handle_keydown(vkey_pressed);
                        break;
                }
            }
            last_vkey_pressed = vkey_pressed;
        }
        else if (vkflag[4]==1 && (!input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, i) && !input_state_cb(1, RETRO_DEVICE_JOYPAD, 0, i) && !input_state_cb(0, RETRO_DEVICE_KEYBOARD, 0, RETROK_RETURN) && !input_state_cb(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_PRESSED)))
        {
            vkey_pressed = -1;
            vkflag[4]=0;
        }

        if (vkflag[4] && vkey_pressed > 0)
        {
            if (let_go_of_button)
                last_press_time_button = now;
            if (now - last_press_time_button > VKBD_STICKY_HOLDING_TIME)
                vkey_sticky = 1;
            let_go_of_button = 0;
        }
        else
        {
            let_go_of_button = 1;
            vkey_sticky = 0;
        }
    }
    //printf("vkey:%d sticky:%d sticky1:%d sticky2:%d, now:%d last:%d\n", vkey_pressed, vkey_sticky, vkey_sticky1, vkey_sticky2, now, last_press_time_button);
}

void retro_poll_event()
{
    /* If RetroPad is controlled with keyboard keys, then prevent RetroPad from generating */
    /* keyboard key presses, this prevents cursor up from becoming a run/stop input */
    if ((vice_devices[0] == RETRO_DEVICE_VICE_JOYSTICK || vice_devices[0] == RETRO_DEVICE_JOYPAD) && TABON==-1 &&
        (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B) ||
         input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y) ||
         input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A) ||
         input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X) ||
         input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L) ||
         input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R) ||
         input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2) ||
         input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2) ||
         input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L3) ||
         input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R3) ||
         input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT) ||
         input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START)
        ) &&
        !RETROKEYBOARDPASSTHROUGH
    )
        update_input(2); /* Skip all keyboard input when fire is pressed */

    else if ((vice_devices[0] == RETRO_DEVICE_VICE_JOYSTICK || vice_devices[0] == RETRO_DEVICE_JOYPAD) && TABON==-1 &&
        (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP) ||
         input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN) ||
         input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT) ||
         input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT)
        ) &&
        !RETROKEYBOARDPASSTHROUGH
    )
        update_input(1); /* Process all inputs but disable cursor keys */

    else if ((vice_devices[1] == RETRO_DEVICE_VICE_JOYSTICK || vice_devices[1] == RETRO_DEVICE_JOYPAD) && TABON==-1 &&
        (input_state_cb(1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B) ||
         input_state_cb(1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y) ||
         input_state_cb(1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A) ||
         input_state_cb(1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X) ||
         input_state_cb(1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L) ||
         input_state_cb(1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R) ||
         input_state_cb(1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2) ||
         input_state_cb(1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2) ||
         input_state_cb(1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L3) ||
         input_state_cb(1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R3) ||
         input_state_cb(1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT) ||
         input_state_cb(1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START) ||
         input_state_cb(1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP) ||
         input_state_cb(1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN) ||
         input_state_cb(1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT) ||
         input_state_cb(1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT)
        ) &&
        !RETROKEYBOARDPASSTHROUGH
    )
        update_input(2); /* Skip all keyboard input from RetroPad 2 */

    else
        update_input(0); /* Process all inputs */

    /* retro joypad take control over keyboard joy */
    /* override keydown, but allow keyup, to prevent key sticking during keyboard use, if held down on opening keyboard */
    /* keyup allowing most likely not needed on actual keyboard presses even though they get stuck also */
    if (TABON==-1)
    {
        int retro_port;
        for (retro_port = 0; retro_port <= 4; retro_port++)
        {
            if (vice_devices[retro_port] == RETRO_DEVICE_VICE_JOYSTICK || vice_devices[retro_port] == RETRO_DEVICE_JOYPAD)
            {
                int vice_port = cur_port;
                BYTE j = 0;

                if (retro_port == 1) /* second joypad controls other player */
                    vice_port = (cur_port == 2) ? 1 : 2;
                else if (retro_port == 2)
                    vice_port = 3;
                else if (retro_port == 3)
                    vice_port = 4;
                else if (retro_port == 4)
                    vice_port = 5;

                // No same port joystick movements with non-joysticks
                if (opt_joyport_type > 1 && vice_port == cur_port)
                    continue;
                // No joystick movements with paddles
                if (opt_joyport_type == 2)
                    continue;

                j = joystick_value[vice_port];

                if (input_state_cb(retro_port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP) ||
                    (RETROKEYRAHKEYPAD && vice_port < 3 && vice_port != cur_port && input_state_cb(0, RETRO_DEVICE_KEYBOARD, 0, RETROK_KP9)) ||
                    (RETROKEYRAHKEYPAD && vice_port < 3 && vice_port == cur_port && input_state_cb(0, RETRO_DEVICE_KEYBOARD, 0, RETROK_KP8))
                )
                    j |= (SHOWKEY==-1) ? 0x01 : j;
                else
                    j &= ~0x01;

                if (input_state_cb(retro_port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN) || 
                    (RETROKEYRAHKEYPAD && vice_port < 3 && vice_port != cur_port && input_state_cb(0, RETRO_DEVICE_KEYBOARD, 0, RETROK_KP3)) ||
                    (RETROKEYRAHKEYPAD && vice_port < 3 && vice_port == cur_port && input_state_cb(0, RETRO_DEVICE_KEYBOARD, 0, RETROK_KP2))
                )
                    j |= (SHOWKEY==-1) ? 0x02 : j;
                else
                    j &= ~0x02;

                if (input_state_cb(retro_port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT) || 
                    (RETROKEYRAHKEYPAD && vice_port < 3 && vice_port != cur_port && input_state_cb(0, RETRO_DEVICE_KEYBOARD, 0, RETROK_KP7)) ||
                    (RETROKEYRAHKEYPAD && vice_port < 3 && vice_port == cur_port && input_state_cb(0, RETRO_DEVICE_KEYBOARD, 0, RETROK_KP4))
                )
                    j |= (SHOWKEY==-1) ? 0x04 : j;
                else
                    j &=~ 0x04;

                if (input_state_cb(retro_port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT) || 
                    (RETROKEYRAHKEYPAD && vice_port < 3 && vice_port != cur_port && input_state_cb(0, RETRO_DEVICE_KEYBOARD, 0, RETROK_KP1)) ||
                    (RETROKEYRAHKEYPAD && vice_port < 3 && vice_port == cur_port && input_state_cb(0, RETRO_DEVICE_KEYBOARD, 0, RETROK_KP6))
                )
                    j |= (SHOWKEY==-1) ? 0x08 : j;
                else
                    j &= ~0x08;
                    
                /* Fire button */
                int fire_button = (opt_retropad_options == 1 || opt_retropad_options == 3) ? RETRO_DEVICE_ID_JOYPAD_Y : RETRO_DEVICE_ID_JOYPAD_B;
                if (mapper_keys[fire_button] != 0)
                    fire_button = -1;

                if ((fire_button > -1 && input_state_cb(retro_port, RETRO_DEVICE_JOYPAD, 0, fire_button)) ||
                    (RETROKEYRAHKEYPAD && vice_port < 3 && vice_port != cur_port && input_state_cb(0, RETRO_DEVICE_KEYBOARD, 0, RETROK_KP0)) ||
                    (RETROKEYRAHKEYPAD && vice_port < 3 && vice_port == cur_port && input_state_cb(0, RETRO_DEVICE_KEYBOARD, 0, RETROK_KP5))
                )
                    j |= (SHOWKEY==-1) ? 0x10 : j;
                else
                    j &= ~0x10;

                /* Jump button */
                int jump_button = -1;
                switch (opt_retropad_options)
                {
                    case 2:
                        jump_button = RETRO_DEVICE_ID_JOYPAD_A;
                        break;
                    case 3:
                        jump_button = RETRO_DEVICE_ID_JOYPAD_B;
                        break;
                }

                if (mapper_keys[jump_button] != 0)
                    jump_button = -1;

                if (jump_button > -1 && input_state_cb(retro_port, RETRO_DEVICE_JOYPAD, 0, jump_button))
                {
                    j |= (SHOWKEY==-1) ? 0x01 : j;
                    j &= ~0x02;
                }
                else if (!input_state_cb(retro_port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP) &&
                    (RETROKEYRAHKEYPAD && vice_port < 3 && vice_port != cur_port && !input_state_cb(0, RETRO_DEVICE_KEYBOARD, 0, RETROK_KP9)) &&
                    (RETROKEYRAHKEYPAD && vice_port < 3 && vice_port == cur_port && !input_state_cb(0, RETRO_DEVICE_KEYBOARD, 0, RETROK_KP8))
                )
                    j &= ~0x01;

                /* Turbo fire */
                if (turbo_fire_button != -1)
                {
                    if (input_state_cb(retro_port, RETRO_DEVICE_JOYPAD, 0, turbo_fire_button))
                    {
                        if (turbo_state[vice_port])
                        {
                            if ((turbo_toggle[vice_port]) == (turbo_pulse))
                                turbo_toggle[vice_port]=1;
                            else
                                turbo_toggle[vice_port]++;

                            if (turbo_toggle[vice_port] > (turbo_pulse / 2))
                                j &= ~0x10;
                            else
                                j |= (SHOWKEY==-1) ? 0x10 : j;
                        }
                        else
                        {
                            turbo_state[vice_port] = 1;
                            j |= (SHOWKEY==-1) ? 0x10 : j;
                        }
                    }
                    else
                    {
                        turbo_state[vice_port] = 0;
                        turbo_toggle[vice_port] = 0;
                    }
                }
                    
                joystick_value[vice_port] = j;
                    
                //if (vice_port == 2) {
                //    printf("Joy %d: Button %d, %2d %d %d\n", vice_port, turbo_fire_button, j, turbo_state[vice_port], turbo_toggle[vice_port]);
                //}
            }
        }
    }

    // Default to joysticks, set both ports
    if (opt_joyport_type == 1)
    {
        if (opt_joyport_type_prev != opt_joyport_type)
        {
            opt_joyport_type_prev = opt_joyport_type;
            resources_set_int("JoyPort1Device", opt_joyport_type);
            resources_set_int("JoyPort2Device", opt_joyport_type);
        }
    }
    // Other than a joystick, set only cur_port
    else if (opt_joyport_type > 1 && SHOWKEY==-1)
    {
        if (opt_joyport_type_prev != opt_joyport_type || cur_port_prev != cur_port)
        {
            opt_joyport_type_prev = opt_joyport_type;
            cur_port_prev = cur_port;

            if (cur_port == 2)
            {
                resources_set_int("JoyPort1Device", 1);
                resources_set_int("JoyPort2Device", opt_joyport_type);
            }
            else
            {
                resources_set_int("JoyPort2Device", 1);
                resources_set_int("JoyPort1Device", opt_joyport_type);
            }
        }

        int j = cur_port - 1;
        int retro_j = 0;
        static float mouse_multiplier[2] = {1};
        static int dpadmouse_speed[2] = {0};
        static long dpadmouse_press[2] = {0};
        static int dpadmouse_pressed[2] = {0};
        static long now = 0;
        now = GetTicks();

        int retro_mouse_x[2] = {0}, retro_mouse_y[2] = {0};
        unsigned int retro_mouse_l[2] = {0}, retro_mouse_r[2] = {0}, retro_mouse_m[2] = {0};
        static unsigned int vice_mouse_l[2] = {0}, vice_mouse_r[2] = {0}, vice_mouse_m[2] = {0};

        int analog_left[2] = {0};
        int analog_right[2] = {0};
        double analog_left_magnitude = 0;
        double analog_right_magnitude = 0;
        int analog_deadzone = 0;
        analog_deadzone = (opt_analogmouse_deadzone * 32768 / 100);

        /* Paddles (opt_joyport_type = 2) share the same joyport, but are meant for 2 players.
           Therefore treat retroport0 vertical axis as retroport1 horizontal axis, and second fire as retroport1 fire. */

        // Joypad buttons
        if (SHOWKEY==-1)
        {
            if (vice_devices[0] == RETRO_DEVICE_JOYPAD && (opt_retropad_options == 1 || opt_retropad_options == 3))
            {
               retro_mouse_l[j] = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y);
               // Paddles-split
               if (opt_joyport_type == 2)
                  retro_mouse_r[j] = input_state_cb(1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y);
               else
                  retro_mouse_r[j] = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B);
            }
            else
            {
               retro_mouse_l[j] = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B);
               // Paddles-split
               if (opt_joyport_type == 2)
                  retro_mouse_r[j] = input_state_cb(1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B);
               else
                  retro_mouse_r[j] = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A);
            }
        }

        // Real mouse buttons
        if (!retro_mouse_l[j] && !retro_mouse_r[j] && !retro_mouse_m[j])
        {
            retro_mouse_l[j] = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_LEFT);
            retro_mouse_r[j] = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_RIGHT);
            retro_mouse_m[j] = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_MIDDLE);
        }

        // Joypad movement
        if (SHOWKEY==-1)
        {
            for (retro_j = 0; retro_j < 2; retro_j++)
            {
                // Digital mouse speed modifiers
                if (dpadmouse_pressed[retro_j] == 0)
                   dpadmouse_speed[retro_j] = opt_dpadmouse_speed;
                if (mouse_speed[retro_j] & MOUSE_SPEED_FASTER)
                   dpadmouse_speed[retro_j] = dpadmouse_speed[retro_j] + 4;
                if (mouse_speed[retro_j] & MOUSE_SPEED_SLOWER)
                   dpadmouse_speed[retro_j] = dpadmouse_speed[retro_j] - 3;

                // Digital mouse acceleration
                if (dpadmouse_pressed[retro_j] == 1)
                {
                   if (now - dpadmouse_press[retro_j] > 500 * 200)
                   {
                      dpadmouse_speed[retro_j]++;
                      dpadmouse_press[retro_j] = now;
                   }
                }

                // Digital mouse speed limits
                if (dpadmouse_speed[retro_j] < 4) dpadmouse_speed[retro_j] = 4;
                if (dpadmouse_speed[retro_j] > 20) dpadmouse_speed[retro_j] = 20;
            }

            if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT))
               retro_mouse_x[j] += dpadmouse_speed[0];
            else if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT))
               retro_mouse_x[j] -= dpadmouse_speed[0];

            // Paddles-split
            if (opt_joyport_type == 2)
            {
               if (input_state_cb(1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT))
                  retro_mouse_y[j] += dpadmouse_speed[1];
               else if (input_state_cb(1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT))
                  retro_mouse_y[j] -= dpadmouse_speed[1];
            }
            else
            {
               if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN))
                  retro_mouse_y[j] += dpadmouse_speed[0];
               else if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP))
                  retro_mouse_y[j] -= dpadmouse_speed[0];
            }

            for (retro_j = 0; retro_j < 2; retro_j++)
            {
                // Acceleration timestamps
                if ((retro_mouse_x[j] != 0 || retro_mouse_y[j] != 0) && dpadmouse_pressed[retro_j] == 0)
                {
                   dpadmouse_press[retro_j] = now;
                   dpadmouse_pressed[retro_j] = 1;
                }
                else if ((retro_mouse_x[j] == 0 && retro_mouse_y[j] == 0) && dpadmouse_pressed[retro_j] == 1)
                {
                   dpadmouse_press[retro_j] = 0;
                   dpadmouse_pressed[retro_j] = 0;
                }
            }
        }

        // Left analog movement
        analog_left[0] = input_state_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X);
        // Paddles split
        if (opt_joyport_type == 2)
           analog_left[1] = -input_state_cb(1, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X);
        else
           analog_left[1] = input_state_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y);
        analog_left_magnitude = sqrt((analog_left[0]*analog_left[0]) + (analog_left[1]*analog_left[1]));
        if (analog_left_magnitude <= analog_deadzone)
        {
            analog_left[0] = 0;
            analog_left[1] = 0;
        }

        for (retro_j = 0; retro_j < 2; retro_j++)
        {
            // Analog stick speed modifiers
            mouse_multiplier[retro_j] = 1;
            if (mouse_speed[retro_j] & MOUSE_SPEED_FASTER)
                mouse_multiplier[retro_j] = mouse_multiplier[retro_j] * MOUSE_SPEED_FAST;
            if (mouse_speed[retro_j] & MOUSE_SPEED_SLOWER)
                mouse_multiplier[retro_j] = mouse_multiplier[retro_j] / MOUSE_SPEED_SLOW;
        }

        if (abs(analog_left[0]) > 0)
        {
            retro_mouse_x[j] = analog_left[0] * 15 * opt_analogmouse_speed / (32768 / mouse_multiplier[0]);
            if (retro_mouse_x[j] == 0 && abs(analog_left[0]) > analog_deadzone)
                retro_mouse_x[j] = (analog_left[0] > 0) ? 1 : -1;
        }

        if (abs(analog_left[1]) > 0)
        {
            retro_mouse_y[j] = analog_left[1] * 15 * opt_analogmouse_speed / (32768 / mouse_multiplier[(opt_joyport_type == 2) ? 1 : 0]);
            if (retro_mouse_y[j] == 0 && abs(analog_left[1]) > analog_deadzone)
                retro_mouse_y[j] = (analog_left[1] > 0) ? 1 : -1;
        }

        // Real mouse movement
        if (!retro_mouse_x[j] && !retro_mouse_y[j])
        {
            retro_mouse_x[j] = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_X);
            retro_mouse_y[j] = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_Y);
        }

        // Ports 1 & 2 to VICE
        for (j = 0; j < 2; j++)
        {
            if (!retro_mouse_l[j] && !retro_mouse_r[j] && !retro_mouse_m[j])
                mouse_value[j+1] = 0;

            // Buttons
            if (retro_mouse_l[j] && !vice_mouse_l[j])
            {
                mouse_button(0, 1);
                vice_mouse_l[j] = 1;
                mouse_value[j+1] |= 0x10;
            }
            else if (!retro_mouse_l[j] && vice_mouse_l[j])
            {
                mouse_button(0, 0);
                vice_mouse_l[j] = 0;
                mouse_value[j+1] &= ~0x10;
            }

            if (retro_mouse_r[j] && !vice_mouse_r[j])
            {
                mouse_button(1, 1);
                vice_mouse_r[j] = 1;
                mouse_value[j+1] |= 0x20;
            }
            else if (!retro_mouse_r[j] && vice_mouse_r[j])
            {
                mouse_button(1, 0);
                vice_mouse_r[j] = 0;
                mouse_value[j+1] &= ~0x20;
            }

            if (retro_mouse_m[j] && !vice_mouse_m[j])
            {
                mouse_button(2, 1);
                vice_mouse_m[j] = 1;
                mouse_value[j+1] |= 0x40;
            }
            else if (!retro_mouse_m[j] && vice_mouse_m[j])
            {
                mouse_button(2, 0);
                vice_mouse_m[j] = 0;
                mouse_value[j+1] &= ~0x40;
            }

            // Movement
            if (retro_mouse_x[j] || retro_mouse_y[j])
            {
                if (retro_mouse_y[j] < 0 && !(mouse_value[j+1] & 0x01))
                    mouse_value[j+1] |= 0x01;
                if (retro_mouse_y[j] > -1 && mouse_value[j+1] & 0x01)
                    mouse_value[j+1] &= ~0x01;

                if (retro_mouse_y[j] > 0 && !(mouse_value[j+1] & 0x02))
                    mouse_value[j+1] |= 0x02;
                if (retro_mouse_y[j] < -1 && mouse_value[j+1] & 0x02)
                    mouse_value[j+1] &= ~0x02;

                if (retro_mouse_x[j] < 0 && !(mouse_value[j+1] & 0x04))
                    mouse_value[j+1] |= 0x04;
                if (retro_mouse_x[j] > -1 && mouse_value[j+1] & 0x04)
                    mouse_value[j+1] &= ~0x04;

                if (retro_mouse_x[j] > 0 && !(mouse_value[j+1] & 0x08))
                    mouse_value[j+1] |= 0x08;
                if (retro_mouse_x[j] < -1 && mouse_value[j+1] & 0x08)
                    mouse_value[j+1] &= ~0x08;

                mouse_move(retro_mouse_x[j], retro_mouse_y[j]);
            }
        }
    }
}

