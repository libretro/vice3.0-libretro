/* Copyright (C) 2018 
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "libretro-dc.h"
#include "libretro-core.h"

#include "archdep.h"
#include "attach.h"
#include "drive.h"
#include "tape.h"
#include "resources.h"
#include "charset.h"
#include "diskimage.h"
#include "vdrive.h"
#include "vdrive-internal.h"

#ifdef __XSCPU64__
int tape_deinstall(void)
{
   return 0;
}
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#undef  DISK_LABEL_RELAXED              /* Use label even if it doesn't look sane (mostly for testing) */
#undef  DISK_LABEL_FORBID_SHIFTED       /* Reject label if has shifted chars */

static const char flip_file_header[] = "# Vice fliplist file";

extern char retro_save_directory[RETRO_PATH_MAX];
extern char retro_temp_directory[RETRO_PATH_MAX];
extern bool retro_disk_set_image_index(unsigned index);
extern bool retro_disk_set_eject_state(bool ejected);
extern char full_path[RETRO_PATH_MAX];
extern int runstate;

extern retro_log_printf_t log_cb;

/* Return the directory name of 'filename' without trailing separator.
 * Allocates returned string. */
static char* dirname_int(const char* filename)
{
   if (filename == NULL)
      return NULL;

   /* Find last separator */
   char* right = find_last_slash(filename);
   if (right)
      return strleft(filename, right - filename);

   /* Not found */
   return NULL;
}

/* Add known disk labels from conversion tools, famous collectors etc. */
static const char* rude_words[] =
{
   "semprini",
   "ass presents",
};

/* Filter out this annoying "ASS PRESENTS" */
static bool is_ugly(const char* label)
{
   size_t i;
   for (i = 0; i < sizeof(rude_words) / sizeof(rude_words[0]); ++i)
   {
      if (strcasestr(label, rude_words[i]))
         return true;
   }
   return false;
}

#define PETSCII_SHIFTED_BIT 0x20

#define PETSCII_SPACE       0x20
#define PETSCII_NBSP        0xA0
#define PETSCII_UNSHIFTED_A 0x40
#define PETSCII_UNSHIFTED_Z 0x5A
#define PETSCII_SHIFTED_A   0x60
#define PETSCII_SHIFTED_Z   0x7A

/* Try to read disk or tape name from image
 * Allocates returned string */
static char* get_label(const char* filename)
{
   unsigned char label[MAX_LABEL_LEN + 1];
   bool have_disk_label = false;
#if 0
   bool have_tape_label = false;
#endif
   bool have_shifted = false;
   int i;

   label[0] = '\0';
   /* Disk image which we can read name from */
   if (strendswith(filename, "d64") || strendswith(filename, "d71"))
   {
      FILE* fd = fopen(filename, "rb");

      if (fd != NULL)
      {
         if (fseek(fd, D64_NAME_POS, SEEK_SET) == 0
            && fread(label, D64_FULL_NAME_LEN, 1, fd) == 1)
         {
            label[D64_FULL_NAME_LEN] = '\0';
            have_disk_label = true;
         }
         fclose(fd);
      }
   }

   /* Tape image which we can read name from */
   if (strendswith(filename, "t64"))
   {
      FILE* fd = fopen(filename, "rb");

      if (fd != NULL)
      {
         if (fseek(fd, T64_NAME_POS, SEEK_SET) == 0
            && fread(label, T64_NAME_LEN, 1, fd) == 1)
         {
            label[T64_NAME_LEN] = '\0';
#if 0
            have_tape_label = true;
#endif
         }
         fclose(fd);
      }
   }

   /* Special processing for disk label - sanity check and trimming */
   if (have_disk_label)
   {
#if !defined(DISK_LABEL_RELAXED)
      /* Chack if all characters in disk name and padding areas look valid
       * that may be too picky, but it's better to show nothing than garbage */
      for (i = 0; i < D64_FULL_NAME_LEN; ++i)
      {
         unsigned char c = label[i];
         if (c != PETSCII_NBSP && (c < PETSCII_SPACE || c > PETSCII_SHIFTED_Z))
         {
            return strdup((char*)label);
         }
      }
#endif
      label[D64_NAME_LEN - 1] = '\0';
   }

   /* Remove trailing spaces */
   i = strlen((char*)label) - 1;
   while (i > 0 && (label[i] == PETSCII_NBSP || label[i] == PETSCII_SPACE))
      label[i--] = '\0';

   /* Replace other nbsp with spaces */
   while (i > 0)
   {
      if (label[i] == PETSCII_NBSP)
         label[i] = ' ';
      --i;
   }

   /* Check for shifted chars */
   for (i=0; label[i] != '\0'; ++i)
   {
      unsigned char c = label[i];
      if (c >= PETSCII_SHIFTED_A)
      {
#if defined(DISK_LABEL_FORBID_SHIFTED)
         return strdup((char*)label);
#endif
         /* Have shifted chars */
         have_shifted = true;
         break;
      }
   }

   int mode = disk_label_mode;
   if (have_shifted 
      && (mode == DISK_LABEL_MODE_ASCII_OR_UPPERCASE || mode == DISK_LABEL_MODE_ASCII_OR_CAMELCASE))
   {
      mode = DISK_LABEL_MODE_ASCII;
   }

   bool was_space = true;
   /* Convert petscii to ascii */
   for (i=0; label[i] != '\0'; ++i)
   {
      unsigned char c = label[i];
      if (c == PETSCII_SPACE)
      {
         was_space = true;
      }
      else
      {
         if (c >= PETSCII_UNSHIFTED_A && c <= PETSCII_UNSHIFTED_Z)
         {
            /* Unshifted - starts as uppercase, toggles to lowercase */
            if (mode == DISK_LABEL_MODE_ASCII || mode == DISK_LABEL_MODE_LOWERCASE
               || (mode == DISK_LABEL_MODE_ASCII_OR_CAMELCASE && !was_space))
            {
               label[i] = c ^ PETSCII_SHIFTED_BIT;
            }
         }
         else if (c >= PETSCII_SHIFTED_A && c <= PETSCII_SHIFTED_Z)
         {
            /* Shifted - starts as lowercase, toggles to uppercase */
            if (mode == DISK_LABEL_MODE_ASCII || mode == DISK_LABEL_MODE_UPPERCASE)
            {
               label[i] = c ^ PETSCII_SHIFTED_BIT;
            }
         }
         was_space = false;
      }
   }

   if (is_ugly((char*)label))
   {
      return strdup((char*)"");
   }

   return strdup((char*)label);
}

