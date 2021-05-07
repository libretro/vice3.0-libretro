#include "libretro.h"
#include "libretro-core.h"
#include "libretro-mapper.h"
#include "libretro-graph.h"
#include "encodings/utf.h"

#include "archdep.h"
#include "mem.h"
#include "machine.h"
#include "maincpu.h"
#include "snapshot.h"
#include "autostart.h"
#include "drive.h"
#include "tape.h"
#include "diskimage.h"
#include "vdrive.h"
#include "vdrive-internal.h"
#include "charset.h"
#include "attach.h"
#include "interrupt.h"
#include "datasette.h"
#include "cartridge.h"
#include "initcmdline.h"
#include "vsync.h"
#include "log.h"
#include "keyboard.h"
#include "kbdbuf.h"
#include "resources.h"
#include "sid.h"
#include "sid-resources.h"
#if !defined(__XCBM5x0__)
#include "userport_joystick.h"
#endif

/* Main CPU loop */
long retro_now = 0;
unsigned retro_renderloop = 1;

/* VKBD */
extern bool retro_vkbd;
extern void print_vkbd(void);
unsigned int opt_vkbd_theme = 0;
libretro_graph_alpha_t opt_vkbd_alpha = GRAPH_ALPHA_75;

/* Core vars */
static bool autosys = true;
static bool noautostart = false;
static char* autostartString = NULL;
static char* autostartProgram = NULL;
char full_path[RETRO_PATH_MAX] = {0};
static char* core_options_legacy_strings = NULL;
static char* vice_carts_info = NULL;
static struct vice_cart_info vice_carts[RETRO_NUM_CORE_OPTION_VALUES_MAX] = {0};

static snapshot_stream_t* snapshot_stream = NULL;
static int load_trap_happened = 0;
static int save_trap_happened = 0;

unsigned int retro_devices[RETRO_DEVICES] = {0};
unsigned int opt_video_options_display = 0;
unsigned int opt_audio_options_display = 0;
unsigned int opt_mapping_options_display = 1;
unsigned int retro_region = 0;
static double retro_refresh = 0;
static unsigned int prev_sound_sample_rate = 0;

bool retro_ui_finalized = false;
#ifdef RETRO_DEBUG
bool prev_ui_finalized = false;
#endif

extern uint8_t mem_ram[];
int mem_ram_size;

/* Core geometry */
unsigned int retroXS = 0;
unsigned int retroYS = 0;
unsigned int retroXS_offset = 0;
unsigned int retroYS_offset = 0;
unsigned int defaultw = WINDOW_WIDTH;
unsigned int defaulth = WINDOW_HEIGHT;
unsigned int retrow = WINDOW_WIDTH;
unsigned int retroh = WINDOW_HEIGHT;
unsigned int lastw = 0;
unsigned int lasth = 0;

unsigned short int pix_bytes = 2;
static bool pix_bytes_initialized = false;
unsigned short int retro_bmp[RETRO_BMP_SIZE] = {0};

/* Core options */
struct vice_core_options vice_opt;

#if defined(__XVIC__)
int vic20mem_forced = -1;
#endif

unsigned int zoom_mode_id = 0;
int zoom_mode_id_prev = -1;
unsigned int opt_zoom_mode_id = 0;
unsigned int zoom_mode_crop_id = 0;
unsigned int zoomed_width = 0;
unsigned int zoomed_height = 0;
unsigned int zoomed_XS_offset = 0;
unsigned int zoomed_YS_offset = 0;
unsigned int opt_aspect_ratio = 0;
bool opt_aspect_ratio_locked = false;
static unsigned int manual_crop_top = 0;
static unsigned int manual_crop_bottom = 0;
static unsigned int manual_crop_left = 0;
static unsigned int manual_crop_right = 0;

static bool request_reload_restart = false;
static bool request_restart = false;
static bool request_update_work_disk = false;
int request_model_set = -1;
static int request_model_prev = -1;
static unsigned int opt_model_auto = 1;
unsigned int opt_autostart = 1;
unsigned int opt_autoloadwarp = 0;
unsigned int opt_read_vicerc = 0;
static unsigned int opt_work_disk_type = 0;
static unsigned int opt_work_disk_unit = 8;
#if defined(__X64__) || defined(__X64SC__) || defined(__X128__) || defined(__XSCPU64__)
static unsigned int opt_jiffydos_allow = 1;
unsigned int opt_jiffydos = 0;
#endif
#if defined(__XSCPU64__)
unsigned int opt_supercpu_kernal = 0;
#endif
static unsigned int sound_volume_counter = 3;
unsigned int opt_audio_leak_volume = 0;
int opt_datasette_sound_volume = 0;
unsigned int opt_statusbar = 0;
unsigned int opt_reset_type = 0;
bool opt_keyrah_keypad = false;
bool opt_keyboard_pass_through = false;
unsigned int opt_keyboard_keymap = KBD_INDEX_POS;
unsigned int opt_retropad_options = 0;
unsigned int opt_joyport_type = 0;
int opt_joyport_pointer_color = -1;
unsigned int opt_dpadmouse_speed = 6;
unsigned int opt_mouse_speed = 100;
unsigned int opt_analogmouse = 0;
unsigned int opt_analogmouse_deadzone = 20;
float opt_analogmouse_speed = 1.0;

extern bool datasette_hotkeys;
extern unsigned int cur_port;
extern bool cur_port_locked;

extern bool retro_turbo_fire;
extern bool turbo_fire_locked;
extern unsigned int turbo_fire_button;
extern unsigned int turbo_pulse;

#if defined(__XVIC__)
void cartridge_trigger_freeze(void) {}
#elif defined(__XCBM2__) || defined(__XCBM5x0__)
void cartridge_trigger_freeze(void) {}
const char *cartridge_get_file_name(int type) { return NULL; }
#elif defined(__XPET__)
void cartridge_trigger_freeze(void) {}
const char *cartridge_get_file_name(int type) { return NULL; }
#elif defined(__XPLUS4__)
void cartridge_trigger_freeze(void) {}
const char *cartridge_get_file_name(int type) { return NULL; }
#else
extern int cart_getid_slotmain(void);
#endif

retro_input_state_t input_state_cb = NULL;
retro_input_poll_t input_poll_cb = NULL;
retro_log_printf_t log_cb = NULL;
static retro_set_led_state_t led_state_cb = NULL;
static retro_video_refresh_t video_cb = NULL;
static retro_audio_sample_t audio_cb = NULL;
static retro_audio_sample_batch_t audio_batch_cb = NULL;
static retro_environment_t environ_cb = NULL;

static struct retro_perf_callback perf_cb;

bool libretro_supports_bitmasks = false;
static unsigned int retro_led_state[3] = {0};
unsigned int vice_led_state[3] = {0};

char retro_save_directory[RETRO_PATH_MAX] = {0};
char retro_temp_directory[RETRO_PATH_MAX] = {0};
char retro_system_directory[RETRO_PATH_MAX] = {0};
char retro_system_data_directory[RETRO_PATH_MAX] = {0};
static char retro_content_directory[RETRO_PATH_MAX] = {0};

/* Disk Control context */
dc_storage* dc = NULL;
char dc_savestate_filename[RETRO_PATH_MAX] = {0};

int runstate = RUNSTATE_FIRST_START; /* used to detect whether we are just starting the core from scratch */
/* runstate = RUNSTATE_FIRST_START: first time retro_run() is called after loading and starting core */
/* runstate = RUNSTATE_LOADED_CONTENT: load content was selected while core is running, so do an autostart_reset() */
/* runstate = RUNSTATE_RUNNING: core is running normally */

/* Display disk name and label instead of "Changing disk in tray"- maybe make it configurable */
bool display_disk_name = false;
/* See which looks best in most cases and tweak (or make configurable) */
int disk_label_mode = DISK_LABEL_MODE_ASCII_OR_CAMELCASE;

static char* x_strdup(const char* str)
{
   return str ? strdup(str) : NULL;
}

static char CMDFILE[512] = {0};
int loadcmdfile(const char *argv)
{
   int res = 0;
   FILE *fp = fopen(argv, "r");

   CMDFILE[0] = '\0';
   if (fp != NULL)
   {
      if (fgets(CMDFILE, 512, fp) != NULL)
      {
         res = 1;
         snprintf(CMDFILE, sizeof(CMDFILE), "%s", trimwhitespace(CMDFILE));
      }
      fclose (fp);
   }
   return res;
}

/* Args for experimental_cmdline */
static char ARGUV[64][1024];
static unsigned char ARGUC = 0;

/* Args for Core */
static char XARGV[64][1024];
static const char* xargv_cmd[64];
int PARAMCOUNT = 0;

/* Display message on next retro_run */
bool retro_message = false;
char retro_message_msg[1024] = {0};
extern void display_current_image(const char *image, bool inserted);

extern int skel_main(int argc, char *argv[]);

static void Add_Option(const char* option)
{
   sprintf(XARGV[PARAMCOUNT++],"%s",option);
}

static void parse_cmdline(const char *argv)
{
   char *p, *p2, *start_of_word;
   int c, c2;
   static char buffer[512*4];
   enum states { DULL, IN_WORD, IN_STRING } state = DULL;

   ARGUC = 0;

   strcpy(buffer, argv);
   strcat(buffer, " \0");

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
               /* ... do something with the word ... */
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
               /* ... do something with the word ... */
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
            port = 1;
        else if (strcasestr(filename, "_j2.") || strcasestr(filename, "(j2)."))
            port = 2;
    }
    return port;
}

static int retro_disk_get_image_unit()
{
    int unit = dc->unit;
    if (dc->index < dc->count)
    {
        if (dc_get_image_type(dc->files[dc->index]) == DC_IMAGE_TYPE_TAPE)
            dc->unit = 1;
        else if (dc_get_image_type(dc->files[dc->index]) == DC_IMAGE_TYPE_FLOPPY)
            dc->unit = 8;
        else if (dc_get_image_type(dc->files[dc->index]) == DC_IMAGE_TYPE_MEM)
            dc->unit = 0;
        else
            dc->unit = 8;
    }
    else
        unit = 8;

    return unit;
}

/* ReSID 6581 init pop mute shenanigans */
void sound_volume_counter_reset()
{
    resources_set_int("SoundVolume", 0);
    sound_volume_counter = 3;
}

#if defined(__XVIC__)
static int vic20_cart_is_multipart(int prev_type, const char* argv)
{
    int mp_type = prev_type;

    if (strcasestr(argv, "-2000.") || strcasestr(argv, "-4000.")
     || strcasestr(argv, "-6000.") || strcasestr(argv, "-a000."))
        mp_type = -1;
    else if (strcasestr(argv, "[2000]") || strcasestr(argv, "[4000]")
          || strcasestr(argv, "[6000]") || strcasestr(argv, "[A000]"))
        mp_type = -2;
    else if (strcasestr(argv, "$2000") || strcasestr(argv, "$4000")
          || strcasestr(argv, "$6000") || strcasestr(argv, "$A000"))
        mp_type = -3;

    return mp_type;
}

static int vic20_autodetect_cartridge_type(const char* argv)
{
    FILE *fd;
    int addr = 0, len = 0, type = 0;
    char buf[RETRO_PATH_MAX] = {0};

    fd = fopen(argv, MODE_READ);
    fseek(fd, 0, SEEK_END);
    len = ftell(fd);
    fseek(fd, 0, SEEK_SET);
    switch (len & 0xfff)
    {
        case 0: /* plain binary */
            addr = 0;
            type = CARTRIDGE_VIC20_GENERIC;
            break;
        case 2: /* load address */
            addr = fgetc(fd);
            addr = (addr & 0xff) | ((fgetc(fd) << 8) & 0xff00);
            len -= 2; /* remove load address from length */
            type = CARTRIDGE_VIC20_GENERIC;
            break;
        default: /* not a valid file */
            /* M3U analyzing */
            if (strcasestr(argv, ".m3u"))
            {
                fseek(fd, 0, SEEK_SET);
                if (fgets(buf, sizeof(buf), fd) != NULL)
                {
                    buf[strcspn(buf, "\r\n")] = 0;
                    argv = buf;
                }
            }
            break;
    }
    fclose(fd);

    if (type > 0)
    {
        if (len == 0)
            type = -1;
        else if (len == 0x200000 && addr == 0)
            type = CARTRIDGE_VIC20_MEGACART;
        else if ((addr == 0x6000 || addr == 0x7000) && (len <= 0x4000))
            type = CARTRIDGE_VIC20_16KB_6000;
        else if ((addr == 0x4000 || addr == 0x5000) && (len <= 0x4000))
            type = CARTRIDGE_VIC20_16KB_4000;
        else if ((addr == 0x2000 || addr == 0x3000) && (len <= 0x4000))
            type = CARTRIDGE_VIC20_16KB_2000;
        else if ((addr == 0xA000) && (len <= 0x2000))
            type = CARTRIDGE_VIC20_8KB_A000;
        else if ((addr == 0xB000) && (len <= 0x1000))
            type = CARTRIDGE_VIC20_4KB_B000;
        else if (len <= 0x2000)
            type = CARTRIDGE_VIC20_8KB_A000;
    }

    if (strcasestr(argv, ".20"))
        type = CARTRIDGE_VIC20_16KB_2000;
    else if (strcasestr(argv, ".40"))
        type = CARTRIDGE_VIC20_16KB_4000;
    else if (strcasestr(argv, ".60"))
        type = CARTRIDGE_VIC20_16KB_6000;
    else if (strcasestr(argv, ".70"))
        type = CARTRIDGE_VIC20_4KB_6000;
    else if (strcasestr(argv, ".a0"))
        type = CARTRIDGE_VIC20_8KB_A000;
    else if (strcasestr(argv, ".b0"))
        type = CARTRIDGE_VIC20_4KB_B000;

    /* Multipart ROM combinations (type < 0) */
    type = vic20_cart_is_multipart(type, argv);

    return type;
}

static void vic20_mem_force(const char* argv)
{
    char buf[6]       = {0};
    int vic20mem      = 0;
    int vic20mems[6]  = {0, 3, 8, 16, 24, 35};

    for (int i = 0; i < sizeof(vic20mems)/sizeof(vic20mems[0]); i++)
    {
        vic20mem = vic20mems[i];

        snprintf(buf, sizeof(buf), "%c%d%c", '(', vic20mem, 'k');
        if (strcasestr(argv, buf))
        {
            vic20mem_forced = i;
            log_cb(RETRO_LOG_INFO, "VIC-20 memory expansion force found in filename '%s': %dkB\n", argv, vic20mem);
            break;
        }

        snprintf(buf, sizeof(buf), "%c%d%c", '[', vic20mem, 'k');
        if (strcasestr(argv, buf))
        {
            vic20mem_forced = i;
            log_cb(RETRO_LOG_INFO, "VIC-20 memory expansion force found in filename '%s': %dkB\n", argv, vic20mem);
            break;
        }

        snprintf(buf, sizeof(buf), "%c%d%c", FSDEV_DIR_SEP_CHR, vic20mem, 'k');
        if (strcasestr(argv, buf))
        {
            vic20mem_forced = i;
            log_cb(RETRO_LOG_INFO, "VIC-20 memory expansion force found in path '%s': %dkB\n", argv, vic20mem);
            break;
        }
    }
}

static void vic20_autosys_run(const char* full_path)
{
    if (!strcasestr(full_path, "(SYS ") && !strcasestr(full_path, "[SYS "))
        return;

    char command[20] = {0};
    char tmp_path[RETRO_PATH_MAX] = {0};
    snprintf(tmp_path, sizeof(tmp_path), "%s", path_basename(full_path));
    char *token = strtok((char*)tmp_path, " ");
    char *token_prev = token;
    while (token != NULL)
    {
        token = strtok(NULL, " ");
        if (strendswith(token_prev, "SYS"))
        {
            snprintf(command, sizeof(command), "%s", token);
            token = NULL;
        }
        token_prev = token;
    }

    if (!string_is_empty(command))
    {
        if (strcasestr(full_path, "(SYS"))
            token = strtok((char*)command, ")");
        else if (strcasestr(full_path, "[SYS"))
            token = strtok((char*)command, "]");

        log_cb(RETRO_LOG_INFO, "Executing 'SYS %s'\n", command);
        kbdbuf_feed("SYS ");
        kbdbuf_feed(command);
        kbdbuf_feed("\r");
    }

    token = NULL;
    token_prev = NULL;
}

#endif

