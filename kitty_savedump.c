static char SaveKeyPressed[4096] = "" ;
void WriteCountUpAndPath(void) ;


// Buffer contenant du texte a ecrire au besoin dans le fichier kitty.dmp
static char * DebugText = NULL ;

void set_debug_text( const char * txt ) {
	if( DebugText!=NULL ) { free( DebugText ) ; DebugText = NULL ; }
	if( txt != NULL ) {
		DebugText = (char*) malloc( strlen(txt)+1 ) ;
		strcpy( DebugText, txt ) ;
		}
	}

void addkeypressed( UINT message, WPARAM wParam, LPARAM lParam, int shift_flag, int control_flag, int alt_flag, int altgr_flag, int win_flag ) {
	char buffer[256], c=' ' ;
	int p ;
	
	if( message==WM_KEYDOWN ) c='v' ; else if( message==WM_KEYUP ) c='^' ;
	
	if( shift_flag ) shift_flag = 1 ;
	if( control_flag ) control_flag = 1 ;
	if( alt_flag ) alt_flag = 1 ;
	if( altgr_flag ) altgr_flag = 1 ;
	if( win_flag ) win_flag = 1 ;
	
	if( wParam=='\r' ) 
		sprintf( buffer, "%d%d%d%d%d %d%c %03d(%02X)/%d (\\r)\n",shift_flag,control_flag,alt_flag,altgr_flag,win_flag, message,c, wParam, wParam, 0 ) ;
	else if( wParam=='\n' )
 		sprintf( buffer, "%d%d%d%d%d %d%c %03d(%02X)/%d (\\n)\n",shift_flag,control_flag,alt_flag,altgr_flag,win_flag, message,c, wParam, wParam, 0 ) ;
	else if( (wParam>=32) && (wParam<=111 ) ) 
		sprintf( buffer, "%d%d%d%d%d %d%c %03d(%02X)/%d (%c)\n",shift_flag,control_flag,alt_flag,altgr_flag,win_flag, message,c, wParam, wParam, 0, wParam ) ;
	else if( (wParam>=VK_F1 /*70 112*/) && (wParam<=VK_F24 /*87 135*/) )
		sprintf( buffer, "%d%d%d%d%d %d%c %03d(%02X)/%d (F%d)\n",shift_flag,control_flag,alt_flag,altgr_flag,win_flag, message,c, wParam, wParam, 0, wParam-VK_F1+1 ) ;
	else
		sprintf( buffer, "%d%d%d%d%d %d%c %03d(%02X)/%d\n",shift_flag,control_flag,alt_flag,altgr_flag,win_flag, message,c, wParam, wParam, 0 ) ;
	
	if( strlen(SaveKeyPressed) > 4000 ) {
		if( (p=poss("\n",SaveKeyPressed)) > 0 ) {
			del( SaveKeyPressed, 1, p );
			}
		}
	strcat( SaveKeyPressed, buffer ) ;
	}

#include <psapi.h>
void PrintProcessNameAndID( DWORD processID, FILE * fp  ) {
	TCHAR szProcessName[MAX_PATH] = TEXT("<unknown>");
	DWORD SizeOfImage = 0 ;
	
	// Get a handle to the process.
	HANDLE hProcess = OpenProcess( PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processID );

	// Get the process name.
	if (NULL != hProcess ) {
		HMODULE hMod;
		DWORD cbNeeded;
		MODULEINFO modinfo ;
		if ( EnumProcessModules( hProcess, &hMod, sizeof(hMod), &cbNeeded) ) { 
			GetModuleBaseName( hProcess, hMod, szProcessName, sizeof(szProcessName)/sizeof(TCHAR) ) ; 
			GetModuleInformation( hProcess, hMod, &modinfo, sizeof( modinfo ) ) ;
			SizeOfImage = modinfo.SizeOfImage ;
			}
		}

	// Print the process name and identifier.
	fprintf( fp, TEXT("%05u %u \t%s\n"), (unsigned int)processID, (unsigned int)SizeOfImage, szProcessName ) ;
	CloseHandle( hProcess );
	}