/* Search for image file relative to M3U
 * Allocates returned string */
static char* m3u_search_file(const char* basedir, const char* filename)
{
   /* Verify if this item is an absolute pathname (or the file is in working dir) */
   if (path_is_valid(filename))
      return strdup(filename);

   /* If basedir was provided and path is relative, search relative to M3U file location */
   else if (basedir != NULL && !path_is_absolute(filename))
   {
      char filepath[RETRO_PATH_MAX] = {0};
      path_join(filepath, basedir, filename);

      /* Verify if this item is a relative filename */
      if (path_is_valid(filepath))
         return strdup(filepath);
   }

   /* File not found */
   return NULL;
}

void dc_reset(dc_storage* dc)
{
   /* Verify */
   if (dc == NULL)
      return;

   /* Clean the command */
   free(dc->command);
   dc->command = NULL;

   /* Clean the struct */
   for (unsigned i=0; i < dc->count; i++)
   {
      free(dc->files[i]);
      dc->files[i] = NULL;
      free(dc->labels[i]);
      dc->labels[i] = NULL;
      free(dc->disk_labels[i]);
      dc->disk_labels[i] = NULL;
      free(dc->load[i]);
      dc->load[i] = NULL;
      dc->types[i] = DC_IMAGE_TYPE_NONE;
   }

   dc->unit        = 0;
   dc->count       = 0;
   dc->index       = 0;
   dc->eject_state = true;
   dc->replace     = false;
}

dc_storage* dc_create(void)
{
   /* Initialize the struct */
   dc_storage* dc = NULL;

   if ((dc = malloc(sizeof(dc_storage))) != NULL)
   {
      dc->unit        = 0;
      dc->count       = 0;
      dc->index       = 0;
      dc->eject_state = true;
      dc->replace     = false;
      dc->command     = NULL;
      for (int i = 0; i < DC_MAX_SIZE; i++)
      {
         dc->files[i]       = NULL;
         dc->labels[i]      = NULL;
         dc->disk_labels[i] = NULL;
         dc->load[i]        = NULL;
         dc->types[i]       = DC_IMAGE_TYPE_NONE;
      }
   }

   return dc;
}

bool dc_add_file_int(dc_storage* dc, char* filename, char* label, char* disk_label, char* program)
{
   /* Verify */
   if (dc == NULL)
      return false;

   if (!filename || (*filename == '\0'))
      return false;

   /* If max size is not exceeded */
   if (dc->count < DC_MAX_SIZE)
   {
      /* Add the file */
      dc->count++;
      dc->files[dc->count-1]       = filename;
      dc->labels[dc->count-1]      = label;
      dc->disk_labels[dc->count-1] = disk_label;
      dc->load[dc->count-1]        = (!string_is_empty(program)) ? program : NULL;
      dc->types[dc->count-1]       = dc_get_image_type(filename);
      return true;
   }

   /* Determine if tape or disk fliplist from first entry */
   if (dc->unit != -1 && !string_is_empty(dc->files[0]))
   {
      if (dc_get_image_type(dc->files[0]) == DC_IMAGE_TYPE_TAPE)
         dc->unit = 1;
      else if (dc_get_image_type(dc->files[0]) == DC_IMAGE_TYPE_FLOPPY)
         dc->unit = 8;
      else if (dc_get_image_type(dc->files[0]) == DC_IMAGE_TYPE_MEM)
         dc->unit = 0;
      else
         dc->unit = 8;
   }

   return false;
}