static int process_cmdline(const char* argv)
{
    int i = 0;
    bool is_fliplist = false;
    int joystick_control = 0;

#if defined(__XPLUS4__)
    /* Do not reset noautostart if already set, PLUS/4 has issues with starting carts via autostart (?!) */
    noautostart = (noautostart) ? noautostart : !opt_autostart;
#else
    noautostart = !opt_autostart;
#endif
    PARAMCOUNT = 0;
    dc_reset(dc);
    snprintf(full_path, sizeof(full_path), "%s", argv);

    cur_port_locked = false;
    free(autostartString);
    autostartString = NULL;
    free(autostartProgram);
    autostartProgram = NULL;

    /* Load command line arguments from cmd file */
    if (strendswith(argv, ".cmd"))
    {
        if (loadcmdfile(argv))
        {
            log_cb(RETRO_LOG_INFO, "Starting game from command line '%s'\n", argv);
            vice_opt.Model = 99; /* set model to unknown for custom settings - prevents overriding of command line options */
        }
        else
        {
            log_cb(RETRO_LOG_ERROR, "Failed to load command line from '%s'\n", argv);
        }
        parse_cmdline(CMDFILE);
    }
    else
        parse_cmdline(argv);

    /* Core command line is now parsed to ARGUV, ARGUC. */
    /* Build command file for VICE in XARGV, PARAMCOUNT. */
    bool single_image = strcmp(ARGUV[0], CORE_NAME) != 0;

    /* Allow using command lines without CORE_NAME by not allowing single_image */
    if (strendswith(argv, ".cmd"))
       single_image = false;

    /* If first command line argument is CORE_NAME, it's an extended command line
     * otherwise it's just image filename */
    if (single_image)
    {
        /* Command doesn't start with core name, so add it first */
        Add_Option(CORE_NAME);

        /* Ignore parsed arguments, read filename directly from argv */

        /* Check original filename for joystick control,
         * not the first name from M3U or VFL */
        joystick_control = check_joystick_control(argv);
        if (joystick_control)
        {
            cur_port = joystick_control;
            cur_port_locked = true;
        }

        /* "Browsed" file in ZIP */
        char browsed_file[RETRO_PATH_MAX] = {0};
        if (strstr(argv, ".zip#") || strstr(argv, ".7z#"))
        {
            char *token = strtok((char*)argv, "#");
            while (token != NULL)
            {
                snprintf(browsed_file, sizeof(browsed_file), "%s", token);
                token = strtok(NULL, "#");
            }
        }
        snprintf(full_path, sizeof(full_path), "%s", argv);

        /* ZIP + NIB vars, use the same temp directory for single NIBs */
        char zip_basename[RETRO_PATH_MAX] = {0};
        snprintf(zip_basename, sizeof(zip_basename), "%s", path_basename(full_path));
        path_remove_extension(zip_basename);

        char nib_input[RETRO_PATH_MAX] = {0};
        char nib_output[RETRO_PATH_MAX] = {0};

        /* NIB convert to G64 */
        if (dc_get_image_type(argv) == DC_IMAGE_TYPE_NIBBLER)
        {
            snprintf(nib_input, sizeof(nib_input), "%s", argv);
            snprintf(nib_output, sizeof(nib_output), "%s%s%s.g64", retro_temp_directory, FSDEV_DIR_SEP_STR, zip_basename);
            path_mkdir(retro_temp_directory);
            nib_convert(nib_input, nib_output);
            argv = nib_output;
        }

        /* ZIP */
        if (strendswith(argv, "zip") || strendswith(argv, "7z"))
        {
            path_mkdir(retro_temp_directory);
            if (strendswith(argv, "zip"))
               zip_uncompress(full_path, retro_temp_directory, NULL);
            else if (strendswith(argv, "7z"))
               sevenzip_uncompress(full_path, retro_temp_directory, NULL);

            /* Default to directory mode */
            int zip_mode = 0;
            snprintf(full_path, sizeof(full_path), "%s", retro_temp_directory);

            FILE *zip_m3u;
            char zip_m3u_list[DC_MAX_SIZE][RETRO_PATH_MAX] = {0};
            char zip_m3u_path[RETRO_PATH_MAX] = {0};
            snprintf(zip_m3u_path, sizeof(zip_m3u_path), "%s%s%s.m3u", retro_temp_directory, FSDEV_DIR_SEP_STR, zip_basename);
            int zip_m3u_num = 0;

            DIR *zip_dir;
            struct dirent *zip_dirp;

            /* Convert all NIBs to G64 */
            zip_dir = opendir(retro_temp_directory);
            while ((zip_dirp = readdir(zip_dir)) != NULL)
            {
                if (dc_get_image_type(zip_dirp->d_name) == DC_IMAGE_TYPE_NIBBLER)
                {
                    snprintf(nib_input, sizeof(nib_input), "%s%s%s", retro_temp_directory, FSDEV_DIR_SEP_STR, zip_dirp->d_name);
                    snprintf(nib_output, sizeof(nib_output), "%s%s%s.g64", retro_temp_directory, FSDEV_DIR_SEP_STR, path_remove_extension(zip_dirp->d_name));
                    nib_convert(nib_input, nib_output);
                }
            }
            closedir(zip_dir);

            zip_dir = opendir(retro_temp_directory);
            while ((zip_dirp = readdir(zip_dir)) != NULL)
            {
                if (zip_dirp->d_name[0] == '.' || strendswith(zip_dirp->d_name, ".m3u") || zip_mode > 1 || browsed_file[0] != '\0')
                    continue;

                /* Multi file mode, generate playlist */
                if (dc_get_image_type(zip_dirp->d_name) == DC_IMAGE_TYPE_FLOPPY
                 || dc_get_image_type(zip_dirp->d_name) == DC_IMAGE_TYPE_TAPE
                 || dc_get_image_type(zip_dirp->d_name) == DC_IMAGE_TYPE_MEM
                )
                {
                    zip_mode = 1;
                    zip_m3u_num++;
                    snprintf(zip_m3u_list[zip_m3u_num-1], RETRO_PATH_MAX, "%s", zip_dirp->d_name);
                }
            }
            closedir(zip_dir);

            switch (zip_mode)
            {
                case 0: /* Extracted path */
                    if (browsed_file[0] != '\0')
                    {
                        if (dc_get_image_type(browsed_file) == DC_IMAGE_TYPE_NIBBLER)
                            snprintf(full_path, sizeof(full_path), "%s%s%s.g64", retro_temp_directory, FSDEV_DIR_SEP_STR, path_remove_extension(browsed_file));
                        else
                            snprintf(full_path, sizeof(full_path), "%s%s%s", retro_temp_directory, FSDEV_DIR_SEP_STR, browsed_file);
                    }
                    break;
                case 1: /* Generated playlist */
                    zip_m3u = fopen(zip_m3u_path, "w");
                    qsort(zip_m3u_list, zip_m3u_num, RETRO_PATH_MAX, qstrcmp);
                    for (int l = 0; l < zip_m3u_num; l++)
                        fprintf(zip_m3u, "%s\n", zip_m3u_list[l]);
                    fclose(zip_m3u);
                    snprintf(full_path, sizeof(full_path), "%s", zip_m3u_path);
                    break;
            }

            argv = full_path;
        }

#if defined(__X64__) || defined(__X64SC__) || defined(__X128__) || defined(__XSCPU64__)
        /* Do not allow JiffyDOS with non-floppies */
        if (dc_get_image_type(argv) == DC_IMAGE_TYPE_TAPE
         || dc_get_image_type(argv) == DC_IMAGE_TYPE_MEM)
            opt_jiffydos_allow = 0;
        else
            opt_jiffydos_allow = 1;

        /* REU image check */
        if (path_is_valid(argv))
        {
            char reu_path[RETRO_PATH_MAX] = {0};
            char reu_base[RETRO_PATH_MAX] = {0};
            char reu_name[RETRO_PATH_MAX] = {0};

            snprintf(reu_base, sizeof(reu_base), "%s", argv);
            path_basedir(reu_base);

            snprintf(reu_name, sizeof(reu_name), "%s", argv);
            snprintf(reu_name, sizeof(reu_name), "%s", path_basename(reu_name));
            path_remove_extension(reu_name);

            snprintf(reu_path, sizeof(reu_path), "%s%s%s",
                     reu_base, reu_name, ".reu");

            if (path_is_valid(reu_path))
            {
                char reu_size[6] = {0};
                struct stat reu_stat;
                stat(reu_path, &reu_stat);
                snprintf(reu_size, sizeof(reu_size), "%u", (unsigned)reu_stat.st_size / 1024);

                Add_Option("-reu");
                Add_Option("-reusize");
                Add_Option(reu_size);
                Add_Option("+reuimagerw");
                Add_Option("-reuimage");
                Add_Option(reu_path);
            }
        }
#endif

#if defined(__XVIC__)
        /* Pretend to launch cartridge if using core option cartridge while launching
         * without content, because XVIC does not care about CartridgeFile resource */
        if (string_is_empty(argv) && !string_is_empty(vice_opt.CartridgeFile))
            argv = vice_opt.CartridgeFile;

        if (strendswith(argv, ".20"))
        {
            Add_Option("-cart2");
            /* For some unknown reason single cart at
             * $2000 also has to be in $6000.. */
            Add_Option(argv);
            Add_Option("-cart6");
        }
        else if (strendswith(argv, ".40"))
            Add_Option("-cart4");
        else if (strendswith(argv, ".60")
              || strendswith(argv, ".70"))
            Add_Option("-cart6");
        else if (strendswith(argv, ".a0"))
            Add_Option("-cartA");
        else if (strendswith(argv, ".b0"))
            Add_Option("-cartB");
        else if (strendswith(argv, ".prg")
              || strendswith(argv, ".crt")
              || strendswith(argv, ".rom")
              || strendswith(argv, ".bin")
              || strendswith(argv, ".m3u"))
        {
            /* There are PRGs that are actually carts, so we need to save the hassle
             * of mass renaming by differentiating them from regular program-PRGs.
             * Also separated cart PRGs meant to be assigned to specific memory
             * addresses require special care for hassle-free usage. */
            if (path_is_valid(argv))
            {
                char cart_base[RETRO_PATH_MAX] = {0};
                char cart_2000[RETRO_PATH_MAX] = {0};
                char cart_4000[RETRO_PATH_MAX] = {0};
                char cart_6000[RETRO_PATH_MAX] = {0};
                char cart_A000[RETRO_PATH_MAX] = {0};

                char cartmega_base[RETRO_PATH_MAX] = {0};
                char cartmega_nvram[RETRO_PATH_MAX] = {0};

                int type = vic20_autodetect_cartridge_type(argv);

                /* Need to examine the first playlist entry */
                if (strcasestr(argv, ".m3u"))
                {
                    FILE *fd;
                    char buf[RETRO_PATH_MAX] = {0};
                    char basepath[RETRO_PATH_MAX] = {0};
                    char fullpath[RETRO_PATH_MAX] = {0};
                    snprintf(basepath, sizeof(basepath), "%s", argv);
                    path_basedir(basepath);

                    fd = fopen(argv, MODE_READ);
                    if (fgets(buf, sizeof(buf), fd) != NULL)
                    {
                        buf[strcspn(buf, "\r\n")] = 0;
                        snprintf(fullpath, sizeof(fullpath), "%s%s", basepath, buf);
                    }
                    fclose(fd);

                    argv = fullpath;
                    type = vic20_autodetect_cartridge_type(argv);
                }

                switch (type)
                {
                    case CARTRIDGE_VIC20_16KB_2000:
                        Add_Option("-cart2");
                        Add_Option(argv);
                        Add_Option("-cart6");
                        break;
                    case CARTRIDGE_VIC20_8KB_2000:
                    case CARTRIDGE_VIC20_4KB_2000:
                        Add_Option("-cart2");
                        break;
                    case CARTRIDGE_VIC20_16KB_4000:
                    case CARTRIDGE_VIC20_8KB_4000:
                    case CARTRIDGE_VIC20_4KB_4000:
                        Add_Option("-cart4");
                        break;
                    case CARTRIDGE_VIC20_16KB_6000:
                    case CARTRIDGE_VIC20_8KB_6000:
                    case CARTRIDGE_VIC20_4KB_6000:
                        Add_Option("-cart6");
                        break;
                    case CARTRIDGE_VIC20_8KB_A000:
                    case CARTRIDGE_VIC20_4KB_A000:
                        Add_Option("-cartA");
                        break;
                    case CARTRIDGE_VIC20_4KB_B000:
                        Add_Option("-cartB");
                        break;
                    case CARTRIDGE_VIC20_GENERIC:
                        Add_Option("-cartgeneric");
                        break;
                    case CARTRIDGE_VIC20_MEGACART:
                        snprintf(cartmega_base, sizeof(cartmega_base), "%s", path_basename(argv));
                        path_remove_extension(cartmega_base);
                        snprintf(cartmega_nvram, sizeof(cartmega_nvram), "%s%s%s%s",
                                 retro_save_directory, FSDEV_DIR_SEP_STR, cartmega_base, ".nvr");
                        Add_Option("-mcnvramfile");
                        Add_Option(cartmega_nvram);
                        Add_Option("-cartmega");
                        break;
                    /* Separate ROM combination shenanigans */
                    case -1:
                    case -2:
                    case -3:
                        switch (type)
                        {
                            case -1: /* Gamebase */
                                snprintf(cart_base, sizeof(cart_base), "%s", argv);
                                path_remove_extension(cart_base);

                                snprintf(cart_base, sizeof(cart_base), "%s", string_replace_substring(cart_base, "-2000", ""));
                                snprintf(cart_base, sizeof(cart_base), "%s", string_replace_substring(cart_base, "-4000", ""));
                                snprintf(cart_base, sizeof(cart_base), "%s", string_replace_substring(cart_base, "-6000", ""));
                                snprintf(cart_base, sizeof(cart_base), "%s", string_replace_substring(cart_base, "-a000", ""));

                                snprintf(cart_2000, sizeof(cart_2000), "%s%s%s", cart_base, "-2000", ".prg");
                                snprintf(cart_4000, sizeof(cart_4000), "%s%s%s", cart_base, "-4000", ".prg");
                                snprintf(cart_6000, sizeof(cart_6000), "%s%s%s", cart_base, "-6000", ".prg");
                                snprintf(cart_A000, sizeof(cart_A000), "%s%s%s", cart_base, "-a000", ".prg");
                                break;

                            case -2: /* TOSEC */
                                snprintf(cart_base, sizeof(cart_base), "%s", argv);
                                path_remove_extension(cart_base);
                                snprintf(cart_base, sizeof(cart_base), "%s", string_replace_substring(cart_base, "[4000]", "[2000]"));
                                snprintf(cart_base, sizeof(cart_base), "%s", string_replace_substring(cart_base, "[6000]", "[2000]"));
                                snprintf(cart_base, sizeof(cart_base), "%s", string_replace_substring(cart_base, "[A000]", "[2000]"));
                                snprintf(cart_2000, sizeof(cart_2000), "%s%s", cart_base, ".crt");

                                snprintf(cart_base, sizeof(cart_base), "%s", argv);
                                path_remove_extension(cart_base);
                                snprintf(cart_base, sizeof(cart_base), "%s", string_replace_substring(cart_base, "[2000]", "[4000]"));
                                snprintf(cart_base, sizeof(cart_base), "%s", string_replace_substring(cart_base, "[6000]", "[4000]"));
                                snprintf(cart_base, sizeof(cart_base), "%s", string_replace_substring(cart_base, "[A000]", "[4000]"));
                                snprintf(cart_4000, sizeof(cart_4000), "%s%s", cart_base, ".crt");

                                snprintf(cart_base, sizeof(cart_base), "%s", argv);
                                path_remove_extension(cart_base);
                                snprintf(cart_base, sizeof(cart_base), "%s", string_replace_substring(cart_base, "[2000]", "[6000]"));
                                snprintf(cart_base, sizeof(cart_base), "%s", string_replace_substring(cart_base, "[4000]", "[6000]"));
                                snprintf(cart_base, sizeof(cart_base), "%s", string_replace_substring(cart_base, "[A000]", "[6000]"));
                                snprintf(cart_6000, sizeof(cart_6000), "%s%s", cart_base, ".crt");

                                snprintf(cart_base, sizeof(cart_base), "%s", argv);
                                path_remove_extension(cart_base);
                                snprintf(cart_base, sizeof(cart_base), "%s", string_replace_substring(cart_base, "[2000]", "[A000]"));
                                snprintf(cart_base, sizeof(cart_base), "%s", string_replace_substring(cart_base, "[4000]", "[A000]"));
                                snprintf(cart_base, sizeof(cart_base), "%s", string_replace_substring(cart_base, "[6000]", "[A000]"));
                                snprintf(cart_A000, sizeof(cart_A000), "%s%s", cart_base, ".crt");
                                break;

                            case -3: /* No-Intro */
                                snprintf(cart_base, sizeof(cart_base), "%s", argv);
                                path_remove_extension(cart_base);
                                snprintf(cart_base, sizeof(cart_base), "%s", string_replace_substring(cart_base, "$4000", "$2000"));
                                snprintf(cart_base, sizeof(cart_base), "%s", string_replace_substring(cart_base, "$6000", "$2000"));
                                snprintf(cart_base, sizeof(cart_base), "%s", string_replace_substring(cart_base, "$A000", "$2000"));
                                snprintf(cart_2000, sizeof(cart_2000), "%s%s", cart_base, ".20");

                                snprintf(cart_base, sizeof(cart_base), "%s", argv);
                                path_remove_extension(cart_base);
                                snprintf(cart_base, sizeof(cart_base), "%s", string_replace_substring(cart_base, "$2000", "$4000"));
                                snprintf(cart_base, sizeof(cart_base), "%s", string_replace_substring(cart_base, "$6000", "$4000"));
                                snprintf(cart_base, sizeof(cart_base), "%s", string_replace_substring(cart_base, "$A000", "$4000"));
                                snprintf(cart_4000, sizeof(cart_4000), "%s%s", cart_base, ".40");

                                snprintf(cart_base, sizeof(cart_base), "%s", argv);
                                path_remove_extension(cart_base);
                                snprintf(cart_base, sizeof(cart_base), "%s", string_replace_substring(cart_base, "$2000", "$6000"));
                                snprintf(cart_base, sizeof(cart_base), "%s", string_replace_substring(cart_base, "$4000", "$6000"));
                                snprintf(cart_base, sizeof(cart_base), "%s", string_replace_substring(cart_base, "$A000", "$6000"));
                                snprintf(cart_6000, sizeof(cart_6000), "%s%s", cart_base, ".60");

                                snprintf(cart_base, sizeof(cart_base), "%s", argv);
                                path_remove_extension(cart_base);
                                snprintf(cart_base, sizeof(cart_base), "%s", string_replace_substring(cart_base, "$2000", "$A000"));
                                snprintf(cart_base, sizeof(cart_base), "%s", string_replace_substring(cart_base, "$4000", "$A000"));
                                snprintf(cart_base, sizeof(cart_base), "%s", string_replace_substring(cart_base, "$6000", "$A000"));
                                snprintf(cart_A000, sizeof(cart_A000), "%s%s", cart_base, ".a0");
                                break;
                        }

                        if (path_is_valid(cart_2000))
                        {
                            Add_Option("-cart2");
                            Add_Option(cart_2000);

                            if (!path_is_valid(cart_6000))
                            {
                                Add_Option("-cart6");
                                Add_Option(cart_2000);
                            }
                        }
                        if (path_is_valid(cart_4000))
                        {
                            Add_Option("-cart4");
                            Add_Option(cart_4000);
                        }
                        if (path_is_valid(cart_6000))
                        {
                            Add_Option("-cart6");
                            Add_Option(cart_6000);
                        }
                        if (path_is_valid(cart_A000))
                        {
                            Add_Option("-cartA");
                            Add_Option(cart_A000);
                        }

                        argv = "";
                        break;
                    default:
                        break;
                }
            }
        }

        /* Memory expansion force:
         * First from content path,
         * then from playlist entry after m3u parsing */
        vic20_mem_force(argv);

#elif defined(__XPLUS4__)
        if (strendswith(argv, ".crt") || strendswith(argv, ".bin"))
            Add_Option("-cart");
#endif

        if (strendswith(argv, "m3u"))
        {
            /* Parse the m3u file */
            dc_parse_m3u(dc, argv);
            is_fliplist = true;
#if defined(__XVIC__)
            /* Memory expansion force */
            if (vic20mem_forced < 0)
                vic20_mem_force(dc->files[0]);
#endif
        }
        else if (strendswith(argv, "vfl"))
        {
            /* Parse the vfl file */
            dc_parse_vfl(dc, argv);
            is_fliplist = true;
        }

        if (!is_fliplist)
        {
            /* Add image name as autostart parameter */
            if (argv[0])
                Add_Option(argv);
        }
        else
        {
            /* Some debugging */
            log_cb(RETRO_LOG_INFO, "M3U/VFL parsed, %d file(s) found\n", dc->count);

            if (!dc->command)
            {
                char option[RETRO_PATH_MAX] = {0};
                if (!string_is_empty(dc->load[0]))
                    snprintf(option, sizeof(option), "%s:%s", dc->files[0], dc->load[0]);
                else
                    snprintf(option, sizeof(option), "%s", dc->files[0]);

                /* Add first disk from list as autostart parameter */
                if (dc->count != 0)
                    Add_Option(option);
            }
            else
            {
                /* Re-parse command line from M3U #COMMAND: */
                log_cb(RETRO_LOG_INFO, "Starting game from command line: %s\n", dc->command);
                vice_opt.Model = 99; /* set model to unknown for custom settings - prevents overriding of command line options */
                parse_cmdline(dc->command);
                /* Reset parameters list for VICE */
                PARAMCOUNT = 0;
                single_image = false;
            }
        }
    }

    /* It might be single_image initially, but changed by M3U file #COMMAND line */
    if (!single_image)
    {
        /* Command doesn't start with core name, so add it first */
        if (ARGUC == 0 || strcmp(ARGUV[0], CORE_NAME) != 0)
            Add_Option(CORE_NAME);

        bool is_flipname_param = false;
        /* Scan vice arguments for special processing */
        for (i = 0; i < ARGUC; i++)
        {
            const char* arg = ARGUV[i];

            /* Previous arg was '-flipname' */
            if (is_flipname_param)
            {
                is_flipname_param = false;
                /* Parse the vfl file, don't pass to vice */
                dc_parse_vfl(dc, arg);
                /* Don't pass -flipname argument to vice - it has no use of it */
                /* and we won't have to take care of cleaning it up */
                is_fliplist = true;
            }
            else if (!strcmp(arg, "-j1"))
            {
                cur_port = 1;
                cur_port_locked = true;
            }
            else if (!strcmp(arg, "-j2"))
            {
                cur_port = 2;
                cur_port_locked = true;
            }
            else if (strendswith(arg, "m3u"))
            {
                /* Parse the m3u file, don't pass to vice */
                dc_parse_m3u(dc, arg);
                is_fliplist = true;
            }
            else if (!strcmp(arg, "-flipname"))
            {
                /* Set flag for next arg */
                is_flipname_param = true;
            }
            else if (!strcmp(arg, "-noautostart"))
            {
                /* User ask to not automatically start image in drive */
                noautostart = true;
            }
            else if (!strcmp(arg, "-autostart"))
            {
                /* User ask to automatically start image in drive */
                noautostart = false;
            }
            else
            {
                /* Fill cmd arg path from argv if there is none */
                if (strstr(arg, ".") && !strstr(arg, FSDEV_DIR_SEP_STR))
                {
                    char arg_path[RETRO_PATH_MAX] = {0};
                    char arg_full[RETRO_PATH_MAX] = {0};
                    strcpy(arg_path, argv);
                    path_basedir(arg_path);
                    strcpy(arg_full, arg_path);
                    strcat(arg_full, arg);
                    Add_Option(arg_full);
                }
                else
                    Add_Option(arg);
            }
        }

        if (is_fliplist)
        {
            /* Some debugging */
            log_cb(RETRO_LOG_INFO, "M3U/VFL parsed, %d file(s) found\n", dc->count);
        }
    }

#if defined(__XVIC__) || defined(__XPLUS4__)
     /* Disable floppy drive when using carts with cores that won't disable it via autostart */
     for (int i = 0; i < PARAMCOUNT; i++)
     {
         if (strstr(XARGV[i], "-cart") && XARGV[i][0] == '-')
         {
             Add_Option("-drive8type");
             Add_Option("0");
             break;
         }
     }
#endif
    return 0;
}

static void autodetect_drivetype(int unit)
{
    int drive_type, set_drive_type;
    char drive_type_resource_var[20] = {0};
    snprintf(drive_type_resource_var, sizeof(drive_type_resource_var), "Drive%dType", unit);
    resources_get_int(drive_type_resource_var, &drive_type);
    const char* attached_image = NULL;
    attached_image = file_system_get_disk_name(unit, 0);

    /* Autodetect drive type */
    vdrive_t *vdrive;
    struct disk_image_s *diskimg;

    vdrive = file_system_get_vdrive(unit, 0);
    if (vdrive == NULL)
        log_cb(RETRO_LOG_ERROR, "Failed to get vdrive reference for unit %d.\n", unit);
    else
    {
        diskimg = vdrive->image;
        if (diskimg == NULL)
            log_cb(RETRO_LOG_ERROR, "Failed to get disk image for unit %d.\n", unit);
        else
        {
            /* G64/G71 exceptions for preventing unwanted drive type sets */
            if (diskimg->type == DISK_IMAGE_TYPE_G64)
                set_drive_type = DISK_IMAGE_TYPE_D64;
            else if (diskimg->type == DISK_IMAGE_TYPE_G71)
                set_drive_type = DISK_IMAGE_TYPE_D71;
            /* Force 1541 to 1541-II */
            else if (diskimg->type == DRIVE_TYPE_1541)
                set_drive_type = DRIVE_TYPE_1541II;
            else
                set_drive_type = diskimg->type;

            if (set_drive_type == drive_type)
                return;

            log_cb(RETRO_LOG_INFO, "Autodetected image type %u.\n", diskimg->type);
            if (log_resources_set_int(drive_type_resource_var, set_drive_type) < 0)
                log_cb(RETRO_LOG_ERROR, "Failed to set drive type.\n");

            /* Change from 1581 to 1541 will not detect disk properly without reattaching (?!) */
            file_system_detach_disk(unit, 0);
            file_system_attach_disk(unit, 0, attached_image);

            /* Don't bother with drive sound muting when autoloadwarp is on */
            if (opt_autoloadwarp & AUTOLOADWARP_DISK)
               return;
            /* Drive motor sound keeps on playing if the drive type is changed while the motor is running */
            /* Also happens when toggling TDE */
            switch (set_drive_type)
            {
                case DISK_IMAGE_TYPE_G64:
                case DISK_IMAGE_TYPE_G71:
                case DISK_IMAGE_TYPE_D64:
                case DISK_IMAGE_TYPE_D71:
                    resources_set_int("DriveSoundEmulationVolume", vice_opt.DriveSoundEmulation);
                    break;
                default:
                    resources_set_int("DriveSoundEmulationVolume", 0);
                    break;
            }
        }
    }
}

void update_work_disk()
{
    request_update_work_disk = false;
    const char* attached_image = NULL;

    /* Skip if device unit collides with autostart */
    if (!string_is_empty(full_path) && opt_work_disk_unit == 8)
        opt_work_disk_type = 0;
    if (opt_work_disk_type)
    {
        /* Path vars */
        char opt_work_disk_filename[RETRO_PATH_MAX] = {0};
        char opt_work_disk_filepath[RETRO_PATH_MAX] = {0};
        char opt_work_disk_extension[4] = {0};
        switch (opt_work_disk_type)
        {
            default:
            case DISK_IMAGE_TYPE_D64:
                snprintf(opt_work_disk_extension, sizeof(opt_work_disk_extension), "%s", "d64");
                break;
            case DISK_IMAGE_TYPE_D71:
                snprintf(opt_work_disk_extension, sizeof(opt_work_disk_extension), "%s", "d71");
                break;
            case DISK_IMAGE_TYPE_D81:
                snprintf(opt_work_disk_extension, sizeof(opt_work_disk_extension), "%s", "d81");
                break;
        }
        snprintf(opt_work_disk_filename, sizeof(opt_work_disk_filename), "vice_work.%s", opt_work_disk_extension);
        path_join((char*)&opt_work_disk_filepath, retro_save_directory, opt_work_disk_filename);

        /* Create disk */
        if (!path_is_valid(opt_work_disk_filepath))
        {
            /* Label format */
            char format_name[28];
            snprintf(format_name, sizeof(format_name), "%s-%s", "work", opt_work_disk_extension);
            charset_petconvstring((uint8_t *)format_name, 0);

            if (vdrive_internal_create_format_disk_image(opt_work_disk_filepath, format_name, opt_work_disk_type))
                log_cb(RETRO_LOG_INFO, "Work disk creation failed: '%s'\n", opt_work_disk_filepath);
            else
                log_cb(RETRO_LOG_INFO, "Work disk created: '%s'\n", opt_work_disk_filepath);
        }

        /* Attach disk */
        if (path_is_valid(opt_work_disk_filepath))
        {
            /* Detach previous disks */
            if ((attached_image = file_system_get_disk_name(8, 0)) != NULL)
                file_system_detach_disk(8, 0);

            if ((attached_image = file_system_get_disk_name(9, 0)) != NULL)
            {
                file_system_detach_disk(9, 0);
                log_resources_set_int("Drive9Type", DRIVE_TYPE_NONE);
            }

            if (opt_work_disk_unit == 9)
                log_resources_set_int("Drive9Type", opt_work_disk_type);
            file_system_attach_disk(opt_work_disk_unit, 0, opt_work_disk_filepath);
            autodetect_drivetype(opt_work_disk_unit);
            log_cb(RETRO_LOG_INFO, "Work disk '%s' attached in drive #%d\n", opt_work_disk_filepath, opt_work_disk_unit);
            display_current_image(opt_work_disk_filename, true);
        }
    }
    else
    {
        /* Detach work disk if disabled while running */
        if ((attached_image = file_system_get_disk_name(8, 0)) != NULL && strstr(attached_image, "vice_work"))
        {
            if (string_is_empty(full_path) || (!string_is_empty(full_path) && !strstr(full_path, "vice_work")))
            {
                log_cb(RETRO_LOG_INFO, "Work disk '%s' detached from drive #%d\n", attached_image, 8);
                file_system_detach_disk(8, 0);
                log_resources_set_int("Drive8Type", DRIVE_TYPE_1541);
                display_current_image(attached_image, false);
            }
        }

        if ((attached_image = file_system_get_disk_name(9, 0)) != NULL && strstr(attached_image, "vice_work"))
        {
            log_cb(RETRO_LOG_INFO, "Work disk '%s' detached from drive #%d\n", attached_image, 9);
            file_system_detach_disk(9, 0);
            log_resources_set_int("Drive9Type", DRIVE_TYPE_NONE);
        }
    }
}

