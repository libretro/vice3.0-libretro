/*
 * archdep.c - Miscellaneous system-specific stuff.
 *
 * Written by
 *  Ettore Perazzoli <ettore@comm2000.it>
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

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef HAVE_VFORK_H
#include <vfork.h>
#endif

#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

#if defined(PSP) || defined(VITA)
#include <sys/time.h>
#endif

#if defined(VITA)
#include <psp2/io/stat.h>
#endif

#include "archdep.h"
#include "findpath.h"
#include "ioutil.h"
#include "lib.h"
#include "log.h"
#include "machine.h"
#include "ui.h"
#include "util.h"
#include "keyboard.h"

#define LIBRETROHACK 1
#ifdef LIBRETROHACK
//FIXME remove me!
BYTE plus4tcbm_outputa[2], plus4tcbm_outputb[2], plus4tcbm_outputc[2];
BYTE tpid_outputa[2], tpid_outputb[2], tpid_outputc[2];
/* 8502 cycle stretch indicator */
int maincpu_stretch;
/* 8502 memory refresh alarm */
 CLOCK c128cpu_memory_refresh_clk;
#endif

#include "../shared/archdep_atexit.c"
#include "../shared/archdep_extra_title_text.c"



static char *argv0 = NULL;
static char *boot_path = NULL;

/* alternate storage of preferences */
const char *archdep_pref_path = NULL; /* NULL -> use home_path + ".vice" */

#include <stddef.h>

#if defined(VITA) || defined(__SWITCH__)
char* getcwd( char* buf, size_t size )
{
  if (size > strlen(retro_system_data_directory) && buf)
  {
    strcpy(buf, retro_system_data_directory);
    return buf;
  }

  return NULL;
}

int chdir( const char* path)
{
  return 0;
}
#endif

int joystick_arch_cmdline_options_init(void)
{
        return 1;
}
/* returns host keyboard mapping. used to initialize the keyboard map when
   starting with a black (default) config, so an educated guess works good
   enough most of the time :)

   FIXME: add more languages/actual detection
*/

int kbd_arch_get_host_mapping(void)
{
    return KBD_MAPPING_US;
}

#include <time.h>

int archdep_rtc_get_centisecond(void)
{
    struct timespec dtm;
    int status;
#if defined(PSP) || defined(VITA)
    struct timeval tm;
    status = gettimeofday(&tm, NULL);
    if(status==0)
        dtm.tv_nsec = tm.tv_usec * 1000;
#else
    status = clock_gettime(CLOCK_REALTIME, &dtm);
#endif
    if (status == 0) {
        return dtm.tv_nsec / 10000L;
    }
    return 0;
}

int archdep_init(int *argc, char **argv)
{
    argv0 = lib_stralloc(argv[0]);

    archdep_pref_path = archdep_boot_path();

    return 0;
}

char *archdep_default_rtc_file_name(void)
{
    if (archdep_pref_path == NULL) {
        const char *home;

        home = archdep_home_path();
        return util_concat(home, "/.vice/vice.rtc", NULL);
    } else {
        return util_concat(archdep_pref_path, "/vice.rtc", NULL);
    }
}

int archdep_rename(const char *oldpath, const char *newpath)
{
    return rename(oldpath, newpath);
}

const char *archdep_program_name(void)
{
    static char *program_name = NULL;

    if (program_name == NULL) {
        char *p;
#if defined(__WIN32__) 
  p = strrchr(argv0, '\\');
#else
        p = strrchr(argv0, '/');
#endif
        if (p == NULL)
            program_name = lib_stralloc(argv0);
        else
            program_name = lib_stralloc(p + 1);
    }

    return program_name;
}

const char *archdep_boot_path(void)
{  
    return retro_system_data_directory;
}

const char *archdep_home_path(void)
{
    return retro_system_data_directory;
}

char *archdep_default_autostart_disk_image_file_name(void)
{
    if (archdep_pref_path == NULL) {
        const char *home;

        home = archdep_home_path();
        return util_concat(home, "/.vice/autostart-", machine_name, ".d64", NULL);
    } else {
        return util_concat(archdep_pref_path, "/autostart-", machine_name, ".d64", NULL);
    }
}