bool dc_add_file(dc_storage* dc, const char* filename, const char* label, const char* disk_label, const char* program)
{
   unsigned index = 0;

   /* Verify */
   if (dc == NULL)
      return false;

   if (!filename || (*filename == '\0'))
      return false;

   /* Dupecheck */
   for (index = 0; index < dc->count; index++)
      if (!strcmp(dc->files[index], filename))
         return true;

   /* Get 'name' - just the filename without extension
    * > It would be nice to make use of the image label
    *   here, but this often bears no relationship to
    *   the actual file content... */
   char file_label[512];
   file_label[0] = '\0';
   if (!string_is_empty(label))
      snprintf(file_label, sizeof(file_label), "%s", label);
   else
      fill_short_pathname_representation(file_label, filename, sizeof(file_label));

   /* Copy and return */
   return dc_add_file_int(dc, strdup(filename),
      string_is_empty(file_label) ? NULL : strdup(file_label),
      string_is_empty(disk_label) ? NULL : strdup(disk_label),
      string_is_empty(program)    ? NULL : strdup(program));
}

bool dc_remove_file(dc_storage* dc, int index)
{
   if (dc == NULL)
      return false;

   if (index < 0 || index >= dc->count)
      return false;

   /* "If ptr is a null pointer, no action occurs" */
   free(dc->files[index]);
   dc->files[index] = NULL;
   free(dc->labels[index]);
   dc->labels[index] = NULL;
   free(dc->disk_labels[index]);
   dc->disk_labels[index] = NULL;
   free(dc->load[index]);
   dc->load[index] = NULL;
   dc->types[index] = DC_IMAGE_TYPE_NONE;

   /* Shift all entries after index one slot up */
   if (index != dc->count - 1)
   {
      memmove(dc->files + index, dc->files + index + 1, (dc->count - 1 - index) * sizeof(dc->files[0]));
      memmove(dc->labels + index, dc->labels + index + 1, (dc->count - 1 - index) * sizeof(dc->labels[0]));
      memmove(dc->disk_labels + index, dc->disk_labels + index + 1, (dc->count - 1 - index) * sizeof(dc->disk_labels[0]));
      memmove(dc->load + index, dc->load + index + 1, (dc->count - 1 - index) * sizeof(dc->load[0]));
   }

   dc->count--;

   /* Reset fliplist unit after removing last entry */
   if (dc->count == 0)
      dc->unit = 0;

   return true;
}