/* Update autostart image from vice and add disk in drive to fliplist */
void update_from_vice()
{
    const char* attachedImage = NULL;

    /* Get autostart string from vice, handle carts differently */
    if (dc->unit == 0 && autostartString != NULL)
    {
        free(autostartProgram);
        autostartProgram = NULL;
        free(autostartString);
        autostartString = NULL;
        attachedImage = dc->files[dc->index];
        /* Disable AutostartWarp & WarpMode, otherwise warp gets stuck with PRGs in M3Us */
        resources_set_int("AutostartWarp", 0);
        resources_set_int("WarpMode", 0);
    }
    else
    {
        free(autostartProgram);
        autostartProgram = x_strdup(dc->load[dc->index]);
        free(autostartString);
        autostartString = x_strdup(cmdline_get_autostart_string());
        if (!autostartString && !string_is_empty(full_path))
            autostartString = x_strdup(full_path);
    }

    if (autostartString)
        log_cb(RETRO_LOG_INFO, "Image for autostart: '%s'\n", autostartString);
    else
        log_cb(RETRO_LOG_INFO, "No image for autostart\n");

#if defined(__X64__) || defined(__X64SC__) || defined(__XSCPU64__) || defined(__XVIC__)
    /* Automatic model request */
    if (opt_model_auto && !string_is_empty(full_path))
    {
        if (strstr(full_path, "NTSC") ||
            strstr(full_path, "(USA)") ||
            strstr(full_path, "(Japan)") ||
            strstr(full_path, "(Japan, USA)"))
        {
#if defined(__X64__) || defined(__X64SC__) || defined(__XSCPU64__)
            request_model_set = C64MODEL_C64_NTSC;
            if (vice_opt.Model == C64MODEL_C64C_PAL || vice_opt.Model == C64MODEL_C64C_NTSC)
                request_model_set = C64MODEL_C64C_NTSC;
#elif defined(__XVIC__)
            request_model_set = VIC20MODEL_VIC20_NTSC;
#endif
        }

        if (strstr(full_path, "PAL") ||
            strstr(full_path, "(Europe)") ||
            strstr(full_path, "(Finland)") ||
            strstr(full_path, "(France)") ||
            strstr(full_path, "(Germany)") ||
            strstr(full_path, "(Netherlands)") ||
            strstr(full_path, "(Sweden)"))
        {
#if defined(__X64__) || defined(__X64SC__) || defined(__XSCPU64__)
            request_model_set = C64MODEL_C64_PAL;
            if (vice_opt.Model == C64MODEL_C64C_NTSC || vice_opt.Model == C64MODEL_C64C_PAL)
                request_model_set = C64MODEL_C64C_PAL;
#elif defined(__XVIC__)
            request_model_set = VIC20MODEL_VIC20_PAL;
#endif
        }

#if defined(__X64__) || defined(__X64SC__) || defined(__XSCPU64__)
        if (strstr(full_path, "(GS)"))
            request_model_set = C64MODEL_C64_GS;
        else if (strstr(full_path, "(MAX)"))
            request_model_set = C64MODEL_ULTIMAX;
#endif
    }
    else
        request_model_set = -1;
#endif

    /* If flip list is empty, get current tape or floppy image name and add to the list */
    if (dc->count == 0)
    {
#if defined(__X64__) || defined(__X64SC__) || defined(__XSCPU64__) || defined(__X128__)
        if ((attachedImage = cartridge_get_file_name(cart_getid_slotmain())) != NULL)
#else
        if ((attachedImage = cartridge_get_file_name(0)) != NULL)
#endif
        {
            dc->unit = 0;
            dc_add_file(dc, attachedImage, NULL, NULL, NULL);
        }
        else if ((attachedImage = tape_get_file_name()) != NULL)
        {
            dc->unit = 1;
            dc_add_file(dc, attachedImage, NULL, NULL, NULL);
        }
        else
        {
#if 0
            int unit;
            for (unit = 8; unit <= 11; ++unit)
            {
                if ((attachedImage = file_system_get_disk_name(unit, 0)) != NULL)
                {
                    dc->unit = unit;
                    dc_add_file(dc, attachedImage, NULL, NULL, NULL);
                    break;
                }
            }
#else
            /* Only add images to the list from device 8, otherwise leads to confusion when other devices have disks,
             * because Disk Control operates only on device 8 for now. */
            int unit = 8;
            if ((attachedImage = file_system_get_disk_name(unit, 0)) != NULL)
            {
                dc->unit = unit;
                dc_add_file(dc, attachedImage, NULL, NULL, NULL);
            }
#endif
        }
    }

#if defined(__X64__) || defined(__X64SC__) || defined(__X128__) || defined(__XSCPU64__)
    /* Disable JiffyDOS with tapes and carts */
    if (opt_jiffydos && dc->unit <= 1 && dc->count > 0)
    {
        opt_jiffydos_allow = 0;
        opt_jiffydos = 0;
        reload_restart();
    }
#endif

    /* Logging */
    if (dc->count > 0)
    {
        if (dc->unit == 1)
            log_cb(RETRO_LOG_INFO, "Tape image list has %d file(s)\n", dc->count);
        else if (dc->unit >= 8 && dc->unit <= 11)
            log_cb(RETRO_LOG_INFO, "Drive #%d image list has %d file(s)\n", dc->unit, dc->count);
        else if (dc->unit == 0)
            log_cb(RETRO_LOG_INFO, "Cartridge image list has %d file(s)\n", dc->count);

        for(unsigned i = 0; i < dc->count; i++)
            log_cb(RETRO_LOG_DEBUG, "File %d: %s\n", i+1, dc->files[i]);
    }

    /* Scan for save disk 0, append if exists */
    if (dc->count && dc->unit == 8)
    {
        bool file_check = dc_save_disk_toggle(dc, true, false);
        if (file_check)
            dc_save_disk_toggle(dc, false, false);
    }

    /* If flip list is not empty, but there is no image attached to drive, attach the first one from list.
     * This can only happen if flip list was loaded via cmd file or from m3u with #COMMAND */
    if (dc->count != 0)
    {
        if (dc->unit == 1)
        {
            if ((attachedImage = tape_get_file_name()) == NULL)
            {
                attachedImage = dc->files[0];
                autostartProgram = x_strdup(dc->load[0]);
                /* Don't attach if we will autostart from it just in a moment */
                if (autostartString != NULL || noautostart)
                {
                    log_cb(RETRO_LOG_INFO, "Attaching first tape '%s'\n", attachedImage);
                    tape_image_attach(dc->unit, attachedImage);
                }
            }
        }
        else if (dc->unit == 8)
        {
            if ((attachedImage = file_system_get_disk_name(dc->unit, 0)) == NULL)
            {
                attachedImage = dc->files[0];
                autostartProgram = x_strdup(dc->load[0]);
                /* Don't attach if we will autostart from it just in a moment */
                if (autostartString != NULL || noautostart)
                {
                    log_cb(RETRO_LOG_INFO, "Attaching first disk '%s' to drive #%d\n", attachedImage, dc->unit);
                    file_system_attach_disk(dc->unit, 0, attachedImage);
                }
            }
        }
        if (dc->unit == 0)
        {
            if (attachedImage == NULL)
            {
                attachedImage = dc->files[0];
                autostartProgram = NULL;
                /* Don't attach if we will autostart from it just in a moment */
                if (autostartString != NULL || noautostart)
                {
                    log_cb(RETRO_LOG_INFO, "Attaching first cart '%s'\n", attachedImage);
#if defined(__XVIC__)
                    cartridge_attach_image(vic20_autodetect_cartridge_type(attachedImage), attachedImage);
#elif defined(__XPLUS4__)
                    cartridge_attach_image(CARTRIDGE_PLUS4_DETECT, attachedImage);
                    /* No autostarting carts, otherwise gfx gets corrupted (?!) */
                    noautostart = true;
#else
                    cartridge_attach_image(dc->unit, attachedImage);
#endif
                }
            }
        }
    }

    /* Disable autostart only with disks or tapes */
    if (!string_is_empty(attachedImage))
    {
        if (noautostart)
            autostart_disable();
        else if (!noautostart && !string_is_empty(autostartString) &&
                 strcmp(autostartString, attachedImage) &&
                 string_is_empty(autostartProgram) &&
                 dc_get_image_type(attachedImage) != DC_IMAGE_TYPE_MEM)
            autostartString = NULL;
    }

    /* If there an image attached, but autostart is empty, autostart from the image */
    if (string_is_empty(autostartString) && !string_is_empty(attachedImage) && !noautostart && !CMDFILE[0])
    {
        log_cb(RETRO_LOG_INFO, "Autostarting from attached or first image '%s'\n", attachedImage);
        autostartString = x_strdup(attachedImage);
        if (!string_is_empty(autostartProgram))
            charset_petconvstring((uint8_t *)autostartProgram, 0);

        autostart_autodetect(autostartString, autostartProgram, 0, AUTOSTART_MODE_RUN);
    }

    /* If vice has image attached to drive, tell libretro that the 'tray' is closed */
    if (!string_is_empty(attachedImage))
    {
        dc->eject_state = false;
        display_current_image(dc->labels[dc->index], true);
    }
    else
    {
        dc->eject_state = true;
        display_current_image("", false);
    }
}

void build_params()
{
    int i;

    if (PARAMCOUNT == 0)
    {
        /* No game loaded - set command line to 'x64' */
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

    *archdep_startup_error = 0;
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

        *archdep_startup_error = 0;
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

void reload_restart(void)
{
    /* Clear request */
    request_reload_restart = false;

    /* Reset Datasette */
    datasette_control(DATASETTE_CONTROL_RESET);

    /* Cleanup after previous content and reset resources */
    initcmdline_cleanup();

    /* Update resources from environment just like on fresh start of core */
    sound_volume_counter_reset();
    request_model_prev = -1;
    retro_ui_finalized = false;
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

/* FPS counter + mapper tick */
long retro_ticks(void)
{
   if (!perf_cb.get_time_usec)
      return retro_now;

   return perf_cb.get_time_usec();
}

static int retro_keymap_id(const char *val)
{
   int i = 0;
   while (retro_keys[i].id < RETROK_LAST)
   {
      if (!strcmp(retro_keys[i].value, val))
         return retro_keys[i].id;
      i++;
   }
   return 0;
}

static void retro_led_interface(void)
{
   /* 0: Power
    * 1: Floppy
    * 2: Tape */

   unsigned int led_state[3] = {0};

   led_state[0] = (request_restart) ? 0 : 1;
   led_state[1] = (vice_opt.DriveTrueEmulation) ? vice_led_state[1] : 0;
   led_state[2] = vice_led_state[2];

   for (unsigned l = 0; l < sizeof(led_state)/sizeof(led_state[0]); l++)
   {
      if (retro_led_state[l] != led_state[l])
      {
         retro_led_state[l] = led_state[l];
         led_state_cb(l, led_state[l]);
      }
   }
}

void retro_set_paths(void)
{
   const char *system_dir = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &system_dir) && system_dir)
   {
      strlcpy(retro_system_directory,
              system_dir,
              sizeof(retro_system_directory));
   }

   const char *content_dir = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_CONTENT_DIRECTORY, &content_dir) && content_dir)
   {
      strlcpy(retro_content_directory,
              content_dir,
              sizeof(retro_content_directory));
   }

   /* Paths are first run in retro_set_environment(), but saves will not yet be correct then,
    * therefore re-run in retro_init() and replace when necessary */
   if (string_is_empty(retro_save_directory) || !strcmp(retro_save_directory, retro_system_directory))
   {
      const char *save_dir = NULL;
      if (environ_cb(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY, &save_dir) && save_dir)
      {
         /* If save directory is defined use it, otherwise use system directory */
         strlcpy(retro_save_directory,
                 string_is_empty(save_dir) ? retro_system_directory : save_dir,
                 sizeof(retro_save_directory));
      }
      else
      {
         /* Make retro_save_directory the same in case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY is not implemented by the frontend */
         strlcpy(retro_save_directory,
                 retro_system_directory,
                 sizeof(retro_save_directory));
      }
   }

   if (string_is_empty(retro_system_directory))
   {
#if defined(ANDROID)
      strlcpy(retro_system_directory, "/mnt/sdcard", sizeof(retro_system_directory));
#elif defined(VITA)
      strlcpy(retro_system_directory, "ux0:/data", sizeof(retro_system_directory));
#elif defined(__SWITCH__)
      strlcpy(retro_system_directory, "/", sizeof(retro_system_directory));
#else
      strlcpy(retro_system_directory, ".", sizeof(retro_system_directory));
#endif
   }

   /* Temp directory for ZIPs and NIB->G64 conversions */
   snprintf(retro_temp_directory, sizeof(retro_temp_directory), "%s%s%s",
            retro_save_directory, FSDEV_DIR_SEP_STR, "TEMP");

   /* Use system directory for data files such as JiffyDOS and keymaps */
   snprintf(retro_system_data_directory, sizeof(retro_system_data_directory), "%s%s%s",
            retro_system_directory, FSDEV_DIR_SEP_STR, "vice");
   if (!path_is_directory(retro_system_data_directory))
      archdep_mkdir(retro_system_data_directory, 0);
}

static void free_vice_carts(void)
{
   size_t i;
   for (i = 0; i < RETRO_NUM_CORE_OPTION_VALUES_MAX; i++)
   {
      if (vice_carts[i].value)
      {
         free(vice_carts[i].value);
         vice_carts[i].value = NULL;
      }
      if (vice_carts[i].label)
      {
         free(vice_carts[i].label);
         vice_carts[i].label = NULL;
      }
   }

   free(vice_carts_info);
   vice_carts_info = NULL;
}

