/*
 * Nuklear - v1.17 - public domain
 * no warrenty implied; use at your own risk.
 * authored from 2015-2016 by Micha Mettke
 */
/*
 * ==============================================================
 *
 *                              API
 *
 * ===============================================================
 */
#ifndef NK_RSDL_H_
#define NK_RSDL_H_

// RSDL_surface/RSDL_maprgba (implementation from RSDL_wrapper)
#include "RSDL_wrapper.h"

typedef struct nk_retro_Font nk_retro_Font;
typedef struct nk_retro_event nk_retro_event;

NK_API struct nk_context*   nk_retro_init(nk_retro_Font *font,RSDL_Surface *screen_surface ,unsigned int width, unsigned int height);
NK_API void                 nk_retro_handle_event(int *evt,int poll);
NK_API void                 nk_retro_render(struct nk_color clear);
NK_API void                 nk_retro_shutdown(void);

NK_API nk_retro_Font* nk_retrofont_create(const char *name, int size);
NK_API void nk_retrofont_del(nk_retro_Font *font);
NK_API void nk_retro_set_font(nk_retro_Font *font);
NK_API struct nk_retro_event* nk_retro_event_ptr();
#endif
/*
 * ==============================================================
 *
 *                          IMPLEMENTATION
 *
 * ===============================================================
 */
#ifdef NK_RETRO_SOFT_IMPLEMENTATION

#include <string.h>

// graphics primitives taken from SDL_gfxPrimitives ( minimal implementation for use with RSDL_wrapper)
#include "SDL_gfxPrimitives.h"

#ifndef MAX
#define MAX(a,b) ((a) < (b) ? (b) : (a))
#endif

#ifndef NK_RSDL_MAX_POINTS
#define NK_RSDL_MAX_POINTS 128
#endif

#include "libretro.h"

extern retro_input_poll_t input_poll_cb;
extern retro_input_state_t input_state_cb;

struct nk_retro_event {

	char key_state[512];
	char old_key_state[512];
	int LSHIFTON;
	//int MOUSE_EMULATED; // 1 = joypad act as mouse in GUI
	int MOUSE_PAS; // 4 = default
	int MOUSE_RELATIVE; //0 = absolute
	int JOYPAD_PRESSED;
	int gmx;
	int gmy; // mouse
	int mouse_wu;
	int mouse_wd;
	int slowdown;
	int showpointer;
};

static struct nk_retro_event revent;

struct nk_retro_Font {
    int width;
    int height;
    struct nk_user_font handle;
};

static struct nk_retro {
    RSDL_Surface *screen_surface;
    unsigned int width;
    unsigned int height;
    struct nk_context ctx;
} retro;

static RSDL_Rect RSDL_clip_rect;

static void
nk_retro_scissor(RSDL_Surface *surface, float x, float y, float w, float h)
{
    RSDL_clip_rect.x = x;
    RSDL_clip_rect.y = y;
    RSDL_clip_rect.w = w  + 1; 
    RSDL_clip_rect.h = h;
    RSDL_SetClipRect(surface, &RSDL_clip_rect);
}

static void
nk_retro_stroke_line(RSDL_Surface *surface, short x0, short y0, short x1,
    short y1, unsigned int line_thickness, struct nk_color col)
{
    thickLineRGBA(surface, x0, y0, x1, y1, line_thickness, col.r, col.g, col.b, col.a);
}

static void
nk_retro_stroke_rect(RSDL_Surface *surface, short x, short y, unsigned short w,
    unsigned short h, unsigned short r, unsigned short line_thickness, struct nk_color col)
{
    /* Note: thickness is not used by default */
    if (r == 0) {
        rectangleRGBA(surface, x, y, x + w, y + h, col.r, col.g, col.b, col.a); 
    } else {
        roundedRectangleRGBA(surface, x, y, x + w, y + h, r, col.r, col.g, col.b, col.a);
    }
}

