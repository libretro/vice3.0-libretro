

	    #define DEFHSZ 16
	    #define DEFWSZ 64

	    //joystick options
	    static int joy1on = nk_false;
    	    static int joy2on = nk_false;

	    if(cur_port==1){
		joy1on = nk_true;
		joy2on = nk_false;
	    }
	    else if(cur_port==2){
		joy2on = nk_true;
		joy1on = nk_false;
	    }

	    //misc options
	    static int showled = nk_false;
		
	    if (vice_statusbar) { 
		showled = nk_true;
	    }
	    else showled = nk_false;

	    //c64 sid model
	    int sid_item=0;
            static int sidmod=0;
	    sid_engine_model_t **list = sid_get_engine_model_list();

            int sengine, smodel,cursidmod;
            resources_get_int("SidEngine", &sengine);
            resources_get_int("SidModel", &smodel);
	    cursidmod=((sengine << 8) | smodel) ;

	    for (sid_item = 0; list[sid_item]; ++sid_item) {
		int sdata=list[sid_item]->value;
		if(cursidmod==sdata)sidmod=sid_item;
		
	    }
	    sid_mod = malloc( (sid_item)*sizeof(char*) );

	    //c64 VICII palette available
	    static int vicii_extpal = nk_false;//VICIIExternalPalette
	    int cur_palette=0;
	    const char *external_file_name;
	    int numb_pal=0;

	    resources_get_int("VICIIExternalPalette",&tmpval);
	    if(tmpval)vicii_extpal=nk_true;
	    else vicii_extpal =nk_false;

    	    palette_info_t *palettelist = palette_get_info_list();

	    int res=resources_get_string_sprintf("%sPaletteFile", &external_file_name, "VICII");

	    tmpval=0;
    	    while (palettelist->name ) {
        	if (palettelist->chip && !strcmp(palettelist->chip, "VICII")) {	
			if(res!=-1)if(!strcmp( (char*)palettelist->file,external_file_name) )cur_palette=tmpval;
	       		++tmpval;
        	}
        	++palettelist;
    	    }

	    palette_av = malloc( (tmpval)*sizeof(char*) );
	    numb_pal=tmpval;

	    //c64 model option

	    static int current_model = 0;
	    static int selected_model = 0;
	    static int list_model = 0;

#if defined(__VIC20__)
	    current_model = vic20model_get();
	    for(tmpval=0;tmpval< NUMB(c64modint);tmpval++)
		if(c64modint[tmpval]==current_model)break;
	    list_model = tmpval;
#elif defined(__PLUS4__)
	    current_model = plus4model_get();
	    for(tmpval=0;tmpval< NUMB(c64modint);tmpval++)
		if(c64modint[tmpval]==current_model)break;
	    list_model = tmpval;
#elif defined(__X128__)
	    current_model = c128model_get();
	    for(tmpval=0;tmpval< NUMB(c64modint);tmpval++)
		if(c64modint[tmpval]==current_model)break;
	    list_model = tmpval;
#elif defined(__PET__)
	    current_model = petmodel_get();
	    for(tmpval=0;tmpval< NUMB(c64modint);tmpval++)
		if(c64modint[tmpval]==current_model)break;
	    list_model = tmpval;
#else
	    current_model = c64model_get();
	    for(tmpval=0;tmpval< NUMB(c64modint);tmpval++)
		if(c64modint[tmpval]==current_model)break;
	    list_model = tmpval;
#endif
	    //floppy option
	    static int fliplst = nk_false;
	    static int DriveTrueEmu = nk_false;
	    static int current_drvtype = 2;
	    static int old_drvtype = 2;

	    resources_get_int("DriveTrueEmulation",&tmpval);

	    if(tmpval)DriveTrueEmu=nk_true;
	    else DriveTrueEmu=nk_false;

	    // button toggle GUI/EMU
            nk_layout_row_dynamic(ctx, DEFHSZ, 3);
            if (nk_button_label(ctx, "Resume")){
                fprintf(stdout, "quit GUI\n");
		pauseg=0;
	    }
            if (nk_button_label(ctx, "Reset")){
                fprintf(stdout, "quit GUI & reset\n");
		pauseg=0;
		emu_reset();
	    }
            if (nk_button_label(ctx, "Quit")){
                fprintf(stdout, "quit GUI & emu\n");
		pauseg=0;
		want_quit=1;
	    }

	    //joystick options
            nk_layout_row_dynamic(ctx, DEFHSZ, 3);
            nk_checkbox_label(ctx, "Joy1 on", &joy1on);
            nk_checkbox_label(ctx, "Joy2 on", &joy2on);

	    if(joy1on && cur_port!=1){
		cur_port=1;
		joy2on=false;
	    }
	    else if (joy2on && cur_port!=2){
	    	cur_port=2;
		joy1on=false;
   	    }

	    //misc options
            nk_layout_row_dynamic(ctx, DEFHSZ, 1);
            nk_checkbox_label(ctx, "Statusbar", &showled);

	    if(showled){
		if(!vice_statusbar)
			vice_statusbar = 1;
	    }
	    else if(vice_statusbar)
		vice_statusbar = 0;

	    //C64/VIC20 Modele