void retro_set_environment(retro_environment_t cb)
{
   environ_cb = cb;
   retro_set_paths();
   free_vice_carts();

   /* Controller ports */
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
      { "Keyboard", RETRO_DEVICE_VICE_KEYBOARD },
      { "None", RETRO_DEVICE_NONE },
   };

   static const struct retro_controller_info ports[] = {
      { p1_controllers, 3 }, /* port 1 */
      { p2_controllers, 3 }, /* port 2 */
      { p3_controllers, 3 }, /* port 3 */
      { p4_controllers, 3 }, /* port 4 */
      { p5_controllers, 2 }, /* port 5 */
      { NULL, 0 }
   };

   cb(RETRO_ENVIRONMENT_SET_CONTROLLER_INFO, (void*)ports);

   /* Core options */
   static struct retro_core_option_definition core_options[] =
   {
#if defined(__XVIC__)
      {
         "vice_vic20_model",
         "Model",
         "'Automatic' switches region per file path tags.\nChanging while running resets the system!",
         {
            { "VIC20 PAL auto", "VIC-20 PAL Automatic" },
            { "VIC20 NTSC auto", "VIC-20 NTSC Automatic" },
            { "VIC20 PAL", "VIC-20 PAL" },
            { "VIC20 NTSC", "VIC-20 NTSC" },
            { "VIC21", "Super VIC (+16K) NTSC" },
            { NULL, NULL },
         },
         "VIC20 PAL auto"
      },
      {
         "vice_vic20_memory_expansions",
         "System > Memory Expansion",
         "Can be forced with filename tags '(8k)' & '(8kb)' or directory tags '8k' & '8kb'.\nChanging while running resets the system!",
         {
            { "none", "disabled" },
            { "3kB", "3kB" },
            { "8kB", "8kB" },
            { "16kB", "16kB" },
            { "24kB", "24kB" },
            { "35kB", "35kB" },
            { NULL, NULL },
         },
         "none"
      },
#elif defined(__XPLUS4__)
      {
         "vice_plus4_model",
         "Model",
         "Changing while running resets the system!",
         {
            { "C16 PAL", "C16 PAL" },
            { "C16 NTSC", "C16 NTSC" },
            { "PLUS4 PAL", "Plus/4 PAL" },
            { "PLUS4 NTSC", "Plus/4 NTSC" },
            { "V364 NTSC", "V364 NTSC" },
            { "232 NTSC", "C232 NTSC" },
            { NULL, NULL }
         },
         "PLUS4 PAL"
      },
#elif defined(__X128__)
      {
         "vice_c128_model",
         "Model",
         "Changing while running resets the system!",
         {
            { "C128 PAL", "C128 PAL" },
            { "C128 NTSC", "C128 NTSC" },
            { "C128 D PAL", "C128 D PAL" },
            { "C128 D NTSC", "C128 D NTSC" },
            { "C128 DCR PAL", "C128 DCR PAL" },
            { "C128 DCR NTSC", "C128 DCR NTSC" },
            { NULL, NULL },
         },
         "C128 PAL"
      },
      {
         "vice_c128_video_output",
         "System > Video Output",
         "",
         {
            { "VICII", "VIC-II (40 cols)" },
            { "VDC", "VDC (80 cols)" },
            { NULL, NULL },
         },
         "VICII"
      },
      {
         "vice_c128_go64",
         "System > GO64",
         "Start in C64 compatibility mode.\nChanging while running resets the system!",
         {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
         },
         "disabled"
      },
#elif defined(__XPET__)
      {
         "vice_pet_model",
         "Model",
         "Changing while running resets the system!",
         {
            { "2001", "PET 2001" },
            { "3008", "PET 3008" },
            { "3016", "PET 3016" },
            { "3032", "PET 3032" },
            { "3032B", "PET 3032B" },
            { "4016", "PET 4016" },
            { "4032", "PET 4032" },
            { "4032B", "PET 4032B" },
            { "8032", "PET 8032" },
            { "8096", "PET 8096" },
            { "8296", "PET 8296" },
            { "SUPERPET", "Super PET" },
            { NULL, NULL },
         },
         "8032"
      },
#elif defined(__XCBM2__)
      {
         "vice_cbm2_model",
         "Model",
         "Changing while running resets the system!",
         {
            { "610 PAL", "CBM 610 PAL" },
            { "610 NTSC", "CBM 610 NTSC" },
            { "620 PAL", "CBM 620 PAL" },
            { "620 NTSC", "CBM 620 NTSC" },
            { "620PLUS PAL", "CBM 620+ PAL" },
            { "620PLUS NTSC", "CBM 620+ NTSC" },
            { "710 NTSC", "CBM 710 NTSC" },
            { "720 NTSC", "CBM 720 NTSC" },
            { "720PLUS NTSC", "CBM 720+ NTSC" },
            { NULL, NULL },
         },
         "610 PAL"
      },
#elif defined(__XCBM5x0__)
      {
         "vice_cbm5x0_model",
         "Model",
         "Changing while running resets the system!",
         {
            { "510 PAL", "CBM 510 PAL" },
            { "510 NTSC", "CBM 510 NTSC" },
            { NULL, NULL },
         },
         "510 PAL"
      },
#elif defined(__X64DTV__)
      {
         "vice_c64dtv_model",
         "Model",
         "Changing while running resets the system!",
         {
            { "DTV2 PAL", "DTV v2 PAL" },
            { "DTV2 NTSC", "DTV v2 NTSC" },
            { "DTV3 PAL", "DTV v3 PAL" },
            { "DTV3 NTSC", "DTV v3 NTSC" },
            { "HUMMER NTSC", "Hummer NTSC" },
            { NULL, NULL },
         },
         "DTV3 PAL"
      },
#else
      {
         "vice_c64_model",
         "Model",
         "'Automatic' switches region per file path tags.\nChanging while running resets the system!",
         {
            { "C64 PAL auto", "C64 PAL Automatic" },
            { "C64 NTSC auto", "C64 NTSC Automatic" },
            { "C64C PAL auto", "C64C PAL Automatic" },
            { "C64C NTSC auto", "C64C NTSC Automatic" },
            { "C64 PAL", "C64 PAL" },
            { "C64 NTSC", "C64 NTSC" },
            { "C64C PAL", "C64C PAL" },
            { "C64C NTSC", "C64C NTSC" },
            { "C64SX PAL", "SX-64 PAL" },
            { "C64SX NTSC", "SX-64 NTSC" },
            { "PET64 PAL", "Educator 64 (PET 64) PAL" },
            { "PET64 NTSC", "Educator 64 (PET 64) NTSC" },
            { "C64 GS PAL", "C64 Games System PAL" },
            { "C64 JAP NTSC", "C64 Japanese NTSC" },
#if 0
            { "C64 OLD PAL", NULL },
            { "C64 PAL N", NULL },
            { "C64 OLD NTSC", NULL },
#endif
            { NULL, NULL },
         },
         "C64 PAL auto"
      },
#if defined(__XSCPU64__)
      {
         "vice_supercpu_simm_size",
         "System > SuperCPU SIMM Size",
         "Changing while running resets the system!",
         {
            { "0", "disabled" },
            { "1", "1024kB" },
            { "2", "2048kB" },
            { "4", "4096kB" },
            { "8", "8192kB" },
            { "16", "16384kB" },
            { NULL, NULL },
         },
         "16"
      },
#else
      {
         "vice_ram_expansion_unit",
         "System > RAM Expansion Unit",
         "Changing while running resets the system!",
         {
            { "none", "disabled" },
            { "128kB", "128kB (1700)" },
            { "256kB", "256kB (1764)" },
            { "512kB", "512kB (1750)" },
            { "1024kB", "1024kB" },
            { "2048kB", "2048kB" },
            { "4096kB", "4096kB" },
            { "8192kB", "8192kB" },
            { "16384kB", "16384kB" },
            { NULL, NULL },
         },
         "none"
      },
#endif /* __XSCPU64__ */
#endif
#if defined(__XSCPU64__)
      {
         "vice_supercpu_speed_switch",
         "System > SuperCPU Speed Switch",
         "",
         {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
         },
         "enabled"
      },
      {
         "vice_supercpu_kernal",
         "System > SuperCPU Kernal",
         "JiffyDOS does not work with the internal kernal! ROMs required in 'system/vice/SCPU64':\n- 'scpu-dos-1.4.bin'\n- 'scpu-dos-2.04.bin'",
         {
            { "0", "Internal" },
            { "1", "1.40" },
            { "2", "2.04" },
            { NULL, NULL },
         },
         "0"
      },
#endif
#if defined(__X64__) || defined(__X64SC__) || defined(__X128__) || defined(__XSCPU64__)
      {
         "vice_jiffydos",
         "System > JiffyDOS",
#if defined(__X64__) || defined(__X64SC__) || defined(__XSCPU64__)
         "'True Drive Emulation' & 1541/1571/1581 drive & ROMs required in 'system/vice':\n- 'JiffyDOS_C64.bin'\n- 'JiffyDOS_1541-II.bin'\n- 'JiffyDOS_1571_repl310654.bin'\n- 'JiffyDOS_1581.bin'",
#elif defined(__X128__)
         "'True Drive Emulation' & 1541/1571/1581 drive & ROMs required in 'system/vice':\n- 'JiffyDOS_C128.bin'\n- 'JiffyDOS_C64.bin' (GO64)\n- 'JiffyDOS_1541-II.bin'\n- 'JiffyDOS_1571_repl310654.bin'\n- 'JiffyDOS_1581.bin'",
#endif
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
         "System > Read 'vicerc'",
         "Process first found 'vicerc' in this order:\n1. 'saves/[content].vicerc'\n2. 'saves/vicerc'\n3. 'system/vice/vicerc'",
         {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
         },
         "enabled"
      },
#if !defined(__X64DTV__)
      {
         "vice_reset",
         "System > Reset Type",
         "- 'Autostart' hard resets and reruns content.\n- 'Soft' keeps some code in memory.\n- 'Hard' erases all memory.\n- 'Freeze' is for cartridges.",
         {
            { "autostart", "Autostart" },
            { "soft", "Soft" },
            { "hard", "Hard" },
            { "freeze", "Freeze" },
            { NULL, NULL },
         },
         "autostart"
      },
#endif
#if !defined(__XPET__) && !defined(__X64DTV__)
      /* Sublabel and options filled dynamically in retro_set_environment() */
      {
         "vice_cartridge",
         "Media > Cartridge",
         "",
         {
            { NULL, NULL },
         },
         NULL
      },
#endif
#if !defined(__X64DTV__)
      {
         "vice_autostart",
         "Media > Autostart",
         "'ON' always runs content, 'OFF' runs only PRG/CRT, 'Warp' turns warp mode on during autostart loading.",
         {
            { "disabled", NULL },
            { "enabled", NULL },
            { "warp", "Warp" },
            { NULL, NULL },
         },
         "enabled"
      },
      {
         "vice_autoloadwarp",
         "Media > Automatic Load Warp",
         "Toggle warp mode during disk and/or tape access. Mutes 'Drive Sound Emulation'.\n'True Drive Emulation' required!",
         {
            { "disabled", NULL },
            { "enabled", NULL },
            { "disk", "Disk only" },
            { "tape", "Tape only" },
            { NULL, NULL },
         },
         "disabled"
      },
      {
         "vice_drive_true_emulation",
         "Media > True Drive Emulation",
         "Loads much slower, but some games need it.\nRequired for 'JiffyDOS', 'Automatic Load Warp' and LED driver interface!",
         {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
         },
         "enabled"
      },
      {
         "vice_virtual_device_traps",
         "Media > Virtual Device Traps",
         "Required for printer device, but causes loading issues on rare cases. Enabled forcefully by disabling 'True Drive Emulation'.",
         {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
         },
         "disabled"
      },
      {
         "vice_floppy_write_protection",
         "Media > Floppy Write Protection",
         "Set device 8 read only.",
         {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
         },
         "disabled"
      },
#if defined(__X64__) || defined(__X64SC__) || defined(__X128__)
      {
         "vice_easyflash_write_protection",
         "Media > EasyFlash Write Protection",
         "Set EasyFlash cartridges read only.",
         {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
         },
         "disabled"
      },
#endif
      {
         "vice_work_disk",
         "Media > Global Work Disk",
         "Global disk in device 8 is only inserted when core is started without content.",
         {
            { "disabled", NULL },
            { "8_d64", "D64 - 664 blocks, 170kB - Device 8" },
            { "9_d64", "D64 - 664 blocks, 170kB - Device 9" },
            { "8_d71", "D71 - 1328 blocks, 340kB - Device 8" },
            { "9_d71", "D71 - 1328 blocks, 340kB - Device 9" },
            { "8_d81", "D81 - 3160 blocks, 800kB - Device 8" },
            { "9_d81", "D81 - 3160 blocks, 800kB - Device 9" },
            { NULL, NULL },
         },
         "disabled"
      },
#endif /* !defined(__X64DTV__) */
      {
         "vice_video_options_display",
         "Show Video Options",
         "Page refresh by menu toggle required!",
         {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
         },
         "disabled"
      },
#if defined(__X64__) || defined(__X64SC__) || defined(__X64DTV__) || defined(__X128__) || defined(__XSCPU64__) || defined(__XCBM5x0__) || defined(__XVIC__) || defined(__XPLUS4__)
      {
         "vice_aspect_ratio",
         "Video > Pixel Aspect Ratio",
         "Hotkey toggling disables this option until core restart.",
         {
            { "auto", "Automatic" },
            { "pal", "PAL" },
            { "ntsc", "NTSC" },
            { "raw", "1:1" },
            { NULL, NULL },
         },
         "auto"
      },
      {
         "vice_zoom_mode",
         "Video > Zoom Mode",
         "Crop borders to fit various host screens. Requirements in RetroArch settings:\n- Aspect Ratio: Core provided,\n- Integer Scale: Off.",
         {
            { "disabled", NULL },
            { "small", "Small" },
            { "medium", "Medium" },
            { "maximum", "Maximum" },
            { "manual", "Manual" },
            { NULL, NULL },
         },
         "disabled"
      },
      {
         "vice_zoom_mode_crop",
         "Video > Zoom Mode Crop",
         "'Horizontal + Vertical' & 'Maximum' removes borders completely. Ignored with 'Manual' zoom.",
         {
            { "both", "Horizontal + Vertical" },
            { "horizontal", "Horizontal" },
            { "vertical", "Vertical" },
            { "16:9", "16:9" },
            { "16:10", "16:10" },
            { "4:3", "4:3" },
            { "5:4", "5:4" },
            { NULL, NULL },
         },
         "both"
      },
      {
         "vice_manual_crop_top",
         "Video > Manual Crop Top",
#if defined(__X64__) || defined(__X64SC__) || defined(__X64DTV__) || defined(__X128__) || defined(__XSCPU64__) || defined(__XCBM5x0__)
         "VIC-II top border height:\n- 35px PAL\n- 23px NTSC",
#elif defined(__XVIC__)
         "VIC top border height:\n- 48px PAL\n- 22px NTSC",
#elif defined(__XPLUS4__)
         "TED top border height:\n- 40px PAL\n- 18px NTSC",
#endif
         MANUAL_CROP_OPTIONS,
         "0",
      },
      {
         "vice_manual_crop_bottom",
         "Video > Manual Crop Bottom",
#if defined(__X64__) || defined(__X64SC__) || defined(__X64DTV__) || defined(__X128__) || defined(__XSCPU64__) || defined(__XCBM5x0__)
         "VIC-II bottom border height:\n- 37px PAL\n- 24px NTSC",
#elif defined(__XVIC__)
         "VIC bottom border height:\n- 52px PAL\n- 28px NTSC",
#elif defined(__XPLUS4__)
         "TED bottom border height:\n- 48px PAL\n- 24px NTSC",
#endif
         MANUAL_CROP_OPTIONS,
         "0",
      },
      {
         "vice_manual_crop_left",
         "Video > Manual Crop Left",
#if defined(__X64__) || defined(__X64SC__) || defined(__X64DTV__) || defined(__X128__) || defined(__XSCPU64__) || defined(__XCBM5x0__)
         "VIC-II left border width:\n- 32px",
#elif defined(__XVIC__)
         "VIC left border width:\n- 48px PAL\n- 32px NTSC",
#elif defined(__XPLUS4__)
         "TED left border width:\n- 32px",
#endif
         MANUAL_CROP_OPTIONS,
         "0",
      },
      {
         "vice_manual_crop_right",
         "Video > Manual Crop Right",
#if defined(__X64__) || defined(__X64SC__) || defined(__X64DTV__) || defined(__X128__) || defined(__XSCPU64__) || defined(__XCBM5x0__)
         "VIC-II right border width:\n- 32px",
#elif defined(__XVIC__)
         "VIC right border width:\n- 48px PAL\n- 16px NTSC",
#elif defined(__XPLUS4__)
         "TED right border width:\n- 32px",
#endif
         MANUAL_CROP_OPTIONS,
         "0",
      },
#endif
      {
         "vice_statusbar",
         "Video > Statusbar Mode",
         "- 'Full': Joyports + Current image + LEDs\n- 'Basic': Current image + LEDs\n- 'Minimal': Track number + FPS hidden",
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
         "Video > Virtual KBD Theme",
         "By default, the keyboard comes up with RetroPad Select.",
         {
            { "auto", "Automatic (shadow)" },
            { "auto_outline", "Automatic (outline)" },
            { "brown", "Brown (shadow)" },
            { "brown_outline", "Brown (outline)" },
            { "beige", "Beige (shadow)" },
            { "beige_outline", "Beige (outline)" },
            { "dark", "Dark (shadow)" },
            { "dark_outline", "Dark (outline)" },
            { "light", "Light (shadow)" },
            { "light_outline", "Light (outline)" },
            { NULL, NULL },
         },
         "auto"
      },
      {
         "vice_vkbd_transparency",
         "Video > Virtual KBD Transparency",
         "Keyboard transparency can be toggled with RetroPad A.",
         {
            { "0%",   NULL },
            { "25%",  NULL },
            { "50%",  NULL },
            { "75%",  NULL },
            { "100%", NULL },
            { NULL, NULL },
         },
         "25%"
      },
      {
         "vice_gfx_colors",
         "Video > Color Depth",
         "24-bit is slower and not available on all platforms. Full restart required.",
         {
            { "16bit", "Thousands (16-bit)" },
            { "24bit", "Millions (24-bit)" },
            { NULL, NULL },
         },
         "16bit"
      },
      {
         "vice_joyport_pointer_color",
         "Video > Light Pen/Gun Pointer Color",
         "Crosshair color for light pens and guns.",
         {
            { "disabled", NULL },
            { "black", "Black" },
            { "white", "White" },
            { "red", "Red" },
            { "green", "Green" },
            { "blue", "Blue" },
            { "yellow", "Yellow" },
            { "purple", "Purple" },
            { NULL, NULL },
         },
         "blue"
      },
      {
#if defined(__X64__) || defined(__X64SC__) || defined(__X64DTV__) || defined(__X128__) || defined(__XSCPU64__) || defined(__XCBM5x0__)
         "vice_vicii_filter",
         "Video > VIC-II Filter",
#elif defined(__XVIC__)
         "vice_vic_filter",
         "Video > VIC Filter",
#elif defined(__XPLUS4__)
         "vice_ted_filter",
         "Video > TED Filter",
#elif defined(__XPET__) || defined(__XCBM2__)
         "vice_crtc_filter",
         "Video > CRTC Filter",
#endif
         "CRT emulation filter with custom horizontal blur.",
         {
            { "disabled", NULL },
            { "enabled", NULL },
            { "enabled_medblur", "50% blur" },
            { "enabled_lowblur", "10% blur" },
         },
         "disabled"
      },
#if defined(__XVIC__)
      {
         "vice_vic20_external_palette",
         "Video > VIC Color Palette",
         "'Colodore' is recommended for the most accurate colors.",
         {
            { "default", "Internal" },
            { "colodore_vic", "Colodore" },
            { "mike-pal", "Mike (PAL)" },
            { "mike-ntsc", "Mike (NTSC)" },
            { "vice", "VICE" },
            { NULL, NULL },
         },
         "colodore_vic"
      },
#elif defined(__XPLUS4__)
      {
         "vice_plus4_external_palette",
         "Video > TED Color Palette",
         "'Colodore' is recommended for the most accurate colors.",
         {
            { "default", "Internal" },
            { "colodore_ted", "Colodore" },
            { "yape-pal", "Yape (PAL)" },
            { "yape-ntsc", "Yape (NTSC)" },
            { NULL, NULL },
         },
         "colodore_ted"
      },
#elif defined(__XPET__)
      {
         "vice_pet_external_palette",
         "Video > CRTC Color Palette",
         "",
         {
            { "default", "Internal" },
            { "green", "Green" },
            { "amber", "Amber" },
            { "white", "White" },
            { NULL, NULL },
         },
         "default"
      },
#elif defined(__XCBM2__)
      {
         "vice_cbm2_external_palette",
         "Video > CRTC Color Palette",
         "",
         {
            { "default", "Internal" },
            { "green", "Green" },
            { "amber", "Amber" },
            { "white", "White" },
            { NULL, NULL },
         },
         "default"
      },
#elif !defined(__X64DTV__)
      {
         "vice_external_palette",
         "Video > VIC-II Color Palette",
         "'Colodore' is recommended for most accurate colors.",
         {
            { "default", "Internal" },
            { "colodore", "Colodore" },
            { "pepto-pal", "Pepto (PAL)" },
#if 0
            { "pepto-palold", "Pepto (old PAL)" },
#endif
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
         "colodore"
      },
#endif
#if defined(__X64__) || defined(__X64SC__) || defined(__X64DTV__) || defined(__X128__) || defined(__XSCPU64__) || defined(__XCBM5x0__) || defined(__XVIC__) || defined(__XPLUS4__)
      {
#if defined(__X64__) || defined(__X64SC__) || defined(__X64DTV__) || defined(__X128__) || defined(__XSCPU64__) || defined(__XCBM5x0__)
         "vice_vicii_color_gamma",
         "Video > VIC-II Color Gamma",
#elif defined(__XVIC__)
         "vice_vic_color_gamma",
         "Video > VIC Color Gamma",
#elif defined(__XPLUS4__)
         "vice_ted_color_gamma",
         "Video > TED Color Gamma",
#endif
         "Gamma for the internal palette.",
         PALETTE_GAMMA_OPTIONS,
         "2800"
      },
      {
#if defined(__X64__) || defined(__X64SC__) || defined(__X64DTV__) || defined(__X128__) || defined(__XSCPU64__) || defined(__XCBM5x0__)
         "vice_vicii_color_brightness",
         "Video > VIC-II Color Brightness",
#elif defined(__XVIC__)
         "vice_vic_color_brightness",
         "Video > VIC Color Brightness",
#elif defined(__XPLUS4__)
         "vice_ted_color_brightness",
         "Video > TED Color Brightness",
#endif
         "Brightness for the internal palette.",
         PALETTE_COLOR_OPTIONS,
         "1000"
      },
      {
#if defined(__X64__) || defined(__X64SC__) || defined(__X64DTV__) || defined(__X128__) || defined(__XSCPU64__) || defined(__XCBM5x0__)
         "vice_vicii_color_contrast",
         "Video > VIC-II Color Contrast",
#elif defined(__XVIC__)
         "vice_vic_color_contrast",
         "Video > VIC Color Contrast",
#elif defined(__XPLUS4__)
         "vice_ted_color_contrast",
         "Video > TED Color Contrast",
#endif
         "Contrast for the internal palette.",
         PALETTE_COLOR_OPTIONS,
         "1000"
      },
      {
#if defined(__X64__) || defined(__X64SC__) || defined(__X64DTV__) || defined(__X128__) || defined(__XSCPU64__) || defined(__XCBM5x0__)
         "vice_vicii_color_saturation",
         "Video > VIC-II Color Saturation",
#elif defined(__XVIC__)
         "vice_vic_color_saturation",
         "Video > VIC Color Saturation",
#elif defined(__XPLUS4__)
         "vice_ted_color_saturation",
         "Video > TED Color Saturation",
#endif
         "Saturation for the internal palette.",
         PALETTE_COLOR_OPTIONS,
         "1000"
      },
      {
#if defined(__X64__) || defined(__X64SC__) || defined(__X64DTV__) || defined(__X128__) || defined(__XSCPU64__) || defined(__XCBM5x0__)
         "vice_vicii_color_tint",
         "Video > VIC-II Color Tint",
#elif defined(__XVIC__)
         "vice_vic_color_tint",
         "Video > VIC Color Tint",
#elif defined(__XPLUS4__)
         "vice_ted_color_tint",
         "Video > TED Color Tint",
#endif
         "Tint for the internal palette.",
         PALETTE_COLOR_OPTIONS,
         "1000"
      },
#endif
      {
         "vice_audio_options_display",
         "Show Audio Options",
         "Page refresh by menu toggle required!",
         {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
         },
         "disabled"
      },
      {
         "vice_drive_sound_emulation",
         "Audio > Drive Sound Emulation",
         "'True Drive Emulation' & D64/D71 disk image required.",
         {
            { "disabled", NULL },
            { "5%", NULL },
            { "10%", NULL },
            { "15%", NULL },
            { "20%", NULL },
            { "25%", NULL },
            { "30%", NULL },
            { "35%", NULL },
            { "40%", NULL },
            { "45%", NULL },
            { "50%", NULL },
            { "55%", NULL },
            { "60%", NULL },
            { "65%", NULL },
            { "70%", NULL },
            { "75%", NULL },
            { "80%", NULL },
            { "85%", NULL },
            { "90%", NULL },
            { "95%", NULL },
            { "100%", NULL },
            { NULL, NULL },
         },
         "20%"
      },
      {
         "vice_datasette_sound",
         "Audio > Datasette Sound",
         "TAP tape image required.",
         {
            { "disabled", NULL },
            { "5%", NULL },
            { "10%", NULL },
            { "15%", NULL },
            { "20%", NULL },
            { "25%", NULL },
            { "30%", NULL },
            { "35%", NULL },
            { "40%", NULL },
            { "45%", NULL },
            { "50%", NULL },
            { "55%", NULL },
            { "60%", NULL },
            { "65%", NULL },
            { "70%", NULL },
            { "75%", NULL },
            { "80%", NULL },
            { "85%", NULL },
            { "90%", NULL },
            { "95%", NULL },
            { "100%", NULL },
            { "-1", "100% + Mute" },
            { NULL, NULL },
         },
         "disabled"
      },
#if defined(__X64__) || defined(__X64SC__) || defined(__X64DTV__) || defined(__X128__) || defined(__XSCPU64__) || defined(__XCBM5x0__) || defined(__XVIC__) || defined(__XPLUS4__)
      {
         "vice_audio_leak_emulation",
#if defined(__X64__) || defined(__X64SC__) || defined(__X64DTV__) || defined(__X128__) || defined(__XSCPU64__) || defined(__XCBM5x0__)
         "Audio > VIC-II Audio Leak Emulation",
#elif defined(__XVIC__)
         "Audio > VIC Audio Leak Emulation",
#elif defined(__XPLUS4__)
         "Audio > TED Audio Leak Emulation",
#endif
         "",
         {
            { "disabled", NULL },
            { "1", "100%" },
            { "2", "200%" },
            { "3", "300%" },
            { "4", "400%" },
            { "5", "500%" },
            { "6", "600%" },
            { "7", "700%" },
            { "8", "800%" },
            { "9", "900%" },
            { "10", "1000%" },
            { NULL, NULL },
         },
         "disabled"
      },
#endif
#if !defined(__XPET__) && !defined(__XPLUS4__) && !defined(__XVIC__)
      {
         "vice_sid_engine",
         "Audio > SID Engine",
#if defined(__X64DTV__)
         "'ReSID-DTV' is accurate, 'FastSID' is the last resort.",
#else
         "'ReSID' is accurate, 'ReSID-FP' is more accurate, 'FastSID' is the last resort.",
#endif
         {
            { "FastSID", NULL },
#if defined(__X64DTV__)
            { "ReSID", "ReSID-DTV" },
#else
            { "ReSID", NULL },
#endif
#if defined(__X64__) || defined(__X64SC__) || defined(__XSCPU64__) || defined(__X128__)
#ifdef HAVE_RESID33
            { "ReSID-3.3", NULL },
#endif
            { "ReSID-FP", NULL },
#endif
            { NULL, NULL },
         },
         "ReSID"
      },
#if !defined(__X64DTV__)
      {
         "vice_sid_model",
         "Audio > SID Model",
         "C64 has '6581', C64C has '8580'.",
         {
            { "default", "Default" },
            { "6581", NULL },
            { "8580", NULL },
            { "8580RD", "8580 ReSID + digi boost" },
            { NULL, NULL },
         },
         "default"
      },
      {
         "vice_sid_extra",
         "Audio > SID Extra",
         "Second SID base address.",
         {
            { "disabled", NULL },
            { "0xd420", "$D420" },
            { "0xd500", "$D500" },
            { "0xde00", "$DE00" },
            { "0xdf00", "$DF00" },
            { NULL, NULL },
         },
         "disabled"
      },
#endif
      {
         "vice_resid_sampling",
         "Audio > ReSID Sampling",
         "'Resampling' provides best quality. 'Fast' improves performance dramatically on PS Vita.",
         {
            { "fast", "Fast" },
            { "interpolation", "Interpolation" },
            { "fast resampling", "Fast Resampling" },
            { "resampling", "Resampling" },
            { NULL, NULL },
         },
#if defined(__X64__) || defined(__XCBM5x0__) || defined(__XCBM2__) || defined(PSP) || defined(VITA) || defined(__SWITCH__) || defined(DINGUX) || defined(ANDROID)
         "fast"
#else
         "resampling"
#endif
      },
      {
         "vice_resid_passband",
         "Audio > ReSID Filter Passband",
         "",
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
         "Audio > ReSID Filter Gain",
         "",
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
         "Audio > ReSID Filter 6581 Bias",
         "",
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
         "Audio > ReSID Filter 8580 Bias",
         "",
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
#if defined(__X64__) || defined(__X64SC__) || defined(__X128__)
      {
         "vice_sfx_sound_expander",
         "Audio > SFX Sound Expander",
         "",
         {
            { "disabled", NULL },
            { "3526", "YM3526" },
            { "3812", "YM3812" },
            { NULL, NULL },
         },
         "disabled"
      },
#endif
      {
         "vice_sound_sample_rate",
         "Audio > Output Sample Rate",
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
#if !defined(__XPET__) && !defined(__XCBM2__)
      {
         "vice_analogmouse_deadzone",
         "Input > Analog Stick Mouse Deadzone",
         "",
         {
            { "0", "0%" },
            { "5", "5%" },
            { "10", "10%" },
            { "15", "15%" },
            { "20", "20%" },
            { "25", "25%" },
            { "30", "30%" },
            { "35", "35%" },
            { "40", "40%" },
            { "45", "45%" },
            { "50", "50%" },
            { NULL, NULL },
         },
         "20"
      },
      {
         "vice_analogmouse_speed",
         "Input > Analog Stick Mouse Speed",
         "",
         {
            { "0.5", "50%" },
            { "0.6", "60%" },
            { "0.7", "70%" },
            { "0.8", "80%" },
            { "0.9", "90%" },
            { "1.0", "100%" },
            { "1.1", "110%" },
            { "1.2", "120%" },
            { "1.3", "130%" },
            { "1.4", "140%" },
            { "1.5", "150%" },
            { "1.6", "160%" },
            { "1.7", "170%" },
            { "1.8", "180%" },
            { "1.9", "190%" },
            { "2.0", "200%" },
            { NULL, NULL },
         },
         "1.0"
      },
      {
         "vice_dpadmouse_speed",
         "Input > D-Pad Mouse Speed",
         "",
         {
            { "3", "50%" },
            { "4", "66%" },
            { "5", "83%" },
            { "6", "100%" },
            { "7", "116%" },
            { "8", "133%" },
            { "9", "150%" },
            { "10", "166%" },
            { "11", "183%" },
            { "12", "200%" },
            { NULL, NULL },
         },
         "6"
      },
      {
         "vice_mouse_speed",
         "Input > Mouse Speed",
         "Affects mouse speed globally.",
         {
            { "10", "10%" },
            { "20", "20%" },
            { "30", "30%" },
            { "40", "40%" },
            { "50", "50%" },
            { "60", "60%" },
            { "70", "70%" },
            { "80", "80%" },
            { "90", "90%" },
            { "100", "100%" },
            { "110", "110%" },
            { "120", "120%" },
            { "130", "130%" },
            { "140", "140%" },
            { "150", "150%" },
            { "160", "160%" },
            { "170", "170%" },
            { "180", "180%" },
            { "190", "190%" },
            { "200", "200%" },
            { NULL, NULL },
         },
         "100"
      },
#endif
#if !defined(__XCBM5x0__)
      {
         "vice_userport_joytype",
         "Input > Userport Joystick Adapter",
         "Required for more than 2 joysticks, for example IK+ Gold with 3 players.",
         {
            { "disabled", NULL },
            { "CGA", "Protovision / Classical Games" },
            { "HIT", "Digital Excess & Hitmen" },
            { "Kingsoft", "Kingsoft" },
            { "Starbyte", "Starbyte" },
            { "Hummer", "C64DTV Hummer" },
            { "OEM", "VIC-20 OEM" },
            { "PET", "PET" },
            { NULL, NULL },
         },
         "disabled"
      },
#endif
#if !defined(__XPET__) && !defined(__XCBM2__)
      {
         "vice_keyrah_keypad_mappings",
         "Input > Keyrah Keypad Mappings",
         "Hardcoded keypad to joyport mappings for Keyrah hardware.",
         {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
         },
         "disabled"
      },
#endif
#if !defined(__XPET__) && !defined(__XCBM2__) && !defined(__XCBM5x0__)
      {
         "vice_keyboard_keymap",
         "Input > Keyboard Keymap",
#if defined(__XPLUS4__)
         "User-defined keymaps go in 'system/vice/PLUS4'.\n- Positional: 'sdl_pos.vkm'\n- Symbolic: 'sdl_sym.vkm'",
#elif defined(__XVIC__)
         "User-defined keymaps go in 'system/vice/VIC20'.\n- Positional: 'sdl_pos.vkm'\n- Symbolic: 'sdl_sym.vkm'",
#elif defined(__X128__)
         "User-defined keymaps go in 'system/vice/C128'.\n- Positional: 'sdl_pos.vkm'\n- Symbolic: 'sdl_sym.vkm'",
#elif defined(__XSCPU64__)
         "User-defined keymaps go in 'system/vice/SCPU64'.\n- Positional: 'sdl_pos.vkm'\n- Symbolic: 'sdl_sym.vkm'",
#else
         "User-defined keymaps go in 'system/vice/C64'.\n- Positional: 'sdl_pos.vkm'\n- Symbolic: 'sdl_sym.vkm'",
#endif
         {
            { "positional", "Positional" },
            { "symbolic", "Symbolic" },
            { "positional-user", "Positional (User-defined)" },
            { "symbolic-user", "Symbolic (User-defined)" },
            { NULL, NULL },
         },
         "positional"
      },
#endif
      {
         "vice_physical_keyboard_pass_through",
         "Input > Keyboard Pass-through",
         "'ON' passes all physical keyboard events to the core. 'OFF' prevents RetroPad keys from generating keyboard events.",
         {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
         },
         "disabled"
      },
      {
         "vice_datasette_hotkeys",
         "Input > Datasette Hotkeys",
         "Toggle all Datasette hotkeys.",
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
         "Page refresh by menu toggle required!",
         {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
         },
         "enabled"
      },
      /* Hotkeys */
      {
         "vice_mapper_vkbd",
         "Hotkey > Toggle Virtual Keyboard",
         "Press the mapped key to toggle the virtual keyboard.",
         {{ NULL, NULL }},
         "---"
      },
      {
         "vice_mapper_statusbar",
         "Hotkey > Toggle Statusbar",
         "Press the mapped key to toggle the statusbar.",
         {{ NULL, NULL }},
         "RETROK_F12"
      },
#if !defined(__XPET__) && !defined(__XCBM2__) && !defined(__XVIC__)
      {
         "vice_mapper_joyport_switch",
         "Hotkey > Switch Joyport",
         "Press the mapped key to switch joyports 1 & 2.\nSwitching disables 'RetroPad Port' option until core restart.",
         {{ NULL, NULL }},
         "RETROK_RCTRL"
      },
#endif
      {
         "vice_mapper_reset",
         "Hotkey > Reset",
         "Press the mapped key to trigger the selected 'Reset Type'.",
         {{ NULL, NULL }},
         "RETROK_END"
      },
      {
         "vice_mapper_warp_mode",
         "Hotkey > Hold Warp Mode",
         "Hold the mapped key for warp mode.",
         {{ NULL, NULL }},
         ""
      },
#if defined(__X64__) || defined(__X64SC__) || defined(__X64DTV__) || defined(__X128__) || defined(__XSCPU64__) || defined(__XCBM5x0__) || defined(__XVIC__) || defined(__XPLUS4__)
      {
         "vice_mapper_aspect_ratio_toggle",
         "Hotkey > Toggle Aspect Ratio",
         "Press the mapped key to toggle aspect ratio.\nToggling disables 'Pixel Aspect Ratio' option until core restart.",
         {{ NULL, NULL }},
         "---"
      },
      {
         "vice_mapper_zoom_mode_toggle",
         "Hotkey > Toggle Zoom Mode",
         "Press the mapped key to toggle zoom mode.",
         {{ NULL, NULL }},
         "---"
      },
#endif
      /* Datasette controls */
      {
         "vice_mapper_datasette_toggle_hotkeys",
         "Hotkey > Toggle Datasette Hotkeys",
         "Press the mapped key to toggle Datasette hotkeys.",
         {{ NULL, NULL }},
         "---"
      },
      {
         "vice_mapper_datasette_start",
         "Hotkey > Datasette Start",
         "Press start on tape.",
         {{ NULL, NULL }},
         "RETROK_UP"
      },
      {
         "vice_mapper_datasette_stop",
         "Hotkey > Datasette Stop",
         "Press stop on tape.",
         {{ NULL, NULL }},
         "RETROK_DOWN"
      },
      {
         "vice_mapper_datasette_rewind",
         "Hotkey > Datasette Rewind",
         "Press rewind on tape.",
         {{ NULL, NULL }},
         "RETROK_LEFT"
      },
      {
         "vice_mapper_datasette_forward",
         "Hotkey > Datasette Fast Forward",
         "Press fast forward on tape.",
         {{ NULL, NULL }},
         "RETROK_RIGHT"
      },
      {
         "vice_mapper_datasette_reset",
         "Hotkey > Datasette Reset",
         "Press reset on tape.",
         {{ NULL, NULL }},
         "---"
      },
      /* Button mappings */
      {
         "vice_mapper_select",
         "RetroPad > Select",
         "",
         {{ NULL, NULL }},
         "TOGGLE_VKBD"
      },
      {
         "vice_mapper_start",
         "RetroPad > Start",
         "VKBD: Press 'Return'. Remapping to non-keyboard keys overrides VKBD function!",
         {{ NULL, NULL }},
         "---"
      },
      {
         "vice_mapper_b",
         "RetroPad > B",
         "Unmapped defaults to fire button.\nVKBD: Press selected key.",
         {{ NULL, NULL }},
         "---"
      },
      {
         "vice_mapper_a",
         "RetroPad > A",
         "VKBD: Toggle transparency. Remapping to non-keyboard keys overrides VKBD function!",
         {{ NULL, NULL }},
         "---"
      },
      {
         "vice_mapper_y",
         "RetroPad > Y",
         "VKBD: Toggle 'ShiftLock'. Remapping to non-keyboard keys overrides VKBD function!",
         {{ NULL, NULL }},
         "---"
      },
      {
         "vice_mapper_x",
         "RetroPad > X",
         "VKBD: Press 'Space'. Remapping to non-keyboard keys overrides VKBD function!",
         {{ NULL, NULL }},
         "RETROK_SPACE"
      },
      {
         "vice_mapper_l",
         "RetroPad > L",
         "",
         {{ NULL, NULL }},
         "---"
      },
      {
         "vice_mapper_r",
         "RetroPad > R",
         "",
         {{ NULL, NULL }},
         "---"
      },
      {
         "vice_mapper_l2",
         "RetroPad > L2",
         "",
         {{ NULL, NULL }},
         "RETROK_ESCAPE"
      },
      {
         "vice_mapper_r2",
         "RetroPad > R2",
         "",
         {{ NULL, NULL }},
         "RETROK_RETURN"
      },
      {
         "vice_mapper_l3",
         "RetroPad > L3",
         "",
         {{ NULL, NULL }},
         "---"
      },
      {
         "vice_mapper_r3",
         "RetroPad > R3",
         "",
         {{ NULL, NULL }},
         "---"
      },
      /* Left Stick */
      {
         "vice_mapper_lu",
         "RetroPad > Left Analog > Up",
         "",
         {{ NULL, NULL }},
         "---"
      },
      {
         "vice_mapper_ld",
         "RetroPad > Left Analog > Down",
         "",
         {{ NULL, NULL }},
         "---"
      },
      {
         "vice_mapper_ll",
         "RetroPad > Left Analog > Left",
         "",
         {{ NULL, NULL }},
         "---"
      },
      {
         "vice_mapper_lr",
         "RetroPad > Left Analog > Right",
         "",
         {{ NULL, NULL }},
         "---"
      },
      /* Right Stick */
      {
         "vice_mapper_ru",
         "RetroPad > Right Analog > Up",
         "",
         {{ NULL, NULL }},
         "---"
      },
      {
         "vice_mapper_rd",
         "RetroPad > Right Analog > Down",
         "",
         {{ NULL, NULL }},
         "---"
      },
      {
         "vice_mapper_rl",
         "RetroPad > Right Analog > Left",
         "",
         {{ NULL, NULL }},
         "---"
      },
      {
         "vice_mapper_rr",
         "RetroPad > Right Analog > Right",
         "",
         {{ NULL, NULL }},
         "---"
      },
#if !defined(__XPET__) && !defined(__XCBM2__)
      /* Turbo Fire */
      {
         "vice_turbo_fire",
         "RetroPad > Turbo Fire",
         "Hotkey toggling disables this option until core restart.",
         {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
         },
         "disabled"
      },
      {
         "vice_turbo_fire_button",
         "RetroPad > Turbo Button",
         "Replace the mapped button with turbo fire button.",
         {
            { "B", "RetroPad B" },
            { "A", "RetroPad A" },
            { "Y", "RetroPad Y" },
            { "X", "RetroPad X" },
            { "L", "RetroPad L" },
            { "R", "RetroPad R" },
            { "L2", "RetroPad L2" },
            { "R2", "RetroPad R2" },
            { NULL, NULL },
         },
         "B"
      },
      {
         "vice_turbo_pulse",
         "RetroPad > Turbo Pulse",
         "Frames in a button cycle.\n- '2' = 1 frame down, 1 frame up\n- '4' = 2 frames down, 2 frames up\n- '6' = 3 frames down, 3 frames up\netc.",
         {
            { "2", "2 frames" },
            { "4", "4 frames" },
            { "6", "6 frames" },
            { "8", "8 frames" },
            { "10", "10 frames" },
            { "12", "12 frames" },
            { NULL, NULL },
         },
         "6"
      },
#endif
#if !defined(__XPET__) && !defined(__XCBM2__) && !defined(__XVIC__)
      {
         "vice_joyport",
         "RetroPad > Port",
         "Most games use port 2, some use port 1.\nFilename forcing or hotkey toggling disables this option until core restart.",
         {
            { "1", "Port 1" },
            { "2", "Port 2" },
            { NULL, NULL },
         },
         "2"
      },
#endif
#if !defined(__XPET__) && !defined(__XCBM2__)
      {
         "vice_joyport_type",
         "RetroPad > Port Type",
         "Non-joysticks are plugged in current port only and are controlled with left analog stick or mouse. Paddles are split to 1st and 2nd RetroPort.",
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
            { "11", "Light Pen (Up trigger)" },
            { "12", "Light Pen (Left trigger)" },
            { "13", "Light Pen (Datel)" },
            { "16", "Light Pen (Inkwell)" },
            { "14", "Light Gun (Magnum Light Phaser)" },
            { "15", "Light Gun (Stack Light Rifle)" },
            { NULL, NULL },
         },
         "1"
      },
      {
         "vice_retropad_options",
         "RetroPad > Face Button Options",
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
#endif
      { NULL, NULL, NULL, {{0}}, NULL },
   };

   /* Fill in the values for all the mappers */
   int i = 0;
   int j = 0;
   int hotkey = 0;
   int hotkeys_skipped = 0;
   /* Count special hotkeys */
   while (retro_keys[j].value[0] && j < RETRO_NUM_CORE_OPTION_VALUES_MAX - 1)
   {
      if (retro_keys[j].id < 0)
         hotkeys_skipped++;
      ++j;
   }
   while (core_options[i].key) 
   {
      if (strstr(core_options[i].key, "vice_mapper_"))
      {
         /* Show different key list for hotkeys (special negatives removed) */
         if (  strstr(core_options[i].key, "vice_mapper_vkbd")
            || strstr(core_options[i].key, "vice_mapper_statusbar")
            || strstr(core_options[i].key, "vice_mapper_joyport_switch")
            || strstr(core_options[i].key, "vice_mapper_reset")
            || strstr(core_options[i].key, "vice_mapper_aspect_ratio_toggle")
            || strstr(core_options[i].key, "vice_mapper_zoom_mode_toggle")
            || strstr(core_options[i].key, "vice_mapper_warp_mode")
            || strstr(core_options[i].key, "vice_mapper_datasette_toggle_hotkeys")
            || strstr(core_options[i].key, "vice_mapper_datasette_start")
            || strstr(core_options[i].key, "vice_mapper_datasette_stop")
            || strstr(core_options[i].key, "vice_mapper_datasette_rewind")
            || strstr(core_options[i].key, "vice_mapper_datasette_forward")
            || strstr(core_options[i].key, "vice_mapper_datasette_reset"))
            hotkey = 1;
         else
            hotkey = 0;

         j = 0;
         if (hotkey)
         {
            while (retro_keys[j].value[0] && j < RETRO_NUM_CORE_OPTION_VALUES_MAX - 1)
            {
               if (j == 0) /* "---" unmapped */
               {
                  core_options[i].values[j].value = retro_keys[j].value;
                  core_options[i].values[j].label = retro_keys[j].label;
               }
               else
               {
                  core_options[i].values[j].value = retro_keys[j + hotkeys_skipped + 1].value;
                  core_options[i].values[j].label = retro_keys[j + hotkeys_skipped + 1].label;
               }
               ++j;
            }
         }
         else
         {
            while (retro_keys[j].value[0] && j < RETRO_NUM_CORE_OPTION_VALUES_MAX - 1)
            {
               core_options[i].values[j].value = retro_keys[j].value;
               core_options[i].values[j].label = retro_keys[j].label;
               ++j;
            }
         }
         core_options[i].values[j].value = NULL;
         core_options[i].values[j].label = NULL;
      }
      else if (!strcmp(core_options[i].key, "vice_cartridge"))
      {
         j = 0;
         core_options[i].values[0].value = "none";
         core_options[i].values[0].label = "disabled";
         ++j;

         DIR *cart_dir;
         struct dirent *cart_dirp;

         char machine_directory[RETRO_PATH_MAX] = {0};
         snprintf(machine_directory, sizeof(machine_directory), "%s%s%s",
               retro_system_data_directory, FSDEV_DIR_SEP_STR, machine_name);

         /* Scan system/vice/machine directory for cartridges */
         if (path_is_directory(machine_directory))
         {
            cart_dir = opendir(machine_directory);
            while ((cart_dirp = readdir(cart_dir)) != NULL && j < RETRO_NUM_CORE_OPTION_VALUES_MAX - 1)
            {
               /* Blacklisted */
               if (!strcmp(cart_dirp->d_name, "scpu-dos-1.4.bin") ||
                   !strcmp(cart_dirp->d_name, "scpu-dos-2.04.bin"))
                  continue;

               if (dc_get_image_type(cart_dirp->d_name) == DC_IMAGE_TYPE_MEM)
               {
                  char cart_value[RETRO_PATH_MAX] = {0};
                  char cart_label[50] = {0};
                  snprintf(cart_value, sizeof(cart_value), "%s", cart_dirp->d_name);
                  snprintf(cart_label, sizeof(cart_label), "%s", path_remove_extension(cart_dirp->d_name));

                  vice_carts[j].value = strdup(cart_value);
                  vice_carts[j].label = strdup(cart_label);

                  core_options[i].values[j].value = vice_carts[j].value;
                  core_options[i].values[j].label = vice_carts[j].label;
                  ++j;
               }

               vice_carts[j].value = NULL;
               vice_carts[j].label = NULL;
            }
            closedir(cart_dir);
         }

         core_options[i].values[j].value = NULL;
         core_options[i].values[j].label = NULL;

         /* Info sublabel */
         char info[100] = {0};
         snprintf(info, sizeof(info), "Cartridge images go in 'system/vice/%s'.\nChanging while running resets the system!", machine_name);
         vice_carts_info = strdup(info);
         core_options[i].info = vice_carts_info;
      }
      ++i;
   }

   unsigned version = 0;
   if (!cb(RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION, &version))
   {
      if (log_cb)
         log_cb(RETRO_LOG_DEBUG, "retro_set_environment: GET_CORE_OPTIONS_VERSION failed, not setting CORE_OPTIONS now.\n");
   }
   else if (version == 1)
   {
      cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS, core_options);
   }
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
            unsigned alloc_len = 0;
            struct retro_core_option_definition *o = core_options + NUM_CORE_OPTIONS - 1;
            struct retro_variable *rv = variables + NUM_CORE_OPTIONS - 1;
            for (; o >= core_options; --o, --rv)
            {
               int l = snprintf(0, 0, "%s; %s", o->desc, o->default_value);
               for (struct retro_core_option_value *v = o->values; v->value; ++v)
                  l += snprintf(0, 0, "|%s", v->value);
               alloc_len += l + 1;
            }
            buf = core_options_legacy_strings = (char *)malloc(alloc_len);
            buf_len = alloc_len;
         }
         /* Second pass: Fill string-buffer */
         struct retro_core_option_definition *o = core_options + NUM_CORE_OPTIONS - 1;
         struct retro_variable *rv = variables + NUM_CORE_OPTIONS;
         rv->key = rv->value = 0;
         --rv;
         for (; o >= core_options; --o, --rv)
         {
            int l = snprintf(buf, buf_len, "%s; %s", o->desc, o->default_value);
            for (struct retro_core_option_value *v = o->values; v->value; ++v)
               if (v->value != o->default_value)
                  l += snprintf(buf+l, buf_len, "|%s", v->value);
            rv->key = o->key;
            rv->value = buf;
            ++l;
            buf += l;
            buf_len -= l;
         }
      }
      cb(RETRO_ENVIRONMENT_SET_VARIABLES, variables);
