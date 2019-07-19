#include "kbd.h"
#include "libretro.h"
#include "joystick.h"
#include "keyboard.h"

extern unsigned int cur_port;

int joystick_handle_key(int kcode, int pressed)
{
   BYTE value = 0;

   BYTE j = joystick_value[cur_port];

   switch (kcode)
   {
      default:
         /* (make compiler happy) */
         break;
   }

   if(value)
   {
      if (pressed)
         j |= value;
      else
         j &=~value;

      joystick_value[cur_port] = j;

      //printf("JOY) k:%d v:%d %s\n",kcode,value,pressed?"press":"release");
   }

   return value;
}

int kbd_handle_keydown(int kcode)
{
   //if (!joystick_handle_key(kcode, 1))
      keyboard_key_pressed((signed long)kcode);

   return 0;
}

int kbd_handle_keyup(int kcode)
{
   //if (!joystick_handle_key(kcode, 0))
      keyboard_key_released((signed long)kcode);

   return 0;
}