char *archdep_default_sysfile_pathlist(const char *emu_id)
{
    static char *default_path;

#if defined(MINIXVMD) || defined(MINIX_SUPPORT)
    static char *default_path_temp;
#endif

    if (default_path == NULL) {
        const char *boot_path;
        const char *home_path;

        boot_path = archdep_boot_path();
        home_path = archdep_home_path();

        /* First search in the `LIBDIR' then the $HOME/.vice/ dir (home_path)
           and then in the `boot_path'.  */

#if defined(MINIXVMD) || defined(MINIX_SUPPORT)
        default_path_temp = util_concat(LIBDIR, "/", emu_id,
                                   ARCHDEP_FINDPATH_SEPARATOR_STRING,
                                   home_path, "/", VICEUSERDIR, "/", emu_id,NULL);

        default_path = util_concat(default_path_temp,
                                   ARCHDEP_FINDPATH_SEPARATOR_STRING,
                                   boot_path, "/", emu_id,
                                   ARCHDEP_FINDPATH_SEPARATOR_STRING,
                                   LIBDIR, "/DRIVES",
                                   ARCHDEP_FINDPATH_SEPARATOR_STRING,
                                   home_path, "/", VICEUSERDIR, "/DRIVES",
                                   ARCHDEP_FINDPATH_SEPARATOR_STRING,
                                   boot_path, "/DRIVES",
                                   ARCHDEP_FINDPATH_SEPARATOR_STRING,
                                   LIBDIR, "/PRINTER",
                                   ARCHDEP_FINDPATH_SEPARATOR_STRING,
                                   home_path, "/", VICEUSERDIR, "/PRINTER",
                                   ARCHDEP_FINDPATH_SEPARATOR_STRING,
                                   boot_path, "/PRINTER",
                                   NULL);
        lib_free(default_path_temp);
#elif defined(__WIN32__) 
  
       default_path = util_concat( home_path, "\\", emu_id, ARCHDEP_FINDPATH_SEPARATOR_STRING,
                                   home_path, "\\DRIVES", ARCHDEP_FINDPATH_SEPARATOR_STRING,
                                   home_path, "\\PRINTER", NULL);

#else 
        default_path = util_concat(LIBDIR, "/", emu_id,
                                   ARCHDEP_FINDPATH_SEPARATOR_STRING,
                                   home_path, "/", VICEUSERDIR, "/", emu_id,
                                   ARCHDEP_FINDPATH_SEPARATOR_STRING,
                                   boot_path, "/", emu_id,
                                   ARCHDEP_FINDPATH_SEPARATOR_STRING,
                                   LIBDIR, "/DRIVES",
                                   ARCHDEP_FINDPATH_SEPARATOR_STRING,
                                   home_path, "/", VICEUSERDIR, "/DRIVES",
                                   ARCHDEP_FINDPATH_SEPARATOR_STRING,
                                   boot_path, "/DRIVES",
                                   ARCHDEP_FINDPATH_SEPARATOR_STRING,
                                   LIBDIR, "/PRINTER",
                                   ARCHDEP_FINDPATH_SEPARATOR_STRING,
                                   home_path, "/", VICEUSERDIR, "/PRINTER",
                                   ARCHDEP_FINDPATH_SEPARATOR_STRING,
                                   boot_path, "/PRINTER",
                                   NULL);
#endif
    }

    return default_path;
}

/* Return a malloc'ed backup file name for file `fname'.  */
char *archdep_make_backup_filename(const char *fname)
{
    return util_concat(fname, "~", NULL);
}

char *archdep_default_resource_file_name(void)
{
    if(archdep_pref_path==NULL) {
      const char *home;
      
      home = archdep_home_path();
      return util_concat(home, "/.vice/vicerc", NULL);
    } else {
      return util_concat(archdep_pref_path, "/vicerc", NULL);
    }
}

char *archdep_default_fliplist_file_name(void)
{
    if(archdep_pref_path==NULL) {
      const char *home;

      home = archdep_home_path();
      return util_concat(home, "/.vice/fliplist-", machine_name, ".vfl", NULL);
    } else {
      return util_concat(archdep_pref_path, "/fliplist-", machine_name, ".vfl", NULL);
    }
}

