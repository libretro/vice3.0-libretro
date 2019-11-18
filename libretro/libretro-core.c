#include "libretro.h"
#include "libretro-core.h"
#include "file/file_path.h"
#include "log.h"

#include "archdep.h"
#include "c64.h"
#include "c64mem.h"
#include "mem.h"
#include "machine.h"
#include "snapshot.h"
#include "autostart.h"
#include "tape.h"
#include "attach.h"
#include "interrupt.h"
#include "datasette.h"
#ifndef __PET__
#include "cartridge.h"
#endif
#include "initcmdline.h"
#include "vsync.h"

#include "compat/strcasestr.h"

//CORE VAR
char slash = FSDEV_DIR_SEP_CHR;

bool retro_load_ok = false;

retro_log_printf_t log_cb;

char RETRO_DIR[512];
static snapshot_stream_t* snapshot_stream = NULL;
static int load_trap_happened = 0;
static int save_trap_happened = 0;

int mapper_keys[35] = { 0 };
static char buf[64][4096] = { 0 };

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

//PATH
static char* autostartString = NULL;

extern int SHOWKEY;

extern int app_init(void);
extern int app_free(void);
extern int app_render(void);

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

unsigned int vice_devices[5];
unsigned int opt_theme;
unsigned int retro_region;

extern int RETROTDE;
extern int RETRODSE;
extern int RETRORESET;
extern int RETROSIDMODL;
extern int RETRORESIDSAMPLING;
extern int RETROAUDIOLEAK;
extern int RETROC64MODL;
#if defined(__X128__)
extern int RETROC128COLUMNKEY;
#endif
#if defined(__VIC20__)
extern int RETROVIC20MEM;
#endif
extern int RETROUSERPORTJOY;
extern int RETROEXTPAL;
extern int RETROAUTOSTARTWARP;
extern int RETROBORDERS;
extern int RETROTHEME;
extern int RETROKEYRAHKEYPAD;
extern int RETROKEYBOARDPASSTHROUGH;
extern char RETROEXTPALNAME[512];
extern int retro_ui_finalized;
extern unsigned int cur_port;
extern unsigned int datasette;
extern int cur_port_locked;

extern int turbo_fire_button;
extern unsigned int turbo_pulse;

extern void reset_mouse_pos();
extern uint8_t mem_ram[];
extern int g_mem_ram_size;

//VICE DEF BEGIN
#include "resources.h"
#include "sid.h"
#include "userport_joystick.h"
#if defined(__VIC20__)
#include "c64model.h"
#include "vic20model.h"
#elif defined(__PLUS4__)
#include "c64model.h"
#include "plus4model.h"
#elif defined(__X128__)
#include "c64model.h"
#include "c128model.h"
#elif defined(__PET__)
#include "petmodel.h"
#elif defined(__CBM2__)
#include "cbm2model.h"
#else
#include "c64model.h"
#endif
//VICE DEF END

const char *retro_save_directory;
const char *retro_system_directory;
const char *retro_content_directory;
char retro_system_data_directory[512];

/*static*/ retro_input_state_t input_state_cb;
/*static*/ retro_input_poll_t input_poll_cb;
static retro_video_refresh_t video_cb;
static retro_audio_sample_t audio_cb;
static retro_audio_sample_batch_t audio_batch_cb;
static retro_environment_t environ_cb;
static bool noautostart = false;

#include "retro_strings.h"
#include "retro_files.h"
#include "retro_disk_control.h"
static dc_storage* dc;
enum {
	RUNSTATE_FIRST_START = 0,
	RUNSTATE_LOADED_CONTENT,
	RUNSTATE_RUNNING,
};
static int runstate = RUNSTATE_FIRST_START; /* used to detect whether we are just starting the core from scratch */
/* runstate = RUNSTATE_FIRST_START: first time retro_run() is called after loading and starting core */
/* runstate = RUNSTATE_LOADED_CONTENT: load content was selected while core is running, so do an autostart_reset() */
/* runstate = RUNSTATE_RUNNING: core is running normally */

/* Display disk name and label instead of "Changing disk in tray"- maybe make it configurable */
bool display_disk_name = true;
/* See which looks best in most cases and tweak (or make configurable) */
int disk_label_mode = DISK_LABEL_MODE_ASCII_OR_CAMELCASE;

static char* x_strdup(const char* str)
{
    return str ? strdup(str) : NULL;
}

unsigned retro_get_borders(void)
{
   return RETROBORDERS;
}

unsigned retro_toggle_theme(void)
{
   opt_theme = (opt_theme % 2) ? opt_theme-1 : opt_theme+1;
   return opt_theme;
}

void retro_set_input_state(retro_input_state_t cb)
{
   input_state_cb = cb;
}

void retro_set_input_poll(retro_input_poll_t cb)
{
   input_poll_cb = cb;
}

static char CMDFILE[512];

int loadcmdfile(const char *argv)
{
   int res=0;

   FILE *fp = fopen(argv,"r");

   CMDFILE[0] = '\0';
   if (fp != NULL)
   {
      if (fgets(CMDFILE , 512 , fp) != NULL)
         res=1;
      fclose (fp);
   }

   return res;
}

#include <ctype.h>

//Args for experimental_cmdline
static char ARGUV[64][1024];
static unsigned char ARGUC=0;

// Args for Core
static char XARGV[64][1024];
static const char* xargv_cmd[64];
int PARAMCOUNT=0;

// Display message on next retro_run
static char queued_msg[1024];
static int show_queued_msg = 0;

extern int skel_main(int argc, char *argv[]);

static void log_disk_in_tray(bool display);

static void Add_Option(const char* option)
{
   sprintf(XARGV[PARAMCOUNT++],"%s",option);
}

static void parse_cmdline(const char *argv)
{
   char *p,*p2,*start_of_word;
   int c,c2;
   static char buffer[512*4];
   enum states { DULL, IN_WORD, IN_STRING } state = DULL;

   ARGUC = 0;

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
               /* terminate word */
               ARGUV[ARGUC][c2] = '\0';
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
               /* terminate word */
               ARGUV[ARGUC][c2] = '\0';
               ARGUC++;

               state = DULL; /* back to "not in word, not in string" state */
            }
            continue; /* either still IN_WORD or we handled the end above */
      }
   }
}

static int check_joystick_control(const char* filename)
{
    int port = 0;
    if (filename != NULL)
    {
        if (strcasestr(filename, "_j1.") || strcasestr(filename, "(j1)."))
        {
            port = 1;
        }
        else if (strcasestr(filename, "_j2.") || strcasestr(filename, "(j2)."))
        {
            port = 2;
        }
    }
    return port;
}

static int get_image_unit()
{
    int unit = dc->unit;
    if (unit == 0 && dc->index < dc->count)
    {
        if (strendswith(dc->files[dc->index], "tap") || strendswith(dc->files[dc->index], "t64"))
           unit = 1;
        else
           unit = 8;
    }
    return unit;
}

// If we display the message now, it wold be immediatelly overwritten
// by "changed disk in drive", so queue it to display on next retro_run.
// The side effect is that if disk is changed from menu, the message will be displayed
// only after emulation is unpaused.
static void log_disk_in_tray(bool display)
{
    if (dc->index < dc->count)
    {
        int unit = get_image_unit();
        size_t pos = 0;
        const char* label;
        // Build message do display
        if (unit == 1)
            snprintf(queued_msg, sizeof(queued_msg), "Tape: ");
        else
            snprintf(queued_msg, sizeof(queued_msg), "Drive %d: ", unit);
        pos = strlen(queued_msg);
        snprintf(queued_msg + pos, sizeof(queued_msg) - pos, "%d/%d: %s", dc->index + 1, dc->count, path_basename(dc->files[dc->index]));
        pos += strlen(queued_msg + pos);
        label = dc->labels[dc->index];
        if (label && label[0])
        {
            snprintf(queued_msg + pos, sizeof(queued_msg) - pos, " (%s)", label);
        }
        log_cb(RETRO_LOG_INFO, "%s\n", queued_msg);
        if (display)
            show_queued_msg = 180;
    }
}

static int process_cmdline(const char* argv)
{
    int i=0;
    bool is_fliplist = false;
    int joystick_control = 0;

    noautostart = false;
    PARAMCOUNT = 0;
    dc_reset(dc);

    cur_port_locked = 0;
    free(autostartString);
    autostartString = NULL;

    // Load command line arguments from cmd file
    if (strendswith(argv, ".cmd"))
    {
        if (loadcmdfile(argv))
        {
            argv = trimwhitespace(CMDFILE);
            log_cb(RETRO_LOG_INFO, "Starting game from command line: %s\n", argv);
            RETROC64MODL = 99; // set model to unknown for custom settings - prevents overriding of command line options
        }
        else
        {
            log_cb(RETRO_LOG_ERROR, "Failed to load command line from %s\n", argv);
            argv = CMDFILE;
        }
    }
    parse_cmdline(argv);

    // Core command line is now parsed to ARGUV, ARGUC.
    // Build command file for VICE in XARGV, PARAMCOUNT.

    bool single_image = strcmp(ARGUV[0], CORE_NAME) != 0;

    // If first command line argument is "x64", it's and extended command line
    // otherwise it's just image filename
    if (single_image)
    {
        Add_Option(CORE_NAME);

        // Ignore parsed arguments, read filename directly from argv

        // Check original filename for joystick control,
        // not the first name from M3U or VFL
        joystick_control = check_joystick_control(argv);
        if (joystick_control)
        {
            cur_port = joystick_control;
            cur_port_locked = 1;
        }

#if defined(__VIC20__)
        cur_port = 1;

        if (strendswith(argv, ".20"))
            Add_Option("-cart2");
        else if (strendswith(argv, ".40"))
            Add_Option("-cart4");
        else if (strendswith(argv, ".60"))
            Add_Option("-cart6");
        else if (strendswith(argv, ".a0"))
            Add_Option("-cartA");
        else if (strendswith(argv, ".b0"))
            Add_Option("-cartB");
#endif

        if (strendswith(argv, ".m3u"))
        {
            // Parse the m3u file
            dc_parse_m3u(dc, argv);
            is_fliplist = true;
        }
        else if (strendswith(argv, ".vfl"))
        {
            // Parse the vfl file
            dc_parse_vfl(dc, argv);
            is_fliplist = true;
        }

        if (!is_fliplist)
        {
            // Add image name as autostart parameter
            Add_Option(argv);
        }
        else
        {
            // Some debugging
            log_cb(RETRO_LOG_INFO, "m3u/vfl file parsed, %d file(s) found\n", dc->count);

            if (!dc->command)
            {
                // Add first disk from list as autostart parameter
                if (dc->count != 0)
                    Add_Option(dc->files[0]);
            }
            else
            {
                // Re-parse command line from M3U #COMMAND:
                log_cb(RETRO_LOG_INFO, "Starting game from command line: %s\n", dc->command);
                RETROC64MODL = 99; // set model to unknown for custom settings - prevents overriding of command line options
                parse_cmdline(dc->command);
                // Reset parameters list for Vice
                PARAMCOUNT = 0;
                single_image = false;
            }
        }
    }

    // It might be single_image initially, but changed by M3U file #COMMAND line
    if (!single_image)
    {
        if (ARGUC == 0 || strcmp(ARGUV[0], CORE_NAME) != 0)
        {
            // Command doesn't start with core name, so add it first
            Add_Option(CORE_NAME);
        }

        bool is_flipname_param = false;
        // Scan vice arguments for special processing
        for (i = 0; i < ARGUC; i++)
        {
            const char* arg = ARGUV[i];

            // Previous arg was '-flipname'
            if (is_flipname_param)
            {
                is_flipname_param = false;
                // Parse the vfl file, don't pass to vice
                dc_parse_vfl(dc, arg);
                // Don't pass -flipname argument to vice - it has no use of it
                // and we won't have to take care of cleaning it up
                is_fliplist = true;
            }
            // Was strstr, but I don't see the point
            else if (strcmp(arg, "-j1") == 0)
            {
                cur_port = 1;
                cur_port_locked = 1;
            }
            else if (strcmp(arg, "-j2") == 0)
            {
                cur_port = 2;
                cur_port_locked = 1;
            }
            else if (strendswith(arg, ".m3u"))
            {
                // Parse the m3u file, don't pass to vice
                dc_parse_m3u(dc, arg);
                is_fliplist = true;
            }
            else if (strcmp(arg, "-flipname") == 0)
            {
                // Set flag for next arg
                is_flipname_param = true;
            }
            else if (strcmp(arg, "-noautostart") == 0)
            {
                // User ask to not automatically start image in drive
                noautostart = true;
            }
            else
            {
                Add_Option(arg);
            }
        }

        if (is_fliplist)
        {
            // Some debugging
            log_cb(RETRO_LOG_INFO, "m3u file parsed, %d file(s) found\n", dc->count);
        }
    }

    return 0;
}

