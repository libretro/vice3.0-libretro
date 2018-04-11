#include "libretro.h"
#include "libretro-core.h"

//CORE VAR
#ifdef _WIN32
char slash = '\\';
#else
char slash = '/';
#endif

bool retro_load_ok = false;

char RETRO_DIR[512];

char DISKA_NAME[512]="\0";
char DISKB_NAME[512]="\0";
char TAPE_NAME[512]="\0";

int mapper_keys[16]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
char keys[4096];
char buf[10][4096];

// Our virtual time counter, increased by retro_run()
long microSecCounter=0;
int cpuloop=1;

#ifdef FRONTEND_SUPPORTS_RGB565
	uint16_t *Retro_Screen;
	uint16_t bmp[WINDOW_SIZE];
	uint16_t save_Screen[WINDOW_SIZE];
#else
	unsigned int *Retro_Screen;
	unsigned int bmp[WINDOW_SIZE];
	unsigned int save_Screen[WINDOW_SIZE];
#endif 

int vice_statusbar=0;

//SOUND
short signed int SNDBUF[1024*2];
//FIXME: handle 50/60
int snd_sampler = 44100 / 50;

//PATH
char RPATH[512];

int pauseg=0; //emu status run/pause/end
int want_quit=0;

extern int MOUSE_EMULATED;
extern int SHOWKEY;

extern int app_init(void);
extern int app_free(void);
extern int app_render(int poll);

int CROP_WIDTH;
int CROP_HEIGHT;
int VIRTUAL_WIDTH;

int retroXS=0;
int retroYS=0;
int retroW=1024;
int retroH=768;
int retrow=1024;
int retroh=768;
int lastW=1024;
int lastH=768;

#include "vkbd.i"

unsigned vice_devices[ 2 ];

extern int RETROJOY,RETROTDE,RETROSTATUS,RETRODRVTYPE,RETROSIDMODL,RETROC64MODL,RETROUSERPORTJOY;
extern int retrojoy_init,retro_ui_finalized;
extern unsigned int cur_port;
extern void set_drive_type(int drive,int val);
extern void set_truedrive_emulation(int val);
extern void reset_mouse_pos();

//VICE DEF BEGIN
#include "resources.h"
#include "sid.h"
#include "c64model.h"
#include "userport_joystick.h"
#if  defined(__VIC20__)
#include "vic20model.h"
#elif defined(__PLUS4__)
#include "plus4model.h"
#elif defined(__X128__)
#include "c128model.h"
#endif
//VICE DEF END

extern void Emu_init(void);
extern void Emu_uninit(void);
extern void vice_main_exit(void);
extern void emu_reset(void);

const char *retro_save_directory;
const char *retro_system_directory;
const char *retro_content_directory;
char retro_system_data_directory[512];;

/*static*/ retro_input_state_t input_state_cb;
/*static*/ retro_input_poll_t input_poll_cb;
static retro_video_refresh_t video_cb;
static retro_audio_sample_t audio_cb;
static retro_audio_sample_batch_t audio_batch_cb;
static retro_environment_t environ_cb;

void retro_set_input_state(retro_input_state_t cb)
{
   input_state_cb = cb;
}

void retro_set_input_poll(retro_input_poll_t cb)
{
   input_poll_cb = cb;
}

static char CMDFILE[512];

int loadcmdfile(char *argv)
{
   int res=0;

   FILE *fp = fopen(argv,"r");

   if( fp != NULL )
   {
      if ( fgets (CMDFILE , 512 , fp) != NULL )
         res=1;	
      fclose (fp);
   }

   return res;
}

int HandleExtension(char *path,char *ext)
{
   int len = strlen(path);

   if (len >= 4 &&
         path[len-4] == '.' &&
         path[len-3] == ext[0] &&
         path[len-2] == ext[1] &&
         path[len-1] == ext[2])
   {
      return 1;
   }

   return 0;
}

#include <ctype.h>

//Args for experimental_cmdline
static char ARGUV[64][1024];
static unsigned char ARGUC=0;

// Args for Core
static char XARGV[64][1024];
static const char* xargv_cmd[64];
int PARAMCOUNT=0;

extern int  skel_main(int argc, char *argv[]);
void parse_cmdline( const char *argv );

void Add_Option(const char* option)
{
   static int first=0;

   if(first==0)
   {
      PARAMCOUNT=0;	
      first++;
   }

   sprintf(XARGV[PARAMCOUNT++],"%s",option);
}