bool dc_replace_file(dc_storage* dc, int index, const char* filename)
{
   if (dc == NULL)
      return false;

   if (index < 0 || index >= dc->count)
      return false;

   /* "If ptr is a null pointer, no action occurs" */
   free(dc->files[index]);
   dc->files[index] = NULL;
   free(dc->labels[index]);
   dc->labels[index] = NULL;
   free(dc->disk_labels[index]);
   dc->disk_labels[index] = NULL;
   free(dc->load[index]);
   dc->load[index] = NULL;
   dc->types[index] = DC_IMAGE_TYPE_NONE;

   if (filename == NULL)
   {
      dc_remove_file(dc, index);
   }
   else
   {
      dc->replace = false;

      char full_path_replace[RETRO_PATH_MAX] = {0};
      strcpy(full_path_replace, (char*)filename);

      /* ZIP + NIB vars, use the same temp directory for single NIBs */
      char zip_basename[RETRO_PATH_MAX] = {0};
      snprintf(zip_basename, sizeof(zip_basename), "%s", path_basename(full_path_replace));
      path_remove_extension(zip_basename);

      char nib_input[RETRO_PATH_MAX] = {0};
      char nib_output[RETRO_PATH_MAX] = {0};

      /* NIB convert to G64 */
      if (dc_get_image_type(full_path_replace) == DC_IMAGE_TYPE_NIBBLER)
      {
         snprintf(nib_input, sizeof(nib_input), "%s", full_path_replace);
         snprintf(nib_output, sizeof(nib_output), "%s%s%s.g64", retro_temp_directory, FSDEV_DIR_SEP_STR, zip_basename);
         path_mkdir(retro_temp_directory);
         nib_convert(nib_input, nib_output);
         snprintf(full_path_replace, sizeof(full_path_replace), "%s", nib_output);
      }

      /* ZIP */
      if (strendswith(full_path_replace, "zip") || strendswith(full_path_replace, "7z"))
      {
         path_mkdir(retro_temp_directory);
         if (strendswith(full_path_replace, "zip"))
            zip_uncompress(full_path_replace, retro_temp_directory, NULL);
         else if (strendswith(full_path_replace, "7z"))
            sevenzip_uncompress(full_path_replace, retro_temp_directory, NULL);

         /* Default to directory mode */
         int zip_mode = 0;
         snprintf(full_path_replace, sizeof(full_path_replace), "%s", retro_temp_directory);

         FILE *zip_m3u;
         char zip_m3u_list[DC_MAX_SIZE][RETRO_PATH_MAX] = {0};
         char zip_m3u_path[RETRO_PATH_MAX] = {0};
         snprintf(zip_m3u_path, sizeof(zip_m3u_path), "%s%s%s.m3u", retro_temp_directory, FSDEV_DIR_SEP_STR, zip_basename);
         int zip_m3u_num = 0;

         DIR *zip_dir = NULL;
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
            if (zip_dirp->d_name[0] == '.' || strendswith(zip_dirp->d_name, "m3u") || zip_mode > 1)
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
               dc_reset(dc);
               return true;
               break;
            case 1: /* Generated playlist */
               if (zip_m3u_num == 1)
               {
                  snprintf(full_path_replace, sizeof(full_path_replace), "%s%s%s", retro_temp_directory, FSDEV_DIR_SEP_STR, zip_m3u_list[0]);
               }
               else
               {
                  zip_m3u = fopen(zip_m3u_path, "w");
                  qsort(zip_m3u_list, zip_m3u_num, RETRO_PATH_MAX, qstrcmp);
                  for (int l = 0; l < zip_m3u_num; l++)
                     fprintf(zip_m3u, "%s\n", zip_m3u_list[l]);
                  fclose(zip_m3u);
                  snprintf(full_path_replace, sizeof(full_path_replace), "%s", zip_m3u_path);
               }
               break;
         }
      }

      /* M3U replace */
      if (strendswith(full_path_replace, ".m3u"))
      {
         /* Parse the M3U file */
         dc_parse_m3u(dc, full_path_replace);

         /* Some debugging */
         log_cb(RETRO_LOG_INFO, "M3U/VFL parsed, %d file(s) found\n", dc->count);

         /* Insert first disk */
         retro_disk_set_image_index(0);

         /* Trick frontend to return to index 0 after successful "append" does +1 */
         dc->replace = true;
      }
      /* Single append */
      else
      {
         char image_label[RETRO_PATH_MAX];
         image_label[0] = '\0';
         fill_short_pathname_representation(image_label, full_path_replace, sizeof(image_label));

         dc->files[index]       = strdup(full_path_replace);
         dc->labels[index]      = strdup(image_label);
         dc->disk_labels[index] = get_label(full_path_replace);
         dc->load[index]        = NULL;
         dc->types[index]       = dc_get_image_type(full_path_replace);
      }
   }

   return true;
}

