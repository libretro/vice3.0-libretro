#include "libretro.h"
#include "joystick.h"
#include "keyboard.h"
#include "machine.h"
//#include "fliplist.h"
#include "mouse.h"
#include "resources.h"
#include "autostart.h"

#include "kbd.h"
#include "mousedrv.h"
#include "libretro-core.h"

extern retro_input_poll_t input_poll_cb;
extern retro_input_state_t input_state_cb;

extern void save_bkg();
extern void Screen_SetFullUpdate(int scr);

//#include "retro_strings.h"
//#include "retro_files.h"
//#include "retro_disk_control.h"
//static dc_storage* dc;
//retro_log_printf_t log_cb;

//EMU FLAGS
int SHOWKEY=-1;
int SHIFTON=-1;
int KBMOD=-1;
int RSTOPON=-1;
int CTRLON=-1;
int NPAGE=-1;
int KCOL=1;
int SND=1;
int vkey_pressed;
unsigned char MXjoy[2]; // joy
char Core_Key_Sate[512];
char Core_old_Key_Sate[512];
//int MOUSE_EMULATED=1;
int PAS=4;
int slowdown=0;
int pushi=0; //mouse button
int c64mouse_enable=0;
unsigned int cur_port = 2;
bool num_locked = false;

extern bool retro_load_ok;
extern int mapper_keys[28];
//int vice_statusbar=0;
unsigned int statusbar;

#define EMU_VKBD 1
#define EMU_STATUSBAR 2
#define EMU_JOYPORT 3
#define EMU_RESET 4

//extern void emu_init(void);
//extern void emu_uninit(void);
extern void emu_reset(void);

//void emu_reset()
//{
	//machine_trigger_reset(MACHINE_RESET_MODE_SOFT);
//}

void emu_function(int function) {
    switch (function){
        case EMU_VKBD:
            SHOWKEY=-SHOWKEY;            
            break;
        case EMU_STATUSBAR:
            resources_get_int("SDLStatusbar", &statusbar);
            if(statusbar==0) {
                statusbar=1;
            } else {
                statusbar=0;
            }
            resources_set_int("SDLStatusbar", statusbar);
            break;
        case EMU_JOYPORT:
            cur_port++;
            if(cur_port>2)cur_port=1;
            break;
        case EMU_RESET:
            emu_reset();
            break;
    } 
}

void Keymap_KeyUp(int symkey)
{
	if (symkey == RETROK_NUMLOCK)
		num_locked = false;
	else 
		kbd_handle_keyup(symkey);
		
    printf("KEY U: %d\n", symkey);
}

void Keymap_KeyDown(int symkey)
{
    /*
    //FIXME detect retroarch hotkey
    switch (symkey)
    {
        case RETROK_NUMLOCK:
            num_locked = true;
            break;
        
        //case RETROK_KP_PLUS:	// '+' on keypad: Flip List NEXT
            //fliplist_attach_head(8, 1);
            //break;
        //case RETROK_KP_MINUS:	// '-' on keypad: Flip List PREV
            //fliplist_attach_head(8, 0);
            //break;

        default:
            kbd_handle_keydown(symkey);
            break;
    }
    */
    
    if (symkey == RETROK_NUMLOCK)
        num_locked = true;
    else
        kbd_handle_keydown(symkey);

    printf("KEY D: %d\n", symkey);
}

void app_vkb_handle(void)
{
   static int oldi=-1;
   int i;

   if(oldi!=-1)
   {  
      kbd_handle_keyup(oldi);      
      oldi=-1;
   }

   if(vkey_pressed==-1)return;

   i=vkey_pressed;
   vkey_pressed=-1;


   if(i==-1){
      oldi=-1;
   }
   if(i==-2)
   {
      NPAGE=-NPAGE;oldi=-1;
   }
   else if(i==-3)
   {
      emu_function(EMU_VKBD);
      oldi=-1;
   }
   else if(i==-4)
   {
      emu_function(EMU_JOYPORT);
      oldi=-1;
   }
   else if(i==-5)
   {
      emu_function(EMU_STATUSBAR);
      oldi=-1;
   }
   else
   {

      if(i==-10) //SHIFT
      {
         if(SHIFTON == 1)kbd_handle_keyup(RETROK_LSHIFT);
         else kbd_handle_keydown(RETROK_LSHIFT);
         SHIFTON=-SHIFTON;

         oldi=-1;
      }
      else if(i==-11) //CTRL
      {     
         if(CTRLON == 1)kbd_handle_keyup(RETROK_LCTRL);
         else kbd_handle_keydown(RETROK_LCTRL);
         CTRLON=-CTRLON;

         oldi=-1;
      }
      else if(i==-12) //RSTOP
      {
         if(RSTOPON == 1)kbd_handle_keyup(RETROK_ESCAPE);
         else kbd_handle_keydown(RETROK_ESCAPE); 
         RSTOPON=-RSTOPON;

         oldi=-1;
      }
      else
      {
         oldi=i;
         kbd_handle_keydown(oldi); 
      }

   }


}