int pre_main(const char *argv)
{
   int i=0;
   bool Only1Arg;

   if (strlen(argv) > strlen("cmd"))
   {
      if( HandleExtension((char*)argv,"cmd") || HandleExtension((char*)argv,"CMD"))
         i=loadcmdfile((char*)argv);     
   }

   if(i==1)
   {
      parse_cmdline(CMDFILE);      
      LOGI("Starting game from command line :%s\n",CMDFILE);  
   }
   else
      parse_cmdline(argv); 

   Only1Arg = (strcmp(ARGUV[0],CORE_NAME) == 0) ? 0 : 1;

   for (i = 0; i<64; i++)
      xargv_cmd[i] = NULL;


   if(Only1Arg)
   {  Add_Option(CORE_NAME);
      /*
         if (strlen(RPATH) >= strlen("crt"))
         if(!strcasecmp(&RPATH[strlen(RPATH)-strlen("crt")], "crt"))
         Add_Option("-cartcrt");
         */

#if defined(__VIC20__)
     if (strlen(RPATH) >= strlen(".20"))
       if (!strcasecmp(&RPATH[strlen(RPATH)-strlen(".20")], ".20"))
	 Add_Option("-cart2");

     if (strlen(RPATH) >= strlen(".40"))
       if (!strcasecmp(&RPATH[strlen(RPATH)-strlen(".40")], ".40"))
	 Add_Option("-cart4");
     
     if (strlen(RPATH) >= strlen(".60"))
       if (!strcasecmp(&RPATH[strlen(RPATH)-strlen(".60")], ".60"))
	 Add_Option("-cart6");

     if (strlen(RPATH) >= strlen(".a0"))
       if (!strcasecmp(&RPATH[strlen(RPATH)-strlen(".a0")], ".a0"))
	 Add_Option("-cartA");

     if (strlen(RPATH) >= strlen(".b0"))
       if (!strcasecmp(&RPATH[strlen(RPATH)-strlen(".b0")], ".b0"))
	 Add_Option("-cartB");
#endif

     Add_Option(RPATH/*ARGUV[0]*/);
   }
   else
   { // Pass all cmdline args
      for(i = 0; i < ARGUC; i++)
         Add_Option(ARGUV[i]);
   }

   for (i = 0; i < PARAMCOUNT; i++)
   {
      xargv_cmd[i] = (char*)(XARGV[i]);
      LOGI("%2d  %s\n",i,XARGV[i]);
   }

   skel_main(PARAMCOUNT,( char **)xargv_cmd); 

   xargv_cmd[PARAMCOUNT - 2] = NULL;

   return 0;
}

void parse_cmdline(const char *argv)
{
   char *p,*p2,*start_of_word;
   int c,c2;
   static char buffer[512*4];
   enum states { DULL, IN_WORD, IN_STRING } state = DULL;

   strcpy(buffer,argv);
   strcat(buffer," \0");

   for (p = buffer; *p != '\0'; p++)
   {
      c = (unsigned char) *p; /* convert to unsigned char for is* functions */
      switch (state)
      {
         case DULL: /* not in a word, not in a double quoted string */
            if (isspace(c)) /* still not in a word, so ignore this char */
               continue;
            /* not a space -- if it's a double quote we go to IN_STRING, else to IN_WORD */
            if (c == '"')
            {
               state = IN_STRING;
               start_of_word = p + 1; /* word starts at *next* char, not this one */
               continue;
            }
            state = IN_WORD;
            start_of_word = p; /* word starts here */
            continue;
         case IN_STRING:
            /* we're in a double quoted string, so keep going until we hit a close " */
            if (c == '"')
            {
               /* word goes from start_of_word to p-1 */
               //... do something with the word ...
               for (c2 = 0,p2 = start_of_word; p2 < p; p2++, c2++)
                  ARGUV[ARGUC][c2] = (unsigned char) *p2;
               ARGUC++; 

               state = DULL; /* back to "not in word, not in string" state */
            }
            continue; /* either still IN_STRING or we handled the end above */
         case IN_WORD:
            /* we're in a word, so keep going until we get to a space */
            if (isspace(c))
            {
               /* word goes from start_of_word to p-1 */
               //... do something with the word ...
               for (c2 = 0,p2 = start_of_word; p2 <p; p2++,c2++)
                  ARGUV[ARGUC][c2] = (unsigned char) *p2;
               ARGUC++; 

               state = DULL; /* back to "not in word, not in string" state */
            }
            continue; /* either still IN_WORD or we handled the end above */
      }	
   }
}

long GetTicks(void) {
   // NOTE: Cores should normally not depend on real time, so we return a
   // counter here
   // GetTicks() is used by vsyncarch_gettime() which is used by
   // * Vsync (together with sleep) to sync to 50Hz
   // * Mouse timestamps
   // * Networking
   // Returning a frame based msec counter could potentially break
   // networking but it's not something libretro uses at the moment.
   return microSecCounter;
} 

void save_bkg(void)
{
   memcpy(save_Screen,Retro_Screen,PITCH*WINDOW_SIZE);
}