// Update autostart image from vice and add disk in drive to fliplist
void update_from_vice()
{
    const char* attachedImage = NULL;

    // Get autostart string from vice
    free(autostartString);
    autostartString = x_strdup(cmdline_get_autostart_string());
    if (autostartString)
    {
        log_cb(RETRO_LOG_INFO, "Image for autostart: %s\n", autostartString);
    }
    else
    {
        log_cb(RETRO_LOG_INFO, "No image for autostart\n");
    }

    // If flip list is empty, get current tape or floppy image name and add to the list
    if (dc->count == 0)
    {
        if ((attachedImage = tape_get_file_name()) != NULL)
        {
            dc->unit = 1;
            dc_add_file(dc, attachedImage);
        }
        else
        {
            int unit;
            for (unit = 8; unit <= 11; ++unit)
            {
                if ((attachedImage = file_system_get_disk_name(unit)) != NULL)
                {
                    dc->unit = unit;
                    dc_add_file(dc, attachedImage);
                    break;
                }
            }
        }
    }

    if (dc->unit == 1)
    {
        log_cb(RETRO_LOG_INFO, "Image list is active for tape\n");
    }
    else if (dc->unit != 0)
    {
        log_cb(RETRO_LOG_INFO, "Image list is active for drive #%d\n", dc->unit);
    }
    log_cb(RETRO_LOG_INFO, "Image list has %d file(s)\n", dc->count);
    for(unsigned i = 0; i < dc->count; i++)
    {
        log_cb(RETRO_LOG_INFO, "File %d: %s\n", i+1, dc->files[i]);
    }

    // If flip list is not empty, but there is no image attached to drive, attach the first one from list.
    // This can only happen if flip list was loaded via cmd file or from m3u with #COMMAND
    if (dc->count != 0)
    {
        if (dc->unit == 1)
        {
            if ((attachedImage = tape_get_file_name()) == NULL)
            {
                attachedImage = dc->files[0];
                // Don't attach if we will autostart from it just in a moment
                if (autostartString != NULL || noautostart)
                {
                    log_cb(RETRO_LOG_INFO, "Attaching first tape %s\n", attachedImage);
                    tape_image_attach(1, attachedImage);
                }
            }
        }
        else if (dc->unit != 0)
        {
            if ((attachedImage = file_system_get_disk_name(dc->unit)) == NULL)
            {
                attachedImage = dc->files[0];
                // Don't attach if we will autostart from it just in a moment
                if (autostartString != NULL || noautostart)
                {
                    log_cb(RETRO_LOG_INFO, "Attaching first disk %s to drive #%d\n", attachedImage, dc->unit);
                    file_system_attach_disk(dc->unit, attachedImage);
                }
            }
        }
    }

    // If there an image attached, but autostart is empty, autostart from the image
    if (autostartString == NULL && attachedImage != NULL && !noautostart)
    {
        log_cb(RETRO_LOG_INFO, "Autostarting from attached or first image %s\n", attachedImage);
        autostartString = x_strdup(attachedImage);
        autostart_autodetect(autostartString, NULL, 0, AUTOSTART_MODE_RUN);
    }

    dc->index = 0;
    // If vice has image attached to drive, tell libretro that the 'tray' is closed
    if (attachedImage != NULL)
        dc->eject_state = false;
    else
        dc->eject_state = true;
}

int build_params()
{
    int i;

    if (PARAMCOUNT == 0)
    {
        // No game loaded - set command line to 'x64'
        Add_Option(CORE_NAME);
    }

    for (i = 0; i < PARAMCOUNT; i++)
    {
        xargv_cmd[i] = (char*)(XARGV[i]);
        log_cb(RETRO_LOG_INFO, "Arg%d: %s\n",i,XARGV[i]);
    }

    xargv_cmd[PARAMCOUNT] = NULL;
}

int pre_main()
{
    int argc = PARAMCOUNT;

    /* start core with empty params */
    xargv_cmd[0] = CORE_NAME;
    xargv_cmd[1] = NULL;
    argc = 1;

    if (skel_main(argc, (char**)xargv_cmd) < 0)
    {
        log_cb(RETRO_LOG_ERROR, "Core startup failed\n");
        environ_cb(RETRO_ENVIRONMENT_SHUTDOWN, NULL);
    }

    return 0;
}

static void update_variables(void);

extern int ui_init_finalize(void);