static void
nk_retro_fill_rect(RSDL_Surface *surface, short x, short y, unsigned short w,
    unsigned short h, unsigned short r, struct nk_color col)
{
    if (r == 0) {
       boxRGBA(surface, x, y, x + w, y + h, col.r, col.g, col.b, col.a); 
    } else {
        roundedBoxRGBA(surface, x, y, x + w, y + h, r, col.r, col.g, col.b, col.a);
    }
}

static void 
nk_retro_fill_triangle(RSDL_Surface *surface, short x0, short y0, short x1, short y1, short x2, short y2, struct nk_color col)
{
    filledTrigonRGBA(surface, x0, y0, x1, y1, x2, y2, col.r, col.g, col.b, col.a);
}

static void
nk_retro_stroke_triangle(RSDL_Surface *surface, short x0, short y0, short x1,
    short y1, short x2, short y2, unsigned short line_thickness, struct nk_color col)
{
    /* Note: thickness is not used by default */
    aatrigonRGBA(surface, x0, y0, x1, y1, x2, y2, col.r, col.g, col.b, col.a); 
}

static void
nk_retro_fill_polygon(RSDL_Surface *surface, const struct nk_vec2i *pnts, int count, struct nk_color col)
{
    Sint16 p_x[NK_RSDL_MAX_POINTS];
    Sint16 p_y[NK_RSDL_MAX_POINTS];
    int i;
    for (i = 0; (i < count) && (i < NK_RSDL_MAX_POINTS); ++i) {
        p_x[i] = pnts[i].x;
        p_y[i] = pnts[i].y;
    }
    filledPolygonRGBA (surface, (Sint16 *)p_x, (Sint16 *)p_y, count, col.r, col.g, col.b, col.a);
}

static void
nk_retro_stroke_polygon(RSDL_Surface *surface, const struct nk_vec2i *pnts, int count,
    unsigned short line_thickness, struct nk_color col)
{
    /* Note: thickness is not used by default */
    Sint16 p_x[NK_RSDL_MAX_POINTS];
    Sint16 p_y[NK_RSDL_MAX_POINTS];
    int i;
    for (i = 0; (i < count) && (i < NK_RSDL_MAX_POINTS); ++i) {
        p_x[i] = pnts[i].x;
        p_y[i] = pnts[i].y;
    }
    aapolygonRGBA(surface, (Sint16 *)p_x, (Sint16 *)p_y, count, col.r, col.g, col.b, col.a); 
}

static void
nk_retro_stroke_polyline(RSDL_Surface *surface, const struct nk_vec2i *pnts,
    int count, unsigned short line_thickness, struct nk_color col)
{
    int x0, y0, x1, y1;
    if (count == 1) {
        x0 = pnts[0].x;
        y0 = pnts[0].y;
        x1 = x0;
        y1 = y0;
        thickLineRGBA(surface, x0, y0, x1, y1, line_thickness, col.r, col.g, col.b, col.a);
    } else if (count >= 2) {
        int i;
        for (i = 0; i < (count - 1); i++) {
            x0 = pnts[i].x;
            y0 = pnts[i].y;
            x1 = pnts[i + 1].x;
            y1 = pnts[i + 1].y;
            thickLineRGBA(surface, x0, y0, x1, y1, line_thickness, col.r, col.g, col.b, col.a);
        }
    }
}

static void
nk_retro_fill_circle(RSDL_Surface *surface, short x, short y, unsigned short w,
    unsigned short h, struct nk_color col)
{
    filledEllipseRGBA(surface,  x + w /2, y + h /2, w / 2, h / 2, col.r, col.g, col.b, col.a); 
}

static void
nk_retro_stroke_circle(RSDL_Surface *surface, short x, short y, unsigned short w,
    unsigned short h, unsigned short line_thickness, struct nk_color col)
{
    /* Note: thickness is not used by default */
    aaellipseRGBA (surface,  x + w /2, y + h /2, w / 2, h / 2, col.r, col.g, col.b, col.a); 
}