void restore_bgk(void)
{
   memcpy(Retro_Screen,save_Screen,PITCH*WINDOW_SIZE);
}

void Screen_SetFullUpdate(int scr)
{
   if(scr==0 ||scr>1)
      memset(&Retro_Screen, 0, sizeof(Retro_Screen));
   if(scr>0)
      memset(&bmp,0,sizeof(bmp));
}

int keyId(const char *val)
{
   int i=0;
   while (keyDesc[i]!=NULL)
   {
      if (!strcmp(keyDesc[i],val))
         return keyVal[i];
      i++;
   }
   return 0;
}

void retro_set_environment(retro_environment_t cb)
{
   environ_cb = cb;

   static const struct retro_controller_description p1_controllers[] = {
      { "Vice Joystick", RETRO_DEVICE_VICE_JOYSTICK },
      { "Vice Keyboard", RETRO_DEVICE_VICE_KEYBOARD },
   };
   static const struct retro_controller_description p2_controllers[] = {
      { "Vice Joystick", RETRO_DEVICE_VICE_JOYSTICK },
      { "Vice Keyboard", RETRO_DEVICE_VICE_KEYBOARD },
   };


   static const struct retro_controller_info ports[] = {
      { p1_controllers, 2  }, // port 1
      { p2_controllers, 2  }, // port 2
      { NULL, 0 }
   };

   cb( RETRO_ENVIRONMENT_SET_CONTROLLER_INFO, (void*)ports );

   int i=0;
   while (keyDesc[i]!=NULL)
   {
      if (i == 0)
         strcpy (keys, keyDesc[i]);
      else
         strcat (keys, keyDesc[i]);
      if (keyDesc[i+1]!=NULL)
         strcat (keys, "|");
      i++;
   }

   snprintf(buf[0],sizeof(buf[0]), "RetroPad Y; %s|%s","RETROK_F9",     keys);
   snprintf(buf[1],sizeof(buf[1]), "RetroPad X; %s|%s","RETROK_RETURN",     keys);
   snprintf(buf[2],sizeof(buf[2]), "RetroPad B; %s|%s","RETROK_ESCAPE",     keys);
   snprintf(buf[3],sizeof(buf[3]), "RetroPad L; %s|%s","RETROK_KP_PLUS",     keys);
   snprintf(buf[4],sizeof(buf[4]), "RetroPad R; %s|%s","RETROK_KP_MINUS",     keys);
   snprintf(buf[5],sizeof(buf[5]), "RetroPad L2; %s|%s","RETROK_KP_MULTIPLY",    keys);
   snprintf(buf[6],sizeof(buf[6]), "RetroPad R2; %s|%s","RETROK_ESCAPE",   keys);
   snprintf(buf[7],sizeof(buf[7]), "RetroPad L3; %s|%s","RETROK_TAB",   keys);
   snprintf(buf[8],sizeof(buf[8]), "RetroPad R3; %s|%s","RETROK_F5",  keys);
   snprintf(buf[9],sizeof(buf[9]),"RetroPad START; %s|%s","RETROK_KP_DIVIDE", keys);

   struct retro_variable variables[] = {
      {
         "vice_Statusbar",
         "Status Bar; disabled|enabled",
      },
      {
         "vice_Drive8Type",
         "Drive8Type; 1541|1540|1542|1551|1570|1571|1573|1581|2000|4000|2031|2040|3040|4040|1001|8050|8250",
      },
      {
         "vice_DriveTrueEmulation",
         "DriveTrueEmulation; enabled|disabled",
      },
      {
         "vice_SidModel",
         "SidModel; 6581F|8580F|6581R|8580R|8580RD",
      },
#if  defined(__VIC20__)
      {
         "vice_VIC20Model",
         "VIC20Model; VIC20MODEL_VIC20_PAL|VIC20MODEL_VIC20_NTSC|VIC20MODEL_VIC21|VIC20MODEL_UNKNOWN",
      },
#elif  defined(__PLUS4__)
      {
         "vice_PLUS4Model",
         "PLUS4Model; PLUS4MODEL_C16_PAL|PLUS4MODEL_C16_NTSC|PLUS4MODEL_PLUS4_PAL|PLUS4MODEL_PLUS4_NTSC|PLUS4MODEL_V364_NTSC|PLUS4MODEL_232_NTSC|PLUS4MODEL_UNKNOWN",
      },
#elif  defined(__X128__)
      {
         "vice_C128Model",
         "C128Model; C128MODEL_C128_PAL|C128MODEL_C128DCR_PAL|C128MODEL_C128_NTSC|C128MODEL_C128DCR_NTSC|C128MODEL_UNKNOWN",
      },
#else
      {
         "vice_C64Model",
         "C64Model; C64MODEL_C64_PAL|C64MODEL_C64C_PAL|C64MODEL_C64_OLD_PAL|C64MODEL_C64_NTSC|C64MODEL_C64C_NTSC|C64MODEL_C64_OLD_NTSC|C64MODEL_C64_PAL_N|C64MODEL_C64SX_PAL|C64MODEL_C64SX_NTSC|C64MODEL_C64_JAP|C64MODEL_C64_GS|C64MODEL_PET64_PAL|C64MODEL_PET64_NTSC|C64MODEL_ULTIMAX|C64MODEL_UNKNOWN",
      },
#endif
      {
         "vice_UserportJoyType",
         "4-player adapter; none|Protovision_CGA|PET|Hummer|OEM|Hit|Kingsoft|Starbyte",
      },
      {
         "vice_JoyPort",
         "Controller0 port; port_2|port_1",
      },
      {
         "vice_RetroJoy",
         "Retro joy0; enabled|disabled",
      },
      {
         "vice_Controller",
         "Controller0 type; keyboard|joystick",
      },
   { "vice_mapper_y", buf[0] },
   { "vice_mapper_x", buf[1] },
   { "vice_mapper_b", buf[2] },
   { "vice_mapper_l", buf[3] },
   { "vice_mapper_r", buf[4] },
   { "vice_mapper_l2", buf[5] },
   { "vice_mapper_r2", buf[6] },
   { "vice_mapper_l3", buf[7] },
   { "vice_mapper_r3", buf[8] },
   { "vice_mapper_start", buf[9] },

      { NULL, NULL },
   };

   cb(RETRO_ENVIRONMENT_SET_VARIABLES, variables);

   bool noGame = true;
   environ_cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &noGame);
}