// Core input Key(not GUI) 
void Core_Processkey(void)
{
   int i;

   for(i=0;i<320;i++)
      Core_Key_Sate[i]=input_state_cb(0, RETRO_DEVICE_KEYBOARD, 0,i) ? 0x80: 0;

   if(memcmp( Core_Key_Sate,Core_old_Key_Sate , sizeof(Core_Key_Sate) ) )
      for(i=0;i<320;i++)
         if(Core_Key_Sate[i] && Core_Key_Sate[i]!=Core_old_Key_Sate[i]  )
         {	
            if(i==RETROK_LALT)
            {
               //KBMOD=-KBMOD;
               //printf("Modifier alt pressed %d \n",KBMOD); 
               continue;
            }
            //printf("press: %d \n",i);
            Keymap_KeyDown(i);

         }	
         else if ( !Core_Key_Sate[i] && Core_Key_Sate[i]!=Core_old_Key_Sate[i]  )
         {
            if(i==RETROK_LALT)
            {
               //KBMOD=-KBMOD;
               //printf("Modifier alt released %d \n",KBMOD); 
               continue;
            }
            //printf("release: %d \n",i);
            Keymap_KeyUp(i);

         }	

   memcpy(Core_old_Key_Sate,Core_Key_Sate , sizeof(Core_Key_Sate) );

}