void reload_restart()
{
    /* Stop datasette */
    datasette_control(DATASETTE_CONTROL_STOP);

    /* Cleanup after previous content and reset resources */
    initcmdline_cleanup();

    /* Update resources from environment just like on fresh start of core */
    retro_ui_finalized = 0;
    update_variables();
    /* Some resources are not set until we call this */
    ui_init_finalize();

    /* And process command line */
    build_params();
    if (initcmdline_restart(PARAMCOUNT, (char**)xargv_cmd) < 0)
    {
        log_cb(RETRO_LOG_ERROR, "Restart failed\n");
        /* Nevermind, the core is already running */
    }

    /* Now read disk image and autostart file (may be the same or not) from vice */
    update_from_vice();
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

void Screen_SetFullUpdate(int scr)
{
   if (scr==0 || scr>1)
      memset(&Retro_Screen, 0, sizeof(Retro_Screen));
   if (scr>0)
      memset(&bmp, 0, sizeof(bmp));
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
   static const struct retro_controller_description p1_controllers[] = {
      { "Joystick", RETRO_DEVICE_VICE_JOYSTICK },
      { "Keyboard", RETRO_DEVICE_VICE_KEYBOARD },
      { "None", RETRO_DEVICE_NONE },
   };
   static const struct retro_controller_description p2_controllers[] = {
      { "Joystick", RETRO_DEVICE_VICE_JOYSTICK },
      { "Keyboard", RETRO_DEVICE_VICE_KEYBOARD },
      { "None", RETRO_DEVICE_NONE },
   };
   static const struct retro_controller_description p3_controllers[] = {
      { "Joystick", RETRO_DEVICE_VICE_JOYSTICK },
      { "Keyboard", RETRO_DEVICE_VICE_KEYBOARD },
      { "None", RETRO_DEVICE_NONE },
   };
   static const struct retro_controller_description p4_controllers[] = {
      { "Joystick", RETRO_DEVICE_VICE_JOYSTICK },
      { "Keyboard", RETRO_DEVICE_VICE_KEYBOARD },
      { "None", RETRO_DEVICE_NONE },
   };
   static const struct retro_controller_description p5_controllers[] = {
      { "Joystick", RETRO_DEVICE_VICE_JOYSTICK },
      { "Keyboard", RETRO_DEVICE_VICE_KEYBOARD },
      { "None", RETRO_DEVICE_NONE },
   };

   static const struct retro_controller_info ports[] = {
      { p1_controllers, 3 }, // port 1
      { p2_controllers, 3 }, // port 2
      { p3_controllers, 3 }, // port 3
      { p4_controllers, 3 }, // port 4
      { p5_controllers, 3 }, // port 5
      { NULL, 0 }
   };

   static struct retro_core_option_definition core_options[] =
   {
#if defined(__VIC20__)
      {
         "vice_vic20_model",
         "Model",
         "",
         {
            { "VIC20 PAL", NULL },
            { "VIC20 NTSC", NULL },
            { "SuperVIC NTSC (+16K)", NULL },
            { NULL, NULL },
         },
         "VIC20 PAL"
      },
      {
         "vice_vic20_memory_expansions",
         "Memory expansions",
         "",
         {
            { "none", "disabled" },
            { "3kB", NULL },
            { "8kB", NULL },
            { "16kB", NULL },
            { "24kB", NULL },
            { "all", "All" },
            { NULL, NULL },
         },
         "none"
      },
#elif defined(__PLUS4__)
      {
         "vice_plus4_model",
         "Model",
         "",
         {
            { "C16 PAL", NULL },
            { "C16 NTSC", NULL },
            { "PLUS4 PAL", NULL },
            { "PLUS4 NTSC", NULL },
            { "V364 NTSC", NULL },
            { "232 NTSC", NULL },
            { NULL, NULL }
         },
         "C16 PAL"
      },
#elif defined(__X128__)
      {
         "vice_c128_model",
         "Model",
         "",
         {
            { "C128 PAL", NULL },
            { "C128 DCR PAL", NULL },
            { "C128 NTSC", NULL },
            { "C128 DCR NTSC", NULL },
            { NULL, NULL },
         },
         "C128 PAL"
      },
      {
         "vice_c128_video_output",
         "Video Output",
         "",
         {
            { "VICII", "VIC-II (40 cols)" },
            { "VDC", "VCD (80 cols)" },
            { NULL, NULL },
         },
         "VICII"
      },
#elif defined(__PET__)
      {
         "vice_pet_model",
         "Model",
         "",
         {
            { "2001", NULL },
            { "3008", NULL },
            { "3016", NULL },
            { "3032", NULL },
            { "3032B", NULL },
            { "4016", NULL },
            { "4032", NULL },
            { "4032B", NULL },
            { "8032", NULL },
            { "8096", NULL },
            { "8096", NULL },
            { "8296", NULL },
            { "SUPERPET", NULL },
            { NULL, NULL },
         },
         "2001"
      },
#elif defined(__CBM2__)
      {
         "vice_cbm2_model",
         "Model",
         "",
         {
            { "510 PAL", NULL },
            { "610 PAL", NULL },
            { "610 NTSC", NULL },
            { "620 PAL", NULL },
            { "620 NTSC", NULL },
            { "620PLUS PAL", NULL },
            { "710 NTSC", NULL },
            { "720 NTSC", NULL },
            { "710 NTSC", NULL },
            { "720PLUS NTSC", NULL },
            { NULL, NULL },
         },
         "510 PAL"
      },
#elif defined(__XSCPU64__)
 
#else
      {
         "vice_c64_model",
         "Model",
         "",
         {
            { "C64 PAL", NULL },
            { "C64C PAL", NULL },
            //{ "C64 OLD PAL", NULL },
            { "C64 NTSC", NULL },
            { "C64C NTSC", NULL },
            //{ "C64 OLD NTSC", NULL },
            //{ "C64 PAL N", NULL },
            { "C64SX PAL", NULL },
            { "C64SX NTSC", NULL },
            { "C64 JAP", NULL },
            { "C64 GS", NULL },
            { "PET64 PAL", NULL },
            { "PET64 NTSC", NULL },
            { NULL, NULL },
         },
         "C64 PAL"
      },
#endif
      {
         "vice_reset",
         "Reset Type",
         "Soft keeps some code in memory, hard erases all memory.",
         {
            { "Autostart", NULL },
            { "Soft", NULL },
            { "Hard", NULL },
            { NULL, NULL },
         },
         "Autostart"
      },
      {
         "vice_autostart_warp",
         "Autostart Warp",
         "Automatically turns on warp mode between load and run for faster startup.",
         {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
         },
         "disabled"
      },
      {
         "vice_drive_true_emulation",
         "True Drive Emulation",
         "Loads much slower, but some games need it.",
         {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
         },
         "disabled"
      },
      {
         "vice_drive_sound_emulation",
         "Drive Sound Emulation",
         "Emulates the iconic floppy drive sounds.\nTrue Drive Emulation & D64 disk image required.",
         {
            { "disabled", NULL },
            { "10\%", "10\% volume" },
            { "20\%", "20\% volume" },
            { "30\%", "30\% volume" },
            { "40\%", "40\% volume" },
            { "50\%", "50\% volume" },
            { "60\%", "60\% volume" },
            { "70\%", "70\% volume" },
            { "80\%", "80\% volume" },
            { "90\%", "90\% volume" },
            { "100\%", "100\% volume" },
            { NULL, NULL },
         },
         "disabled"
      },
#if defined(__X64__) || defined(__X64SC__) || defined(__X128__) || defined(__VIC20__)
      {
         "vice_audio_leak_emulation",
#if defined(__X64__) || defined(__X64SC__) || defined(__X128__)
         "VIC-II Audio Leak Emulation",
#elif defined(__VIC20__)
         "VIC Audio Leak Emulation",
#endif
         "",
         {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
         },
         "disabled"
      },
#endif
#if !defined(__PET__) && !defined(__PLUS4__) && !defined(__VIC20__)
      {
         "vice_sid_model",
         "SID Model",
         "ReSID is accurate but slower. 6581 was used in original C64, 8580 in C64C.",
         {
            { "6581F", "6581 FastSID" },
            { "6581R", "6581 ReSID" },
            { "8580F", "8580 FastSID" },
            { "8580R", "8580 ReSID" },
            { "8580RD", "8580 ReSID + Digi Boost" },
            { NULL, NULL },
         },
         "6581R"
      },
      {
         "vice_resid_sampling",
         "ReSID Sampling",
         "Resampling for best quality. 'Fast' improves performance dramatically on PS Vita.",
         {
            { "Fast", NULL },
            { "Interpolation", NULL },
            { "Resampling", NULL },
            { "Fast resampling", NULL },
            { NULL, NULL },
         },
         "Resampling"
      },
#endif
#if !defined(__PET__) && !defined(__CBM2__)
      {
         "vice_border",
         "Display Borders",
         "",
         {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
         },
         "enabled"
      },
#endif
#if defined(__VIC20__)
      {
         "vice_vic20_external_palette",
         "Color Palette",
         "Colodore is recommended for the most accurate colors.",
         {
            { "Default", NULL },
            { "Colodore VIC", NULL },
            { "Mike NTSC", NULL },
            { "Mike PAL", NULL },
            { "Vice", NULL },
            { NULL, NULL },
         },
         "Default"
      },
#elif defined(__PLUS4__) 
      {
         "vice_plus4_external_palette",
         "Color Palette",
         "Colodore is recommended for the most accurate colors.",
         {
            { "Default", NULL },
            { "Colodore TED", NULL },
            { "Yape PAL", NULL },
            { "Yape NTSC", NULL },
            { NULL, NULL },
         },
         "Default"
      },
#elif defined(__PET__)
      {
         "vice_pet_external_palette",
         "Color Palette",
         "",
         {
            { "Default", NULL },
            { "Amber", NULL },
            { "Green", NULL },
            { "White", NULL },
            { NULL, NULL },
         },
         "Default"
      },
#elif defined(__CBM2__)
      {
         "vice_cbm2_external_palette",
         "Color Palette",
         "",
         {
            { "Default", NULL },
            { "Amber", NULL },
            { "Green", NULL },
            { "White", NULL },
            { NULL, NULL },
         },
         "Default"
      },
#else
      {
         "vice_external_palette",
         "Color Palette",
         "Colodore is recommended for most accurate colors.",
         {
            { "Default", NULL },
            { "Colodore", NULL },
            { "Community Colors", NULL },
            { "Pepto PAL", NULL },
            { "Pepto PAL old", NULL },
            { "Pepto NTSC", NULL },
            { "Pepto NTSC Sony", NULL },
            { "C64HQ", NULL },
            { "C64S", NULL },
            { "CCS64", NULL },
            //{ "Deekay", NULL },
            { "Frodo", NULL },
            { "Godot", NULL },
            { "PC64", NULL },
            //{ "Ptoing", NULL },
            { "RGB", NULL },
            { "Vice", NULL },
            { NULL, NULL },
         },
         "Default"
      },
#endif
      {
         "vice_theme",
         "Virtual Keyboard Theme",
         "By default, the keyboard comes up with L button or F11 key.",
         {
            { "C64", NULL },
            { "C64 transparent", NULL },
            { "C64C", NULL },
            { "C64C transparent", NULL },
            { "Dark transparent", NULL },
            { "Light transparent", NULL },
            { NULL, NULL },
         },
         "C64"
      },
#if !defined(__PET__) && !defined(__CBM2__) && !defined(__VIC20__)
      {
         "vice_joyport",
         "RetroPad Port",
         "Most games use port 2, some use port 1.",
         {
            { "Port 2", NULL },
            { "Port 1", NULL },
            { NULL, NULL },
         },
         "Port 2"
      },
#endif
      {
         "vice_userport_joytype",
         "Userport Joystick Adapter",
         "Essential when 2 joysticks are not enough, for example IK+ Gold with 3 players.",
         {
            { "None", "disabled" },
            { "Protovision CGA", NULL },
            { "PET", NULL },
            { "Hummer", NULL },
            { "OEM", NULL },
            { "Hit", NULL }, 
            { "Kingsoft", NULL },
            { "Starbyte", NULL },
            { NULL, NULL },
         },
         "None"
      },
      {
         "vice_keyrah_keypad_mappings",
         "Keyrah Keypad Mappings",
         "Hardcoded keypad to joy mappings for Keyrah hardware.",
         {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
         },
         "disabled"
      },
      {
         "vice_physical_keyboard_pass_through",
         "Physical Keyboard Pass-through",
         "Pass all physical keyboard events to the core. Disable this to prevent cursor keys and fire key from generating key events.",
         {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
         },
         "disabled"
      },
/* Hotkeys */
      {
         "vice_mapper_vkbd",
         "Hotkey: Toggle Virtual Keyboard",
         "Press the mapped key to toggle the virtual keyboard.",
         {{ NULL, NULL }},
         "RETROK_F11"
      },
      {
         "vice_mapper_statusbar",
         "Hotkey: Toggle Statusbar",
         "Press the mapped key to toggle the statusbar.",
         {{ NULL, NULL }},
         "RETROK_F10"
      },
#if !defined(__PET__) && !defined(__CBM2__) && !defined(__VIC20__)
      {
         "vice_mapper_joyport_switch",
         "Hotkey: Swap Joyports",
         "Press the mapped key to swap joyports 1 & 2.",
         {{ NULL, NULL }},
         "RETROK_RCTRL"
      },
#endif
      {
         "vice_mapper_reset",
         "Hotkey: Reset",
         "Press the mapped key to trigger reset.",
         {{ NULL, NULL }},
         "RETROK_END"
      },
      {
         "vice_mapper_warp_mode",
         "Hotkey: Warp Mode",
         "Hold the mapped key for warp mode.",
         {{ NULL, NULL }},
         "RETROK_PAGEDOWN"
      },
/* Datasette controls */
      {
         "vice_mapper_datasette_toggle_hotkeys",
         "Hotkey: Toggle Datasette Hotkeys",
         "Press the mapped key to toggle the Datasette hotkeys.",
         {{ NULL, NULL }},
         "---"
      },
      {
         "vice_datasette_hotkeys",
         "Datasette Hotkeys",
         "Enable/disable all Datasette hotkeys.",
         {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
         },
         "disabled"
      },
      {
         "vice_mapper_datasette_start",
         "Hotkey: Datasette Start",
         "Press start on tape.",
         {{ NULL, NULL }},
         "RETROK_UP"
      },
      {
         "vice_mapper_datasette_stop",
         "Hotkey: Datasette Stop",
         "Press stop on tape.",
         {{ NULL, NULL }},
         "RETROK_DOWN"
      },
      {
         "vice_mapper_datasette_rewind",
         "Hotkey: Datasette Rewind",
         "Press rewind on tape.",
         {{ NULL, NULL }},
         "RETROK_LEFT"
      },
      {
         "vice_mapper_datasette_forward",
         "Hotkey: Datasette Fast Forward",
         "Press fast forward on tape.",
         {{ NULL, NULL }},
         "RETROK_RIGHT"
      },
      {
         "vice_mapper_datasette_reset",
         "Hotkey: Datasette Reset",
         "Press reset on tape.",
         {{ NULL, NULL }},
         "---"
      },
/* Button mappings */
      {
         "vice_mapper_select",
         "RetroPad Select",
         "",
         {{ NULL, NULL }},
         "---"
      },
      {
         "vice_mapper_start",
         "RetroPad Start",
         "",
         {{ NULL, NULL }},
         "---"
      },
      {
         "vice_mapper_a",
         "RetroPad A",
         "",
         {{ NULL, NULL }},
         "---"
      },
      {
         "vice_mapper_y",
         "RetroPad Y",
         "",
         {{ NULL, NULL }},
         "---"
      },
      {
         "vice_mapper_x",
         "RetroPad X",
         "",
         {{ NULL, NULL }},
         "---"
      },
      {
         "vice_mapper_l",
         "RetroPad L",
         "",
         {{ NULL, NULL }},
         "RETROK_F11"
      },
      {
         "vice_mapper_r",
         "RetroPad R",
         "",
         {{ NULL, NULL }},
         "RETROK_F10"
      },
      {
         "vice_mapper_l2",
         "RetroPad L2",
         "",
         {{ NULL, NULL }},
         "RETROK_ESCAPE"
      },
      {
         "vice_mapper_r2",
         "RetroPad R2",
         "",
         {{ NULL, NULL }},
         "RETROK_RETURN"
      },
      {
         "vice_mapper_l3",
         "RetroPad L3",
         "",
         {{ NULL, NULL }},
         "RETROK_h"
      },
      {
         "vice_mapper_r3",
         "RetroPad R3",
         "",
         {{ NULL, NULL }},
         "RETROK_t"
      },
/* Left Stick */
      {
         "vice_mapper_lu",
         "RetroPad L-Up",
         "Mapping for left analog stick up.",
         {{ NULL, NULL }},
         "---"
      },
      {
         "vice_mapper_ld",
         "RetroPad L-Down",
         "Mapping for left analog stick down.",
         {{ NULL, NULL }},
         "---"
      },
      {
         "vice_mapper_ll",
         "RetroPad L-Left",
         "Mapping for left analog stick left.",
         {{ NULL, NULL }},
         "---"
      },
      {
         "vice_mapper_lr",
         "RetroPad L-Right",
         "Mapping for left analog stick right.",
         {{ NULL, NULL }},
         "---"
      },
/* Right Stick */
      {
         "vice_mapper_ru",
         "RetroPad R-Up",
         "Mapping for right analog stick up.",
         {{ NULL, NULL }},
         "---"
      },
      {
         "vice_mapper_rd",
         "RetroPad R-Down",
         "Mapping for right analog stick down.",
         {{ NULL, NULL }},
         "---"
      },
      {
         "vice_mapper_rl",
         "RetroPad R-Left",
         "Mapping for right analog stick left.",
         {{ NULL, NULL }},
         "---"
      },
      {
         "vice_mapper_rr",
         "RetroPad R-Right",
         "Mapping for right analog stick right.",
         {{ NULL, NULL }},
         "---"
      },
      {
         "vice_turbo_fire_button",
         "RetroPad Turbo Fire",
         "Replaces the mapped button with a turbo fire button.",
         {
            { "disabled", NULL },
            { "A", "RetroPad A" },
            { "Y", "RetroPad Y" },
            { "X", "RetroPad X" },
            { "L", "RetroPad L" },
            { "R", "RetroPad R" },
            { "L2", "RetroPad L2" },
            { "R2", "RetroPad R2" },
            { NULL, NULL },
         },
         "disabled"
      },
      {
         "vice_turbo_pulse",
         "RetroPad Turbo Pulse",
         "Frames in a button cycle. 2 equals button press on a frame and release on the next frame.",
         {
            { "2", NULL },
            { "4", NULL },
            { "6", NULL },
            { "8", NULL },
            { "10", NULL },
            { "12", NULL },
            { NULL, NULL },
         },
         "4"
      },

      { NULL, NULL, NULL, {{0}}, NULL },
   };

   /* fill in the values for all the mappers */
   int i = 0;
   int j = 0;
   while (core_options[i].key) 
   {
      if (strstr(core_options[i].key, "vice_mapper_"))
      {
         j = 0;
         while (keyDesc[j] && j < RETRO_NUM_CORE_OPTION_VALUES_MAX - 1)
         {
            core_options[i].values[j].value = keyDesc[j];
            core_options[i].values[j].label = NULL;
            ++j;
         };
         core_options[i].values[j].value = NULL;
         core_options[i].values[j].label = NULL;
      };
      ++i;
   }

   static bool allowNoGameMode;

   environ_cb = cb;

   cb(RETRO_ENVIRONMENT_SET_CONTROLLER_INFO, (void*)ports);

   unsigned version = 0;
   if (cb(RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION, &version) && (version == 1))
      cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS, core_options);
   else
   {
      /* Fallback for older API */
      static struct retro_variable variables[64] = { 0 };
      i = 0;
      while (core_options[i].key) 
      {
         buf[i][0] = 0;
         variables[i].key = core_options[i].key;
         strcpy(buf[i], core_options[i].desc);
         strcat(buf[i], "; ");
         strcat(buf[i], core_options[i].default_value);
         j = 0;
         while (core_options[i].values[j].value && j < RETRO_NUM_CORE_OPTION_VALUES_MAX)
         {
            strcat(buf[i], "|");
            strcat(buf[i], core_options[i].values[j].value);
            ++j;
         };
         variables[i].value = buf[i];
         ++i;
      };
      variables[i].key = NULL;
      variables[i].value = NULL;      
      cb( RETRO_ENVIRONMENT_SET_VARIABLES, variables);
   }

   allowNoGameMode = true;
   environ_cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &allowNoGameMode);
}