static void
nk_retro_stroke_curve(RSDL_Surface *surface, struct nk_vec2i p1,
    struct nk_vec2i p2, struct nk_vec2i p3, struct nk_vec2i p4, unsigned int num_segments,
    unsigned short line_thickness, struct nk_color col)
{
    unsigned int i_step;
    float t_step;
    struct nk_vec2i last = p1;

    num_segments = MAX(num_segments, 1);
    t_step = 1.0f/(float)num_segments;
    for (i_step = 1; i_step <= num_segments; ++i_step) {
        float t = t_step * (float)i_step;
        float u = 1.0f - t;
        float w1 = u*u*u;
        float w2 = 3*u*u*t;
        float w3 = 3*u*t*t;
        float w4 = t * t *t;
        float x = w1 * p1.x + w2 * p2.x + w3 * p3.x + w4 * p4.x;
        float y = w1 * p1.y + w2 * p2.y + w3 * p3.y + w4 * p4.y;
        nk_retro_stroke_line(surface, last.x, last.y, (short)x, (short)y, line_thickness, col);
        last.x = (short)x; last.y = (short)y;
    }
}

/*static*/ void
nk_retro_draw_text(RSDL_Surface *surface, short x, short y, unsigned short w, unsigned short h,
    const char *text, int len, nk_retro_Font *font, struct nk_color cbg, struct nk_color cfg)
{
    int i;
    nk_retro_fill_rect(surface, x, y, len * font->width, font->height, 0, cbg);

    for (i = 0; i < len; i++) {
        //characterRGBA(surface, x, y, text[i], cfg.r, cfg.g, cfg.b, cfg.a);
#ifdef M16B

	Retro_Draw_char(surface,  x,  y,  text[i],  1, 1,cfg.r<<8|cfg.g<<3|cfg.b>>3,0);

//	Retro_Draw_char(surface,  x,  y,  text[i],  1, 1,/*cfg.a<<8|*/cfg.r<<8|cfg.g<<3|cfg.b>>3, /*cbg.a<<24|*/cbg.r<<8|cbg.g<<3|cbg.b>>3);
#else
	Retro_Draw_char(surface,  x,  y,  text[i],  1, 1,cfg.a<<24|cfg.r<<16|cfg.g<<8|cfg.b, cbg.a<<24|cbg.r<<16|cbg.g<<8|cbg.b);
#endif
        x += font->width;
    }

//FIXME TODO
// Retro_Draw_string(surface,  x,  y,  text, len, 1, 1,cfg.a<<24|cfg.r<<16|cfg.g<<8|cfg.b, cbg.a<<24|cbg.r<<16|cbg.g<<8|cbg.b);

}
static void
interpolate_color(struct nk_color c1, struct nk_color c2, struct nk_color *result, float fraction)
{
    float r = c1.r + (c2.r - c1.r) * fraction;
    float g = c1.g + (c2.g - c1.g) * fraction;
    float b = c1.b + (c2.b - c1.b) * fraction;
    float a = c1.a + (c2.a - c1.a) * fraction;

    result->r = (nk_byte)NK_CLAMP(0, r, 255);
    result->g = (nk_byte)NK_CLAMP(0, g, 255);
    result->b = (nk_byte)NK_CLAMP(0, b, 255);
    result->a = (nk_byte)NK_CLAMP(0, a, 255);
}

static void
nk_retro_fill_rect_multi_color(RSDL_Surface *surface, short x, short y, unsigned short w, unsigned short h,
    struct nk_color left, struct nk_color top,  struct nk_color right, struct nk_color bottom)
{
    struct nk_color X1, X2, Y;
    float fraction_x, fraction_y;
    int i,j;

    for (j = 0; j < h; j++) {
        fraction_y = ((float)j) / h;
        for (i = 0; i < w; i++) {
            fraction_x = ((float)i) / w;
            interpolate_color(left, top, &X1, fraction_x);
            interpolate_color(right, bottom, &X2, fraction_x);
            interpolate_color(X1, X2, &Y, fraction_y);
            pixelRGBA(surface, x + i, y + j, Y.r, Y.g, Y.b, Y.a); 
        }
    }
}