// Core input (not GUI) 
int Core_PollEvent(void)
{
    //   RETRO        B    Y    SLT  STA  UP   DWN  LEFT RGT  A    X    L    R    L2   R2   L3   R3  LR  LL  LD  LU  RR  RL  RD  RU
    //   INDEX        0    1    2    3    4    5    6    7    8    9    10   11   12   13   14   15  16  17  18  19  20  21  22  23
    //   C64          BOOT VKB  M/J  R/S  UP   DWN  LEFT RGT  B1   GUI  F7   F1   F5   F3   SPC  1 

    int i;
    static int jbt[24]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    static int vbt[16]={0x1C,0x39,0x01,0x3B,0x01,0x02,0x04,0x08,0x80,0x40,0x15,0x31,0x24,0x1F,0x6E,0x6F};
    static int kbt[4]={0,0,0,0};
    
    // MXjoy[0]=0;
    if(!retro_load_ok)return 1;
    input_poll_cb();

    int mouse_l;
    int mouse_r;
    int16_t mouse_x,mouse_y;
    mouse_x=mouse_y=0;
    
    int LX, LY, RX, RY;
    int threshold=20000;

    //if(SHOWKEY==-1 && pauseg==0)Core_Processkey();
    if(SHOWKEY==-1) Core_Processkey();

    //if(pauseg==0){ // if emulation running

        // Virtual Keyboard
        i=0;
        if (input_state_cb(0, RETRO_DEVICE_KEYBOARD, 0, mapper_keys[24]) && kbt[i]==0){ 
            kbt[i]=1;
            emu_function(EMU_VKBD);
        }   
        else if ( kbt[i]==1 && ! input_state_cb(0, RETRO_DEVICE_KEYBOARD, 0, mapper_keys[24]) ){
            kbt[i]=0;
        }

        // Statusbar
        i=1;
        if (input_state_cb(0, RETRO_DEVICE_KEYBOARD, 0, mapper_keys[25]) && kbt[i]==0){ 
            kbt[i]=1;
            emu_function(EMU_STATUSBAR);
        }
        else if ( kbt[i]==1 && ! input_state_cb(0, RETRO_DEVICE_KEYBOARD, 0, mapper_keys[25]) ){
            kbt[i]=0;
        }

        // Switch Joyport
        i=2;
        if (input_state_cb(0, RETRO_DEVICE_KEYBOARD, 0, mapper_keys[26]) && kbt[i]==0){ 
            kbt[i]=1;
            emu_function(EMU_JOYPORT);
        }   
        else if ( kbt[i]==1 && ! input_state_cb(0, RETRO_DEVICE_KEYBOARD, 0, mapper_keys[26]) ){
            kbt[i]=0;
        }

        // Reset
        i=3;
        if (input_state_cb(0, RETRO_DEVICE_KEYBOARD, 0, mapper_keys[27]) && kbt[i]==0){ 
            kbt[i]=1;
            emu_function(EMU_RESET);
        }   
        else if ( kbt[i]==1 && ! input_state_cb(0, RETRO_DEVICE_KEYBOARD, 0, mapper_keys[27]) ){
            kbt[i]=0;
        }




        if(vice_devices[0]==RETRO_DEVICE_VICE_JOYSTICK){
        //shortcut for joy mode only

            for(i=0;i<16;i++){
            //if( (i<4 || i>8) && i!=2){
                if(i<4 || i>8){ 

                    if ( input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, i) && jbt[i]==0 )
                    {
                        jbt[i]=1;
                        if(mapper_keys[i]==mapper_keys[24]) { // Virtual Keyborad
                            emu_function(EMU_VKBD);
                        } else if(mapper_keys[i]==mapper_keys[25]) { // Statusbar
                            emu_function(EMU_STATUSBAR);
                        } else if(mapper_keys[i]==mapper_keys[26]) { // Switch Joyport
                            emu_function(EMU_JOYPORT);
                        } else if(mapper_keys[i]==mapper_keys[27]) { // Reset
                            emu_function(EMU_RESET);
                        } else {
                            Keymap_KeyDown(mapper_keys[i]);
                        }

                        //printf("kdown: %d %d\n",mapper_keys[i],i);
                    }
                    else if ( jbt[i]==1 && ! input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, i) )
                    {
                        jbt[i]=0;
                        if(mapper_keys[i]==mapper_keys[24]) {
                        } else if(mapper_keys[i]==mapper_keys[25]) {
                        } else if(mapper_keys[i]==mapper_keys[26]) {
                        } else if(mapper_keys[i]==mapper_keys[27]) {
                        } else {
                            Keymap_KeyUp(mapper_keys[i]);
                        }

                        //printf("kup: %d %d\n",mapper_keys[i],i);
                    }
                }
            }

            LX = input_state_cb(0, RETRO_DEVICE_ANALOG, 0, 0);
            LY = input_state_cb(0, RETRO_DEVICE_ANALOG, 0, 1);
            RX = input_state_cb(0, RETRO_DEVICE_ANALOG, 1, 0);
            RY = input_state_cb(0, RETRO_DEVICE_ANALOG, 1, 1);
            
            i=19; // LU
            if(LY < -threshold && jbt[i]==0) {
                jbt[i]=1;
                Keymap_KeyDown(mapper_keys[i]);
            } else if(LY > -threshold && jbt[i]==1) {
                jbt[i]=0;
                Keymap_KeyUp(mapper_keys[i]);
            }
        
            i=18; // LD
            if(LY > threshold && jbt[i]==0) {
                jbt[i]=1;
                Keymap_KeyDown(mapper_keys[i]);
            } else if(LY < threshold && jbt[i]==1) {
                jbt[i]=0;
                Keymap_KeyUp(mapper_keys[i]);
            }

            i=17; // LL
            if(LX < -threshold && jbt[i]==0) {
                jbt[i]=1;
                Keymap_KeyDown(mapper_keys[i]);
            } else if(LX > -threshold && jbt[i]==1) {
                jbt[i]=0;
                Keymap_KeyUp(mapper_keys[i]);
            }

            i=16; // LR
            if(LX > threshold && jbt[i]==0) {
                jbt[i]=1;
                Keymap_KeyDown(mapper_keys[i]);
            } else if(LX < threshold && jbt[i]==1) {
                jbt[i]=0;
                Keymap_KeyUp(mapper_keys[i]);
            }

            i=23; // RU
            if(RY < -threshold && jbt[i]==0) {
                jbt[i]=1;
                Keymap_KeyDown(mapper_keys[i]);
            } else if(RY > -threshold && jbt[i]==1) {
                jbt[i]=0;
                Keymap_KeyUp(mapper_keys[i]);
            }
        
            i=22; // RD
            if(RY > threshold && jbt[i]==0) {
                jbt[i]=1;
                Keymap_KeyDown(mapper_keys[i]);
            } else if(RY < threshold && jbt[i]==1) {
                jbt[i]=0;
                Keymap_KeyUp(mapper_keys[i]);
            }

            i=21; // RL
            if(RX < -threshold && jbt[i]==0) {
                jbt[i]=1;
                Keymap_KeyDown(mapper_keys[i]);
            } else if(RX > -threshold && jbt[i]==1) {
                jbt[i]=0;
                Keymap_KeyUp(mapper_keys[i]);
            }

            i=20; // RR
            if(RX > threshold && jbt[i]==0) {
                jbt[i]=1;
                Keymap_KeyDown(mapper_keys[i]);
            } else if(RX < threshold && jbt[i]==1) {
                jbt[i]=0;
                Keymap_KeyUp(mapper_keys[i]);
            }
        
            //printf("analogs: lx:%d ly:%d,\t rx:%d ry:%d\n", LX, LY, RX, RY);

        }//if vice_devices=joy

    //}//if pauseg=0
    
    return 1;

   /*
   i=2;//mouse/joy toggle
   if ( input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, i) && jbt[i]==0 )
      jbt[i]=1;
   else if ( jbt[i]==1 && ! input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, i) )
   {
      jbt[i]=0;
      MOUSE_EMULATED=-MOUSE_EMULATED;	  
   }
   */

   if(slowdown>0)return 0;

   //if(MOUSE_EMULATED==1){

      if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT))mouse_x += PAS;
      if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT))mouse_x -= PAS;
      if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN))mouse_y += PAS;
      if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP))mouse_y -= PAS;
      mouse_l = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A);
      mouse_r = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B);
   //}
   //else {

      mouse_x = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_X);
      mouse_y = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_Y);
      mouse_l = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_LEFT);
      mouse_r = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_RIGHT);
   //}

   slowdown=1;

   static int mmbL=0,mmbR=0;

   if(mmbL==0 && mouse_l){

      mmbL=1;		
      pushi=1;
   }
   else if(mmbL==1 && !mouse_l) {

      mmbL=0;
      pushi=0;
   }

   if(mmbR==0 && mouse_r){
      mmbR=1;		
   }
   else if(mmbR==1 && !mouse_r) {
      mmbR=0;
   }

   if(pauseg==0 && c64mouse_enable){

      mouse_move((int)mouse_x, (int)mouse_y);
      mouse_button(0,mmbL);
      mouse_button(1,mmbR);
  }

  return 1;
}