DWORD PrintAllProcess( FILE * fp ) {
	DWORD aProcesses[1024], cbNeeded, cProcesses;
	unsigned int i;

	if ( !EnumProcesses( aProcesses, sizeof(aProcesses), &cbNeeded ) ) return 0 ;

	// Calculate how many process identifiers were returned.
	cProcesses = cbNeeded / sizeof(DWORD);

	// Print the name and process identifier for each process.
	printf( "ID    MEM   \tMODULE\n" );
	if( cProcesses > 0 )
	for ( i = 0; i < cProcesses; i++ )
		if( aProcesses[i] != 0 )
			PrintProcessNameAndID( aProcesses[i], fp ) ;
	return cProcesses ;
	}

void SaveDumpListFile( FILE * fp, const char * directory ) {
	DIR *dir ;
	struct dirent *de ;
	char buffer[MAX_VALUE_NAME] ;
	
	fprintf( fp, "\n===> Listing directory %s\n", directory ) ;
	if( ( dir=opendir( directory ) ) != NULL ) {
		while( ( de=readdir( dir ) ) != NULL ) {
			if( strcmp(de->d_name,".")&&strcmp(de->d_name,"..") ) {
				sprintf( buffer, "%s\\%s", directory, de->d_name ) ;
				if( GetFileAttributes( buffer ) & FILE_ATTRIBUTE_DIRECTORY ) 
					strcat( buffer, "\\" ) ;
				fprintf( fp, "%s\n", buffer ) ;
				}
			}
		closedir( dir ) ;
		}
	}
	
void SaveDumpListConf( FILE *fp, const char *directory ) {
	char buffer[1025], fullpath[MAX_VALUE_NAME] ;
	FILE *fp2 ;
	DIR *dir ;
	struct dirent *de ;
	if( ( dir=opendir( directory ) ) != NULL ) {
		while( ( de=readdir( dir ) ) != NULL ) {
			if( strcmp(de->d_name,".")&&strcmp(de->d_name,"..") ) {
				sprintf( fullpath, "%s\\%s", directory, de->d_name ) ;
				if( GetFileAttributes( fullpath ) & FILE_ATTRIBUTE_DIRECTORY )
					SaveDumpListConf( fp, fullpath ) ;
				else {
					fprintf( fp, "[%s]\n", fullpath ) ;
					if( ( fp2 = fopen( fullpath, "r" ) ) != NULL ) {
						while( fgets( buffer, 1024, fp2 ) != NULL ) fputs( buffer, fp ) ;
						fclose( fp2 ) ;
						}
					fprintf( fp, "\n\n" ) ;
					}
				}
			}
		closedir( dir ) ;
		}
	}

void SaveDumpClipBoard( FILE *fp ) {
	char *pst = NULL ;
	
	term_copyall(term) ;
	if( OpenClipboard(NULL) ) {
		HGLOBAL hglb ;
		if( (hglb = GetClipboardData( CF_TEXT ) ) != NULL ) {
			if( ( pst = GlobalLock( hglb ) ) != NULL ) {
				//fputs( pst, fp ) ;
				fwrite( pst, 1, strlen(pst), fp ) ;
				GlobalUnlock( hglb ) ;
				}
			}
		CloseClipboard();
		}
	}
	
