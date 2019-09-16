static void retro_defaultkeyboard(){

	//log_message(keyboard_log, "Loading default retro keymap:");

    if (keyconvmap != NULL) {
        keyboard_keyconvmap_free();
    }

    keyboard_keyconvmap_alloc();

	/*
	!CLEAR
	!LSHIFT 1 7
	!RSHIFT 6 4
	!VSHIFT RSHIFT
	!SHIFTL LSHIFT
	*/

        keyboard_keyword_clear();

	kbd_lshiftrow = 1;
	kbd_lshiftcol = 7;
	kbd_rshiftrow = 6;
	kbd_rshiftcol = 4;
	vshift=KEY_RSHIFT;
	shiftl=KEY_LSHIFT;

#if defined(__VIC20__)
keyboard_parse_set_pos_row(27, 0, 3, 8);                /*          ESC -> Run/Stop     */
keyboard_parse_set_pos_row(49, 0, 0, 8);                /*            1 -> 1            */
keyboard_parse_set_pos_row(50, 0, 7, 8);                /*            2 -> 2            */
keyboard_parse_set_pos_row(51, 1, 0, 8);                /*            3 -> 3            */
keyboard_parse_set_pos_row(52, 1, 7, 8);                /*            4 -> 4            */
keyboard_parse_set_pos_row(53, 2, 0, 8);                /*            5 -> 5            */
keyboard_parse_set_pos_row(54, 2, 7, 8);                /*            6 -> 6            */
keyboard_parse_set_pos_row(55, 3, 0, 8);                /*            7 -> 7            */
keyboard_parse_set_pos_row(56, 3, 7, 8);                /*            8 -> 8            */
keyboard_parse_set_pos_row(57, 4, 0, 8);                /*            9 -> 9            */
keyboard_parse_set_pos_row(48, 4, 7, 8);                /*            0 -> 0            */
keyboard_parse_set_pos_row(45, 5, 0, 8);                /*        Minus -> Plus         */
keyboard_parse_set_pos_row(61, 5, 7, 8);                /*        Equal -> Minus        */
keyboard_parse_set_pos_row(8, 7, 0, 8);                 /*    Backspace -> Del          */
keyboard_parse_set_pos_row(9, 0, 2, 8);                 /*          TAB -> Ctrl         */
keyboard_parse_set_pos_row(113, 0, 6, 8);               /*            Q -> Q            */
keyboard_parse_set_pos_row(119, 1, 1, 8);               /*            W -> W            */
keyboard_parse_set_pos_row(101, 1, 6, 8);               /*            E -> E            */
keyboard_parse_set_pos_row(114, 2, 1, 8);               /*            R -> R            */
keyboard_parse_set_pos_row(116, 2, 6, 8);               /*            T -> T            */
keyboard_parse_set_pos_row(121, 3, 1, 8);               /*            Y -> Y            */
keyboard_parse_set_pos_row(117, 3, 6, 8);               /*            U -> U            */
keyboard_parse_set_pos_row(105, 4, 1, 8);               /*            I -> I            */
keyboard_parse_set_pos_row(111, 4, 6, 8);               /*            O -> O            */
keyboard_parse_set_pos_row(112, 5, 1, 8);               /*            P -> P            */
keyboard_parse_set_pos_row(91, 5, 6, 8);                /*            [ -> @            */
keyboard_parse_set_pos_row(93, 6, 1, 8);                /*            ] -> *            */
keyboard_parse_set_pos_row(13, 7, 1, 8);                /*       Return -> Return       */
keyboard_parse_set_pos_row(306, 0, 5, 8);               /*    Left Ctrl -> CBM          */ 
keyboard_parse_set_pos_row(97, 1, 2, 8);                /*            A -> A            */
keyboard_parse_set_pos_row(115, 1, 5, 8);               /*            S -> S            */
keyboard_parse_set_pos_row(100, 2, 2, 8);               /*            D -> D            */
keyboard_parse_set_pos_row(102, 2, 5, 8);               /*            F -> F            */
keyboard_parse_set_pos_row(103, 3, 2, 8);               /*            G -> G            */
keyboard_parse_set_pos_row(104, 3, 5, 8);               /*            H -> H            */
keyboard_parse_set_pos_row(106, 4, 2, 8);               /*            J -> J            */
keyboard_parse_set_pos_row(107, 4, 5, 8);               /*            K -> K            */
keyboard_parse_set_pos_row(108, 5, 2, 8);               /*            L -> L            */
keyboard_parse_set_pos_row(59, 5, 5, 8);                /*            ; -> :            */
keyboard_parse_set_pos_row(39, 6, 2, 8);                /*            ' -> ;            */
keyboard_parse_set_pos_row(96, 0, 1, 8);                /*            ` -> Left Arrow   */
keyboard_parse_set_pos_row(92, 6, 5, 8);                /*            \ -> =            */
keyboard_parse_set_pos_row(304, 1, 3, 2);               /*   Left Shift -> Left Shift   */
keyboard_parse_set_pos_row(301, 1, 3, 64);              /*    Caps Lock -> Shift Lock   */
keyboard_parse_set_pos_row(122, 1, 4, 8);               /*            Z -> Z            */
keyboard_parse_set_pos_row(120, 2, 3, 8);               /*            X -> X            */
keyboard_parse_set_pos_row(99, 2, 4, 8);                /*            C -> C            */
keyboard_parse_set_pos_row(118, 3, 3, 8);               /*            V -> V            */
keyboard_parse_set_pos_row(98, 3, 4, 8);                /*            B -> B            */
keyboard_parse_set_pos_row(110, 4, 3, 8);               /*            N -> N            */
keyboard_parse_set_pos_row(109, 4, 4, 8);               /*            M -> M            */
keyboard_parse_set_pos_row(44, 5, 3, 8);                /*            , -> ,            */
keyboard_parse_set_pos_row(46, 5, 4, 8);                /*            . -> .            */
keyboard_parse_set_pos_row(47, 6, 3, 8);                /*            / -> /            */
keyboard_parse_set_pos_row(303, 6, 4, 4);               /*  Right Shift -> Right Shift  */
keyboard_parse_set_pos_row(32, 0, 4, 8);                /*        Space -> Space        */
keyboard_parse_set_pos_row(282, 7, 4, 8);               /*           F1 -> F1           */
keyboard_parse_set_pos_row(283, 7, 4, 1);               /*           F2 -> F2           */
keyboard_parse_set_pos_row(284, 7, 5, 8);               /*           F3 -> F3           */
keyboard_parse_set_pos_row(285, 7, 5, 1);               /*           F4 -> F4           */
keyboard_parse_set_pos_row(286, 7, 6, 8);               /*           F5 -> F5           */
keyboard_parse_set_pos_row(287, 7, 6, 1);               /*           F6 -> F6           */
keyboard_parse_set_pos_row(288, 7, 7, 8);               /*           F7 -> F7           */
keyboard_parse_set_pos_row(289, 7, 7, 1);               /*           F8 -> F8           */
keyboard_parse_set_pos_row(278, 6, 7, 8);               /*         Home -> CLR/HOME     */
keyboard_parse_set_pos_row(273, 7, 3, 1);               /*           Up -> CRSR UP      */
keyboard_parse_set_pos_row(276, 7, 2, 1);               /*         Left -> CRSR LEFT    */
keyboard_parse_set_pos_row(275, 7, 2, 8);               /*        Right -> CRSR RIGHT   */
keyboard_parse_set_pos_row(274, 7, 3, 8);               /*         Down -> CRSR DOWN    */
keyboard_parse_set_pos_row(277, 6, 0, 8);               /*          Ins -> Pound        */
keyboard_parse_set_pos_row(127, 6, 6, 8);               /*          Del -> Up Arrow     */
#elif defined(__CBM2__)
keyboard_parse_set_pos_row(27, 8, 1, 8);         /*          ESC -> ESC          */
keyboard_parse_set_pos_row(49, 9, 1, 8);         /*            1 -> 1            */
keyboard_parse_set_pos_row(50, 10, 1, 8);        /*            2 -> 2            */
keyboard_parse_set_pos_row(51, 11, 1, 8);        /*            3 -> 3            */
keyboard_parse_set_pos_row(52, 12, 1, 8);        /*            4 -> 4            */
keyboard_parse_set_pos_row(53, 13, 1, 8);        /*            5 -> 5            */
keyboard_parse_set_pos_row(54, 13, 2, 8);        /*            6 -> 6            */
keyboard_parse_set_pos_row(55, 14, 1, 8);        /*            7 -> 7            */
keyboard_parse_set_pos_row(56, 15, 1, 8);        /*            8 -> 8            */
keyboard_parse_set_pos_row(57, 0, 1, 8);         /*            9 -> 9            */
keyboard_parse_set_pos_row(48, 1, 1, 8);         /*            0 -> 0            */
keyboard_parse_set_pos_row(45, 1, 2, 8);         /*        Minus -> Minus        */
keyboard_parse_set_pos_row(61, 2, 1, 8);         /*        Equal -> Equal        */
keyboard_parse_set_pos_row(8, 3, 3, 8);          /*    Backspace -> Del          */
keyboard_parse_set_pos_row(9, 8, 2, 8);          /*          TAB -> TAB          */
keyboard_parse_set_pos_row(113, 9, 2, 8);        /*            Q -> Q            */
keyboard_parse_set_pos_row(119, 10, 2, 8);       /*            W -> W            */
keyboard_parse_set_pos_row(101, 11, 2, 8);       /*            E -> E            */
keyboard_parse_set_pos_row(114, 12, 2, 8);       /*            R -> R            */
keyboard_parse_set_pos_row(116, 12, 3, 8);       /*            T -> T            */
keyboard_parse_set_pos_row(121, 13, 3, 8);       /*            Y -> Y            */
keyboard_parse_set_pos_row(117, 14, 2, 8);       /*            U -> U            */
keyboard_parse_set_pos_row(105, 15, 2, 8);       /*            I -> I            */
keyboard_parse_set_pos_row(111, 0, 2, 8);        /*            O -> O            */
keyboard_parse_set_pos_row(112, 1, 3, 8);        /*            P -> P            */
keyboard_parse_set_pos_row(91, 1, 4, 8);         /*            [ -> [            */
keyboard_parse_set_pos_row(93, 2, 3, 8);         /*            ] -> ]            */
keyboard_parse_set_pos_row(13, 2, 4, 8);         /*       Return -> Return       */
keyboard_parse_set_pos_row(306, 8, 5, 8);        /*    Left Ctrl -> CTRL         */
keyboard_parse_set_pos_row(97, 9, 3, 8);         /*            A -> A            */
keyboard_parse_set_pos_row(115, 10, 3, 8);       /*            S -> S            */
keyboard_parse_set_pos_row(100, 11, 3, 8);       /*            D -> D            */
keyboard_parse_set_pos_row(102, 11, 4, 8);       /*            F -> F            */
keyboard_parse_set_pos_row(103, 12, 4, 8);       /*            G -> G            */
keyboard_parse_set_pos_row(104, 13, 4, 8);       /*            H -> H            */
keyboard_parse_set_pos_row(106, 14, 3, 8);       /*            J -> J            */
keyboard_parse_set_pos_row(107, 15, 3, 8);       /*            K -> K            */
keyboard_parse_set_pos_row(108, 0, 3, 8);        /*            L -> L            */
keyboard_parse_set_pos_row(59, 0, 4, 8);         /*            ; -> ;            */
keyboard_parse_set_pos_row(39, 1, 5, 8);         /*            ' -> '            */
keyboard_parse_set_pos_row(304, 8, 4, 2);        /*   Left Shift -> Left Shift   */
keyboard_parse_set_pos_row(122, 9, 4, 8);        /*            Z -> Z            */
keyboard_parse_set_pos_row(120, 10, 4, 8);       /*            X -> X            */
keyboard_parse_set_pos_row(99, 10, 5, 8);        /*            C -> C            */
keyboard_parse_set_pos_row(118, 11, 5, 8);       /*            V -> V            */
keyboard_parse_set_pos_row(98, 12, 5, 8);        /*            B -> B            */
keyboard_parse_set_pos_row(110, 13, 5, 8);       /*            N -> N            */
keyboard_parse_set_pos_row(109, 14, 4, 8);       /*            M -> M            */
keyboard_parse_set_pos_row(44, 15, 4, 8);        /*            , -> ,            */
keyboard_parse_set_pos_row(46, 15, 5, 8);        /*            . -> .            */
keyboard_parse_set_pos_row(47, 0, 5, 8);         /*            / -> /            */
keyboard_parse_set_pos_row(303, 8, 4, 2);        /*  Right Shift -> Right Shift  */
keyboard_parse_set_pos_row(32, 14, 5, 8);        /*        Space -> Space        */
keyboard_parse_set_pos_row(282, 8, 0, 8);        /*           F1 -> F1           */
keyboard_parse_set_pos_row(283, 9, 0, 8);        /*           F2 -> F2           */
keyboard_parse_set_pos_row(284, 10, 0, 8);       /*           F3 -> F3           */
keyboard_parse_set_pos_row(285, 11, 0, 8);       /*           F4 -> F4           */
keyboard_parse_set_pos_row(286, 12, 0, 8);       /*           F5 -> F5           */
keyboard_parse_set_pos_row(287, 13, 0, 8);       /*           F6 -> F6           */
keyboard_parse_set_pos_row(288, 14, 0, 8);       /*           F7 -> F7           */
keyboard_parse_set_pos_row(289, 15, 0, 8);       /*           F8 -> F8           */
keyboard_parse_set_pos_row(290, 0, 0, 8);        /*           F9 -> F9           */
keyboard_parse_set_pos_row(291, 1, 0, 8);        /*          F10 -> F10          */
keyboard_parse_set_pos_row(278, 4, 0, 8);        /*         Home -> CLR/HOME     */
keyboard_parse_set_pos_row(273, 3, 0, 8);        /*           Up -> CRSR UP      */
keyboard_parse_set_pos_row(276, 3, 1, 8);        /*         Left -> CRSR LEFT    */
keyboard_parse_set_pos_row(275, 3, 2, 8);        /*        Right -> CRSR RIGHT   */
keyboard_parse_set_pos_row(274, 2, 0, 8);        /*         Down -> CRSR DOWN    */
keyboard_parse_set_pos_row(277, 2, 2, 8);        /*          Ins -> Pound        */
keyboard_parse_set_pos_row(127, 4, 1, 8);        /*          Del -> ?            */
keyboard_parse_set_pos_row(281, 6, 0, 8);        /*       PgDown -> Norm/Graph   */
keyboard_parse_set_pos_row(19, 2, 5, 8);         /*  Pause/Break -> Pi           */
keyboard_parse_set_pos_row(280, 5, 0, 8);        /*         PgUp -> Rev/Off      */
keyboard_parse_set_pos_row(279, 5, 1, 8);        /*          End -> CE           */
keyboard_parse_set_pos_row(305, 3, 4, 8);        /*   Right Ctrl -> C=           */
keyboard_parse_set_pos_row(271, 7, 4, 8);        /* Numpad Enter -> Numpad Enter */
keyboard_parse_set_pos_row(267, 7, 1, 8);        /*     Numpad / -> Numpad /     */
keyboard_parse_set_pos_row(263, 4, 2, 8);        /*     Numpad 7 -> Numpad 7     */
keyboard_parse_set_pos_row(264, 5, 2, 8);        /*     Numpad 8 -> Numpad 8     */
keyboard_parse_set_pos_row(265, 6, 2, 8);        /*     Numpad 9 -> Numpad 9     */
keyboard_parse_set_pos_row(269, 7, 2, 8);        /*     Numpad - -> Numpad -     */
keyboard_parse_set_pos_row(260, 4, 3, 8);        /*     Numpad 4 -> Numpad 4     */
keyboard_parse_set_pos_row(261, 5, 3, 8);        /*     Numpad 5 -> Numpad 5     */
keyboard_parse_set_pos_row(262, 6, 3, 8);        /*     Numpad 6 -> Numpad 6     */
keyboard_parse_set_pos_row(270, 7, 3, 8);        /*     Numpad + -> Numpad +     */
keyboard_parse_set_pos_row(257, 4, 4, 8);        /*     Numpad 1 -> Numpad 1     */
keyboard_parse_set_pos_row(258, 5, 4, 8);        /*     Numpad 2 -> Numpad 2     */
keyboard_parse_set_pos_row(259, 6, 4, 8);        /*     Numpad 3 -> Numpad 3     */
keyboard_parse_set_pos_row(256, 4, 5, 8);        /*     Numpad 0 -> Numpad 0     */
keyboard_parse_set_pos_row(266, 5, 5, 8);        /*     Numpad . -> Numpad .     */
#else
keyboard_parse_set_pos_row(27, 7, 7, 8);                /*          ESC -> Run/Stop     */
keyboard_parse_set_pos_row(49, 7, 0, 8);                /*            1 -> 1            */
keyboard_parse_set_pos_row(50, 7, 3, 8);                /*            2 -> 2            */
keyboard_parse_set_pos_row(51, 1, 0, 8);                /*            3 -> 3            */
keyboard_parse_set_pos_row(52, 1, 3, 8);                /*            4 -> 4            */
keyboard_parse_set_pos_row(53, 2, 0, 8);                /*            5 -> 5            */
keyboard_parse_set_pos_row(54, 2, 3, 8);                /*            6 -> 6            */
keyboard_parse_set_pos_row(55, 3, 0, 8);                /*            7 -> 7            */
keyboard_parse_set_pos_row(56, 3, 3, 8);                /*            8 -> 8            */
keyboard_parse_set_pos_row(57, 4, 0, 8);                /*            9 -> 9            */
keyboard_parse_set_pos_row(48, 4, 3, 8);                /*            0 -> 0            */
keyboard_parse_set_pos_row(45, 5, 0, 8);                /*        Minus -> Plus         */
keyboard_parse_set_pos_row(61, 5, 3, 8);                /*        Equal -> Minus        */
keyboard_parse_set_pos_row(8, 0, 0, 8);                 /*    Backspace -> Del          */
keyboard_parse_set_pos_row(9, 7, 2, 8);                 /*          TAB -> Ctrl         */
keyboard_parse_set_pos_row(113, 7, 6, 8);               /*            Q -> Q            */
keyboard_parse_set_pos_row(119, 1, 1, 8);               /*            W -> W            */
keyboard_parse_set_pos_row(101, 1, 6, 8);               /*            E -> E            */
keyboard_parse_set_pos_row(114, 2, 1, 8);               /*            R -> R            */
keyboard_parse_set_pos_row(116, 2, 6, 8);               /*            T -> T            */
keyboard_parse_set_pos_row(121, 3, 1, 8);               /*            Y -> Y            */
keyboard_parse_set_pos_row(117, 3, 6, 8);               /*            U -> U            */
keyboard_parse_set_pos_row(105, 4, 1, 8);               /*            I -> I            */
keyboard_parse_set_pos_row(111, 4, 6, 8);               /*            O -> O            */
keyboard_parse_set_pos_row(112, 5, 1, 8);               /*            P -> P            */
keyboard_parse_set_pos_row(91, 5, 6, 8);                /*            [ -> @            */
keyboard_parse_set_pos_row(93, 6, 1, 8);                /*            ] -> *            */
keyboard_parse_set_pos_row(13, 0, 1, 8);                /*       Return -> Return       */
keyboard_parse_set_pos_row(306, 7, 5, 8);               /*    Left Ctrl -> CBM          */ 
keyboard_parse_set_pos_row(97, 1, 2, 8);                /*            A -> A            */
keyboard_parse_set_pos_row(115, 1, 5, 8);               /*            S -> S            */
keyboard_parse_set_pos_row(100, 2, 2, 8);               /*            D -> D            */
keyboard_parse_set_pos_row(102, 2, 5, 8);               /*            F -> F            */
keyboard_parse_set_pos_row(103, 3, 2, 8);               /*            G -> G            */
keyboard_parse_set_pos_row(104, 3, 5, 8);               /*            H -> H            */
keyboard_parse_set_pos_row(106, 4, 2, 8);               /*            J -> J            */
keyboard_parse_set_pos_row(107, 4, 5, 8);               /*            K -> K            */
keyboard_parse_set_pos_row(108, 5, 2, 8);               /*            L -> L            */
keyboard_parse_set_pos_row(59, 5, 5, 8);                /*            ; -> :            */
keyboard_parse_set_pos_row(39, 6, 2, 8);                /*            ' -> ;            */
keyboard_parse_set_pos_row(96, 7, 1, 8);                /*            ` -> Left Arrow   */
keyboard_parse_set_pos_row(92, 6, 5, 8);                /*            \ -> =            */
keyboard_parse_set_pos_row(304, 1, 7, 2);               /*   Left Shift -> Left Shift   */
keyboard_parse_set_pos_row(301, 1, 7, 64);              /*    Caps Lock -> Shift Lock   */
keyboard_parse_set_pos_row(122, 1, 4, 8);               /*            Z -> Z            */
keyboard_parse_set_pos_row(120, 2, 7, 8);               /*            X -> X            */
keyboard_parse_set_pos_row(99, 2, 4, 8);                /*            C -> C            */
keyboard_parse_set_pos_row(118, 3, 7, 8);               /*            V -> V            */
keyboard_parse_set_pos_row(98, 3, 4, 8);                /*            B -> B            */
keyboard_parse_set_pos_row(110, 4, 7, 8);               /*            N -> N            */
keyboard_parse_set_pos_row(109, 4, 4, 8);               /*            M -> M            */
keyboard_parse_set_pos_row(44, 5, 7, 8);                /*            , -> ,            */
keyboard_parse_set_pos_row(46, 5, 4, 8);                /*            . -> .            */
keyboard_parse_set_pos_row(47, 6, 7, 8);                /*            / -> /            */
keyboard_parse_set_pos_row(303, 6, 4, 4);               /*  Right Shift -> Right Shift  */
keyboard_parse_set_pos_row(32, 7, 4, 8);                /*        Space -> Space        */
keyboard_parse_set_pos_row(282, 0, 4, 8);               /*           F1 -> F1           */
keyboard_parse_set_pos_row(283, 0, 4, 1);               /*           F2 -> F2           */
keyboard_parse_set_pos_row(284, 0, 5, 8);               /*           F3 -> F3           */
keyboard_parse_set_pos_row(285, 0, 5, 1);               /*           F4 -> F4           */
keyboard_parse_set_pos_row(286, 0, 6, 8);               /*           F5 -> F5           */
keyboard_parse_set_pos_row(287, 0, 6, 1);               /*           F6 -> F6           */
keyboard_parse_set_pos_row(288, 0, 3, 8);               /*           F7 -> F7           */
keyboard_parse_set_pos_row(289, 0, 3, 1);               /*           F8 -> F8           */
keyboard_parse_set_pos_row(278, 6, 3, 8);               /*         Home -> CLR/HOME     */
keyboard_parse_set_pos_row(273, 0, 7, 1);               /*           Up -> CRSR UP      */
keyboard_parse_set_pos_row(276, 0, 2, 1);               /*         Left -> CRSR LEFT    */
keyboard_parse_set_pos_row(275, 0, 2, 8);               /*        Right -> CRSR RIGHT   */
keyboard_parse_set_pos_row(274, 0, 7, 8);               /*         Down -> CRSR DOWN    */
keyboard_parse_set_pos_row(277, 6, 0, 8);               /*          Ins -> Pound        */
keyboard_parse_set_pos_row(127, 6, 6, 8);               /*          Del -> Up Arrow     */
keyboard_parse_set_neg_row(280, -3, 0);			/*	PageUp -> Restore       */

//# Joyport attached keypad key mappings
keyboard_parse_set_neg_row(300, -5, 0);               /*      NumLock -> keypad x0    */
keyboard_parse_set_neg_row(263, -5, 1);               /*     NumPad 7 -> keypad 7     */
keyboard_parse_set_neg_row(264, -5, 2);               /*     NumPad 8 -> keypad 8     */
keyboard_parse_set_neg_row(265, -5, 3);               /*     NumPad 9 -> keypad 9     */
keyboard_parse_set_neg_row(267, -5, 4);               /*     NumPad / -> keypad /     */
keyboard_parse_set_neg_row(302, -5, 5);               /*   ScrollLock -> keypad x1    */
keyboard_parse_set_neg_row(260, -5, 6);               /*     NumPad 4 -> keypad 4     */
keyboard_parse_set_neg_row(261, -5, 7);               /*     NumPad 5 -> keypad 5     */
keyboard_parse_set_neg_row(262, -5, 8);               /*     NumPad 6 -> keypad 6     */
keyboard_parse_set_neg_row(268, -5, 9);               /*     NumPad * -> keypad *     */
keyboard_parse_set_neg_row(317, -5, 10);              /*       SysReq -> keypad x2    */
keyboard_parse_set_neg_row(257, -5, 11);              /*     NumPad 1 -> keypad 1     */
keyboard_parse_set_neg_row(258, -5, 12);              /*     NumPad 2 -> keypad 2     */
keyboard_parse_set_neg_row(259, -5, 13);              /*     NumPad 3 -> keypad 3     */
keyboard_parse_set_neg_row(269, -5, 14);              /*     NumPad - -> keypad -     */
keyboard_parse_set_neg_row(316, -5, 15);              /*       PrtScr -> keypad x3    */
keyboard_parse_set_neg_row(256, -5, 16);              /*     NumPad 0 -> keypad 0     */
keyboard_parse_set_neg_row(266, -5, 17);              /*     NumPad , -> keypad .     */
keyboard_parse_set_neg_row(271, -5, 18);              /* NumPad Enter -> keypad enter */
keyboard_parse_set_neg_row(270, -5, 19);              /*     NumPad + -> keypad +     */
#endif

	//log_message(keyboard_log, "Done!");
}