static void update_variables(void)
{

   struct retro_variable var;

   var.key = "vice_Statusbar";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if(retro_ui_finalized){
         if (strcmp(var.value, "enabled") == 0)
            vice_statusbar=1;
         if (strcmp(var.value, "disabled") == 0)
            vice_statusbar=0;
      }
      else {
         if (strcmp(var.value, "enabled") == 0)RETROSTATUS=1;
         if (strcmp(var.value, "disabled") == 0)RETROSTATUS=0;
      }
   }

   var.key = "vice_Drive8Type";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      char str[100];
      int val;
      snprintf(str, sizeof(str), "%s", var.value);
      val = strtoul(str, NULL, 0);

      if(retro_ui_finalized)
         set_drive_type(8, val);
      else RETRODRVTYPE=val;
   }

   var.key = "vice_DriveTrueEmulation";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if(retro_ui_finalized){
         if (strcmp(var.value, "enabled") == 0){
            set_truedrive_emulation(1);
	    resources_set_int("VirtualDevices", 0);
         }
         if (strcmp(var.value, "disabled") == 0){
            set_truedrive_emulation(0);
            resources_set_int("VirtualDevices", 1);
	 }
      }
      else  {
         if (strcmp(var.value, "enabled") == 0)RETROTDE=1;
         if (strcmp(var.value, "disabled") == 0)RETROTDE=0;
      }
   }

   var.key = "vice_SidModel";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int eng=0,modl=0,sidmdl=0;

      if (strcmp(var.value, "6581F") == 0){eng=0;modl=0;}
      else if (strcmp(var.value, "8580F") == 0){eng=0;modl=1;}
      else if (strcmp(var.value, "6581R") == 0){eng=1;modl=0;}
      else if (strcmp(var.value, "8580R") == 0){eng=1;modl=1;}
      else if (strcmp(var.value, "8580RD") == 0){eng=1;modl=2;}

      sidmdl=((eng << 8) | modl) ;

      if(retro_ui_finalized)
        sid_set_engine_model(eng, modl);
      else RETROSIDMODL=sidmdl;
   }

#if  defined(__VIC20__)
   var.key = "vice_VIC20Model";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int modl=0;

      if (strcmp(var.value, "VIC20MODEL_VIC20_PAL") == 0)modl=VIC20MODEL_VIC20_PAL;
      else if (strcmp(var.value, "VIC20MODEL_VIC20_NTSC") == 0)modl=VIC20MODEL_VIC20_NTSC;
      else if (strcmp(var.value, "VIC20MODEL_VIC21") == 0)modl=VIC20MODEL_VIC21;
      else if (strcmp(var.value, "VIC20MODEL_UNKNOWN") == 0)modl=VIC20MODEL_UNKNOWN;

      if(retro_ui_finalized)
        vic20model_set(modl);
      else RETROC64MODL=modl;
   }