static void
nk_retro_clear(RSDL_Surface *surface, struct nk_color col)
{
    nk_retro_fill_rect(surface, 0, 0, surface->w, surface->h, 0, col);
}

static void
nk_retro_blit(RSDL_Surface *surface)
{
}

NK_API void
nk_retro_render(struct nk_color clear)
{
    const struct nk_command *cmd;

    RSDL_Surface *screen_surface = retro.screen_surface;
    nk_retro_clear(screen_surface, clear);

    nk_foreach(cmd, &retro.ctx)
    {
        switch (cmd->type) {
        case NK_COMMAND_NOP: break;
        case NK_COMMAND_SCISSOR: {
            const struct nk_command_scissor *s =(const struct nk_command_scissor*)cmd;
            nk_retro_scissor(screen_surface, s->x, s->y, s->w, s->h);
        } break;
        case NK_COMMAND_LINE: {
            const struct nk_command_line *l = (const struct nk_command_line *)cmd;
            nk_retro_stroke_line(screen_surface, l->begin.x, l->begin.y, l->end.x,
                l->end.y, l->line_thickness, l->color);
        } break;
        case NK_COMMAND_RECT: {
            const struct nk_command_rect *r = (const struct nk_command_rect *)cmd;
            nk_retro_stroke_rect(screen_surface, r->x, r->y, r->w, r->h,
                (unsigned short)r->rounding, r->line_thickness, r->color);
        } break;
        case NK_COMMAND_RECT_FILLED: {
            const struct nk_command_rect_filled *r = (const struct nk_command_rect_filled *)cmd;
            nk_retro_fill_rect(screen_surface, r->x, r->y, r->w, r->h,
                (unsigned short)r->rounding, r->color);
        } break;
        case NK_COMMAND_CIRCLE: {
            const struct nk_command_circle *c = (const struct nk_command_circle *)cmd;
            nk_retro_stroke_circle(screen_surface, c->x, c->y, c->w, c->h, c->line_thickness, c->color);
        } break;
        case NK_COMMAND_CIRCLE_FILLED: {
            const struct nk_command_circle_filled *c = (const struct nk_command_circle_filled *)cmd;
            nk_retro_fill_circle(screen_surface, c->x, c->y, c->w, c->h, c->color);
        } break;
        case NK_COMMAND_TRIANGLE: {
            const struct nk_command_triangle*t = (const struct nk_command_triangle*)cmd;
            nk_retro_stroke_triangle(screen_surface, t->a.x, t->a.y, t->b.x, t->b.y,
                t->c.x, t->c.y, t->line_thickness, t->color);
        } break;
        case NK_COMMAND_TRIANGLE_FILLED: {
            const struct nk_command_triangle_filled *t = (const struct nk_command_triangle_filled *)cmd;
            nk_retro_fill_triangle(screen_surface, t->a.x, t->a.y, t->b.x, t->b.y, t->c.x, t->c.y, t->color);
        } break;
        case NK_COMMAND_POLYGON: {
            const struct nk_command_polygon *p =(const struct nk_command_polygon*)cmd;
            nk_retro_stroke_polygon(screen_surface, p->points, p->point_count, p->line_thickness,p->color);
        } break;
        case NK_COMMAND_POLYGON_FILLED: {
            const struct nk_command_polygon_filled *p = (const struct nk_command_polygon_filled *)cmd;
            nk_retro_fill_polygon(screen_surface, p->points, p->point_count, p->color);
        } break;
        case NK_COMMAND_POLYLINE: {
            const struct nk_command_polyline *p = (const struct nk_command_polyline *)cmd;
            nk_retro_stroke_polyline(screen_surface, p->points, p->point_count, p->line_thickness, p->color);
        } break;
        case NK_COMMAND_TEXT: {
            const struct nk_command_text *t = (const struct nk_command_text*)cmd;
            nk_retro_draw_text(screen_surface, t->x, t->y, t->w, t->h,
                (const char*)t->string, t->length,
                (nk_retro_Font*)t->font->userdata.ptr,
                t->background, t->foreground);
        } break;
        case NK_COMMAND_CURVE: {
            const struct nk_command_curve *q = (const struct nk_command_curve *)cmd;
            nk_retro_stroke_curve(screen_surface, q->begin, q->ctrl[0], q->ctrl[1],
                q->end, 22, q->line_thickness, q->color);
        } break;
        case NK_COMMAND_RECT_MULTI_COLOR: {
            const struct nk_command_rect_multi_color *r = (const struct nk_command_rect_multi_color *)cmd;
            nk_retro_fill_rect_multi_color(screen_surface, r->x, r->y, r->w, r->h, r->left, r->top, r->right, r->bottom);
        } break;
        case NK_COMMAND_IMAGE:
        case NK_COMMAND_ARC:
        case NK_COMMAND_ARC_FILLED:
        default: break;
        }
    }
    nk_retro_blit(retro.screen_surface);
    nk_clear(&retro.ctx);
	
	//FIXME draw only in fullscreen or mouse grabbed or when joypad emulate mouse
	if(revent.showpointer==1)draw_cross(retro.screen_surface,revent.gmx,revent.gmy);

}