#if  defined(__VIC20__)

	    nk_layout_row_static(ctx, DEFHSZ, 100, 2);
            nk_label(ctx, "VIC20 model:", NK_TEXT_LEFT);
	    tmpval = nk_combo(ctx, c64mod, LEN(c64mod), list_model, DEFHSZ, nk_vec2(200,200));
	    selected_model=c64modint[tmpval];
            if (selected_model != current_model)
	        vic20model_set(selected_model);
#elif defined(__PLUS4__)
	    nk_layout_row_static(ctx, DEFHSZ, 100, 2);
            nk_label(ctx, "PLUS4 model:", NK_TEXT_LEFT);
	    tmpval = nk_combo(ctx, c64mod, LEN(c64mod), list_model, DEFHSZ, nk_vec2(200,200));
	    selected_model=c64modint[tmpval];
            if (selected_model != current_model)
	        plus4model_set(selected_model);
#elif defined(__X128__)
	    nk_layout_row_static(ctx, DEFHSZ, 100, 2);
            nk_label(ctx, "C128 model:", NK_TEXT_LEFT);
	    tmpval = nk_combo(ctx, c64mod, LEN(c64mod), list_model, DEFHSZ, nk_vec2(200,200));
	    selected_model=c64modint[tmpval];
            if (selected_model != current_model)
	        c128model_set(selected_model);
#elif defined(__PET__)
	    nk_layout_row_static(ctx, DEFHSZ, 100, 2);
            nk_label(ctx, "PET model:", NK_TEXT_LEFT);
	    tmpval = nk_combo(ctx, c64mod, LEN(c64mod), list_model, DEFHSZ, nk_vec2(200,200));
	    selected_model=c64modint[tmpval];
            if (selected_model != current_model)
	        petmodel_set(selected_model);
#else
	    nk_layout_row_static(ctx, DEFHSZ, 100, 2);
            nk_label(ctx, "C64 model:", NK_TEXT_LEFT);
	    tmpval = nk_combo(ctx, c64mod, LEN(c64mod), list_model, DEFHSZ, nk_vec2(200,200));
	    selected_model=c64modint[tmpval];
            if (selected_model != current_model)
	        c64model_set(selected_model);