static bool dc_add_m3u_save_disk(
      dc_storage* dc,
      const char* m3u_file, const char* save_dir,
      const char* disk_name, unsigned int index,
      bool file_check)
{
   bool save_disk_exists                     = false;
   const char *m3u_file_name                 = NULL;
   char m3u_file_name_no_ext[RETRO_PATH_MAX] = {0};
   char save_disk_file_name[RETRO_PATH_MAX]  = {0};
   char save_disk_path[RETRO_PATH_MAX]       = {0};
   char volume_name[MAX_LABEL_LEN]           = {0};
   char format_name[MAX_LABEL_LEN]           = {0};

   /* Verify */
   if (dc == NULL)
      return false;

   if (m3u_file == NULL)
      return false;

   if (save_dir == NULL)
      return false;

   /* Get m3u file name */
   m3u_file_name = path_basename(m3u_file);
   if (!m3u_file_name || (*m3u_file_name == '\0'))
      return false;

   /* Get m3u file name without extension */
   snprintf(m3u_file_name_no_ext, sizeof(m3u_file_name_no_ext),
         "%s", m3u_file_name);
   path_remove_extension(m3u_file_name_no_ext);

   if (*m3u_file_name_no_ext == '\0')
      return false;

   /* Construct save disk file name */
   snprintf(save_disk_file_name, RETRO_PATH_MAX, "%s.save%u.d64",
         m3u_file_name_no_ext, index);

   /* Construct save disk path */
   path_join(save_disk_path, save_dir, save_disk_file_name);

   /* Check whether save disk already exists
    * Note: If a disk already exists, we should be
    * able to support changing the volume label if
    * it differs from 'disk_name'. This is quite
    * fiddly, however - perhaps it can be added later... */
   save_disk_exists = path_is_valid(save_disk_path);

   if (file_check)
      return save_disk_exists;

   /* ...if not, create a new one */
   if (!save_disk_exists)
   {
      /* Get volume name
       * > If disk_name is NULL or empty/EMPTY,
       *   no volume name is set */
      if (disk_name && (*disk_name != '\0'))
      {
         if (strncasecmp(disk_name, "empty", strlen("empty")))
         {
            char *scrub_pointer = NULL;

            /* Ensure volume name is valid
             * > Must be <= 30 characters
             * > Cannot contain '/' or ':' */
            strncpy(volume_name, disk_name, sizeof(volume_name) - 1);

            while ((scrub_pointer = strpbrk(volume_name, "/:")))
               *scrub_pointer = ' ';
         }
      }

      /* Label format */
      if (string_is_empty(volume_name))
         snprintf(volume_name, sizeof(volume_name), "%s %u",
               M3U_SAVEDISK_LABEL, index);

      snprintf(format_name, sizeof(format_name), "%s",
            string_to_lower(volume_name));
      charset_petconvstring((uint8_t*)format_name, 0);

      /* Create save disk */
      save_disk_exists = !vdrive_internal_create_format_disk_image(
            save_disk_path, format_name, DISK_IMAGE_TYPE_D64);
   }

   /* If save disk exists/was created, add it to the list */
   if (save_disk_exists)
   {
      char save_disk_label[64] = {0};

      snprintf(save_disk_label, 64, "%s %u",
            M3U_SAVEDISK_LABEL, index);

      dc_add_file(dc, save_disk_path, save_disk_label, format_name, NULL);
      return true;
   }

   return false;
}

bool dc_save_disk_toggle(dc_storage* dc, bool file_check, bool select)
{
   if (!dc)
      return false;

   if (dc->unit != 8)
      return false;

   if (file_check)
      return dc_add_m3u_save_disk(dc, full_path, retro_save_directory, NULL, 0, true);

   dc_add_m3u_save_disk(dc, full_path, retro_save_directory, NULL, 0, false);
   if (select)
   {
      unsigned save_disk_index = 0;
      unsigned index = 0;
      char save_disk_label[64] = {0};

      snprintf(save_disk_label, 64, "%s %u",
            M3U_SAVEDISK_LABEL, 0);

      /* Scan for save disk index */
      for (index = 0; index < dc->count; index++)
         if (!strcmp(dc->labels[index], save_disk_label))
            save_disk_index = index;

      /* Remember previous index */
      if (dc->index != save_disk_index)
         dc->index_prev = dc->index;

      /* Switch index */
      if (save_disk_index == dc->index)
         dc->index = dc->index_prev;
      else
         dc->index = save_disk_index;

      /* Cycle disk tray */
      retro_disk_set_eject_state(true);
      retro_disk_set_eject_state(false);

      /* Widget notification */
      snprintf(retro_message_msg, sizeof(retro_message_msg),
               "%d/%d - %s",
               dc->index+1, dc->count, path_basename(dc->labels[dc->index]));
      retro_message = true;
   }
   else
      log_cb(RETRO_LOG_INFO, "Save Disk 0 appended\n");
   return true;
}

