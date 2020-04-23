#include "libretro.h"
#include "libretro-core.h"

#include "archdep.h"
#include "c64.h"
#include "c64mem.h"
#include "mem.h"
#include "machine.h"
#include "snapshot.h"
#include "autostart.h"
#include "drive.h"
#include "tape.h"
#include "vdrive.h"
#include "diskimage.h"
#include "attach.h"
#include "interrupt.h"
#include "datasette.h"
#ifdef __PET__
#include "keyboard.h"
#else
#include "cartridge.h"
#endif
#include "initcmdline.h"
#include "vsync.h"
#include "log.h"

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

// Our virtual time counter, increased by retro_run()
long microSecCounter = 0;
int cpuloop = 1;

// VKBD 
extern int SHOWKEY;
unsigned int opt_vkbd_theme;
unsigned int opt_vkbd_alpha = 204;
unsigned int vkbd_alpha = 204;

// Core vars
extern char core_key_state[512];
extern char core_old_key_state[512];
char slash = FSDEV_DIR_SEP_CHR;
bool retro_load_ok = false;
static bool noautostart = false;
static char* autostartString = NULL;
char RETRO_DIR[RETRO_PATH_MAX];
static char* core_options_legacy_strings = NULL;

static snapshot_stream_t* snapshot_stream = NULL;
static int load_trap_happened = 0;
static int save_trap_happened = 0;

int mapper_keys[38] = { 0 };		// Bruno total number of hot key binds - default 36
unsigned int vice_devices[5];
unsigned int opt_mapping_options_display;
unsigned int opt_video_options_display;
unsigned int opt_audio_options_display;
unsigned int retro_region;
unsigned int prev_audio_sample_rate = 0;

extern void retro_poll_event();
extern int retro_ui_finalized;
static int prev_ui_finalized = 0;

extern uint8_t mem_ram[];
extern int g_mem_ram_size;

// Core geometry
int retroXS=0;
int retroYS=0;
int retroXS_offset=0;
int retroYS_offset=0;
int defaultW=WINDOW_WIDTH;
int defaultH=WINDOW_HEIGHT;
int retroW=WINDOW_WIDTH;
int retroH=WINDOW_HEIGHT;
int lastW=0;
int lastH=0;

int pix_bytes = 2;
static bool pix_bytes_initialized = false;
unsigned short int retro_bmp[RETRO_BMP_SIZE];

// Core options
extern int RETROTDE;
extern int RETRODSE;
extern int RETRORESET;
extern int RETROSIDENGINE;
extern int RETROSIDMODL;
extern int RETRORESIDSAMPLING;
extern int RETROSOUNDSAMPLERATE;
extern int RETRORESIDPASSBAND;
extern int RETRORESIDGAIN;
extern int RETRORESIDFILTERBIAS;
extern int RETRORESID8580FILTERBIAS;
extern int RETROAUDIOLEAK;
extern int RETROC64MODL;
#if defined(__X128__)
extern int RETROC128COLUMNKEY;
#endif
#if defined(__VIC20__)
extern int RETROVIC20MEM;
extern int vic20mem_forced;
#endif
extern int RETROUSERPORTJOY;
extern int RETROEXTPAL;
extern int RETROAUTOSTARTWARP;
extern int RETROBORDERS;
extern int RETROTHEME;
extern int RETROKEYRAHKEYPAD;
extern int RETROKEYBOARDPASSTHROUGH;
extern char RETROEXTPALNAME[512];
#if defined(__X64__) || defined(__X64SC__) || defined(__X128__) || defined(__XSCPU64__)
extern int RETROVICIICOLORGAMMA;
extern int RETROVICIICOLORSATURATION;
extern int RETROVICIICOLORCONTRAST;
extern int RETROVICIICOLORBRIGHTNESS;
#endif

 //   /* ### Bruno RESET */  have to have zoom_mode_id_prev = -1 equivalent resets for our new variables

unsigned int zoom_mode_id = 0;
int zoom_mode_id_prev = -1;
unsigned int opt_zoom_mode_id = 0;

unsigned int zoom_mode_count = 4;	// How many Vertical Crop Modes are available? Default 4 - used for hot-key cycling

unsigned int bcrop_horiz_mode = 0;
int bcrop_horiz_mode_prev = -1;
unsigned int bcrop_horiz_mode_count = 3;	// How many Horizontal Crop Modes are available? Default 3 - used for hot-key cycling

// unsigned int opt_bcrop_horiz_mode = 0;  // Bruno - not used


unsigned int crop_top_px = 0;
int crop_top_px_prev = -1;
unsigned int clean_top_crop = 0;

unsigned int crop_bottom_px = 0;
int crop_bottom_px_prev = -1;
unsigned int clean_bottom_crop = 0;

unsigned int zoomed_width;
unsigned int zoomed_height;
unsigned int zoomed_XS_offset;
unsigned int zoomed_YS_offset;

static unsigned int opt_aspect_ratio = 0;
static unsigned int opt_aspect_ratio_prev = 0;

unsigned int opt_read_vicerc = 0;
static unsigned int opt_read_vicerc_prev = 0;
static unsigned int opt_jiffydos_allow = 1;
unsigned int opt_jiffydos = 0;
static unsigned int opt_jiffydos_prev = 0;
static unsigned int request_reload_restart = 0;
static unsigned int sound_volume_counter = 3;
unsigned int opt_audio_leak_volume = 0;
unsigned int opt_statusbar = 0;
unsigned int opt_retropad_options = 0;
unsigned int opt_joyport_type = 0;
unsigned int opt_dpadmouse_speed = 6;
unsigned int opt_mouse_speed = 100;
unsigned int opt_analogmouse = 0;
unsigned int opt_analogmouse_deadzone = 15;
float opt_analogmouse_speed = 1.0;

extern unsigned int datasette_hotkeys;
extern unsigned int cur_port;
extern int cur_port_locked;

extern int turbo_fire_button;
extern unsigned int turbo_pulse;

#if defined(__VIC20__)
char resources_var_border[20] = "VICBorderMode";
#elif defined(__PLUS4__)
char resources_var_border[20] = "TEDBorderMode";
#else
char resources_var_border[20] = "VICIIBorderMode";
#endif

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

char retro_save_directory[RETRO_PATH_MAX] = {0};
char retro_temp_directory[RETRO_PATH_MAX] = {0};
char retro_system_directory[RETRO_PATH_MAX] = {0};
char retro_content_directory[RETRO_PATH_MAX] = {0};
char retro_system_data_directory[RETRO_PATH_MAX] = {0};

retro_input_state_t input_state_cb;
retro_input_poll_t input_poll_cb;
retro_log_printf_t log_cb;
static retro_video_refresh_t video_cb;
static retro_audio_sample_t audio_cb;
static retro_audio_sample_batch_t audio_batch_cb;
static retro_environment_t environ_cb;

static dc_storage* dc;

int runstate = RUNSTATE_FIRST_START; /* used to detect whether we are just starting the core from scratch */
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

unsigned int retro_get_borders(void)
{
   return RETROBORDERS;
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
extern void display_current_image(const char *image, bool inserted);

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
    if (dc->index < dc->count)
    {
        if (strendswith(dc->files[dc->index], "tap") || strendswith(dc->files[dc->index], "t64"))
           unit = 1;
        else
           unit = 8;
    }
    else
        unit = 8;

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
        snprintf(queued_msg + pos, sizeof(queued_msg) - pos, "(%d/%d) %s", dc->index + 1, dc->count, path_basename(dc->files[dc->index]));
        pos += strlen(queued_msg + pos);
        label = dc->labels[dc->index];
        if (label && label[0])
        {
            snprintf(queued_msg + pos, sizeof(queued_msg) - pos, " (%s)", label);
        }
        log_cb(RETRO_LOG_INFO, "%s\n", queued_msg);
        if (display)
            show_queued_msg = 150;
    }
}