char *archdep_default_save_resource_file_name(void)
{ 
    char *fname;
    const char *home;
    const char *viceuserdir;

    if(archdep_pref_path==NULL) {
      home = archdep_home_path();
      viceuserdir = util_concat(home, "/.vice", NULL);
    } else {
      viceuserdir = archdep_pref_path;
    }

    if (access(viceuserdir, F_OK)) {
#if defined(__WIN32__)
        mkdir(viceuserdir);
#elif defined(VITA)
        sceIoMkdir(viceuserdir,0777);
#else
        mkdir(viceuserdir, 0700);
#endif
    }

    fname = util_concat(viceuserdir, "/vicerc", NULL);
    
    if(archdep_pref_path==NULL) {
      lib_free(viceuserdir);
    }

    return fname;
}

FILE *archdep_open_default_log_file(void)
{
    return NULL;
}

int archdep_num_text_lines(void)
{
    char *s;

    s = getenv("LINES");
    if (s == NULL) {
        printf("No LINES!\n");
        return -1;
    }
    return atoi(s);
}

int archdep_num_text_columns(void)
{
    char *s;

    s = getenv("COLUMNS");
    if (s == NULL)
        return -1;
    return atoi(s);
}

int archdep_default_logger(const char *level_string, const char *txt) {
    return 0;
}

int archdep_path_is_relative(const char *path)
{
#ifdef __WIN32__
  return !((isalpha(path[0]) && path[1] == ':') || path[0] == '/' || path[0] == '\\');
#elif defined(VITA)
    if (path == NULL)
        return 0;
    if (*path == '/')
        return 0;
    // Vita might also use "ux0:" or "uma0:" for absolute paths
    for (int i = 0; i <= 4; i++)
    {
        if (path[i] == '\0')
          return 1;
        if (path[i] == ':')
          return 0;
    }
    return 1;
#elif defined(__SWITCH__)
    if (path == NULL)
        return 0;
    if (*path == '/')
        return 0;
    // Switch might also use "sdmc:" for absolute paths
    for (int i = 0; i <= 4; i++)
    {
        if (path[i] == '\0')
          return 1;
        if (path[i] == ':')
          return 0;
    }
    return 1;
#else
    if (path == NULL)
        return 0;
    return *path != '/';
#endif
}

int archdep_spawn(const char *name, char **argv,
                  char **pstdout_redir, const char *stderr_redir)
{
#ifndef HAVE_VFORK
    return -1;
#else
#ifndef __WIN32__
#include <sys/wait.h>
#endif
    pid_t child_pid;
    int child_status;
    char *stdout_redir = NULL;

    if (pstdout_redir != NULL) {
        if (*pstdout_redir == NULL)
            *pstdout_redir = archdep_tmpnam();
        stdout_redir = *pstdout_redir;
    }

    child_pid = vfork();
    if (child_pid < 0) {
        log_error(LOG_DEFAULT, "vfork() failed: %s.", strerror(errno));
        return -1;
    } else {
        if (child_pid == 0) {
            if (stdout_redir && freopen(stdout_redir, "w", stdout) == NULL) {
                log_error(LOG_DEFAULT, "freopen(\"%s\") failed: %s.",
                          stdout_redir, strerror(errno));
                _exit(-1);
            }
            if (stderr_redir && freopen(stderr_redir, "w", stderr) == NULL) {
                log_error(LOG_DEFAULT, "freopen(\"%s\") failed: %s.",
                          stderr_redir, strerror(errno));
                _exit(-1);
            }
            execvp(name, argv);
            _exit(-1);
        }
    }

    if (waitpid(child_pid, &child_status, 0) != child_pid) {
        log_error(LOG_DEFAULT, "waitpid() failed: %s", strerror(errno));
        return -1;
    }

    if (WIFEXITED(child_status))
        return WEXITSTATUS(child_status);
    else
        return -1;
#endif
}

/* return malloc'd version of full pathname of orig_name */
int archdep_expand_path(char **return_path, const char *orig_name)
{
    /* Unix version.  */
    if (*orig_name == '/') {
        *return_path = lib_stralloc(orig_name);
    } else {
        static char *cwd;

        cwd = ioutil_current_dir();
        *return_path = util_concat(cwd, "/", orig_name, NULL);
        lib_free(cwd);
    }
    return 0;
}

void archdep_startup_log_error(const char *format, ...)
{
    va_list ap;

    va_start(ap, format);
    vfprintf(stderr, format, ap);
}

char *archdep_filename_parameter(const char *name)
{
    /* nothing special(?) */
    return lib_stralloc(name);
}

char *archdep_quote_parameter(const char *name)
{
    /*not needed(?) */
    return lib_stralloc(name);
}