#elif  defined(__PLUS4__)
   var.key = "vice_PLUS4Model";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int modl=0;

      if (strcmp(var.value, "PLUS4MODEL_C16_PAL") == 0)modl=PLUS4MODEL_C16_PAL;
      else if (strcmp(var.value, "PLUS4MODEL_C16_NTSC") == 0)modl=PLUS4MODEL_C16_NTSC;
      else if (strcmp(var.value, "PLUS4MODEL_PLUS4_PAL") == 0)modl=PLUS4MODEL_PLUS4_PAL;
      else if (strcmp(var.value, "PLUS4MODEL_PLUS4_NTSC") == 0)modl=PLUS4MODEL_PLUS4_NTSC;
      else if (strcmp(var.value, "PLUS4MODEL_V364_NTSC") == 0)modl=PLUS4MODEL_V364_NTSC;
      else if (strcmp(var.value, "PLUS4MODEL_232_NTSC") == 0)modl=PLUS4MODEL_232_NTSC;
      else if (strcmp(var.value, "PLUS4MODEL_UNKNOWN") == 0)modl=PLUS4MODEL_UNKNOWN;

      if(retro_ui_finalized)
        plus4model_set(modl);
      else RETROC64MODL=modl;
   }
#elif  defined(__X128__)
   var.key = "vice_C128Model";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int modl=0;

      if (strcmp(var.value, "C128MODEL_C128_PAL") == 0)modl=C128MODEL_C128_PAL;
      else if (strcmp(var.value, "C128MODEL_C128DCR_PAL") == 0)modl=C128MODEL_C128DCR_PAL;
      else if (strcmp(var.value, "C128MODEL_C128_NTSC") == 0)modl=C128MODEL_C128_NTSC;
      else if (strcmp(var.value, "C128MODEL_C128DCR_NTSC") == 0)modl=C128MODEL_C128DCR_NTSC;
      else if (strcmp(var.value, "C128MODEL_UNKNOWN") == 0)modl=C128MODEL_UNKNOWN;
      if(retro_ui_finalized)
        c128model_set(modl);
      else RETROC64MODL=modl;
   }
#else

   var.key = "vice_C64Model";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int modl=0;

      if (strcmp(var.value, "C64MODEL_C64_PAL") == 0)modl=C64MODEL_C64_PAL;
      else if (strcmp(var.value, "C64MODEL_C64C_PAL") == 0)modl=C64MODEL_C64C_PAL;
      else if (strcmp(var.value, "C64MODEL_C64_OLD_PAL") == 0)modl=C64MODEL_C64_OLD_PAL;
      else if (strcmp(var.value, "C64MODEL_C64_NTSC") == 0)modl=C64MODEL_C64_NTSC;
      else if (strcmp(var.value, "C64MODEL_C64C_NTSC") == 0)modl=C64MODEL_C64C_NTSC;
      else if (strcmp(var.value, "C64MODEL_C64_OLD_NTSC") == 0)modl=C64MODEL_C64_OLD_NTSC;
      else if (strcmp(var.value, "C64MODEL_C64_PAL_N") == 0)modl=C64MODEL_C64_PAL_N;
      else if (strcmp(var.value, "C64MODEL_C64SX_PAL") == 0)modl=C64MODEL_C64SX_PAL;
      else if (strcmp(var.value, "C64MODEL_C64SX_NTSC") == 0)modl=C64MODEL_C64SX_NTSC;
      else if (strcmp(var.value, "C64MODEL_C64_JAP") == 0)modl=C64MODEL_C64_JAP;
      else if (strcmp(var.value, "C64MODEL_C64_GS") == 0)modl=C64MODEL_C64_GS;
      else if (strcmp(var.value, "C64MODEL_PET64_PAL") == 0)modl=C64MODEL_PET64_PAL;
      else if (strcmp(var.value, "C64MODEL_PET64_NTSC") == 0)modl=C64MODEL_PET64_NTSC;
      else if (strcmp(var.value, "C64MODEL_ULTIMAX") == 0)modl=C64MODEL_ULTIMAX;
      else if (strcmp(var.value, "C64MODEL_UNKNOWN") == 0)modl=C64MODEL_UNKNOWN;

      if(retro_ui_finalized)
        c64model_set(modl);
      else RETROC64MODL=modl;

   }