static void
nk_retro_clipbard_paste(nk_handle usr, struct nk_text_edit *edit)
{
    /* Not supported in SDL 1.2. Use platform specific code.  */
}

static void
nk_retro_clipbard_copy(nk_handle usr, const char *text, int len)
{
    /* Not supported in SDL 1.2. Use platform specific code.  */
}

nk_retro_Font*
nk_retrofont_create(const char *name, int size)
{

    nk_retro_Font *font = (nk_retro_Font*)calloc(1, sizeof(nk_retro_Font));
    font->width = 8; /* Default in  the RSDL_gfx library */
    font->height = 8; /* Default in  the RSDL_gfx library */
    if (!font)
        return NULL;
   //font->handle

    return font;
}
void
nk_retrofont_del(nk_retro_Font *font)
{
    if(!font) return;

    free(font);
}

static float
nk_retro_get_text_width(nk_handle handle, float height, const char *text, int len)
{
    nk_retro_Font *font = (nk_retro_Font*)handle.ptr;
 
    if(!font || !text)
        return 0;
    return len * font->width;
}

void reset_mouse_pos(){
	revent.gmx=178;
	revent.gmy=164;
}

static void retro_init_event()
{
	//revent.MOUSE_EMULATED=1;
	revent.MOUSE_PAS=28;
	revent.MOUSE_RELATIVE=1;
	revent.JOYPAD_PRESSED=0;
	revent.gmx=178;
	revent.gmy=164;
	revent.mouse_wu=0;
	revent.mouse_wd=0;
	revent.slowdown=0;
	memset(revent.key_state,0,512);
	memset(revent.old_key_state ,0, sizeof(revent.old_key_state));
	revent.LSHIFTON=-1;
	revent.showpointer=0;
}


NK_API struct nk_retro_event* 
nk_retro_event_ptr()
{
	nk_retro_event* event=&revent;
	return event;
}

NK_API struct nk_context*
nk_retro_init(nk_retro_Font *rsdlfont,RSDL_Surface *screen_surface,unsigned int w, unsigned int h)
{
    struct nk_user_font *font=&rsdlfont->handle;

    font->userdata = nk_handle_ptr(rsdlfont);
    font->height = (float)rsdlfont->height;
    font->width = nk_retro_get_text_width;

    retro.screen_surface = screen_surface;
    retro.width=retroW;//w;
    retro.height=retroH;//h;

    nk_init_default(&retro.ctx, font);
    retro.ctx.clip.copy = nk_retro_clipbard_copy;
    retro.ctx.clip.paste = nk_retro_clipbard_paste;
    retro.ctx.clip.userdata = nk_handle_ptr(0);

    retro_init_event();

    return &retro.ctx;
}

