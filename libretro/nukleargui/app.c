/* nuklear - v1.00 - public domain */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <math.h>
#include <limits.h>
#include <time.h>

#include <libretro.h>
#include <libretro-core.h>

extern void filebrowser_init();
extern void filebrowser_free();

extern void Screen_SetFullUpdate(int scr);
extern void vkbd_key(int key,int pressed);
extern void app_vkb_handle();

extern char core_key_state[512];
extern char core_old_key_state[512];
extern char RPATH[512];
extern int SHOWKEY;
extern int want_quit;

char LCONTENT[512];
int LOADCONTENT=-1;
int LDRIVE=8;

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_IMPLEMENTATION

#define NK_RETRO_SOFT_IMPLEMENTATION

#include "nuklear.h"
#include "nuklear_retro_soft.h"

static RSDL_Surface *screen_surface;

extern void restore_bgk();
extern void save_bkg();

/* macros */

#define UNUSED(a) (void)a
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) < (b) ? (b) : (a))
#define LEN(a) (sizeof(a)/sizeof(a)[0])

/* Platform */

float bg[4];
struct nk_color background;
/* GUI */
struct nk_context *ctx;

static nk_retro_Font *RSDL_font;

#include "style.c"
#include "filebrowser.c"
#include "gui.i"

int app_init()
{

#ifdef  FRONTEND_SUPPORTS_RGB565
    screen_surface=Retro_CreateRGBSurface16(retrow,retroh,16,0,0,0,0);
    Retro_Screen=(unsigned short int *)screen_surface->pixels;
#else
    screen_surface=Retro_CreateRGBSurface32(retrow,retroh,32,0,0,0,0);
    Retro_Screen=(unsigned int *)screen_surface->pixels;
#endif

    RSDL_font = (nk_retro_Font*)calloc(1, sizeof(nk_retro_Font));
    RSDL_font->width = 8;
    RSDL_font->height = 8;
    if (!RSDL_font)
        return -1;

    /* GUI */
    ctx = nk_retro_init(RSDL_font,screen_surface,retrow,retroh);

    /* style.c */
    /* THEME_BLACK, THEME_WHITE, THEME_RED, THEME_BLUE, THEME_DARK */
    set_style(ctx, THEME_C64);

    /* icons */

    filebrowser_init();
    sprintf(LCONTENT,"%s",RPATH);

    memset(core_key_state,0,512);
    memset(core_old_key_state ,0, sizeof(core_old_key_state));

    printf("Init nuklear %d\n",0);

 return 0;
}

int app_free()
{
   //FIXME: no more memory leak here ?
   if (RSDL_font)
      free(RSDL_font);
   RSDL_font = NULL;
   filebrowser_free();
   nk_retro_shutdown();

   Retro_FreeSurface(screen_surface);
   printf("free surfscreen\n");
   if (screen_surface)
      free(screen_surface);
   screen_surface = NULL;

   return 0;
}

int app_event(int poll)
{
	int evt;

	nk_input_begin(ctx);
	nk_retro_handle_event(&evt,poll);
	nk_input_end(ctx);

	return 0;
}

int app_render(int poll)
{
    static int prevpoll=0,prevstate=0,reset_state=0;
    if(prevpoll!=poll || reset_state){nk_clear(ctx);reset_state=0;}

    if(poll==0)
	app_vkb_handle();
    else 
	restore_bgk();

    app_event(poll);

    int state=gui(&browser,ctx);
    if(state==1 && prevstate!=1)reset_state=1;

    nk_retro_render(nk_rgba(0,0,0,0));

    prevpoll=poll;
    prevstate=state;

    return 0;
}