#endif

   var.key = "vice_UserportJoyType";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {

      int joyadaptertype=-1;
      if (strcmp(var.value, "none") == 0)joyadaptertype=-1;
      else if (strcmp(var.value, "Protovision_CGA") == 0)joyadaptertype=USERPORT_JOYSTICK_CGA;
      else if (strcmp(var.value, "PET") == 0)joyadaptertype=USERPORT_JOYSTICK_PET;
      else if (strcmp(var.value, "Hummer") == 0)joyadaptertype=USERPORT_JOYSTICK_HUMMER;
      else if (strcmp(var.value, "OEM") == 0)joyadaptertype=USERPORT_JOYSTICK_OEM;
      else if (strcmp(var.value, "Hit") == 0)joyadaptertype=USERPORT_JOYSTICK_HIT;
      else if (strcmp(var.value, "Kingsoft") == 0)joyadaptertype=USERPORT_JOYSTICK_KINGSOFT;
      else if (strcmp(var.value, "Starbyte") == 0)joyadaptertype=USERPORT_JOYSTICK_STARBYTE;

      if(retro_ui_finalized){
           if(joyadaptertype==-1)resources_set_int("UserportJoy", 0);
           else {
             resources_set_int("UserportJoy", 1);
             resources_set_int("UserportJoyType", joyadaptertype);
           }
      } else {
        RETROUSERPORTJOY=joyadaptertype;
      }
   }

   var.key = "vice_JoyPort";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "port_2") == 0)cur_port=2;
      else if (strcmp(var.value, "port_1") == 0)cur_port=1;
   }

   var.key = "vice_RetroJoy";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if(retrojoy_init){
         if (strcmp(var.value, "enabled") == 0)
            resources_set_int( "RetroJoy", 1);
         if (strcmp(var.value, "disabled") == 0)
            resources_set_int( "RetroJoy", 0);
      }
      else {
         if (strcmp(var.value, "enabled") == 0)RETROJOY=1;
         if (strcmp(var.value, "disabled") == 0)RETROJOY=0;
      }
   }

   var.key = "vice_Controller";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "keyboard") == 0)
         vice_devices[ 0 ] = 259;
      if (strcmp(var.value, "joystick") == 0)
         vice_devices[ 0 ] = 513;	  
   }

   var.key = "vice_mapper_y";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {

      mapper_keys[1] = keyId(var.value);
   }

   var.key = "vice_mapper_x";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {

      mapper_keys[9] = keyId(var.value);
   }

   var.key = "vice_mapper_b";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {

      mapper_keys[0] = keyId(var.value);
   }

   var.key = "vice_mapper_l";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {

      mapper_keys[10] = keyId(var.value);
   }

   var.key = "vice_mapper_r";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {

      mapper_keys[11] = keyId(var.value);
   }

   var.key = "vice_mapper_l2";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {

      mapper_keys[12] = keyId(var.value);
   }

   var.key = "vice_mapper_r2";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {

      mapper_keys[13] = keyId(var.value);
   }

   var.key = "vice_mapper_l3";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {

      mapper_keys[14] = keyId(var.value);
   }

   var.key = "vice_mapper_r3";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {

      mapper_keys[15] = keyId(var.value);
   }

   var.key = "vice_mapper_start";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {

      mapper_keys[3] = keyId(var.value);
   }

}

void Emu_init(void)
{
#ifdef RETRO_AND
   MOUSE_EMULATED=1;
#endif 
   update_variables();
   pre_main(RPATH);
}

void Emu_uninit(void)
{
   vice_main_exit();
}

void retro_shutdown_core(void)
{
   environ_cb(RETRO_ENVIRONMENT_SHUTDOWN, NULL);
}

void retro_reset(void)
{
   microSecCounter = 0;
   emu_reset();
}

struct DiskImage {
    char* fname;
};

static int diskIndex = 0;
static int diskCount = 0;
static struct DiskImage diskImage[80];
static bool ejected = false;
#include <attach.h>

static bool retro_set_eject_state(bool ejected) {
    printf("EJECT %d", (int)ejected);
    if(ejected)
        file_system_detach_disk(8);
    else
        file_system_attach_disk(8, diskImage[diskIndex].fname);
}

/* Gets current eject state. The initial state is 'not ejected'. */
static bool retro_get_eject_state(void) {
    return ejected;
}

/* Gets current disk index. First disk is index 0.
 * If return value is >= get_num_images(), no disk is currently inserted.
 */
static unsigned retro_get_image_index(void) {
    return diskIndex;
}

/* Sets image index. Can only be called when disk is ejected.
 * The implementation supports setting "no disk" by using an 
 * index >= get_num_images().
 */
static bool retro_set_image_index(unsigned index) {
    diskIndex = index;
}

/* Gets total number of images which are available to use. */
static unsigned retro_get_num_images(void) {
    return diskCount;
}


/* Replaces the disk image associated with index.
 * Arguments to pass in info have same requirements as retro_load_game().
 * Virtual disk tray must be ejected when calling this.
 *
 * Replacing a disk image with info = NULL will remove the disk image 
 * from the internal list.
 * As a result, calls to get_image_index() can change.
 *
 * E.g. replace_image_index(1, NULL), and previous get_image_index() 
 * returned 4 before.
 * Index 1 will be removed, and the new index is 3.
 */
static bool retro_replace_image_index(unsigned index,
      const struct retro_game_info *info) {
    if(diskImage[index].fname)
        free(diskImage[index].fname);
    if(info == NULL) {
        memcpy(&diskImage[index], &diskImage[index+1], sizeof(struct DiskImage)*(diskCount-index-1));
        diskCount--;
        if(diskIndex > 0)
            diskIndex--;
    } else
    diskImage[index].fname = strdup(info->path);
}