NK_API void
nk_retro_set_font(nk_retro_Font *xfont)
{
    struct nk_user_font *font = &xfont->handle;
    font->userdata = nk_handle_ptr(xfont);
    font->height = (float)xfont->height;
    font->width = nk_retro_get_text_width;
    nk_style_set_font(&retro.ctx, font);
}

static void retro_key(int key,int down)
{
	struct nk_context *ctx = &retro.ctx;
	if(key<512);
	else return;

        if (key == RETROK_RSHIFT || key == RETROK_LSHIFT) nk_input_key(ctx, NK_KEY_SHIFT, down);
        else if (key == RETROK_DELETE)    nk_input_key(ctx, NK_KEY_DEL, down);
        else if (key == RETROK_RETURN)    nk_input_key(ctx, NK_KEY_ENTER, down);
        else if (key == RETROK_TAB)       nk_input_key(ctx, NK_KEY_TAB, down);
        else if (key == RETROK_LEFT)      nk_input_key(ctx, NK_KEY_LEFT, down);
        else if (key == RETROK_RIGHT)     nk_input_key(ctx, NK_KEY_RIGHT, down);
        else if (key == RETROK_UP)        nk_input_key(ctx, NK_KEY_UP, down);
        else if (key == RETROK_DOWN)      nk_input_key(ctx, NK_KEY_DOWN, down);
        else if (key == RETROK_BACKSPACE) nk_input_key(ctx, NK_KEY_BACKSPACE, down);
        else if (key == RETROK_HOME)      nk_input_key(ctx, NK_KEY_TEXT_START, down);
        else if (key == RETROK_END)       nk_input_key(ctx, NK_KEY_TEXT_END, down);
        else if (key == RETROK_SPACE && !down) nk_input_char(ctx, ' ');

        else if (key >= RETROK_0 && key <= RETROK_9) {
                    nk_rune rune = '0' + key - RETROK_0;
                    nk_input_unicode(ctx, rune);
        }
	else if (key >= RETROK_a && key <= RETROK_z) {
                    nk_rune rune = 'a' + key - RETROK_a;
                    rune = ((1 == revent.LSHIFTON) ? (nk_rune)nk_to_upper((int)rune):rune);
                    nk_input_unicode(ctx, rune);
        }
}


static void mousebut(int but,int down,int x,int y){

	struct nk_context *ctx = &retro.ctx;

 	if(but==1)nk_input_button(ctx, NK_BUTTON_LEFT, x, y, down);
 	else if(but==2)nk_input_button(ctx, NK_BUTTON_RIGHT, x, y, down);
 	else if(but==3)nk_input_button(ctx, NK_BUTTON_MIDDLE, x, y, down);
	else if(but==4)nk_input_scroll(ctx,(float)down);
	//printf("but:%d %s (%d,%d)\n",but,down==1?"pressed":"released",x,y);
}