int log_resources_set_int(const char *name, int value)
{
    log_cb(RETRO_LOG_INFO, "Resource %s = %d\n", name, value);
    resources_set_int(name, value);
}

int log_resources_set_string(const char *name, const char* value)
{
    log_cb(RETRO_LOG_INFO, "Resource %s = \"%s\"\n", name, value);
    resources_set_string(name, value);
}

static void update_variables(void)
{
   struct retro_variable var;

   log_cb(RETRO_LOG_INFO, "Updating variables, ui finalized = %d\n", retro_ui_finalized);

   var.key = "vice_autostart_warp";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (retro_ui_finalized)
      {
         if (strcmp(var.value, "enabled") == 0)
            log_resources_set_int("AutostartWarp", 1);
         if (strcmp(var.value, "disabled") == 0)
            log_resources_set_int("AutostartWarp", 0);
      }
      else
      {
         if (strcmp(var.value, "enabled") == 0) RETROAUTOSTARTWARP=1;
         if (strcmp(var.value, "disabled") == 0) RETROAUTOSTARTWARP=0;
      }
   }

   var.key = "vice_drive_true_emulation";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (retro_ui_finalized)
      {
         if (strcmp(var.value, "enabled") == 0)
         {
            if (RETROTDE==0)
            {
               RETROTDE=1;
               log_resources_set_int("DriveTrueEmulation", 1);
               log_resources_set_int("VirtualDevices", 0);
            }
         }
         else if (strcmp(var.value, "disabled") == 0)
         {
            if (RETROTDE==1)
            {
               RETROTDE=0;
               log_resources_set_int("DriveTrueEmulation", 0);
               log_resources_set_int("VirtualDevices", 1);
            }
         }
      }
      else
      {
         if (strcmp(var.value, "enabled") == 0) RETROTDE=1;
         if (strcmp(var.value, "disabled") == 0) RETROTDE=0;
      }
   }

   var.key = "vice_drive_sound_emulation";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int val = atoi(var.value);
      val = val * 20;

      if (retro_ui_finalized)
      {
         if (strcmp(var.value, "disabled") == 0)
         {
            log_resources_set_int("DriveSoundEmulation", 0);
         }
         else
         {
            log_resources_set_int("DriveSoundEmulation", 1);
            log_resources_set_int("DriveSoundEmulationVolume", val);
         }
      }
      else
      {
         if (strcmp(var.value, "disabled") == 0) RETRODSE=0;
         else RETRODSE=val;
      }
   }

#if defined(__X64__) || defined(__X64SC__) || defined(__X128__) || defined(__VIC20__)
   var.key = "vice_audio_leak_emulation";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int audioleak=0;

      if (strcmp(var.value, "disabled") == 0) audioleak=0;
      else if (strcmp(var.value, "enabled") == 0) audioleak=1;

      if (retro_ui_finalized && RETROAUDIOLEAK != audioleak)
#if defined(__X64__) || defined(__X64SC__) || defined(__X128__)
         log_resources_set_int("VICIIAudioLeak", audioleak);
#elif defined(__VIC20__)
         log_resources_set_int("VICAudioLeak", audioleak);
#endif
      RETROAUDIOLEAK=audioleak;
   }
#endif

   var.key = "vice_sid_model";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int eng=0,modl=0,sidmdl=0;

      if (strcmp(var.value, "6581F") == 0) { eng=0; modl=0; }
      else if (strcmp(var.value, "8580F") == 0) { eng=0; modl=1; }
      else if (strcmp(var.value, "6581R") == 0) { eng=1; modl=0; }
      else if (strcmp(var.value, "8580R") == 0) { eng=1; modl=1; }
      else if (strcmp(var.value, "8580RD") == 0) { eng=1; modl=2; }

      sidmdl=((eng << 8) | modl);

      if (retro_ui_finalized)
         if (RETROSIDMODL != sidmdl)
            sid_set_engine_model(eng, modl);

      RETROSIDMODL=sidmdl;
   }

   var.key = "vice_resid_sampling";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int resid=0;
      
      if (strcmp(var.value, "Fast") == 0) { resid=0; }
      else if (strcmp(var.value, "Interpolation") == 0) { resid=1; }
      else if (strcmp(var.value, "Resampling") == 0) { resid=2; }
      else if (strcmp(var.value, "Fast resampling") == 0) { resid=3; }

      if (retro_ui_finalized)
         if (RETRORESIDSAMPLING != resid)
            log_resources_set_int("SidResidSampling", resid);

      RETRORESIDSAMPLING=resid;
   }