/* Adds a new valid index (get_num_images()) to the internal disk list.
 * This will increment subsequent return values from get_num_images() by 1.
 * This image index cannot be used until a disk image has been set 
 * with replace_image_index. */
static bool retro_add_image_index(void) {
    diskImage[diskCount].fname = NULL;
    diskCount++;
    return true;
}

static struct retro_disk_control_callback diskControl = {
    retro_set_eject_state,
    retro_get_eject_state,
    retro_get_image_index,
    retro_set_image_index,
    retro_get_num_images,
    retro_replace_image_index,
    retro_add_image_index,
};

void retro_init(void)
{    	
   const char *system_dir = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &system_dir) && system_dir)
   {
      // if defined, use the system directory			
      retro_system_directory=system_dir;		
   }		   

   const char *content_dir = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_CONTENT_DIRECTORY, &content_dir) && content_dir)
   {
      // if defined, use the system directory			
      retro_content_directory=content_dir;		
   }			

   const char *save_dir = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY, &save_dir) && save_dir)
   {
      // If save directory is defined use it, otherwise use system directory
      retro_save_directory = *save_dir ? save_dir : retro_system_directory;      
   }
   else
   {
      // make retro_save_directory the same in case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY is not implemented by the frontend
      retro_save_directory=retro_system_directory;
   }

   if(retro_system_directory==NULL)sprintf(RETRO_DIR, "%s",".");
   else sprintf(RETRO_DIR, "%s", retro_system_directory);

#if defined(__WIN32__) 
   sprintf(retro_system_data_directory, "%s\\data",RETRO_DIR);
#else
   sprintf(retro_system_data_directory, "%s/data",RETRO_DIR);
#endif

#ifdef FRONTEND_SUPPORTS_RGB565
   enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_RGB565;
#else
   enum retro_pixel_format fmt =RETRO_PIXEL_FORMAT_XRGB8888;
#endif

   if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
   {
      fprintf(stderr, "PIXEL FORMAT is not supported.\n");
      exit(0);
   }

   struct retro_input_descriptor inputDescriptors[] = {
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A, "A" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B, "B" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X, "X" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y, "Y" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "Select" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "Start" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "Right" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT, "Left" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP, "Up" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN, "Down" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R, "R" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L, "L" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2, "R2" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2, "L2" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R3, "R3" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L3, "L3" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A, "A" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B, "B" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X, "X" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y, "Y" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "Select" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "Start" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "Right" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT, "Left" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP, "Up" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN, "Down" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R, "R" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L, "L" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2, "R2" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2, "L2" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R3, "R3" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L3, "L3" },
      { 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A, "A" },
      { 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B, "B" },
      { 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X, "X" },
      { 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y, "Y" },
      { 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "Select" },
      { 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "Start" },
      { 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "Right" },
      { 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT, "Left" },
      { 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP, "Up" },
      { 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN, "Down" },
      { 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R, "R" },
      { 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L, "L" },
      { 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2, "R2" },
      { 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2, "L2" },
      { 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R3, "R3" },
      { 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L3, "L3" },
      { 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A, "A" },
      { 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B, "B" },
      { 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X, "X" },
      { 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y, "Y" },
      { 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "Select" },
      { 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "Start" },
      { 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "Right" },
      { 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT, "Left" },
      { 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP, "Up" },
      { 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN, "Down" },
      { 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R, "R" },
      { 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L, "L" },
      { 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2, "R2" },
      { 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2, "L2" },
      { 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R3, "R3" },
      { 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L3, "L3" },
      { 4, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A, "A" },
      { 4, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B, "B" },
      { 4, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X, "X" },
      { 4, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y, "Y" },
      { 4, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "Select" },
      { 4, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "Start" },
      { 4, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "Right" },
      { 4, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT, "Left" },
      { 4, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP, "Up" },
      { 4, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN, "Down" },
      { 4, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R, "R" },
      { 4, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L, "L" },
      { 4, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2, "R2" },
      { 4, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2, "L2" },
      { 4, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R3, "R3" },
      { 4, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L3, "L3" }
   };
   environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, &inputDescriptors);
   environ_cb(RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE, &diskControl);
   microSecCounter = 0;
}

void retro_deinit(void)
{
   app_free(); 
   Emu_uninit(); 
}

unsigned retro_api_version(void)
{
   return RETRO_API_VERSION;
}


void retro_set_controller_port_device( unsigned port, unsigned device )
{
   if ( port < 2 )
   {
      vice_devices[ port ] = device;

      printf(" (%d)=%d \n",port,device);
   }
}