#undef NUM_CORE_OPTIONS
   }

   static bool allowNoGameMode;
   allowNoGameMode = true;
   cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &allowNoGameMode);

   static struct retro_led_interface led_interface;
   cb(RETRO_ENVIRONMENT_GET_LED_INTERFACE, &led_interface);
   if (led_interface.set_led_state)
      led_state_cb = led_interface.set_led_state;
}

int log_resources_set_int(const char *name, int value)
{
    log_cb(RETRO_LOG_INFO, "Set resource: %s => %d\n", name, value);
    return resources_set_int(name, value);
}

int log_resources_set_string(const char *name, const char* value)
{
    log_cb(RETRO_LOG_INFO, "Set resource: %s => \"%s\"\n", name, value);
    return resources_set_string(name, value);
}

static void update_variables(void)
{
   struct retro_variable var;
   struct retro_core_option_display option_display;

#ifdef RETRO_DEBUG
   log_cb(RETRO_LOG_INFO, "Updating variables, UI finalized = %d\n", retro_ui_finalized);
#endif

#if !defined(__XPET__)
   var.key = "vice_cartridge";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      char cart_full[RETRO_PATH_MAX] = {0};

      if (!strcmp(var.value, "none"))
         snprintf(cart_full, sizeof(cart_full), "%s", "");
      else
         snprintf(cart_full, sizeof(cart_full), "%s%s%s%s%s",
               retro_system_data_directory, FSDEV_DIR_SEP_STR, machine_name, FSDEV_DIR_SEP_STR, var.value);

      if (retro_ui_finalized && strcmp(vice_opt.CartridgeFile, cart_full))
      {
         if (!strcmp(cart_full, ""))
            cartridge_detach_image(-1);
         else
#if defined(__XVIC__)
            cartridge_attach_image(vic20_autodetect_cartridge_type(cart_full), cart_full);
#elif defined(__XPLUS4__)
            cartridge_attach_image(CARTRIDGE_PLUS4_DETECT, cart_full);
#else
            cartridge_attach_image(0, cart_full);
#endif
         request_restart = true;
      }

      sprintf(vice_opt.CartridgeFile, "%s", cart_full);
   }
#endif

   var.key = "vice_autostart";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int autostartwarp = 0;

      if (!strcmp(var.value, "warp")) autostartwarp = 1;
      else                            autostartwarp = 0;

      if (!strcmp(var.value, "disabled")) opt_autostart = false;
      else                                opt_autostart = true;

      if (retro_ui_finalized)
      {
         if (vice_opt.AutostartWarp != autostartwarp)
            log_resources_set_int("AutostartWarp", autostartwarp);

         noautostart = !opt_autostart;
      }

      vice_opt.AutostartWarp = autostartwarp;
   }

   var.key = "vice_autoloadwarp";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if      (!strcmp(var.value, "disabled")) opt_autoloadwarp = 0;
      else if (!strcmp(var.value, "disk"))     opt_autoloadwarp = AUTOLOADWARP_DISK;
      else if (!strcmp(var.value, "tape"))     opt_autoloadwarp = AUTOLOADWARP_TAPE;
      else                                     opt_autoloadwarp = AUTOLOADWARP_DISK | AUTOLOADWARP_TAPE;

      /* Silently restore sounds when autoloadwarp is disabled and DSE is enabled */
      if (retro_ui_finalized && vice_opt.DriveSoundEmulation && vice_opt.DriveTrueEmulation &&
          !(opt_autoloadwarp & AUTOLOADWARP_DISK))
         resources_set_int("DriveSoundEmulationVolume", vice_opt.DriveSoundEmulation);
   }

   var.key = "vice_floppy_write_protection";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int readonly = 0;

      if (!strcmp(var.value, "disabled")) readonly = 0;
      else                                readonly = 1;

      if (retro_ui_finalized && vice_opt.AttachDevice8Readonly != readonly)
         log_resources_set_int("AttachDevice8Readonly", readonly);

      vice_opt.AttachDevice8Readonly = readonly;
   }

#if defined(__X64__) || defined(__X64SC__) || defined(__X128__)
   var.key = "vice_easyflash_write_protection";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int writecrt = 0;

      if (!strcmp(var.value, "disabled")) writecrt = 1;
      else                                writecrt = 0;

      if (retro_ui_finalized && vice_opt.EasyFlashWriteCRT != writecrt)
         log_resources_set_int("EasyFlashWriteCRT", writecrt);

      vice_opt.EasyFlashWriteCRT = writecrt;
   }
#endif

   var.key = "vice_work_disk";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int work_disk_type = opt_work_disk_type;
      int work_disk_unit = opt_work_disk_unit;

      if (!strcmp(var.value, "disabled"))    opt_work_disk_type = 0;
      else
      {
         if      (strstr(var.value, "_d64")) opt_work_disk_type = DISK_IMAGE_TYPE_D64;
         else if (strstr(var.value, "_d71")) opt_work_disk_type = DISK_IMAGE_TYPE_D71;
         else if (strstr(var.value, "_d81")) opt_work_disk_type = DISK_IMAGE_TYPE_D81;

         if      (strstr(var.value, "8_"))   opt_work_disk_unit = 8;
         else if (strstr(var.value, "9_"))   opt_work_disk_unit = 9;
      }

      if (work_disk_type != opt_work_disk_type || work_disk_unit != opt_work_disk_unit)
         request_update_work_disk = true;
   }

   var.key = "vice_virtual_device_traps";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (retro_ui_finalized)
      {
         if (!strcmp(var.value, "disabled") && vice_opt.VirtualDevices)
            log_resources_set_int("VirtualDevices", 0);
         else if (!strcmp(var.value, "enabled") && !vice_opt.VirtualDevices)
            log_resources_set_int("VirtualDevices", 1);
      }

      if (!strcmp(var.value, "disabled")) vice_opt.VirtualDevices = 0;
      else                                vice_opt.VirtualDevices = 1;
   }

   var.key = "vice_drive_true_emulation";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (retro_ui_finalized)
      {
         if (!strcmp(var.value, "disabled") && vice_opt.DriveTrueEmulation)
            log_resources_set_int("DriveTrueEmulation", 0);
         else if (!strcmp(var.value, "enabled") && !vice_opt.DriveTrueEmulation)
            log_resources_set_int("DriveTrueEmulation", 1);
      }

      if (!strcmp(var.value, "disabled")) vice_opt.DriveTrueEmulation = 0;
      else                                vice_opt.DriveTrueEmulation = 1;

      /* Silently restore sounds when TDE and DSE is enabled */
      if (retro_ui_finalized && vice_opt.DriveSoundEmulation && vice_opt.DriveTrueEmulation)
         resources_set_int("DriveSoundEmulationVolume", vice_opt.DriveSoundEmulation);

      /* Forcefully enable Virtual Device Traps if TDE is disabled,
       * otherwise floppy access does not work at all */
      if (!vice_opt.DriveTrueEmulation && !vice_opt.VirtualDevices)
      {
         vice_opt.VirtualDevices = 1;
         if (retro_ui_finalized)
            log_resources_set_int("VirtualDevices", vice_opt.VirtualDevices);
      }
   }

   var.key = "vice_drive_sound_emulation";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int val = atoi(var.value) * 20;

      if (retro_ui_finalized && vice_opt.DriveSoundEmulation != val)
      {
         if (!strcmp(var.value, "disabled"))
         {
            log_resources_set_int("DriveSoundEmulation", 0);
            log_resources_set_int("DriveSoundEmulationVolume", 0);
         }
         else
         {
            log_resources_set_int("DriveSoundEmulation", 1);
            log_resources_set_int("DriveSoundEmulationVolume", val);
         }
      }

      vice_opt.DriveSoundEmulation = val;

      /* Silently mute sounds without TDE,
       * because motor sound will not stop if TDE is changed during motor spinning
       * and also with autoloadwarping, because warping is muted anyway */
      if (retro_ui_finalized && vice_opt.DriveSoundEmulation &&
          (!vice_opt.DriveTrueEmulation || opt_autoloadwarp & AUTOLOADWARP_DISK))
         resources_set_int("DriveSoundEmulationVolume", 0);
   }

   var.key = "vice_datasette_sound";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int val = atoi(var.value);
      opt_datasette_sound_volume = val;

      if (retro_ui_finalized && vice_opt.DatasetteSound != val)
      {
         if (!strcmp(var.value, "disabled"))
            log_resources_set_int("DatasetteSound", 0);
         else
            log_resources_set_int("DatasetteSound", 1);
      }

      vice_opt.DatasetteSound = val;
   }

#if defined(__X64__) || defined(__X64SC__) || defined(__X64DTV__) || defined(__X128__) || defined(__XSCPU64__) || defined(__XCBM5x0__) || defined(__XVIC__) || defined(__XPLUS4__)
   var.key = "vice_audio_leak_emulation";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int audioleak = 0;
      opt_audio_leak_volume = atoi(var.value);

      if (!strcmp(var.value, "disabled")) audioleak = 0;
      else                                audioleak = 1;

      if (retro_ui_finalized && vice_opt.AudioLeak != audioleak)
#if defined(__X64__) || defined(__X64SC__) || defined(__X64DTV__) || defined(__X128__) || defined(__XSCPU64__) || defined(__XCBM5x0__)
         log_resources_set_int("VICIIAudioLeak", audioleak);
#elif defined(__XVIC__)
         log_resources_set_int("VICAudioLeak", audioleak);
#elif defined(__XPLUS4__)
         log_resources_set_int("TEDAudioLeak", audioleak);
#endif
      vice_opt.AudioLeak = audioleak;
   }
#endif

   var.key = "vice_sound_sample_rate";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      vice_opt.SoundSampleRate = atoi(var.value);
   }

#if defined(__XVIC__)
   var.key = "vice_vic20_model";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int model = 0;

      if (strstr(var.value, "auto")) opt_model_auto = 1;
      else                           opt_model_auto = 0;

      if      (!strcmp(var.value, "VIC20 PAL auto"))  model = VIC20MODEL_VIC20_PAL;
      else if (!strcmp(var.value, "VIC20 NTSC auto")) model = VIC20MODEL_VIC20_NTSC;
      else if (!strcmp(var.value, "VIC20 PAL"))       model = VIC20MODEL_VIC20_PAL;
      else if (!strcmp(var.value, "VIC20 NTSC"))      model = VIC20MODEL_VIC20_NTSC;
      else if (!strcmp(var.value, "VIC21"))           model = VIC20MODEL_VIC21;

      if (retro_ui_finalized && vice_opt.Model != model)
      {
         vic20model_set(model);
         request_model_prev = -1;
         request_restart = true;
         /* Memory expansion needs to be reseted to get updated */
         vice_opt.VIC20Memory = 0xff;
      }

      vice_opt.Model = model;
   }

   var.key = "vice_vic20_memory_expansions";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int vic20mem = 0;

      if      (!strcmp(var.value, "none")) vic20mem = 0;
      else if (!strcmp(var.value, "3kB"))  vic20mem = 1;
      else if (!strcmp(var.value, "8kB"))  vic20mem = 2;
      else if (!strcmp(var.value, "16kB")) vic20mem = 3;
      else if (!strcmp(var.value, "24kB")) vic20mem = 4;
      else if (!strcmp(var.value, "35kB")) vic20mem = 5;

      /* Super VIC uses memory blocks 1+2 by default */
      if (!vic20mem && vice_opt.Model == VIC20MODEL_VIC21)
         vic20mem = 3;

      if (retro_ui_finalized && vice_opt.VIC20Memory != vic20mem)
      {
         unsigned int vic_blocks = 0;
         switch (vic20mem)
         {
            case 1:
               vic_blocks |= VIC_BLK0;
               break;

            case 2:
               vic_blocks |= VIC_BLK1;
               break;

            case 3:
               vic_blocks |= VIC_BLK1;
               vic_blocks |= VIC_BLK2;
               break;

            case 4:
               vic_blocks |= VIC_BLK1;
               vic_blocks |= VIC_BLK2;
               vic_blocks |= VIC_BLK3;
               break;

            case 5:
               vic_blocks = VIC_BLK_ALL;
               break;
         }

         log_resources_set_int("RAMBlock0", (vic_blocks & VIC_BLK0) ? 1 : 0);
         log_resources_set_int("RAMBlock1", (vic_blocks & VIC_BLK1) ? 1 : 0);
         log_resources_set_int("RAMBlock2", (vic_blocks & VIC_BLK2) ? 1 : 0);
         log_resources_set_int("RAMBlock3", (vic_blocks & VIC_BLK3) ? 1 : 0);
         log_resources_set_int("RAMBlock5", (vic_blocks & VIC_BLK5) ? 1 : 0);
         request_restart = true;
      }

      vice_opt.VIC20Memory = vic20mem;
   }
#elif defined(__XPLUS4__)
   var.key = "vice_plus4_model";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int model = 0;

      if      (!strcmp(var.value, "C16 PAL"))    model = PLUS4MODEL_C16_PAL;
      else if (!strcmp(var.value, "C16 NTSC"))   model = PLUS4MODEL_C16_NTSC;
      else if (!strcmp(var.value, "PLUS4 PAL"))  model = PLUS4MODEL_PLUS4_PAL;
      else if (!strcmp(var.value, "PLUS4 NTSC")) model = PLUS4MODEL_PLUS4_NTSC;
      else if (!strcmp(var.value, "V364 NTSC"))  model = PLUS4MODEL_V364_NTSC;
      else if (!strcmp(var.value, "232 NTSC"))   model = PLUS4MODEL_232_NTSC;

      if (retro_ui_finalized && vice_opt.Model != model)
      {
         plus4model_set(model);
         request_restart = true;
      }

      vice_opt.Model = model;
   }
#elif defined(__X128__)
   var.key = "vice_c128_model";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int model = 0;

      if      (!strcmp(var.value, "C128 PAL"))      model = C128MODEL_C128_PAL;
      else if (!strcmp(var.value, "C128 NTSC"))     model = C128MODEL_C128_NTSC;
      else if (!strcmp(var.value, "C128 D PAL"))    model = C128MODEL_C128D_PAL;
      else if (!strcmp(var.value, "C128 D NTSC"))   model = C128MODEL_C128D_NTSC;
      else if (!strcmp(var.value, "C128 DCR PAL"))  model = C128MODEL_C128DCR_PAL;
      else if (!strcmp(var.value, "C128 DCR NTSC")) model = C128MODEL_C128DCR_NTSC;

      if (retro_ui_finalized && vice_opt.Model != model)
      {
         c128model_set(model);
         request_restart = true;
      }

      vice_opt.Model = model;
   }

   var.key = "vice_c128_video_output";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int c128columnkey = 1;

      if      (!strcmp(var.value, "VICII")) c128columnkey = 1;
      else if (!strcmp(var.value, "VDC"))   c128columnkey = 0;

      if (retro_ui_finalized && vice_opt.C128ColumnKey != c128columnkey)
         log_resources_set_int("C128ColumnKey", c128columnkey);

      vice_opt.C128ColumnKey = c128columnkey;
   }

   var.key = "vice_c128_go64";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int c128go64 = 0;

      if (!strcmp(var.value, "disabled")) c128go64 = 0;
      else                                c128go64 = 1;

      /* Force VIC-II with GO64 */
      if (c128go64)
      {
         vice_opt.C128ColumnKey = 1;
         if (retro_ui_finalized)
            log_resources_set_int("C128ColumnKey", 1);
      }

      if (retro_ui_finalized && vice_opt.Go64Mode != c128go64)
      {
         log_resources_set_int("Go64Mode", c128go64);
         request_restart = true;
      }
      vice_opt.Go64Mode = c128go64;
   }
#elif defined(__XPET__)
   var.key = "vice_pet_model";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int model = 0;

      if      (!strcmp(var.value, "2001"))     model = PETMODEL_2001;
      else if (!strcmp(var.value, "3008"))     model = PETMODEL_3008;
      else if (!strcmp(var.value, "3016"))     model = PETMODEL_3016;
      else if (!strcmp(var.value, "3032"))     model = PETMODEL_3032;
      else if (!strcmp(var.value, "3032B"))    model = PETMODEL_3032B;
      else if (!strcmp(var.value, "4016"))     model = PETMODEL_4016;
      else if (!strcmp(var.value, "4032"))     model = PETMODEL_4032;
      else if (!strcmp(var.value, "4032B"))    model = PETMODEL_4032B;
      else if (!strcmp(var.value, "8032"))     model = PETMODEL_8032;
      else if (!strcmp(var.value, "8096"))     model = PETMODEL_8096;
      else if (!strcmp(var.value, "8296"))     model = PETMODEL_8296;
      else if (!strcmp(var.value, "SUPERPET")) model = PETMODEL_SUPERPET;
      
      if (retro_ui_finalized && vice_opt.Model != model)
      {
         petmodel_set(model);
         request_restart = true;
         /* Keyboard layout refresh required. All models below 8032 except B models use graphics layout, others use business. */
         keyboard_init();
      }
      vice_opt.Model = model;
   }