static int process_cmdline(const char* argv)
{
    int i = 0;
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

        // ZIP
        if (strendswith(argv, ".zip"))
        {
            char full_path[RETRO_PATH_MAX] = {0};
            snprintf(full_path, sizeof(full_path), "%s", argv);

            char zip_basename[RETRO_PATH_MAX] = {0};
            snprintf(zip_basename, sizeof(zip_basename), "%s", path_basename(full_path));
            snprintf(zip_basename, sizeof(zip_basename), "%s", path_remove_extension(zip_basename));
            snprintf(retro_temp_directory, sizeof(retro_temp_directory), "%s%s%s", retro_save_directory, FSDEV_DIR_SEP_STR, "ZIP");
            char zip_path[RETRO_PATH_MAX] = {0};
            snprintf(zip_path, sizeof(zip_path), "%s%s%s", retro_temp_directory, FSDEV_DIR_SEP_STR, zip_basename);

            path_mkdir(zip_path);
            zip_uncompress(full_path, zip_path, NULL);

            // Default to directory mode
            int zip_mode = 0;
            snprintf(full_path, sizeof(full_path), "%s", zip_path);

            FILE *zip_m3u;
            char zip_m3u_buf[2048] = {0};
            char zip_m3u_path[RETRO_PATH_MAX] = {0};
            snprintf(zip_m3u_path, sizeof(zip_m3u_path), "%s%s%s.m3u", zip_path, FSDEV_DIR_SEP_STR, zip_basename);

            DIR *zip_dir;
            struct dirent *zip_dirp;
            zip_dir = opendir(zip_path);
            while ((zip_dirp = readdir(zip_dir)) != NULL)
            {
               if (zip_dirp->d_name[0] == '.' || strendswith(zip_dirp->d_name, ".m3u") || zip_mode > 1)
                  continue;

               // Multi file mode, generate playlist
               if (dc_get_image_type(zip_dirp->d_name) == DC_IMAGE_TYPE_FLOPPY
                || dc_get_image_type(zip_dirp->d_name) == DC_IMAGE_TYPE_TAPE
               )
               {
                  zip_mode = 1;
                  snprintf(zip_m3u_buf+strlen(zip_m3u_buf), sizeof(zip_m3u_buf), "%s\n", zip_dirp->d_name);
               }
               // Single file mode
               else if (dc_get_image_type(zip_dirp->d_name) == DC_IMAGE_TYPE_MEM)
               {
                  zip_mode = 2;
                  snprintf(full_path, sizeof(full_path), "%s%s%s", zip_path, FSDEV_DIR_SEP_STR, zip_dirp->d_name);
               }
            }
            closedir(zip_dir);

            switch (zip_mode)
            {
               case 0: // Extracted path
               case 2: // Single image
                  break;
               case 1: // Generated playlist
                  zip_m3u = fopen(zip_m3u_path, "w");
                  fprintf(zip_m3u, "%s", zip_m3u_buf);
                  fclose(zip_m3u);
                  snprintf(full_path, sizeof(full_path), "%s", zip_m3u_path);
                  break;
            }

            argv = full_path;
        }

#if defined(__X64__) || defined(__X64SC__)
        // Disable JiffyDOS with PRGs & CRTs
        if (strendswith(argv, ".prg")
         || strendswith(argv, ".crt"))
            opt_jiffydos_allow = 0;
        else
            opt_jiffydos_allow = 1;
#endif

#if defined(__VIC20__)
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

        char vic20buf1[6]   = "\0";
        char vic20buf2[6]   = "\0";
        int vic20mem        = 0;
        int vic20mems[5]    = {0, 3, 8, 16, 24};

        for (int i = 0; i < 5; i++)
        {
            vic20mem = vic20mems[i];
            snprintf(vic20buf1, 6, "%c%d%c%c", '(', vic20mem, 'k', ')');
            snprintf(vic20buf2, 6, "%c%d%c%c", FSDEV_DIR_SEP_CHR, vic20mem, 'k', FSDEV_DIR_SEP_CHR);
            if (strcasestr(argv, vic20buf1))
            {
                vic20mem_forced = i;
                log_cb(RETRO_LOG_INFO, "VIC-20 memory expansion force found in filename '%s': %dKB\n", argv, vic20mem);
                break;
            }
            else if (strcasestr(argv, vic20buf2))
            {
                vic20mem_forced = i;
                log_cb(RETRO_LOG_INFO, "VIC-20 memory expansion force found in path '%s': %dKB\n", argv, vic20mem);
                break;
            }
        }
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
            log_cb(RETRO_LOG_INFO, "M3U/VFL parsed, %d file(s) found\n", dc->count);

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
            log_cb(RETRO_LOG_INFO, "M3U/VFL parsed, %d file(s) found\n", dc->count);
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
#if defined(__X64__) || defined(__X64SC__)
        if (opt_jiffydos)
        {
            /* Disable JiffyDOS with tapes */
            opt_jiffydos_allow = 0;
            opt_jiffydos = 0;
            runstate = RUNSTATE_LOADED_CONTENT;
        }
#endif
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

    // Disable autostart only with disks or tapes
    if (noautostart && attachedImage != NULL)
       autostart_disable();

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
    {
        dc->eject_state = false;
        display_current_image(attachedImage, true);
    }
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

extern char archdep_startup_error[];

static void archdep_startup_error_log_lines()
{
    /* Message may contain several lines, log them separately for better readbility. */
    for (char *p=archdep_startup_error, *p_end;strlen(p);p=p_end)
    {
        if (!(p_end=strchr(p,'\n')))
            p_end=p+strlen(p);
        else
            *(p_end++)=0;
        log_cb(RETRO_LOG_WARN, "VICE: %s\n", p);
    }
}

int pre_main()
{
    int argc = PARAMCOUNT;

    /* start core with full params */
    build_params();

    *archdep_startup_error=0;
    if (skel_main(argc, (char**)xargv_cmd) < 0)
    {
        log_cb(RETRO_LOG_WARN, "Core startup failed with error:\n");
        archdep_startup_error_log_lines();
        log_cb(RETRO_LOG_INFO, "Core startup retry without parameters.\n");

        /* Show only first line in message to indicate something went wrong. */
        struct retro_message rmsg;
        rmsg.msg = archdep_startup_error;
        rmsg.frames = 500;
        environ_cb(RETRO_ENVIRONMENT_SET_MESSAGE, &rmsg);

        /* start core with empty params */
        xargv_cmd[0] = CORE_NAME;
        xargv_cmd[1] = NULL;
        argc = 1;

        *archdep_startup_error=0;
        if (skel_main(argc, (char**)xargv_cmd) < 0)
        {
            log_cb(RETRO_LOG_ERROR, "Core startup without parameters failed with error:\n");
            archdep_startup_error_log_lines();
            environ_cb(RETRO_ENVIRONMENT_SHUTDOWN, NULL);
        }
    }

    return 0;
}

static void update_variables(void);

extern int ui_init_finalize(void);

void reload_restart()
{
    /* Clear request */
    request_reload_restart = 0;

    /* Stop datasette */
    datasette_control(DATASETTE_CONTROL_STOP);

    /* Cleanup after previous content and reset resources */
    initcmdline_cleanup();

    /* Update resources from environment just like on fresh start of core */
    sound_volume_counter = 3;
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

#include "libretro-keyboard.i"
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
         "Memory Expansions",
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
            { "VDC", "VDC (80 cols)" },
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
            { "8296", NULL },
            { "SUPERPET", "SuperPET" },
            { NULL, NULL },
         },
         "8032"
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
      {
         "vice_jiffydos",
         "Use JiffyDOS",
         "For D64 & D81 disk images only!\nROMs required in 'system/vice':\n- 'JiffyDOS_C64.bin'\n- 'JiffyDOS_1541-II.bin'\n- 'JiffyDOS_1581.bin'",
         {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
         },
         "disabled"
      },
#endif
      {
         "vice_read_vicerc",
         "Read 'vicerc'",
         "Process 'system/vice/vicerc'. The config file can be used to set other options, such as cartridges.",
         {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
         },
         "enabled"
      },
      {
         "vice_reset",
         "Reset Type",
         "'Autostart' does hard reset and reruns content. 'Soft' keeps some code in memory, 'Hard' erases all memory.",
         {
            { "Autostart", NULL },
            { "Soft", NULL },
            { "Hard", NULL },
            { NULL, NULL },
         },
         "Autostart"
      },
      {
         "vice_autostart",
         "Autostart",
         "'Enabled' always runs content, 'Disabled' runs only PRG/CRT, 'Warp' turns warp mode on during autostart loading.",
         {
            { "disabled", NULL },
            { "enabled", NULL },
            { "warp", "Warp" },
            { NULL, NULL },
         },
         "enabled"
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
         "vice_audio_options_display",
         "Show Audio Options",
         "Core options page refresh required.",
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
         "Emulates the iconic floppy drive sounds.\n- True Drive Emulation & D64 disk image required.",
         {
            { "disabled", NULL },
            { "10\%", "10\% volume" },
            { "15\%", "15\% volume" },
            { "20\%", "20\% volume" },
            { "25\%", "25\% volume" },
            { "30\%", "30\% volume" },
            { "35\%", "35\% volume" },
            { "40\%", "40\% volume" },
            { "45\%", "45\% volume" },
            { "50\%", "50\% volume" },
            { "55\%", "55\% volume" },
            { "60\%", "60\% volume" },
            { "65\%", "65\% volume" },
            { "70\%", "70\% volume" },
            { "75\%", "75\% volume" },
            { "80\%", "80\% volume" },
            { "85\%", "85\% volume" },
            { "90\%", "90\% volume" },
            { "95\%", "95\% volume" },
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
            { "1", "100\% volume" },
            { "2", "200\% volume" },
            { "3", "300\% volume" },
            { "4", "400\% volume" },
            { "5", "500\% volume" },
            { "6", "600\% volume" },
            { "7", "700\% volume" },
            { "8", "800\% volume" },
            { "9", "900\% volume" },
            { "10", "1000\% volume" },
            { NULL, NULL },
         },
         "disabled"
      },
#endif
      {
         "vice_sound_sample_rate",
         "Sound Output Sample Rate",
         "Slightly higher quality or higher performance.",
         {
            { "22050", NULL },
            { "44100", NULL },
            { "48000", NULL },
            { "96000", NULL },
            { NULL, NULL },
         },
         "48000"
      },
#if !defined(__PET__) && !defined(__PLUS4__) && !defined(__VIC20__)
      {
         "vice_sid_engine",
         "SID Engine",
         "ReSID is accurate but slower.",
         {
            { "FastSID", NULL },
            { "ReSID", NULL },
            { "ReSID-3.3", NULL },
            { "ReSID-FP", NULL },
            { NULL, NULL },
         },
         "ReSID"
      },
      {
         "vice_sid_model",
         "SID Model",
         "The original C64 uses 6581, C64C uses 8580.",
         {
            { "Default", NULL },
            { "6581", NULL },
            { "8580", NULL },
            { "8580RD", "8580 ReSID + Digi Boost" },
            { NULL, NULL },
         },
         "Default"
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
#if defined(PSP) || defined(VITA) || defined(__SWITCH__)
         "Fast"
#else
         "Resampling"
#endif
      },
      {
         "vice_resid_passband",
         "ReSID Filter Passband",
         "Parameters for SID Filter.",
         {
            { "0", NULL },
            { "10", NULL },
            { "20", NULL },
            { "30", NULL },
            { "40", NULL },
            { "50", NULL },
            { "60", NULL },
            { "70", NULL },
            { "80", NULL },
            { "90", NULL },
            { NULL, NULL },
         },
         "90"
      },
      {
         "vice_resid_gain",
         "ReSID Filter Gain",
         "Parameters for SID Filter.",
         {
            { "90", NULL },
            { "91", NULL },
            { "92", NULL },
            { "93", NULL },
            { "94", NULL },
            { "95", NULL },
            { "96", NULL },
            { "97", NULL },
            { "98", NULL },
            { "99", NULL },
            { "100", NULL },
            { NULL, NULL },
         },
         "97"
      },
      {
         "vice_resid_filterbias",
         "ReSID Filter Bias",
         "Parameters for SID Filter.",
         {
            { "-5000", NULL },
            { "-4500", NULL },
            { "-4000", NULL },
            { "-3500", NULL },
            { "-3000", NULL },
            { "-2500", NULL },
            { "-2000", NULL },
            { "-1500", NULL },
            { "-1000", NULL },
            { "-500", NULL },
            { "0", NULL },
            { "500", NULL },
            { "1000", NULL },
            { "1500", NULL },
            { "2000", NULL },
            { "2500", NULL },
            { "3000", NULL },
            { "3500", NULL },
            { "4000", NULL },
            { "4500", NULL },
            { "5000", NULL },
            { NULL, NULL },
         },
         "500"
      },
      {
         "vice_resid_8580filterbias",
         "ReSID Filter 8580 Bias",
         "Parameters for SID Filter.",
         {
            { "-5000", NULL },
            { "-4500", NULL },
            { "-4000", NULL },
            { "-3500", NULL },
            { "-3000", NULL },
            { "-2500", NULL },
            { "-2000", NULL },
            { "-1500", NULL },
            { "-1000", NULL },
            { "-500", NULL },
            { "0", NULL },
            { "500", NULL },
            { "1000", NULL },
            { "1500", NULL },
            { "2000", NULL },
            { "2500", NULL },
            { "3000", NULL },
            { "3500", NULL },
            { "4000", NULL },
            { "4500", NULL },
            { "5000", NULL },
            { NULL, NULL },
         },
         "1500"
      },
#endif
      {
         "vice_video_options_display",
         "Show Video Options",
         "Core options page refresh required.",
         {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
         },
         "disabled"
      },


// VIDEO OPTONS START

#if defined(__X64__) || defined(__X64SC__) || defined(__X128__) || defined(__VIC20__) || defined(__PLUS4__)
      {
         "vice_aspect_ratio",
         "Aspect Ratio",
         "Automatic applies Pixel Aspect correction for the current system model\nPAL/NTSC force specific Pixel Aspect & can be used to stretch/compress opposite model\n1:1 does no Pixel Aspect correction",
         {
            { "auto", "Automatic" },
            { "pal", "PAL" },
            { "ntsc", "NTSC" },
            { "raw", "1:1" },
            { NULL, NULL },
         },
         "auto"
      },
#endif

#if !defined(__PET__) && !defined(__CBM2__)
      {
         "vice_border",
         "Display Borders",
         "Leave 'ON' otherwise may affect performance - Use Border Cropping options instead\n"
         "All cores except 'x64sc' reset when this option is changed",
         {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
         },
         "enabled"
      },
#endif

//   /* ### Bruno MENU  */  

#if defined(__X64__) || defined(__X64SC__) || defined(__X128__) || defined(__VIC20__) || defined(__PLUS4__)
      {
         "vice_bordercrop_horiz",
         "Crop Horizontal Borders",
		 "",
         {
            { "none", "disabled" },
            { "small", "Small Crop" },
            { "medium", "Medium Crop" },
            { "full", "Full Crop" },
            { NULL, NULL },
         },
         "none"
      },
      {
         "vice_zoom_mode",				// option name "vice_zoom_mode" for legacy compatibility with existing options files
         "Crop Vertical Borders",
         "Crop only when Display Borders 'ON'\nRetroArch Video/Scaling Settings required: Aspect Ratio: Core provided, Integer Scale: Off",
         {
            { "none", "disabled" },
            { "small", "Small Crop" },
            { "medium", "Medium Crop" },
            { "full", "Full Crop" },
            { "manual", "Manual Crop" },
            { NULL, NULL },
         },
         "none"
      },

      {
         "vice_bordercrop_top",
         "Manual Top Cropping",
         "",
         {
			{ "0", "0 px" },
			{ "1", "1 px" },
			{ "2", "2 px" },
			{ "3", "3 px" },
			{ "4", "4 px" },
			{ "5", "5 px" },
			{ "6", "6 px" },
			{ "7", "7 px" },
			{ "8", "8 px" },
			{ "9", "9 px" },
			{ "10", "10 px" },
			{ "11", "11 px" },
			{ "12", "12 px" },
			{ "13", "13 px" },
			{ "14", "14 px" },
			{ "15", "15 px" },
			{ "16", "16 px" },
			{ "17", "17 px" },
			{ "18", "18 px" },
			{ "19", "19 px" },
			{ "20", "20 px" },
			{ "21", "21 px" },
			{ "22", "22 px" },
			{ "23", "(NTSC Border) 23 px" },
			{ "24", "24 px" },
			{ "25", "25 px" },
			{ "26", "26 px" },
			{ "27", "27 px" },
			{ "28", "28 px" },
			{ "29", "29 px" },
			{ "30", "30 px" },
			{ "31", "31 px" },
			{ "32", "32 px" },
			{ "33", "33 px" },
			{ "34", "34 px" },
			{ "35", "(PAL Border) 35 px" },
			{ "36", "36 px" },
			{ "37", "37 px" },
			{ "38", "38 px" },
			{ "39", "39 px" },
			{ "40", "40 px" },
			{ "41", "41 px" },
			{ "42", "42 px" },
			{ "43", "43 px" },
			{ "44", "44 px" },
			{ "45", "45 px" },
			{ "46", "46 px" },
			{ "47", "47 px" },
			{ "48", "48 px" },
			{ "49", "49 px" },
			{ "50", "50 px" },
			{ "51", "51 px" },
			{ "52", "52 px" },
			{ "53", "53 px" },
			{ "54", "54 px" },
			{ "55", "55 px" },
			{ "56", "56 px" },
			{ "57", "57 px" },
			{ "58", "58 px" },
			{ "59", "59 px" },
			{ "60", "60 px" },
			{ NULL, NULL },
         },
         "none"
      },

      {
         "vice_bordercrop_bottom",
         "Manual Bottom Cropping",
         "Only when Crop Vertical Borders is set to 'Manual'\nUneven cropping can shift/offset screen area, useful for Game Overrides\nNormal border sizes: NTSC (23 px / 24 px), PAL (35 px / 37 px)",
         {
			{ "0", "0 px" },
 			{ "1", "1 px" },
 			{ "2", "2 px" },
 			{ "3", "3 px" },
 			{ "4", "4 px" },
 			{ "5", "5 px" },
 			{ "6", "6 px" },
 			{ "7", "7 px" },
 			{ "8", "8 px" },
 			{ "9", "9 px" },
 			{ "10", "10 px" },
 			{ "11", "11 px" },
 			{ "12", "12 px" },
 			{ "13", "13 px" },
 			{ "14", "14 px" },
 			{ "15", "15 px" },
 			{ "16", "16 px" },
 			{ "17", "17 px" },
 			{ "18", "18 px" },
 			{ "19", "19 px" },
 			{ "20", "20 px" },
 			{ "21", "21 px" },
 			{ "22", "22 px" },
 			{ "23", "23 px" },
 			{ "24", "(NTSC Border) 24 px" },
 			{ "25", "25 px" },
 			{ "26", "26 px" },
 			{ "27", "27 px" },
 			{ "28", "28 px" },
 			{ "29", "29 px" },
 			{ "30", "30 px" },
 			{ "31", "31 px" },
 			{ "32", "32 px" },
 			{ "33", "33 px" },
 			{ "34", "34 px" },
 			{ "35", "35 px" },
 			{ "36", "36 px" },
 			{ "37", "(PAL Border) 37 px" },
			{ "38", "38 px" },
			{ "39", "39 px" },
			{ "40", "40 px" },
			{ "41", "41 px" },
			{ "42", "42 px" },
			{ "43", "43 px" },
			{ "44", "44 px" },
			{ "45", "45 px" },
			{ "46", "46 px" },
			{ "47", "47 px" },
			{ "48", "48 px" },
			{ "49", "49 px" },
			{ "50", "50 px" },
			{ "51", "51 px" },
			{ "52", "52 px" },
			{ "53", "53 px" },
			{ "54", "54 px" },
			{ "55", "55 px" },
			{ "56", "56 px" },
			{ "57", "57 px" },
			{ "58", "58 px" },
			{ "59", "59 px" },
			{ "60", "60 px" },
            { NULL, NULL },
         },
         "none"
      },




#endif

// END VIDEO OPTONS

      {
         "vice_statusbar",
         "Statusbar Mode",
         "- Full: Joyports + Current image + LEDs\n- Basic: Current image + LEDs\n- Minimal: Track number + FPS hidden",
         {
            { "bottom", "Bottom Full" },
            { "bottom_minimal", "Bottom Full Minimal" },
            { "bottom_basic", "Bottom Basic" },
            { "bottom_basic_minimal", "Bottom Basic Minimal" },
            { "top", "Top Full" },
            { "top_minimal", "Top Full Minimal" },
            { "top_basic", "Top Basic" },
            { "top_basic_minimal", "Top Basic Minimal" },
            { NULL, NULL },
         },
         "bottom"
      },
      {
         "vice_vkbd_theme",
         "Virtual Keyboard Theme",
         "By default, the keyboard comes up with SELECT button or F11 key.",
         {
            { "0", "C64" },
            { "1", "C64C" },
            { "2", "Dark" },
            { "3", "Light" },
            { NULL, NULL },
         },
         "0"
      },
      {
         "vice_vkbd_alpha",
         "Virtual Keyboard Transparency",
         "",
         {
            { "0\%", NULL },
            { "5\%", NULL },
            { "10\%", NULL },
            { "15\%", NULL },
            { "20\%", NULL },
            { "25\%", NULL },
            { "30\%", NULL },
            { "35\%", NULL },
            { "40\%", NULL },
            { "45\%", NULL },
            { "50\%", NULL },
            { "55\%", NULL },
            { "60\%", NULL },
            { "65\%", NULL },
            { "70\%", NULL },
            { "75\%", NULL },
            { "80\%", NULL },
            { "85\%", NULL },
            { "90\%", NULL },
            { "95\%", NULL },
            { NULL, NULL },
         },
         "20\%"
      },
      {
         "vice_gfx_colors",
         "Color Depth",
         "24-bit is slower and not available on all platforms. Full restart required.",
         {
            { "16bit", "Thousands (16-bit)" },
            { "24bit", "Millions (24-bit)" },
            { NULL, NULL },
         },
         "16bit"
      },
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
            { "default", "Default" },
            { "colodore", "Colodore (PAL)" },
            { "pepto-pal", "Pepto (PAL)" },
            //{ "pepto-palold", "Pepto (old PAL)" },
            { "pepto-ntsc", "Pepto (NTSC)" },
            { "pepto-ntsc-sony", "Pepto (NTSC, Sony)" },
            { "cjam", "ChristopherJam" },
            { "c64hq", "C64HQ" },
            { "c64s", "C64S" },
            { "ccs64", "CCS64" },
            { "community-colors", "Community Colors" },
            { "deekay", "Deekay" },
            { "frodo", "Frodo" },
            { "godot", "Godot" },
            { "pc64", "PC64" },
            { "ptoing", "Ptoing" },
            { "rgb", "RGB" },
            { "vice", "VICE" },
            { NULL, NULL },
         },
         "default"
      },
      {
         "vice_vicii_color_gamma",
         "VIC-II Color Gamma",
         "Gamma for the internal default palette.",
         {
            { "1000", "1.00" },
            { "1100", "1.10" },
            { "1200", "1.20" },
            { "1300", "1.30" },
            { "1400", "1.40" },
            { "1500", "1.50" },
            { "1600", "1.60" },
            { "1700", "1.70" },
            { "1800", "1.80" },
            { "1900", "1.90" },
            { "2000", "2.00" },
            { "2100", "2.10" },
            { "2200", "2.20" },
            { "2300", "2.30" },
            { "2400", "2.40" },
            { "2500", "2.50" },
            { "2600", "2.60" },
            { "2700", "2.70" },
            { "2800", "2.80" },
            { "2900", "2.90" },
            { "3000", "3.00" },
            { "3100", "3.10" },
            { "3200", "3.20" },
            { "3300", "3.30" },
            { "3400", "3.40" },
            { "3500", "3.50" },
            { "3600", "3.60" },
            { "3700", "3.70" },
            { "3800", "3.80" },
            { "3900", "3.90" },
            { "4000", "4.00" },
            { NULL, NULL },
         },
         "2500"
      },
      {
         "vice_vicii_color_saturation",
         "VIC-II Color Saturation",
         "Saturation for the internal default palette.",
         {
            { "400", "20\%" },
            { "450", "22.5\%" },
            { "500", "25\%" },
            { "550", "27.5\%" },
            { "600", "30\%" },
            { "650", "32.5\%" },
            { "700", "35\%" },
            { "750", "37.5\%" },
            { "800", "40\%" },
            { "850", "42.5\%" },
            { "900", "45\%" },
            { "950", "47.5\%" },
            { "1000", "50\%" },
            { "1050", "52.5\%" },
            { "1100", "55\%" },
            { "1150", "57.5\%" },
            { "1200", "60\%" },
            { "1250", "62.5\%" },
            { "1300", "65\%" },
            { "1350", "67.5\%" },
            { "1400", "70\%" },
            { "1450", "72.5\%" },
            { "1500", "75\%" },
            { "1550", "77.5\%" },
            { "1600", "80\%" },
            { "1650", "82.5\%" },
            { "1700", "85\%" },
            { "1750", "87.5\%" },
            { "1800", "90\%" },
            { "1850", "92.5\%" },
            { "1900", "95\%" },
            { "1950", "97.5\%" },
            { "2000", "100\%" },
            { NULL, NULL },
         },
         "1000"
      },
      {
         "vice_vicii_color_contrast",
         "VIC-II Color Contrast",
         "Contrast for the internal default palette.",
         {
            { "400", "20\%" },
            { "450", "22.5\%" },
            { "500", "25\%" },
            { "550", "27.5\%" },
            { "600", "30\%" },
            { "650", "32.5\%" },
            { "700", "35\%" },
            { "750", "37.5\%" },
            { "800", "40\%" },
            { "850", "42.5\%" },
            { "900", "45\%" },
            { "950", "47.5\%" },
            { "1000", "50\%" },
            { "1050", "52.5\%" },
            { "1100", "55\%" },
            { "1150", "57.5\%" },
            { "1200", "60\%" },
            { "1250", "62.5\%" },
            { "1300", "65\%" },
            { "1350", "67.5\%" },
            { "1400", "70\%" },
            { "1450", "72.5\%" },
            { "1500", "75\%" },
            { "1550", "77.5\%" },
            { "1600", "80\%" },
            { "1650", "82.5\%" },
            { "1700", "85\%" },
            { "1750", "87.5\%" },
            { "1800", "90\%" },
            { "1850", "92.5\%" },
            { "1900", "95\%" },
            { "1950", "97.5\%" },
            { "2000", "100\%" },
            { NULL, NULL },
         },
         "1000"
      },
      {
         "vice_vicii_color_brightness",
         "VIC-II Color Brightness",
         "Brightness for the internal default palette.",
         {
            { "400", "20\%" },
            { "450", "22.5\%" },
            { "500", "25\%" },
            { "550", "27.5\%" },
            { "600", "30\%" },
            { "650", "32.5\%" },
            { "700", "35\%" },
            { "750", "37.5\%" },
            { "800", "40\%" },
            { "850", "42.5\%" },
            { "900", "45\%" },
            { "950", "47.5\%" },
            { "1000", "50\%" },
            { "1050", "52.5\%" },
            { "1100", "55\%" },
            { "1150", "57.5\%" },
            { "1200", "60\%" },
            { "1250", "62.5\%" },
            { "1300", "65\%" },
            { "1350", "67.5\%" },
            { "1400", "70\%" },
            { "1450", "72.5\%" },
            { "1500", "75\%" },
            { "1550", "77.5\%" },
            { "1600", "80\%" },
            { "1650", "82.5\%" },
            { "1700", "85\%" },
            { "1750", "87.5\%" },
            { "1800", "90\%" },
            { "1850", "92.5\%" },
            { "1900", "95\%" },
            { "1950", "97.5\%" },
            { "2000", "100\%" },
            { NULL, NULL },
         },
         "1000"
      },
