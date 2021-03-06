#ifndef LIBRETRO_VKBD_H
#define LIBRETRO_VKBD_H

extern void print_vkbd(void);
extern int check_vkey(int x, int y);

typedef struct
{
   char normal[5];
   char shift[5];
   int value;
} retro_vkeys;

static retro_vkeys vkeys[VKBDX * VKBDY] =
{
   /* 0 */
   { "1"  ,"!"   ,RETROK_1},
   { "2"  ,"\""  ,RETROK_2},
   { "3"  ,"#"   ,RETROK_3},
   { "4"  ,"$"   ,RETROK_4},
   { "5"  ,"%"   ,RETROK_5},
   { "6"  ,"&"   ,RETROK_6},
   { "7"  ,"'"   ,RETROK_7},
   { "8"  ,"("   ,RETROK_8},
   { "9"  ,")"   ,RETROK_9},
#ifdef __XPLUS4__
   { "0"  ,{26}  ,RETROK_0},
#else
   { "0"  ,"0"   ,RETROK_0},
#endif
#ifdef __XPLUS4__
   { "F1" ,"F4"  ,RETROK_F1},
#else
   { "f1" ,"f2"  ,RETROK_F1},
#endif

   /* 11 */
   { "Q"  ,"Q"   ,RETROK_q},
   { "W"  ,"W"   ,RETROK_w},
   { "E"  ,"E"   ,RETROK_e},
   { "R"  ,"R"   ,RETROK_r},
   { "T"  ,"T"   ,RETROK_t},
   { "Y"  ,"Y"   ,RETROK_y},
   { "U"  ,"U"   ,RETROK_u},
   { "I"  ,"I"   ,RETROK_i},
   { "O"  ,"O"   ,RETROK_o},
   { "P"  ,"P"   ,RETROK_p},
#ifdef __XPLUS4__
   { "F2" ,"F5"  ,RETROK_F2},
#else
   { "f3" ,"f4"  ,RETROK_F3},
#endif

   /* 22 */
   { "A"  ,"A"   ,RETROK_a},
   { "S"  ,"S"   ,RETROK_s},
   { "D"  ,"D"   ,RETROK_d},
   { "F"  ,"F"   ,RETROK_f},
   { "G"  ,"G"   ,RETROK_g},
   { "H"  ,"H"   ,RETROK_h},
   { "J"  ,"J"   ,RETROK_j},
   { "K"  ,"K"   ,RETROK_k},
   { "L"  ,"L"   ,RETROK_l},
   { {31} ,{31}  ,RETROK_INSERT}, /* £ */
#ifdef __XPLUS4__
   { "F3" ,"F6"  ,RETROK_F3},
#else
   { "f5" ,"f6"  ,RETROK_F5},
#endif

   /* 33 */
   { "Z"  ,"Z"   ,RETROK_z},
   { "X"  ,"X"   ,RETROK_x},
   { "C"  ,"C"   ,RETROK_c},
   { "V"  ,"V"   ,RETROK_v},
   { "B"  ,"B"   ,RETROK_b},
   { "N"  ,"N"   ,RETROK_n},
   { "M"  ,"M"   ,RETROK_m},
   { ","  ,"<"   ,RETROK_COMMA},
   { "."  ,">"   ,RETROK_PERIOD},
   { "/"  ,"?"   ,RETROK_SLASH},
#ifdef __XPLUS4__
   { "HLP" ,"F7" ,RETROK_F8},
#else
   { "f7" ,"f8"  ,RETROK_F7},
#endif

   /* 44 */
#ifdef __XPLUS4__
   { "ESC","ESC" ,RETROK_BACKQUOTE},
#else
   { {25} ,{25}  ,RETROK_BACKQUOTE}, /* Left arrow */
#endif
   { "CTR","CTR" ,RETROK_TAB},
   { "+"  ,"+"   ,RETROK_MINUS},
   { "-"  ,"-"   ,RETROK_EQUALS},
   { "@"  ,"@"   ,RETROK_LEFTBRACKET},
   { "*"  ,"*"   ,RETROK_RIGHTBRACKET},
   { {26} ,{7}   ,RETROK_DELETE}, /* Up arrow / Pi */
   { ":"  ,"["   ,RETROK_SEMICOLON},
   { ";"  ,"]"   ,RETROK_QUOTE},
   { "="  ,"="   ,RETROK_BACKSLASH},
   { "STB","SVD" ,-3}, /* Statusbar / Save disk */

   /* 55 */
   { {24} ,{24}  ,RETROK_LCTRL},
   { "R/S","R/S" ,RETROK_ESCAPE},
   { "S/L","S/L" ,-10}, /* ShiftLock */
   { "LSH","LSH" ,RETROK_LSHIFT},
   { "RSH","RSH" ,RETROK_RSHIFT},
   { "RST","RST" ,RETROK_PAGEUP},
   { "CLR","CLR" ,RETROK_HOME},
   { "DEL","DEL" ,RETROK_BACKSPACE},
   { {30} ,{30}  ,RETROK_UP},
   { "RET","RET" ,RETROK_RETURN},
   { "JOY","ASR" ,-4}, /* Switch joyport / Toggle aspect ratio */

   /* 66 */
   { {17} ,{17}  ,-2},  /* Reset */
   { {19} ,{19}  ,-15}, /* Datasette RESET */
   { {20} ,{20}  ,-12}, /* Datasette PLAY */
   { {21} ,{21}  ,-14}, /* Datasette RWD */
   { {22} ,{22}  ,-13}, /* Datasette FWD */
   { {23} ,{23}  ,-11}, /* Datasette STOP */
   { {18} ,{18}  ,RETROK_SPACE},
   { {27} ,{27}  ,RETROK_LEFT},
   { {28} ,{28}  ,RETROK_DOWN},
   { {29} ,{29}  ,RETROK_RIGHT},
   { "TRF","ZOM" ,-5}, /* Toggle turbo fire / Toggle zoom mode */
};

#endif /* LIBRETRO_VKBD_H */