#elif defined(__XCBM2__)
   var.key = "vice_cbm2_model";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int model = 0;

      if      (!strcmp(var.value, "610 PAL"))      model = CBM2MODEL_610_PAL;
      else if (!strcmp(var.value, "610 NTSC"))     model = CBM2MODEL_610_NTSC;
      else if (!strcmp(var.value, "620 PAL"))      model = CBM2MODEL_620_PAL;
      else if (!strcmp(var.value, "620 NTSC"))     model = CBM2MODEL_620_NTSC;
      else if (!strcmp(var.value, "620PLUS PAL"))  model = CBM2MODEL_620PLUS_PAL;
      else if (!strcmp(var.value, "620PLUS NTSC")) model = CBM2MODEL_620PLUS_NTSC;
      else if (!strcmp(var.value, "710 NTSC"))     model = CBM2MODEL_710_NTSC;
      else if (!strcmp(var.value, "720 NTSC"))     model = CBM2MODEL_720_NTSC;
      else if (!strcmp(var.value, "720PLUS NTSC")) model = CBM2MODEL_720PLUS_NTSC;

      if (retro_ui_finalized && vice_opt.Model != model)
      {
         cbm2model_set(model);
         request_restart = true;
      }

      vice_opt.Model = model;
   }
#elif defined(__XCBM5x0__)
   var.key = "vice_cbm5x0_model";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int model = 0;

      if      (!strcmp(var.value, "510 PAL"))  model = CBM2MODEL_510_PAL;
      else if (!strcmp(var.value, "510 NTSC")) model = CBM2MODEL_510_NTSC;

      if (retro_ui_finalized && vice_opt.Model != model)
      {
         cbm2model_set(model);
         request_restart = true;
      }

      vice_opt.Model = model;
   }
#elif defined(__X64DTV__)
   var.key = "vice_c64dtv_model";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int model = 0;

      if      (!strcmp(var.value, "DTV2 PAL"))    model = DTVMODEL_V2_PAL;
      else if (!strcmp(var.value, "DTV2 NTSC"))   model = DTVMODEL_V2_NTSC;
      else if (!strcmp(var.value, "DTV3 PAL"))    model = DTVMODEL_V3_PAL;
      else if (!strcmp(var.value, "DTV3 NTSC"))   model = DTVMODEL_V3_NTSC;
      else if (!strcmp(var.value, "HUMMER NTSC")) model = DTVMODEL_HUMMER_NTSC;

      if (retro_ui_finalized && vice_opt.Model != model)
      {
         dtvmodel_set(model);
         request_restart = true;
      }

      vice_opt.Model = model;
   }
#else
   var.key = "vice_c64_model";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int model = 0;

      if (strstr(var.value, "auto")) opt_model_auto = 1;
      else                           opt_model_auto = 0;

      if      (!strcmp(var.value, "C64 PAL auto"))   model = C64MODEL_C64_PAL;
      else if (!strcmp(var.value, "C64 NTSC auto"))  model = C64MODEL_C64_NTSC;
      else if (!strcmp(var.value, "C64C PAL auto"))  model = C64MODEL_C64C_PAL;
      else if (!strcmp(var.value, "C64C NTSC auto")) model = C64MODEL_C64C_NTSC;
      else if (!strcmp(var.value, "C64 PAL"))        model = C64MODEL_C64_PAL;
      else if (!strcmp(var.value, "C64 NTSC"))       model = C64MODEL_C64_NTSC;
      else if (!strcmp(var.value, "C64C PAL"))       model = C64MODEL_C64C_PAL;
      else if (!strcmp(var.value, "C64C NTSC"))      model = C64MODEL_C64C_NTSC;
      else if (!strcmp(var.value, "C64SX PAL"))      model = C64MODEL_C64SX_PAL;
      else if (!strcmp(var.value, "C64SX NTSC"))     model = C64MODEL_C64SX_NTSC;
      else if (!strcmp(var.value, "PET64 PAL"))      model = C64MODEL_PET64_PAL;
      else if (!strcmp(var.value, "PET64 NTSC"))     model = C64MODEL_PET64_NTSC;
      else if (!strcmp(var.value, "C64 GS PAL"))     model = C64MODEL_C64_GS;
      else if (!strcmp(var.value, "C64 JAP NTSC"))   model = C64MODEL_C64_JAP;
      else if (!strcmp(var.value, "C64 PAL N"))      model = C64MODEL_C64_PAL_N;
      else if (!strcmp(var.value, "C64 OLD PAL"))    model = C64MODEL_C64_OLD_PAL;
      else if (!strcmp(var.value, "C64 OLD NTSC"))   model = C64MODEL_C64_OLD_NTSC;

      if (retro_ui_finalized && vice_opt.Model != model)
      {
         c64model_set(model);
         request_restart = true;
         request_model_prev = -1;
      }

      vice_opt.Model = model;
   }

#if defined(__XSCPU64__)
   var.key = "vice_supercpu_simm_size";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int simmsize = atoi(var.value);

      if (retro_ui_finalized && vice_opt.SIMMSize != simmsize)
      {
         log_resources_set_int("SIMMSize", simmsize);
         request_restart = true;
      }

      vice_opt.SIMMSize = simmsize;
   }
#else
   var.key = "vice_ram_expansion_unit";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int reusize = 0;

      if (strcmp(var.value, "none"))
         reusize = atoi(var.value);

      if (retro_ui_finalized && vice_opt.REUsize != reusize)
      {
         if (!reusize)
            log_resources_set_int("REU", 0);
         else
         {
            log_resources_set_int("REUsize", reusize);
            log_resources_set_int("REU", 1);
         }
         request_restart = true;
      }

      vice_opt.REUsize = reusize;
   }
#endif
#endif

#if !defined(__XPET__) && !defined(__XPLUS4__) && !defined(__XVIC__)
   var.key = "vice_sid_engine";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int sid_engine = SID_ENGINE_FASTSID;

      if      (!strcmp(var.value, "ReSID"))     sid_engine = SID_ENGINE_RESID;
#ifdef HAVE_RESID33
      else if (!strcmp(var.value, "ReSID-3.3")) sid_engine = SID_ENGINE_RESID33;
#endif
      else if (!strcmp(var.value, "ReSID-FP"))  sid_engine = SID_ENGINE_RESIDFP;

      if (retro_ui_finalized && vice_opt.SidEngine != sid_engine)
         log_resources_set_int("SidEngine", sid_engine);

      vice_opt.SidEngine = sid_engine;
   }

   var.key = "vice_sid_model";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int sid_model = SID_MODEL_6581;
      switch (vice_opt.Model)
      {
#if defined(__X64__) || defined(__X64SC__) || defined(__XSCPU64__)
         case C64MODEL_C64C_PAL:
         case C64MODEL_C64C_NTSC:
         case C64MODEL_C64_GS:
#elif defined(__X128__)
         case C128MODEL_C128DCR_PAL:
         case C128MODEL_C128DCR_NTSC:
#else
         case -1:
#endif
            sid_model = SID_MODEL_8580;
            break;
      }

      if      (!strcmp(var.value, "6581"))   sid_model = SID_MODEL_6581;
      else if (!strcmp(var.value, "8580"))   sid_model = SID_MODEL_8580;
      /* There is no digiboost for FastSID (and it's not needed either) */
      else if (!strcmp(var.value, "8580RD")) sid_model = (!vice_opt.SidEngine ? SID_MODEL_8580 : SID_MODEL_8580D);

      if (retro_ui_finalized && vice_opt.SidModel != sid_model)
         log_resources_set_int("SidModel", sid_model);

      vice_opt.SidModel = sid_model;
   }

   var.key = "vice_sid_extra";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int sid_extra = atoi(var.value);
      if (strcmp(var.value, "disabled"))
         sid_extra = strtol(var.value, NULL, 16);

      if (retro_ui_finalized && vice_opt.SidExtra != sid_extra)
      {
         if (!sid_extra)
            log_resources_set_int("SidStereo", 0);
         else
         {
            log_resources_set_int("Sid2AddressStart", sid_extra);
            if (!vice_opt.SidExtra)
               log_resources_set_int("SidStereo", 1);
         }
      }

      vice_opt.SidExtra = sid_extra;
   }

   var.key = "vice_resid_sampling";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int val = 0;

      if      (!strcmp(var.value, "fast"))            val = SID_RESID_SAMPLING_FAST;
      else if (!strcmp(var.value, "interpolation"))   val = SID_RESID_SAMPLING_INTERPOLATION;
      else if (!strcmp(var.value, "resampling"))      val = SID_RESID_SAMPLING_RESAMPLING;
      else if (!strcmp(var.value, "fast resampling")) val = SID_RESID_SAMPLING_FAST_RESAMPLING;

      if (retro_ui_finalized && vice_opt.SidResidSampling != val)
         log_resources_set_int("SidResidSampling", val);

      vice_opt.SidResidSampling = val;
   }

   var.key = "vice_resid_passband";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int val = atoi(var.value);

      if (retro_ui_finalized && vice_opt.SidResidPassband != val)
      {
         log_resources_set_int("SidResidPassband", val);
         log_resources_set_int("SidResid8580Passband", val);
      }

      vice_opt.SidResidPassband = val;
   }

   var.key = "vice_resid_gain";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int val = atoi(var.value);

      if (retro_ui_finalized && vice_opt.SidResidGain != val)
      {
         log_resources_set_int("SidResidGain", val);
         log_resources_set_int("SidResid8580Gain", val);
      }

      vice_opt.SidResidGain = val;
   }

   var.key = "vice_resid_filterbias";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int val = atoi(var.value);

      if (retro_ui_finalized && vice_opt.SidResidFilterBias != val)
         log_resources_set_int("SidResidFilterBias", val);

      vice_opt.SidResidFilterBias = val;
   }

   var.key = "vice_resid_8580filterbias";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int val = atoi(var.value);

      if (retro_ui_finalized && vice_opt.SidResid8580FilterBias != val)
         log_resources_set_int("SidResid8580FilterBias", val);

      vice_opt.SidResid8580FilterBias = val;
   }
#endif

#if defined(__X64__) || defined(__X64SC__) || defined(__X128__)
   var.key = "vice_sfx_sound_expander";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int sfx_chip = atoi(var.value);

      if (retro_ui_finalized && vice_opt.SFXSoundExpanderChip != sfx_chip)
      {
         if (!sfx_chip)
            log_resources_set_int("SFXSoundExpander", 0);
         else
         {
            log_resources_set_int("SFXSoundExpanderChip", sfx_chip);
            if (!vice_opt.SFXSoundExpanderChip)
               log_resources_set_int("SFXSoundExpander", 1);
         }
      }

      vice_opt.SFXSoundExpanderChip = sfx_chip;
   }
#endif

#if defined(__X64__) || defined(__X64SC__) || defined(__X64DTV__) || defined(__X128__) || defined(__XSCPU64__) || defined(__XCBM5x0__) || defined(__XVIC__) || defined(__XPLUS4__)
   var.key = "vice_zoom_mode";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if      (!strcmp(var.value, "disabled")) zoom_mode_id = 0;
      else if (!strcmp(var.value, "small"))    zoom_mode_id = 1;
      else if (!strcmp(var.value, "medium"))   zoom_mode_id = 2;
      else if (!strcmp(var.value, "maximum"))  zoom_mode_id = 3;
      else if (!strcmp(var.value, "manual"))   zoom_mode_id = 4;

#if defined(__X128__)
      if (!vice_opt.C128ColumnKey)
         zoom_mode_id = 0;
#endif

      opt_zoom_mode_id = zoom_mode_id;
   }

   var.key = "vice_zoom_mode_crop";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int zoom_mode_crop_id_prev = zoom_mode_crop_id;

      if      (!strcmp(var.value, "both"))       zoom_mode_crop_id = 0;
      else if (!strcmp(var.value, "vertical"))   zoom_mode_crop_id = 1;
      else if (!strcmp(var.value, "horizontal")) zoom_mode_crop_id = 2;
      else if (!strcmp(var.value, "16:9"))       zoom_mode_crop_id = 3;
      else if (!strcmp(var.value, "16:10"))      zoom_mode_crop_id = 4;
      else if (!strcmp(var.value, "4:3"))        zoom_mode_crop_id = 5;
      else if (!strcmp(var.value, "5:4"))        zoom_mode_crop_id = 6;

      /* Zoom reset */
      if (zoom_mode_crop_id != zoom_mode_crop_id_prev)
         zoom_mode_id_prev = -1;
   }

   var.key = "vice_aspect_ratio";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int opt_aspect_ratio_prev = opt_aspect_ratio;

      if      (!strcmp(var.value, "auto")) opt_aspect_ratio = 0;
      else if (!strcmp(var.value, "pal"))  opt_aspect_ratio = 1;
      else if (!strcmp(var.value, "ntsc")) opt_aspect_ratio = 2;
      else if (!strcmp(var.value, "raw"))  opt_aspect_ratio = 3;

      /* Revert if aspect ratio is locked */
      if (opt_aspect_ratio_locked)
         opt_aspect_ratio = opt_aspect_ratio_prev;

      /* Zoom reset */
      if (opt_aspect_ratio != opt_aspect_ratio_prev)
         zoom_mode_id_prev = -1;
   }

   var.key = "vice_manual_crop_top";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int manual_crop_top_prev = manual_crop_top;
      manual_crop_top = atoi(var.value);
      if (manual_crop_top != manual_crop_top_prev)
         zoom_mode_id_prev = -1;
   }
   var.key = "vice_manual_crop_bottom";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int manual_crop_bottom_prev = manual_crop_bottom;
      manual_crop_bottom = atoi(var.value);
      if (manual_crop_bottom != manual_crop_bottom_prev)
         zoom_mode_id_prev = -1;
   }
   var.key = "vice_manual_crop_left";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int manual_crop_left_prev = manual_crop_left;
      manual_crop_left = atoi(var.value);
      if (manual_crop_left != manual_crop_left_prev)
         zoom_mode_id_prev = -1;
   }
   var.key = "vice_manual_crop_right";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int manual_crop_right_prev = manual_crop_right;
      manual_crop_right = atoi(var.value);
      if (manual_crop_right != manual_crop_right_prev)
         zoom_mode_id_prev = -1;
   }
#endif

   var.key = "vice_gfx_colors";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      /* Only allow screenmode change after restart */
      if (!pix_bytes_initialized)
      {
         if (!strcmp(var.value, "16bit"))      pix_bytes = 2;
         else if (!strcmp(var.value, "24bit")) pix_bytes = 4;
      }
   }

#if defined(__X64__) || defined(__X64SC__) || defined(__X64DTV__) || defined(__X128__) || defined(__XSCPU64__) || defined(__XCBM5x0__)
   var.key = "vice_vicii_filter";
#elif defined(__XVIC__)
   var.key = "vice_vic_filter";
#elif defined(__XPLUS4__)
   var.key = "vice_ted_filter";
#elif defined(__XPET__) || defined(__XCBM2__)
   var.key = "vice_crtc_filter";
#endif
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int filter = strcmp(var.value, "disabled");
      int blur = 0;

      if (filter)
      {
         if      (strstr(var.value, "lowblur")) blur = 50;
         else if (strstr(var.value, "medblur")) blur = 250;
         else                                   blur = 500;
      }

      if (retro_ui_finalized && vice_opt.Filter != blur)
      {
#if defined(__XPET__) || defined(__XCBM2__)
         log_resources_set_int("CrtcFilter", filter);
         log_resources_set_int("CrtcPALBlur", blur);
#elif defined(__XPLUS4__)
         log_resources_set_int("TEDFilter", filter);
         log_resources_set_int("TEDPALBlur", blur);
#elif defined(__XVIC__)
         log_resources_set_int("VICFilter", filter);
         log_resources_set_int("VICPALBlur", blur);
#else
         log_resources_set_int("VICIIFilter", filter);
         log_resources_set_int("VICIIPALBlur", blur);
#endif
      }

      vice_opt.Filter = blur;
   }

#if defined(__XVIC__)
   var.key = "vice_vic20_external_palette";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (retro_ui_finalized && strcmp(var.value, vice_opt.ExternalPalette))
      {
         if (!strcmp(var.value, "default"))
            log_resources_set_int("VICExternalPalette", 0);
         else
         {
            log_resources_set_int("VICExternalPalette", 1);
            log_resources_set_string("VICPaletteFile", var.value);
         }
      }

      sprintf(vice_opt.ExternalPalette, "%s", var.value);
   }
#elif defined(__XPLUS4__)
   var.key = "vice_plus4_external_palette";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (retro_ui_finalized && strcmp(var.value, vice_opt.ExternalPalette))
      {
         if (!strcmp(var.value, "default"))
            log_resources_set_int("TEDExternalPalette", 0);
         else
         {
            log_resources_set_int("TEDExternalPalette", 1);
            log_resources_set_string("TEDPaletteFile", var.value);
         }
      }

      sprintf(vice_opt.ExternalPalette, "%s", var.value);
   }
#elif defined(__XPET__)
   var.key = "vice_pet_external_palette";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (retro_ui_finalized && strcmp(var.value, vice_opt.ExternalPalette))
      {
         if (!strcmp(var.value, "default"))
            log_resources_set_int("CrtcExternalPalette", 0);
         else
         {
            log_resources_set_int("CrtcExternalPalette", 1);
            log_resources_set_string("CrtcPaletteFile", var.value);
         }
      }

      sprintf(vice_opt.ExternalPalette, "%s", var.value);
   }
#elif defined(__XCBM2__)
   var.key = "vice_cbm2_external_palette";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (retro_ui_finalized && strcmp(var.value, vice_opt.ExternalPalette))
      {
         if (!strcmp(var.value, "default"))
            log_resources_set_int("CrtcExternalPalette", 0);
         else
         {
            log_resources_set_int("CrtcExternalPalette", 1);
            log_resources_set_string("CrtcPaletteFile", var.value);
         }
      }

      sprintf(vice_opt.ExternalPalette, "%s", var.value);
   }
#else
   var.key = "vice_external_palette";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (retro_ui_finalized && strcmp(var.value, vice_opt.ExternalPalette))
      {
         if (!strcmp(var.value, "default"))
            log_resources_set_int("VICIIExternalPalette", 0);
         else
         {
            log_resources_set_int("VICIIExternalPalette", 1);
            log_resources_set_string("VICIIPaletteFile", var.value);
         }
      }

      sprintf(vice_opt.ExternalPalette, "%s", var.value);
   }
#endif

#if defined(__X64__) || defined(__X64SC__) || defined(__X64DTV__) || defined(__X128__) || defined(__XSCPU64__) || defined(__XCBM5x0__) || defined(__XVIC__) || defined(__XPLUS4__)
#if defined(__X64__) || defined(__X64SC__) || defined(__X64DTV__) || defined(__X128__) || defined(__XSCPU64__) || defined(__XCBM5x0__)
   var.key = "vice_vicii_color_gamma";
#elif defined(__XVIC__)
   var.key = "vice_vic_color_gamma";
#elif defined(__XPLUS4__)
   var.key = "vice_ted_color_gamma";
#endif
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int color_gamma = atoi(var.value);

      if (retro_ui_finalized && vice_opt.ColorGamma != color_gamma)
#if defined(__X64__) || defined(__X64SC__) || defined(__X64DTV__) || defined(__X128__) || defined(__XSCPU64__) || defined(__XCBM5x0__)
         log_resources_set_int("VICIIColorGamma", color_gamma);
#elif defined(__XVIC__)
         log_resources_set_int("VICColorGamma", color_gamma);
#elif defined(__XPLUS4__)
         log_resources_set_int("TEDColorGamma", color_gamma);
#endif

      vice_opt.ColorGamma = color_gamma;
   }

#if defined(__X64__) || defined(__X64SC__) || defined(__X64DTV__) || defined(__X128__) || defined(__XSCPU64__) || defined(__XCBM5x0__)
   var.key = "vice_vicii_color_tint";
#elif defined(__XVIC__)
   var.key = "vice_vic_color_tint";
#elif defined(__XPLUS4__)
   var.key = "vice_ted_color_tint";
#endif
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int color_tint = atoi(var.value);

      if (retro_ui_finalized && vice_opt.ColorTint != color_tint)
#if defined(__X64__) || defined(__X64SC__) || defined(__X64DTV__) || defined(__X128__) || defined(__XSCPU64__) || defined(__XCBM5x0__)
         log_resources_set_int("VICIIColorTint", color_tint);
#elif defined(__XVIC__)
         log_resources_set_int("VICColorTint", color_tint);
#elif defined(__XPLUS4__)
         log_resources_set_int("TEDColorTint", color_tint);
#endif

      vice_opt.ColorTint = color_tint;
   }

#if defined(__X64__) || defined(__X64SC__) || defined(__X64DTV__) || defined(__X128__) || defined(__XSCPU64__) || defined(__XCBM5x0__)
   var.key = "vice_vicii_color_saturation";
#elif defined(__XVIC__)
   var.key = "vice_vic_color_saturation";
#elif defined(__XPLUS4__)
   var.key = "vice_ted_color_saturation";
#endif
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int color_saturation = atoi(var.value);

      if (retro_ui_finalized && vice_opt.ColorSaturation != color_saturation)
#if defined(__X64__) || defined(__X64SC__) || defined(__X64DTV__) || defined(__X128__) || defined(__XSCPU64__) || defined(__XCBM5x0__)
         log_resources_set_int("VICIIColorSaturation", color_saturation);
#elif defined(__XVIC__)
         log_resources_set_int("VICColorSaturation", color_saturation);
#elif defined(__XPLUS4__)
         log_resources_set_int("TEDColorSaturation", color_saturation);
#endif

      vice_opt.ColorSaturation = color_saturation;
   }

#if defined(__X64__) || defined(__X64SC__) || defined(__X64DTV__) || defined(__X128__) || defined(__XSCPU64__) || defined(__XCBM5x0__)
   var.key = "vice_vicii_color_contrast";
#elif defined(__XVIC__)
   var.key = "vice_vic_color_contrast";
#elif defined(__XPLUS4__)
   var.key = "vice_ted_color_contrast";
#endif
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int color_contrast = atoi(var.value);

      if (retro_ui_finalized && vice_opt.ColorContrast != color_contrast)
#if defined(__X64__) || defined(__X64SC__) || defined(__X64DTV__) || defined(__X128__) || defined(__XSCPU64__) || defined(__XCBM5x0__)
         log_resources_set_int("VICIIColorContrast", color_contrast);
#elif defined(__XVIC__)
         log_resources_set_int("VICColorContrast", color_contrast);
#elif defined(__XPLUS4__)
         log_resources_set_int("TEDColorContrast", color_contrast);
#endif

      vice_opt.ColorContrast = color_contrast;
   }

#if defined(__X64__) || defined(__X64SC__) || defined(__X64DTV__) || defined(__X128__) || defined(__XSCPU64__) || defined(__XCBM5x0__)
   var.key = "vice_vicii_color_brightness";
#elif defined(__XVIC__)
   var.key = "vice_vic_color_brightness";
#elif defined(__XPLUS4__)
   var.key = "vice_ted_color_brightness";
#endif
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int color_brightness = atoi(var.value);

      if (retro_ui_finalized && vice_opt.ColorBrightness != color_brightness)
#if defined(__X64__) || defined(__X64SC__) || defined(__X64DTV__) || defined(__X128__) || defined(__XSCPU64__) || defined(__XCBM5x0__)
         log_resources_set_int("VICIIColorBrightness", color_brightness);
#elif defined(__XVIC__)
         log_resources_set_int("VICColorBrightness", color_brightness);
#elif defined(__XPLUS4__)
         log_resources_set_int("TEDColorBrightness", color_brightness);
#endif

      vice_opt.ColorBrightness = color_brightness;
   }
#endif

#if !defined(__XCBM5x0__)
   var.key = "vice_userport_joytype";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int userportjoytype = -1;

      if      (!strcmp(var.value, "disabled")) userportjoytype = -1;
      else if (!strcmp(var.value, "CGA"))      userportjoytype = USERPORT_JOYSTICK_CGA;
      else if (!strcmp(var.value, "PET"))      userportjoytype = USERPORT_JOYSTICK_PET;
      else if (!strcmp(var.value, "Hummer"))   userportjoytype = USERPORT_JOYSTICK_HUMMER;
      else if (!strcmp(var.value, "OEM"))      userportjoytype = USERPORT_JOYSTICK_OEM;
      else if (!strcmp(var.value, "HIT"))      userportjoytype = USERPORT_JOYSTICK_HIT;
      else if (!strcmp(var.value, "Kingsoft")) userportjoytype = USERPORT_JOYSTICK_KINGSOFT;
      else if (!strcmp(var.value, "Starbyte")) userportjoytype = USERPORT_JOYSTICK_STARBYTE;

      if (retro_ui_finalized && vice_opt.UserportJoyType != userportjoytype)
      {
         if (userportjoytype == -1)
            log_resources_set_int("UserportJoy", 0);
         else
         {
            log_resources_set_int("UserportJoy", 1);
            log_resources_set_int("UserportJoyType", userportjoytype);
         }
      }

      vice_opt.UserportJoyType = userportjoytype;
   }
#endif

#if !defined(__XPET__) && !defined(__XCBM2__) && !defined(__XVIC__)
   var.key = "vice_joyport";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if      (!strcmp(var.value, "1") && !cur_port_locked) cur_port = 1;
      else if (!strcmp(var.value, "2") && !cur_port_locked) cur_port = 2;
   }
