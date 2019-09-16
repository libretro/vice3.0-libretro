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

#include "retro_disk_control.h"
#include "retro_strings.h"
#include "retro_files.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/*#include <sys/types.h> 
#include <sys/stat.h> 
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>*/

#define COMMENT "#"
#define M3U_SPECIAL_COMMAND "#COMMAND:"
#define VFL_UNIT_ENTRY "UNIT "

// Return the directory name of filename 'filename'.
char* dirname_int(const char* filename)
{
	if (filename == NULL)
		return NULL;
	
	char* right;
	int len = strlen(filename);
	
	if ((right = strrchr(filename, RETRO_PATH_SEPARATOR[0])) != NULL)
		return strleft(filename, len - strlen(right));
	
#ifdef _WIN32
	// Alternative search for windows beceause it also support the UNIX seperator
	if ((right = strrchr(filename, RETRO_PATH_SEPARATOR_ALT[0])) != NULL)
		return strleft(filename, len - strlen(right));
#endif
	
	// Not found
	return NULL;
}

char* m3u_search_file(const char* basedir, const char* dskName)
{
	// Verify if this item is an absolute pathname (or the file is in working dir)
	if (file_exists(dskName))
	{
		// Copy and return
		char* result = calloc(strlen(dskName) + 1, sizeof(char));
		strcpy(result, dskName);
		return result;
	}
	
	// If basedir was provided
	if(basedir != NULL)
	{
		// Join basedir and dskName
		char* dskPath = calloc(RETRO_PATH_MAX, sizeof(char));
		path_join(dskPath, basedir, dskName);

		// Verify if this item is a relative filename (append it to the m3u path)
		if (file_exists(dskPath))
		{
			// Return
			return dskPath;
		}
		free(dskPath);
	}
	
	// File not found
	return NULL;
}

void dc_reset(dc_storage* dc)
{
	// Verify
	if(dc == NULL)
		return;
	
	// Clean the command
	if(dc->command)
	{
		free(dc->command);
		dc->command = NULL;
	}

	// Clean the struct
	for(unsigned i=0; i < dc->count; i++)
	{
		free(dc->files[i]);
		dc->files[i] = NULL;
	}
	dc->unit = 0;
	dc->count = 0;
	dc->index = -1;
	dc->eject_state = true;
}

dc_storage* dc_create(void)
{
	// Initialize the struct
	dc_storage* dc = NULL;
	
	if((dc = malloc(sizeof(dc_storage))) != NULL)
	{
		dc->unit = 0;
		dc->count = 0;
		dc->index = -1;
		dc->eject_state = true;
		dc->command = NULL;
		for(int i = 0; i < DC_MAX_SIZE; i++)
		{
			dc->files[i] = NULL;
		}
	}
	
	return dc;
}

bool dc_add_file_int(dc_storage* dc, char* filename)
{
	// Verify
	if(filename == NULL)
		return false;

	if(dc == NULL)
	{
		free(filename);
		return false;
	}

	// If max size is not
	if(dc->count < DC_MAX_SIZE)
	{
		// Add the file
		dc->count++;
		dc->files[dc->count-1] = filename;
		return true;
	}
	
	free(filename);

	return false;
}

bool dc_add_file(dc_storage* dc, const char* filename)
{
	// Verify
	if(dc == NULL)
		return false;

	if(filename == NULL)
		return false;

	// Copy and return
	char* filename_int = calloc(strlen(filename) + 1, sizeof(char));
	strcpy(filename_int, filename);
	return dc_add_file_int(dc, filename_int);
}

bool dc_remove_file(dc_storage* dc, int index)
{
    if (dc == NULL)
        return false;

    if (index >= dc->count)
        return false;

    // Shift entries after index up
    if (index != dc->count - 1)
        memmove(dc->files + index, dc->files + index + 1, (dc->count - 1 - index) * sizeof(dc->files[0]));

    dc->count--;

    return true;
}

void dc_parse_list(dc_storage* dc, const char* list_file, bool is_vfl)
{
	// Verify
	if(dc == NULL)
		return;
	
	if(list_file == NULL)
		return;

	FILE* fp = NULL;

	// Try to open the file
	if ((fp = fopen(list_file, "r")) == NULL)
		return;

	// Reset
	dc_reset(dc);
	
	// Get the list base dir for resolving relative path
	char* basedir = dirname_int(list_file);
	
	// Read the lines while there is line to read and we have enough space
	char buffer[2048];
	while ((dc->count <= DC_MAX_SIZE) && (fgets(buffer, sizeof(buffer), fp) != NULL))
	{
		char* string = trimwhitespace(buffer);

		if (string[0] == '\0')
			continue;

		// If it's a m3u special key or a file
		if (!is_vfl && strstartswith(string, M3U_SPECIAL_COMMAND))
		{
			dc->command = strright(string, strlen(string) - strlen(M3U_SPECIAL_COMMAND));
		}
		else if (is_vfl && strstartswith(string, VFL_UNIT_ENTRY))
		{
			int unit = strtol(string + strlen(VFL_UNIT_ENTRY), NULL, 10);
			// VICE doesn't allow flip list for tape (unit 1),
			// but let's not split hairs
			if (unit != 1 && !(unit >= 8 && unit <= 11))
			{
				// Invalid unit number - just ignore the list
				break;
			}
			if (dc->unit != 0 && dc->count != 0)
			{
				// VFL file could teoretically contain flip lists
				// for multliple drives. Read only the first one.
				break;
			}
			dc->unit = unit;
		}
		else if (!strstartswith(string, COMMENT))
		{
			// Search the file (absolute, relative to m3u)
			char* filename;
			if ((filename = m3u_search_file(basedir, string)) != NULL)
			{
				// Add the file to the struct
				dc_add_file_int(dc, filename);
			}

		}
	}

	// If it's vfl, we have to reverse it
	if (is_vfl)
	{
		int idx = 0;
		int ridx = dc->count - 1;
		while (idx < ridx)
		{
			char* tmp = dc->files[idx];
			dc->files[idx] = dc->files[ridx];
			dc->files[ridx] = tmp;
			++idx; --ridx;
		}
	}

	// If basedir was provided
	if(basedir != NULL)
		free(basedir);

	// Close the file 
	fclose(fp);
}

void dc_parse_m3u(dc_storage* dc, const char* m3u_file)
{
	dc_parse_list(dc, m3u_file, false);
}

void dc_parse_vfl(dc_storage* dc, const char* vfl_file)
{
	dc_parse_list(dc, vfl_file, true);
}

void dc_free(dc_storage* dc)
{
	// Clean the struct
	dc_reset(dc);
	free(dc);
	dc = NULL;
	return;
}