void SaveDumpConfig( FILE *fp, Config cfg ) {
	fprintf( fp, "MASTER_PASSWORD=%s\n", MASTER_PASSWORD );
	/* Basic options */
	cfg.host[511]='\0' ; fprintf( fp, "host=%s\n", cfg.host ) ;
	fprintf( fp
	, "port=%d\nprotocol=%d\naddressfamily=%d\nclose_on_exit=%d\nwarn_on_close=%d\nping_interval=%d\ntcp_nodelay=%d\ntcp_keepalives=%d\n"
	, cfg.port,cfg.protocol,cfg.addressfamily,cfg.close_on_exit,cfg.warn_on_close,cfg.ping_interval,cfg.tcp_nodelay,cfg.tcp_keepalives);
	/* Proxy options */
	cfg.proxy_exclude_list[511]='\0' ; fprintf( fp, "proxy_exclude_list=%s\n", cfg.proxy_exclude_list ) ;
	fprintf( fp,"proxy_dns=%d\neven_proxy_localhost=%d\nproxy_type=%d\n", cfg.proxy_dns,cfg.even_proxy_localhost,cfg.proxy_type);
	cfg.proxy_host[511]='\0' ; fprintf( fp, "proxy_host=%s\n", cfg.proxy_host ) ;
	fprintf( fp, "proxy_port=%d\n", cfg.proxy_port ) ;
	cfg.proxy_username[127]='\0' ; fprintf( fp, "proxy_username=%s\n", cfg.proxy_username ) ;
	cfg.proxy_password[127]='\0' ; fprintf( fp, "proxy_password=%s\n", cfg.proxy_password ) ;
	cfg.proxy_telnet_command[511]='\0' ; fprintf( fp, "proxy_telnet_command=%s\n", cfg.proxy_telnet_command ) ;
	/* PERSOPORT OptionS */
	fprintf( fp, "bcdelay=%g\ninitdelay=%g\n", cfg.bcdelay, cfg.initdelay ) ;
	fprintf( fp, "transparencynumber=%d\nsendtotray=%d\nicone =%d\n", cfg.transparencynumber,cfg.sendtotray,cfg.icone  );
	cfg.folder[127]='\0' ; fprintf( fp, "folder=%s\n", cfg.folder ) ;
	MASKPASS(cfg.password);
	cfg.password[127]='\0' ; fprintf( fp, "password=%s\n", cfg.password ) ;
	MASKPASS(cfg.password);
	cfg.sessionname[127]='\0' ; fprintf( fp, "sessionname=%s\n", cfg.sessionname ) ;
	cfg.antiidle[127]='\0' ; fprintf( fp, "antiidle=%s\n", cfg.antiidle ) ;
	cfg.autocommand[1023]='\0' ; fprintf( fp, "autocommand=%s\n", cfg.autocommand ) ;
	cfg.autocommandout[511]='\0' ; fprintf( fp, "autocommandout=%s\n", cfg.autocommandout ) ;
	//Filename scriptfile ; // ????
	/* SSH options */
	cfg.remote_cmd[511]='\0' ; fprintf( fp, "remote_cmd=%s\n", cfg.remote_cmd ) ;
	//char *remote_cmd_ptr;	       /* might point to a larger command but never for loading/saving */
	//char *remote_cmd_ptr2;	       /* might point to a larger command but never for loading/saving */
	fprintf( fp, "nopty=%d\ncompression=%d\nssh_rekey_time=%d\n", cfg.nopty,cfg.compression,cfg.ssh_rekey_time ) ;
	//int ssh_kexlist[KEX_MAX];
	cfg.ssh_rekey_data[15]='\0' ; fprintf( fp, "ssh_rekey_data=%s\n", cfg.ssh_rekey_data ) ;
	fprintf( fp, "tryagent=%d\nagentfwd=%d\nchange_username=%d\n", cfg.tryagent, cfg.agentfwd,cfg.change_username);
	//int ssh_cipherlist[CIPHER_MAX];
	//Filename keyfile;
	fprintf( fp
	, "sshprot=%d\nssh2_des_cbc=%d\nssh_no_userauth=%d\ntry_tis_auth=%d\ntry_ki_auth=%d\nssh_subsys=%d\nssh_subsys2=%d\nssh_no_shell=%d\nssh_nc_port=%d\n"
	, cfg.sshprot,cfg.ssh2_des_cbc,cfg.ssh_no_userauth,cfg.try_tis_auth,cfg.try_ki_auth,cfg.ssh_subsys,cfg.ssh_subsys2,cfg.ssh_no_shell,cfg.ssh_nc_port );
#ifdef SCPORT
	fprintf( fp, "try_write_syslog=%d\ntry_pkcs11_auth=%d\n", cfg.try_write_syslog, cfg.try_pkcs11_auth );
	cfg.pkcs11_token_label[69]='\0';
	cfg.pkcs11_cert_label[69]='\0';
	fprintf( fp,"pkcs11_token_label=%s\npkcs11_cert_label=%s\n",cfg.pkcs11_token_label,cfg.pkcs11_cert_label);
#endif
	cfg.ssh_nc_host[511]='\0' ; fprintf( fp, "ssh_nc_host=%s\n", cfg.ssh_nc_host ) ;
	/* Telnet options */
	cfg.termtype[31]='\0' ; fprintf( fp, "termtype=%s\n", cfg.termtype ) ;
	cfg.termspeed[31]='\0' ; fprintf( fp, "termspeed=%s\n", cfg.termspeed ) ;
	cfg.ttymodes[767]='\0' ; fprintf( fp, "ttymodes=%s\n", cfg.ttymodes ) ;
	cfg.environmt[1023]='\0' ; fprintf( fp, "environmt=%s\n", cfg.environmt ) ;
	cfg.username[99]='\0' ; fprintf( fp, "username=%s\n", cfg.username ) ;
	cfg.localusername[99]='\0' ; fprintf( fp, "localusername=%s\n", cfg.localusername ) ;
	fprintf( fp, "rfc_environ=%d\npassive_telnet=%d\n", cfg.rfc_environ, cfg.passive_telnet ) ;
	/* Serial port options */
	cfg.serline[255]='\0' ; fprintf( fp, "serline=%s\n", cfg.serline ) ;
	fprintf( fp, "serspeed=%d\nserdatabits=%d\nserstopbits=%d\nserparity=%d\nserflow=%d\n"
	,cfg.serspeed,cfg.serdatabits,cfg.serstopbits,cfg.serparity,cfg.serflow);
#ifdef CYGTERMPORT
	/* Cygterm options */
	cfg.cygcmd[511]='\0' ; fprintf( fp, "cygcmd=%s\n", cfg.cygcmd ) ;
	fprintf( fp, "alt_metabit=%d\n", cfg.alt_metabit ) ;
#endif
	/* Keyboard options */
	fprintf( fp, "bksp_is_delete=%d\nrxvt_homeend=%d\nfunky_type=%d\nno_applic_c=%d\nno_applic_k=%d\nno_mouse_rep=%d\n"
	,cfg.bksp_is_delete,cfg.rxvt_homeend,cfg.funky_type,cfg.no_applic_c,cfg.no_applic_k,cfg.no_mouse_rep );
	fprintf( fp, "no_remote_resize=%d\nno_alt_screen=%d\nno_remote_wintitle=%d\nno_dbackspace=%d\nno_remote_charset=%d\n"
	,cfg.no_remote_resize,cfg.no_alt_screen,cfg.no_remote_wintitle,cfg.no_dbackspace,cfg.no_remote_charset);
	fprintf( fp, "remote_qtitle_action=%d\napp_cursor=%d\napp_keypad=%d\nnethack_keypad=%d\ntelnet_keyboard=%d\n"
	,cfg.remote_qtitle_action,cfg.app_cursor,cfg.app_keypad,cfg.nethack_keypad,cfg.telnet_keyboard);
	fprintf( fp, "telnet_newline=%d\nalt_f4=%d\nalt_space=%d\nalt_only=%d\n",cfg.telnet_newline,cfg.alt_f4,cfg.alt_space,cfg.alt_only);
	fprintf( fp, "localecho=%d\nlocaledit=%d\nalwaysontop=%d\nfullscreenonaltenter=%d\nscroll_on_key=%d\nscroll_on_disp=%d\n"
	,cfg.localecho,cfg.localedit,cfg.alwaysontop,cfg.fullscreenonaltenter,cfg.scroll_on_key,cfg.scroll_on_disp);
	fprintf( fp, "erase_to_scrollback=%d\ncompose_key=%d\nctrlaltkeys=%d\n",cfg.erase_to_scrollback,cfg.compose_key,cfg.ctrlaltkeys);
	cfg.wintitle[255]='\0' ; fprintf( fp, "wintitle=%s\n", cfg.wintitle ) ;
	/* Terminal options */
	fprintf( fp, "savelines=%d\ndec_om=%d\nwrap_mode=%d\nlfhascr=%d\ncursor_type=%d\nblink_cur=%d\nbeep=%d\nbeep_ind=%d\nbellovl=%d\nbellovl_n=%d\n"
	,cfg.savelines,cfg.dec_om,cfg.wrap_mode,cfg.lfhascr,cfg.cursor_type,cfg.blink_cur,cfg.beep,cfg.beep_ind,cfg.bellovl,cfg.bellovl_n);
	fprintf( fp, "bellovl_t=%d\nbellovl_s=%d\nscrollbar=%d\nscrollbar_in_fullscreen=%d\nresize_action=%d\nbce=%d\nblinktext=%d\nwin_name_always=%d\n"
	,cfg.bellovl_t,cfg.bellovl_s,cfg.scrollbar,cfg.scrollbar_in_fullscreen,cfg.resize_action,cfg.bce,cfg.blinktext,cfg.win_name_always);
	fprintf( fp, "width=%d\nheight=%d\nfont_quality=%d\nlogtype=%d\nlogxfovr=%d\nlogflush=%d\nlogomitpass=%d\nlogomitdata=%d\nhide_mouseptr=%d\n"
	,cfg.width,cfg.height,cfg.font_quality,cfg.logtype,cfg.logxfovr,cfg.logflush,cfg.logomitpass,cfg.logomitdata,cfg.hide_mouseptr);
	fprintf( fp, "sunken_edge=%d\nwindow_border=%d\n", cfg.sunken_edge, cfg.window_border);
	fprintf( fp, "saveonexit=%d\nXPos=%d\nYPos=%d\n",cfg.saveonexit,cfg.xpos,cfg.ypos);
	//Filename bell_wavefile;
	//FontSpec font;
	//Filename logfilename;
	/* IMAGEPORT Options */
#if (defined IMAGEPORT) && (!defined FDJ)
	fprintf( fp, "bg_opacity=%d\nbg_slideshow=%d\nbg_type=%d\nbg_image_style=%d\nbg_image_abs_x=%d\nbg_image_abs_y=%d\nbg_image_abs_fixed=%d\n"
	,cfg.bg_opacity,cfg.bg_slideshow,cfg.bg_type,cfg.bg_image_style,cfg.bg_image_abs_x,cfg.bg_image_abs_y,cfg.bg_image_abs_fixed );
#endif
	//Filename bg_image_filename;
	cfg.answerback[255]='\0' ; fprintf( fp, "answerback=%s\n", cfg.answerback ) ;
	cfg.printer[127]='\0' ; fprintf( fp, "printer=%s\n", cfg.printer ) ;
	fprintf( fp, "arabicshaping=%d\nbidi=%d\n", cfg.arabicshaping, cfg.bidi ) ;
	/* Colour options */
	fprintf( fp, "ansi_colour=%d\nxterm_256_colour=%d\nsystem_colour=%d\ntry_palette%d\nbold_colour=%d\n"
	,cfg.ansi_colour,cfg.xterm_256_colour,cfg.system_colour,cfg.try_palette,cfg.bold_colour);
	//unsigned char colours[22][3];
	/* Selection options */
	fprintf( fp, "mouse_is_xterm=%d\nrect_select=%d\nrawcnp=%d\nrtf_paste=%d\nmouse_override=%d\n"
	,cfg.mouse_is_xterm,cfg.rect_select,cfg.rawcnp,cfg.rtf_paste,cfg.mouse_override);
	//short wordness[256];
	/* translations */
	cfg.line_codepage[127]='\0' ; fprintf( fp, "line_codepage=%s\n", cfg.line_codepage ) ;
	fprintf( fp, "vtmode=%d\ncjk_ambig_wide=%d\nutf8_override=%d\nxlat_capslockcyr=%d\n"
	,cfg.vtmode,cfg.cjk_ambig_wide,cfg.utf8_override,cfg.xlat_capslockcyr );
	/* X11 forwarding */
	fprintf( fp, "x11_forward=%d\nx11_auth=%d\n", cfg.x11_forward, cfg.x11_auth );
	cfg.x11_display[127]='\0' ; fprintf( fp, "x11_display=%s\n", cfg.x11_display ) ;
	/* port forwarding */
	fprintf( fp, "lport_acceptall=%d\nrport_acceptall=%d\n", cfg.lport_acceptall,cfg.rport_acceptall ) ;
	cfg.portfwd[1023]='\0' ; fprintf( fp, "portfwd=%s\n", cfg.portfwd ) ;
	/* SSH bug compatibility modes */
	fprintf( fp, "sshbug_ignore1=%d\nsshbug_plainpw1=%d\nsshbug_rsa1=%d\nsshbug_hmac2=%d\nsshbug_derivekey2=%d\nsshbug_rsapad2=%d\nsshbug_pksessid2=%d\nsshbug_rekey2=%d\n"
	,cfg.sshbug_ignore1,cfg.sshbug_plainpw1,cfg.sshbug_rsa1,cfg.sshbug_hmac2,cfg.sshbug_derivekey2,cfg.sshbug_rsapad2,cfg.sshbug_pksessid2,cfg.sshbug_rekey2);
	/* Options for pterm. Should split out into platform-dependent part. */
	fprintf( fp, "stamp_utmp=%d\nlogin_shell=%d\nscrollbar_on_left=%d\nshadowbold=%d\nshadowboldoffset=%d\n"
	,cfg.stamp_utmp,cfg.login_shell,cfg.scrollbar_on_left,cfg.shadowbold,cfg.shadowboldoffset);
#ifdef RECONNECTPORT
	fprintf( fp, "wakeup_reconnect=%d\nfailure_reconnect=%d\n", cfg.wakeup_reconnect,cfg.failure_reconnect );
#endif
#ifdef HYPERLINKPORT
	fprintf( fp, "url_ctrl_click=%d\nurl_underline=%d\nurl_defbrowser=%d\nurl_defregex=%d\nurl_browser=%s\nurl_regex=%s\n", cfg.url_ctrl_click,cfg.url_underline, cfg.url_defbrowser, cfg.url_defregex, cfg.url_browser, cfg.url_regex );
#endif
#ifdef ZMODEMPORT
	fprintf( fp, "rzcommand=%s\nrzoptions=%s\nszcommand=%s\nszoptions=%s\nzdownloaddir=%s\n",cfg.rzcommand,cfg.rzoptions,cfg.szcommand,cfg.szoptions,cfg.zdownloaddir);
#endif
	//FontSpec boldfont; //FontSpec widefont; //FontSpec wideboldfont;

	fprintf( fp, "\ninternal_delay=%d\ninit_delay=%g\nautocommand_delay=%g\nbetween_char_delay=%d\nTransparencyNumber=%d\nProtectFlag=%d\nIniFileFlag=%d\n"
	,internal_delay,init_delay,autocommand_delay,between_char_delay,TransparencyNumber,ProtectFlag,IniFileFlag );
	
	if( AutoCommand!= NULL ) fprintf( fp, "AutoCommand=%s\n", AutoCommand ) ;
	if( ScriptCommand!= NULL ) fprintf( fp, "ScriptCommand=%s\n", ScriptCommand ) ;
	if( PasteCommand!= NULL ) fprintf( fp, "PasteCommand=%s\n", PasteCommand ) ;
	fprintf( fp, "PasteCommandFlag=%d\n", PasteCommandFlag );
	if( ScriptFileContent!= NULL ) {
		char * pst = ScriptFileContent ;
		fprintf( fp, "ScriptFileContent=" ) ;
		while( strlen(pst) > 0 ) { fprintf( fp, "%s|", pst ) ; pst=pst+strlen(pst)+1 ; }
		fprintf( fp, "\n" )  ;
		}
	if( IconFile!= NULL ) fprintf( fp, "IconFile=%s\n", IconFile ) ;
	fprintf( fp, "DirectoryBrowseFlag=%d\nVisibleFlag=%d\nShortcutsFlag=%d\nIconeFlag=%d\nNumberOfIcons=%d\nSizeFlag=%d\nCapsLockFlag=%d\nTitleBarFlag=%d\n"
	,DirectoryBrowseFlag,VisibleFlag,ShortcutsFlag,IconeFlag,NumberOfIcons,SizeFlag,CapsLockFlag,TitleBarFlag);
	//static HINSTANCE hInstIcons =  NULL ;
	fprintf( fp, "WinHeight=%d\nAutoSendToTray=%d\nNoKittyFileFlag=%d\nConfigBoxHeight=%d\nConfigBoxWindowHeight=%d\nPuttyFlag=%d\n",WinHeight,AutoSendToTray,NoKittyFileFlag,ConfigBoxHeight,ConfigBoxWindowHeight,PuttyFlag);
#if (defined IMAGEPORT) && (!defined FDJ)
	fprintf( fp,"BackgroundImageFlag=%d\n",BackgroundImageFlag );
#endif
#ifdef CYGTERMPORT
	fprintf( fp,"CygTermFlag=%d\n",cygterm_get_flag() );
#endif
	if( PasswordConf!= NULL ) fprintf( fp, "PasswordConf=%s\n", PasswordConf ) ;
	fprintf( fp, "SessionFilterFlag=%d\nImageViewerFlag=%d\nImageSlideDelay=%d\nPrintCharSize=%d\nPrintMaxLinePerPage=%d\nPrintMaxCharPerLine=%d\n"
	,SessionFilterFlag,ImageViewerFlag,ImageSlideDelay,PrintCharSize,PrintMaxLinePerPage,PrintMaxCharPerLine);
	fprintf( fp, "AntiIdleCount=%d\nAntiIdleCountMax=%d\nIconeNum=%d\n"
	,AntiIdleCount,AntiIdleCountMax,IconeNum);
	fprintf( fp, "AntiIdleStr=%s\nInitialDirectory=%s\nConfigDirectory=%s\nBuildVersionTime=%s\n",AntiIdleStr,InitialDirectory,ConfigDirectory,BuildVersionTime);
	if( WinSCPPath!= NULL ) fprintf( fp, "WinSCPPath=%s\n", WinSCPPath ) ;
	if( PSCPPath!= NULL ) fprintf( fp, "PSCPPath=%s\n", PSCPPath ) ;
	if( KittyIniFile!= NULL ) fprintf( fp, "KittyIniFile=%s\n", KittyIniFile ) ;
	if( KittySavFile!= NULL ) fprintf( fp, "KittySavFile=%s\n", KittySavFile ) ;
	if( CtHelperPath!= NULL ) fprintf( fp, "CtHelperPath=%s\n", CtHelperPath ) ;
	}