void dc_parse_list(dc_storage* dc, const char* list_file, bool is_vfl, const char* save_dir)
{
   /* Verify */
   if (dc == NULL)
      return;

   /* Reset */
   dc_reset(dc);

   if (list_file == NULL)
      return;

   FILE* fp = NULL;

   /* Try to open the file */
   if ((fp = fopen(list_file, "r")) == NULL)
   {
      log_cb(RETRO_LOG_ERROR, "Failed to open list file %s\n", list_file);
      return;
   }

   /* Read the lines while there is line to read and we have enough space */
   char buffer[1024];

   /* Enforce standard compatibility to avoid invalid vfl files on the loose */
   if (is_vfl)
   {
      /* Read and verify header */
      if (fgets(buffer, sizeof(buffer), fp) == NULL
         || strncmp(buffer, flip_file_header, strlen(flip_file_header)) != 0)
      {
         log_cb(RETRO_LOG_ERROR, "File %s is not a fliplist file\n", list_file);
         fclose(fp);
         return;
      }
   }

   /* Get the list base dir for resolving relative path */
   char* basedir = dirname_int(list_file);
   unsigned int save_disk_index = 0;

   while ((dc->count <= DC_MAX_SIZE) && (fgets(buffer, sizeof(buffer), fp) != NULL))
   {
      /* Label for the following file */
      char* label = NULL;

      /* Disk control interface 'name' for the following file */
      char* image_name = NULL;

      /* M3U path for the following file */
      char* filename = NULL;

      char* string = trimwhitespace(buffer);
      if (string[0] == '\0')
         continue;

      /* Is it a m3u special key? (#COMMAND:) */
      if (!is_vfl && strstartswith(string, M3U_SPECIAL_COMMAND))
      {
         dc->command = strright(string, strlen(string) - strlen(M3U_SPECIAL_COMMAND));
      }
      /* Disk name / label (#LABEL:) */
      else if (!is_vfl && strstartswith(string, M3U_NONSTD_LABEL))
      {
         char* newlabel = trimwhitespace(buffer + strlen(M3U_NONSTD_LABEL));
         free(label);
         free(image_name);
         if (newlabel && newlabel[0])
         {
            label = strdup(newlabel);
            image_name = strdup(newlabel);
         }
         else
         {
            label = NULL;
            image_name = NULL;
         }
      }
      /* Disk name / label - EXTM3U standard compiant (#EXTINF) */
      else if (!is_vfl && strstartswith(string, M3U_EXTSTD_LABEL))
      {
         /* "Track" description should be following comma (https://en.wikipedia.org/wiki/M3U#Extended_M3U) */
         char* newlabel = strchr(buffer + strlen(M3U_EXTSTD_LABEL), ',');
         if (newlabel)
            newlabel = trimwhitespace(newlabel);
         free(label);
         free(image_name);
         if (newlabel && newlabel[0])
         {
            label = strdup(newlabel);
            image_name = strdup(newlabel);
         }
         else
         {
            label = NULL;
            image_name = NULL;
         }
      }
      /* VFL UNIT number (UNIT) */
      else if (is_vfl && strstartswith(string, VFL_UNIT_ENTRY))
      {
         int unit = strtol(string + strlen(VFL_UNIT_ENTRY), NULL, 10);
         /* VICE doesn't allow flip list for tape (unit 1),
          * but let's not split hairs */
         if (unit != 1 && !(unit >= 8 && unit <= 11))
         {
            /* Invalid unit number - just ignore the list */
            log_cb(RETRO_LOG_ERROR, "Invalid unit number %d in fliplist %s", unit, list_file);
            break;
         }
         if (dc->unit != 0 && dc->unit != unit && dc->count != 0)
         {
            /* VFL file could theoretically contain flip lists for multliple drives - read only the first one. */
            log_cb(RETRO_LOG_WARN, "Ignored entries for other unit(s) in fliplist %s", list_file);
            break;
         }
         dc->unit = unit;
      }
      /* Save disk (#SAVEDISK:) */
      else if (strstartswith(string, M3U_SAVEDISK))
      {
         /* Get volume name */
         char* disk_name = strright(string, strlen(string) - strlen(M3U_SAVEDISK));

         /* Add save disk, creating it if necessary */
         if (dc_add_m3u_save_disk(
               dc, list_file, save_dir,
               disk_name, save_disk_index,
               false))
               save_disk_index++;

         /* Clean up */
         if (disk_name != NULL)
            free(disk_name);
      }
      /* VICE doesn't allow comments in vfl files - enforce the standard */
      else if (is_vfl || string[0] != COMMENT)
      {
         /* Path format:
          *   FILE_NAME|FILE_LABEL
          * Delimiter + FILE_LABEL is optional */
         char file_name[RETRO_PATH_MAX]  = {0};
         char file_label[RETRO_PATH_MAX] = {0};
         char* delim_ptr                 = strchr(string, M3U_PATH_DELIM);
         bool label_set                  = false;

         if (delim_ptr)
         {
            /* Not going to use strleft()/strright() here,
             * since these functions allocate new strings,
             * which we don't want to do... */

            /* Get FILE_NAME segment */
            size_t len = (size_t)(1 + delim_ptr - string);
            if (len > 0)
               strncpy(
                     file_name, string,
                     ((len < RETRO_PATH_MAX ? len : RETRO_PATH_MAX) * sizeof(char)) - 1);

            /* Get FILE_LABEL segment */
            delim_ptr++;
            if (*delim_ptr != '\0')
               strncpy(
                     file_label, delim_ptr, sizeof(file_label) - 1);

            /* Note: If delimiter is present but FILE_LABEL
             * is omitted, label is intentionally left blank */
            label_set = true;
         }
         else
            strncpy(file_name, string, sizeof(file_name) - 1);

         /* Parse direct PRG load */
         char image_prg[D64_NAME_LEN] = {0};
         if (strstr(file_name, ":"))
         {
            char *token = strtok((char*)file_name, ":");
            while (token != NULL)
            {
               snprintf(image_prg, sizeof(image_prg), "%s", token);
               token = strtok(NULL, ":");
            }
         }

         /* "Browsed" file in ZIP */
         char browsed_file[RETRO_PATH_MAX] = {0};
         if (strstr(file_name, ".zip#") || strstr(file_name, ".7z#"))
         {
            char *token = strtok((char*)file_name, "#");
            while (token != NULL)
            {
               snprintf(browsed_file, sizeof(browsed_file), "%s", token);
               token = strtok(NULL, "#");
            }
         }

         /* Search the file (absolute, relative to m3u) */
         if ((filename = m3u_search_file(basedir, file_name)) != NULL)
         {
            /* If image name is missing, use filename without extension */
            if (!image_name)
            {
               char tmp[512];
               tmp[0] = '\0';

               if (!string_is_empty(browsed_file))
                  fill_short_pathname_representation(tmp, browsed_file, sizeof(tmp));
               else
                  fill_short_pathname_representation(tmp, filename, sizeof(tmp));

               image_name = strdup(tmp);
            }

            if (!label)
               label = get_label(filename);

            if (!file_label[0] && !label_set)
               snprintf(file_label, sizeof(file_label), "%s", image_name);

            char full_path[RETRO_PATH_MAX] = {0};
            snprintf(full_path, sizeof(full_path), "%s", filename);

            /* ZIP + NIB vars, use the same temp directory for single NIBs */
            char zip_basename[RETRO_PATH_MAX] = {0};
            snprintf(zip_basename, sizeof(zip_basename), "%s", path_basename(full_path));
            path_remove_extension(zip_basename);

            char nib_input[RETRO_PATH_MAX] = {0};
            char nib_output[RETRO_PATH_MAX] = {0};

            /* NIB convert to G64 */
            if (dc_get_image_type(filename) == DC_IMAGE_TYPE_NIBBLER)
            {
               snprintf(nib_input, sizeof(nib_input), "%s", filename);
               snprintf(nib_output, sizeof(nib_output), "%s%s%s.g64", retro_temp_directory, FSDEV_DIR_SEP_STR, zip_basename);
               path_mkdir(retro_temp_directory);
               nib_convert(nib_input, nib_output);
               snprintf(full_path, sizeof(full_path), "%s", nib_output);
            }

            /* ZIP */
            if (strendswith(filename, "zip") || strendswith(filename, "7z"))
            {
               char lastfile[RETRO_PATH_MAX] = {0};

               path_mkdir(retro_temp_directory);
               if (strendswith(filename, "zip"))
                  zip_uncompress(full_path, retro_temp_directory, lastfile);
               else if (strendswith(filename, "7z"))
                  sevenzip_uncompress(full_path, retro_temp_directory, lastfile);

               /* Convert all NIBs to G64 */
               if (!string_is_empty(browsed_file))
               {
                  snprintf(nib_input, sizeof(nib_input), "%s%s%s", retro_temp_directory, FSDEV_DIR_SEP_STR, browsed_file);
                  if (dc_get_image_type(nib_input) == DC_IMAGE_TYPE_NIBBLER && path_is_valid(nib_input))
                  {
                     /* Reuse lastfile */
                     snprintf(lastfile, sizeof(lastfile), "%s", browsed_file);
                     snprintf(nib_output, sizeof(nib_output), "%s%s%s.g64", retro_temp_directory, FSDEV_DIR_SEP_STR, path_remove_extension(lastfile));
                     nib_convert(nib_input, nib_output);
                     snprintf(browsed_file, sizeof(browsed_file), "%s", path_basename(nib_output));
                  }
                  snprintf(lastfile, sizeof(lastfile), "%s", browsed_file);
               }
               else
               {
                  DIR *zip_dir = NULL;
                  struct dirent *zip_dirp;

                  zip_dir = opendir(retro_temp_directory);
                  while ((zip_dirp = readdir(zip_dir)) != NULL)
                  {
                     if (dc_get_image_type(zip_dirp->d_name) == DC_IMAGE_TYPE_NIBBLER)
                     {
                        snprintf(nib_input, sizeof(nib_input), "%s%s%s", retro_temp_directory, FSDEV_DIR_SEP_STR, zip_dirp->d_name);
                        snprintf(nib_output, sizeof(nib_output), "%s%s%s.g64", retro_temp_directory, FSDEV_DIR_SEP_STR, path_remove_extension(zip_dirp->d_name));
                        nib_convert(nib_input, nib_output);
                        snprintf(lastfile, sizeof(lastfile), "%s", path_basename(nib_output));
                     }
                  }
                  closedir(zip_dir);
               }

               snprintf(full_path, RETRO_PATH_MAX, "%s%s%s", retro_temp_directory, FSDEV_DIR_SEP_STR, lastfile);
            }

            /* Add the file to the struct */
            if (path_is_valid(full_path))
               dc_add_file(dc, full_path,
                     file_label,
                     label,
                     image_prg);

            free(filename);
            filename = NULL;
         }
         else
            log_cb(RETRO_LOG_WARN, "File '%s' from list '%s' not found in dir '%s'\n", file_name, list_file, basedir);
      }

      /* Throw away the label and image name */
      free(label);
      label = NULL;

      free(image_name);
      image_name = NULL;
   }

   /* If it's vfl, we have to reverse it */
   if (is_vfl)
   {
      int idx = 0;
      int ridx = dc->count - 1;
      while (idx < ridx)
      {
         char* tmp = dc->files[idx];
         dc->files[idx] = dc->files[ridx];
         dc->files[ridx] = tmp;
         tmp = dc->labels[idx];
         dc->labels[idx] = dc->labels[ridx];
         dc->labels[ridx] = tmp;
         tmp = dc->disk_labels[idx];
         dc->disk_labels[idx] = dc->disk_labels[ridx];
         dc->disk_labels[ridx] = tmp;
         ++idx; --ridx;
      }
   }

   free(basedir);
   basedir = NULL;

   /* Close the file */
   fclose(fp);

   /* M3U - Determine if tape or disk fliplist from first entry */
   if (dc->count != 0)
   {
      switch (dc_get_image_type(dc->files[0]))
      {
         case DC_IMAGE_TYPE_TAPE:
            dc->unit = 1;
            break;
         case DC_IMAGE_TYPE_FLOPPY:
         default:
            dc->unit = 8;
            break;
         case DC_IMAGE_TYPE_MEM:
            dc->unit = 0;
            break;
      }

      if (runstate == RUNSTATE_RUNNING)
      {
         switch (dc->unit)
         {
            case 1:
               /* Detach & disable drive 8 */
               file_system_detach_disk(8);
               log_resources_set_int("Drive8Type", DRIVE_TYPE_NONE);
               break;
            
            case 8:
               /* Detach & disable tape, enable drive 8 */
               tape_deinstall();
               log_resources_set_int("Drive8Type", DRIVE_TYPE_1541);
               break;

            case 0:
               /* Detach & disable tape, detach & disable drive 8 */
               tape_deinstall();
               file_system_detach_disk(8);
               log_resources_set_int("Drive8Type", DRIVE_TYPE_NONE);
               break;
         }
      }
   }
}