static void Process_key()
{
	int i;

	for(i=0;i<320;i++)
        	revent.key_state[i]=input_state_cb(0, RETRO_DEVICE_KEYBOARD, 0,i) ? 0x80: 0;
   
	if(memcmp( revent.key_state,revent.old_key_state , sizeof(revent.key_state) ) )
	 	for(i=0;i<320;i++)
			if(revent.key_state[i] && revent.key_state[i]!=revent.old_key_state[i]  )
        	{
	
				if(i==RETROK_LSHIFT){
					revent.LSHIFTON=-revent.LSHIFTON;
					printf("Modifier shift pressed %d \n",revent.LSHIFTON); 
					continue;
				}
/*
				if(i==RETROK_F12){
					//play_tape();
					continue;
				}

				if(i==RETROK_RCTRL){
					//CTRLON=-CTRLON;
					printf("Modifier crtl pressed %d \n",CTRLON); 
					continue;
				}
				if(i==RETROK_RSHIFT){
					//SHITFON=-SHITFON;
					printf("Modifier shift pressed %d \n",SHIFTON); 
					continue;
				}

				if(i==RETROK_LALT){
					//KBMOD=-KBMOD;
					printf("Modifier alt pressed %d \n",KBMOD); 
					continue;
				}
//printf("press: %d \n",i);
*/
				retro_key(i,1);
	
        	}	
        	else if ( !revent.key_state[i] && revent.key_state[i]!=revent.old_key_state[i]  )
        	{
				if(i==RETROK_LSHIFT){
					revent.LSHIFTON=-revent.LSHIFTON;
					printf("Modifier shift released %d \n",revent.LSHIFTON); 
					continue;
				}
/*
				if(i==RETROK_F12){
      				//kbd_buf_feed("|tape\nrun\"\n^");
					continue;
				}

				if(i==RETROK_RCTRL){
					CTRLON=-CTRLON;
					printf("Modifier crtl released %d \n",CTRLON); 
					continue;
				}
				if(i==RETROK_RSHIFT){
					SHIFTON=-SHIFTON;
					printf("Modifier shift released %d \n",SHIFTON); 
					continue;
				}

				if(i==RETROK_LALT){
					KBMOD=-KBMOD;
					printf("Modifier alt released %d \n",KBMOD); 
					continue;
				}
//printf("release: %d \n",i);
*/
				retro_key(i,0);
	
        	}	

	memcpy(revent.old_key_state,revent.key_state , sizeof(revent.key_state) );

}