#endif
#if !defined(__PET__) && !defined(__CBM2__) && !defined(__VIC20__)
      {
         "vice_joyport",
         "RetroPad Port",
         "Most games use port 2, some use port 1.\nFilename forcing or hotkey toggling will disable this option until core restart.",
         {
            { "Port 2", NULL },
            { "Port 1", NULL },
            { NULL, NULL },
         },
         "Port 2"
      },
      {
         "vice_joyport_type",
         "RetroPad Port Type",
         "Non-joysticks will be plugged into current port only and are controlled with the left analog stick or a real mouse. Paddles will be split to 1st and 2nd RetroPort.",
         {
            { "1", "Joystick" },
            { "2", "Paddles" },
            { "3", "Mouse (1351)" },
            { "4", "Mouse (NEOS)" },
            { "5", "Mouse (Amiga)" },
            { "6", "Trackball (Atari CX-22)" },
            { "7", "Mouse (Atari ST)" },
            { "8", "Mouse (SmartMouse)" },
            { "9", "Mouse (Micromys)" },
            { "10", "Koalapad" },
            { NULL, NULL },
         },
         "1"
      },
      {
         "vice_analogmouse_deadzone",
         "Analog Stick Mouse Deadzone",
         "",
         {
            { "0", "0\%" },
            { "5", "5\%" },
            { "10", "10\%" },
            { "15", "15\%" },
            { "20", "20\%" },
            { "25", "25\%" },
            { "30", "30\%" },
            { "35", "35\%" },
            { "40", "40\%" },
            { "45", "45\%" },
            { "50", "50\%" },
            { NULL, NULL },
         },
         "15"
      },
      {
         "vice_analogmouse_speed",
         "Analog Stick Mouse Speed",
         "",
         {
            { "0.5", "50\%" },
            { "0.6", "60\%" },
            { "0.7", "70\%" },
            { "0.8", "80\%" },
            { "0.9", "90\%" },
            { "1.0", "100\%" },
            { "1.1", "110\%" },
            { "1.2", "120\%" },
            { "1.3", "130\%" },
            { "1.4", "140\%" },
            { "1.5", "150\%" },
            { NULL, NULL },
         },
         "1.0"
      },
      {
         "vice_dpadmouse_speed",
         "D-Pad Mouse Speed",
         "",
         {
            { "3", "50\%" },
            { "4", "66\%" },
            { "5", "83\%" },
            { "6", "100\%" },
            { "7", "116\%" },
            { "8", "133\%" },
            { "9", "150\%" },
            { "10", "166\%" },
            { "11", "183\%" },
            { "12", "200\%" },
            { NULL, NULL },
         },
         "6"
      },
      {
         "vice_mouse_speed",
         "Mouse Speed",
         "Affects mouse speed globally.",
         {
            { "10", "10\%" },
            { "20", "20\%" },
            { "30", "30\%" },
            { "40", "40\%" },
            { "50", "50\%" },
            { "60", "60\%" },
            { "70", "70\%" },
            { "80", "80\%" },
            { "90", "90\%" },
            { "100", "100\%" },
            { "110", "110\%" },
            { "120", "120\%" },
            { "130", "130\%" },
            { "140", "140\%" },
            { "150", "150\%" },
            { "160", "160\%" },
            { "170", "170\%" },
            { "180", "180\%" },
            { "190", "190\%" },
            { "200", "200\%" },
            { NULL, NULL },
         },
         "100"
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
      {
         "vice_mapping_options_display",
         "Show Mapping Options",
         "Show options for hotkeys & RetroPad mappings.\nCore options page refresh required.",
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
         "RETROK_F12"
      },
#if !defined(__PET__) && !defined(__CBM2__) && !defined(__VIC20__)
      {
         "vice_mapper_joyport_switch",
         "Hotkey: Switch Joyports",
         "Press the mapped key to switch joyports 1 & 2.\nSwitching will disable 'RetroPad Port' option until core restart.",
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

   /* ### Bruno MAPPER Hotkeys Menu */

#if defined(__X64__) || defined(__X64SC__) || defined(__X128__) || defined(__VIC20__) || defined(__PLUS4__)
      {
         "vice_mapper_zoom_mode_toggle",
         "Hotkey: Toggle Zoom Mode",
         "Press the mapped key to toggle zoom mode.",
         {{ NULL, NULL }},
         "---"
      },


      {
         "vice_mapper_bcrop_horiz_mode_cycle",
         "Hotkey: Cycle Horizontal Crop Modes",
         "Press the mapped key to cycle modes.",
         {{ NULL, NULL }},
         "---"
      },


      {
         "vice_mapper_bcrop_vert_mode_cycle",
         "Hotkey: Cycle Vertical Crop Modes",
         "Press the mapped key to cycle modes.",
         {{ NULL, NULL }},
         "---"
      },



#endif
      {
         "vice_mapper_warp_mode",
         "Hotkey: Hold Warp Mode",
         "Hold the mapped key for warp mode.",
         {{ NULL, NULL }},
         ""
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
         "TOGGLE_VKBD"
      },
      {
         "vice_mapper_start",
         "RetroPad Start",
         "",
         {{ NULL, NULL }},
         "---"
      },
      {
         "vice_mapper_b",
         "RetroPad B",
         "Unmapped defaults to fire button.",
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
         "---"
      },
      {
         "vice_mapper_r",
         "RetroPad R",
         "",
         {{ NULL, NULL }},
         "---"
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
         "---"
      },
      {
         "vice_mapper_r3",
         "RetroPad R3",
         "",
         {{ NULL, NULL }},
         "---"
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
      /* Turbo Fire */
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
      {
         "vice_retropad_options",
         "RetroPad Face Button Options",
         "Rotate face buttons clockwise and/or make 2nd fire press up.",
         {
            { "disabled", "B = Fire" },
            { "jump", "B = Fire, A = Up" },
            { "rotate", "Y = Fire" },
            { "rotate_jump", "Y = Fire, B = Up" },
            { NULL, NULL },
         },
         "disabled"
      },

      { NULL, NULL, NULL, {{0}}, NULL },
   };

   /* fill in the values for all the mappers */
   int i = 0;
   int j = 0;
   int hotkey = 0;
   while (core_options[i].key) 
   {
      if (strstr(core_options[i].key, "vice_mapper_"))
      {

   /* ### Bruno MAPPER Hotkeys */

         /* Show different key list for hotkeys (special negatives removed) */
         if (  strstr(core_options[i].key, "vice_mapper_vkbd")
            || strstr(core_options[i].key, "vice_mapper_statusbar")
            || strstr(core_options[i].key, "vice_mapper_joyport_switch")
            || strstr(core_options[i].key, "vice_mapper_reset")
            || strstr(core_options[i].key, "vice_mapper_zoom_mode_toggle")
            || strstr(core_options[i].key, "vice_mapper_bcrop_horiz_mode_cycle")
            || strstr(core_options[i].key, "vice_mapper_bcrop_vert_mode_cycle")
            || strstr(core_options[i].key, "vice_mapper_warp_mode")
            || strstr(core_options[i].key, "vice_mapper_datasette_toggle_hotkeys")
            || strstr(core_options[i].key, "vice_mapper_datasette_start")
            || strstr(core_options[i].key, "vice_mapper_datasette_stop")
            || strstr(core_options[i].key, "vice_mapper_datasette_rewind")
            || strstr(core_options[i].key, "vice_mapper_datasette_forward")
            || strstr(core_options[i].key, "vice_mapper_datasette_reset")
            )
            hotkey = 1;
         else
            hotkey = 0;

         j = 0;
         if (hotkey)
         {
             while (keyDescHotkeys[j] && j < RETRO_NUM_CORE_OPTION_VALUES_MAX - 1)
             {
                core_options[i].values[j].value = keyDescHotkeys[j];
                core_options[i].values[j].label = NULL;
                ++j;
             };
         }
         else
         {
             while (keyDesc[j] && j < RETRO_NUM_CORE_OPTION_VALUES_MAX - 1)
             {
                core_options[i].values[j].value = keyDesc[j];
                core_options[i].values[j].label = NULL;
                ++j;
             };
         }
         core_options[i].values[j].value = NULL;
         core_options[i].values[j].label = NULL;
      };
      ++i;
   }

   environ_cb = cb;
   cb(RETRO_ENVIRONMENT_SET_CONTROLLER_INFO, (void*)ports);

   unsigned version = 0;
   if (!cb(RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION, &version))
   {
      /* Only log the error if we were not called after retro_deinit */
      if (log_cb)
         log_cb(RETRO_LOG_WARN,"retro_set_environment: GET_CORE_OPTIONS_VERSION failed, not setting core-options now.\n");
   }
   else if (version == 1)
      cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS, core_options);
   else
   {
      /* Fallback for older API */

      /* Use define because C doesn't care about const. */
#define NUM_CORE_OPTIONS ( sizeof(core_options)/sizeof(core_options[0])-1 )
      static struct retro_variable variables[NUM_CORE_OPTIONS+1];

      /* Only generate variables once, it's as static as core_options */
      if (!core_options_legacy_strings)
      {
         /* First pass: Calculate size of string-buffer required */
         unsigned buf_len;
         char *buf;
         {
            unsigned alloc_len=0;
            struct retro_core_option_definition *o=core_options+NUM_CORE_OPTIONS-1;
            struct retro_variable *rv=variables+NUM_CORE_OPTIONS-1;
            for (; o>=core_options; --o, --rv)
            {
               int l=snprintf(0,0,"%s; %s",o->desc,o->default_value);
               for (struct retro_core_option_value *v=o->values;v->value;++v)
                  l+=snprintf(0,0,"|%s",v->value);
               alloc_len+=l+1;
            }
            buf=core_options_legacy_strings=(char *)malloc(alloc_len);
            buf_len=alloc_len;
         }
         /* Second pass: Fill string-buffer */
         struct retro_core_option_definition *o=core_options+NUM_CORE_OPTIONS-1;
         struct retro_variable *rv=variables+NUM_CORE_OPTIONS;
         rv->key = rv->value = 0;
         --rv;
         for (; o>=core_options; --o, --rv)
         {
            int l=snprintf(buf,buf_len,"%s; %s",o->desc,o->default_value);
            for (struct retro_core_option_value *v=o->values;v->value;++v)
               l+=snprintf(buf+l,buf_len,"|%s",v->value);
            rv->key = o->key;
            rv->value = buf;
            ++l;
            buf+=l, buf_len-=l;
         }
      }
      cb( RETRO_ENVIRONMENT_SET_VARIABLES, variables);
#undef NUM_CORE_OPTIONS
   }

   static bool allowNoGameMode;
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
   struct retro_core_option_display option_display;

   log_cb(RETRO_LOG_INFO, "Updating variables, UI finalized = %d\n", retro_ui_finalized);

   var.key = "vice_autostart";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (retro_ui_finalized)
      {
         if (strcmp(var.value, "warp") == 0)
            log_resources_set_int("AutostartWarp", 1);
         else
            log_resources_set_int("AutostartWarp", 0);
      }
      else
      {
         if (strcmp(var.value, "warp") == 0) RETROAUTOSTARTWARP=1;
         else RETROAUTOSTARTWARP=0;
      }

      if (strcmp(var.value, "disabled") == 0)
         noautostart = true;
      else
         noautostart = false;
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
      else
      {
         audioleak=1;
         opt_audio_leak_volume=atoi(var.value);
      }

      if (retro_ui_finalized && RETROAUDIOLEAK != audioleak)
#if defined(__X64__) || defined(__X64SC__) || defined(__X128__)
         log_resources_set_int("VICIIAudioLeak", audioleak);
#elif defined(__VIC20__)
         log_resources_set_int("VICAudioLeak", audioleak);
#endif
      RETROAUDIOLEAK=audioleak;
   }