void retro_get_system_info(struct retro_system_info *info)
{
   memset(info, 0, sizeof(*info));
   info->library_name     = "VICE " CORE_NAME;
   info->library_version  = "3.0";
#if defined(__VIC20__)
   info->valid_extensions = "20|40|60|a0|b0|d64|d71|d80|d81|d82|g64|g41|x64|t64|tap|prg|p00|crt|bin|zip|gz|d6z|d7z|d8z|g6z|g4z|x6z|cmd";
#else
   info->valid_extensions = "d64|d71|d80|d81|d82|g64|g41|x64|t64|tap|prg|p00|crt|bin|zip|gz|d6z|d7z|d8z|g6z|g4z|x6z|cmd";
#endif
   info->need_fullpath    = true;
   info->block_extract    = false;

}

void update_geometry()
{
   struct retro_system_av_info system_av_info;
   system_av_info.geometry.base_width = retroW;
   system_av_info.geometry.base_height = retroH;
   system_av_info.geometry.aspect_ratio = (float)4.0/3.0;
   environ_cb(RETRO_ENVIRONMENT_SET_GEOMETRY, &system_av_info);
}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
   /* FIXME handle PAL/NTSC */
   struct retro_game_geometry geom = { 320, 240, retrow, retroh,4.0 / 3.0 };
   struct retro_system_timing timing = { 50.0, 44100.0 };

   info->geometry = geom;
   info->timing   = timing;
}

void retro_set_audio_sample(retro_audio_sample_t cb)
{
   audio_cb = cb;
}

void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb)
{
   audio_batch_cb = cb;
}

void retro_set_video_refresh(retro_video_refresh_t cb)
{
   video_cb = cb;
}

void retro_audio_cb( short l, short r)
{
   audio_cb(l,r);
}

void retro_audiocb(signed short int *sound_buffer,int sndbufsize){
   int x; 
   if(pauseg==0)for(x=0;x<sndbufsize;x++)audio_cb(sound_buffer[x],sound_buffer[x]);	
}

void retro_blit(void)
{
   memcpy(Retro_Screen,bmp,PITCH*WINDOW_SIZE);
}

void retro_run(void)
{
   static int mfirst=1;
   bool updated = false;

   if(lastW!=retroW || lastH!=retroH){
      update_geometry();
      printf("Update Geometry Old(%d,%d) New(%d,%d)\n",lastW,lastH,retroW,retroH);
      lastW=retroW;
      lastH=retroH;
      reset_mouse_pos();
   }

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated)
      update_variables();

   if(mfirst==1)
   {
      mfirst++;
      printf("First time we return from retro_run()!\n");
      retro_load_ok=true;
      app_init();
      memset(SNDBUF,0,1024*2*2);

      Emu_init();
      return;
   }

   if(pauseg==0)
   {
      while(cpuloop==1)
         maincpu_mainloop_retro();
      cpuloop=1;

      retro_blit();
      if(SHOWKEY==1)app_render(0);
   }
   else if (pauseg==1)app_render(1);
   //app_render(pauseg);

   video_cb(Retro_Screen,retroW,retroH,retrow<<PIXEL_BYTES);

   if(want_quit)retro_shutdown_core();
   microSecCounter += (1000000/50);
}

/*
   unsigned int lastdown,lastup,lastchar;
   static void keyboard_cb(bool down, unsigned keycode,
   uint32_t character, uint16_t mod)
   {

   printf( "Down: %s, Code: %d, Char: %u, Mod: %u.\n",
   down ? "yes" : "no", keycode, character, mod);


   if(down)lastdown=keycode;
   else lastup=keycode;
   lastchar=character;

   }
   */

bool retro_load_game(const struct retro_game_info *info)
{
   /*
      struct retro_keyboard_callback cb = { keyboard_cb };
      environ_cb(RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK, &cb);
      */
   const char *full_path = info->path;

   strcpy(RPATH,full_path);

   update_variables();



   return true;
}

void retro_unload_game(void){

   pauseg=-1;
}

unsigned retro_get_region(void)
{
   return RETRO_REGION_NTSC;
}

bool retro_load_game_special(unsigned type, const struct retro_game_info *info, size_t num)
{
   (void)type;
   (void)info;
   (void)num;
   return false;
}

size_t retro_serialize_size(void)
{
   return 0;
}

bool retro_serialize(void *data_, size_t size)
{
   return false;
}

bool retro_unserialize(const void *data_, size_t size)
{
   return false;
}

void *retro_get_memory_data(unsigned id)
{
   (void)id;
   return NULL;
}

size_t retro_get_memory_size(unsigned id)
{
   (void)id;
   return 0;
}

void retro_cheat_reset(void) {}

void retro_cheat_set(unsigned index, bool enabled, const char *code)
{
   (void)index;
   (void)enabled;
   (void)code;
}