char *archdep_tmpnam(void)
{

#ifdef HAVE_MKSTEMP
    char *tmpName;
    const char mkstempTemplate[] = "/vice.XXXXXX";
    int fd;
    char* tmp;

    tmpName = (char *)lib_malloc(ioutil_maxpathlen());
    if ((tmp = getenv("TMPDIR")) != NULL ) {
        strncpy(tmpName, tmp, ioutil_maxpathlen());
        tmpName[ioutil_maxpathlen() - sizeof(mkstempTemplate)] = '\0';
    }
    else
        strcpy(tmpName, "/tmp" );
    strcat(tmpName, mkstempTemplate );
    if ((fd = mkstemp(tmpName)) < 0 )
        tmpName[0] = '\0';
    else
        close(fd);

    lib_free(tmpName);
    return lib_stralloc(tmpName);
#else
    return lib_stralloc(tmpnam(NULL));
#endif

}

FILE *archdep_mkstemp_fd(char **filename, const char *mode)
 {

#if defined HAVE_MKSTEMP
    char *tmp;
    const char template[] = "/vice.XXXXXX";
    int fildes;
    FILE *fd;
    char *tmpdir;

    tmpdir = getenv("TMPDIR");

    if (tmpdir != NULL ) 
        tmp = util_concat(tmpdir, template, NULL);
    else
        tmp = util_concat("/tmp", template, NULL);

    fildes = mkstemp(tmp);

    if (fildes < 0 ) {
        lib_free(tmp);
        return NULL;
    }

    fd = fdopen(fildes, mode);

    if (fd == NULL) {
        lib_free(tmp);
        return NULL;
    }

    *filename = tmp;

    return fd;
#else
    char *tmp;

    tmp = tmpnam(NULL);

    if (tmp == NULL)
        return NULL;

    fd = fopen(tmp, mode);

    if (fd == NULL)
        return NULL;

    *filename = lib_stralloc(tmp);

    return fd;
#endif
}

int archdep_file_is_gzip(const char *name)
{
    size_t l = strlen(name);

    if ((l < 4 || strcasecmp(name + l - 3, ".gz"))
        && (l < 3 || strcasecmp(name + l - 2, ".z"))
        && (l < 4 || toupper(name[l - 1]) != 'Z' || name[l - 4] != '.'))
        return 0;
    return 1;
}

int archdep_file_set_gzip(const char *name)
{
    return 0;
}

int archdep_mkdir(const char *pathname, int mode)
{
#if defined(__WIN32__)
       return mkdir(pathname);
#elif defined(VITA)
       return sceIoMkdir(pathname,0777);
#else
     return mkdir(pathname, (mode_t)mode);
#endif
}

int archdep_stat(const char *file_name, unsigned int *len, unsigned int *isdir)
{
    struct stat statbuf;

    if (stat(file_name, &statbuf) < 0)
        return -1;

    *len = statbuf.st_size;
    *isdir = S_ISDIR(statbuf.st_mode);

    return 0;
}

int archdep_file_is_blockdev(const char *name)
{
    struct stat buf;

    if (stat(name, &buf) != 0)
        return 0;

    if (S_ISBLK(buf.st_mode))
        return 1;

    return 0;
}

int archdep_file_is_chardev(const char *name)
{
    struct stat buf;

    if (stat(name, &buf) != 0)
        return 0;

    if (S_ISCHR(buf.st_mode))
        return 1;

    return 0;
}

void archdep_shutdown(void)
{
    log_message(LOG_DEFAULT, "\nExiting...");

    lib_free(argv0);
    lib_free(boot_path);

}

signed long kbd_arch_keyname_to_keynum(char *keyname) {
	return (signed long)atoi(keyname);
}

const char *kbd_arch_keynum_to_keyname(signed long keynum) {
	static char keyname[20];

	memset(keyname, 0, 20);
	sprintf(keyname, "%li", keynum);
	return keyname;
}

void kbd_arch_init()
{
  keyboard_clear_keymatrix();
}

/*
int archdep_network_init(void)
{
    return 0;
}

void archdep_network_shutdown(void)
{
}
*/

char *archdep_get_runtime_os(void)
{
#ifndef __WIN32__
     return "*nix";
#else
    return "win*";
#endif
}

char *archdep_get_runtime_cpu(void)
{
    return "Unknown CPU";
}


int archdep_rmdir(const char *pathname)
{
}




/*
void archdep_vice_exit(int excode)
{
    exit(excode);
}
*/