void dc_parse_m3u(dc_storage* dc, const char* m3u_file)
{
   dc_parse_list(dc, m3u_file, false, retro_save_directory);
}

void dc_parse_vfl(dc_storage* dc, const char* vfl_file)
{
   dc_parse_list(dc, vfl_file, true, retro_save_directory);
}

void dc_free(dc_storage* dc)
{
   /* Clean the struct */
   dc_reset(dc);
   free(dc);
   dc = NULL;
   return;
}

enum dc_image_type dc_get_image_type(const char* filename)
{
   /* Missing file */
   if (!filename || (*filename == '\0'))
      return DC_IMAGE_TYPE_NONE;

   /* Floppy image */
   if (strendswith(filename, "d64") ||
       strendswith(filename, "d71") ||
       strendswith(filename, "d80") ||
       strendswith(filename, "d81") ||
       strendswith(filename, "d82") ||
       strendswith(filename, "g64") ||
       strendswith(filename, "x64") ||
       strendswith(filename, "d6z") ||
       strendswith(filename, "d7z") ||
       strendswith(filename, "d8z") ||
       strendswith(filename, "g6z") ||
       strendswith(filename, "g4z") ||
       strendswith(filename, "x6z"))
      return DC_IMAGE_TYPE_FLOPPY;

   /* Tape image */
   if (strendswith(filename, "tap") ||
       strendswith(filename, "t64"))
      return DC_IMAGE_TYPE_TAPE;

   /* Memory image */
   if (strendswith(filename, "prg") ||
       strendswith(filename, "p00") ||
       strendswith(filename, "20")  ||
       strendswith(filename, "40")  ||
       strendswith(filename, "60")  ||
       strendswith(filename, "70")  ||
       strendswith(filename, "a0")  ||
       strendswith(filename, "b0")  ||
       strendswith(filename, "crt") ||
       strendswith(filename, "rom") ||
       strendswith(filename, "bin"))
      return DC_IMAGE_TYPE_MEM;

   /* Nibbler floppy image, requires conversion */
   if (strendswith(filename, "nib") ||
       strendswith(filename, "nbz"))
      return DC_IMAGE_TYPE_NIBBLER;

   /* Fallback */
   return DC_IMAGE_TYPE_UNKNOWN;
}