// récupere la configuration des shortcuts
void SaveShortCuts( FILE *fp ) {
	int i ;
	fprintf( fp, "autocommand=%d\n", shortcuts_tab.autocommand ) ;
	fprintf( fp, "command=%d\n", shortcuts_tab.command ) ;
	fprintf( fp, "editor=%d\n", shortcuts_tab.editor ) ;
	fprintf( fp, "getfile=%d\n", shortcuts_tab.getfile ) ;
	fprintf( fp, "imagechange=%d\n", shortcuts_tab.imagechange ) ;
	fprintf( fp, "input=%d\n", shortcuts_tab.input ) ;
	fprintf( fp, "inputm=%d\n", shortcuts_tab.inputm ) ;
	fprintf( fp, "print=%d\n", shortcuts_tab.print ) ;
	fprintf( fp, "printall=%d\n", shortcuts_tab.printall ) ;
	fprintf( fp, "protect=%d\n", shortcuts_tab.protect ) ;
	fprintf( fp, "script=%d\n", shortcuts_tab.script ) ;
	fprintf( fp, "sendfile=%d\n", shortcuts_tab.sendfile ) ;
	fprintf( fp, "rollup=%d\n", shortcuts_tab.rollup ) ;
	fprintf( fp, "tray=%d\n", shortcuts_tab.tray ) ;
	fprintf( fp, "viewer=%d\n", shortcuts_tab.viewer ) ;
	fprintf( fp, "visible=%d\n", shortcuts_tab.visible ) ;
	fprintf( fp, "winscp=%d\n", shortcuts_tab.winscp ) ;
	
	fprintf( fp, "\nNbShortCuts=%d\n", NbShortCuts ) ;
	if( NbShortCuts>0 ) {
		for( i=0 ; i<NbShortCuts ; i++ ) 
			fprintf( fp, "%d=%s|\n",shortcuts_tab2[i].num, shortcuts_tab2[i].st );
		}
	}
	