#if defined(__VIC20__)
   var.key = "vice_vic20_model";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int modl=0;

      if (strcmp(var.value, "VIC20 PAL") == 0) modl=VIC20MODEL_VIC20_PAL;
      else if (strcmp(var.value, "VIC20 NTSC") == 0) modl=VIC20MODEL_VIC20_NTSC;
      else if (strcmp(var.value, "SuperVIC (+16K)") == 0) modl=VIC20MODEL_VIC21;

      RETROC64MODL=modl;
      if (retro_ui_finalized)
         vic20model_set(modl);
   }

   var.key = "vice_vic20_memory_expansions";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int vic20mem=0;

      if (strcmp(var.value, "none") == 0) vic20mem=0;
      else if (strcmp(var.value, "3kB") == 0) vic20mem=1;
      else if (strcmp(var.value, "8kB") == 0) vic20mem=2;
      else if (strcmp(var.value, "16kB") == 0) vic20mem=3;
      else if (strcmp(var.value, "24kB") == 0) vic20mem=4;
      else if (strcmp(var.value, "all") == 0) vic20mem=5;

      if (retro_ui_finalized)
         switch (vic20mem)
         {
            case 0:
               log_resources_set_int("RAMBlock0", 0);
               log_resources_set_int("RAMBlock1", 0);
               log_resources_set_int("RAMBlock2", 0);
               log_resources_set_int("RAMBlock3", 0);
               log_resources_set_int("RAMBlock5", 0);
               break;

            case 1:
               log_resources_set_int("RAMBlock0", 1);
               log_resources_set_int("RAMBlock1", 0);
               log_resources_set_int("RAMBlock2", 0);
               log_resources_set_int("RAMBlock3", 0);
               log_resources_set_int("RAMBlock5", 0);
               break;

            case 2:
               log_resources_set_int("RAMBlock0", 0);
               log_resources_set_int("RAMBlock1", 1);
               log_resources_set_int("RAMBlock2", 0);
               log_resources_set_int("RAMBlock3", 0);
               log_resources_set_int("RAMBlock5", 0);
               break;

            case 3:
               log_resources_set_int("RAMBlock0", 0);
               log_resources_set_int("RAMBlock1", 1);
               log_resources_set_int("RAMBlock2", 1);
               log_resources_set_int("RAMBlock3", 0);
               log_resources_set_int("RAMBlock5", 0);
               break;

            case 4:
               log_resources_set_int("RAMBlock0", 0);
               log_resources_set_int("RAMBlock1", 1);
               log_resources_set_int("RAMBlock2", 1);
               log_resources_set_int("RAMBlock3", 1);
               log_resources_set_int("RAMBlock5", 0);
               break;

            case 5:
               log_resources_set_int("RAMBlock0", 1);
               log_resources_set_int("RAMBlock1", 1);
               log_resources_set_int("RAMBlock2", 1);
               log_resources_set_int("RAMBlock3", 1);
               log_resources_set_int("RAMBlock5", 1);
               break;
        }

        if (retro_ui_finalized && RETROVIC20MEM != vic20mem)
            machine_trigger_reset(MACHINE_RESET_MODE_HARD);
        RETROVIC20MEM=vic20mem;
   }
#elif defined(__PLUS4__)
   var.key = "vice_plus4_model";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int modl=0;

      if (strcmp(var.value, "C16 PAL") == 0) modl=PLUS4MODEL_C16_PAL;
      else if (strcmp(var.value, "C16 NTSC") == 0) modl=PLUS4MODEL_C16_NTSC;
      else if (strcmp(var.value, "PLUS4 PAL") == 0) modl=PLUS4MODEL_PLUS4_PAL;
      else if (strcmp(var.value, "PLUS4 NTSC") == 0) modl=PLUS4MODEL_PLUS4_NTSC;
      else if (strcmp(var.value, "V364 NTSC") == 0) modl=PLUS4MODEL_V364_NTSC;
      else if (strcmp(var.value, "232 NTSC") == 0) modl=PLUS4MODEL_232_NTSC;

      RETROC64MODL=modl;
      if (retro_ui_finalized)
        plus4model_set(modl);
   }
#elif defined(__X128__)
   var.key = "vice_c128_model";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int modl=0;

      if (strcmp(var.value, "C128 PAL") == 0) modl=C128MODEL_C128_PAL;
      else if (strcmp(var.value, "C128 DCR PAL") == 0) modl=C128MODEL_C128DCR_PAL;
      else if (strcmp(var.value, "C128 NTSC") == 0) modl=C128MODEL_C128_NTSC;
      else if (strcmp(var.value, "C128 DCR NTSC") == 0) modl=C128MODEL_C128DCR_NTSC;

      RETROC64MODL=modl;
      if (retro_ui_finalized)
         c128model_set(modl);
   }

   var.key = "vice_c128_video_output";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int c128columnkey=1;

      if (strcmp(var.value, "VICII") == 0) c128columnkey=1;
      else if (strcmp(var.value, "VDC") == 0) c128columnkey=0;

      if (retro_ui_finalized && RETROC128COLUMNKEY != c128columnkey)
      {
         log_resources_set_int("C128ColumnKey", c128columnkey);
         machine_trigger_reset(MACHINE_RESET_MODE_SOFT);
      }
      RETROC128COLUMNKEY=c128columnkey;
   }
#elif defined(__PET__)
   var.key = "vice_pet_model";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int modl=0;

      if (strcmp(var.value, "2001") == 0) modl=PETMODEL_2001;
      else if (strcmp(var.value, "3008") == 0) modl=PETMODEL_3008;
      else if (strcmp(var.value, "3016") == 0) modl=PETMODEL_3016;
      else if (strcmp(var.value, "3032") == 0) modl=PETMODEL_3032;
      else if (strcmp(var.value, "3032B") == 0) modl=PETMODEL_3032B;
      else if (strcmp(var.value, "4016") == 0) modl=PETMODEL_4016;
      else if (strcmp(var.value, "4032") == 0) modl=PETMODEL_4032;
      else if (strcmp(var.value, "4032B") == 0) modl=PETMODEL_4032B;
      else if (strcmp(var.value, "8032") == 0) modl=PETMODEL_8032;
      else if (strcmp(var.value, "8096") == 0) modl=PETMODEL_8096;
      else if (strcmp(var.value, "8296") == 0) modl=PETMODEL_8296;
      else if (strcmp(var.value, "SUPERPET") == 0) modl=PETMODEL_SUPERPET;
      
      RETROC64MODL=modl;
      if (retro_ui_finalized)
        petmodel_set(modl);
   }
#elif defined(__CBM2__)
   var.key = "vice_cbm2_model";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int modl=0;

      if (strcmp(var.value, "510 PAL") == 0) modl=CBM2MODEL_510_PAL;
      else if (strcmp(var.value, "510 NTSC") == 0) modl=CBM2MODEL_510_NTSC;
      else if (strcmp(var.value, "610 PAL") == 0) modl=CBM2MODEL_610_PAL;
      else if (strcmp(var.value, "610 NTSC") == 0) modl=CBM2MODEL_610_NTSC;
      else if (strcmp(var.value, "620 PAL") == 0) modl=CBM2MODEL_620_PAL;
      else if (strcmp(var.value, "620 NTSC") == 0) modl=CBM2MODEL_620_NTSC;
      else if (strcmp(var.value, "620PLUS PAL") == 0) modl=CBM2MODEL_620PLUS_PAL;
      else if (strcmp(var.value, "620PLUS NTSC") == 0) modl=CBM2MODEL_620PLUS_NTSC;
      else if (strcmp(var.value, "710 NTSC") == 0) modl=CBM2MODEL_710_NTSC;
      else if (strcmp(var.value, "720 NTSC") == 0) modl=CBM2MODEL_720_NTSC;
      else if (strcmp(var.value, "720PLUS NTSC") == 0) modl=CBM2MODEL_720PLUS_NTSC;

      RETROC64MODL=modl;
      if (retro_ui_finalized)
        cbm2model_set(modl);
   }
#else
   var.key = "vice_c64_model";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int modl=0;

      if (strcmp(var.value, "C64 PAL") == 0) modl=C64MODEL_C64_PAL;
      else if (strcmp(var.value, "C64C PAL") == 0) modl=C64MODEL_C64C_PAL;
      //else if (strcmp(var.value, "C64 OLD PAL") == 0) modl=C64MODEL_C64_OLD_PAL;
      else if (strcmp(var.value, "C64 NTSC") == 0) modl=C64MODEL_C64_NTSC;
      else if (strcmp(var.value, "C64C NTSC") == 0) modl=C64MODEL_C64C_NTSC;
      //else if (strcmp(var.value, "C64 OLD NTSC") == 0)modl=C64MODEL_C64_OLD_NTSC;
      //else if (strcmp(var.value, "C64 PAL N") == 0)modl=C64MODEL_C64_PAL_N;
      else if (strcmp(var.value, "C64SX PAL") == 0) modl=C64MODEL_C64SX_PAL;
      else if (strcmp(var.value, "C64SX NTSC") == 0) modl=C64MODEL_C64SX_NTSC;
      else if (strcmp(var.value, "C64 JAP") == 0) modl=C64MODEL_C64_JAP;
      else if (strcmp(var.value, "C64 GS") == 0) modl=C64MODEL_C64_GS;
      else if (strcmp(var.value, "PET64 PAL") == 0) modl=C64MODEL_PET64_PAL;
      else if (strcmp(var.value, "PET64 NTSC") == 0) modl=C64MODEL_PET64_NTSC;

      RETROC64MODL=modl;
      if (retro_ui_finalized)
        c64model_set(modl);
   }
#endif

#if !defined(__PET__) && !defined(__CBM2__)
   var.key = "vice_border";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int border=0; /* 0 : normal, 1: full, 2: debug, 3: none */
      if (strcmp(var.value, "enabled") == 0) border=0;
      else if (strcmp(var.value, "disabled") == 0) border=3;

      RETROBORDERS=border;
      if (retro_ui_finalized)
#if defined(__VIC20__)
        log_resources_set_int("VICBorderMode", border);
#elif defined(__PLUS4__)
        log_resources_set_int("TEDBorderMode", border);
#else 
        log_resources_set_int("VICIIBorderMode", border);
#endif
   }
#endif

#if defined(__VIC20__)
   var.key = "vice_vic20_external_palette";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      char extpal[20] = "";
      if (strcmp(var.value, "Default") == 0) sprintf(extpal, "%s", "");
      else if (strcmp(var.value, "Mike NTSC") == 0) sprintf(extpal, "%s", "mike-ntsc");
      else if (strcmp(var.value, "Mike PAL") == 0) sprintf(extpal, "%s", "mike-pal");
      else if (strcmp(var.value, "Colodore VIC") == 0) sprintf(extpal, "%s", "colodore_vic");
      else if (strcmp(var.value, "Vice") == 0) sprintf(extpal, "%s", "vice");

      if (retro_ui_finalized)
      {
         if (!*extpal)
         {
            log_resources_set_int("VICExternalPalette", 0);
         }
         else
         {
            log_resources_set_int("VICExternalPalette", 1);
            log_resources_set_string("VICPaletteFile", extpal);
         }
      }
      else
      {
         if (!*extpal)
         {
            RETROEXTPAL=-1;
         }
         else
         {
            RETROEXTPAL=1;
            sprintf(RETROEXTPALNAME, "%s", extpal);
         }
      }
   }
