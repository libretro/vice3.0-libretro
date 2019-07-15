/* nuklear - v1.00 - public domain */

/* VICE HEADER */
#include "autostart.h"
#include "vice.h"
#include "lib.h"
#include "machine.h"
#include "ui.h"
#include "attach.h"
#include "util.h"
#include "sid.h"
#include "log.h"
#include "resources.h"
#include "tape.h"
#include "cartridge.h"
#include "imagecontents.h"
#include "tapecontents.h"
#include "diskcontents.h"
#include "videoarch.h"
#include "fliplist.h"
#include "c64model.h"
#include "palette.h"

//c64/vic20 model option
#if  defined(__VIC20__)
#include "vic20model.h"
static const char *c64mod[] =  {"VIC20 PAL","VIC20 NTSC","VIC21","VIC20 UNKNOW Model"};
int c64modint[] ={
		VIC20MODEL_VIC20_PAL,VIC20MODEL_VIC20_NTSC ,VIC20MODEL_VIC21 ,VIC20MODEL_UNKNOWN
};
#elif defined(__PLUS4__)
#include "plus4model.h"
static const char *c64mod[] =  {"C16/C116 (PAL)","C16/C116 (NTSC)","Plus4 (PAL)","Plus4/C264 (NTSC)","V364 (NTSC)","C232 (NTSC)","PLUS4 UNKNOW Model"};
int c64modint[] ={
		PLUS4MODEL_C16_PAL,PLUS4MODEL_C16_NTSC ,PLUS4MODEL_PLUS4_PAL ,PLUS4MODEL_PLUS4_NTSC,
PLUS4MODEL_V364_NTSC,PLUS4MODEL_232_NTSC,PLUS4MODEL_UNKNOWN
};
#elif defined(__X128__)
#include "c128model.h"
static const char *c64mod[] =  {"C128 PAL","C128DCR PAL","C128 NTSC","C128DCR NTSC","C128 UNKNOW Model"};
int c64modint[] ={
		C128MODEL_C128_PAL,C128MODEL_C128DCR_PAL ,C128MODEL_C128_NTSC ,C128MODEL_C128DCR_NTSC,
C128MODEL_UNKNOWN
};
#elif defined(__PET__)
#include "petmodel.h"
static const char *c64mod[] =  {"PET 2001","PET 3008","PET 3016","PET 3032","PET 3032B","PET 4016","PET 4032","PET 4032B","PET 8032","PET 8096","PET 8296","SUPERPET","PET UNKNOWN Model"};
int c64modint[] ={
		PETMODEL_2001,PETMODEL_3008,PETMODEL_3016,PETMODEL_3032,PETMODEL_3032B,PETMODEL_4016,PETMODEL_4032,PETMODEL_4032B,PETMODEL_8032,PETMODEL_8096,PETMODEL_8296,PETMODEL_SUPERPET,
PETMODEL_UNKNOWN
};
#else
static const char *c64mod[] =  {"C64 PAL","C64C PAL","C64 old PAL","C64 NTSC","C64C NTSC","C64 old NTSC","Drean","C64 SX PAL","C64 SX NTSC","Japanese","C64 GS","PET64 PAL","PET64 NTSC","MAX Machine","C64 UNKNOW Model"};
int c64modint[] ={
		C64MODEL_C64_PAL ,C64MODEL_C64C_PAL ,C64MODEL_C64_OLD_PAL ,C64MODEL_C64_NTSC ,
		C64MODEL_C64C_NTSC ,C64MODEL_C64_OLD_NTSC ,C64MODEL_C64_PAL_N ,C64MODEL_C64SX_PAL ,C64MODEL_C64SX_NTSC ,
		C64MODEL_C64_JAP ,C64MODEL_C64_GS ,C64MODEL_PET64_PAL,C64MODEL_PET64_NTSC ,C64MODEL_ULTIMAX ,C64MODEL_UNKNOWN
};
#endif