#endif

#if !defined(__XPET__) && !defined(__XCBM2__)
   var.key = "vice_joyport_type";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      opt_joyport_type = atoi(var.value);

      /* Light guns/pens only possible in port 1 */
      if (opt_joyport_type > 10)
         cur_port = 1;
   }

   var.key = "vice_joyport_pointer_color";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if      (!strcmp(var.value, "disabled")) opt_joyport_pointer_color = -1;
      else if (!strcmp(var.value, "black"))    opt_joyport_pointer_color = 0;
      else if (!strcmp(var.value, "white"))    opt_joyport_pointer_color = 1;
      else if (!strcmp(var.value, "red"))      opt_joyport_pointer_color = 2;
      else if (!strcmp(var.value, "green"))    opt_joyport_pointer_color = 3;
      else if (!strcmp(var.value, "blue"))     opt_joyport_pointer_color = 4;
      else if (!strcmp(var.value, "yellow"))   opt_joyport_pointer_color = 5;
      else if (!strcmp(var.value, "cyan"))     opt_joyport_pointer_color = 6;
      else if (!strcmp(var.value, "purple"))   opt_joyport_pointer_color = 7;
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
      if (!strcmp(var.value, "disabled")) opt_keyrah_keypad = false;
      else                                opt_keyrah_keypad = true;
   }

   var.key = "vice_keyboard_keymap";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int val = opt_keyboard_keymap;

      if      (!strcmp(var.value, "symbolic"))        opt_keyboard_keymap = KBD_INDEX_SYM;
      else if (!strcmp(var.value, "positional"))      opt_keyboard_keymap = KBD_INDEX_POS;
      else if (!strcmp(var.value, "symbolic-user"))   opt_keyboard_keymap = KBD_INDEX_USERSYM;
      else if (!strcmp(var.value, "positional-user")) opt_keyboard_keymap = KBD_INDEX_USERPOS;

      if (retro_ui_finalized && opt_keyboard_keymap != val)
         keyboard_init();
   }

   var.key = "vice_physical_keyboard_pass_through";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "disabled")) opt_keyboard_pass_through = false;
      else                                opt_keyboard_pass_through = true;
   }

   var.key = "vice_retropad_options";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if      (!strcmp(var.value, "disabled"))    opt_retropad_options = 0;
      else if (!strcmp(var.value, "rotate"))      opt_retropad_options = 1;
      else if (!strcmp(var.value, "jump"))        opt_retropad_options = 2;
      else if (!strcmp(var.value, "rotate_jump")) opt_retropad_options = 3;
   }

   var.key = "vice_turbo_fire";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!turbo_fire_locked)
      {
         if (!strcmp(var.value, "disabled")) retro_turbo_fire = false;
         else                                retro_turbo_fire = true;
      }
   }

   var.key = "vice_turbo_fire_button";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if      (!strcmp(var.value, "B"))  turbo_fire_button = RETRO_DEVICE_ID_JOYPAD_B;
      else if (!strcmp(var.value, "A"))  turbo_fire_button = RETRO_DEVICE_ID_JOYPAD_A;
      else if (!strcmp(var.value, "Y"))  turbo_fire_button = RETRO_DEVICE_ID_JOYPAD_Y;
      else if (!strcmp(var.value, "X"))  turbo_fire_button = RETRO_DEVICE_ID_JOYPAD_X;
      else if (!strcmp(var.value, "L"))  turbo_fire_button = RETRO_DEVICE_ID_JOYPAD_L;
      else if (!strcmp(var.value, "R"))  turbo_fire_button = RETRO_DEVICE_ID_JOYPAD_R;
      else if (!strcmp(var.value, "L2")) turbo_fire_button = RETRO_DEVICE_ID_JOYPAD_L2;
      else if (!strcmp(var.value, "R2")) turbo_fire_button = RETRO_DEVICE_ID_JOYPAD_R2;
   }

   var.key = "vice_turbo_pulse";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      turbo_pulse = atoi(var.value);
   }

   var.key = "vice_reset";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if      (!strcmp(var.value, "autostart")) opt_reset_type = 0;
      else if (!strcmp(var.value, "soft"))      opt_reset_type = 1;
      else if (!strcmp(var.value, "hard"))      opt_reset_type = 2;
      else if (!strcmp(var.value, "freeze"))    opt_reset_type = 3;
   }

   var.key = "vice_vkbd_theme";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if      (strstr(var.value, "auto"))    opt_vkbd_theme = 0;
      else if (strstr(var.value, "brown"))   opt_vkbd_theme = 1;
      else if (strstr(var.value, "beige"))   opt_vkbd_theme = 2;
      else if (strstr(var.value, "dark"))    opt_vkbd_theme = 3;
      else if (strstr(var.value, "light"))   opt_vkbd_theme = 4;

      if      (strstr(var.value, "outline")) opt_vkbd_theme |= 0x80;
   }

   var.key = "vice_vkbd_transparency";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if      (!strcmp(var.value, "0%"))   opt_vkbd_alpha = GRAPH_ALPHA_100;
      else if (!strcmp(var.value, "25%"))  opt_vkbd_alpha = GRAPH_ALPHA_75;
      else if (!strcmp(var.value, "50%"))  opt_vkbd_alpha = GRAPH_ALPHA_50;
      else if (!strcmp(var.value, "75%"))  opt_vkbd_alpha = GRAPH_ALPHA_25;
      else if (!strcmp(var.value, "100%")) opt_vkbd_alpha = GRAPH_ALPHA_0;
   }

   var.key = "vice_statusbar";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      opt_statusbar = 0;

      if (strstr(var.value, "top"))     opt_statusbar |= STATUSBAR_TOP;
      else                              opt_statusbar |= STATUSBAR_BOTTOM;

      if (strstr(var.value, "basic"))   opt_statusbar |= STATUSBAR_BASIC;
      if (strstr(var.value, "minimal")) opt_statusbar |= STATUSBAR_MINIMAL;
   }

   var.key = "vice_mapping_options_display";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "disabled")) opt_mapping_options_display = 0;
      else                                opt_mapping_options_display = 1;
   }

   var.key = "vice_audio_options_display";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "disabled")) opt_audio_options_display = 0;
      else                                opt_audio_options_display = 1;
   }

   var.key = "vice_video_options_display";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "disabled")) opt_video_options_display = 0;
      else                                opt_video_options_display = 1;
   }

   var.key = "vice_read_vicerc";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int opt_read_vicerc_prev = opt_read_vicerc;
      if (!strcmp(var.value, "disabled")) opt_read_vicerc = 0;
      else                                opt_read_vicerc = 1;

      if (retro_ui_finalized)
         request_reload_restart = (opt_read_vicerc != opt_read_vicerc_prev) ? true : request_reload_restart;
   }

#if defined(__XSCPU64__)
   var.key = "vice_supercpu_speed_switch";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int speedswitch = 0;
      if (!strcmp(var.value, "enabled")) speedswitch = 1;

      if (retro_ui_finalized && vice_opt.SpeedSwitch != speedswitch)
         log_resources_set_int("SpeedSwitch", speedswitch);

      vice_opt.SpeedSwitch = speedswitch;
   }

   var.key = "vice_supercpu_kernal";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int opt_supercpu_kernal_prev = opt_supercpu_kernal;
      opt_supercpu_kernal = atoi(var.value);

      if (retro_ui_finalized)
         request_reload_restart = (opt_supercpu_kernal != opt_supercpu_kernal_prev) ? true : request_reload_restart;
   }
#endif

#if defined(__X64__) || defined(__X64SC__) || defined(__X128__) || defined(__XSCPU64__)
   var.key = "vice_jiffydos";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int opt_jiffydos_prev = opt_jiffydos;
      if (!strcmp(var.value, "disabled")) opt_jiffydos = 0;
      else                                opt_jiffydos = 1;

      /* Forcefully disable JiffyDOS if TDE is disabled */
      if (!vice_opt.DriveTrueEmulation)
         opt_jiffydos = 0;

      if (!opt_jiffydos_allow)
         opt_jiffydos = 0;

      if (retro_ui_finalized)
         request_reload_restart = (opt_jiffydos != opt_jiffydos_prev) ? true : request_reload_restart;
   }
#endif

   /* Mapper */
   /* RetroPad */
   var.key = "vice_mapper_select";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[RETRO_DEVICE_ID_JOYPAD_SELECT] = retro_keymap_id(var.value);
   }

   var.key = "vice_mapper_start";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[RETRO_DEVICE_ID_JOYPAD_START] = retro_keymap_id(var.value);
   }

   var.key = "vice_mapper_b";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[RETRO_DEVICE_ID_JOYPAD_B] = retro_keymap_id(var.value);
   }

   var.key = "vice_mapper_a";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[RETRO_DEVICE_ID_JOYPAD_A] = retro_keymap_id(var.value);
   }

   var.key = "vice_mapper_y";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[RETRO_DEVICE_ID_JOYPAD_Y] = retro_keymap_id(var.value);
   }

   var.key = "vice_mapper_x";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[RETRO_DEVICE_ID_JOYPAD_X] = retro_keymap_id(var.value);
   }

   var.key = "vice_mapper_l";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[RETRO_DEVICE_ID_JOYPAD_L] = retro_keymap_id(var.value);
   }

   var.key = "vice_mapper_r";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[RETRO_DEVICE_ID_JOYPAD_R] = retro_keymap_id(var.value);
   }

   var.key = "vice_mapper_l2";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[RETRO_DEVICE_ID_JOYPAD_L2] = retro_keymap_id(var.value);
   }

   var.key = "vice_mapper_r2";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[RETRO_DEVICE_ID_JOYPAD_R2] = retro_keymap_id(var.value);
   }

   var.key = "vice_mapper_l3";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[RETRO_DEVICE_ID_JOYPAD_L3] = retro_keymap_id(var.value);
   }

   var.key = "vice_mapper_r3";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[RETRO_DEVICE_ID_JOYPAD_R3] = retro_keymap_id(var.value);
   }

   var.key = "vice_mapper_lr";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[RETRO_DEVICE_ID_JOYPAD_LR] = retro_keymap_id(var.value);
   }

   var.key = "vice_mapper_ll";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[RETRO_DEVICE_ID_JOYPAD_LL] = retro_keymap_id(var.value);
   }

   var.key = "vice_mapper_ld";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[RETRO_DEVICE_ID_JOYPAD_LD] = retro_keymap_id(var.value);
   }

   var.key = "vice_mapper_lu";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[RETRO_DEVICE_ID_JOYPAD_LU] = retro_keymap_id(var.value);
   }

   var.key = "vice_mapper_rr";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[RETRO_DEVICE_ID_JOYPAD_RR] = retro_keymap_id(var.value);
   }

   var.key = "vice_mapper_rl";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[RETRO_DEVICE_ID_JOYPAD_RL] = retro_keymap_id(var.value);
   }

   var.key = "vice_mapper_rd";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[RETRO_DEVICE_ID_JOYPAD_RD] = retro_keymap_id(var.value);
   }

   var.key = "vice_mapper_ru";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[RETRO_DEVICE_ID_JOYPAD_RU] = retro_keymap_id(var.value);
   }

   /* Hotkeys */
   var.key = "vice_mapper_vkbd";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[RETRO_MAPPER_VKBD] = retro_keymap_id(var.value);
   }

   var.key = "vice_mapper_statusbar";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[RETRO_MAPPER_STATUSBAR] = retro_keymap_id(var.value);
   }

   var.key = "vice_mapper_joyport_switch";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[RETRO_MAPPER_JOYPORT] = retro_keymap_id(var.value);
   }

   var.key = "vice_mapper_reset";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[RETRO_MAPPER_RESET] = retro_keymap_id(var.value);
   }

   var.key = "vice_mapper_aspect_ratio_toggle";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[RETRO_MAPPER_ASPECT_RATIO] = retro_keymap_id(var.value);
   }

   var.key = "vice_mapper_zoom_mode_toggle";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[RETRO_MAPPER_ZOOM_MODE] = retro_keymap_id(var.value);
   }

   var.key = "vice_mapper_warp_mode";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[RETRO_MAPPER_WARP_MODE] = retro_keymap_id(var.value);
   }

   var.key = "vice_datasette_hotkeys";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "disabled")) datasette_hotkeys = false;
      else                                datasette_hotkeys = true;
   }

   var.key = "vice_mapper_datasette_toggle_hotkeys";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[RETRO_MAPPER_DATASETTE_HOTKEYS] = retro_keymap_id(var.value);
   }
   
   var.key = "vice_mapper_datasette_stop";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[RETRO_MAPPER_DATASETTE_STOP] = retro_keymap_id(var.value);
   }

   var.key = "vice_mapper_datasette_start";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[RETRO_MAPPER_DATASETTE_START] = retro_keymap_id(var.value);
   }

   var.key = "vice_mapper_datasette_forward";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[RETRO_MAPPER_DATASETTE_FORWARD] = retro_keymap_id(var.value);
   }

   var.key = "vice_mapper_datasette_rewind";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[RETRO_MAPPER_DATASETTE_REWIND] = retro_keymap_id(var.value);
   }

   var.key = "vice_mapper_datasette_reset";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[RETRO_MAPPER_DATASETTE_RESET] = retro_keymap_id(var.value);
   }


   /*** Options display ***/

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
#if !defined(__XPET__) && !defined(__XCBM2__) && !defined(__XVIC__)
   option_display.key = "vice_mapper_joyport_switch";
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
#endif
#if defined(__X64__) || defined(__X64SC__) || defined(__X64DTV__) || defined(__X128__) || defined(__XSCPU64__) || defined(__XCBM5x0__) || defined(__XVIC__) || defined(__XPLUS4__)
   option_display.key = "vice_mapper_aspect_ratio_toggle";
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
   option_display.key = "vice_mapper_zoom_mode_toggle";
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
   option_display.key = "vice_datasette_sound";
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
#if defined(__X64__) || defined(__X64SC__) || defined(__X64DTV__) || defined(__X128__) || defined(__XSCPU64__) || defined(__XCBM5x0__) || defined(__XVIC__) || defined(__XPLUS4__)
   option_display.key = "vice_audio_leak_emulation";
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
#endif
#if !defined(__XPET__) && !defined(__XPLUS4__) && !defined(__XVIC__)
   option_display.key = "vice_sid_engine";
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
   option_display.key = "vice_sid_model";
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
   option_display.key = "vice_sid_extra";
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
#if defined(__X64__) || defined(__X64SC__) || defined(__X128__)
   option_display.key = "vice_sfx_sound_expander";
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
#endif
   option_display.key = "vice_sound_sample_rate";
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);

   /* Video options */
   option_display.visible = opt_video_options_display;

   option_display.key = "vice_vkbd_theme";
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
   option_display.key = "vice_vkbd_transparency";
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
   option_display.key = "vice_statusbar";
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
   option_display.key = "vice_gfx_colors";
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
   option_display.key = "vice_joyport_pointer_color";
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
#if defined(__X64__) || defined(__X64SC__) || defined(__X64DTV__) || defined(__X128__) || defined(__XSCPU64__) || defined(__XCBM5x0__) || defined(__XVIC__) || defined(__XPLUS4__)
   option_display.key = "vice_zoom_mode";
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
   option_display.key = "vice_zoom_mode_crop";
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
   option_display.key = "vice_aspect_ratio";
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
   option_display.key = "vice_manual_crop_top";
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
   option_display.key = "vice_manual_crop_bottom";
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
   option_display.key = "vice_manual_crop_left";
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
   option_display.key = "vice_manual_crop_right";
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
#endif
#if defined(__XVIC__)
   option_display.key = "vice_vic20_external_palette";
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
   option_display.key = "vice_vic_filter",
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
   option_display.key = "vice_vic_color_gamma",
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
   option_display.key = "vice_vic_color_tint",
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
   option_display.key = "vice_vic_color_saturation",
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
   option_display.key = "vice_vic_color_contrast",
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
   option_display.key = "vice_vic_color_brightness",
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
#elif defined(__XPLUS4__)
   option_display.key = "vice_plus4_external_palette";
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
   option_display.key = "vice_ted_filter",
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
   option_display.key = "vice_ted_color_gamma",
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
   option_display.key = "vice_ted_color_tint",
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
   option_display.key = "vice_ted_color_saturation",
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
   option_display.key = "vice_ted_color_contrast",
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
   option_display.key = "vice_ted_color_brightness",
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
#elif defined(__XPET__)
   option_display.key = "vice_pet_external_palette";
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
   option_display.key = "vice_crtc_filter",
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
#elif defined(__XCBM2__)
   option_display.key = "vice_cbm2_external_palette";
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
   option_display.key = "vice_crtc_filter",
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
#else
   option_display.key = "vice_external_palette";
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
   option_display.key = "vice_vicii_filter",
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
   option_display.key = "vice_vicii_color_gamma",
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
   option_display.key = "vice_vicii_color_tint",
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
   option_display.key = "vice_vicii_color_saturation",
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
   option_display.key = "vice_vicii_color_contrast",
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
   option_display.key = "vice_vicii_color_brightness",
   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
#endif
}

void emu_reset(int type)
{
   /* Reset Datasette or autostart from tape will fail */
   datasette_control(DATASETTE_CONTROL_RESET);

   /* Release keyboard keys */
   keyboard_clear_keymatrix();

   /* Disable Warp */
   resources_set_int("WarpMode", 0);

   /* Changing opt_read_vicerc requires reloading */
   if (request_reload_restart)
      reload_restart();

   /* Follow core option type with -1 */
   type = (type == -1) ? opt_reset_type : type;
   switch (type)
   {
      case 0:
         /* Hard reset before autostart */
         machine_trigger_reset(MACHINE_RESET_MODE_HARD);

         /* Check command line on autostart */
         if (dc->command || CMDFILE[0])
         {
            initcmdline_check_attach();
            break;
         }

         /* Build direct launch PRG */
         if (dc->load[dc->index])
         {
            autostartString  = strdup(dc->files[dc->index]);
            path_remove_program(autostartString);
            autostartProgram = strdup(dc->load[dc->index]);
            charset_petconvstring((uint8_t *)autostartProgram, 0);
         }
         else
            autostartProgram = NULL;

         /* Allow autostarting with a different disk */
         if (dc->count > 1)
            autostartString = x_strdup(dc->files[dc->index]);
         if (autostartString != NULL && autostartString[0] != '\0' && !noautostart)
            autostart_autodetect(autostartString, autostartProgram, 0, AUTOSTART_MODE_RUN);
         break;
      case 1:
         machine_trigger_reset(MACHINE_RESET_MODE_SOFT);

         /* Allow restarting PRGs with RUN */
         if (autostartString != NULL && autostartString[0] != '\0' && strendswith(autostartString, "prg"))
            autostart_autodetect(autostartString, autostartProgram, 0, AUTOSTART_MODE_NONE);
         break;
      case 2:
         machine_trigger_reset(MACHINE_RESET_MODE_HARD);
         break;
      case 3:
         cartridge_trigger_freeze();
         break;
   }
}

/*****************************************************************************/
/* Disk Control */
bool retro_disk_set_eject_state(bool ejected)
{
   if (dc)
   {
      int unit = retro_disk_get_image_unit();

      if (dc->eject_state == ejected)
         return true;
      else
         dc->eject_state = ejected;

      if (!dc->files[dc->index])
         return false;

      if (path_is_valid(dc->files[dc->index]))
          display_current_image(((!dc->eject_state) ? dc->labels[dc->index] : ""), !dc->eject_state);

      if (dc->eject_state)
      {
         switch (unit)
         {
            case 0:
               cartridge_detach_image(-1);
               break;
            case 1:
               tape_image_detach(unit);
               break;
            default:
               file_system_detach_disk(unit, 0);
               break;
         }
      }
      else if (path_is_valid(dc->files[dc->index]))
      {
         switch (unit)
         {
            case 0:
#if defined(__XVIC__)
               cartridge_attach_image(vic20_autodetect_cartridge_type(dc->files[dc->index]), dc->files[dc->index]);
#elif defined(__XPLUS4__)
               cartridge_attach_image(CARTRIDGE_PLUS4_DETECT, dc->files[dc->index]);
               /* Soft reset required, otherwise gfx gets corrupted (?!) */
               emu_reset(1);
#else
               cartridge_attach_image(unit, dc->files[dc->index]);
#endif
               /* PRGs must autostart on attach, cartridges reset anyway */
               if (strendswith(dc->files[dc->index], "prg"))
                  emu_reset(0);
               break;
            case 1:
               tape_image_attach(unit, dc->files[dc->index]);
               break;
            default:
               file_system_attach_disk(unit, 0, dc->files[dc->index]);
               autodetect_drivetype(unit);
               break;
         }
      }
   }

   return true;
}

bool retro_disk_get_eject_state(void)
{
   if (dc)
      return dc->eject_state;

   return true;
}

static unsigned retro_disk_get_image_index(void)
{
   if (dc)
      return dc->index;

   return 0;
}

bool retro_disk_set_image_index(unsigned index)
{
   if (dc)
   {
      if (index == dc->index)
         return true;

      if (dc->replace)
      {
         dc->replace = false;
         index = 0;
      }

      if (index < dc->count && dc->files[index])
      {
         dc->index = index;
         display_current_image(dc->labels[dc->index], false);
         return true;
      }
   }

   return false;
}

static unsigned retro_disk_get_num_images(void)
{
   if (dc)
      return dc->count;

   return 0;
}

static bool retro_disk_replace_image_index(unsigned index, const struct retro_game_info *info)
{
   if (dc)
   {
      if (info != NULL)
         dc_replace_file(dc, index, info->path);
      else
         dc_remove_file(dc, index);
      return true;
   }

   return false;
}

static bool retro_disk_add_image_index(void)
{
   if (dc)
   {
      if (dc->count <= DC_MAX_SIZE)
      {
         dc->files[dc->count]       = NULL;
         dc->labels[dc->count]      = NULL;
         dc->disk_labels[dc->count] = NULL;
         dc->load[dc->count]        = NULL;
         dc->types[dc->count]       = DC_IMAGE_TYPE_NONE;
         dc->count++;
         return true;
      }
   }

   return false;
}

static bool retro_disk_get_image_path(unsigned index, char *path, size_t len)
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

static bool retro_disk_get_image_label(unsigned index, char *label, size_t len)
{
   if (len < 1)
      return false;

   if (dc)
   {
      if (index < dc->count)
      {
         if (!string_is_empty(dc->labels[index]))
         {
            strlcpy(label, dc->labels[index], len);
            return true;
         }
      }
   }

   return false;
}

static struct retro_disk_control_callback disk_interface = {
   retro_disk_set_eject_state,
   retro_disk_get_eject_state,
   retro_disk_get_image_index,
   retro_disk_set_image_index,
   retro_disk_get_num_images,
   retro_disk_replace_image_index,
   retro_disk_add_image_index,
};

static struct retro_disk_control_ext_callback disk_interface_ext = {
   retro_disk_set_eject_state,
   retro_disk_get_eject_state,
   retro_disk_get_image_index,
   retro_disk_set_image_index,
   retro_disk_get_num_images,
   retro_disk_replace_image_index,
   retro_disk_add_image_index,
   NULL, /* set_initial_image */
   retro_disk_get_image_path,
   retro_disk_get_image_label,
};

/*****************************************************************************/

void retro_reset(void)
{
   /* Reset DC index to first entry */
   if (dc)
   {
      dc->index = 0;
      retro_disk_set_eject_state(true);
      retro_disk_set_eject_state(false);
   }

   /* Trigger autostart-reset in retro_run() */
   request_restart = true;
}

static void fallback_log(enum retro_log_level level, const char *fmt, ...)
{
   (void)level;
   va_list va;
   va_start(va, fmt);
   vfprintf(stderr, fmt, va);
   va_end(va);
}

void retro_init(void)
{
   struct retro_log_callback log;
   log_cb = fallback_log;
   if (environ_cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log))
      log_cb = log.log;

   if (!environ_cb(RETRO_ENVIRONMENT_GET_PERF_INTERFACE, &perf_cb))
      perf_cb.get_time_usec = NULL;

   retro_set_paths();

   /* Clean ZIP temp */
   if (!string_is_empty(retro_temp_directory) && path_is_directory(retro_temp_directory))
      remove_recurse(retro_temp_directory);

   /* Disk Control interface */
   dc = dc_create();
   unsigned dci_version = 0;
   if (environ_cb(RETRO_ENVIRONMENT_GET_DISK_CONTROL_INTERFACE_VERSION, &dci_version) && (dci_version >= 1))
      environ_cb(RETRO_ENVIRONMENT_SET_DISK_CONTROL_EXT_INTERFACE, &disk_interface_ext);
   else
      environ_cb(RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE, &disk_interface);

   /* Keep as incomplete until rewind can be enabled at startup (snapshot size is 0 at that time) */
   static uint64_t quirks = RETRO_SERIALIZATION_QUIRK_INCOMPLETE | RETRO_SERIALIZATION_QUIRK_MUST_INITIALIZE | RETRO_SERIALIZATION_QUIRK_CORE_VARIABLE_SIZE;
   environ_cb(RETRO_ENVIRONMENT_SET_SERIALIZATION_QUIRKS, &quirks);

   /* Inputs */
   #define RETRO_DESCRIPTOR_BLOCK(_user)                                                                        \
   { _user, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP, "Up" },                                          \
   { _user, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN, "Down" },                                      \
   { _user, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT, "Left" },                                      \
   { _user, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "Right" },                                    \
   { _user, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B, "B / Fire" },                                     \
   { _user, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A, "A" },                                            \
   { _user, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y, "Y" },                                            \
   { _user, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X, "X" },                                            \
   { _user, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "Select" },                                  \
   { _user, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "Start" },                                    \
   { _user, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L, "L" },                                            \
   { _user, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R, "R" },                                            \
   { _user, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2, "L2" },                                          \
   { _user, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2, "R2" },                                          \
   { _user, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L3, "L3" },                                          \
   { _user, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R3, "R3" },                                          \
   { _user, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X, "Left Analog X" },   \
   { _user, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y, "Left Analog Y" },   \
   { _user, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X, "Right Analog X" }, \
   { _user, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y, "Right Analog Y" }

   static struct retro_input_descriptor input_descriptors[] =
   {
      RETRO_DESCRIPTOR_BLOCK(0),
      RETRO_DESCRIPTOR_BLOCK(1),
      RETRO_DESCRIPTOR_BLOCK(2),
      RETRO_DESCRIPTOR_BLOCK(3),
      RETRO_DESCRIPTOR_BLOCK(4),
      {0},
   };
   #undef RETRO_DESCRIPTOR_BLOCK
   environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, &input_descriptors);

   if (environ_cb(RETRO_ENVIRONMENT_GET_INPUT_BITMASKS, NULL))
      libretro_supports_bitmasks = true;

   static struct retro_keyboard_callback keyboard_callback = {retro_keyboard_event};
   environ_cb(RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK, &keyboard_callback);

   enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_RGB565;
   if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
   {
      log_cb(RETRO_LOG_ERROR, "RGB565 is not supported.\n");
      environ_cb(RETRO_ENVIRONMENT_SHUTDOWN, NULL);
      return;
   }

   bool achievements = true;
   environ_cb(RETRO_ENVIRONMENT_SET_SUPPORT_ACHIEVEMENTS, &achievements);

   memset(retro_bmp, 0, sizeof(retro_bmp));

   retro_ui_finalized = false;
   update_variables();
}