#endif
	    //c64 sid model

	    for (tmpval = 0; list[tmpval]; ++tmpval) {
		sid_mod[tmpval]=malloc(sizeof(char)*512);
		sprintf(sid_mod[tmpval],"%s",(char*) list[tmpval]->name);
	    }

	    nk_layout_row_static(ctx, DEFHSZ, 100, 2);
            nk_label(ctx, "C64 SID:", NK_TEXT_LEFT);
 	    tmpval = nk_combo(ctx, (const char **)sid_mod, sid_item, sidmod, DEFHSZ, nk_vec2(200,200));

	    if(tmpval!=sidmod){
		int sdata=list[tmpval]->value;
        	sengine = sdata >> 8;
        	smodel = sdata & 0xff;
        	sid_set_engine_model(sengine, smodel);
		printf("sid change to %d/%d\n",sdata,sid_item);
	    }

	    //c64 VICII palette available

            nk_layout_row_dynamic(ctx, DEFHSZ, 1);
            nk_checkbox_label(ctx, "C64 external Palette", &vicii_extpal);

	    resources_get_int("VICIIExternalPalette",&tmpval);

	    if(vicii_extpal){
		if(!tmpval)
		    resources_set_int("VICIIExternalPalette", 1);
	    }	
	    else if(tmpval)
		resources_set_int("VICIIExternalPalette", 0);

	    palettelist = palette_get_info_list();
	    tmpval=0;
    	    while (palettelist->name) {
        	if (palettelist->chip && !strcmp(palettelist->chip, "VICII")) {			
			palette_av[tmpval]=malloc(sizeof(char)*512);
			sprintf(palette_av[tmpval],"%s",(char*) palettelist->file);
	       		++tmpval;
        	}
        	++palettelist;
    	    }

	    nk_layout_row_static(ctx, DEFHSZ, 150, 2);
            nk_label(ctx, "VICII Palette:", NK_TEXT_LEFT);
 	    tmpval = nk_combo(ctx, (const char **)palette_av, numb_pal, cur_palette, DEFHSZ, nk_vec2(200,200));

	    if(tmpval!=cur_palette){
		resources_set_string_sprintf("%sPaletteFile",(char*)palette_av[tmpval],"VICII");
		printf("Palette change to %s /%d\n",(char*)palette_av[tmpval],numb_pal);
	    }

	    //floppy option

	    nk_layout_row_static(ctx, DEFHSZ, 100, 2);
            nk_label(ctx, "Drive8Type:", NK_TEXT_LEFT);
	    current_drvtype = nk_combo(ctx, drivename, LEN(drivename), current_drvtype, DEFHSZ, nk_vec2(200,200));

	    if (old_drvtype != current_drvtype){

		    char str[100];
		    int val;
	      	    snprintf(str, sizeof(str), "%s", drivename[current_drvtype]);
	      	    val = strtoul(str, NULL, 0);
		    set_drive_type(8, val);
		    old_drvtype=current_drvtype;
	    }    
	
            nk_layout_row_dynamic(ctx, DEFHSZ, 1);
            nk_checkbox_label(ctx, "Attach to Fliplist", &fliplst);

            nk_layout_row_dynamic(ctx, DEFHSZ, 1);
            nk_checkbox_label(ctx, "DriveTrueEmulation", &DriveTrueEmu);

	    resources_get_int("DriveTrueEmulation",&tmpval);

	    if(DriveTrueEmu){
		if(!tmpval)
		    resources_set_int("DriveTrueEmulation", 1);
	    }	
	    else if(tmpval)
		resources_set_int("DriveTrueEmulation", 0);

	    int i;

	    for(i=0;i<2;i++)
		if(LOADCONTENT==2 && LDRIVE==(i+8));
		else if( (i==0? DISKA_NAME: DISKB_NAME)!=NULL){
		    sprintf((i==0?DF8NAME:DF9NAME),"%s",(i==0? DISKA_NAME: DISKB_NAME));
		}
		//else sprintf(LCONTENT,"Choose Content\0");
	     
            nk_layout_row_dynamic(ctx, DEFHSZ, 1);
            nk_label(ctx, "DF8:", NK_TEXT_LEFT);
            nk_layout_row_dynamic(ctx, DEFHSZ, 1);

            if (nk_button_label(ctx, DF8NAME)){
                fprintf(stdout, "LOAD DISKA\n");
		LOADCONTENT=1;
		LDRIVE=8;
	    }

            nk_layout_row_dynamic(ctx, DEFHSZ, 1);
            nk_label(ctx, "DF9:", NK_TEXT_LEFT);
            nk_layout_row_dynamic(ctx, DEFHSZ, 1);

            if (nk_button_label(ctx, DF9NAME)){
                fprintf(stdout, "LOAD DISKB\n");
		LOADCONTENT=1;
		LDRIVE=9;
	    }
	    if(LOADCONTENT==2 && strlen(LCONTENT) > 0){

		fprintf(stdout, "LOAD CONTENT DF%d (%s)\n",LDRIVE,LCONTENT);

		sprintf((LDRIVE==8? DISKA_NAME: DISKB_NAME),"%s",LCONTENT);
		LOADCONTENT=-1;

		cartridge_detach_image(-1);
		tape_image_detach(1);

		if(HandleExtension(LCONTENT,"CRT") || HandleExtension(LCONTENT,"crt"))
			cartridge_attach_image(CARTRIDGE_CRT, LCONTENT);
		else {

			if(fliplst){
				file_system_detach_disk(GET_DRIVE(LDRIVE));
				printf("Attach to flip list\n");
				file_system_attach_disk(LDRIVE, LCONTENT);
				fliplist_add_image(LDRIVE);
			}
			else if (LDRIVE==9) {
				file_system_detach_disk(GET_DRIVE(LDRIVE));
				printf("Attach DF9 disk\n");
				file_system_attach_disk(LDRIVE, LCONTENT);
			}
			else {
				printf("autostart\n");
				autostart_autodetect(LCONTENT, NULL, 0, AUTOSTART_MODE_RUN);
			}

		}

	    
	    }
	    else if(LOADCONTENT==2)LOADCONTENT=-1;
	
	   for(tmpval=0;tmpval<numb_pal;tmpval++)free(palette_av[tmpval]);
	   free(palette_av);

	   for(tmpval=0;tmpval<sid_item;tmpval++)free(sid_mod[tmpval]);
	   free(sid_mod);

