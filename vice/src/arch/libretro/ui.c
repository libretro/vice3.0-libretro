/*
 * ui.c - PSP user interface.
 *
 * Written by
 *  Akop Karapetyan <dev@psp.akop.org>
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
#include "machine.h"
#include "cmdline.h"
#include "uistatusbar.h"
#include "resources.h"
#include "sid.h"
#include "userport_joystick.h"
#if  defined(__VIC20__)
#include "vic20model.h"
#elif defined(__PLUS4__)
#include "plus4model.h"
#elif  defined(__X128__)
#include "c128model.h"
#elif  defined(__PET__)
#include "petmodel.h"
#else
#include "c64model.h"
#endif

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

int RETROTDE=0;
int RETRODSE=0;
int RETRODSEVOL=800;
int RETROSTATUS=0;
int RETRORESET=0;
int RETRODRVTYPE=1542;
int RETROSIDMODL=0;
int RETROC64MODL=0;
int RETROUSERPORTJOY=-1;
int RETROEXTPAL=-1;
int RETROAUTOSTARTWARP=0;
int RETROBORDERS=0;
char RETROEXTPALNAME[512]="pepto-pal";
int retro_ui_finalized = 0;

static const cmdline_option_t cmdline_options[] = {
     { NULL }
};

/* Initialization  */
int ui_resources_init(void)
{
   if (machine_class != VICE_MACHINE_VSID)
      return uistatusbar_init_resources();
   return 0;
}

void ui_resources_shutdown(void)
{

}

int ui_init(void)
{
    return 0;
}

void ui_shutdown(void)
{
}

void ui_check_mouse_cursor(void)
{
   /* needed */
}

void ui_error(const char *format, ...)
{

   char text[512];	   	
   va_list	ap;	

   if (format == NULL)return;		

   va_start(ap,format );		
   vsprintf(text, format, ap);	
   va_end(ap);	
   fprintf(stderr, "ui_error: %s\n", text);
}

/* Update all the menus according to the current settings.  */

void ui_update_menus(void)
{
}

int ui_extend_image_dialog(void)
{
   return 0;
}

void ui_dispatch_events(void)
{
}

int ui_cmdline_options_init(void)
{
   return cmdline_register_options(cmdline_options);
}

int ui_init_finish(void)
{
   return 0;
}

int ui_init_finalize(void)
{
   //FIXME

   resources_set_int("Mouse", 0);
   resources_set_int("Mousetype", 0);
   resources_set_int("Mouseport", 1);

   resources_set_int("CrtcFilter", 0);
   resources_set_int("CrtcStretchVertical", 0);

   //RETRO CORE OPT
   if(RETROSTATUS==1) {
      resources_set_int("SDLStatusbar", 1);
   } else if(RETROSTATUS==0) {
      resources_set_int("SDLStatusbar", 0);
   }
   
   if(RETROEXTPAL==-1)resources_set_int("VICIIExternalPalette", 0);
   else {
      resources_set_int("VICIIExternalPalette", 1);
      resources_set_string_sprintf("%sPaletteFile", RETROEXTPALNAME, "VICII");
   }

   if(RETROUSERPORTJOY==-1)resources_set_int("UserportJoy", 0);
   else {
      resources_set_int("UserportJoy", 1);
      resources_set_int("UserportJoyType", RETROUSERPORTJOY);
   }

   if(RETROTDE==1){
	resources_set_int("DriveTrueEmulation", 1);
	resources_set_int("VirtualDevices", 0);
   }
   else if(RETROTDE==0){
	resources_set_int("DriveTrueEmulation", 0);
	resources_set_int("VirtualDevices", 1);
   }

   resources_set_int("DriveSoundEmulation", RETRODSE);
   resources_set_int("DriveSoundEmulationVolume", RETRODSEVOL);

   resources_set_int_sprintf("Drive%iType", RETRODRVTYPE, 8);

   resources_set_int("AutostartWarp", RETROAUTOSTARTWARP);

   sid_set_engine_model((RETROSIDMODL >> 8),  (RETROSIDMODL & 0xff));

#if  defined(__VIC20__) 
   vic20model_set(RETROC64MODL);
#elif defined(__PLUS4__)
   plus4model_set(RETROC64MODL);
#elif defined(__X128__)
   c128model_set(RETROC64MODL);
#elif defined(__PET__)
   petmodel_set(RETROC64MODL);
#else
   c64model_set(RETROC64MODL);
#endif

#if defined(__VIC20__)
   resources_set_int("VICBorderMode", RETROBORDERS);
#elif defined(__PLUS4__)
   resources_set_int("TEDBorderMode", RETROBORDERS);
#else
   resources_set_int("VICIIBorderMode", RETROBORDERS);
#endif

   retro_ui_finalized = 1;

   return 0;
}

char* ui_get_file(const char *format,...)
{
   return NULL;
}