// Récupere le menu utilisateur
void SaveSpecialMenu( FILE *fp ) {
	int i ;
	for( i=0 ; i<NB_MENU_MAX ; i++ )
		if( SpecialMenu[i]!=NULL ) 
			fprintf( fp, "%d=%s\n", i, SpecialMenu[i] );
	}
	
// récupère toute la configuration en un seul fichier
void SaveDump( void ) {
	FILE * fp, * fpout ;
	char buffer[1025], buffer2[1025] ;
	int i;

	if( IniFileFlag != SAVEMODE_REG ) { WriteCountUpAndPath() ; }
	
	sprintf( buffer, "%s\\%s", InitialDirectory, "kitty.dmp" ) ;
	if( ( fpout = fopen( buffer, "w" ) ) != NULL ) {
		
		fputs( "\n@ InitialDirectoryListing @\n\n", fpout ) ;
		SaveDumpListFile( fpout, InitialDirectory ) ;
		
		fputs( "\n@ KiTTYIniFile @\n\n", fpout ) ;
		if( ( fp = fopen( KittyIniFile, "r" ) ) != NULL ) {
			while( fgets( buffer, 1024, fp ) != NULL ) fputs( buffer, fpout ) ;
			fclose( fp ) ;
			}

		if( RegTestKey( HKEY_CURRENT_USER, TEXT("Software\\SimonTatham\\PuTTY") ) ) {
			fputs( "\n@ PuTTY RegistryBackup @\n\n", fpout ) ;
			SaveRegistryKeyEx( HKEY_CURRENT_USER, TEXT("Software\\SimonTatham\\PuTTY"), KittySavFile ) ;
			if( ( fp = fopen( KittySavFile, "r" ) ) != NULL ) {
				while( fgets( buffer, 1024, fp ) != NULL ) fputs( buffer, fpout ) ;
				fclose( fp ) ;
				}
			unlink( KittySavFile ) ;
			}

		fputs( "\n@ KiTTY RegistryBackup @\n\n", fpout ) ;
		if( (IniFileFlag == SAVEMODE_REG)||(IniFileFlag == SAVEMODE_FILE) ) {
			SaveRegistryKey() ;
			if( ( fp = fopen( KittySavFile, "r" ) ) != NULL ) {
				while( fgets( buffer, 1024, fp ) != NULL ) fputs( buffer, fpout ) ;
				fclose( fp ) ;
				}
			}
		else if( IniFileFlag == SAVEMODE_DIR ) {
			sprintf( buffer, "%s\\Commands", ConfigDirectory ) ; SaveDumpListConf( fpout, buffer ) ;
			sprintf( buffer, "%s\\Folders", ConfigDirectory ) ; SaveDumpListConf( fpout, buffer ) ;
			sprintf( buffer, "%s\\Launcher", ConfigDirectory ) ; SaveDumpListConf( fpout, buffer ) ;
			sprintf( buffer, "%s\\Sessions", ConfigDirectory ) ; SaveDumpListConf( fpout, buffer ) ;
			sprintf( buffer, "%s\\Sessions_Commands", ConfigDirectory ) ; SaveDumpListConf( fpout, buffer ) ;
			sprintf( buffer, "%s\\SshHostKeys", ConfigDirectory ) ; SaveDumpListConf( fpout, buffer ) ;
			}
			
		fputs( "\n@ RunningProcess @\n\n", fpout ) ;
		PrintAllProcess( fpout ) ;
			
		fputs( "\n@ ClipBoardContent @\n\n", fpout ) ;
		SaveDumpClipBoard( fpout ) ;
		
		if( debug_flag ) {
			fputs( "\n@ KeyPressed @\n\n", fpout ) ;
			fprintf( fpout, "%d: WM_KEYDOWN\n%d: WM_SYSKEYDOWN\n%d: WM_KEYUP\n%d: WM_SYSKEYUP\n%d: WM_CHAR\n\n", WM_KEYDOWN,WM_SYSKEYDOWN,WM_KEYUP,WM_SYSKEYUP,WM_CHAR);
			fprintf( fpout, "SHIFT CONTROL ALT ALTGR WIN\n" ) ;
			fprintf( fpout, "%s\n", SaveKeyPressed ) ;
			}
			
		fputs( "\n@ RunningConfig @\n\n", fpout ) ;
		SaveDumpConfig( fpout, cfg ) ;
			
		fputs( "\n@ CurrentEventLog @\n\n", fpout ) ;
		i=0 ; while( print_event_log( fpout, i ) ) { i++ ; }

		if( DebugText!= NULL ) {
			fputs( "\n@ Debug @\n\n", fpout ) ;
			fprintf( fpout, "%s\n",  DebugText ) ;
			}
		
		fputs( "\n@ Shortcuts @\n\n", fpout ) ;
		SaveShortCuts( fpout ) ;
		
		fputs( "\n@ SpecialMenu @\n\n", fpout ) ;
		SaveSpecialMenu( fpout ) ;
		
		fclose( fpout ) ;

		sprintf( buffer, "%s\\%s", InitialDirectory, "kitty.dmp" ) ;
		sprintf( buffer2, "%s\\%s", InitialDirectory, "kitty.dmp.bcr" ) ;
		bcrypt_file_base64( buffer, buffer2, MASTER_PASSWORD, 80 ) ; unlink( buffer ) ; rename( buffer2, buffer ) ;
		}
	}