#elif defined(__PLUS4__)
   var.key = "vice_plus4_external_palette";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      char extpal[20] = "";
      if (strcmp(var.value, "Default") == 0) sprintf(extpal, "%s", "");
      else if (strcmp(var.value, "Yape PAL") == 0) sprintf(extpal, "%s", "yape-pal");
      else if (strcmp(var.value, "Yape NTSC") == 0) sprintf(extpal, "%s", "yape-ntsc");
      else if (strcmp(var.value, "Colodore TED") == 0) sprintf(extpal, "%s", "colodore_ted");

      if (retro_ui_finalized)
      {
         if (!*extpal)
         {
            log_resources_set_int("TEDExternalPalette", 0);
         }
         else
         {
            log_resources_set_int("TEDExternalPalette", 1);
            log_resources_set_string("TEDPaletteFile", extpal);
         }
      }
      else
      {
         if (!*extpal)
         {
            RETROEXTPAL=-1;
         }
         else
         {
            RETROEXTPAL=1;
            sprintf(RETROEXTPALNAME, "%s", extpal);
         }
      }
   }
#elif defined(__PET__)
   var.key = "vice_pet_external_palette";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      char extpal[20] = "";
      if (strcmp(var.value, "Default") == 0) sprintf(extpal, "%s", "");
      else if (strcmp(var.value, "Amber") == 0) sprintf(extpal, "%s", "amber");
      else if (strcmp(var.value, "Green") == 0) sprintf(extpal, "%s", "green");
      else if (strcmp(var.value, "White") == 0) sprintf(extpal, "%s", "white");

      if (retro_ui_finalized)
      {
         if (!*extpal)
         {
            log_resources_set_int("CrtcExternalPalette", 0);
         }
         else
         {
            log_resources_set_int("CrtcExternalPalette", 1);
            log_resources_set_string("CrtcPaletteFile", extpal);
         }
      }
      else
      {
         if (!*extpal)
         {
            RETROEXTPAL=-1;
         }
         else
         {
            RETROEXTPAL=1;
            sprintf(RETROEXTPALNAME, "%s", extpal);
         }
      }
   }
#elif defined(__CBM2__)
   var.key = "vice_cbm2_external_palette";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      char extpal[20] = "";
      if (strcmp(var.value, "Default") == 0) sprintf(extpal, "%s", "");
      else if (strcmp(var.value, "Amber") == 0) sprintf(extpal, "%s", "amber");
      else if (strcmp(var.value, "Green") == 0) sprintf(extpal, "%s", "green");
      else if (strcmp(var.value, "White") == 0) sprintf(extpal, "%s", "white");

      if (retro_ui_finalized)
      {
         if (!*extpal)
         {
            log_resources_set_int("CrtcExternalPalette", 0);
         }
         else
         {
            log_resources_set_int("CrtcExternalPalette", 1);
            log_resources_set_string("CrtcPaletteFile", extpal);
         }
      }
      else
      {
         if (!*extpal)
         {
            RETROEXTPAL=-1;
         }
         else
         {
            RETROEXTPAL=1;
            sprintf(RETROEXTPALNAME, "%s", extpal);
         }
      }
   }
#else
   var.key = "vice_external_palette";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      char extpal[20] = "";
      if (strcmp(var.value, "Default") == 0) sprintf(extpal, "%s", "");
      else if (strcmp(var.value, "Pepto PAL") == 0) sprintf(extpal, "%s", "pepto-pal");
      else if (strcmp(var.value, "Pepto PAL old") == 0) sprintf(extpal, "%s", "pepto-palold");
      else if (strcmp(var.value, "Pepto NTSC Sony") == 0) sprintf(extpal, "%s", "pepto-ntsc-sony");
      else if (strcmp(var.value, "Pepto NTSC") == 0) sprintf(extpal, "%s", "pepto-ntsc");
      else if (strcmp(var.value, "Colodore") == 0) sprintf(extpal, "%s", "colodore");
      else if (strcmp(var.value, "Vice") == 0) sprintf(extpal, "%s", "vice");
      else if (strcmp(var.value, "C64HQ") == 0) sprintf(extpal, "%s", "c64hq");
      else if (strcmp(var.value, "C64S") == 0) sprintf(extpal, "%s", "c64s");
      else if (strcmp(var.value, "CCS64") == 0) sprintf(extpal, "%s", "ccs64");
      else if (strcmp(var.value, "Frodo") == 0) sprintf(extpal, "%s", "frodo");
      else if (strcmp(var.value, "Godot") == 0) sprintf(extpal, "%s", "godot");
      else if (strcmp(var.value, "PC64") == 0) sprintf(extpal, "%s", "pc64");
      else if (strcmp(var.value, "RGB") == 0) sprintf(extpal, "%s", "rgb");
      else if (strcmp(var.value, "Deekay") == 0) sprintf(extpal, "%s", "deekay");
      else if (strcmp(var.value, "Ptoing") == 0) sprintf(extpal, "%s", "ptoing");
      else if (strcmp(var.value, "Community Colors") == 0) sprintf(extpal, "%s", "community-colors");

      if (retro_ui_finalized)
      {
         if (!*extpal)
         {
            log_resources_set_int("VICIIExternalPalette", 0);
         }
         else
         {
            log_resources_set_int("VICIIExternalPalette", 1);
            log_resources_set_string("VICIIPaletteFile", extpal);
         }
      }
      else
      {
         if (!*extpal)
         {
            RETROEXTPAL=-1;
         }
         else
         {
            RETROEXTPAL=1;
            sprintf(RETROEXTPALNAME, "%s", extpal);
         }
      }
   }
#endif

   var.key = "vice_userport_joytype";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {

      int joyadaptertype=-1;
      if (strcmp(var.value, "None") == 0) joyadaptertype=-1;
      else if (strcmp(var.value, "Protovision CGA") == 0) joyadaptertype=USERPORT_JOYSTICK_CGA;
      else if (strcmp(var.value, "PET") == 0) joyadaptertype=USERPORT_JOYSTICK_PET;
      else if (strcmp(var.value, "Hummer") == 0) joyadaptertype=USERPORT_JOYSTICK_HUMMER;
      else if (strcmp(var.value, "OEM") == 0) joyadaptertype=USERPORT_JOYSTICK_OEM;
      else if (strcmp(var.value, "Hit") == 0) joyadaptertype=USERPORT_JOYSTICK_HIT;
      else if (strcmp(var.value, "Kingsoft") == 0) joyadaptertype=USERPORT_JOYSTICK_KINGSOFT;
      else if (strcmp(var.value, "Starbyte") == 0) joyadaptertype=USERPORT_JOYSTICK_STARBYTE;

      if (retro_ui_finalized)
      {
         if (joyadaptertype==-1) log_resources_set_int("UserportJoy", 0);
         else
         {
            log_resources_set_int("UserportJoy", 1);
            log_resources_set_int("UserportJoyType", joyadaptertype);
         }
      }

      RETROUSERPORTJOY=joyadaptertype;
   }

   var.key = "vice_joyport";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "Port 2") == 0 && !cur_port_locked) cur_port=2;
      else if (strcmp(var.value, "Port 1") == 0 && !cur_port_locked) cur_port=1;
   }

   var.key = "vice_keyrah_keypad_mappings";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "disabled") == 0) RETROKEYRAHKEYPAD=0;
      else if (strcmp(var.value, "enabled") == 0) RETROKEYRAHKEYPAD=1;
   }

   var.key = "vice_physical_keyboard_pass_through";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "disabled") == 0) RETROKEYBOARDPASSTHROUGH=0;
      else if (strcmp(var.value, "enabled") == 0) RETROKEYBOARDPASSTHROUGH=1;
   }

   var.key = "vice_turbo_fire_button";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "disabled") == 0) turbo_fire_button=-1;
      else if (strcmp(var.value, "A") == 0) turbo_fire_button=RETRO_DEVICE_ID_JOYPAD_A;
      else if (strcmp(var.value, "Y") == 0) turbo_fire_button=RETRO_DEVICE_ID_JOYPAD_Y;
      else if (strcmp(var.value, "X") == 0) turbo_fire_button=RETRO_DEVICE_ID_JOYPAD_X;
      else if (strcmp(var.value, "L") == 0) turbo_fire_button=RETRO_DEVICE_ID_JOYPAD_L;
      else if (strcmp(var.value, "R") == 0) turbo_fire_button=RETRO_DEVICE_ID_JOYPAD_R;
      else if (strcmp(var.value, "L2") == 0) turbo_fire_button=RETRO_DEVICE_ID_JOYPAD_L2;
      else if (strcmp(var.value, "R2") == 0) turbo_fire_button=RETRO_DEVICE_ID_JOYPAD_R2;
      else if (strcmp(var.value, "L3") == 0) turbo_fire_button=RETRO_DEVICE_ID_JOYPAD_L3;
      else if (strcmp(var.value, "R3") == 0) turbo_fire_button=RETRO_DEVICE_ID_JOYPAD_R3;
   }

   var.key = "vice_turbo_pulse";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "2") == 0) turbo_pulse=2;
      else if (strcmp(var.value, "4") == 0) turbo_pulse=4;
      else if (strcmp(var.value, "6") == 0) turbo_pulse=6;
      else if (strcmp(var.value, "8") == 0) turbo_pulse=8;
      else if (strcmp(var.value, "10") == 0) turbo_pulse=10;
      else if (strcmp(var.value, "12") == 0) turbo_pulse=12;
   }

   var.key = "vice_reset";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "Autostart") == 0) RETRORESET=0;
      else if (strcmp(var.value, "Soft") == 0) RETRORESET=1;
      else if (strcmp(var.value, "Hard") == 0) RETRORESET=2;
   }

   var.key = "vice_theme";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "C64") == 0) RETROTHEME=0;
      else if (strcmp(var.value, "C64 transparent") == 0) RETROTHEME=1;
      else if (strcmp(var.value, "C64C") == 0) RETROTHEME=2;
      else if (strcmp(var.value, "C64C transparent") == 0) RETROTHEME=3;
      else if (strcmp(var.value, "Dark transparent") == 0) RETROTHEME=4;
      else if (strcmp(var.value, "Light transparent") == 0) RETROTHEME=5;

      opt_theme=RETROTHEME;
   }

   var.key = "vice_mapper_select";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[RETRO_DEVICE_ID_JOYPAD_SELECT] = keyId(var.value);
   }

   var.key = "vice_mapper_start";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[RETRO_DEVICE_ID_JOYPAD_START] = keyId(var.value);
   }

   var.key = "vice_mapper_a";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[RETRO_DEVICE_ID_JOYPAD_A] = keyId(var.value);
   }

   var.key = "vice_mapper_y";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[RETRO_DEVICE_ID_JOYPAD_Y] = keyId(var.value);
   }

   var.key = "vice_mapper_x";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[RETRO_DEVICE_ID_JOYPAD_X] = keyId(var.value);
   }

   var.key = "vice_mapper_l";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[RETRO_DEVICE_ID_JOYPAD_L] = keyId(var.value);
   }

   var.key = "vice_mapper_r";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[RETRO_DEVICE_ID_JOYPAD_R] = keyId(var.value);
   }

   var.key = "vice_mapper_l2";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[RETRO_DEVICE_ID_JOYPAD_L2] = keyId(var.value);
   }

   var.key = "vice_mapper_r2";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[RETRO_DEVICE_ID_JOYPAD_R2] = keyId(var.value);
   }

   var.key = "vice_mapper_l3";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[RETRO_DEVICE_ID_JOYPAD_L3] = keyId(var.value);
   }

   var.key = "vice_mapper_r3";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[RETRO_DEVICE_ID_JOYPAD_R3] = keyId(var.value);
   }



   var.key = "vice_mapper_lr";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[16] = keyId(var.value);
   }

   var.key = "vice_mapper_ll";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[17] = keyId(var.value);
   }

   var.key = "vice_mapper_ld";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[18] = keyId(var.value);
   }

   var.key = "vice_mapper_lu";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[19] = keyId(var.value);
   }

   var.key = "vice_mapper_rr";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[20] = keyId(var.value);
   }

   var.key = "vice_mapper_rl";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[21] = keyId(var.value);
   }

   var.key = "vice_mapper_rd";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[22] = keyId(var.value);
   }

   var.key = "vice_mapper_ru";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[23] = keyId(var.value);
   }


   var.key = "vice_mapper_vkbd";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[24] = keyId(var.value);
   }

   var.key = "vice_mapper_statusbar";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[25] = keyId(var.value);
   }

   var.key = "vice_mapper_joyport_switch";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[26] = keyId(var.value);
   }

   var.key = "vice_mapper_reset";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[27] = keyId(var.value);
   }

   var.key = "vice_mapper_warp_mode";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[28] = keyId(var.value);
   }

   var.key = "vice_datasette_hotkeys";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "enabled") == 0) datasette=1;
      else if (strcmp(var.value, "disabled") == 0) datasette=0;
   }

   var.key = "vice_mapper_datasette_toggle_hotkeys";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[29] = keyId(var.value);
   }
   
   var.key = "vice_mapper_datasette_stop";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[30] = keyId(var.value);
   }

   var.key = "vice_mapper_datasette_start";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[31] = keyId(var.value);
   }

   var.key = "vice_mapper_datasette_forward";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[32] = keyId(var.value);
   }

   var.key = "vice_mapper_datasette_rewind";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[33] = keyId(var.value);
   }

   var.key = "vice_mapper_datasette_reset";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[34] = keyId(var.value);
   }
}