#endif

   var.key = "vice_sound_sample_rate";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      RETROSOUNDSAMPLERATE=atoi(var.value);
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

      if (retro_ui_finalized && RETROC64MODL != modl)
         vic20model_set(modl);
      RETROC64MODL=modl;
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
      {
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

      if (retro_ui_finalized && RETROC64MODL != modl)
         plus4model_set(modl);
      RETROC64MODL=modl;
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

      if (retro_ui_finalized && RETROC64MODL != modl)
         c128model_set(modl);
      RETROC64MODL=modl;
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
      
      if (retro_ui_finalized && RETROC64MODL != modl)
      {
         petmodel_set(modl);
         // Keyboard layout refresh required. All models below 8032 except B models use graphics layout, others use business.
         keyboard_init();
      }
      RETROC64MODL=modl;
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

      if (retro_ui_finalized && RETROC64MODL != modl)
         cbm2model_set(modl);
      RETROC64MODL=modl;
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

      if (retro_ui_finalized && RETROC64MODL != modl)
         c64model_set(modl);
      RETROC64MODL=modl;
   }
#endif

#if !defined(__PET__) && !defined(__PLUS4__) && !defined(__VIC20__)
   var.key = "vice_sid_engine";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int eng=0;

      if (strcmp(var.value, "ReSID") == 0) { eng=1; }
      else if (strcmp(var.value, "ReSID-3.3") == 0) { eng=6; }
      else if (strcmp(var.value, "ReSID-FP") == 0) { eng=7; }

      if (retro_ui_finalized)
         if (RETROSIDENGINE != eng)
            if (RETROSIDMODL == 0xff)
               resources_set_int("SidEngine", eng);
            else
               sid_set_engine_model(eng, RETROSIDMODL);

      RETROSIDENGINE=eng;
   }

   var.key = "vice_sid_model";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int modl=0xff;

      if (strcmp(var.value, "6581") == 0) { modl=0; }
      else if (strcmp(var.value, "8580") == 0) { modl=1; }
      /* There is no digiboost for FastSID (and it's not needed either) */
      else if (strcmp(var.value, "8580RD") == 0) { modl=(!RETROSIDENGINE ? 1 : 2); }

      if (retro_ui_finalized)
         if (modl != 0xff)
            sid_set_engine_model(RETROSIDENGINE, modl);

      RETROSIDMODL=modl;
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

   var.key = "vice_resid_passband";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int val = atoi(var.value);

      if (retro_ui_finalized)
         if (RETRORESIDPASSBAND != val)
         {
            log_resources_set_int("SidResidPassband", val);
            log_resources_set_int("SidResid8580Passband", val);
         }
         RETRORESIDPASSBAND=val;
   }

   var.key = "vice_resid_gain";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int val = atoi(var.value);

      if (retro_ui_finalized)
         if (RETRORESIDGAIN != val)
         {
            log_resources_set_int("SidResidGain", val);
            log_resources_set_int("SidResid8580Gain", val);
         }
         RETRORESIDGAIN=val;
   }

   var.key = "vice_resid_filterbias";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int val = atoi(var.value);

      if (retro_ui_finalized)
         if (RETRORESIDFILTERBIAS != val)
            log_resources_set_int("SidResidFilterBias", val);
         RETRORESIDFILTERBIAS=val;
   }

   var.key = "vice_resid_8580filterbias";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int val = atoi(var.value);

      if (retro_ui_finalized)
         if (RETRORESID8580FILTERBIAS != val)
            log_resources_set_int("SidResid8580FilterBias", val);
         RETRORESID8580FILTERBIAS=val;
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
         log_resources_set_int(resources_var_border, border);
   }
