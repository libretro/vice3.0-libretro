/* nuklear - v1.00 - public domain */

/* VICE HEADER */
#include "ui.h"
#include "log.h"
#include "resources.h"

#define NUMB(a) (sizeof(a) / sizeof(*a))

#define GUIRECT nk_rect(32, 35, 319, 199)

typedef enum
{
    GUI_NONE = 0,
    GUI_VKBD
} GUI_ACTION;

int GUISTATE = GUI_NONE;

extern int SHIFTON;
extern int vkey_pressed;
extern int opt_theme;

extern unsigned retro_get_borders(void);

static int gui(struct nk_context *ctx)
{
    GUISTATE = GUI_NONE;
    if(SHOWKEY==1)
        GUISTATE = GUI_VKBD;

    switch(GUISTATE)
    {
        case GUI_VKBD:
            if (nk_begin(ctx,"Vice Keyboard", GUIRECT, NK_WINDOW_NO_SCROLLBAR)) {
                switch(opt_theme) {
                    default:
                    case 0:
                        set_style(ctx, THEME_C64);
                        break;
                    case 1:
                        set_style(ctx, THEME_C64_TRANSPARENT);
                        break;
                    case 2:
                        set_style(ctx, THEME_C64C);
                        break;
                    case 3:
                        set_style(ctx, THEME_C64C_TRANSPARENT);
                        break;
                    case 4:
                        set_style(ctx, THEME_DARK_TRANSPARENT);
                        break;
                    case 5:
                        set_style(ctx, THEME_LIGHT_TRANSPARENT);
                        break;
                }
                
                /* ensure vkbd is centered regardless of border setting */
                if (retro_get_borders()) {
                    offset.x = 0;
                    offset.y = 0;
                } else {
                    offset.x = GUIRECT.x;
                    offset.y = GUIRECT.y;
                    if(retro_get_region() == RETRO_REGION_NTSC) offset.y -= 12;
                }
                nk_window_set_position(ctx, offset);
                #include "vkboard.i"
                nk_end(ctx);
            }
            break;

        default:
            break;
    }
    return GUISTATE;
}