NK_API void
nk_retro_handle_event(int *evt,int poll)
{
   struct nk_context *ctx = &retro.ctx;

   if(poll)input_poll_cb();

   static int lmx=0,lmy=0;
   static int mmbL=0,mmbR=0,mmbM=0;
   static int mbt[16]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

   int mouse_l,mouse_m,mouse_r;

   int16_t mouse_x=0,mouse_y=0;

   Process_key();

   /* pointless
   int i=2;//TOGGLE: real mouse/ joypad emulate mouse 
   if ( input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, i) && mbt[i]==0 )
      mbt[i]=1;
   else if ( mbt[i]==1 && ! input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, i) ){
      mbt[i]=0;
      revent.MOUSE_EMULATED=-revent.MOUSE_EMULATED;
   }
   */

   revent.mouse_wu = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_WHEELUP);
   revent.mouse_wd = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_WHEELDOWN);
   if(revent.mouse_wu || revent.mouse_wd)mousebut(4,revent.mouse_wd?-1:1,0,0);

    // Joypad buttons
    mouse_l = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A);
    mouse_r = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B);
    mouse_m = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y);
       
    if(!mouse_l && !mouse_r && !mouse_m) {
       // Mouse buttons
       mouse_l = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_LEFT);
       mouse_r = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_RIGHT);
       mouse_m = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_MIDDLE); 
   }

	//relative
	if(revent.MOUSE_RELATIVE){

      		// Joypad
      		if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT)) {
      		    mouse_x += revent.MOUSE_PAS;
      		    revent.JOYPAD_PRESSED = 1;
            }
      		else if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT)) {
      		    mouse_x -= revent.MOUSE_PAS;
      		    revent.JOYPAD_PRESSED = 1;
            } 
      		else if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN)) {
      		    mouse_y += revent.MOUSE_PAS;
      		    revent.JOYPAD_PRESSED = 1;
            }
      		else if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP)) {
      		    mouse_y -= revent.MOUSE_PAS;
      		    revent.JOYPAD_PRESSED = 1;
            } else {
                revent.JOYPAD_PRESSED = 0;

                // Mouse
                mouse_x = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_X);
                mouse_y = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_Y);
                
                if(mouse_x || mouse_y) {
                    revent.showpointer = 1;
                }
            }
            
   		if(revent.JOYPAD_PRESSED == 0) {
   		    revent.slowdown = 0;
   		}

   		if(revent.slowdown>0) return;

   		if(revent.JOYPAD_PRESSED > 0) {
   		    revent.showpointer = 0;
   		    revent.slowdown = 1;
   		}

   		revent.gmx+=mouse_x;
   		revent.gmy+=mouse_y;
   		
   		if(revent.JOYPAD_PRESSED > 0) {
   	    	// Joypad wraparound
            // Offset changes depending on whether borders are on or off
            struct nk_vec2 offset;
            offset = nk_window_get_position(ctx);
            if(revent.gmx<offset.x+12) 	revent.gmx=offset.x+319-20-12;
            if(revent.gmx>offset.x+319-12) revent.gmx=offset.x+18+12;
            if(revent.gmy<offset.y+4) 	revent.gmy=offset.y+199-12;
            if(revent.gmy>offset.y+199-4) revent.gmy=offset.y+20;
   		} else {
       		// Mouse corners
       		if(revent.gmx<0)		revent.gmx=0;
       		if(revent.gmx>retroW-1)	revent.gmx=retroW-1;
       		if(revent.gmy<0)		revent.gmy=0;
       		if(revent.gmy>retroH-1)	revent.gmy=retroH-1;
       	}	

	}
   else{

      //absolute
      //FIXME FULLSCREEN no pointer
      int p_x = input_state_cb(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_X);
      int p_y = input_state_cb(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_Y);

      if(p_x!=0 && p_y!=0)
      {
         int px=(int)((p_x+0x7fff)*retro.width/0xffff);
         int py=(int)((p_y+0x7fff)*retro.height/0xffff);
         //printf("px:%d py:%d (%d,%d)\n",p_x,p_y,px,py);
         revent.gmx=px;
         revent.gmy=py;
#if 0
#if defined(__ANDROID__) || defined(ANDROID)
         //mouse_l=input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A);
         //mouse_r=input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B);

         if(holdleft==0)
         {
            starthold=GetTicks();
            holdleft=1;	
         }
         else if(holdleft==1)
         {

            timehold=GetTicks()-starthold;

            if(timehold>250)
            {
               mouse_l=input_state_cb(0, RETRO_DEVICE_POINTER, 	0,RETRO_DEVICE_ID_POINTER_PRESSED);
            }
         }

         //mouse_l=input_state_cb(0, RETRO_DEVICE_POINTER, 0,RETRO_DEVICE_ID_POINTER_PRESSED);

         //FIXME: mouse left button in scale widget.
#endif
#endif
      }

   }

    if(mmbL==0 && mouse_l){

		mmbL=1;		
		mousebut(1,1,revent.gmx,revent.gmy);
    }
    else if(mmbL==1 && !mouse_l)
    {
#if 0
#if defined(__ANDROID__) || defined(ANDROID)
       holdleft=0;
#endif
#endif
       mmbL=0;
       mousebut(1,0,revent.gmx,revent.gmy);
    }

    if(mmbR==0 && mouse_r){

      	mmbR=1;
		mousebut(2,1,revent.gmx,revent.gmy);		
   	}
   	else if(mmbR==1 && !mouse_r) {

      	mmbR=0;
		mousebut(2,0,revent.gmx,revent.gmy);
   	}

   	if(mmbM==0 && mouse_m){

      	mmbM=1;
		mousebut(3,1,revent.gmx,revent.gmy);		
   	}
   	else if(mmbM==1 && !mouse_m) {

      	mmbM=0;
		mousebut(3,0,revent.gmx,revent.gmy);
   	}

	if(revent.gmx!=lmx || lmy!=revent.gmy){
		nk_input_motion(ctx, revent.gmx, revent.gmy);
		//printf("mx:%d my:%d \n",gmx,gmy);
	}
	lmx=revent.gmx;lmy=revent.gmy;

}

NK_API void
nk_retro_shutdown(void)
{
    nk_free(&retro.ctx);
}


#endif