void emu_reset(void)
{
   // Always stop datasette or autostart from tape will fail
   datasette_control(DATASETTE_CONTROL_STOP);

   switch (RETRORESET)
   {
      case 0:
         if (autostartString != NULL && autostartString[0] != '\0')
            autostart_autodetect(autostartString, NULL, 0, AUTOSTART_MODE_RUN);
         else
            machine_trigger_reset(MACHINE_RESET_MODE_HARD);
         break;
      case 1:
         machine_trigger_reset(MACHINE_RESET_MODE_SOFT);
         break;
      case 2:
         machine_trigger_reset(MACHINE_RESET_MODE_HARD);
         break;
   }
}

void retro_reset(void)
{
   microSecCounter = 0;

   // Always stop datasette, or autostart from tape will fail
   datasette_control(DATASETTE_CONTROL_STOP);

   /* Retro reset should always autostart */
   if (autostartString != NULL && autostartString[0] != '\0')
      autostart_autodetect(autostartString, NULL, 0, AUTOSTART_MODE_RUN);
   else
      machine_trigger_reset(MACHINE_RESET_MODE_HARD);
}

struct DiskImage {
    char* fname;
};

static bool retro_set_eject_state(bool ejected)
{
    if (dc)
    {
        int unit = get_image_unit();

        if (dc->eject_state != ejected)
        {
            if (ejected && dc->index <= dc->count)
            {
                dc->eject_state = ejected;
                if (unit == 1)
                    tape_image_detach(unit);
                else
                    file_system_detach_disk(unit);

                return true;
            }
            else if (!ejected && dc->index < dc->count && dc->files[dc->index] != NULL)
            {
                dc->eject_state = ejected;
                if (unit == 1)
                    tape_image_attach(unit, dc->files[dc->index]);
                else
                    file_system_attach_disk(unit, dc->files[dc->index]);

                return true;
            }
        }
    }

    return false;
}

/* Gets current eject state. The initial state is 'not ejected'. */
static bool retro_get_eject_state(void)
{
    if (dc)
        return dc->eject_state;

    return true;
}

/* Gets current disk index. First disk is index 0.
 * If return value is >= get_num_images(), no disk is currently inserted.
 */
static unsigned retro_get_image_index(void)
{
    if (dc)
        return dc->index;

    return 0;
}

/* Sets image index. Can only be called when disk is ejected.
 * The implementation supports setting "no disk" by using an
 * index >= get_num_images().
 */
static bool retro_set_image_index(unsigned index)
{
    // Switch disk in drive
    if (dc)
    {
        if (index <= dc->count)
        {
            dc->index = index;

            if ((index < dc->count) && (dc->files[index]))
            {
                log_disk_in_tray(display_disk_name);
            }

            return true;
        }
    }

    return false;
}

/* Gets total number of images which are available to use. */
static unsigned retro_get_num_images(void)
{
    if (dc)
        return dc->count;

    return 0;
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
      const struct retro_game_info *info)
{
    if (dc)
    {
        if (info != NULL)
        {
            dc_replace_file(dc, index, info->path);
        }
        else
        {
            dc_remove_file(dc, index);
        }
    }

    return false;	
}

/* Adds a new valid index (get_num_images()) to the internal disk list.
 * This will increment subsequent return values from get_num_images() by 1.
 * This image index cannot be used until a disk image has been set
 * with replace_image_index. */
static bool retro_add_image_index(void)
{
    if (dc)
    {
        if (dc->count <= DC_MAX_SIZE)
        {
            dc->files[dc->count] = NULL;
            dc->labels[dc->count] = NULL;
            dc->count++;
            return true;
        }
    }

    return false;
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

static void fallback_log(enum retro_log_level level, const char *fmt, ...)
{
   /* stub */
}

void retro_init(void)
{
   struct retro_log_callback log;

	// Init disk control context
   	dc = dc_create();

   if (environ_cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log))
      log_cb = log.log;
   else
      log_cb = fallback_log;

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

   if (retro_system_directory==NULL)
   {
#if defined(__ANDROID__) || defined(ANDROID)
      strcpy(RETRO_DIR, "/mnt/sdcard");
#elif defined(VITA)
      strcpy(RETRO_DIR, "ux0:/data");
#elif defined(__SWITCH__)
      strcpy(RETRO_DIR, "/");
#else
      strcpy(RETRO_DIR, ".");
#endif
   }
   else
      sprintf(RETRO_DIR, "%s", retro_system_directory);

   /* Use system directory for data files such as C64/.vpl etc. */
   snprintf(retro_system_data_directory, sizeof(retro_system_data_directory), "%s%svice", RETRO_DIR, FSDEV_DIR_SEP_STR);
   archdep_mkdir(retro_system_data_directory, 0);

#ifdef FRONTEND_SUPPORTS_RGB565
   enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_RGB565;
#else
   enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_XRGB8888;
#endif

   if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
   {
      log_cb(RETRO_LOG_ERROR, "PIXEL FORMAT is not supported.\n");
      environ_cb(RETRO_ENVIRONMENT_SHUTDOWN, NULL);
      return;
   }

#define RETRO_DESCRIPTOR_BLOCK( _user )                                            \
   { _user, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A, "A" },               \
   { _user, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B, "B / Fire" },        \
   { _user, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X, "X" },               \
   { _user, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y, "Y" },               \
   { _user, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "Select" },     \
   { _user, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "Start" },       \
   { _user, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "Right" },       \
   { _user, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT, "Left" },         \
   { _user, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP, "Up" },             \
   { _user, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN, "Down" },         \
   { _user, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R, "R" },               \
   { _user, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L, "L" },               \
   { _user, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2, "R2" },             \
   { _user, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2, "L2" },             \
   { _user, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R3, "R3" },             \
   { _user, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L3, "L3" },			   \
   { _user, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X, "Left Analog X" },			   \
   { _user, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y, "Left Analog Y" },			   \
   { _user, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X, "Right Analog X" },			   \
   { _user, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y, "Right Analog Y" }
   
   static struct retro_input_descriptor inputDescriptors[] =
   {
      RETRO_DESCRIPTOR_BLOCK( 0 ),
      RETRO_DESCRIPTOR_BLOCK( 1 ),
      RETRO_DESCRIPTOR_BLOCK( 2 ),
      RETRO_DESCRIPTOR_BLOCK( 3 ),
      RETRO_DESCRIPTOR_BLOCK( 4 ),

      { 0 },
   };

#undef RETRO_DESCRIPTOR_BLOCK

   environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, &inputDescriptors);
   environ_cb(RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE, &diskControl);

   static uint32_t quirks =  RETRO_SERIALIZATION_QUIRK_MUST_INITIALIZE | RETRO_SERIALIZATION_QUIRK_CORE_VARIABLE_SIZE;
   environ_cb(RETRO_ENVIRONMENT_SET_SERIALIZATION_QUIRKS, &quirks);

   microSecCounter = 0;
}

void retro_deinit(void)
{
   /* Clean the disk control context */
   if (dc)
      dc_free(dc);
}

unsigned retro_api_version(void)
{
   return RETRO_API_VERSION;
}

void retro_set_controller_port_device( unsigned port, unsigned device )
{
   if (port < 5)
      vice_devices[port] = device;
}

void retro_get_system_info(struct retro_system_info *info)
{
#ifndef GIT_VERSION
#define GIT_VERSION ""
#endif
   memset(info, 0, sizeof(*info));
   info->library_name     = "VICE " CORE_NAME;
   info->library_version  = "3.3" GIT_VERSION;
#if defined(__VIC20__)
   info->valid_extensions = "20|40|60|a0|b0|d64|d71|d80|d81|d82|g64|g41|x64|t64|tap|prg|p00|crt|bin|zip|gz|d6z|d7z|d8z|g6z|g4z|x6z|cmd|m3u|vfl";
#else
   info->valid_extensions = "d64|d71|d80|d81|d82|g64|g41|x64|t64|tap|prg|p00|crt|bin|zip|gz|d6z|d7z|d8z|g6z|g4z|x6z|cmd|m3u|vfl";
#endif
   info->need_fullpath    = true;
   info->block_extract    = false;
}