//floppy option
static char DF8NAME[512]="Choose Content\0";
static char DF9NAME[512]="Choose Content\0";
static const char *drivename[] =  {"1540","1541","1542","1551","1570","1571","1573","1581","2000","4000","2031","2040","3040","4040","1001","8050","8250"};

#define GET_DRIVE(code) ((code)&0x0F)
#define NUMB(a) (sizeof(a) / sizeof(*a))
#define GUIRECT nk_rect(5,25, 364+10, 222) //nk_rect(10,30, 364, 212)

typedef enum
{
	GUI_NONE = 0,
	GUI_BROWSE ,
	GUI_VKBD ,
	GUI_MAIN ,

} GUI_ACTION;

int GUISTATE=GUI_NONE;

extern int pauseg;
extern int NPAGE,SHIFTON;
extern int vkey_pressed;
extern int vice_statusbar;
extern unsigned int cur_port;
extern int retrojoy_init;

extern char DISKA_NAME[512];
extern char DISKB_NAME[512];
extern char TAPE_NAME[512];

extern void emu_reset(void);
extern int HandleExtension(char *path,char *ext);
extern void set_drive_type(int drive,int val);
extern void retro_shutdown_core(void);

static char **palette_av;
static char **sid_mod;

static int
gui(struct file_browser *browser,struct nk_context *ctx)
{
    struct nk_rect total_space;

    /* window flags */
    static int border = nk_true;
    static int resize = nk_true;
    static int movable = nk_true;
    static int no_scrollbar = nk_false;
    static nk_flags window_flags = 0;
    static int minimizable = nk_true;
    static int title = nk_true;

    /* window flags */
    window_flags = 0;

    if (border) window_flags |= NK_WINDOW_BORDER;
    if (resize) window_flags |= NK_WINDOW_SCALABLE;
    if (movable) window_flags |= NK_WINDOW_MOVABLE;
    if (no_scrollbar || (pauseg==1 && LOADCONTENT==1) ) window_flags |= NK_WINDOW_NO_SCROLLBAR;
    if (minimizable) window_flags |= NK_WINDOW_MINIMIZABLE;
    if (title) window_flags |= NK_WINDOW_TITLE;

    int tmpval;

    if(pauseg==1 && SHOWKEY==1)SHOWKEY=-1;
    if(pauseg==0 && SHOWKEY==1)GUISTATE=GUI_VKBD;
    if(pauseg==1 && SHOWKEY==-1 && LOADCONTENT==1)GUISTATE=GUI_BROWSE;
    if(pauseg==1 && SHOWKEY==-1 && LOADCONTENT!=1)GUISTATE=GUI_MAIN;

    switch(GUISTATE){

	case GUI_VKBD:

		if (nk_begin(ctx,"Vice keyboard", GUIRECT, window_flags)){
		#include "vkboard.i"
	    	nk_end(ctx);
		}
		break;
	
	case GUI_BROWSE:

		if (nk_begin(ctx,"File Select", GUIRECT,NK_WINDOW_TITLE| NK_WINDOW_BORDER|NK_WINDOW_NO_SCROLLBAR|NK_WINDOW_MOVABLE)){
		#include "filebrowser.i"
		nk_end(ctx);
		}	
		break;

	case GUI_MAIN:

		if (nk_begin(ctx,"Vice GUI", GUIRECT, window_flags)){
		#include "c64menu.i"
		nk_end(ctx);
		}
		break;

	default:				
		break;

    }

    if(pauseg==1 && SHOWKEY==1)SHOWKEY=-1;
    if(pauseg==0 && SHOWKEY==1)GUISTATE=GUI_VKBD;
    if(pauseg==1 && SHOWKEY==-1 && LOADCONTENT==1)GUISTATE=GUI_BROWSE;
    if(pauseg==1 && SHOWKEY==-1 && LOADCONTENT!=1)GUISTATE=GUI_MAIN;

return GUISTATE;
}

