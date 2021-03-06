#include "libretro-core.h"
#include "libretro-graph.h"
#include "libretro-vkbd.h"

bool retro_vkbd = false;
bool retro_vkbd_transparent = true;
extern bool retro_capslock;
extern int vkflag[10];
extern unsigned int opt_vkbd_theme;
extern libretro_graph_alpha_t opt_vkbd_alpha;
extern unsigned int zoom_mode_id;

int RGB(int r, int g, int b)
{
   if (pix_bytes == 4) 
      return ARGB888(255, r, g, b);
   else
      return RGB565(r, g, b);
}

static int BKG_PADDING(int len)
{
   static int font_width = 6;
   switch (len)
   {
      case 2:
         return -font_width;
         break;
      case 3:
         return -((font_width / 2) * 3);
         break;
   }
   return 0;
}

void print_vkbd(void)
{
   bool shifted                      = false;
   bool text_outline                 = false;
   int page                          = 0;
   int x                             = 0;
   int y                             = 0;

   int XKEY                          = 0;
   int YKEY                          = 0;
   int XTEXT                         = 0;
   int YTEXT                         = 0;
   int XOFFSET                       = 0;
   int XPADDING                      = 0;
   int YOFFSET                       = 0;
   int YPADDING                      = 0;
   int XKEYSPACING                   = 1;
   int YKEYSPACING                   = 1;
   libretro_graph_alpha_t ALPHA      = opt_vkbd_alpha;
   libretro_graph_alpha_t BKG_ALPHA  = ALPHA;
   int BKG_PADDING_X                 = 0;
   int BKG_PADDING_Y                 = 0;
   int BKG_COLOR                     = 0;
   int BKG_COLOR_NORMAL              = 0;
   int BKG_COLOR_ALT                 = 0;
   int BKG_COLOR_EXTRA               = 0;
   int BKG_COLOR_EXTRA2              = 0;
   int BKG_COLOR_SEL                 = 0;
   int BKG_COLOR_ACTIVE              = 0;
   int FONT_MAX                      = 3;
   int FONT_WIDTH                    = 1;
   int FONT_HEIGHT                   = 1;
   int FONT_COLOR                    = 0;
   int FONT_COLOR_NORMAL             = 0;
   int FONT_COLOR_SEL                = 0;

   unsigned COLOR_BLACK              = RGB(  5,   5,   5);
   unsigned COLOR_WHITE              = RGB(250, 250, 250);

   unsigned theme                    = opt_vkbd_theme;
   if (theme & 0x80)
   {
      text_outline = true;
      theme &= ~0x80;
   }

   if (!theme)
   {
#if defined(__X64__) || defined(__X64SC__) || defined(__XSCPU64__)
      unsigned model = vice_opt.Model;
      if (request_model_set > -1 && request_model_set != model)
         model = request_model_set;

      switch (model)
      {
         case C64MODEL_C64C_PAL:
         case C64MODEL_C64C_NTSC:
         case C64MODEL_C64_GS:
            theme = 2;
            break;
         default:
            theme = 1;
            break;
      }
#elif defined(__XVIC__)
      theme = 1;
#elif defined(__XPLUS4__) || defined(__X128__)
      theme = 2;
#else
      theme = 3;
#endif
   }

   switch (theme)
   {
      default:
      case 1: /* C64 brown */
         BKG_COLOR_NORMAL  = RGB( 68,  59,  58);
         BKG_COLOR_ALT     = RGB(123, 127, 130);
         BKG_COLOR_EXTRA   = RGB(143, 140, 129);
         BKG_COLOR_EXTRA2  = RGB( 89,  79,  78);
         BKG_COLOR_SEL     = RGB(160, 160, 160);
         BKG_COLOR_ACTIVE  = RGB( 48,  44,  45);
         FONT_COLOR_NORMAL = COLOR_WHITE;
         FONT_COLOR_SEL    = COLOR_BLACK;
         break;
      
      case 2: /* C64C beige */
         BKG_COLOR_NORMAL  = RGB(208, 208, 202);
         BKG_COLOR_ALT     = RGB(154, 154, 150);
         BKG_COLOR_EXTRA   = RGB(132, 132, 132);
         BKG_COLOR_EXTRA2  = RGB( 89,  79,  78);
         BKG_COLOR_SEL     = RGB( 40,  40,  40);
         BKG_COLOR_ACTIVE  = RGB(250, 250, 250);
         FONT_COLOR_NORMAL = COLOR_BLACK;
         FONT_COLOR_SEL    = COLOR_WHITE;
         break;
      
      case 3: /* Dark */
         BKG_COLOR_NORMAL  = RGB( 32,  32,  32);
         BKG_COLOR_ALT     = RGB( 70,  70,  70);
         BKG_COLOR_EXTRA   = RGB( 14,  14,  14);
         BKG_COLOR_EXTRA2  = RGB( 50,  50,  50);
         BKG_COLOR_SEL     = RGB(140, 140, 140);
         BKG_COLOR_ACTIVE  = RGB( 16,  16,  16);
         FONT_COLOR_NORMAL = COLOR_WHITE;
         FONT_COLOR_SEL    = COLOR_BLACK;
         break;
      
      case 4: /* Light */
         BKG_COLOR_NORMAL  = RGB(200, 204, 206);
         BKG_COLOR_ALT     = RGB(160, 160, 160);
         BKG_COLOR_EXTRA   = RGB(132, 132, 132);
         BKG_COLOR_EXTRA2  = RGB(180, 180, 180);
         BKG_COLOR_SEL     = RGB( 40,  40,  40);
         BKG_COLOR_ACTIVE  = RGB(250, 250, 250);
         FONT_COLOR_NORMAL = COLOR_BLACK;
         FONT_COLOR_SEL    = COLOR_WHITE;
         break;
   }

   int BKG_PADDING_X_DEFAULT = -3;
   int BKG_PADDING_Y_DEFAULT = -3;

#if defined(__XVIC__)
   XOFFSET  = 4;
   YOFFSET  = 0;
   XPADDING = 108;
   YPADDING = 118;

   if (retro_region == RETRO_REGION_NTSC)
   {
      XOFFSET  = 12;
      YOFFSET  = -1;
      XPADDING = 60;
      YPADDING = 68;
   }
#elif defined(__XPLUS4__)
   XOFFSET  = 4;
   YOFFSET  = -2;
   XPADDING = 66;
   YPADDING = 108;

   if (retro_region == RETRO_REGION_NTSC)
   {
      YPADDING = 64;
   }
#else
   XOFFSET  = 0;
   YOFFSET  = 1;
   XPADDING = 74;
   YPADDING = 92;

   if (retro_region == RETRO_REGION_NTSC)
   {
      YOFFSET  = -1;
      YPADDING = 72;
   }
#endif

   int XSIDE = (retrow - XPADDING) / VKBDX;
   int YSIDE = (retroh - YPADDING) / VKBDY;

   int XBASEKEY = (XPADDING > 0) ? (XPADDING / 2) : 0;
   int YBASEKEY = (YPADDING > 0) ? (YPADDING / 2) : 0;

   int XBASETEXT = XBASEKEY + (XSIDE / 2);
   int YBASETEXT = YBASEKEY + (YSIDE / 2);

   /* Coordinates */
   vkbd_x_min = XOFFSET + XBASEKEY + XKEYSPACING;
   vkbd_x_max = XOFFSET - XBASEKEY - XKEYSPACING + retrow;
   vkbd_y_min = YOFFSET + YBASEKEY + YKEYSPACING;
   vkbd_y_max = YOFFSET + YBASEKEY + (YSIDE * VKBDY);

   /* Opacity */
   BKG_ALPHA  = (retro_vkbd_transparent) ? ALPHA : GRAPH_ALPHA_100;

   /* Alternate color keys */
   int alt_keys[] =
   {
      RETROK_F1, RETROK_F3, RETROK_F5, RETROK_F7,
#ifdef __XPLUS4__
      RETROK_F2, RETROK_F8
#endif
   };
   int alt_keys_len = sizeof(alt_keys) / sizeof(alt_keys[0]);

   /* Extra color keys */
   int extra_keys[] =
   {
      -3 /* STB */ , -4 /* JOY/ASR */, -5 /* TRF/ZOM */
   };
   int extra_keys_len = sizeof(extra_keys) / sizeof(extra_keys[0]);

   /* Datasette color keys */
   int datasette_keys[] =
   {
      -11, -12, -13, -14, -15
   };
   int datasette_keys_len = sizeof(datasette_keys) / sizeof(datasette_keys[0]);

   /* Key label shifted */
   shifted = false;
   if (retro_capslock || vkey_sticky1 == RETROK_LSHIFT || vkey_sticky2 == RETROK_LSHIFT ||
                         vkey_sticky1 == RETROK_RSHIFT || vkey_sticky2 == RETROK_RSHIFT)
      shifted = true;
   if (vkflag[RETRO_DEVICE_ID_JOYPAD_B] && (vkey_pressed == RETROK_LSHIFT || vkey_pressed == RETROK_RSHIFT))
      shifted = true;

   /* Key layout */
   for (x = 0; x < VKBDX; x++)
   {
      for (y = 0; y < VKBDY; y++)
      {
         /* Skip selected key */
         if (((vkey_pos_y * VKBDX) + vkey_pos_x + page) == ((y * VKBDX) + x + page))
            continue;

         /* Default key color */
         BKG_COLOR = BKG_COLOR_NORMAL;
         BKG_ALPHA = (retro_vkbd_transparent) ? ALPHA : GRAPH_ALPHA_100;

         /* Reset key color */
         if (vkeys[(y * VKBDX) + x].value == -2)
            BKG_COLOR = RGB(128, 0, 0);
         else
         {
            /* Alternate key color */
            for (int alt_key = 0; alt_key < alt_keys_len; ++alt_key)
                if (alt_keys[alt_key] == vkeys[(y * VKBDX) + x + page].value)
                    BKG_COLOR = BKG_COLOR_ALT;

            /* Extra key color */
            for (int extra_key = 0; extra_key < extra_keys_len; ++extra_key)
                if (extra_keys[extra_key] == vkeys[(y * VKBDX) + x + page].value)
                    BKG_COLOR = BKG_COLOR_EXTRA;

            /* Datasette key color */
            for (int datasette_key = 0; datasette_key < datasette_keys_len; ++datasette_key)
                if (datasette_keys[datasette_key] == vkeys[(y * VKBDX) + x + page].value)
                    BKG_COLOR = BKG_COLOR_EXTRA2;
         }

         /* Key centering */
         BKG_PADDING_X = BKG_PADDING_X_DEFAULT;
         BKG_PADDING_Y = BKG_PADDING_Y_DEFAULT;
         if (!shifted && strlen(vkeys[(y * VKBDX) + x + page].normal) > 1)
            BKG_PADDING_X = BKG_PADDING(strlen(vkeys[(y * VKBDX) + x + page].normal));
         else if (shifted && strlen(vkeys[(y * VKBDX) + x + page].shift) > 1)
            BKG_PADDING_X = BKG_PADDING(strlen(vkeys[(y * VKBDX) + x + page].shift));

         /* Key positions */
         XKEY  = XOFFSET + XBASEKEY + (x * XSIDE);
         XTEXT = XOFFSET + XBASETEXT + BKG_PADDING_X + (x * XSIDE);
         YKEY  = YOFFSET + YBASEKEY + (y * YSIDE);
         YTEXT = YOFFSET + YBASETEXT + BKG_PADDING_Y + (y * YSIDE);

         /* Default font color */
         FONT_COLOR = FONT_COLOR_NORMAL;

         /* Sticky + CapsLock + pressed colors */
         if ( (vkey_sticky1 == vkeys[(y * VKBDX) + x + page].value
          ||   vkey_sticky2 == vkeys[(y * VKBDX) + x + page].value
          ||(retro_capslock && vkeys[(y * VKBDX) + x + page].value == -10)
          ||(vkflag[RETRO_DEVICE_ID_JOYPAD_START] && vkeys[(y * VKBDX) + x + page].value == RETROK_RETURN))
          && BKG_COLOR != BKG_COLOR_EXTRA && vkeys[(y * VKBDX) + x + page].value != -2)
         {
            FONT_COLOR = FONT_COLOR_NORMAL;
            BKG_COLOR  = BKG_COLOR_ACTIVE;

            switch (BKG_ALPHA)
            {
               case GRAPH_ALPHA_0:
               case GRAPH_ALPHA_25:
               case GRAPH_ALPHA_50:
                  BKG_ALPHA = GRAPH_ALPHA_75;
                  break;
               case GRAPH_ALPHA_75:
               case GRAPH_ALPHA_100:
               default:
                  /* Do nothing */
                  break;
            }
         }

         /* Key background */
         draw_fbox(XKEY+XKEYSPACING, YKEY+YKEYSPACING, XSIDE-XKEYSPACING, YSIDE-YKEYSPACING,
                   BKG_COLOR, BKG_ALPHA);

         /* Key text */
         draw_text(XTEXT, YTEXT, FONT_COLOR, BKG_COLOR, GRAPH_ALPHA_100,
                   (text_outline) ? GRAPH_BG_OUTLINE : GRAPH_BG_SHADOW, FONT_WIDTH, FONT_HEIGHT, FONT_MAX,
                   (!shifted) ? vkeys[(y * VKBDX) + x + page].normal : vkeys[(y * VKBDX) + x + page].shift);
      }
   }

   /* Key centering */
   BKG_PADDING_X = BKG_PADDING_X_DEFAULT;
   BKG_PADDING_Y = BKG_PADDING_Y_DEFAULT;
   if (!shifted && strlen(vkeys[(vkey_pos_y * VKBDX) + vkey_pos_x + page].normal) > 1)
      BKG_PADDING_X = BKG_PADDING(strlen(vkeys[(vkey_pos_y * VKBDX) + vkey_pos_x + page].normal));
   else if (shifted && strlen(vkeys[(vkey_pos_y * VKBDX) + vkey_pos_x + page].shift) > 1)
      BKG_PADDING_X = BKG_PADDING(strlen(vkeys[(vkey_pos_y * VKBDX) + vkey_pos_x + page].shift));

   /* Selected key position */
   XKEY  = XOFFSET + XBASEKEY + (vkey_pos_x * XSIDE);
   XTEXT = XOFFSET + XBASETEXT + BKG_PADDING_X + (vkey_pos_x * XSIDE);
   YKEY  = YOFFSET + YBASEKEY + (vkey_pos_y * YSIDE);
   YTEXT = YOFFSET + YBASETEXT + BKG_PADDING_Y + (vkey_pos_y * YSIDE);

   /* Opacity */
   BKG_ALPHA = (retro_vkbd_transparent) ?
         ((BKG_ALPHA == GRAPH_ALPHA_100) ? GRAPH_ALPHA_100 : GRAPH_ALPHA_75) : GRAPH_ALPHA_100;

   /* Pressed key color */
   if (vkflag[RETRO_DEVICE_ID_JOYPAD_B] && (vkeys[(vkey_pos_y * VKBDX) + vkey_pos_x + page].value == vkey_sticky1 ||
                                            vkeys[(vkey_pos_y * VKBDX) + vkey_pos_x + page].value == vkey_sticky2))
      ; /* no-op */
   else if (vkflag[RETRO_DEVICE_ID_JOYPAD_B])
      BKG_COLOR_SEL = BKG_COLOR_ACTIVE;
   else
      FONT_COLOR = FONT_COLOR_SEL;

   /* Selected key background */
   draw_fbox(XKEY+XKEYSPACING, YKEY+YKEYSPACING, XSIDE-XKEYSPACING, YSIDE-YKEYSPACING,
             BKG_COLOR_SEL, BKG_ALPHA);

   /* Selected key text */
   draw_text(XTEXT, YTEXT, FONT_COLOR, 0, GRAPH_ALPHA_100,
             GRAPH_BG_NONE, FONT_WIDTH, FONT_HEIGHT, FONT_MAX,
             (!shifted) ? vkeys[(vkey_pos_y * VKBDX) + vkey_pos_x + page].normal
                        : vkeys[(vkey_pos_y * VKBDX) + vkey_pos_x + page].shift);

#ifdef POINTER_DEBUG
   draw_hline(pointer_x, pointer_y, 1, 1, RGB565(255, 0, 255));
#endif
}

int check_vkey(int x, int y)
{
   /* Check which key is pressed */
   int page = 0;
   return vkeys[(y * VKBDX) + x + page].value;
}