#endif

   /* ### Bruno Setting CROPPING values */

#if defined(__X64__) || defined(__X64SC__) || defined(__X128__) || defined(__VIC20__) || defined(__PLUS4__)

	// Vertical Cropping Mode (Zoom Mode) 
   var.key = "vice_zoom_mode";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "none") == 0) zoom_mode_id=0;
      else if (strcmp(var.value, "small") == 0) zoom_mode_id=1;
      else if (strcmp(var.value, "medium") == 0) zoom_mode_id=2;
      else if (strcmp(var.value, "full") == 0) zoom_mode_id=3;
      else if (strcmp(var.value, "manual") == 0) zoom_mode_id=4;

      if (RETROBORDERS)
         zoom_mode_id = 0;
#if defined(__X128__)
      if (RETROC128COLUMNKEY==0)
         zoom_mode_id = 0;
#endif

      opt_zoom_mode_id = zoom_mode_id;
   }

	// Manual Top & Bottom Cropping values in Pixels for zoom_mode_id 4 (manual vertical cropping)
   var.key = "vice_bordercrop_top";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      crop_top_px = atoi(var.value);
      
      if (crop_top_px != crop_top_px_prev)
      {
          zoom_mode_id_prev = -1;	// forces geometry to update
      }
   }

   var.key = "vice_bordercrop_bottom";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      crop_bottom_px = atoi(var.value);

      if (crop_bottom_px != crop_bottom_px_prev)
      {
          zoom_mode_id_prev = -1;	// forces geometry to update
      }
   }

	// Horizontal Cropping Mode 
   var.key = "vice_bordercrop_horiz";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "none") == 0) bcrop_horiz_mode=0;
      else if (strcmp(var.value, "small") == 0) bcrop_horiz_mode=1;
      else if (strcmp(var.value, "medium") == 0) bcrop_horiz_mode=2;
      else if (strcmp(var.value, "full") == 0) bcrop_horiz_mode=3;

      if (RETROBORDERS)
         bcrop_horiz_mode = 0;
#if defined(__X128__)
      if (RETROC128COLUMNKEY==0)
         bcrop_horiz_mode = 0;
#endif

   }


// End new Cropping variable setting


   var.key = "vice_aspect_ratio";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "auto") == 0) opt_aspect_ratio=0;
      else if (strcmp(var.value, "pal") == 0) opt_aspect_ratio=1;
      else if (strcmp(var.value, "ntsc") == 0) opt_aspect_ratio=2;
      else if (strcmp(var.value, "raw") == 0) opt_aspect_ratio=3;

   /* ### Bruno RESET */

      // Zoom reset
      if (opt_aspect_ratio != opt_aspect_ratio_prev)
      {
         opt_aspect_ratio_prev = opt_aspect_ratio;
         zoom_mode_id_prev = -1;
      }
   }
#endif

   var.key = "vice_gfx_colors";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      // Only allow screenmode change after restart
      if (!pix_bytes_initialized)
      {
         if (strcmp(var.value, "16bit") == 0) pix_bytes=2;
         else if (strcmp(var.value, "24bit") == 0) pix_bytes=4;
         pix_bytes_initialized = true;
      }
   }

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
      if (retro_ui_finalized)
      {
         if (strcmp(var.value, "default") == 0)
            log_resources_set_int("VICIIExternalPalette", 0);
         else
         {
            log_resources_set_int("VICIIExternalPalette", 1);
            log_resources_set_string("VICIIPaletteFile", var.value);
         }
      }
      else
      {
         if (strcmp(var.value, "default") == 0)
            RETROEXTPAL=-1;
         else
         {
            RETROEXTPAL=1;
            sprintf(RETROEXTPALNAME, "%s", var.value);
         }
      }
   }

   var.key = "vice_vicii_color_gamma";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int color_gamma = atoi(var.value);

      if (retro_ui_finalized)
         log_resources_set_int("VICIIColorGamma", color_gamma);

      RETROVICIICOLORGAMMA=color_gamma;
   }

   var.key = "vice_vicii_color_saturation";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int color_saturation = atoi(var.value);

      if (retro_ui_finalized)
         log_resources_set_int("VICIIColorSaturation", color_saturation);

      RETROVICIICOLORSATURATION=color_saturation;
   }

   var.key = "vice_vicii_color_contrast";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int color_contrast = atoi(var.value);

      if (retro_ui_finalized)
         log_resources_set_int("VICIIColorContrast", color_contrast);

      RETROVICIICOLORCONTRAST=color_contrast;
   }

   var.key = "vice_vicii_color_brightness";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int color_brightness = atoi(var.value);

      if (retro_ui_finalized)
         log_resources_set_int("VICIIColorBrightness", color_brightness);

      RETROVICIICOLORBRIGHTNESS=color_brightness;
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

#if !defined(__PET__) && !defined(__CBM2__) && !defined(__VIC20__)
   var.key = "vice_joyport";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "Port 2") == 0 && !cur_port_locked) cur_port=2;
      else if (strcmp(var.value, "Port 1") == 0 && !cur_port_locked) cur_port=1;
   }

   var.key = "vice_joyport_type";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      opt_joyport_type = atoi(var.value);
   }

   var.key = "vice_analogmouse_deadzone";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      opt_analogmouse_deadzone = atoi(var.value);
   }

   var.key = "vice_analogmouse_speed";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      opt_analogmouse_speed = atof(var.value);
   }

   var.key = "vice_dpadmouse_speed";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      opt_dpadmouse_speed = atoi(var.value);
   }

   var.key = "vice_mouse_speed";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      opt_mouse_speed = atoi(var.value);
   }

#endif

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

   var.key = "vice_retropad_options";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "disabled") == 0) opt_retropad_options=0;
      else if (strcmp(var.value, "rotate") == 0) opt_retropad_options=1;
      else if (strcmp(var.value, "jump") == 0) opt_retropad_options=2;
      else if (strcmp(var.value, "rotate_jump") == 0) opt_retropad_options=3;
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
      turbo_pulse=atoi(var.value);
   }

   var.key = "vice_reset";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "Autostart") == 0) RETRORESET=0;
      else if (strcmp(var.value, "Soft") == 0) RETRORESET=1;
      else if (strcmp(var.value, "Hard") == 0) RETRORESET=2;
   }

   var.key = "vice_vkbd_theme";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      RETROTHEME=atoi(var.value);
      opt_vkbd_theme=RETROTHEME;
   }

   var.key = "vice_vkbd_alpha";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      opt_vkbd_alpha = 255 - (255 * atoi(var.value) / 100);
      vkbd_alpha = opt_vkbd_alpha;
   }

   var.key = "vice_statusbar";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      opt_statusbar = 0;

      if (strstr(var.value, "top"))
         opt_statusbar |= STATUSBAR_TOP;
      else
         opt_statusbar |= STATUSBAR_BOTTOM;

      if (strstr(var.value, "basic"))
         opt_statusbar |= STATUSBAR_BASIC;

      if (strstr(var.value, "minimal"))
         opt_statusbar |= STATUSBAR_MINIMAL;
   }

   var.key = "vice_mapping_options_display";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "disabled") == 0) opt_mapping_options_display=0;
      else if (strcmp(var.value, "enabled") == 0) opt_mapping_options_display=1;
   }

   var.key = "vice_audio_options_display";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "disabled") == 0) opt_audio_options_display=0;
      else if (strcmp(var.value, "enabled") == 0) opt_audio_options_display=1;
   }

   var.key = "vice_video_options_display";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "disabled") == 0) opt_video_options_display=0;
      else if (strcmp(var.value, "enabled") == 0) opt_video_options_display=1;
   }

   var.key = "vice_read_vicerc";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "disabled") == 0) opt_read_vicerc=0;
      else if (strcmp(var.value, "enabled") == 0) opt_read_vicerc=1;

      request_reload_restart = (opt_read_vicerc != opt_read_vicerc_prev) ? 1 : request_reload_restart;
      opt_read_vicerc_prev = opt_read_vicerc;
   }