void retro_poll_event(int joyon)
{
	if(joyon && (
	    input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP) ||
	    input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN) ||
	    input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT) ||
	    input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT)
	    ) )
	{
    } else {
	    Core_PollEvent();
    }

	if(joyon && SHOWKEY==-1) // retro joypad take control over keyboard joy
	{
		int retro_port;
		for (retro_port = 0; retro_port <= 4; retro_port++) {

			int vice_port = cur_port;

			if (retro_port == 1) { // second joypad controls other player
				if (cur_port == 2) {
					vice_port = 1;
				} else {
					vice_port = 2;
				}
			} else if (retro_port == 2) {
				vice_port = 3;
			} else if (retro_port == 3) {
				vice_port = 4;
			} else if (retro_port == 4) {
				vice_port = 5;
			}

			BYTE j = joystick_value[vice_port];
			
			if (input_state_cb(retro_port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP) || 
			    (vice_port == 1 && input_state_cb(retro_port, RETRO_DEVICE_KEYBOARD, 0, RETROK_KP9)) ||
			    (vice_port == 2 && input_state_cb(retro_port, RETRO_DEVICE_KEYBOARD, 0, RETROK_KP8)) 			
			){
				j |= 0x01;
			} else {
				j &= ~0x01;
			}
			if (input_state_cb(retro_port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN) || 
			    (vice_port == 1 && input_state_cb(retro_port, RETRO_DEVICE_KEYBOARD, 0, RETROK_KP3)) ||
			    (vice_port == 2 && input_state_cb(retro_port, RETRO_DEVICE_KEYBOARD, 0, RETROK_KP2)) 			
			){
				j |= 0x02;
			} else {
				j &= ~0x02;
			}
			if (input_state_cb(retro_port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT) || 
			    (vice_port == 1 && input_state_cb(retro_port, RETRO_DEVICE_KEYBOARD, 0, RETROK_KP7)) ||
			    (vice_port == 2 && input_state_cb(retro_port, RETRO_DEVICE_KEYBOARD, 0, RETROK_KP4)) 
			){
				j |= 0x04;
			} else {
				j &=~ 0x04;
			}
			if (input_state_cb(retro_port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT) || 
			    (vice_port == 1 && input_state_cb(retro_port, RETRO_DEVICE_KEYBOARD, 0, RETROK_KP1)) ||
			    (vice_port == 2 && input_state_cb(retro_port, RETRO_DEVICE_KEYBOARD, 0, RETROK_KP6)) 			    
            ){
				j |= 0x08;
			} else {
				j &= ~0x08;
			}
			if (input_state_cb(retro_port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A) || 
			    (vice_port == 1 && input_state_cb(retro_port, RETRO_DEVICE_KEYBOARD, 0, RETROK_KP0)) ||
			    (vice_port == 2 && input_state_cb(retro_port, RETRO_DEVICE_KEYBOARD, 0, RETROK_KP5)) 
			){
				j |= 0x10;
			} else {
				j &= ~0x10;
			}
			
			joystick_value[vice_port] = j;
		}
	}

}