void retro_deinit(void)
{
   /* VICE shutdown */
   machine_shutdown();

   /* Clean Disc Control context */
   if (dc)
      dc_free(dc);

   /* Clean legacy strings */
   if (core_options_legacy_strings)
      free(core_options_legacy_strings);

   /* Clean dynamic cartridge info */
   free_vice_carts();

   /* Clean ZIP temp */
   if (!string_is_empty(retro_temp_directory) && path_is_directory(retro_temp_directory))
      remove_recurse(retro_temp_directory);

   /* Free buffers uses by libretro-graph */
   libretro_graph_free();

   /* 'Reset' troublesome static variables */
   libretro_supports_bitmasks = false;
   pix_bytes_initialized = false;
   cur_port_locked = false;
   opt_aspect_ratio_locked = false;
   request_model_set = -1;
   request_model_prev = -1;
   opt_model_auto = 1;
}

unsigned retro_api_version(void)
{
   return RETRO_API_VERSION;
}

void retro_set_controller_port_device(unsigned port, unsigned device)
{
   if (port < RETRO_DEVICES)
      retro_devices[port] = device;
}

void retro_get_system_info(struct retro_system_info *info)
{
#ifndef GIT_VERSION
#define GIT_VERSION ""
#endif
   memset(info, 0, sizeof(*info));
   info->library_name     = "VICE " CORE_NAME;
   info->library_version  = "3.5" GIT_VERSION;
#if defined(__XVIC__)
   info->valid_extensions = "d64|d71|d80|d81|d82|g64|g41|x64|t64|tap|prg|p00|crt|bin|zip|7z|gz|d6z|d7z|d8z|g6z|g4z|x6z|cmd|m3u|vfl|vsf|nib|nbz|d2m|d4m|20|40|60|a0|b0|rom";
#else
   info->valid_extensions = "d64|d71|d80|d81|d82|g64|g41|x64|t64|tap|prg|p00|crt|bin|zip|7z|gz|d6z|d7z|d8z|g6z|g4z|x6z|cmd|m3u|vfl|vsf|nib|nbz|d2m|d4m";
#endif
   info->need_fullpath    = true;
   info->block_extract    = true;
}

double retro_get_aspect_ratio(unsigned int width, unsigned int height, bool pixel_aspect)
{
   static double ar;
   static double par;
   static int region = 0;
   region = retro_region;
   switch (opt_aspect_ratio)
   {
      case 1:
         region = RETRO_REGION_PAL;
         break;
      case 2:
         region = RETRO_REGION_NTSC;
         break;
      case 3:
         region = -1;
         par = 1;
         break;
   }

#if defined(__X64__) || defined(__X64SC__) || defined(__X64DTV__) || defined(__X128__) || defined(__XSCPU64__) || defined(__XCBM5x0__)
      switch (region)
      {
         case RETRO_REGION_NTSC:
            par = (double)0.75000000;
            break;
         case RETRO_REGION_PAL:
            par = (double)0.93650794;
            break;
      }
      ar = ((double)width / (double)height) * par;
#if defined(__X128__)
      if (vice_opt.C128ColumnKey == 0)
         ar = ((double)width / (double)height) / (double)2.0;
#endif
#elif defined(__XVIC__)
      switch (region)
      {
         case RETRO_REGION_NTSC:
            par = ((double)1.50411479 / (double)2.0);
            break;
         case RETRO_REGION_PAL:
            par = ((double)1.66574035 / (double)2.0);
            break;
      }
      ar = ((double)width / (double)height) * par;
#elif defined(__XPLUS4__)
      switch (region)
      {
         case RETRO_REGION_NTSC:
            par = (double)0.85760931;
            break;
         case RETRO_REGION_PAL:
            par = (double)1.03743478;
            break;
      }
      ar = ((double)width / (double)height) * par;
#else
      ar = (double)4 / (double)3;
#endif

   if (pixel_aspect)
      return par;
   if (opt_aspect_ratio == 3) /* 1:1 */
      return ((double)width / (double)height);
   return ar;
}

void update_geometry(int mode)
{
   struct retro_system_av_info system_av_info;

#ifdef RETRO_DEBUG
   log_cb(RETRO_LOG_INFO, "Update Geometry: Old(%d,%d) New(%d,%d)\n", lastw, lasth, retrow, retroh);
#endif
   lastw = retrow;
   lasth = retroh;

   switch (mode)
   {
      case 0:
         /* Zoom mode init */
         zoom_mode_id_prev  = 0;
         zoomed_width       = retrow;
         zoomed_height      = retroh;
         zoomed_XS_offset   = 0;
         zoomed_YS_offset   = 0;
         retroXS_offset     = 0;
         retroYS_offset     = 0;

         system_av_info.geometry.base_width = retrow;
         system_av_info.geometry.base_height = retroh;
         system_av_info.geometry.aspect_ratio = retro_get_aspect_ratio(retrow, retroh, false);

         /* Update av_info only when PAL/NTSC change occurs */
         if (retro_region != retro_get_region())
         {
            retro_get_system_av_info(&system_av_info);
            environ_cb(RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO, &system_av_info);
            return;
         }
         break;

      case 1:
#if defined(__X64__) || defined(__X64SC__) || defined(__X64DTV__) || defined(__X128__) || defined(__XSCPU64__) || defined(__XCBM5x0__) || defined(__XVIC__) || defined(__XPLUS4__)
         if (zoom_mode_id != zoom_mode_id_prev)
         {
            zoom_mode_id_prev = zoom_mode_id;
#if defined(__X64__) || defined(__X64SC__) || defined(__X64DTV__) || defined(__X128__) || defined(__XSCPU64__) || defined(__XCBM5x0__)
            /* PAL: 384x272, NTSC: 384x247, VIC-II: 320x200 */
            int zoom_width_max      = 320;
            int zoom_height_max     = 200;
#elif defined(__XVIC__)
            /* PAL: 448x284, NTSC: 400x234, VIC: 352x184 */
            int zoom_width_max      = 352;
            int zoom_height_max     = 184;
#elif defined(__XPLUS4__)
            /* PAL: 384x288, NTSC: 384x242, TED: 320x200 */
            int zoom_width_max      = 320;
            int zoom_height_max     = 200;
#endif
            int zoom_crop_width     = 0;
            int zoom_crop_height    = 0;
            int zoom_border_width   = 0;
            int zoom_border_height  = 0;

            double zoom_dar = 0;
            double zoom_par = retro_get_aspect_ratio(0, 0, true);

            switch (zoom_mode_id)
            {
               case 1:
               case 2:
               case 3:
                  switch (zoom_mode_id)
                  {
                     case 1: /* Small */
                        zoom_border_width     = 44;
                        zoom_border_height    = 36;
                        break;
                     case 2: /* Medium */
                        zoom_border_width     = 22;
                        zoom_border_height    = 18;
                        break;
                     case 3: /* Maximum */
                        break;
                  }

                  zoom_crop_width = retrow - zoom_width_max - zoom_border_width;
                  zoom_crop_height = retroh - zoom_height_max - zoom_border_height;

                  switch (zoom_mode_crop_id)
                  {
                     case 0: /* Both */
                        break;
                     case 1: /* Vertical disables horizontal crop */
                        zoom_crop_width = 0;
                        break;
                     case 2: /* Horizontal disables vertical crop */
                        zoom_crop_height = 0;
                        break;
                     case 3: /* 16:9 */
                        zoom_dar = (double)16/9;
                        break;
                     case 4: /* 16:10 */
                        zoom_dar = (double)16/10;
                        break;
                     case 5: /* 4:3 */
                        zoom_dar = (double)4/3;
                        break;
                     case 6: /* 5:4 */
                        zoom_dar = (double)5/4;
                        break;
                  }

                  if (zoom_dar > 0)
                  {
                     if (zoom_mode_crop_id > 4)
                        zoom_crop_height = retroh - zoom_height_max - ((double)zoom_border_height * (double)zoom_dar / (double)zoom_par);
                     zoom_crop_width = retrow - ((double)(retroh - zoom_crop_height) * (double)zoom_dar / (double)zoom_par);
                     if (retrow - zoom_crop_width <= zoom_width_max)
                        zoom_crop_height = retroh - ((double)(zoom_width_max) / (double)zoom_dar * (double)zoom_par);
                  }

                  if (retrow - zoom_crop_width < zoom_width_max)
                     zoom_crop_width = retrow - zoom_width_max;
                  if (retroh - zoom_crop_height < zoom_height_max)
                     zoom_crop_height = retroh - zoom_height_max;

                  if (zoom_crop_width < 0)
                     zoom_crop_width = 0;
                  if (zoom_crop_height < 0)
                     zoom_crop_height = 0;

                  zoomed_width        = retrow - zoom_crop_width;
                  zoomed_height       = retroh - zoom_crop_height;
#if 0
                  printf("zoom: dar:%f par:%f - x-%3d y-%3d = %3dx%3d = %f * %f = %f\n", zoom_dar, zoom_par, zoom_crop_width, zoom_crop_height, zoomed_width, zoomed_height, ((double)zoomed_width / (double)zoomed_height), zoom_par, ((double)zoomed_width / (double)zoomed_height * zoom_par));
#endif
#if defined(__X64__) || defined(__X64SC__) || defined(__X64DTV__) || defined(__X128__) || defined(__XSCPU64__) || defined(__XCBM5x0__)
                  zoomed_XS_offset    = (zoom_crop_width > 1) ? (zoom_crop_width / 2) : 0;
                  zoomed_YS_offset    = (zoom_crop_height > 1) ? (zoom_crop_height / 2) - ((retro_region == RETRO_REGION_PAL) ? 1 : 0) : 0;
#elif defined(__XVIC__)
                  zoomed_XS_offset    = (zoom_crop_width > 1) ? (zoom_crop_width / 2) - ((retro_region == RETRO_REGION_PAL) ? 0 : -8) : 0;
                  zoomed_YS_offset    = (zoom_crop_height > 1) ? (zoom_crop_height / 2) - ((retro_region == RETRO_REGION_PAL) ? 2 : 3) : 0;
#elif defined(__XPLUS4__)
                  zoomed_XS_offset    = (zoom_crop_width > 1) ? (zoom_crop_width / 2) : 0;
                  zoomed_YS_offset    = (zoom_crop_height > 1) ? (zoom_crop_height / 2) - ((retro_region == RETRO_REGION_PAL) ? 4 : 3) : 0;
#endif
                  break;

               case 4:
                  zoom_crop_width    = manual_crop_left + manual_crop_right;
                  zoom_crop_height   = manual_crop_top + manual_crop_bottom;

                  zoomed_width       = retrow - zoom_crop_width;
                  zoomed_height      = retroh - zoom_crop_height;
                  zoomed_XS_offset   = manual_crop_left;
                  zoomed_YS_offset   = manual_crop_top;
                  break;

               default:
                  zoomed_width        = retrow;
                  zoomed_height       = retroh;
                  zoomed_XS_offset    = 0;
                  zoomed_YS_offset    = 0;
                  retroXS_offset      = 0;
                  retroYS_offset      = 0;
                  break;
            }

            system_av_info.geometry.base_width = zoomed_width;
            system_av_info.geometry.base_height = zoomed_height;
            system_av_info.geometry.aspect_ratio = retro_get_aspect_ratio(zoomed_width, zoomed_height, false);
         }
#endif
         break;
   }
   environ_cb(RETRO_ENVIRONMENT_SET_GEOMETRY, &system_av_info);
}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
   /* Need to do this here because core option values are not available in retro_init */
   if (!pix_bytes_initialized)
   {
      pix_bytes_initialized = true;
      if (pix_bytes == 4)
      {
         enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_XRGB8888;
         if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
         {
            pix_bytes = 2;
            log_cb(RETRO_LOG_INFO, "XRGB8888 is not supported. Trying RGB565.\n");
            fmt = RETRO_PIXEL_FORMAT_RGB565;
            if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
            {
               log_cb(RETRO_LOG_INFO, "RGB565 is not supported.\n");
               exit(0);
            }
         }
      }
   }

   /* Remember region for av_info update */
   retro_region = retro_get_region();

   info->geometry.max_width    = defaultw;
   info->geometry.max_height   = defaulth;
   info->geometry.base_width   = retrow;
   info->geometry.base_height  = retroh;
   info->geometry.aspect_ratio = retro_get_aspect_ratio(retrow, retroh, false);
   info->timing.sample_rate    = vice_opt.SoundSampleRate;
   prev_sound_sample_rate      = vice_opt.SoundSampleRate;

#if defined(__X64__) || defined(__X64SC__) || defined(__X64DTV__)
   retro_refresh = (retro_region == RETRO_REGION_PAL) ? C64_PAL_RFSH_PER_SEC : C64_NTSC_RFSH_PER_SEC;
#elif defined(__X128__)
   retro_refresh = (retro_region == RETRO_REGION_PAL) ? C128_PAL_RFSH_PER_SEC : C128_NTSC_RFSH_PER_SEC;
#elif defined(__XCBM2__)
   retro_refresh = (retro_region == RETRO_REGION_PAL) ? C610_PAL_RFSH_PER_SEC : C610_NTSC_RFSH_PER_SEC;
#elif defined(__XCBM5x0__)
   retro_refresh = (retro_region == RETRO_REGION_PAL) ? C500_PAL_RFSH_PER_SEC : C500_NTSC_RFSH_PER_SEC;
#elif defined(__XPET__)
   retro_refresh = (retro_region == RETRO_REGION_PAL) ? PET_PAL_RFSH_PER_SEC : PET_NTSC_RFSH_PER_SEC;
#elif defined(__XPLUS4__)
   retro_refresh = (retro_region == RETRO_REGION_PAL) ? PLUS4_PAL_RFSH_PER_SEC : PLUS4_NTSC_RFSH_PER_SEC;
#elif defined(__XSCPU64__)
   retro_refresh = (retro_region == RETRO_REGION_PAL) ? SCPU64_PAL_RFSH_PER_SEC : SCPU64_NTSC_RFSH_PER_SEC;
#elif defined(__XVIC__)
   retro_refresh = (retro_region == RETRO_REGION_PAL) ? VIC20_PAL_RFSH_PER_SEC : VIC20_NTSC_RFSH_PER_SEC;
#endif
   info->timing.fps = retro_refresh;
}

void retro_set_video_refresh(retro_video_refresh_t cb)
{
   video_cb = cb;
}

void retro_set_audio_sample(retro_audio_sample_t cb)
{
   audio_cb = cb;
}

void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb)
{
   audio_batch_cb = cb;
}

#define RETRO_AUDIO_BATCH

void retro_audio_render(const int16_t *data, size_t frames)
{
   if ((frames < 1) || !runstate)
      return;
#if ARCHDEP_SOUND_OUTPUT_MODE == SOUND_OUTPUT_STEREO
#ifdef RETRO_AUDIO_BATCH
   audio_batch_cb(data, frames/2);
#else
   for (int x = 0; x < frames; x += 2) audio_cb(data[x], data[x+1]);
#endif
#elif ARCHDEP_SOUND_OUTPUT_MODE == SOUND_OUTPUT_MONO
   for (int x = 0; x < frames; x++) audio_cb(data[x], data[x]);
#endif
}



void retro_run(void)
{
   /* Core options */
   bool updated = false;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated)
      update_variables();

   if (retro_ui_finalized)
   {
#if defined(__X64__) || defined(__X64SC__) || defined(__XSCPU64__) || defined(__XVIC__)
      /* Set model when requested */
      if (opt_model_auto == 1 && request_model_set > -1)
      {
         if (request_model_prev != request_model_set)
         {
#if defined(__X64__) || defined(__X64SC__) || defined(__XSCPU64__)
             if (request_model_set == C64MODEL_C64_NTSC || request_model_set == C64MODEL_C64C_NTSC)
                log_cb(RETRO_LOG_INFO, "Forcing NTSC mode\n");
             else if (request_model_set == C64MODEL_C64_PAL || request_model_set == C64MODEL_C64C_PAL)
                log_cb(RETRO_LOG_INFO, "Forcing PAL mode\n");
             else if (request_model_set == C64MODEL_C64_GS)
                log_cb(RETRO_LOG_INFO, "Forcing C64GS mode\n");
             else if (request_model_set == C64MODEL_ULTIMAX)
                log_cb(RETRO_LOG_INFO, "Forcing ULTIMAX mode\n");

             c64model_set(request_model_set);
#elif defined(__XVIC__)
             if (request_model_set == VIC20MODEL_VIC20_NTSC)
                log_cb(RETRO_LOG_INFO, "Forcing NTSC mode\n");
             else if (request_model_set == VIC20MODEL_VIC20_PAL)
                log_cb(RETRO_LOG_INFO, "Forcing PAL mode\n");

             vic20model_set(request_model_set);
#endif
             request_model_prev = request_model_set;
         }
         opt_model_auto = 2;
      }
#endif

      /* Update samplerate if changed by core option */
      if (prev_sound_sample_rate != vice_opt.SoundSampleRate)
      {
         prev_sound_sample_rate = vice_opt.SoundSampleRate;

         /* Ensure audio rendering is reinitialized on next use. */
         sound_close();

         /* Reset zoom for proper aspect ratio after av_info change */
         zoom_mode_id_prev = -1;

         struct retro_system_av_info system_av_info;
         retro_get_system_av_info(&system_av_info);
         environ_cb(RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO, &system_av_info);
      }
   }

#ifdef RETRO_DEBUG
   if (retro_ui_finalized && !prev_ui_finalized)
   {
      log_cb(RETRO_LOG_INFO, "UI finalized now\n");
      prev_ui_finalized = true;
   }
#endif

   if (retro_message)
   {
      struct retro_message msg;
      msg.msg = retro_message_msg;
      msg.frames = 500;
      environ_cb(RETRO_ENVIRONMENT_SET_MESSAGE, &msg);
      retro_message = false;
   }

   if (runstate == RUNSTATE_FIRST_START)
   {
      /* this is only done once after just loading the core from scratch and starting it */
      runstate = RUNSTATE_RUNNING;
      pre_main();
      reload_restart();
      update_geometry(0);
#ifdef RETRO_DEBUG
      log_cb(RETRO_LOG_INFO, "First time we return from retro_run()!\n");
#endif
      return;
   } 
   else if (runstate == RUNSTATE_LOADED_CONTENT)
   {
      /* Load content was called while core was already running, just do a reset with autostart */
      runstate = RUNSTATE_RUNNING;
      reload_restart();
      /* Autostart reset */
      request_restart = true;
   }
   else if (runstate == RUNSTATE_RUNNING)
   {
      /* Update work disk */
      if (request_update_work_disk)
         update_work_disk();

#if defined(__XVIC__)
      /* Autorun "SYS x" command */
      if (autosys)
      {
         autosys = false;
         vic20_autosys_run(full_path);
      }
#endif
   }

   /* Input poll */
   retro_poll_event();

   /* Main loop with Warp Mode maximizing without too much input lag */
   unsigned int frame_max = retro_warp_mode_enabled() ? retro_refresh : 1;
   unsigned int frame_time = 0;
   retro_now += 1000000 / retro_refresh;

   for (int frame_count = 0; frame_count < frame_max; ++frame_count)
   {
      frame_time = retro_ticks();
      while (retro_renderloop)
         maincpu_mainloop();
      retro_renderloop = 1;

      if (perf_cb.get_time_usec && frame_max > 1)
         frame_max = 1000000 / (retro_refresh / 5) / (retro_ticks() - frame_time);
   }

   /* LED interface */
   retro_led_interface();

   /* Virtual keyboard */
   if (retro_vkbd)
      print_vkbd();

   /* Set volume back to maximum after starting with mute, due to ReSID 6581 init pop */
   if (sound_volume_counter > 0)
   {
      sound_volume_counter--;
      if (sound_volume_counter == 0)
         resources_set_int("SoundVolume", 100);
   }

   /* Statusbar disk display timer */
   if (imagename_timer > 0)
      imagename_timer--;

   /* Video output */
   video_cb(retro_bmp+(retroXS_offset*pix_bytes/2)+(retroYS_offset*(retrow<<(pix_bytes/4))), zoomed_width, zoomed_height, retrow<<(pix_bytes/2));

   /* Update geometry if model or zoom mode changes */
   if ((lastw == retrow && lasth == retroh) && zoom_mode_id != zoom_mode_id_prev)
      update_geometry(1);
   else if (lastw != retrow || lasth != retroh)
      update_geometry(0);

   /* Finalize zoom offsets */
   if (zoomed_XS_offset != retroXS_offset || zoomed_YS_offset != retroYS_offset)
   {
      retroXS_offset = zoomed_XS_offset;
      retroYS_offset = zoomed_YS_offset;
   }

   /* retro_reset() postponed here for proper JiffyDOS+vicerc core option refresh operation */
   if (request_restart)
   {
      request_restart = false;
      emu_reset(0);
   }
}

bool retro_load_game(const struct retro_game_info *info)
{
   char *local_path;
   if (info)
   {
      /* Special unicode chars won't work internally in VICE without conversion */
      local_path = utf8_to_local_string_alloc(info->path);
      if (local_path)
      {
         process_cmdline(local_path);
         free(local_path);
         local_path = NULL;
      }
      else
         return false;
   }
   else
      /* Empty cmdline processing required for VIC-20 core option cartridges on startup */
      process_cmdline("");

#if defined(__XPET__) || defined(__XCBM2__) || defined(__XVIC__)
   /* Joyport limit has to apply always */
   cur_port = 1;
   cur_port_locked = true;
#endif

   if (runstate == RUNSTATE_RUNNING)
   {
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
   file_system_detach_disk(8, 0);
   if (opt_work_disk_type && opt_work_disk_unit == 9)
      file_system_detach_disk(9, 0);
   tape_image_detach(1);
   cartridge_detach_image(-1);
   file_system_detach_disk_shutdown();
   dc_reset(dc);

   free(autostartString);
   autostartString = NULL;
   free(autostartProgram);
   autostartProgram = NULL;
}

unsigned retro_get_region(void)
{
   int machine_sync = 0;
   if (!retro_ui_finalized)
      return retro_region;

   resources_get_int("MachineVideoStandard", &machine_sync);
   switch (machine_sync)
   {
      default:
      case MACHINE_SYNC_PAL:
      case MACHINE_SYNC_PALN:
         return RETRO_REGION_PAL;
         break;
      case MACHINE_SYNC_NTSC:
      case MACHINE_SYNC_NTSCOLD:
         return RETRO_REGION_NTSC;
         break;
   }
}

bool retro_load_game_special(unsigned type, const struct retro_game_info *info, size_t num)
{
   (void)type;
   (void)info;
   (void)num;
   return false;
}

static void dc_sync_index(void)
{
    unsigned dc_index;
    char* filename = strdup(dc_savestate_filename);
    drive_t *drive = diskunit_context[0]->drives[0];
    if (drive == NULL || string_is_empty(filename))
        return;

    if (!drive->GCR_image_loaded)
        return;

    for (dc_index = 0; dc_index < dc->count; dc_index++)
    {
        if (strcasestr(dc->files[dc_index], filename) && dc->index != dc_index)
        {
            dc->index = dc_index;
            retro_disk_set_eject_state(true);
            retro_disk_set_eject_state(false);
        }
    }
}

/* CPU traps ensure we are never saving snapshots or loading them in the middle of a cpu instruction.
   Without this, savestate corruption occurs.
*/

static void save_trap(uint16_t addr, void *success)
{
   int save_disks;
   int drive_type;
   resources_get_int("Drive8Type", &drive_type);
   save_disks = (drive_type == 1541) ? 1 : 0;

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
         maincpu_mainloop();
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
         maincpu_mainloop();
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
         maincpu_mainloop();
      if (snapshot_stream != NULL)
      {
         snapshot_fclose(snapshot_stream);
         snapshot_stream = NULL;
      }
      if (success)
      {
         resources_set_int("WarpMode", 0);
         dc_sync_index();
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
      return mem_ram_size;
   return 0;
}

void retro_cheat_reset(void) {}

void retro_cheat_set(unsigned index, bool enabled, const char *code)
{
   (void)index;
   (void)enabled;
   (void)code;
}