#if defined(__X64__) || defined(__X64SC__)
   var.key = "vice_jiffydos";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "disabled") == 0) opt_jiffydos=0;
      else if (strcmp(var.value, "enabled") == 0) opt_jiffydos=1;

      if (!opt_jiffydos_allow)
         opt_jiffydos = 0;

      request_reload_restart = (opt_jiffydos != opt_jiffydos_prev) ? 1 : request_reload_restart;
      opt_jiffydos_prev = opt_jiffydos;
   }
#endif

   /* Mapper */
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

   var.key = "vice_mapper_b";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[RETRO_DEVICE_ID_JOYPAD_B] = keyId(var.value);
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

   /* ### Bruno MAPPER hotkey toggle */

   var.key = "vice_mapper_zoom_mode_toggle";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[28] = keyId(var.value);
   }

   var.key = "vice_mapper_warp_mode";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[29] = keyId(var.value);
   }

   var.key = "vice_mapper_bcrop_horiz_mode_cycle";		// Bruno CYCLE
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[30] = keyId(var.value);
   }

   var.key = "vice_mapper_bcrop_vert_mode_cycle";		// Bruno CYCLE
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[31] = keyId(var.value);
   }

   var.key = "vice_datasette_hotkeys";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "enabled") == 0) datasette_hotkeys=1;
      else if (strcmp(var.value, "disabled") == 0) datasette_hotkeys=0;
   }

   var.key = "vice_mapper_datasette_toggle_hotkeys";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[32] = keyId(var.value);
   }
   
   var.key = "vice_mapper_datasette_stop";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[33] = keyId(var.value);
   }

   var.key = "vice_mapper_datasette_start";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[34] = keyId(var.value);
   }

   var.key = "vice_mapper_datasette_forward";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[35] = keyId(var.value);
   }

   var.key = "vice_mapper_datasette_rewind";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[36] = keyId(var.value);
   }

   var.key = "vice_mapper_datasette_reset";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[37] = keyId(var.value);
   }


   /*** Options display ***/

   /* ### Bruno MAPPY  HIDE hotkeys - shouldn't need to change anything here*/

   /* Mapping options */
   option_display.visible = opt_mapping_options_display;

   option_display.key = "vice_mapper_select";
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
   option_display.key = "vice_mapper_start";
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
   option_display.key = "vice_mapper_b";
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
   option_display.key = "vice_mapper_a";
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
   option_display.key = "vice_mapper_y";
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
   option_display.key = "vice_mapper_x";
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
   option_display.key = "vice_mapper_l";
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
   option_display.key = "vice_mapper_r";
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
   option_display.key = "vice_mapper_l2";
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
   option_display.key = "vice_mapper_r2";
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
   option_display.key = "vice_mapper_l3";
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
   option_display.key = "vice_mapper_r3";
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
   option_display.key = "vice_mapper_lu";
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
   option_display.key = "vice_mapper_ld";
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
   option_display.key = "vice_mapper_ll";
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
   option_display.key = "vice_mapper_lr";
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
   option_display.key = "vice_mapper_ru";
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
   option_display.key = "vice_mapper_rd";
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
   option_display.key = "vice_mapper_rl";
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
   option_display.key = "vice_mapper_rr";
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
   option_display.key = "vice_mapper_vkbd";
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
   option_display.key = "vice_mapper_statusbar";
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
#if !defined(__PET__) && !defined(__CBM2__) && !defined(__VIC20__)
   option_display.key = "vice_mapper_joyport_switch";
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
#endif

//   /* ### Bruno */  MAPPER SETTINGS - button/key shortcuts to toggle features

#if defined(__X64__) || defined(__X64SC__) || defined(__X128__) || defined(__VIC20__) || defined(__PLUS4__)
   option_display.key = "vice_mapper_zoom_mode_toggle";
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);

   option_display.key = "vice_mapper_bcrop_horiz_mode_cycle";					// Bruno CYCLE
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
   option_display.key = "vice_mapper_bcrop_vert_mode_cycle";
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
   
#endif
   option_display.key = "vice_mapper_reset";
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
   option_display.key = "vice_mapper_warp_mode";
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
   option_display.key = "vice_mapper_datasette_toggle_hotkeys";
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
   option_display.key = "vice_mapper_datasette_start";
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
   option_display.key = "vice_mapper_datasette_stop";
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
   option_display.key = "vice_mapper_datasette_rewind";
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
   option_display.key = "vice_mapper_datasette_forward";
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
   option_display.key = "vice_mapper_datasette_reset";
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);

   /* Audio options */
   option_display.visible = opt_audio_options_display;

   option_display.key = "vice_drive_sound_emulation";
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
#if defined(__X64__) || defined(__X64SC__) || defined(__X128__) || defined(__VIC20__)
   option_display.key = "vice_audio_leak_emulation";
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
#endif
   option_display.key = "vice_sound_sample_rate";
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
#if !defined(__PET__) && !defined(__PLUS4__) && !defined(__VIC20__)
   option_display.key = "vice_sid_engine";
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
   option_display.key = "vice_sid_model";
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
   option_display.key = "vice_resid_sampling";
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
   option_display.key = "vice_resid_passband";
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
   option_display.key = "vice_resid_gain";
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
   option_display.key = "vice_resid_filterbias";
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
   option_display.key = "vice_resid_8580filterbias";
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
#endif

//   /* ### Bruno */  HIDE VIDEO SETTINGS

   /* Video options */
   option_display.visible = opt_video_options_display;

#if !defined(__PET__) && !defined(__CBM2__)
   option_display.key = "vice_border";
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
#endif
#if defined(__X64__) || defined(__X64SC__) || defined(__X128__) || defined(__VIC20__) || defined(__PLUS4__)
   option_display.key = "vice_bordercrop_horiz";							//bruno
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);	//bruno
   option_display.key = "vice_bordercrop_top";								//bruno
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);	//bruno
   option_display.key = "vice_bordercrop_bottom";							//bruno
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);	//bruno
   option_display.key = "vice_zoom_mode";
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
   option_display.key = "vice_aspect_ratio";
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
#endif
   option_display.key = "vice_gfx_colors";
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
#if defined(__VIC20__)
   option_display.key = "vice_vic20_external_palette";
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
#elif defined(__PLUS4__)
   option_display.key = "vice_plus4_external_palette";
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
#elif defined(__PET__)
   option_display.key = "vice_pet_external_palette";
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
#elif defined(__CBM2__)
   option_display.key = "vice_cbm2_external_palette";
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
#else
   option_display.key = "vice_external_palette";
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
   option_display.key = "vice_vicii_color_gamma",
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
   option_display.key = "vice_vicii_color_saturation",
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
   option_display.key = "vice_vicii_color_contrast",
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
   option_display.key = "vice_vicii_color_brightness",
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
#endif
   option_display.key = "vice_vkbd_theme";
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
   option_display.key = "vice_vkbd_alpha";
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
   option_display.key = "vice_statusbar";
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
}

void emu_reset(void)
{
   // Always stop datasette or autostart from tape will fail
   datasette_control(DATASETTE_CONTROL_STOP);

   // Always disable Warp
   resources_set_int("WarpMode", 0);

   /* Changing opt_read_vicerc requires reloading */
   if (request_reload_restart)
      reload_restart();

   switch (RETRORESET)
   {
      case 0:
         machine_trigger_reset(MACHINE_RESET_MODE_HARD);
         if (autostartString != NULL && autostartString[0] != '\0' && !noautostart)
            autostart_autodetect(autostartString, NULL, 0, AUTOSTART_MODE_RUN);
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

   // Always disable Warp
   resources_set_int("WarpMode", 0);

   // Changing opt_read_vicerc requires reloading
   if (request_reload_restart)
      reload_restart();

   // Retro reset should always hard reset & autostart
   machine_trigger_reset(MACHINE_RESET_MODE_HARD);
   if (autostartString != NULL && autostartString[0] != '\0' && !noautostart)
      autostart_autodetect(autostartString, NULL, 0, AUTOSTART_MODE_RUN);
}

struct DiskImage {
    char* fname;
};

static bool retro_set_eject_state(bool ejected)
{
    if (dc)
    {
        int unit = get_image_unit();

        if (dc->eject_state == ejected)
            return true;
        else
            dc->eject_state = ejected;

        if (ejected && dc->index <= dc->count)
        {
            dc->eject_state = ejected;
            if (unit == 1)
                tape_image_detach(unit);
            else
                file_system_detach_disk(unit);

            display_current_image("", false);
            return true;
        }
        else if (!ejected && dc->index < dc->count && dc->files[dc->index] != NULL)
        {
            dc->eject_state = ejected;
            if (unit == 1)
                tape_image_attach(unit, dc->files[dc->index]);
            else
            {
                file_system_attach_disk(unit, dc->files[dc->index]);

                int drive_type;
                resources_get_int("Drive8Type", &drive_type);

                // Autodetect drive type
                vdrive_t *vdrive;
                struct disk_image_s *diskimg;

                vdrive = file_system_get_vdrive(8);
                if (vdrive == NULL)
                    log_cb(RETRO_LOG_ERROR, "Failed to get vdrive reference for unit 8.\n");
                else
                {
                    diskimg = vdrive->image;
                    if (diskimg == NULL)
                        log_cb(RETRO_LOG_ERROR, "Failed to get disk image for unit 8.\n");
                    else
                    {
                        log_cb(RETRO_LOG_INFO, "Autodetected image type %u.\n", diskimg->type);
                        if (log_resources_set_int("Drive8Type", diskimg->type) < 0)
                            log_cb(RETRO_LOG_ERROR, "Failed to set drive type.\n");

                        // Change from 1581 to 1541 will not detect disk properly without reattaching (?!)
                        if (diskimg->type != drive_type)
                            file_system_attach_disk(unit, dc->files[dc->index]);
                    }
                }
            }

            display_current_image(dc->files[dc->index], true);
            return true;
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
bool retro_set_image_index(unsigned index)
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
                display_current_image(dc->files[dc->index], false);
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
static bool retro_replace_image_index(unsigned index, const struct retro_game_info *info)
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
        return true;
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
            dc->names[dc->count] = NULL;
            dc->count++;
            return true;
        }
    }

    return false;
}

static bool retro_get_image_path(unsigned index, char *path, size_t len)
{
    if (len < 1)
        return false;

    if (dc)
    {
        if (index < dc->count)
        {
            if (!string_is_empty(dc->files[index]))
            {
                strlcpy(path, dc->files[index], len);
                return true;
            }
        }
    }

   return false;
}

static bool retro_get_image_label(unsigned index, char *label, size_t len)
{
    if (len < 1)
        return false;

    if (dc)
    {
        if (index < dc->count)
        {
            if (!string_is_empty(dc->names[index]))
            {
                strlcpy(label, dc->names[index], len);
                return true;
            }
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

static struct retro_disk_control_ext_callback diskControlExt = {
   retro_set_eject_state,
   retro_get_eject_state,
   retro_get_image_index,
   retro_set_image_index,
   retro_get_num_images,
   retro_replace_image_index,
   retro_add_image_index,
   NULL, // set_initial_image
   retro_get_image_path,
   retro_get_image_label,
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
      strlcpy(
            retro_system_directory,
            system_dir,
            sizeof(retro_system_directory));
   }

   const char *content_dir = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_CONTENT_DIRECTORY, &content_dir) && content_dir)
   {
      // if defined, use the system directory
      strlcpy(
            retro_content_directory,
            content_dir,
            sizeof(retro_content_directory));
   }

   const char *save_dir = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY, &save_dir) && save_dir)
   {
      // If save directory is defined use it, otherwise use system directory
      strlcpy(
            retro_save_directory,
            string_is_empty(save_dir) ? retro_system_directory : save_dir,
            sizeof(retro_save_directory));
   }
   else
   {
      // make retro_save_directory the same in case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY is not implemented by the frontend
      strlcpy(
            retro_save_directory,
            retro_system_directory,
            sizeof(retro_save_directory));
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
   int dir_mode = 0;
#if defined(IOS)
   dir_mode = 0755;
#elif defined(VITA) || defined(PSP)
   dir_mode = 0777;
#elif defined(PS2)
   dir_mode = 0777;
#elif defined(ORBIS)
   dir_mode = 0755;
#elif defined(__QNX__)
   dir_mode = 0777;
#else
   dir_mode = 0750;
#endif
   archdep_mkdir(retro_system_data_directory, dir_mode);

   enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_RGB565;
   if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
   {
      log_cb(RETRO_LOG_ERROR, "PIXEL FORMAT RGB565 is not supported.\n");
      environ_cb(RETRO_ENVIRONMENT_SHUTDOWN, NULL);
      return;
   }

   memset(retro_bmp, 0, sizeof(retro_bmp));
   memset(core_key_state, 0, 512);
   memset(core_old_key_state, 0, sizeof(core_old_key_state));

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
   
   unsigned dci_version = 0;
   if (environ_cb(RETRO_ENVIRONMENT_GET_DISK_CONTROL_INTERFACE_VERSION, &dci_version) && (dci_version >= 1))
      environ_cb(RETRO_ENVIRONMENT_SET_DISK_CONTROL_EXT_INTERFACE, &diskControlExt);
   else
      environ_cb(RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE, &diskControl);

   static uint32_t quirks =  RETRO_SERIALIZATION_QUIRK_MUST_INITIALIZE | RETRO_SERIALIZATION_QUIRK_CORE_VARIABLE_SIZE;
   environ_cb(RETRO_ENVIRONMENT_SET_SERIALIZATION_QUIRKS, &quirks);

   microSecCounter = 0;
}