void update_geometry()
{
   struct retro_system_av_info system_av_info;
   system_av_info.geometry.base_width = retroW;
   system_av_info.geometry.base_height = retroH;

   // Update av_info only when PAL/NTSC change occurs
   if (retro_region != retro_get_region())
   {
      retro_region = retro_get_region();
      retro_get_system_av_info(&system_av_info);
      environ_cb(RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO, &system_av_info);
   }

   if (retro_get_borders())
     // When borders are disabled, each system has a different aspect ratio.
     // For example, C64 & C128 have 320 / 200 pixel resolution with a 15 / 16
     // pixel aspect ratio leading to a total aspect of 320 / 200 * 15 / 16 = 1.5
#if defined(__VIC20__)
     system_av_info.geometry.aspect_ratio = (float)1.6;
#elif defined(__PLUS4__)
     system_av_info.geometry.aspect_ratio = (float)1.65;
#elif defined(__PET__)
     system_av_info.geometry.aspect_ratio = (float)4.0/3.0;
#else
     system_av_info.geometry.aspect_ratio = (float)1.5;
#endif
   else
     system_av_info.geometry.aspect_ratio = (float)4.0/3.0;
   environ_cb(RETRO_ENVIRONMENT_SET_GEOMETRY, &system_av_info);
}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
   info->geometry.max_width = retrow;
   info->geometry.max_height = retroh;
   info->geometry.base_width = 384;
   info->geometry.aspect_ratio = 4.0 / 3.0;

   info->timing.sample_rate = 44100.0;

   switch(retro_get_region())
   {
      case RETRO_REGION_PAL:
         info->geometry.base_height = 272;
         info->timing.fps = C64_PAL_RFSH_PER_SEC;
         break;
      
      case RETRO_REGION_NTSC:
         info->geometry.base_height = 247;
         info->timing.fps = C64_NTSC_RFSH_PER_SEC;
         break;
   }

   // Remember region for av_info update
   retro_region = retro_get_region();
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

void retro_audio_cb(short l, short r)
{
   audio_cb(l, r);
}

void retro_audiocb(signed short int *sound_buffer, int sndbufsize)
{
   int x;
   for (x=0; x<sndbufsize; x++) audio_cb(sound_buffer[x], sound_buffer[x]);
}

void retro_blit(void)
{
   memcpy(Retro_Screen, bmp, PITCH*WINDOW_SIZE);
}

static int prev_ui_finalized = 0;

void retro_run(void)
{
   bool updated = false;

   if (lastW!=retroW || lastH!=retroH) {
      update_geometry();
#ifdef RETRO_DEBUG
      log_cb(RETRO_LOG_INFO, "Update Geometry: Old(%d,%d) New(%d,%d)\n",lastW,lastH,retroW,retroH);
#endif
      lastW=retroW;
      lastH=retroH;
      reset_mouse_pos();
   }

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated)
      update_variables();

   if (retro_ui_finalized && !prev_ui_finalized)
   {
      log_cb(RETRO_LOG_INFO, "UI finalized now\n");
      prev_ui_finalized = 1;
   }

   if (show_queued_msg != 0)
   {
      struct retro_message rmsg;
      rmsg.msg = queued_msg;
      rmsg.frames = show_queued_msg;
      environ_cb(RETRO_ENVIRONMENT_SET_MESSAGE, &rmsg);
      show_queued_msg = 0;
   }

   if (runstate == RUNSTATE_FIRST_START)
   {
      /* this is only done once after just loading the core from scratch and starting it */
#ifdef RETRO_DEBUG
      log_cb(RETRO_LOG_INFO, "First time we return from retro_run()!\n");
#endif
      retro_load_ok = true;
      app_init();
      pre_main();
      reload_restart();
      runstate = RUNSTATE_RUNNING;
      return;
   } 
   else if (runstate == RUNSTATE_LOADED_CONTENT)
   {
      /* Load content was called while core was already running, just do a reset with autostart */
      reload_restart();
      /* After retro_load_game, get_system_av_info is always called by the frontend */
      /* resetting the aspect to 4/3 etc. So we inform the frontend of the actual */
      /* current aspect ratio and screen size again here */
      update_geometry();
      runstate = RUNSTATE_RUNNING;
   } 

   /* Measure frame-time and time between frames to render as much frames as possible when warp is enabled. Does not work
      perfectly as the time needed by the framework cannot be accounted, but should not reduce amount of actually rendered
      frames too much. */
   {
      static struct retro_perf_callback pcb;
      if (!pcb.get_time_usec)
         environ_cb(RETRO_ENVIRONMENT_GET_PERF_INTERFACE, &pcb);

      static retro_time_t t_frame=0, t_end_prev=0;

      retro_time_t t_begin=pcb.get_time_usec();
      retro_time_t t_interframe=(t_end_prev ? t_begin-t_end_prev : 0);

      for (int frame_count=0;frame_count<(retro_warp_mode_enabled() ? (t_interframe+t_frame)/t_frame : 1);++frame_count)
      {
         while(cpuloop==1)
            maincpu_mainloop_retro();
         cpuloop=1;

         if (!frame_count)
         {
            retro_time_t t_end=pcb.get_time_usec();
            t_end_prev=t_end;
            t_frame=t_end-t_begin;
         }
      }
   }

   retro_blit();
   if (SHOWKEY==1) app_render();

   video_cb(Retro_Screen,retroW,retroH,retrow<<PIXEL_BYTES);

   microSecCounter += (1000000/(retro_get_region() == RETRO_REGION_NTSC ? C64_NTSC_RFSH_PER_SEC : C64_PAL_RFSH_PER_SEC));
}

bool retro_load_game(const struct retro_game_info *info)
{
   if (info)
   {
      process_cmdline(info->path);
   }

   update_variables();
   
   if (runstate == RUNSTATE_RUNNING) {
      /* load game was called while core is already running */
      /* so we update runstate and do the deferred autostart_reset in retro_run */
      /* the autostart_reset has to be deferred because a bunch of other init stuff */
      /* is done between retro_load_game() and retro_run() at a higher level */
      runstate = RUNSTATE_LOADED_CONTENT;
   }

   return true;
}

void retro_unload_game(void){
   file_system_detach_disk(8);
   tape_image_detach(1);
#ifndef __PET__
   cartridge_detach_image(-1);
#endif
   dc_reset(dc);
   free(autostartString);
   autostartString = NULL;
}

unsigned retro_get_region(void)
{
#if defined(__PET__) || defined(__XSCPU64__)
   return RETRO_REGION_PAL;
#else
   switch(RETROC64MODL) {
#if defined(__VIC20__)
      case VIC20MODEL_VIC20_NTSC:
#elif defined(__CBM2__)
      case CBM2MODEL_510_NTSC:
      case CBM2MODEL_610_NTSC:
      case CBM2MODEL_620_NTSC:
      case CBM2MODEL_620PLUS_NTSC:
      case CBM2MODEL_710_NTSC:
      case CBM2MODEL_720_NTSC:
      case CBM2MODEL_720PLUS_NTSC:
#elif defined(__PLUS4__)
      case PLUS4MODEL_C16_NTSC:
      case PLUS4MODEL_PLUS4_NTSC:
      case PLUS4MODEL_V364_NTSC:
      case PLUS4MODEL_232_NTSC:
#elif defined(__X128__)
      case C128MODEL_C128_NTSC:
      case C128MODEL_C128DCR_NTSC:
#else
      case C64MODEL_C64_NTSC:
      case C64MODEL_C64C_NTSC:
      case C64MODEL_C64_OLD_NTSC:
      case C64MODEL_C64SX_NTSC:
      case C64MODEL_PET64_NTSC:
#endif
         return RETRO_REGION_NTSC;
         break;
      default:
         return RETRO_REGION_PAL;
         break;
   }
#endif /* __PET__ */
}


bool retro_load_game_special(unsigned type, const struct retro_game_info *info, size_t num)
{
   (void)type;
   (void)info;
   (void)num;
   return false;
}

/* CPU traps ensure we are never saving snapshots or loading them in the middle of a cpu instruction.
   Without this, savestate corruption occurs.
*/

static void save_trap(uint16_t addr, void *success)
{
   /* params: stream, save_roms, save_disks, event_mode */
   if (machine_write_snapshot_to_stream(snapshot_stream, 0, 0, 0) >= 0) 
      *((int *)success) = 1;
   else
      *((int *)success) = 0;
   save_trap_happened = 1;
}

static void load_trap(uint16_t addr, void *success)
{
   /* params: stream, event_mode */
   if (machine_read_snapshot_from_stream(snapshot_stream, 0) >= 0)
      *((int *)success) = 1;
   else
      *((int *)success) = 0;
   load_trap_happened = 1;
}

size_t retro_serialize_size(void)
{
   long snapshot_size = 0;
   if (retro_ui_finalized)
   {
      snapshot_stream = snapshot_memory_write_fopen(NULL, 0);
      int success = 0;
      interrupt_maincpu_trigger_trap(save_trap, (void *)&success);
      save_trap_happened = 0;
      while (!save_trap_happened)
         maincpu_mainloop_retro();
      if (snapshot_stream != NULL)
      {
         if (success)
         {
            snapshot_fseek(snapshot_stream, 0, SEEK_END);
            snapshot_size = snapshot_ftell(snapshot_stream);
         }
         else
         {
            log_cb(RETRO_LOG_INFO, "Failed to calculate snapshot size\n");
         }
         snapshot_fclose(snapshot_stream);
         snapshot_stream = NULL;
      }
   }
   return snapshot_size;
}

bool retro_serialize(void *data_, size_t size)
{
   if (retro_ui_finalized)
   {
      snapshot_stream = snapshot_memory_write_fopen(data_, size);
      int success = 0;
      interrupt_maincpu_trigger_trap(save_trap, (void *)&success);
      save_trap_happened = 0;
      while (!save_trap_happened)
         maincpu_mainloop_retro();
      if (snapshot_stream != NULL)
      {
         snapshot_fclose(snapshot_stream);
         snapshot_stream = NULL;
      }
      if (success)
      {
         return true;
      }
      log_cb(RETRO_LOG_INFO, "Failed to serialize snapshot\n");
   }
   return false;
}

bool retro_unserialize(const void *data_, size_t size)
{
   if (retro_ui_finalized)
   {
      snapshot_stream = snapshot_memory_read_fopen(data_, size);
      int success = 0;
      interrupt_maincpu_trigger_trap(load_trap, (void *)&success);
      load_trap_happened = 0;
      while (!load_trap_happened)
         maincpu_mainloop_retro();
      if (snapshot_stream != NULL)
      {
         snapshot_fclose(snapshot_stream);
         snapshot_stream = NULL;
      }
      if (success)
      {
         return true;
      }
      log_cb(RETRO_LOG_INFO, "Failed to unserialize snapshot\n");
   }
   return false;
}

void *retro_get_memory_data(unsigned id)
{
   if (id == RETRO_MEMORY_SYSTEM_RAM)
      return mem_ram;
   return NULL;
}

size_t retro_get_memory_size(unsigned id)
{
   if (id == RETRO_MEMORY_SYSTEM_RAM)
      return g_mem_ram_size;
   return 0;
}

void retro_cheat_reset(void) {}

void retro_cheat_set(unsigned index, bool enabled, const char *code)
{
   (void)index;
   (void)enabled;
   (void)code;
}