void retro_deinit(void)
{
   // Clean the disk control context
   if (dc)
      dc_free(dc);

   // Clean legacy strings
   if (core_options_legacy_strings)
      free(core_options_legacy_strings);

   // Clean ZIP temp
   if (retro_temp_directory && path_is_directory(retro_temp_directory))
      remove_recurse(retro_temp_directory);
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
   info->valid_extensions = "20|40|60|a0|b0|d64|d71|d80|d81|d82|g64|g41|x64|t64|tap|prg|p00|crt|bin|zip|gz|d6z|d7z|d8z|g6z|g4z|x6z|cmd|m3u|vfl|vsf";
#else
   info->valid_extensions = "d64|d71|d80|d81|d82|g64|g41|x64|t64|tap|prg|p00|crt|bin|zip|gz|d6z|d7z|d8z|g6z|g4z|x6z|cmd|m3u|vfl|vsf";
#endif
   info->need_fullpath    = true;
   info->block_extract    = true;
}

double retro_get_aspect_ratio(unsigned int width, unsigned int height)
{
   static double ar;
   static unsigned int region = 0;
   region = retro_region;
   switch (opt_aspect_ratio)
   {
      case 1:
         region = RETRO_REGION_PAL;
         break;
      case 2:
         region = RETRO_REGION_NTSC;
         break;
   }

   #if defined(__X64__) || defined(__X64SC__) || defined(__X128__)
      if (region == RETRO_REGION_NTSC)
         ar = ((float)width / (float)height) * (float)0.75000000;
      else
         ar = ((float)width / (float)height) * (float)0.93650794;
   #if defined(__X128__)
      if (RETROC128COLUMNKEY == 0)
         ar = ((float)width / (float)height) / 2;
   #endif
   #elif defined(__VIC20__)
      if (region == RETRO_REGION_NTSC)
         ar = ((float)width / (float)height) * ((float)1.50411479 / (float)2.0);
      else
         ar = ((float)width / (float)height) * ((float)1.66574035 / (float)2.0);
   #elif defined(__PLUS4__)
      if (region == RETRO_REGION_NTSC)
         ar = ((float)width / (float)height) * (float)0.85760931;
      else
         ar = ((float)width / (float)height) * (float)1.03743478;
   #else
      ar = (float)4 / (float)3;
   #endif
   return (opt_aspect_ratio == 3) ? (float)((float)width / (float)height) : ar;
}

void update_geometry(int mode)
{
   struct retro_system_av_info system_av_info;

#ifdef RETRO_DEBUG
   log_cb(RETRO_LOG_INFO, "Update Geometry: Old(%d,%d) New(%d,%d)\n", lastW, lastH, retroW, retroH);
#endif
   lastW = retroW;
   lastH = retroH;

   /* ### Bruno Display format/geometry resetting */

   switch (mode)
   {
      case 0:
         /* Zoom mode init */
         zoom_mode_id_prev = 0;
         bcrop_horiz_mode_prev = 0;
         
         zoomed_width = retroW;
         zoomed_height = retroH;
         zoomed_XS_offset = 0;
         zoomed_YS_offset = 0;
         retroXS_offset = 0;
         retroYS_offset = 0;

         system_av_info.geometry.base_width = retroW;
         system_av_info.geometry.base_height = retroH;
         system_av_info.geometry.aspect_ratio = retro_get_aspect_ratio(retroW, retroH);

         /* Update av_info only when PAL/NTSC change occurs */
         if (retro_region != retro_get_region())
         {
            retro_region = retro_get_region();
            retro_get_system_av_info(&system_av_info);
            environ_cb(RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO, &system_av_info);
            return;
         }
#if 0
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
            system_av_info.geometry.aspect_ratio = retro_get_aspect_ratio(retroW, retroH);
#endif
         break;

   /* ### Bruno CROPPING BUSINESS */
   /* ### Bruno This is the Zoom/vertical crop */

      case 1:
#if defined(__X64__) || defined(__X64SC__) || defined(__X128__) || defined(__VIC20__) || defined(__PLUS4__)

		 // Crop Vertical (aka Zoom Mode) - Don't touch any Width variables
         if (zoom_mode_id != zoom_mode_id_prev) 
         {
            zoom_mode_id_prev = zoom_mode_id;
            crop_top_px_prev = crop_top_px;
            crop_bottom_px_prev = crop_bottom_px;
            
            switch (zoom_mode_id)
            {
            #if defined(__X64__) || defined(__X64SC__) || defined(__X128__)

               // NTSC: 384 x (247), PAL: 384 x (272)
               case 1: // Small Vert Border Crop
                  zoomed_height       = (retro_region == RETRO_REGION_NTSC) ? 230 : 240;
                  zoomed_YS_offset    = (retro_region == RETRO_REGION_NTSC) ? 8 : 15;		// PAL offset was off by 1
                  break;

               case 2: // Medium Vert Border Crop
                  zoomed_height       = (retro_region == RETRO_REGION_NTSC) ? 216 : 218;	// NTSC/PAL values previously incorrectly swapped
                  zoomed_YS_offset    = (retro_region == RETRO_REGION_NTSC) ? 15 : 26;		// PAL offset was off by 1
                  break;

               case 3: // Full Vert Border Crop
                  zoomed_height       = 200;
                  zoomed_YS_offset    = (retro_region == RETRO_REGION_NTSC) ? 23 : 35;
                  break;

               case 4: // Manual Vert border Crop - NTSC Borders 23 & 24 px, PAL borders 35 & 37 px
						
				  clean_top_crop = crop_top_px;
				  clean_bottom_crop = crop_bottom_px;

					// Cap the maximum crop for NTSC so you can't crop more of the visible screen than PAL
					// Max value for NTSC = NTSC border size + diff of highest menu value (60) - PAL border size(s)

					// Max top
					if (retro_region == RETRO_REGION_NTSC && clean_top_crop > 23 + 25)  // PAL diff = (60-35) = 25
					{
						clean_top_crop = 23 + 25;
					}

					// Max bottom
					if (retro_region == RETRO_REGION_NTSC && clean_bottom_crop > 24 + 23)  // PAL diff = (60-37) = 23
					{
						clean_bottom_crop = 24 + 23;
					}

				  // grab base display height for current model
				  zoomed_height = (retro_region == RETRO_REGION_NTSC) ? 247 : 272;
				  
				  // calculate new display height after cropping
				  zoomed_height = zoomed_height - clean_top_crop - clean_bottom_crop;

				  // set crop origin (offset) by top crop value
                  zoomed_YS_offset    = clean_top_crop;

                  break;
                  
            #elif defined(__VIC20__)
               // PAL: 448x284, NTSC: 400x234
               case 1:
                  zoomed_width        = retroW;
                  zoomed_XS_offset    = (retro_region == RETRO_REGION_NTSC) ? 8 : 0;
                  zoomed_height       = (retro_region == RETRO_REGION_NTSC) ? 218 : 236;
                  zoomed_YS_offset    = (retro_region == RETRO_REGION_NTSC) ? 6 : 20;
                  break;
               case 2:
                  zoomed_width        = retroW;
                  zoomed_XS_offset    = (retro_region == RETRO_REGION_NTSC) ? 8 : 0;
                  zoomed_height       = (retro_region == RETRO_REGION_NTSC) ? 202 : 216;
                  zoomed_YS_offset    = (retro_region == RETRO_REGION_NTSC) ? 13 : 32;
                  break;
               case 3:
                  zoomed_width        = (retro_region == RETRO_REGION_NTSC) ? retroW : 392;
                  zoomed_XS_offset    = (retro_region == RETRO_REGION_NTSC) ? 8 : 28;
                  zoomed_height       = 184;
                  zoomed_YS_offset    = (retro_region == RETRO_REGION_NTSC) ? 22 : 48;
                  break;
            #elif defined(__PLUS4__)
               // PAL: 384x288, NTSC: 384x242
               case 1:
                  zoomed_width        = retroW;
                  zoomed_XS_offset    = 0;
                  zoomed_height       = (retro_region == RETRO_REGION_NTSC) ? 228 : 246;
                  zoomed_YS_offset    = (retro_region == RETRO_REGION_NTSC) ? 4 : 18;
                  break;
               case 2:
                  zoomed_width        = (retro_region == RETRO_REGION_NTSC) ? retroW : 377;
                  zoomed_XS_offset    = (retro_region == RETRO_REGION_NTSC) ? 0 : 4;
                  zoomed_height       = (retro_region == RETRO_REGION_NTSC) ? 212 : 220;
                  zoomed_YS_offset    = (retro_region == RETRO_REGION_NTSC) ? 12 : 30;
                  break;
               case 3:
                  zoomed_width        = (retro_region == RETRO_REGION_NTSC) ? retroW : 343;
                  zoomed_XS_offset    = (retro_region == RETRO_REGION_NTSC) ? 0 : 21;
                  zoomed_height       = 200;
                  zoomed_YS_offset    = (retro_region == RETRO_REGION_NTSC) ? 18 : 40;
                  break;
            #endif

               default:								// Only adjust Vertical values
                  zoomed_height       = retroH;
                  zoomed_YS_offset    = 0;
                  retroYS_offset      = 0;
                  break;
            }

            system_av_info.geometry.base_width = zoomed_width;
            system_av_info.geometry.base_height = zoomed_height;
            system_av_info.geometry.aspect_ratio = retro_get_aspect_ratio(zoomed_width, zoomed_height);

         } // end Crop Borders Vertical (aka Zoom Mode)


		 // Crop Horizontal - Don't touch any Height variables
         if (bcrop_horiz_mode != bcrop_horiz_mode_prev)
         {
            bcrop_horiz_mode_prev = bcrop_horiz_mode;

            switch (bcrop_horiz_mode)
            {
            #if defined(__X64__) || defined(__X64SC__) || defined(__X128__)

               // NTSC: (384) x 247, PAL: (384) x 272
               case 1: // Small Horiz Border Crop
                  zoomed_width        = 362;		// 22 pixels narrower than normal
                  zoomed_XS_offset    = 11;
                  break;

               case 2: // Medium Horiz Border Crop
                  zoomed_width        = 340;		// 44 pixels narrower than normal
                  zoomed_XS_offset    = 22;
                  break;

               case 3: // Full Horiz Border Crop
                  zoomed_width        = 320;		// 64 pixels narrower than normal
                  zoomed_XS_offset    = 32;
                  break;

            #elif defined(__VIC20__)
               // PAL: 448x284, NTSC: 400x234
               case 1:
                  zoomed_width        = retroW;
                  zoomed_XS_offset    = (retro_region == RETRO_REGION_NTSC) ? 8 : 0;
                  zoomed_height       = (retro_region == RETRO_REGION_NTSC) ? 218 : 236;
                  zoomed_YS_offset    = (retro_region == RETRO_REGION_NTSC) ? 6 : 20;
                  break;
               case 2:
                  zoomed_width        = retroW;
                  zoomed_XS_offset    = (retro_region == RETRO_REGION_NTSC) ? 8 : 0;
                  zoomed_height       = (retro_region == RETRO_REGION_NTSC) ? 202 : 216;
                  zoomed_YS_offset    = (retro_region == RETRO_REGION_NTSC) ? 13 : 32;
                  break;
               case 3:
                  zoomed_width        = (retro_region == RETRO_REGION_NTSC) ? retroW : 392;
                  zoomed_XS_offset    = (retro_region == RETRO_REGION_NTSC) ? 8 : 28;
                  zoomed_height       = 184;
                  zoomed_YS_offset    = (retro_region == RETRO_REGION_NTSC) ? 22 : 48;
                  break;
            #elif defined(__PLUS4__)
               // PAL: 384x288, NTSC: 384x242
               case 1:
                  zoomed_width        = retroW;
                  zoomed_XS_offset    = 0;
                  zoomed_height       = (retro_region == RETRO_REGION_NTSC) ? 228 : 246;
                  zoomed_YS_offset    = (retro_region == RETRO_REGION_NTSC) ? 4 : 18;
                  break;
               case 2:
                  zoomed_width        = (retro_region == RETRO_REGION_NTSC) ? retroW : 377;
                  zoomed_XS_offset    = (retro_region == RETRO_REGION_NTSC) ? 0 : 4;
                  zoomed_height       = (retro_region == RETRO_REGION_NTSC) ? 212 : 220;
                  zoomed_YS_offset    = (retro_region == RETRO_REGION_NTSC) ? 12 : 30;
                  break;
               case 3:
                  zoomed_width        = (retro_region == RETRO_REGION_NTSC) ? retroW : 343;
                  zoomed_XS_offset    = (retro_region == RETRO_REGION_NTSC) ? 0 : 21;
                  zoomed_height       = 200;
                  zoomed_YS_offset    = (retro_region == RETRO_REGION_NTSC) ? 18 : 40;
                  break;
            #endif
            
               default:								// Only adjust Horizontal values
                  zoomed_width        = retroW;
                  zoomed_XS_offset    = 0;
                  retroXS_offset      = 0;
                  break;
            }

            system_av_info.geometry.base_width = zoomed_width;
            system_av_info.geometry.base_height = zoomed_height;
            system_av_info.geometry.aspect_ratio = retro_get_aspect_ratio(zoomed_width, zoomed_height);

         } // end Crop Borders Horizontal


#endif
         break;
   }  // End Switch (mode)
   
   environ_cb(RETRO_ENVIRONMENT_SET_GEOMETRY, &system_av_info);
}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
   /* need to do this here because core option values are not available in retro_init */
   if (pix_bytes == 4)
   {
      enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_XRGB8888;
      if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
      {
         log_cb(RETRO_LOG_INFO, "XRGB8888 is not supported. Trying RGB565.\n");
         fmt = RETRO_PIXEL_FORMAT_RGB565;
         pix_bytes = 2;
         if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
         {
            log_cb(RETRO_LOG_INFO, "RGB565 is not supported.\n");
            exit(0);//return false;
         }
      }
   }

   info->geometry.max_width = defaultW;
   info->geometry.max_height = defaultH;
   info->geometry.base_width = retroW;
   info->geometry.base_height = retroH;
   info->geometry.aspect_ratio = retro_get_aspect_ratio(retroW, retroH);
   info->timing.sample_rate = RETROSOUNDSAMPLERATE;
   prev_audio_sample_rate = RETROSOUNDSAMPLERATE;

   // Remember region for av_info update
   retro_region = retro_get_region();

   switch (retro_region)
   {
      case RETRO_REGION_PAL:
         info->timing.fps = C64_PAL_RFSH_PER_SEC;
         break;
      
      case RETRO_REGION_NTSC:
         info->timing.fps = C64_NTSC_RFSH_PER_SEC;
         break;
   }
}

void retro_set_video_refresh(retro_video_refresh_t cb)
{
   video_cb = cb;
}

void retro_set_audio_sample(retro_audio_sample_t cb)
{
   audio_cb = cb;
}

void retro_audio_cb(short l, short r)
{
   audio_cb(l, r);
}

void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb)
{
   audio_batch_cb = cb;
}

void retro_audio_batch_cb(const int16_t *data, size_t frames)
{
   audio_batch_cb(data, frames);
}

void retro_audio_render(signed short int *sound_buffer, int sndbufsize)
{
   int x;
#if 1
   for (x=0; x<sndbufsize; x++) audio_cb(sound_buffer[x], sound_buffer[x]);
#else
   //FIXME audio_batch_cb(sound_buffer, sndbufsize);
#endif
}



void retro_run(void)
{
   bool updated = false;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated)
      update_variables();

   if (retro_ui_finalized)
   {
      /* Update samplerate if changed by core option */
      if (prev_audio_sample_rate != RETROSOUNDSAMPLERATE)
      {
         prev_audio_sample_rate = RETROSOUNDSAMPLERATE;

         /* Ensure audio rendering is reinitialized on next use. */
         sound_close();

   /* ### Bruno RESET */

         /* Reset zoom for proper aspect ratio after av_info change */
         zoom_mode_id_prev = -1;

         struct retro_system_av_info system_av_info;
         retro_get_system_av_info(&system_av_info);
         environ_cb(RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO, &system_av_info);
      }

   /* ### Bruno UPDATE the geometry */

      /* Update geometry if model or zoom mode changes */
      if ((lastW == retroW && lastH == retroH) && (zoom_mode_id != zoom_mode_id_prev || bcrop_horiz_mode != bcrop_horiz_mode_prev) ) // Zoom/Crop Changed
         update_geometry(1);
      else if (lastW != retroW || lastH != retroH || retro_region != retro_get_region())
         update_geometry(0);
   }

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
      runstate = RUNSTATE_RUNNING;
      pre_main();
      reload_restart();
      return;
   } 
   else if (runstate == RUNSTATE_LOADED_CONTENT)
   {
      /* Load content was called while core was already running, just do a reset with autostart */
      reload_restart();
      /* After retro_load_game, get_system_av_info is always called by the frontend */
      /* resetting the aspect to 4/3 etc. So we inform the frontend of the actual */
      /* current aspect ratio and screen size again here */
      update_geometry(0);
      runstate = RUNSTATE_RUNNING;
   } 

   /* Input poll */
   retro_poll_event();

   /* Measure frame-time and time between frames to render as much frames as possible when warp is enabled. Does not work
      perfectly as the time needed by the framework cannot be accounted, but should not reduce amount of actually rendered
      frames too much. */
   {
      static struct retro_perf_callback pcb;
      if (!pcb.get_time_usec)
         environ_cb(RETRO_ENVIRONMENT_GET_PERF_INTERFACE, &pcb);

      static retro_time_t t_frame=1, t_end_prev=0;

      retro_time_t t_begin=pcb.get_time_usec();
      retro_time_t t_interframe=MIN((t_end_prev ? t_begin-t_end_prev : 0), 20000-t_frame);

      for (int frame_count=0;frame_count<(retro_warp_mode_enabled() ? (t_interframe+t_frame)/t_frame : 1);++frame_count)
      {
         while(cpuloop==1)
            maincpu_mainloop_retro();
         cpuloop=1;

         if (!frame_count)
         {
            retro_time_t t_end=pcb.get_time_usec();
            t_end_prev=t_end;
            if (!(t_frame=MIN(t_end-t_begin, 20000)))
               /* It was seen with x64 that mainloop actually returned within one nanosecond, so make sure
               we don't end up with 0 here. */
               t_frame=20000;
         }
      }
   }

   /* Show VKBD */
   if (SHOWKEY==1)
      print_virtual_kbd(retro_bmp);

   /* Finalize zoom offsets */
   if (zoomed_XS_offset != retroXS_offset || zoomed_YS_offset != retroYS_offset)
   {
      retroXS_offset = zoomed_XS_offset;
      retroYS_offset = zoomed_YS_offset;
   }

   /* Set volume back to maximum after starting with mute, due to ReSID 6581 init pop */
   if (sound_volume_counter > 0)
   {
      sound_volume_counter--;
      if (sound_volume_counter == 0)
         log_resources_set_int("SoundVolume", 100);
   }

   /* Statusbar disk display timer */
   if (imagename_timer > 0)
      imagename_timer--;

   video_cb(retro_bmp+(retroXS_offset*pix_bytes/2)+(retroYS_offset*(retroW<<(pix_bytes/4))), zoomed_width, zoomed_height, retroW<<(pix_bytes/2));
   microSecCounter += (1000000/(retro_get_region() == RETRO_REGION_NTSC ? C64_NTSC_RFSH_PER_SEC : C64_PAL_RFSH_PER_SEC));
}

bool retro_load_game(const struct retro_game_info *info)
{
   if (info)
   {
      process_cmdline(info->path);
   }

   update_variables();

#if defined(__VIC20__)
   /* Moved this here so it also applies without loading content */
   cur_port = 1;
   cur_port_locked = 1;
#endif
   
   if (runstate == RUNSTATE_RUNNING) {
      /* load game was called while core is already running */
      /* so we update runstate and do the deferred autostart_reset in retro_run */
      /* the autostart_reset has to be deferred because a bunch of other init stuff */
      /* is done between retro_load_game() and retro_run() at a higher level */
      runstate = RUNSTATE_LOADED_CONTENT;
   }

   return true;
}

void retro_unload_game(void)
{
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
   switch (RETROC64MODL)
   {
#if defined(__VIC20__)
      case VIC20MODEL_VIC20_NTSC:
      case VIC20MODEL_VIC21:
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
   static unsigned int save_disks;
   static unsigned int drive_type;
   resources_get_int("Drive8Type", &drive_type);
   save_disks = (drive_type < 1550) ? 1 : 0;

   /* params: stream, save_roms, save_disks, event_mode */
   if (machine_write_snapshot_to_stream(snapshot_stream, 0, save_disks, 0) >= 0)
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
      resources_set_int("WarpMode", 0);
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

