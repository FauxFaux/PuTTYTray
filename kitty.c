#define IDM_QUIT 0x0100
#define IDM_VISIBLE   0x0120

#define IDM_PROTECT   0x0210
#define IDM_PRINT   0x0220
#define IDM_TRANSPARUP	0x0230
#define IDM_TRANSPARDOWN	0x0240
#define IDM_WINROL   0x0250
#define IDM_PSCP	0x0260
#define IDM_WINSCP	0x0270
#define IDM_TOTRAY   0x0280
#define IDM_FROMTRAY   0x0290
#define IDM_SHOWPORTFWD	0x0300
#define IDM_HIDE	0x0310
#define IDM_UNHIDE	0x0320
#define IDM_SWITCH_HIDE 0x0330
#define IDM_GONEXT	0x0340
#define IDM_GOPREVIOUS	0x0350
#define IDM_SCRIPTFILE 0x0360
#define IDM_RESIZE 0x0370
#define IDM_REPOS 0x0380


#ifndef SAVEMODE_REG
#define SAVEMODE_REG 0
#endif
#ifndef SAVEMODE_FILE
#define SAVEMODE_FILE 1
#endif
#ifndef SAVEMODE_DIR
#define SAVEMODE_DIR 2
#endif

// Flag pour le fonctionnement en mode "portable" (gestion par fichiers)
#ifdef PORTABLE
int IniFileFlag = SAVEMODE_DIR ;
#else
int IniFileFlag = SAVEMODE_REG ;
#endif

// Flag permettant la gestion de l'arborscence (dossier=folder) dans le cas d'un savemode=dir
#ifdef PORTABLE
int DirectoryBrowseFlag = 1 ;
#else
int DirectoryBrowseFlag = 0 ;
#endif


#ifdef ZMODEMPORT
#define IDM_XYZSTART  0x0810
#define IDM_XYZUPLOAD 0x0820
#define IDM_XYZABORT  0x0830
int xyz_Process(Backend *back, void *backhandle, Terminal *term);
void xyz_ReceiveInit(Terminal *term);
void xyz_StartSending(Terminal *term);
void xyz_Cancel(Terminal *term);
void xyz_updateMenuItems(Terminal *term);
#endif

// Doit être le dernier
#define IDM_LAUNCHER	0x1000

// USERCMD doit être la plus grande valeur pour permettre d'avoir autant de raccourcis qu'on le souhaite
#define IDM_USERCMD   0x8000

// Idem USERCMD
#define IDM_GOHIDE    0x9000

#define IDB_OK	1098

#include "../../MD5check.h"

#define SI_INIT 0
#define SI_NEXT 1
#define SI_RANDOM 2


// Delai avant d'envoyer le password et d'envoyer vers le tray (automatiquement à la connexion) (en seconde)
static double init_delay = 2.0 ;
// Delai entre chaque ligne de la commande automatique (en seconde)
static double autocommand_delay = 0.0005 ;
// Delai entre chaque caractères d'une commande (en millisecondes)
static int between_char_delay = 0 ;
// Delai entre deux lignes d'une même commande et entre deux raccourcis \x \k
static int internal_delay = 10 ;

// Pointeur sur la commande autocommand
static char * AutoCommand = NULL ;
// Contenu d'un script a envoyer à l'écran
static char * ScriptCommand = NULL ;
// Pointeur sur la commande a passer ligne a ligne
static char * PasteCommand = NULL ;
static int PasteCommandFlag = 0 ;


// Gestion du script file au lancement
static char * ScriptFileContent = NULL ;

// Flag de gestion de la transparence (-1 = inactif, [0,255] actif)
static int TransparencyNumber = -1 ;

// Flag pour la protection contre les saisies malheureuses
static int ProtectFlag = 0 ; 

// Flags de definition du mode de sauvegarde
#ifndef SAVEMODE_REG
#define SAVEMODE_REG 0
#endif
#ifndef SAVEMODE_FILE
#define SAVEMODE_FILE 1
#endif
#ifndef SAVEMODE_DIR
#define SAVEMODE_DIR 2
#endif

// Definition de la section du fichier de configuration
#if (defined PERSOPORT) && (!defined FDJ)
#define INIT_SECTION "KiTTY"
#define DEFAULT_INIT_FILE "kitty.ini"
#define DEFAULT_SAV_FILE "kitty.sav"
#define DEFAULT_EXE_FILE "kitty.exe"
#else
#define INIT_SECTION "PuTTY"
#define DEFAULT_INIT_FILE "putty.ini"
#define DEFAULT_SAV_FILE "putty.sav"
#define DEFAULT_EXE_FILE "putty.exe"
#endif

#ifndef VISIBLE_NO
#define VISIBLE_NO 0
#endif
#ifndef VISIBLE_YES
#define VISIBLE_YES 1
#endif
#ifndef VISIBLE_TRAY
#define VISIBLE_TRAY -1
#endif

// Flag de definition de la visibilité d'une fenêtres
static int VisibleFlag = VISIBLE_YES ;

// Flag pour inhiber le raccourcis clavier
#ifdef FDJ
static int ShortcutsFlag = 0 ;
#else
static int ShortcutsFlag = 1 ;
#endif

// Flag pour permettre la definition d'icone de connexion
static int IconeFlag = 0 ;

// La librairie dans laquelle chercher les icones (fichier defini dans kitty.ini, sinon kitty.dll s'il existe, sinon kitty.exe)
static HINSTANCE hInstIcons =  NULL ;

// Fichier contenant les icones à charger
static char * IconFile = NULL ;

// Nombre d'icones différentes (identifiant commence à 1 dans le fichier .rc)
static int NumberOfIcons = NB_ICONES ;

// Flag pour l'affichage de la taille de la fenêtre
static int SizeFlag = 0 ;

// Flag pour imposer le passage en majuscule
static int CapsLockFlag = 0 ;

// Flag pour gérer la présence de la barre de titre
static int TitleBarFlag = 1 ;

// Hauteur de la fenetre pour la fonction winrol
static int WinHeight = -1 ;

// Password de protection de la configuration (registry)
static char PasswordConf[256] = "" ;

// Renvoi automatiquement dans le tray (pour les tunnel), fonctionne avec le l'option -send-to-tray
static int AutoSendToTray = 0 ;

// Flag pour ne pas créer les fichiers kitty.ini et kitty.sav
static int NoKittyFileFlag = 0 ;

// Hauteur de la boîte de configuration
int ConfigBoxHeight = 21 ;

// Hauteur de la fenêtre de la boîte de configuration (0=valeur par defaut)
static int ConfigBoxWindowHeight = 0 ;

// Flag pour repasser en mode Putty basic
static int PuttyFlag = 0 ;

// Flag pour afficher l'image de fond
#if (defined IMAGEPORT) && (!defined FDJ)
// Suite àa PuTTY 0.61, le patch covidimus ne fonctionne plus très bien
// Il impose de démarrer les sessions avec -load même depuis la config box (voir CONFIG.C)
// Le patch est désactivé par défaut
static int BackgroundImageFlag = 0 ;
#else
static int BackgroundImageFlag = 0 ;
#endif

// Flag pour inhiber les fonctions ZMODEM
static int ZModemFlag = 1 ;

// Flag pour inhiber le filtre sur la liste des sessions de la boîte de configuration
static int SessionFilterFlag = 1 ;

// Flag pour passer en mode visualiseur d'images
static int ImageViewerFlag = 0 ;

// Durée (en secondes) pour switcher l'image de fond d'écran (<=0 pas de slide)
static int ImageSlideDelay = - 1 ;

// Compteur pour l'envoi de anti-idle
static int AntiIdleCount = 0 ;
static int AntiIdleCountMax = 5 ;
static char AntiIdleStr[128] = "" ;

// Chemin vers le programme cthelper.exe
static char * CtHelperPath = NULL ;

// Chemin vers le programme WinSCP
static char * WinSCPPath = NULL ;

// Chemin vers le programme pscp.exe
static char * PSCPPath = NULL ;

// Répertoire de lancement
static char InitialDirectory[4096] ;

// Chemin complet des fichiers de configuration kitty.ini et kitty.sav
static char * KittyIniFile = NULL ;
static char * KittySavFile = NULL ;

// Flag permettant d'activer l'acces a du code particulier permettant d'avoir plus d'info dans le kitty.dmp
static int debug_flag = 0 ;

// Nom de la classe de l'application
static char KiTTYClassName[128] = "" ;

// Parametres de l'impression
extern int PrintCharSize ;
extern int PrintMaxLinePerPage ;
extern int PrintMaxCharPerLine ;

extern char puttystr[1024] ;

NOTIFYICONDATA TrayIcone ;
#define MYWM_NOTIFYICON		(WM_USER+3)

static int IconeNum = 0 ;

void SetNewIcon( HWND hwnd, Config cfg, const int mode ) ;

#define TIMER_INIT 12341
#define TIMER_AUTOCOMMAND 12342
#if (defined IMAGEPORT) && (!defined FDJ)
#define TIMER_SLIDEBG 12343
#endif
#define TIMER_REDRAW 12344
#define TIMER_AUTOPASTE 12345
#define TIMER_BLINKTRAYICON 12346

#include <stdio.h>
#include <stdlib.h>
#include <io.h>
#include <sys/locking.h>
#include <windows.h>

#ifndef BUILD_TIME
#define BUILD_TIME "Undefined"
#endif

#ifndef BUILD_VERSION
#define BUILD_VERSION "0.0"
#endif

#ifndef BUILD_SUBVERSION
#define BUILD_SUBVERSION 0
#endif

char BuildVersionTime[256] = "0.0.0.0 @ 0" ;

// Gestion des raccourcis
#define SHIFTKEY 500
#define CONTROLKEY 1000
#define ALTKEY 1500
#define ALTGRKEY 2000
#define WINKEY 2500

// F1=0x70 (112) => F12=0x7B (123)
	
static struct TShortcuts {
	int autocommand ;
	int command ;
	int editor ;
	int getfile ;
	int imagechange ;
	int input ;
	int inputm ;
	int print ;
	int printall ;
	int protect ;
	int script ;
	int sendfile ;
	int rollup ;
	int tray ;
	int viewer ;
	int visible ;
	int winscp ;
	} shortcuts_tab ;
	
static int NbShortCuts = 0 ;
static struct TShortcuts2 { int num ; char * st ; } shortcuts_tab2[512] ;


char * getcwd (char * buf, size_t size);
int chdir(const char *path); 
char * InputBox( HINSTANCE hInstance, HWND hwnd ) ;
char *itoa(int value, char *string, int radix);
void GetAndSendLinePassword( HWND hwnd ) ;
int unlink(const char *pathname);
void InitLauncherRegistry( void ) ;
void RunScriptFile( HWND hwnd, const char * filename ) ;
void InfoBoxSetText( HWND hwnd, char * st ) ;
void ReadInitScript( const char * filename ) ;
int ReadParameter( const char * key, const char * name, char * value ) ;
int WriteParameter( const char * key, const char * name, char * value ) ;
int DelParameter( const char * key, const char * name ) ;
void GetSessionFolderName( const char * session_in, char * folder ) ;
void MakeDirTree( const char * Directory, const char * s, const char * sd ) ;
int ManageShortcuts( HWND hwnd, int key_num, int shift_flag, int control_flag, int alt_flag, int altgr_flag, int win_flag ) ;
void mungestr(const char *in, char *out);
void unmungestr(const char *in, char *out, int outlen);
void print_log( const char *fmt, ...) ;
char * SetInitialSessPath( void ) ;
char * SetSessPath( const char * dec ) ;
void CleanFolderName( char * folder ) ;
void SetInitCurrentFolder( const char * name ) ;
int print_event_log( FILE * fp, int i ) ;
void set_sshver( const char * vers ) ;
int ResizeWinList( HWND hwnd, int width, int height ) ;
int SendCommandAllWindows( HWND hwnd, char * cmd ) ;
int decode64 (char *buffer) ;
void RunCommand( HWND hwnd, char * cmd ) ;

static char * InputBoxResult = NULL ;

int WINAPI Notepad_WinMain(HINSTANCE hinstance, HINSTANCE hPrevInstance,LPSTR lpCmdLine, int nCmdShow) ;
int InternalCommand( HWND hwnd, char * st ) ;

#include "kitty_tools.h"
#include "kitty_win.h"

	
#include "kitty_crypt.h"
#include "kitty_commun.h"
#include "kitty_registry.h"

// Procedure de debug
void debug_log( const char *fmt, ...) {
	char filename[4096]="" ;
	va_list ap;
	FILE *fp ;

	if( (InitialDirectory!=NULL) && (strlen(InitialDirectory)>0) )
		sprintf(filename,"%s\\kitty.log",InitialDirectory);
	else strcpy(filename,"kitty.log");
 
	va_start( ap, fmt ) ;
	//vfprintf( stdout, fmt, ap ) ; // Ecriture a l'ecran
	if( ( fp = fopen( filename, "ab" ) ) != NULL ) {
		vfprintf( fp, fmt, ap ) ; // ecriture dans un fichier
		fclose( fp ) ;
		}
 
	va_end( ap ) ;
	}


#ifdef CYGTERMPORT
void cygterm_set_flag( int flag ) ;
int cygterm_get_flag( void ) ;
#endif

// Procedure de recuperation de la valeur d'un flag
int get_param( const char * val ) {
	if( !stricmp( val, "ICON" ) ) return IconeFlag ;
	else if( !stricmp( val, "PUTTY" ) ) return PuttyFlag ;
	else if( !stricmp( val, "CONFIGBOXHEIGHT" ) ) return ConfigBoxHeight ;
	else if( !stricmp( val, "CONFIGBOXWINDOWHEIGHT" ) ) return ConfigBoxWindowHeight ;
	else if( !stricmp( val, "NUMBEROFICONS" ) ) return NumberOfIcons ;
	else if( !stricmp( val, "SESSIONFILTER" ) ) return SessionFilterFlag ;
	else if( !stricmp( val, "INIFILE" ) ) return IniFileFlag ;
	else if( !stricmp( val, "DIRECTORYBROWSE" ) ) return DirectoryBrowseFlag ;
#ifdef ZMODEMPORT
	else if( !stricmp( val, "ZMODEM" ) ) return ZModemFlag ;
#endif
#if (defined IMAGEPORT) && (!defined FDJ)
	else if( !stricmp( val, "BACKGROUNDIMAGE" ) ) return BackgroundImageFlag ;
#endif
#ifdef CYGTERMPORT
	else if( !stricmp( val, "CYGTERM" ) ) return cygterm_get_flag() ;
#endif
	return 0 ;
	}

#if (defined IMAGEPORT) && (!defined FDJ)
	/* Le patch Background image ne marche plus bien sur la version PuTTY 0.61
		- il est en erreur lorsqu'on passe par la config box
		- il est ok lorsqu'on démarrer par -load ou par duplicate session
	   On le désactive dans la config box (fin du fichier WINCFG.C)
	*/
void DisableBackgroundImage( void ) { BackgroundImageFlag = 0 ; }
#endif

// Procedure de recuperation de la valeur d'une chaine
char * get_param_str( const char * val ) {
	if( !stricmp( val, "INI" ) ) return KittyIniFile ;
	else if( !stricmp( val, "SAV" ) ) return KittySavFile ;
	else if( !stricmp( val, "NAME" ) ) return INIT_SECTION ;
	return NULL ;
	}
	

#ifdef ZMODEMPORT
void xyz_updateMenuItems(Terminal *term)
{
	HMENU m = GetSystemMenu(hwnd, FALSE);
	EnableMenuItem(m, IDM_XYZSTART, term->xyz_transfering?MF_GRAYED:MF_ENABLED);
	EnableMenuItem(m, IDM_XYZUPLOAD, term->xyz_transfering?MF_GRAYED:MF_ENABLED);
	EnableMenuItem(m, IDM_XYZABORT, !term->xyz_transfering?MF_GRAYED:MF_ENABLED);

}
#endif

char * kitty_current_dir() { 
	static char cdir[1024]; 

return NULL ;  /* Ce code est très spécifique et ne marche pas partout */
	
	char * dir = strstr(term->osc_string, ":") ; 
	if(dir) { 
		if( strlen(dir) > 1 ) {
			dir = dir + 1 ;
			if(*dir == '~') { 
				if(strlen(cfg.username)>0) { 
				snprintf(cdir, 1024, "\"/home/%s/%s\"", cfg.username, dir + 1); 
				return cdir; 
				} 
			} 
			else if(*dir == '/') { 
				snprintf(cdir, 1024, "\"%s\"", dir); 
				return cdir; 
				} 
			} 
		}
	return NULL; 
	} 

// Liste des folder	
char **FolderList ;

int readINI( const char * filename, const char * section, const char * key, char * pStr) ;
int writeINI( const char * filename, const char * section, const char * key, char * pStr) ;
int delINI( const char * filename, const char * section, const char * key ) ;
#include <dirent.h>
// Initialise la liste des folders à partir des sessions deja existantes et du fichier kitty.ini
void InitFolderList( void ) {
	char * pst, fList[4096], buffer[1024] ;
	int i ;
	FolderList=(char**)malloc( 1024*sizeof(char*) );
	FolderList[0] = NULL ;
	StringList_Add( FolderList, "Default" ) ;
	//if( GetValueData(HKEY_CURRENT_USER, TEXT(PUTTY_REG_POS), "Folders", fList) == NULL ) return ;
	//if( ReadParameter( "KiTTY", "Folders", fList ) == 0 ) return ;
	ReadParameter( INIT_SECTION, "Folders", fList ) ;
	if( strlen( fList ) != 0 ) {
		pst = fList ;
		while( strlen( pst ) > 0 ) {
			i = 0 ;
			while( ( pst[i] != ',' ) && ( pst[i] != '\0' ) ) {
				buffer[i] = pst[i] ;
				i++ ;
				}
			buffer[i] = '\0' ;
			StringList_Add( FolderList, buffer ) ;
			if( pst[i] == '\0' ) pst = pst + i ;
			else pst = pst + i + 1 ;
			}
		//free( fList ) ; fList = NULL ;
		}
	
	if( (IniFileFlag==SAVEMODE_REG)||(IniFileFlag==SAVEMODE_FILE) ) {
		HKEY hKey ;
		TCHAR    achKey[MAX_KEY_LENGTH];   // buffer for subkey name
		DWORD    cbName;                   // size of name string 
		TCHAR    achClass[MAX_PATH] = TEXT("");  // buffer for class name 
		DWORD    cchClassName = MAX_PATH;  // size of class string 
		DWORD    cSubKeys=0;               // number of subkeys 
		DWORD    cbMaxSubKey;              // longest subkey size 
		DWORD    cchMaxClass;              // longest class string 
		DWORD    cValues;              // number of values for key 
		DWORD    cchMaxValue;          // longest value name 
		DWORD    cbMaxValueData;       // longest value data 
		DWORD    cbSecurityDescriptor; // size of security descriptor 
		FILETIME ftLastWriteTime;      // last write time 
		DWORD retCode; 

		sprintf( buffer, "%s\\\\Sessions", PUTTY_REG_POS );
		if( RegOpenKeyEx( HKEY_CURRENT_USER, buffer, 0, KEY_READ, &hKey) != ERROR_SUCCESS ) return ;
	
		retCode = RegQueryInfoKey(
		hKey,                    // key handle 
		achClass,                // buffer for class name 
		&cchClassName,           // size of class string 
		NULL,                    // reserved 
		&cSubKeys,               // number of subkeys 
		&cbMaxSubKey,            // longest subkey size 
		&cchMaxClass,            // longest class string 
		&cValues,                // number of values for this key 
		&cchMaxValue,            // longest value name 
		&cbMaxValueData,         // longest value data 
		&cbSecurityDescriptor,   // security descriptor 
		&ftLastWriteTime);       // last write time 
		// Enumerate the subkeys, until RegEnumKeyEx fails.
		if (cSubKeys) {
			for (i=0; i<cSubKeys; i++) { 
				cbName = MAX_KEY_LENGTH;
				retCode = RegEnumKeyEx(hKey, i, achKey, &cbName, NULL, NULL, NULL, &ftLastWriteTime); 
				if (retCode == ERROR_SUCCESS) {
					char nValue[1024] ;
					sprintf( nValue, "%s\\%s", buffer, achKey ) ;
					if( GetValueData(HKEY_CURRENT_USER, nValue, "Folder", fList ) != NULL ) {
						if( strlen( fList ) > 0 ) 
							StringList_Add( FolderList, fList ) ;
						//free( fList ) ; fList = NULL ;
						}
			
		}
				}
			} 
		RegCloseKey( hKey ) ;
		}
	else if( IniFileFlag == SAVEMODE_DIR ) {
		DIR * dir ;
		struct dirent * de ;
		sprintf( buffer, "%s\\Sessions", ConfigDirectory ) ;
		if( (dir=opendir(buffer)) != NULL ) {
			while( (de=readdir(dir)) != NULL ) 
			if( strcmp(de->d_name, ".")&&strcmp(de->d_name, "..") ) {
				unmungestr( de->d_name, fList, 1024 ) ;
				GetSessionFolderName( fList, buffer ) ;
				if( strlen(buffer)>0 ) StringList_Add( FolderList, buffer ) ;
				}
			closedir( dir ) ;
			}
		}
	
	if( readINI( KittyIniFile, "Folder", "new", buffer ) ) {
		if( strlen( buffer ) > 0 ) {
			for( i=0; i<strlen(buffer); i++ ) if( buffer[i]==',' ) buffer[i]='\0' ;
			StringList_Add( FolderList, buffer ) ;
			}
		delINI( KittyIniFile, "Folder", "new" ) ;
		}
	
	}

int GetSessionFolderNameInSubDir( const char * session, const char * subdir, char * folder ) {
	int return_code=0;
	char buffer[2048], buf[2048] ;
	DIR * dir ;
	struct dirent * de ;

	if( !strcmp(subdir,"") ) sprintf( buffer, "%s\\Sessions", ConfigDirectory ) ;
	else sprintf(buffer,"%s\\Sessions\\%s",ConfigDirectory, subdir ) ;

	if( (dir=opendir(buffer))!=NULL ) {
		while( (de=readdir(dir)) != NULL ) 
			if( strcmp(de->d_name,".") && strcmp(de->d_name,"..") )	{
				if( !strcmp(subdir,"") ) sprintf(buf,"%s\\Sessions\\%s",ConfigDirectory,de->d_name ) ;
				else sprintf(buf,"%s\\Sessions\\%s\\%s",ConfigDirectory, subdir,de->d_name ) ;
				
				if( existdirectory( buf ) ) {
					if( !strcmp(subdir,"") ) sprintf( buf, "%s", de->d_name ) ;
					else sprintf( buf, "%s\\%s", subdir, de->d_name ) ;
					return_code = GetSessionFolderNameInSubDir( session, buf, folder ) ;
					if( return_code ) break ;
					}
				else if( !strcmp(session,de->d_name) ) {
					strcpy( folder, subdir ) ;
					return_code=1;
					break ;
					}
			}			
		closedir(dir) ;
		}
		
	return return_code ;
	}

// Recupère le nom du folder associé à une session
void GetSessionFolderName( const char * session_in, char * folder ) {
	HKEY hKey ;
	char buffer[1024], session[1024] ;
	FILE *fp ;
	
	strcpy( folder, "" ) ;
	if( session_in == NULL ) return ;
	if( strlen(session_in)==0 ) return ;
	
	strcpy( buffer, session_in ) ;
	//if( (p = strrchr(buffer, '[')) != NULL ) *(p-1) = '\0' ;

	if( (IniFileFlag==SAVEMODE_REG)||(IniFileFlag==SAVEMODE_FILE) ) {
		mungestr(buffer, session) ;
		sprintf( buffer, "%s\\Sessions\\%s", PUTTY_REG_POS, session ) ;
		if( RegOpenKeyEx( HKEY_CURRENT_USER, buffer, 0, KEY_READ, &hKey) == ERROR_SUCCESS ) {
			DWORD lpType ;
			unsigned char lpData[1024] ;
			DWORD dwDataSize = 1024 ;
			if( RegQueryValueEx( hKey, "Folder", 0, &lpType, lpData, &dwDataSize ) == ERROR_SUCCESS ) {
				strcpy( folder, (char*)lpData ) ;
				}
			RegCloseKey( hKey ) ;
			}
		}
	else if( IniFileFlag==SAVEMODE_DIR ) {
		mungestr(session_in, session ) ;
		if( DirectoryBrowseFlag ) {
			GetSessionFolderNameInSubDir( session, "", folder ) ;
			}
		else {
			sprintf(buffer,"%s\\Sessions\\%s", ConfigDirectory, session );
			if( (fp=fopen(buffer,"r"))!=NULL ) {
				while( fgets(buffer,1024,fp)!=NULL ) {
					while( (buffer[strlen(buffer)-1]=='\n')||(buffer[strlen(buffer)-1]=='\r') ) 
						buffer[strlen(buffer)-1]='\0' ;
					if( buffer[strlen(buffer)-1]=='\\' )
						if( strstr( buffer, "Folder" ) == buffer ) {
							if( buffer[6]=='\\' ) strcpy( folder, buffer+7 ) ;
							folder[strlen(folder)-1] = '\0' ;
							unmungestr(folder, buffer, MAX_PATH) ;
							strcpy( folder, buffer) ;
							break  ;
							}
					}
				fclose(fp);
				}
			}
		}
	}

// Récupère une entrée d'une session ( retourne 1 si existe )
int GetSessionField( const char * session_in, const char * folder_in, const char * field, char * result ) {
	HKEY hKey ;
	char buffer[1024], session[1024], folder[1024], *p ;
	int res = 0 ;
	FILE * fp ;

	if( session_in == NULL ) return 0 ;
	if( strlen(session_in)==0 ) return 0 ;
	
	strcpy( result, "" ) ;
	strcpy( buffer, session_in ) ;
	if( (p = strrchr(buffer, '[')) != NULL ) *(p-1) = '\0' ;
	mungestr(buffer, session) ;
	sprintf( buffer, "%s\\Sessions\\%s", PUTTY_REG_POS, session ) ;
	strcpy( folder, folder_in );
	CleanFolderName( folder );

	if( (IniFileFlag==SAVEMODE_REG)||(IniFileFlag==SAVEMODE_FILE) ) {
		if( RegOpenKeyEx( HKEY_CURRENT_USER, buffer, 0, KEY_READ, &hKey) == ERROR_SUCCESS ) {
			DWORD lpType ;
			unsigned char lpData[1024] ;
			DWORD dwDataSize = 1024 ;
			if( RegQueryValueEx( hKey, field, 0, &lpType, lpData, &dwDataSize ) == ERROR_SUCCESS ) {
				strcpy( result, (char*)lpData ) ;
				res = 1 ;
				}
			RegCloseKey( hKey ) ;
			}
		}
	else if( IniFileFlag==SAVEMODE_DIR ) {
		if( DirectoryBrowseFlag ) {
			if( !strcmp(folder,"Default") ) sprintf(buffer,"%s\\Sessions\\%s", ConfigDirectory, session ) ;
			else sprintf(buffer,"%s\\Sessions\\%s\\%s", ConfigDirectory, folder, session ) ;
			}
		else sprintf(buffer,"%s\\Sessions\\%s", ConfigDirectory, session ) ;
		if( (fp=fopen(buffer,"r"))!=NULL ) {
			while( fgets(buffer,1024,fp)!=NULL ) {
				while( (buffer[strlen(buffer)-1]=='\n')||(buffer[strlen(buffer)-1]=='\r') ) buffer[strlen(buffer)-1]='\0' ;
				if( buffer[strlen(buffer)-1]=='\\' )
					if( strstr( buffer, field ) == buffer ) {
						if( buffer[strlen(field)]=='\\' ) strcpy( result, buffer+strlen(field)+1 ) ;
						result[strlen(result)-1] = '\0' ;
						unmungestr(result, buffer,MAX_PATH) ;
						strcpy( result, buffer) ;
						res = 1 ;
						break ;
						}
				}
			fclose(fp);
			}
		}
	return res ;
	}
	
void MASKPASS( char * password ) ;
void RenewPassword( Config * cfg ) {
	if( 1 || (strlen( cfg->password ) == 0) ) {
		char buffer[1024] = "", host[1024], termtype[1024] ;
		if( GetSessionField( cfg->sessionname, cfg->folder, "Password", buffer ) ) {
			GetSessionField( cfg->sessionname, cfg->folder, "HostName", host );
			GetSessionField( cfg->sessionname, cfg->folder, "TerminalType", termtype );
			sprintf( PassKey, "%s%sKiTTY", host, termtype ) ;
			decryptstring(buffer, PassKey ) ;
			strcpy( cfg->password, buffer ) ;
			MASKPASS(cfg->password);
			memset(buffer,0,strlen(cfg->password) );
			}
		}
	}

void SetPasswordInConfig( char * password ) {
	int len ;
	if( (password!=NULL)&&(strlen(cfg.username)>0) ) {
		len = strlen( password ) ;
		if( len > 126 ) len = 126 ;
		memcpy( cfg.password, password, len+1 ) ;
		MASKPASS(cfg.password) ;
		cfg.password[len] = '\0' ;
		}
	}

// Sauvegarde la liste des folders
void SaveFolderList( void ) {
	int i = 0 ;
	char buffer[4096] = "" ;
	while( FolderList[i] != NULL ) {
		if( strlen( FolderList[i] ) > 0 )
			strcat( buffer, FolderList[i] ) ;
		if( FolderList[i+1] != NULL ) 
			if( strlen( FolderList[i+1] ) > 0 )
				strcat( buffer, "," ) ;
		i++;
		}

	if( strlen( buffer ) > 0 ) 
		WriteParameter( INIT_SECTION, "Folders", buffer ) ;
	}

// Sauvegarde une clé de registre dans un fichier
void QueryKey( HKEY hMainKey, LPCTSTR lpSubKey, FILE * fp_out ) { 
	HKEY hKey ;
    TCHAR    achKey[MAX_KEY_LENGTH];   // buffer for subkey name
    DWORD    cbName;                   // size of name string 
    TCHAR    achClass[MAX_PATH] = TEXT("");  // buffer for class name 
    DWORD    cchClassName = MAX_PATH;  // size of class string 
    DWORD    cSubKeys=0;               // number of subkeys 
    DWORD    cbMaxSubKey;              // longest subkey size 
    DWORD    cchMaxClass;              // longest class string 
    DWORD    cValues;              // number of values for key 
    DWORD    cchMaxValue;          // longest value name 
    DWORD    cbMaxValueData;       // longest value data 
    DWORD    cbSecurityDescriptor; // size of security descriptor 
    FILETIME ftLastWriteTime;      // last write time 
 
    DWORD i,j, retCode; 
 
    TCHAR  achValue[MAX_VALUE_NAME]; 
    DWORD cchValue = MAX_VALUE_NAME; 
	
    char str[4096], b[2] =" " ;
	
	DWORD lpType, dwDataSize = 1024 ;
	char * buffer = NULL ;
	
	// On ouvre la clé
	if( RegOpenKeyEx( hMainKey, TEXT(lpSubKey), 0, KEY_READ, &hKey) != ERROR_SUCCESS ) return ;
 
    // Get the class name and the value count. 
    retCode = RegQueryInfoKey(
        hKey,                    // key handle 
        achClass,                // buffer for class name 
        &cchClassName,           // size of class string 
        NULL,                    // reserved 
        &cSubKeys,               // number of subkeys 
        &cbMaxSubKey,            // longest subkey size 
        &cchMaxClass,            // longest class string 
        &cValues,                // number of values for this key 
        &cchMaxValue,            // longest value name 
        &cbMaxValueData,         // longest value data 
        &cbSecurityDescriptor,   // security descriptor 
        &ftLastWriteTime);       // last write time 
 
	//fprintf( fp_out, "\r\n[HKEY_CURRENT_USER\\%s]\r\n" TEXT(lpSubKey) ) ;
	sprintf( str, "[HKEY_CURRENT_USER\\%s]", TEXT(lpSubKey) ) ;
	if( strlen( PasswordConf ) > 0 ) { cryptstring( str, PasswordConf ) ; }
	fprintf( fp_out, "\r\n%s\r\n", str ) ;

    // Enumerate the key values. 
    if (cValues) 
    {
        //printf( "\nNumber of values: %d\n", cValues);

        for (i=0, retCode=ERROR_SUCCESS; i<cValues; i++) 
        { 
            cchValue = MAX_VALUE_NAME; 
            achValue[0] = '\0'; 
            retCode = RegEnumValue(hKey, i, 
                achValue, 
                &cchValue, 
                NULL, 
                NULL,
                NULL,
                NULL);
 
            if (retCode == ERROR_SUCCESS ) 
            { 
                //fprintf( fp_out, "\"%s\"=",  achValue ) ;
				unsigned char lpData[1024] ;
				dwDataSize = 1024 ;
				RegQueryValueEx( hKey, TEXT( achValue ), 0, &lpType, lpData, &dwDataSize ) ;
				switch ((int)lpType){
					case REG_BINARY:
							// A FAIRE
						break ;
					case REG_DWORD:
						//fprintf( fp_out, "dword:%08x", *(int*)(lpData) ) ;
						sprintf( str, "\"%s\"=dword:%08x", achValue, *(int*)(lpData) ) ;
						break;
					case REG_EXPAND_SZ:
					case REG_MULTI_SZ:
					case REG_SZ:
						//fprintf( fp_out, "\"" ) ;
						sprintf( str, "\"%s\"=\"", achValue ) ;
						for( j=0; j<strlen((char*)lpData) ; j++ ) {
							//fprintf( fp_out, "%c", lpData[j] ) ;
							b[0]=lpData[j] ;
							strcat( str, b ) ;
							//if( lpData[j]=='\\' ) fprintf( fp_out, "\\" ) ;
							if( lpData[j]=='\\' ) strcat( str,"\\" ) ;
							}
						//fprintf( fp_out, "\"" ) ;
						strcat( str, "\"" ) ;
						break;
					}
				//fprintf( fp_out, "\r\n");
				if( strlen( PasswordConf ) > 0 ) { cryptstring( str, PasswordConf ) ; }
				fprintf( fp_out, "%s\r\n", str ) ;
            } 
        }
    }
	
    // Enumerate the subkeys, until RegEnumKeyEx fails.
    if (cSubKeys)
    {
        //printf( "\nNumber of subkeys: %d\n", cSubKeys);

        for (i=0; i<cSubKeys; i++) 
        { 
            cbName = MAX_KEY_LENGTH;
            retCode = RegEnumKeyEx(hKey, i,
                     achKey, 
                     &cbName, 
                     NULL, 
                     NULL, 
                     NULL, 
                     &ftLastWriteTime); 
            if (retCode == ERROR_SUCCESS) 
            {
				buffer = (char*) malloc( strlen( TEXT(lpSubKey) ) + strlen( achKey ) + 3 ) ;
                sprintf( buffer, "%s\\%s", TEXT(lpSubKey), achKey ) ;
				QueryKey( hMainKey, buffer, fp_out ) ;
				free( buffer );				
            }
        }
    } 
 
	RegCloseKey( hKey ) ;
}

// Renomme une Clé de registre
void RegRenameTree( HWND hdlg, HKEY hMainKey, LPCTSTR lpSubKey, LPCTSTR lpDestKey ) { // hdlg boite d'information
	if( RegTestKey( hMainKey, lpDestKey ) ) {
		if( hdlg != NULL ) InfoBoxSetText( hdlg, "Cleaning backup registry" ) ;
		RegDelTree( hMainKey, lpDestKey ) ;
		}
	if( hdlg != NULL ) InfoBoxSetText( hdlg, "Saving registry" ) ;
	RegCopyTree( hMainKey, lpSubKey, lpDestKey ) ;
	if( hdlg != NULL ) InfoBoxSetText( hdlg, "Preparing local registry" ) ;
	RegDelTree( hMainKey, lpSubKey ) ;
	}

// Permet de récupérer les sessions KiTTY dans PuTTY  (PUTTY_REG_POS)
void RepliqueToPuTTY( LPCTSTR Key ) { 
	char buffer[1024] ;
#ifdef FDJ
return ;
#endif
	if( readINI( KittyIniFile, "PuTTY", "keys", buffer ) ) {
		while( (buffer[strlen(buffer)-1]=='\n')||(buffer[strlen(buffer)-1]=='\r')||(buffer[strlen(buffer)-1]==' ')||(buffer[strlen(buffer)-1]=='\t') ) buffer[strlen(buffer)-1]='\0';
		if( !stricmp( buffer, "load" ) ) {
			sprintf( buffer, "%s\\Sessions", Key ) ;
			RegDelTree (HKEY_CURRENT_USER, "Software\\SimonTatham\\PuTTY\\Sessions" ) ;
			RegCopyTree( HKEY_CURRENT_USER, buffer, "Software\\SimonTatham\\PuTTY\\Sessions" ) ;
			sprintf( buffer, "%s\\SshHostKeys", Key ) ;
			RegCopyTree( HKEY_CURRENT_USER, buffer, "Software\\SimonTatham\\PuTTY\\SshHostKeys" ) ;
			}
		//delINI( KittyIniFile, "PuTTY", "keys" ) ;
		RegCleanPuTTY() ;
		}
	}


int license_make_with_first( char * license, int length, int modulo, int result ) ;
void license_form( char * license, char sep, int size ) ;
int license_test( char * license, char sep, int modulo, int result ) ;

// Augmente le compteur d'utilisation dans la base de registre
void CountUp( void ) {
	char buffer[1024] = "0", *pst ;
	long int n ;
	int len = 1024 ;
	
	if( ReadParameter( INIT_SECTION, "KiCount", buffer ) == 0 ) { strcpy( buffer, "0" ) ; }
	n = atol( buffer ) + 1 ;
	sprintf( buffer, "%ld", n ) ;
	WriteParameter( INIT_SECTION, "KiCount", buffer) ;
	
	if( ReadParameter( INIT_SECTION, "KiLastUp", buffer ) == 0 ) { sprintf( buffer, "%ld/", time(0) ) ; }
	if( (pst=strstr(buffer,"/"))==NULL ) { strcat(buffer,"/") ; pst=buffer+strlen(buffer)-1 ; }
	sprintf( pst+1, "%ld", time(0) ) ;
	WriteParameter( INIT_SECTION, "KiLastUp", buffer) ;
	
	if( GetUserName( buffer, (void*)&len ) ) { 
		strcat( buffer, "@" ) ;
		len = 1024 ;
		if( GetComputerName( buffer+strlen(buffer), (void*)&len ) ) {
			cryptstring( buffer, MASTER_PASSWORD ) ;
			WriteParameter( INIT_SECTION, "KiLastUH", buffer) ;
			}
		}
		
	sprintf( buffer, "%s\\Sessions", PUTTY_REG_POS ) ;	
	n = (long int) RegCountKey( HKEY_CURRENT_USER, buffer ) ;
	sprintf( buffer, "%ld", n ) ;
	WriteParameter( INIT_SECTION, "KiSess", buffer) ;
		
	GetOSInfo( buffer ) ;
	cryptstring( buffer, MASTER_PASSWORD ) ;
	WriteParameter( INIT_SECTION, "KiVers", buffer) ;
		
	if( GetModuleFileName( NULL, (LPTSTR)buffer, 1024 ) ) 
		if( strlen( buffer ) > 0 ) 
			{ WriteParameter( INIT_SECTION, "KiPath", buffer) ; }
			
	if( ReadParameter( INIT_SECTION, "KiLic", buffer ) == 0 ) { 
		strcpy( buffer, "KI60" ) ;
		license_make_with_first( buffer, 25, 97, 0 )  ;
		license_form( buffer, '-', 5 ) ;
		WriteParameter( INIT_SECTION, "KiLic", buffer) ; 
		}
	else if( !license_test( buffer, '-', 97, 0 ) ) {
		strcpy( buffer, "KI60" ) ;
		license_make_with_first( buffer, 25, 97, 0 )  ;
		license_form( buffer, '-', 5 ) ;
		WriteParameter( INIT_SECTION, "KiLic", buffer) ; 
		}
	}
	
// Si le fichier kitty.ini n'existe pas => creation du fichier par defaut
void CreateDefaultIniFile( void ) {
	char buffer[4096] ;
	if( !NoKittyFileFlag ) {
		if( KittyIniFile==NULL ) return ;
		if( strlen(KittyIniFile)==0 ) return ;
		if( !existfile( KittyIniFile ) ) {
		
			writeINI( KittyIniFile, "ConfigBox", "height", "21" ) ;
			writeINI( KittyIniFile, "ConfigBox", "filter", "yes" ) ;

#if (defined IMAGEPORT) && (!defined FDJ)
			writeINI( KittyIniFile, INIT_SECTION, "backgroundimage", "no" ) ;
#endif

			writeINI( KittyIniFile, INIT_SECTION, "capslock", "no" ) ;
			writeINI( KittyIniFile, INIT_SECTION, "conf", "yes" ) ;
#ifndef FDJ
			writeINI( KittyIniFile, INIT_SECTION, "cygterm", "yes" ) ;
#else
			writeINI( KittyIniFile, INIT_SECTION, "cygterm", "no" ) ;
#endif
			writeINI( KittyIniFile, INIT_SECTION, "icon", "no" ) ;
			writeINI( KittyIniFile, INIT_SECTION, "#iconfile", DEFAULT_EXE_FILE ) ;
			sprintf( buffer, "%d", NB_ICONES ) ;
			writeINI( KittyIniFile, INIT_SECTION, "#numberoficons", buffer ) ;
			writeINI( KittyIniFile, INIT_SECTION, "paste", "no" ) ;
			writeINI( KittyIniFile, INIT_SECTION, "print", "yes" ) ;
			
			writeINI( KittyIniFile, INIT_SECTION, "scriptfilefilter", "All files (*.*)|*.*" ) ;
			writeINI( KittyIniFile, INIT_SECTION, "size", "no" ) ;
			writeINI( KittyIniFile, INIT_SECTION, "shortcuts", "yes" ) ;
#ifndef NO_TRANSPARENCY
			writeINI( KittyIniFile, INIT_SECTION, "transparency", "no" ) ;
			writeINI( KittyIniFile, INIT_SECTION, "transparencyvalue", "0" ) ;
#endif
			writeINI( KittyIniFile, INIT_SECTION, "#configdir", "" ) ;
			writeINI( KittyIniFile, INIT_SECTION, "#downloaddir", "" ) ;
			writeINI( KittyIniFile, INIT_SECTION, "#uploaddir", "" ) ;
			writeINI( KittyIniFile, INIT_SECTION, "#remotedir", "" ) ;
			writeINI( KittyIniFile, INIT_SECTION, "#pscpdir", "" ) ;
			writeINI( KittyIniFile, INIT_SECTION, "#winscpdir", "" ) ;
			writeINI( KittyIniFile, INIT_SECTION, "#antiidle=", " \\k08\\" ) ;
			writeINI( KittyIniFile, INIT_SECTION, "#antiidledelay=", "60" ) ;
			writeINI( KittyIniFile, INIT_SECTION, "#sshversion=", "OpenSSH_5.5" ) ;

#ifdef PORTABLE
			writeINI( KittyIniFile, INIT_SECTION, "savemode", "dir" ) ;
#else
			sprintf( buffer, "%s\\%s\\%s", getenv("APPDATA"), INIT_SECTION, DEFAULT_SAV_FILE );
			writeINI( KittyIniFile, INIT_SECTION, "sav", buffer ) ;
			writeINI( KittyIniFile, INIT_SECTION, "savemode", "registry" ) ;
#endif

			writeINI( KittyIniFile, INIT_SECTION, "bcdelay", "0" ) ;
			writeINI( KittyIniFile, INIT_SECTION, "commanddelay", "0.0005" ) ;
			writeINI( KittyIniFile, INIT_SECTION, "initdelay", "2.0" ) ;
			writeINI( KittyIniFile, INIT_SECTION, "internaldelay", "10" ) ;
			writeINI( KittyIniFile, INIT_SECTION, "slidedelay", "0" ) ;
			writeINI( KittyIniFile, INIT_SECTION, "wintitle", "yes" ) ;
		
			writeINI( KittyIniFile, "Print", "height", "100" ) ;
			writeINI( KittyIniFile, "Print", "maxline", "60" ) ;
			writeINI( KittyIniFile, "Print", "maxchar", "85" ) ;

			writeINI( KittyIniFile, "Folder", "", "" ) ;
	
			writeINI( KittyIniFile, "Launcher", "reload", "yes" ) ;
			
			writeINI( KittyIniFile, "Print", "height", "100" ) ;
			writeINI( KittyIniFile, "Print", "maxline", "60" ) ;
			writeINI( KittyIniFile, "Print", "maxchar", "85" ) ;
			
			writeINI( KittyIniFile, "Shortcuts", "print", "{SHIFT}{F7}" ) ;
			writeINI( KittyIniFile, "Shortcuts", "printall", "{F7}" ) ;

			}
		}
	}

// Ecrit un parametre soit en registre soit dans le fichier de configuration
int WriteParameter( const char * key, const char * name, char * value ) {
	int ret = 1 ;
	char buffer[4096] ;
	if( IniFileFlag == SAVEMODE_DIR ) { 
		ret = writeINI( KittyIniFile, key, name, value ) ; 
		}
	else { 
		sprintf( buffer, "%s\\%s", TEXT(PUTTY_REG_PARENT), key ) ;
		RegTestOrCreate( HKEY_CURRENT_USER, buffer, name, value ) ; 
		}
	return ret ;
	}

// Lit un parametre soit dans le fichier de configuration, soit dans le registre
int ReadParameter( const char * key, const char * name, char * value ) {
	char buffer[1024] ;
	strcpy( buffer, "" ) ;

	if( GetValueData( HKEY_CURRENT_USER, TEXT(PUTTY_REG_POS), name, buffer ) == NULL ) {
		if( !readINI( KittyIniFile, key, name, buffer ) ) {
			strcpy( buffer, "" ) ;
			}
		}
	strcpy( value, buffer ) ;
	return strcmp( buffer, "" ) ;
	}
	
// Supprime un parametre
int DelParameter( const char * key, const char * name ) {
	char buffer[4096] ;
	delINI( KittyIniFile, key, name ) ;
	sprintf( buffer, "%s\\%s", TEXT(PUTTY_REG_PARENT), key ) ;
	RegDelValue( HKEY_CURRENT_USER, buffer, (char*)name ) ;
	return 1 ;
	}
	
// Test la configuration (mode file ou registry) et charge le fichier kitty.sav si besoin
void GetSaveMode( void ) {
#ifndef PORTABLE
	char buffer[256] ;
	if( readINI( KittyIniFile, INIT_SECTION, "savemode", buffer ) ) {
		while( (buffer[strlen(buffer)-1]=='\n')||(buffer[strlen(buffer)-1]=='\r')||(buffer[strlen(buffer)-1]==' ')||(buffer[strlen(buffer)-1]=='\t') ) buffer[strlen(buffer)-1]='\0';
		if( !stricmp( buffer, "registry" ) ) IniFileFlag = SAVEMODE_REG ;
		else if( !stricmp( buffer, "file" ) ) IniFileFlag = SAVEMODE_FILE ;
		else if( !stricmp( buffer, "dir" ) ) IniFileFlag = SAVEMODE_DIR ;
		}
#endif
	if( IniFileFlag!=SAVEMODE_DIR ) DirectoryBrowseFlag = 0 ;
	}

// Sauvegarde de la clé de registre
int fileno(FILE *stream) ;
void SaveRegistryKeyEx( HKEY hMainKey, LPCTSTR lpSubKey, const char * filename ) {
	FILE * fp_out ;
	//FILE * fp_out1 ;
	char buffer[4096] ;

	if( ( fp_out=fopen( filename, "wb" ) ) == NULL ) return ;
	if( _locking( fileno(fp_out) , LK_LOCK, 10000000L ) == -1 ) { fclose(fp_out); return ; }
	
	strcpy( buffer, "Windows Registry Editor Version 5.00" ) ;

	if( strlen( PasswordConf ) > 0 ) cryptstring( buffer, PasswordConf ) ;
	fprintf( fp_out, "%s\r\n", buffer ); 

	QueryKey( hMainKey, lpSubKey, fp_out ) ;
	
	_locking( fileno(fp_out) , LK_UNLCK, 10000000L );
	fclose( fp_out ) ;
	}

void SaveRegistryKey( void ) {
	if( IniFileFlag == SAVEMODE_DIR ) return ;
	if( NoKittyFileFlag || (KittySavFile==NULL) ) return ;
	if( strlen(KittySavFile)==0 ) return ;

	if( GetValueData( HKEY_CURRENT_USER, TEXT(PUTTY_REG_POS), "password", PasswordConf ) == NULL ) 
		{ strcpy( PasswordConf, "" ) ; }

	if( strlen( PasswordConf ) > 0 ) 
		{ WriteParameter( INIT_SECTION, "password", PasswordConf ) ; }

	SaveRegistryKeyEx( HKEY_CURRENT_USER, TEXT(PUTTY_REG_POS), KittySavFile ) ;
	}

void routine_SaveRegistryKey( void * st ) { SaveRegistryKey() ; }

// Charge la cle de registre
void LoadRegistryKey( HWND hdlg ) { // hdlg est la boîte de dialogue d'information de l'avancement (si null pas d'info)
	FILE *fp ;
	HKEY hKey = NULL ;
	char buffer[4096], KeyName[1024] = "", ValueName[1024], *Value ;
	int nb=0 ;
	
	if( KittySavFile==NULL ) return ;
	if( strlen(KittySavFile)==0 ) return ;
	
	if( ( fp = fopen( KittySavFile,"rb" ) ) == NULL ) return ;
	while( fgets( buffer, 4096, fp ) != NULL ) {
		while( (buffer[strlen(buffer)-1]=='\n')||(buffer[strlen(buffer)-1]=='\r')||(buffer[strlen(buffer)-1]==' ')||(buffer[strlen(buffer)-1]=='\t') ) buffer[strlen(buffer)-1]='\0' ;
		
		// Test si on a un fichier crypté
		if( nb == 0 ) {
			if( strcmp( buffer, "Windows Registry Editor Version 5.00" ) ) {
				GetAndSendLinePassword( NULL ) ;
				if( InputBoxResult == NULL ) exit(0) ;
				if( strlen( InputBoxResult ) == 0 ) exit(0) ;
				strcpy( PasswordConf, InputBoxResult ) ;
				decryptstring( buffer, PasswordConf ) ;
				if( strcmp( buffer, "Windows Registry Editor Version 5.00" ) ) {
					MessageBox( NULL, "Wrong password", "Error", MB_OK|MB_ICONERROR ) ;
					exit(1) ;
					}
				if( strlen(PasswordConf) > 0 )
					WriteParameter( INIT_SECTION, "password", PasswordConf ) ;
				}
			}
		nb++ ;
		if( strlen( PasswordConf ) > 0 ) {
			decryptstring( buffer, PasswordConf ) ;
			}
			
		if( strlen( buffer ) == 0 ) ;
		if( (buffer[0]=='[') && (buffer[strlen(buffer)-1]==']') ) {
			strcpy( KeyName, buffer+19 ) ; // +19 pour supprimer [HKEY_CURRENT_USER
			KeyName[strlen(KeyName)-1] = '\0' ;
			if( hKey != NULL ) { RegCloseKey( hKey ) ; hKey = NULL ; }
			if( RegOpenKeyEx( HKEY_CURRENT_USER, TEXT(KeyName), 0, KEY_WRITE, &hKey) != ERROR_SUCCESS ) 
				{
					if( hdlg != NULL ) {
						sprintf( buffer, "Loading %s", KeyName ) ;
						InfoBoxSetText( hdlg, buffer ) ;
						}
					RegCreateKey( HKEY_CURRENT_USER, TEXT(KeyName), &hKey ) ; 
					}
			}
		else {
			if( ( Value = strstr( buffer, "=" ) ) != NULL ) {
				strcpy( ValueName, buffer+1 ) ;
				ValueName[ (int)(Value-buffer-2) ] = '\0' ;
				Value++;
			if( Value[0] == '\"' ) { // REG_SZ
			  	Value++;
			  	Value[strlen(Value)-1] = '\0' ;
				DelDoubleBackSlash( Value ) ;
			  	RegSetValueEx( hKey, TEXT( ValueName ), 0, REG_SZ, Value, strlen(Value)+1 ) ;
			  	}
			else if( strstr(Value,"dword:") == Value ) { //REG_DWORD
				int dwData = 0 ;			  	
				Value = Value + 6 ;
				sscanf( Value, "%08x", (int*)&dwData ) ;
				RegSetValueEx( hKey, TEXT( ValueName ), 0, REG_DWORD, (LPBYTE)&dwData, sizeof(DWORD) ) ;
				}
			else { // erreur
				MessageBox( NULL, "Unknown value type", "Error", MB_OK|MB_ICONERROR ); 
				exit( 1 ) ;
				}
			}
			}
		}
	if( hKey != NULL ) { RegCloseKey( hKey ) ; hKey = NULL ; }
	fclose( fp ) ;
	}

void DelRegistryKey( void ) {
	RegDelTree( HKEY_CURRENT_USER, TEXT(PUTTY_REG_PARENT) ) ;
	}

#include <process.h>  
//extern const int Default_Port ;
//void server_run_routine( const int port, const int timeout ) ;
extern int PORT ; int main_m1( void ) ;
void routine_server( void * st ) { main_m1() ; }

void SendStrToTerminal( const char * str, const int len ) {
	char c ;
	int i ;
	if( len <= 0 ) return ;
	term_seen_key_event(term);
	for( i=0 ; i<len ; i++ ) {
		c=(unsigned char)str[i] ;
		if (ldisc)
			lpage_send(ldisc, CP_ACP, &c, 1, 1);
		}
	}

void SendKeyboard( HWND hwnd, char * buffer ) {
	int i ; 
	if( strlen( buffer) > 0 ) {
		for( i=0; i< strlen( buffer ) ; i++ ) {
			if( buffer[i] == '\n' ) {
				SendMessage(hwnd, WM_KEYDOWN, VK_RETURN, 0) ;
				}
			else if( buffer[i] == '\r' ) {
				}
			else 
				//lpage_send(ldisc, CP_ACP, buffer+i, 1, 1);
				SendMessage(hwnd, WM_CHAR, buffer[i], 0) ;
			if( between_char_delay > 0 ) Sleep( between_char_delay ) ;
			}
		}
	}

/*
SetForegroundWindow(hwnd);
keybd_event(VK_CONTROL, 0x1D, 0, 0);
keybd_event(0x58, 0x47, 0, 0);
keybd_event(0x58, 0x47, KEYEVENTF_KEYUP, 0);
keybd_event(VK_CONTROL, 0x1D, KEYEVENTF_KEYUP, 0);
*/
	
static int keyb_control_flag = 0 ;
static int keyb_shift_flag = 0 ;
static int keyb_win_flag = 0 ;
static int keyb_alt_flag = 0 ;
static int keyb_altgr_flag = 0 ;
	
void SendKeyboardPlus( HWND hwnd, const char * st ) {
	if( strlen(st) <= 0 ) return ;
	int i=0, j=0;
	//int internal_delay = 10 ;
	char *buffer = NULL, stb[6] ;
	
	if( ( buffer = (char*) malloc( strlen( st ) + 10 ) ) != NULL ) {
		buffer[0] = '\0' ;
		do {
		if( st[i] == '\\' ) {
			if( st[i+1] == '\\' ) { buffer[j] = '\\' ; i++ ; j++ ; }
			//else if( st[i+1] == '\0' ) { buffer[j] = '\\' ; j++ ; }
			else if( (st[i+1] == '/') && (i==0) ) {	buffer[j] = '/' ; i++ ; j++ ; }
			else if( st[i+1] == 't' ) { buffer[j] = '\t' ; i++ ; j++ ; }
			else if( st[i+1] == 'r' ) { buffer[j] = '\r' ; i++ ; j++ ; }
			else if( st[i+1] == 'h' ) { buffer[j] = 8 ; i++ ; j++ ; }
			else if( st[i+1] == 'n' ) {
				SendKeyboard( hwnd, buffer ) ; SendKeyboard( hwnd, "\n" ) ;
				Sleep( internal_delay ) ;
				buffer[0] = '\0' ; j = 0 ;
				i++ ;
				}
			else if( st[i+1] == 'p' ) { 
				SendKeyboard( hwnd, buffer ) ;
				Sleep(1000);
				buffer[0] = '\0' ; j = 0 ;
				i++ ; 
				}
			else if( st[i+1] == 's' ) { 
				SendKeyboard( hwnd, buffer ) ;
				j = 1 ;
				if( (st[i+2]>='0')&&(st[i+2]<='9')&&(st[i+3]>='0')&&(st[i+3]<='9') ) {
					stb[0]=st[i+2];stb[1]=st[i+3];stb[2]='\0' ;
					j=atoi(stb) ;
					i++ ; i++ ;
					}
				Sleep(j*1000);
				buffer[0] = '\0' ; j = 0 ;
				i++ ; 
				}
			else if( st[i+1] == 'c' ) { 
				keybd_event(VK_CONTROL, 0x1D, 0, 0) ;
				keybd_event(VK_CANCEL, 0x1D, 0, 0) ;
				keybd_event(VK_CANCEL, 0x1D, KEYEVENTF_KEYUP, 0) ;
				keybd_event(VK_CONTROL, 0x1D, KEYEVENTF_KEYUP, 0) ;
				buffer[0] = '\0' ; j = 0 ;
				i++ ; 
				}
			else if( st[i+1] == 'k' ) {
				SendKeyboard( hwnd, buffer ) ;
				sprintf( stb, "0x%c%c", st[i+2],st[i+3] ) ;
				sscanf( stb, "%x", &j ) ;
				//SendMessage(hwnd, WM_KEYDOWN, j, 0) ;
				if( j==VK_CONTROL ) keyb_control_flag = abs( keyb_control_flag - 1 ) ;
				else if( j==VK_SHIFT ) keyb_shift_flag = abs( keyb_shift_flag - 1 ) ;
				else if( (j==VK_MENU)||(j==VK_LMENU) ) keyb_alt_flag = abs( keyb_alt_flag - 1 ) ;
				else if( j==VK_RMENU ) keyb_altgr_flag = abs( keyb_altgr_flag - 1 ) ;
				else if( (j==VK_RWIN)||(j==VK_LWIN) ) keyb_win_flag = abs( keyb_win_flag - 1 ) ;
				else {
					if( keyb_control_flag || keyb_shift_flag || keyb_win_flag || keyb_alt_flag || keyb_altgr_flag ) {
						SetForegroundWindow(hwnd) ; Sleep( internal_delay ) ;
						if( keyb_control_flag )	keybd_event(VK_CONTROL, 0x1D, 0, 0) ;
						if( keyb_shift_flag )	keybd_event(VK_SHIFT , 0x1D, 0, 0) ;
						if( keyb_alt_flag )	keybd_event(VK_LMENU , 0x1D, 0, 0) ;
						if( keyb_altgr_flag )	keybd_event(VK_RMENU , 0x1D, 0, 0) ;
						if( keyb_win_flag )	keybd_event(VK_LWIN , 0x1D, 0, 0) ;
						keybd_event( j, 0x47, 0, 0); 
						keybd_event( j, 0x47, KEYEVENTF_KEYUP, 0) ;
						if( keyb_win_flag )	keybd_event(VK_LWIN, 0x1D, KEYEVENTF_KEYUP, 0) ;
						if( keyb_altgr_flag )	keybd_event(VK_RMENU, 0x1D, KEYEVENTF_KEYUP, 0) ;
						if( keyb_alt_flag )	keybd_event(VK_LMENU, 0x1D, KEYEVENTF_KEYUP, 0) ;
						if( keyb_shift_flag )	keybd_event(VK_SHIFT, 0x1D, KEYEVENTF_KEYUP, 0) ;
						if( keyb_control_flag )	keybd_event(VK_CONTROL, 0x1D, KEYEVENTF_KEYUP, 0) ;
						}
					else if( j==VK_ESCAPE ) {
						keybd_event( VK_ESCAPE, 1, 0, 0); 
						keybd_event( VK_ESCAPE, 1, KEYEVENTF_KEYUP, 0) ;
						}
					else {
						sprintf( stb, "%c", j ) ;
						SendStrToTerminal( stb, 1 ) ;
						//SendMessage(hwnd, WM_CHAR, j, 0) ;
						}
					}
				Sleep( internal_delay ) ;
				buffer[0] = '\0' ; j = 0 ;
				i++ ; i++ ; i++ ;
				}
			else if( st[i+1] == 'x' ) {
				SendKeyboard( hwnd, buffer ) ;
				sprintf( stb, "0x%c%c", st[i+2],st[i+3] ) ;
				sscanf( stb, "%X", &j ) ;
				stb[0]=j ; stb[1] = '\0' ;
				SendStrToTerminal( stb, 1 ) ;
				Sleep( internal_delay ) ;
				buffer[0] = '\0' ; j = 0 ;
				i++ ; i++ ; i++ ;
				}
			else { buffer[j] = '\\' ; j++ ; }
			}
		else { buffer[j] = st[i] ; j++ ; }
		buffer[j] = '\0' ;
		i++ ; 
		} while( st[i] != '\0' ) ;
		
		if( strlen( buffer ) > 0 ) {
			if( buffer[strlen(buffer)-1]=='\\' ) { // si la command se termine par \ on n'envoie pas de retour charriot
				buffer[strlen(buffer)-1]='\0' ;
				SendKeyboard( hwnd, buffer ) ;
				}
			else {
				SendKeyboard( hwnd, buffer ) ;
				if( buffer[strlen(buffer)-1] != '\n' ) // On ajoute un retour charriot au besoin
					SendKeyboard( hwnd, "\n" ) ;
				}
			}
		free( buffer ) ;
		}
	}

void SendAutoCommand( HWND hwnd, const char * cmd ) {
	if( strlen( cmd ) > 0 ) {
		/*FILE * fp ;
		if( ( fp = fopen( cmd, "r" ) ) != NULL ){
			char buffer[4096] ;
			while( fgets( buffer, 4096, fp) != NULL ) {
				SendKeyboard( hwnd, buffer ) ;
				}
			SendKeyboard( hwnd, "\n" ) ;
			fclose( fp ) ;
			}*/
		if( existfile( cmd ) ) RunScriptFile( hwnd, cmd ) ;
		else if( (toupper(cmd[0])=='C')&&(toupper(cmd[1])==':')&&(toupper(cmd[2])=='\\') ) {
			//MessageBox( NULL, cmd,"Info", MB_OK );
			return ;
			}
		else { SendKeyboardPlus( hwnd, cmd ) ; }
		}
	}


void ManageProtect( HWND hwnd, Config cfg ) {
	HMENU m ;
	if( ( m = GetSystemMenu (hwnd, FALSE) ) != NULL ) {
		DWORD fdwMenu = GetMenuState( m, (UINT) IDM_PROTECT, MF_BYCOMMAND); 
		if (!(fdwMenu & MF_CHECKED)) {
			CheckMenuItem( m, (UINT)IDM_PROTECT, MF_BYCOMMAND|MF_CHECKED ) ;
			ProtectFlag = 1 ;
			//char buffer[1024] = "" ;
			//sprintf( buffer, "%s  (PROTECTED)", cfg.wintitle ) ;
			set_title(NULL, cfg.wintitle ) ;
			}
		else {
			CheckMenuItem( m, (UINT)IDM_PROTECT, MF_BYCOMMAND|MF_UNCHECKED ) ;
			ProtectFlag = 0 ;
			set_title(NULL, cfg.wintitle);
			}
		}
	}

// Affiche un menu dans le système Tray
void DisplaySystemTrayMenu( HWND hwnd ) {
	HMENU menu ;
	POINT pt;
	long  lReturnValue = 0;

	menu = CreatePopupMenu () ;
	AppendMenu( menu, MF_ENABLED, IDM_FROMTRAY, "&Restore" ) ;
	AppendMenu( menu, MF_SEPARATOR, 0, 0 ) ;
	AppendMenu( menu, MF_ENABLED, IDM_ABOUT, "&About" ) ;
	AppendMenu( menu, MF_ENABLED, IDM_QUIT, "&Quit" ) ;
		
	SetForegroundWindow( hwnd ) ;
	GetCursorPos (&pt);
	lReturnValue = TrackPopupMenu (menu, TPM_LEFTALIGN, pt.x, pt.y, 0, hwnd, NULL);
	}
	
// Gère l'envoi dans le System Tray
int ManageToTray( HWND hwnd ) {
	//SendMessage(hwnd, WM_SYSCOMMAND, SC_MINIMIZE, 0);
	//MessageBox( NULL, "To tray", "Tray", MB_OK ) ;
	//SendMessage( hwnd, WM_SETICON, ICON_SMALL, (LPARAM)LoadIcon( hInstIcons, MAKEINTRESOURCE(IDI_MAINICON_0 + IconeNum ) ) );
	// Message MYWM_NOTIFYICON pour faire réapparaître

	int ResShell ;
	char buffer[4096] ;
	TrayIcone.hWnd = hwnd;
	//TrayIcone.hIcon = LoadIcon((HINSTANCE) GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_MAINICON_0 + IconeNum));
	ResShell = Shell_NotifyIcon(NIM_ADD, &TrayIcone);
						
	if( ResShell ) {
		GetWindowText( hwnd, buffer, 4096 ) ;
		//buffer[strlen(buffer)-21] = '\0' ;
		strcpy( TrayIcone.szTip, buffer ) ;
		ResShell = Shell_NotifyIcon(NIM_MODIFY, &TrayIcone);
		if (IsWindowVisible(hwnd)) ShowWindow(hwnd, SW_HIDE);
		//SendMessage(hwnd, WM_SYSCOMMAND, SC_MINIMIZE, 0);
		return 1 ;
		}
	else return 0 ;
	}

// Gère l'option always visible
void ManageVisible( HWND hwnd ) {
	HMENU m ;
	if( ( m = GetSystemMenu (hwnd, FALSE) ) != NULL ) {
		DWORD fdwMenu = GetMenuState( m, (UINT) IDM_VISIBLE, MF_BYCOMMAND); 
		if (!(fdwMenu & MF_CHECKED)) {
			CheckMenuItem( m, (UINT)IDM_VISIBLE, MF_BYCOMMAND|MF_CHECKED ) ;
			SetWindowPos(hwnd,(HWND)-1,0,0,0,0,  SWP_NOMOVE |SWP_NOSIZE );
			}
		else {
			CheckMenuItem( m, (UINT)IDM_VISIBLE, MF_BYCOMMAND|MF_UNCHECKED ) ;
			SetWindowPos(hwnd,(HWND)-2,0,0,0,0,  SWP_NOMOVE |SWP_NOSIZE );
			}
		}
	}

// Gère la demande de relance de l'application
void ManageRestart( HWND hwnd ) {
	SendMessage( hwnd, WM_COMMAND, IDM_RESTART, 0 ) ;
	}

// Modification de l'icone de l'application
//SendMessage( hwnd, WM_SETICON, ICON_SMALL, (LPARAM)LoadIcon( hInstIcons, MAKEINTRESOURCE(IDI_MAINICON_0 + IconeNum ) ) );
void SetNewIcon( HWND hwnd, Config cfg, const int mode ) {
	
	HICON hIcon = NULL ;
	if( (strlen(cfg.iconefile)>0) && existfile(cfg.iconefile) ) { 
		hIcon = LoadImage(NULL, cfg.iconefile, IMAGE_ICON, 32, 32, LR_LOADFROMFILE|LR_SHARED) ; 
		}

	if(hIcon) {
		SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon) ; 
		SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon) ;
		TrayIcone.hIcon = hIcon ;
		//DeleteObject( hIcon ) ; 
		}
	else {
	if( mode == SI_INIT ) {
		if( cfg.icone!=0 ) IconeNum = cfg.icone - 1 ;
		hIcon = LoadIcon( hInstIcons, MAKEINTRESOURCE(IDI_MAINICON_0 + IconeNum ) ) ;
		SendMessage( hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon );
		SendMessage( hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon );
		TrayIcone.hIcon = hIcon ;
		}
	else {
		if( IconeFlag==0 ) return ;
		if( IconeFlag <= 0 ) { IconeNum = 0 ; } 
		else {
			if( mode == SI_RANDOM ) IconeNum = time( NULL ) % NumberOfIcons ;
			else { IconeNum++ ; if( IconeNum >= NumberOfIcons ) IconeNum = 0 ; }
			}
		hIcon = LoadIcon( hInstIcons, MAKEINTRESOURCE(IDI_MAINICON_0 + IconeNum ) ) ;
		SendMessage( hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon );	
		SendMessage( hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon );
		TrayIcone.hIcon = hIcon ;
		}
	}
	}

// Envoi d'un fichier de script local
void RunScriptFile( HWND hwnd, const char * filename ) {
	long len = 0 ; size_t lread ;
	char * oldcmd = NULL ;
	FILE * fp ;
		/*
		strcpy( buffer, "" ) ;
		if( ( fp = fopen( filename, "r" ) ) != NULL ){
			while( fgets( buffer, 4096, fp) != NULL ) {
				SendKeyboard( hwnd, buffer ) ;
				}
			SendKeyboard( hwnd, "\n" ) ;
			fclose( fp ) ;
			}
		*/
	if( ScriptCommand != NULL ) { free( ScriptCommand ) ; ScriptCommand = NULL ; }
		if( existfile( filename ) ) {

		len = filesize( filename ) ;
		if( (AutoCommand!=NULL)&&(strlen(AutoCommand)>0) ) {
			oldcmd=(char*)malloc(strlen(AutoCommand)+3) ;
			sprintf( oldcmd, "\\n%s", AutoCommand );
			}
		if( oldcmd==NULL ) ScriptCommand = (char*) malloc( len + 1 ) ; 
		else ScriptCommand = (char*) malloc( len + strlen(oldcmd) + 2 ) ; 
		if( ( fp = fopen( filename, "r" ) ) != NULL ) {
			lread = fread( ScriptCommand, 1, len, fp ) ;
			ScriptCommand[lread]='\0' ;
			fclose( fp ) ;
			if( oldcmd!=NULL ) strcat( ScriptCommand, oldcmd ) ;
			if( strlen( ScriptCommand) > 0 ) {
				AutoCommand = ScriptCommand ;
				SetTimer(hwnd, TIMER_AUTOCOMMAND, (int)(autocommand_delay*1000), NULL) ;
				}
			}
		if( oldcmd!=NULL ) free( oldcmd ) ;
		}
	}

void OpenAndSendScriptFile( HWND hwnd ) {
	char filename[4096], buffer[4096] ;
	if( ReadParameter( INIT_SECTION, "scriptfilefilter", buffer ) ) {
		}
	else strcpy( buffer, "Script files (*.ksh,*.sh)|*.ksh;*.sh|SQL files (*.sql)|*.sql|All files (*.*)|*.*|" ) ;
	if( buffer[strlen(buffer)-1]!='|' ) strcat( buffer, "|" ) ;
	if( OpenFileName( hwnd, filename, "Open file...", buffer ) ) {
		RunScriptFile( hwnd, filename ) ;
		}
	}
	
// Envoi d'un fichier par SCP vers la racine du compte
int SearchPSCP( void ) ;
static int nb_pscp_run = 0 ;
void SendOneFile( HWND hwnd, char * directory, char * filename, char * distantdir) {
	char buffer[4096], pscppath[4096]="", pscpport[16]="22", remotedir[4096]=".",dir[4096], b1[256] ;
	
	if( distantdir == NULL ) { distantdir = kitty_current_dir(); } 
	if( PSCPPath==NULL ) {
		if( IniFileFlag == SAVEMODE_REG ) return ;
		else if( !SearchPSCP() ) return ;
		}
	if( !existfile( PSCPPath ) ) {
		if( IniFileFlag == SAVEMODE_REG ) return ;
		else if( !SearchPSCP() ) return ;
		}
		
	if( !GetShortPathName( PSCPPath, pscppath, 4095 ) ) return ;
	
	/*
	if( ReadParameter( INIT_SECTION, "pscpdir", buffer ) ) {
		if( buffer[strlen(buffer)-1] != '\\' ) strcat( buffer, "\\" ) ;
		strcat( buffer, "pscp.exe" ) ;
		if( !GetShortPathName( buffer, pscppath, 4095 ) ) strcpy( pscppath, "pscp.exe" ) ;
		}
	else strcpy( pscppath, "pscp.exe" ) ;
	*/

	if( ReadParameter( INIT_SECTION, "uploaddir", dir ) ) {
		if( chdir( dir ) == -1 ) 
			strcpy( dir, InitialDirectory ) ;
		}
	if (strlen( dir ) == 0) strcpy( dir, InitialDirectory ) ;

	if( (distantdir!=NULL) && (strlen(distantdir)>0) ) {
		strcpy( remotedir, distantdir ) ;
		}
	else if( !ReadParameter( INIT_SECTION, "remotedir", remotedir ) ) {
		strcpy( remotedir, "." ) ;
		}
	if( strlen( remotedir ) == 0 ) strcpy( remotedir, "." ) ;

	strcpy( buffer, "" ) ;
	
	if( nb_pscp_run<4 ) { sprintf( buffer, "start %s ", pscppath ) ; nb_pscp_run++ ; }
	else { sprintf( buffer, "%s ", pscppath ) ; nb_pscp_run = 0 ; }
	
	strcat( buffer, "-r " ) ; //strcat( buffer, "-batch " ) ;
	if( ReadParameter( INIT_SECTION, "pscpport", pscpport ) ) {
		if( !strcmp( pscpport,"*" ) ) sprintf( pscpport, "%d", cfg.port ) ;
		strcat( buffer, "-P " ) ;
		strcat( buffer, pscpport ) ;
		strcat( buffer, " " ) ;
		}
	else {
		sprintf( b1, "-P %d ", cfg.port ) ;
		strcat( buffer, b1 ) ;
		}

	if( cfg.sshprot == 2 ) {
		sprintf( b1, "-%d", cfg.sshprot ) ; strcat( buffer, b1 ); 
		strcat( buffer, " " ) ;
		}
	if( strlen( cfg.password ) > 0 ) {
		strcat( buffer, "-pw \"" ) ;
		MASKPASS(cfg.password);	strcat( buffer, cfg.password ) ; MASKPASS(cfg.password);
		strcat( buffer, "\" " ) ;
		}
	if( strlen( cfg.keyfile.path ) > 0 ) {
		strcat( buffer, "-i \"" ) ;
		strcat( buffer, cfg.keyfile.path ) ;
		strcat( buffer, "\" " ) ;
		}
	strcat( buffer, "\"" ) ; //strcat( buffer, filename ) ; 
	if( (strlen(directory)>0) && (strlen(filename)>0) ) {
		strcat( buffer, directory ) ; 
		strcat( buffer, "\\" ) ; 
		strcat( buffer, filename ) ;
		}
	else if( (directory!=NULL)&&(strlen(directory)>0) ) { strcat(buffer, directory ) ; }
	else { strcat(buffer, filename ) ; }
	strcat( buffer, "\" " ) ;
	strcat( buffer, cfg.username ) ; strcat( buffer, "@" ) ;
	strcat( buffer, cfg.host ) ; strcat( buffer, ":" ) ; strcat( buffer, remotedir ) ;
	//strcat( buffer, " > kitty.log 2>&1" ) ; //if( !system( buffer ) ) unlink( "kitty.log" ) ;

	chdir( InitialDirectory ) ;
	if( debug_flag ) logevent(NULL, buffer);
	if( system( buffer ) ) MessageBox( NULL, buffer, "Transfer problem", MB_OK|MB_ICONERROR  ) ;
	memset(buffer,0,strlen(buffer));
	//debug_log("%s\n",buffer);//MessageBox( NULL, buffer, "Info",MB_OK );
	}

void SendFileList( HWND hwnd, char * filelist ) {
	char *pname=NULL,dir[4096] ;
	int i;

	if( filelist==NULL ) return ;
	if( strlen( filelist ) == 0 ) return ;

	if( (filelist[strlen(filelist)]=='\0') && (filelist[strlen(filelist)+1]=='\0') ) {
		i=strlen(filelist) ;
		while( i>0 ) {
			i--;
			if( (filelist[i]=='/')||(filelist[i]=='\\') ) { filelist[i]='\0' ; break ; }
			}
		}
	strcpy( dir, filelist ) ;

	pname=filelist+strlen(filelist)+1;
	
	i = 0 ;
	while( pname[i] != '\0' ){
			
		while( (pname[i] != '\0') && (pname[i] != '\n') && (pname[i] != '\r') ) { i++ ; }
		pname[i]='\0';
		SendOneFile( hwnd, dir, pname, NULL ) ;
		pname=pname+i+1 ; i=0;
		}
		
	}

void SendFile( HWND hwnd ) {
	char filename[32768] ;

	//if( cfg.protocol == PROT_SSH ) {
	if( cfg.protocol != PROT_SSH ) {
		MessageBox( hwnd, "This function is only available with SSH connections.", "Error", MB_OK|MB_ICONERROR ) ;
		return ;
		}

	if( OpenFileName( hwnd, filename, "Send file...", "All files (*.*)|*.*|" ) ) 
		if( strlen( filename ) > 0 ) {
			SendFileList( hwnd, filename ) ;
		}
	}

// Reception d'un fichier par pscp
/* ALIAS UNIX A DEFINIR POUR RECUPERER UN FICHIER
get()
{
echo "\033]0;__pw:"`pwd`"\007"
for file in ${*} ; do echo "\033]0;__rv:"${file}"\007" ; done
}
Il faut ensuite simplement taper: get filename
C'est traité dans KiTTY par la fonction ManageLocalCmd
*/
void GetOneFile( HWND hwnd, char * directory, char * filename ) {
	char buffer[4096], pscppath[4096]="", pscpport[16]="22", dir[4096]=".", b1[256] ;
	
	if( PSCPPath==NULL ) {
		if( IniFileFlag == SAVEMODE_REG ) return ;
		else if( !SearchPSCP() ) return ;
		}
	if( !existfile( PSCPPath ) ) {
		if( IniFileFlag == SAVEMODE_REG ) return ;
		else if( !SearchPSCP() ) return ;
		}
		
	if( !GetShortPathName( PSCPPath, pscppath, 4095 ) ) return ;

	/*
	if( ReadParameter( INIT_SECTION, "pscpdir", buffer ) ) {
		if( buffer[strlen(buffer)-1] != '\\' ) strcat( buffer, "\\" ) ;
		strcat( buffer, "pscp.exe" ) ;
		if( !GetShortPathName( buffer, pscppath, 4095 ) ) strcpy( pscppath, "pscp.exe" ) ;
		}
	else strcpy( pscppath, "pscp.exe" ) ;
	*/

	if( ReadParameter( INIT_SECTION, "downloaddir", dir ) ) {
		if(chdir( dir ) == -1)
			strcpy( dir, InitialDirectory ) ;
		}
	if(strlen( dir ) == 0) strcpy( dir, InitialDirectory ) ;

	strcpy( buffer, "" ) ;
	
	if( nb_pscp_run<4 ) { sprintf( buffer, "start %s ", pscppath ) ; nb_pscp_run++ ; }
	else { sprintf( buffer, "%s ", pscppath ) ; nb_pscp_run = 0 ; }

	strcat( buffer, "-r " ) ; //strcat( buffer, "-batch " ) ;
	if( ReadParameter( INIT_SECTION, "pscpport", pscpport ) ) {
		if( !strcmp( pscpport,"*" ) ) sprintf( pscpport, "%d", cfg.port ) ;
		strcat( buffer, "-P " ) ;
		strcat( buffer, pscpport ) ;
		strcat( buffer, " " ) ;
		}
	
	if( cfg.sshprot == 2 ) {
		sprintf( b1, "-%d", cfg.sshprot ) ; strcat( buffer, b1 ); 
		strcat( buffer, " " ) ;
		}
	if( strlen( cfg.password ) > 0 ) {
		strcat( buffer, "-pw \"" ) ;
		MASKPASS(cfg.password); strcat( buffer, cfg.password ) ; MASKPASS(cfg.password);
		strcat( buffer, "\" " ) ;
		}
	if( strlen( cfg.keyfile.path ) > 0 ) {
		strcat( buffer, "-i \"" ) ;
		strcat( buffer, cfg.keyfile.path ) ;
		strcat( buffer, "\" " ) ;
		}
	strcat( buffer, "\"" ) ; 
	strcat( buffer, cfg.username ) ; strcat( buffer, "@" ) ;
	strcat( buffer, cfg.host ) ; strcat( buffer, ":" ) ; 
	
	if( filename[0]=='/' ) { strcat(buffer, filename ) ; }
	else {
		if( (directory!=NULL) && (strlen(directory)>0) && (strlen(filename)>0) ) {
			strcat( buffer, directory ) ; strcat( buffer, "/" ) ; strcat( buffer, filename ) ;
			}
		else if( (directory!=NULL) && (strlen(directory)>0) ) 
			{ strcat(buffer, directory ) ; strcat( buffer, "/*") ; }
		else { strcat(buffer, filename ) ; }
		}
	strcat( buffer, "\" \"" ) ; strcat( buffer, dir ) ; strcat( buffer, "\"" ) ;
	//strcat( buffer, " > kitty.log 2>&1" ) ; //if( !system( buffer ) ) unlink( "kitty.log" ) ;

	chdir( InitialDirectory ) ;

	if( debug_flag ) logevent(NULL, buffer);
	if( system( buffer ) ) MessageBox( NULL, buffer, "Transfer problem", MB_OK|MB_ICONERROR  ) ;
	//debug_log("%s\n",buffer);//MessageBox( NULL, buffer, "Info",MB_OK );
	}

// Reception d'un fichier par SCP
void GetFile( HWND hwnd ) {
	char buffer[4096]="", b1[256], *pst ;
	char dir[4096], pscppath[4096]="", pscpport[16]="22" ;
	
	if( cfg.protocol != PROT_SSH ) {
		MessageBox( hwnd, "This function is only available with SSH connections.", "Error", MB_OK|MB_ICONERROR ) ;
		return ;
		}

	if( PSCPPath==NULL ) {
		if( IniFileFlag == SAVEMODE_REG ) return ;
		else if( !SearchPSCP() ) return ;
		}
	if( !existfile( PSCPPath ) ) {
		if( IniFileFlag == SAVEMODE_REG ) return ;
		else if( !SearchPSCP() ) return ;
		}
		
	if( !GetShortPathName( PSCPPath, pscppath, 4095 ) ) return ;

	if (!IsClipboardFormatAvailable(CF_TEXT)) return ;
	
	if( OpenClipboard(NULL) ) {
		HGLOBAL hglb ;
		
		if( (hglb = GetClipboardData( CF_TEXT ) ) != NULL ) {
			if( ( pst = GlobalLock( hglb ) ) != NULL ) {
//sprintf(buffer,"#%s#%d",pst,strlen(pst));MessageBox(hwnd,buffer,"Info",MB_OK);
				while( (pst[strlen(pst)-1]=='\n')||(pst[strlen(pst)-1]=='\r')||(pst[strlen(pst)-1]==' ')||(pst[strlen(pst)-1]=='\t') ) pst[strlen(pst)-1]='\0' ;
//sprintf(buffer,"#%s#%d",pst,strlen(pst));MessageBox(hwnd,buffer,"Info",MB_OK);
				strcpy( buffer, "" ) ;
				if( strlen( pst ) > 0 ) {
				if( ReadParameter( INIT_SECTION, "downloaddir", dir ) ) {
					if( chdir( dir ) == -1 )
					strcpy( dir, InitialDirectory ) ;
					}
				else if( OpenDirName( hwnd, dir ) ) {
					if( chdir( dir ) == -1 ) { GlobalUnlock( hglb ) ; CloseClipboard(); return ; }
					//strcpy( dir, InitialDirectory ) ;
					}
				else { return ; }
				//else { strcpy( dir, InitialDirectory ) ; }

				sprintf( buffer, "start %s ", pscppath ) ;
				if( cfg.sshprot == 2 ) {
					sprintf( b1, "-%d", cfg.sshprot ) ; strcat( buffer, b1 ); 
					strcat( buffer, " " ) ;
					}
				if( ReadParameter( INIT_SECTION, "pscpport", pscpport ) ) {
					if( !strcmp( pscpport,"*" ) ) sprintf( pscpport, "%d", cfg.port ) ;
					strcat( buffer, "-P " ) ;
					strcat( buffer, pscpport ) ;
					strcat( buffer, " " ) ;
					}
				if( strlen( cfg.password ) > 0 ) {
					strcat( buffer, "-pw " ) ;
					MASKPASS(cfg.password); strcat( buffer, cfg.password ) ; MASKPASS(cfg.password);
					strcat( buffer, " " ) ;
					}
				if( strlen( cfg.keyfile.path ) > 0 ) {
					strcat( buffer, "-i \"" ) ;
					strcat( buffer, cfg.keyfile.path ) ;
					strcat( buffer, "\" " ) ;
					}
				strcat( buffer, cfg.username ) ; strcat( buffer, "@" ) ;
				strcat( buffer, cfg.host ) ; strcat( buffer, ":" ) ;
				strcat( buffer, pst ) ; strcat( buffer, " \"" ) ;
				strcat( buffer, dir ) ; strcat( buffer, "\"" ) ;
				//strcat( buffer, " > kitty.log 2>&1" ) ;
				}
				GlobalUnlock( hglb ) ;
				}
			}

		CloseClipboard();
		}
	if( strlen( buffer ) > 0 ) {
		chdir( InitialDirectory ) ;
		if( debug_flag ) logevent(NULL, buffer);
		if( system( buffer ) ) MessageBox( NULL, buffer, "Transfer problem", MB_OK|MB_ICONERROR  ) ;
		//if( !system( buffer ) ) unlink( "kitty.log" ) ;
		}
	}
	
// Grep
#ifdef URLPORT
const char* urlhack_default_regex = "(^|[ ]|((https?|ftp):\\/\\/))(([0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9]+)|localhost|([a-zA-Z0-9\\-]+\\.)*[a-zA-Z0-9\\-]+\\.(com|net|org|info|biz|gov|name|edu|[a-zA-Z][a-zA-Z]))(:[0-9]+)?((\\/|\\?)[^ \"]*[^ ,;\\.:\">)])?";
int grep( const char * pattern, const char * str ) {
	int return_code = 1 ;
	regex_t preg ;
 
	if( (return_code = regcomp (&preg, pattern, REG_NOSUB | REG_EXTENDED ) ) == 0 ) {
		return_code = regexec( &preg, str, 0, NULL, 0 ) ;
		regfree( &preg ) ;
		}
	return !return_code ;
	}
	
// test un lien
void URLclick( HWND hwnd ) {
	char buffer[4096]="", * pst = NULL ;
	
	if (!IsClipboardFormatAvailable(CF_TEXT)) return ;
	
	if( OpenClipboard(NULL) ) {
		HGLOBAL hglb ;
		
		if( (hglb = GetClipboardData( CF_TEXT ) ) != NULL ) {
			if( ( pst = GlobalLock( hglb ) ) != NULL ) {
				sprintf( buffer, "%s", pst ) ;
				GlobalUnlock( hglb ) ;
				if( grep( urlhack_default_regex, (const char*)buffer ) ) 
					ShellExecute(hwnd,"open",buffer,NULL,NULL,SW_SHOWNORMAL) ;
				}
			}

		CloseClipboard();
		}
	}
#endif

// Lancement d'un commande locale (Lancement Internet Explorer par exemple)
void RunCmd( HWND hwnd ) {
	char buffer[4096]="", * pst = NULL ;
	
	if (!IsClipboardFormatAvailable(CF_TEXT)) return ;
	
	if( OpenClipboard(NULL) ) {
		HGLOBAL hglb ;
		
		if( (hglb = GetClipboardData( CF_TEXT ) ) != NULL ) {
			if( ( pst = GlobalLock( hglb ) ) != NULL ) {
				sprintf( buffer, "%s", pst ) ;
				GlobalUnlock( hglb ) ;
				}
			}

		CloseClipboard();
		}

	if( strlen( buffer ) > 0 ) {
		chdir( InitialDirectory ) ;
		//system( buffer ) ;
		STARTUPINFO si ;
  		PROCESS_INFORMATION pi ;
		ZeroMemory( &si, sizeof(si) );
		si.cb = sizeof(si);
		ZeroMemory( &pi, sizeof(pi) );
		if( !CreateProcess(NULL, buffer, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi) ) {
			ShellExecute(hwnd, "open", buffer,0, 0, SW_SHOWDEFAULT);
			}
		}
	}
	
// Gestion de commandes à dstance
static char * RemotePath = NULL ;
void StartWinSCP( HWND hwnd, char * directory ) ;
/* Execution de commande en local
cmd()
{
if [ $# -eq 0 ] ; then echo "Usage: cmd command" ; return 0 ; fi
printf "\033]0;__cm:"$*"\007"
}
*/
int ManageLocalCmd( HWND hwnd, char * cmd ) {
	char buffer[1024] = "", title[1024] = "" ;
	if( cmd == NULL ) return 0 ; if( (cmd[2] != ':')&&(cmd[2] != '\0') ) return 0 ;
	if( (cmd[2] == ':')&&( strlen( cmd ) <= 3 ) ) return 0 ;
	if( (cmd[0]=='p')&&(cmd[1]=='w')&&(cmd[2]==':') ) { // nouveau remote directory
		if( RemotePath!= NULL ) free( RemotePath ) ;
		RemotePath = (char*) malloc( strlen( cmd ) - 2 ) ;
		strcpy( RemotePath, cmd+3 ) ;
		return 1 ;
		}
	else if( (cmd[0]=='r')&&(cmd[1]=='v')&&(cmd[2]==':') ) { // Reception d'un fichiers
		GetOneFile( hwnd, RemotePath, cmd+3 ) ;
		return 1 ;
		}
	else if( (cmd[0]=='t')&&(cmd[1]=='i')&&(cmd[2]=='\0') ) { // Récupération du titre de la fenêtres
		GetWindowText( hwnd, buffer, 1024 ) ;
		sprintf( title, "printf \"\\033]0;%s\\007\"\n", buffer ) ;
		SendStrToTerminal( title, strlen(title) ) ;
		return 1 ;
		}
	else if( (cmd[0]=='i')&&(cmd[1]=='n')&&(cmd[2]==':') ) { // Affiche d'information dans le log
		logevent(NULL,cmd+3);
		return 1 ;
		}
	else if( (cmd[0]=='w')&&(cmd[1]=='s')&&(cmd[2]==':') ) { // Lance WinSCP dans un repertoire donne
		if( RemotePath!= NULL ) free( RemotePath ) ;
		RemotePath = (char*) malloc( strlen( cmd ) - 2 ) ;
		strcpy( RemotePath, cmd+3 ) ;
		StartWinSCP( hwnd, RemotePath ) ;
		free( RemotePath ) ;
		return 1 ;
		}
	else if( (cmd[0]=='c')&&(cmd[1]=='m')&&(cmd[2]==':') ) { // Execute une commande externe
		RunCommand( hwnd, cmd+3 ) ;
		return 1 ;
		}
	return 0 ;
	}

// resize en convertissant en nombre de lignes et colonnes
void resize( int height, int width ) { 
	int w,h;

	if( width!=-1 ) {
		prev_rows = term->rows;
		prev_cols = term->cols;

		w = width / font_width ;
		if (w < 1) w = 1;
		h = height / font_height ;
		if (h < 1) h = 1;
		}
	else {
		h = prev_rows ;
		w = prev_cols ;
		}

	term_size(term, h, w, cfg.savelines) ; 
	reset_window(0);

	w = (width-cfg.window_border*2) / font_width ;
	if (w < 1) w = 1;
	h = (height-cfg.window_border*2) / font_height ;
	if (h < 1) h = 1;
	cfg.height = h ;
	cfg.width = w ;
	}

// Recupere les coordonnees de la fenetre
void GetWindowCoord( HWND hwnd ) {
	RECT rc ;
	GetWindowRect( hwnd, &rc ) ;

	cfg.xpos = rc.left ;
	cfg.ypos = rc.top ;

	cfg.windowstate = IsZoomed( hwnd ) ;
	
	//cfg.width = rc.right-rc.left ;
	//cfg.height = rc.bottom-rc.top ;
	}

// Sauve les coordonnees de la fenetre
void SaveWindowCoord( Config cfg ) {
	char key[1024], session[1024] ;
	if( cfg.saveonexit )
	if( cfg.sessionname!= NULL )
	if( strlen( cfg.sessionname ) > 0 ) {
		if( IniFileFlag == SAVEMODE_REG ) {
			mungestr( cfg.sessionname, session ) ;
			sprintf( key, "%s\\Sessions\\%s", TEXT(PUTTY_REG_POS), session ) ;
			RegTestOrCreateDWORD( HKEY_CURRENT_USER, key, "TermXPos", cfg.xpos ) ;
			RegTestOrCreateDWORD( HKEY_CURRENT_USER, key, "TermYPos", cfg.ypos ) ;
			RegTestOrCreateDWORD( HKEY_CURRENT_USER, key, "TermWidth", cfg.width ) ;
			RegTestOrCreateDWORD( HKEY_CURRENT_USER, key, "TermHeight", cfg.height ) ;
			RegTestOrCreateDWORD( HKEY_CURRENT_USER, key, "WindowState", cfg.windowstate ) ;
			RegTestOrCreateDWORD( HKEY_CURRENT_USER, key, "TransparencyValue", cfg.transparencynumber ) ;
			}
		else { 
			int xpos=cfg.xpos, ypos=cfg.ypos, width=cfg.width, height=cfg.height, windowstate=cfg.windowstate, transparency=cfg.transparencynumber ;
			load_settings( cfg.sessionname, &cfg ) ;
			cfg.xpos=xpos ; cfg.ypos=ypos ; cfg.width=width ; cfg.height=height ; cfg.windowstate=windowstate ; cfg.transparencynumber=transparency ;
			save_settings( cfg.sessionname, &cfg ) ; 
			}
		}
	}
	
// Gestion de la fonction winrol
void ManageWinrol( HWND hwnd ) {
	RECT rcClient ;
	int mode = -1 ;
	
	if( cfg.resize_action==RESIZE_DISABLED ) {
	    	mode = GetWindowLong(hwnd, GWL_STYLE) ;
		cfg.resize_action = RESIZE_TERM ;
		SetWindowLongPtr( hwnd, GWL_STYLE, mode|WS_THICKFRAME|WS_MAXIMIZEBOX ) ;
		SetWindowPos( hwnd, 0, 0, 0, 0, 0, SWP_FRAMECHANGED|SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER ) ;
		}

	if( WinHeight == -1 ) {
		GetWindowRect(hwnd, &rcClient) ;
		WinHeight  = rcClient.bottom-rcClient.top ;
		resize(0, rcClient.right-rcClient.left) ;
		MoveWindow( hwnd, rcClient.left, rcClient.top, rcClient.right-rcClient.left, 0, TRUE ) ;
    		}
	else {
		GetWindowRect(hwnd, &rcClient) ;
		rcClient.bottom = rcClient.top + WinHeight ;
		resize(WinHeight, -1) ;
		MoveWindow( hwnd, rcClient.left, rcClient.top, rcClient.right-rcClient.left, WinHeight, TRUE ) ;
		WinHeight = -1 ;
		}
		
	if( mode != -1 ) {
	    //winmode &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
		SetWindowLongPtr(hwnd, GWL_STYLE, mode ) ;
		SetWindowPos( hwnd, 0, 0, 0, 0, 0, SWP_FRAMECHANGED|SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER ) ;
		cfg.resize_action = RESIZE_DISABLED ;
		}

	InvalidateRect(hwnd, NULL, TRUE);
	}
	
#if (defined IMAGEPORT) && (!defined FDJ)
BOOL load_bg_bmp() ;
void clean_bg( void ) ;
void RedrawBackground( HWND hwnd ) ;
#endif

void RefreshBackground( HWND hwnd ) {
#if (defined IMAGEPORT) && (!defined FDJ)
	if( BackgroundImageFlag ) RedrawBackground( hwnd ) ; 
	else
#endif
	InvalidateRect(hwnd, NULL, TRUE) ;
	}

#if (defined IMAGEPORT) && (!defined FDJ)
/* Changement du fond d'ecran */
int GetExt( const char * filename, char * ext) {
	int i;
	strcpy( ext, "" ) ;
	if( filename==NULL ) return 0;
	if( strlen(filename)<=0 ) return 0;
	for( i=(strlen(filename)-1) ; i>=0 ; i-- ) 
		if( filename[i]=='.' ) strcpy( ext, filename+i+1 ) ;
	if( i<0 ) return 0;
	return 1;
	}

int PreviousBgImage( HWND hwnd ) {
	char buffer[1024], basename[1024], ext[10], previous[1024]="" ;
	int i ;
	DIR * dir ;
	struct dirent * de ;

	strcpy( basename, cfg.bg_image_filename.path ) ;

	for( i=(strlen(basename)-1) ; i>=0 ; i-- ) 
		if( (basename[i]=='\\')||(basename[i]=='/') ) { basename[i]='\0' ; break ; }
	if( i<0 ) strcpy( basename, ".") ;

	if( ( dir = opendir( basename ) ) == NULL ) { return 0 ; }
	
	while( ( de = readdir(dir) ) != NULL ) {
		if( strcmp(de->d_name,".") && strcmp(de->d_name,"..") ) {
			sprintf( buffer,"%s\\%s", basename, de->d_name ) ;
			if( !(GetFileAttributes( buffer ) & FILE_ATTRIBUTE_DIRECTORY) ) {
				if( !strcmp(buffer, cfg.bg_image_filename.path ) )
					if( strcmp( previous, "" ) ) break ;
		
				GetExt( de->d_name, ext ) ;
				if( (!stricmp(ext,"BMP"))||(!stricmp(ext,"JPG"))||(!stricmp(ext,"JPEG"))) 
					{ sprintf( previous,"%s\\%s", basename, de->d_name ) ; }
				}
			}
		}
	if( strcmp( previous, "" ) ){
		strcpy( cfg.bg_image_filename.path, previous ) ;
		RefreshBackground( hwnd ) ;
		}
	return 1 ;
	}

int NextBgImage( HWND hwnd ) {
	char buffer[1024], basename[1024], ext[10] ;
	int i ;
	DIR * dir ;
	struct dirent * de ;

	strcpy( basename, cfg.bg_image_filename.path ) ;

	for( i=(strlen(basename)-1) ; i>=0 ; i-- ) 
		if( (basename[i]=='\\')||(basename[i]=='/') ) { basename[i]='\0' ; break ; }
	if( i<0 ) strcpy( basename, ".") ;

	if( ( dir = opendir( basename ) ) == NULL ) { return 0 ; }
	
	while( ( de = readdir(dir) ) != NULL ) {
		GetExt( de->d_name, ext ) ;

		if( strcmp(de->d_name,".") && strcmp(de->d_name,"..") 
			&& ( (!stricmp(ext,"BMP"))||(!stricmp(ext,"JPG"))||(!stricmp(ext,"JPEG"))) 
			) {
			sprintf( buffer,"%s\\%s", basename, de->d_name ) ;
			if( !(GetFileAttributes( buffer ) & FILE_ATTRIBUTE_DIRECTORY) ) {
				if( !stricmp( buffer, cfg.bg_image_filename.path ) ) {
					if( ( de = readdir(dir) ) != NULL ) 
						GetExt( de->d_name, ext ) ; 
					else 
						strcpy( ext, "" ) ;
						
					while( (de!=NULL)&&stricmp(ext,"BMP")&&stricmp(ext,"JPG")&&stricmp(ext,"JPEG") ) {
						if( ( de = readdir(dir) ) != NULL ) 
							GetExt( de->d_name, ext ) ; 
						else 
							strcpy( ext, "" ) ;
						}
					break ;
					}
				}
			}
		}
	if( de==NULL ) { rewinddir( dir ) ; do { de = readdir(dir) ; } while( (!strcmp(de->d_name,".")) || (!strcmp(de->d_name,"..")) ) ; }
	if( de!=NULL ) GetExt( de->d_name, ext ) ; else strcpy( ext, "" ) ;
	if( de!=NULL )
	while( (de!=NULL)&&stricmp(ext,"BMP")&&stricmp(ext,"JPG")&&stricmp(ext,"JPEG") ) {
		if( ( de = readdir(dir) ) != NULL ) GetExt( de->d_name, ext ) ; else { strcpy( ext, "" ) ; break ; }
		}
	if( de != NULL  ) {
		sprintf( buffer,"%s\\%s", basename, de->d_name ) ;
		strcpy( cfg.bg_image_filename.path, buffer ) ;
		RefreshBackground( hwnd );
		}
	else { closedir(dir) ; return 0 ; }

	closedir( dir ) ;
	return 1 ;
	}
#endif
	
// Boîte de dialogue d'information
static LRESULT CALLBACK InfoCallBack( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam ) {
	switch(message) {
		case WM_INITDIALOG: SetWindowText( GetDlgItem(hwnd,IDC_RESULT), "" ) ; break;
		case WM_COMMAND:
			if( LOWORD(wParam) == 1001 ) {
				SetWindowText( GetDlgItem(hwnd,IDC_RESULT), (char*)lParam ) ;
				}

			if (LOWORD(wParam) == IDCANCEL)
			{
				EndDialog(hwnd, LOWORD(0));
			}
			break;
		case WM_CLOSE:
			EndDialog(hwnd, LOWORD(0));
			break;

		return DefWindowProc (hwnd, message, wParam, lParam);
		}
	return 0;
	}
	
HWND InfoBox( HINSTANCE hInstance, HWND hwnd ) {
	HWND hdlg = CreateDialog( hInstance, (LPCTSTR)120, hwnd, (DLGPROC)InfoCallBack ) ;
	return hdlg ;
	}
	
void InfoBoxSetText( HWND hwnd, char * st ) { SendMessage( hwnd, WM_COMMAND, 1001, (LPARAM) st ) ; SendMessage( hwnd, WM_PAINT, 0,0 ) ; }

void InfoBoxClose( HWND hwnd ) { EndDialog(hwnd, LOWORD(0)) ; DestroyWindow( hwnd ) ; }

//CallBack du dialog InputBox
HWND MainHwnd ;
static int InputBox_Flag = 0 ;

static LRESULT CALLBACK InputCallBack (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam ) {
	HWND handle;
	switch (message) {
		case WM_INITDIALOG:
			handle = GetDlgItem(hwnd,IDC_RESULT);
			if( InputBoxResult == NULL ) SetWindowText(handle,"");
			else SetWindowText( handle, InputBoxResult ) ;
			break;

		case WM_COMMAND:
			if (LOWORD(wParam) == IDOK)
			{
				handle = GetDlgItem(hwnd,IDC_RESULT);
				if( InputBoxResult != NULL ) { free( InputBoxResult ) ; InputBoxResult = NULL ; }
				size_t length = GetWindowTextLength( handle ) ;
				InputBoxResult = (char*) malloc( length + 10 ) ;
				GetWindowText(handle,InputBoxResult,length+1);
				//EndDialog(hwnd, LOWORD(1));
				if( !InternalCommand( hwnd, InputBoxResult ) )
					SendKeyboardPlus( MainHwnd, InputBoxResult );
				SetWindowText(handle,"") ;
				
			}
			if (LOWORD(wParam) == IDCANCEL)
			{
				if( InputBoxResult != NULL ) { free( InputBoxResult ) ; InputBoxResult = NULL ; }
				EndDialog(hwnd, LOWORD(0));
			}
			
			break;

		case WM_CLOSE:
			EndDialog(hwnd, LOWORD(0));
			break;

		return DefWindowProc (hwnd, message, wParam, lParam);
	}
	return 0;
}

static LRESULT CALLBACK InputCallBackPassword(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam){
	HWND handle;
	switch (message) {
		case WM_INITDIALOG:
			handle = GetDlgItem(hwnd,IDC_RESULT);
			if( InputBoxResult == NULL ) SetWindowText(handle,"");
			else SetWindowText( handle, InputBoxResult ) ;
			break;

		case WM_COMMAND:
			if (LOWORD(wParam) == IDOK)
			{
				handle = GetDlgItem(hwnd,IDC_RESULT);
				if( InputBoxResult != NULL ) { free( InputBoxResult ) ; InputBoxResult = NULL ; }
				size_t length = GetWindowTextLength( handle ) ;
				InputBoxResult = (char*) malloc( length + 10 ) ;
				GetWindowText(handle,InputBoxResult,length+1);
				EndDialog(hwnd, LOWORD(0));
			}
			if (LOWORD(wParam) == IDCANCEL)
			{
				if( InputBoxResult != NULL ) { free( InputBoxResult ) ; InputBoxResult = NULL ; }
				EndDialog(hwnd, LOWORD(0));
			}
			
			break;

		case WM_CLOSE:
			EndDialog(hwnd, LOWORD(0));
			break;

		return DefWindowProc (hwnd, message, wParam, lParam);
	}
	return 0;
}

// Procédure spécifique à la editbox multiligne (SHIFT+F8)
FARPROC lpfnOldEditProc ;
BOOL FAR PASCAL EditMultilineCallBack(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
	char buffer[4096], key_name[1024] ;
	switch (message) {
		case WM_KEYDOWN:
			if( (wParam==VK_RETURN) && (GetKeyState( VK_SHIFT )& 0x8000) ){
				SendMessage(GetParent(hwnd),WM_COMMAND,IDB_OK,0 ) ;
				return 0;
				}
			else if( (wParam==VK_F12) && (GetKeyState( VK_SHIFT )& 0x8000) ){
				GetWindowText( hwnd, buffer, 4096 ) ;
				cryptstring( buffer, MASTER_PASSWORD ) ;
				SetWindowText( hwnd, buffer ) ;
				return 0 ;
				}
			else if( (wParam==VK_F11) && (GetKeyState( VK_SHIFT )& 0x8000) ){
				GetWindowText( hwnd, buffer, 4096 ) ;
				decryptstring( buffer, MASTER_PASSWORD ) ;
				SetWindowText( hwnd, buffer ) ;
				return 0 ;
				}
			else
				return CallWindowProc((WNDPROC)lpfnOldEditProc, hwnd, message, wParam, lParam);
			break;
		case WM_KEYUP:
		case WM_CHAR:
		case WM_SYSKEYDOWN:
		case WM_SYSKEYUP:
			if( (wParam==VK_RETURN) && (GetKeyState( VK_SHIFT )& 0x8000) )
				return 0;
			else if( (wParam==VK_F2) && (GetKeyState( VK_SHIFT )& 0x8000) ) { // Charge une Notes
				sprintf( key_name, "%s\\Sessions\\%s", TEXT(PUTTY_REG_POS), cfg.sessionname ) ;
				if( GetValueData(HKEY_CURRENT_USER, key_name, "Notes", buffer) != NULL ) {
					if( GetWindowTextLength(hwnd) > 0 ) 
						if( MessageBox(hwnd, "Are you sure you want to load Notes\nand erase this edit box ?","Load Warning", MB_YESNO|MB_ICONWARNING ) != IDYES ) break ;
					SetWindowText( hwnd, buffer ) ;
					}
				}
			else if( (wParam==VK_F3) && (GetKeyState( VK_SHIFT )& 0x8000) ) { // Sauve une Notes
				GetSessionField( cfg.sessionname, cfg.folder, "Notes", buffer ) ;
				if( strlen( buffer ) > 0 ) 
					if( MessageBox(hwnd, "Are you sure you want to save Edit box\ninto Notes registry ?","Save Warning", MB_YESNO|MB_ICONWARNING ) != IDYES ) break ;
				GetWindowText( hwnd, buffer, 4096 ) ;
				sprintf( key_name, "%s\\Sessions\\%s", TEXT(PUTTY_REG_POS), cfg.sessionname ) ;
				RegTestOrCreate( HKEY_CURRENT_USER, key_name, "Notes", buffer ) ;
				}
			else 
				return CallWindowProc((WNDPROC)lpfnOldEditProc, hwnd, message, wParam, lParam);	
			break ;
            	default:
			return CallWindowProc((WNDPROC)lpfnOldEditProc, hwnd, message, wParam, lParam);
			break;
		}
	return TRUE ;
	}

static int EditReadOnly = 0 ;
#ifndef GWL_WNDPROC
#define GWL_WNDPROC GWLP_WNDPROC
#endif
static LRESULT CALLBACK InputMultilineCallBack (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	HWND handle;

	switch (message)
	{
		case WM_INITDIALOG: {
			char * buffer ;
			buffer=(char*)malloc(1024);
			sprintf( buffer, "%s - Text input", cfg.wintitle ) ;
			SetWindowText( hwnd, buffer ) ;
			free(buffer);
			handle = GetDlgItem(hwnd,IDC_RESULT) ;
			if( EditReadOnly ) SendMessage(handle, EM_SETREADONLY, 1, 0);
			if( InputBoxResult == NULL ) SetWindowText(handle,"");
			else {
				int i,j=0;
				buffer=(char*)malloc(2*strlen(InputBoxResult)+10);
				for( i=0; i<=strlen(InputBoxResult);i++ ) {
					if( (InputBoxResult[i]=='\n') && (buffer[j-1]!='\r') ) 
						{ buffer[j]='\r';j++;}
					buffer[j]=InputBoxResult[i];
					j++;
					}
				SetWindowText( handle, buffer ) ;
				free(buffer);
				}
			
			FARPROC lpfnSubClassProc = MakeProcInstance( EditMultilineCallBack, hInst );
			if( lpfnSubClassProc )
				lpfnOldEditProc = (FARPROC)SetWindowLong( handle, GWL_WNDPROC, (DWORD)(FARPROC)lpfnSubClassProc );
			}
			break;
		case WM_COMMAND:
			if (LOWORD(wParam) == IDB_OK) {
				handle = GetDlgItem(hwnd,IDC_RESULT);
				if( InputBoxResult != NULL ) { free( InputBoxResult ) ; InputBoxResult = NULL ; }
				size_t length = GetWindowTextLength( handle ) ;
				InputBoxResult = (char*) malloc( length + 10 ) ;

				GetWindowText(handle,InputBoxResult,length+1);

				// Si il y un texte selectionné, on ne récupère que celui-ci
				DWORD result = SendMessage( handle, EM_GETSEL, (WPARAM)0, (LPARAM)NULL );
				if( LOWORD(result) != HIWORD(result) ) {
					int i ;
					InputBoxResult[HIWORD(result)] = '\0' ;
					if( LOWORD(result) > 0 ) 
						for( i=0 ; i<=(HIWORD(result)-LOWORD(result)) ; i++ )
						InputBoxResult[i]=InputBoxResult[i+LOWORD(result)];
					}
				if( InputBoxResult[strlen(InputBoxResult)-1] != '\n' ) 
					strcat( InputBoxResult, "\n" ) ;

				SendKeyboard( MainHwnd, InputBoxResult ) ;
				ShowWindow( MainHwnd, SW_RESTORE ) ;
				BringWindowToTop( MainHwnd ) ;
				SetFocus( handle ) ;
				}
			else if (LOWORD(wParam) == IDCANCEL) {
				if( InputBoxResult != NULL ) { free( InputBoxResult ) ; InputBoxResult = NULL ; }
				EditReadOnly = 0 ;
				EndDialog(hwnd, LOWORD(0));
				}
			else if( HIWORD( wParam ) == EN_SETFOCUS ) {
				ShowWindow( MainHwnd, SW_SHOWNOACTIVATE ) ;
				DefWindowProc (hwnd, message, wParam, lParam) ;
				}
			break;
		case WM_CHAR: {
			DefWindowProc (hwnd, message, wParam, lParam) ;
			}
			break ;
		case WM_SIZE: {
			int h = HIWORD(lParam),w = LOWORD(lParam) ;
			handle = GetDlgItem( hwnd, IDC_RESULT ) ;
			SetWindowPos( handle, HWND_TOP, 7, 35, w-15, h-43, 0 ) ;
			DefWindowProc (hwnd, message, wParam, lParam) ;
			} 
			break ;
		case WM_CLOSE:
			EditReadOnly = 0 ;
			EndDialog(hwnd, LOWORD(0));
			break;
		
		return DefWindowProc(hwnd, message, wParam, lParam);
	}
	return 0;
}

char * InputBox( HINSTANCE hInstance, HWND hwnd ) {
	if( InputBoxResult != NULL ) { free( InputBoxResult ) ; InputBoxResult = NULL ; }
	DialogBox(hInstance, (LPCTSTR)114, hwnd, (DLGPROC)InputCallBack) ;
	return InputBoxResult ;
	}

char * InputBoxMultiline( HINSTANCE hInstance, HWND hwnd ) {
	if( InputBoxResult != NULL ) { free( InputBoxResult ) ; InputBoxResult = NULL ; }
	
	if( IsClipboardFormatAvailable(CF_TEXT) ) {
		char * pst = NULL ;
		if( OpenClipboard(NULL) ) {
			HGLOBAL hglb ;
			if( (hglb = GetClipboardData( CF_TEXT ) ) != NULL ) {
				if( ( pst = GlobalLock( hglb ) ) != NULL ) {
					InputBoxResult = (char*) malloc( strlen( pst ) +1 ) ;
					strcpy( InputBoxResult, pst ) ;
					GlobalUnlock( hglb ) ;
					}
				}
			CloseClipboard();
			}
		}

	DialogBox(hInstance, (LPCTSTR)115, NULL, (DLGPROC)InputMultilineCallBack) ;
	return InputBoxResult ;
	}
	
char * InputBoxPassword( HINSTANCE hInstance, HWND hwnd ) {
	if( InputBoxResult != NULL ) { free( InputBoxResult ) ; InputBoxResult = NULL ; }
	DialogBox(hInstance, (LPCTSTR)116, hwnd, (DLGPROC)InputCallBackPassword) ;
	return InputBoxResult ;
	}

void GetAndSendLine( HWND hwnd ) {
	if( InputBox_Flag == 1 ) return ;
	InputBox_Flag = 1 ;
	InputBox( hinst, hwnd ) ; // Essayer avec GetModuleHandle(NULL)
	InputBox_Flag = 0 ;
	}
	
void GetAndSendMultiLine( HWND hwnd ) {
	if( InputBox_Flag == 1 ) return ;

	InputBox_Flag = 1 ;
	InputBoxMultiline( hinst, hwnd ) ;
	InputBox_Flag = 0 ;
	}

void GetAndSendLinePassword( HWND hwnd ) {
	if( InputBox_Flag == 1 ) return ;
	InputBox_Flag = 1 ;
	InputBoxPassword( hinst, hwnd ) ; // Essayer avec GetModuleHandle(NULL)
	InputBox_Flag = 0 ;
	}

void routine_inputbox( void * phwnd ) { 
	GetAndSendLine( MainHwnd ) ;
	}

void routine_inputbox_multiline( void * phwnd ) { 
	GetAndSendMultiLine( MainHwnd ) ;
	}

void routine_inputbox_password( void * phwnd ) { 
	GetAndSendLinePassword( MainHwnd ) ;
	}

// Demarre le timer d'autocommand à la connexion
void CreateTimerInit( void ) {
	SetTimer(MainHwnd, TIMER_INIT, (int)(init_delay*1000), NULL) ; 
	}

// Positionne le repertoire ou se trouve la configuration 
void SetConfigDirectory( const char * Directory ) {
	char *buf ;
	if( ConfigDirectory != NULL ) { free( ConfigDirectory ) ; ConfigDirectory = NULL ; }
	if( (Directory!=NULL)&&(strlen(Directory)>0) ) { 
		if( existdirectory(Directory) ) {
			ConfigDirectory = (char*)malloc( strlen(Directory)+1 ) ; 
			strcpy( ConfigDirectory, Directory ) ; 
			}
		else {
			buf=(char*)malloc(strlen(InitialDirectory)+strlen(Directory)+10) ;
			sprintf(buf,"%s\\%s",InitialDirectory,Directory) ;
			if( existdirectory(buf) ) {
				ConfigDirectory = (char*)malloc( strlen(buf)+1 ) ; 
				strcpy( ConfigDirectory, buf ) ; 
				}
			free(buf);
			}
		}
	if( ConfigDirectory==NULL ) { 
		ConfigDirectory = (char*)malloc( strlen(InitialDirectory)+1 ) ; 
		strcpy( ConfigDirectory, InitialDirectory ) ; 
		}
	}
	
void GetInitialDirectory( char * InitialDirectory ) {
	int i ;
	if( GetModuleFileName( NULL, (LPTSTR)InitialDirectory, 4096 ) ) {
		if( strlen( InitialDirectory ) > 0 ) {
			i = strlen( InitialDirectory ) -1 ;
			do {
				if( InitialDirectory[i] == '\\' ) { InitialDirectory[i]='\0' ; i = 0 ; }
				i-- ;
				}
			while( i >= 0 ) ;
			}
		}
	else { strcpy( InitialDirectory, "" ) ; }
	
	SetConfigDirectory( InitialDirectory ) ;
	}
	
void GotoInitialDirectory( void ) { chdir( InitialDirectory ) ; }
void GotoConfigDirectory( void ) { if( ConfigDirectory!=NULL ) chdir( ConfigDirectory ) ; }

#define NB_MENU_MAX 1024
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
static char *SpecialMenu[NB_MENU_MAX] ;
int ReadSpecialMenu( HMENU menu, char * KeyName, int * nbitem, int separator ) {
	HKEY hKey ;
	HMENU SubMenu ;
	char buffer[4096], fullpath[1024], *p ;
	int i, nb ;
	int local_nb = 0 ;
	if( (IniFileFlag == SAVEMODE_REG)||(IniFileFlag == SAVEMODE_FILE) ) {
	if( RegOpenKeyEx( HKEY_CURRENT_USER, TEXT(KeyName), 0, KEY_READ, &hKey) == ERROR_SUCCESS ) {
		TCHAR achValue[MAX_VALUE_NAME], achClass[MAX_PATH] = TEXT("");
		DWORD    retCode,cchClassName=MAX_PATH,cSubKeys=0,cbMaxSubKey,cchMaxClass,cValues,cchMaxValue,cbMaxValueData,cbSecurityDescriptor;
		FILETIME ftLastWriteTime;

		retCode = RegQueryInfoKey(hKey,achClass,&cchClassName,NULL,&cSubKeys,&cbMaxSubKey,&cchMaxClass,&cValues,&cchMaxValue,&cbMaxValueData,&cbSecurityDescriptor,&ftLastWriteTime);
		nb = (*nbitem) ;

		if( cSubKeys>0 ) { // Récuperation des sous-menu
		for (i=0, retCode=ERROR_SUCCESS; (i<cSubKeys)&&(nb<NB_MENU_MAX); i++) {
			DWORD cchValue = MAX_VALUE_NAME; 
			DWORD dwDataSize=4096 ;
			unsigned char lpData[4096] ;
			dwDataSize = 4096 ;
			achValue[0] = '\0';

			if( RegEnumKeyEx(hKey, i, lpData, &cchValue, NULL, NULL, NULL, &ftLastWriteTime) == ERROR_SUCCESS ) {
				SubMenu = CreateMenu() ;
				sprintf( buffer, "%s\\%s", KeyName, lpData ) ;
				ReadSpecialMenu( SubMenu, buffer, nbitem, 0 ) ;
				unmungestr( lpData, buffer, MAX_PATH ) ;
				AppendMenu( menu, MF_POPUP, (UINT_PTR)SubMenu, buffer ) ;
				}
			}
		}
		
		nb = (*nbitem) ;
		
		if (cValues) { // Récuperation des item de menu
		if( separator ) AppendMenu( menu, MF_SEPARATOR, 0, 0 ) ;
		
		if( nb<NB_MENU_MAX )
	        for (i=0, retCode=ERROR_SUCCESS; (i<cValues)&&(nb<NB_MENU_MAX); i++) {
			DWORD cchValue = MAX_VALUE_NAME; 
			DWORD lpType,dwDataSize=4096 ;
			unsigned char lpData[4096] ;
			dwDataSize = 4096 ;
			achValue[0] = '\0';

			if( RegEnumValue(hKey,i,achValue,&cchValue,NULL,&lpType,lpData,&dwDataSize) == ERROR_SUCCESS ) {
				if( ShortcutsFlag ) {
					if( nb < 26 ) 
						sprintf( buffer, "%s\tCtrl+Shift+%c", achValue, ('A'+nb) ) ;
					else 
						sprintf( buffer, "%s", achValue ) ;
					}
				else
					sprintf( buffer, "%s", achValue ) ;
				AppendMenu(menu, MF_ENABLED, IDM_USERCMD+nb, buffer ) ;
				SpecialMenu[nb]=(char*)malloc( strlen( lpData ) + 1 ) ;
				strcpy( SpecialMenu[nb], lpData ) ;
				nb++ ;
				local_nb++ ;
				}
			}
    		}

		(*nbitem)=nb ;
		
		RegCloseKey( hKey ) ;
		}
		}
	else if( IniFileFlag == SAVEMODE_DIR ) {
		sprintf( fullpath, "%s\\%s", ConfigDirectory, KeyName ) ;
		DIR * dir ;
		struct dirent * de ;
		FILE *fp ;
		if( ( dir = opendir( fullpath ) ) != NULL ) {
			if( separator ) AppendMenu( menu, MF_SEPARATOR, 0, 0 ) ;
			nb = (*nbitem) ;
			while( ( de = readdir(dir) ) != NULL ) { // Recherche de sous-clé (répertoire)
				if( strcmp(de->d_name,".") && strcmp(de->d_name,"..") ) {
					sprintf( buffer, "%s\\%s", fullpath, de->d_name ) ;
					if( GetFileAttributes( buffer ) & FILE_ATTRIBUTE_DIRECTORY ) {
						SubMenu = CreateMenu() ;
						sprintf( buffer, "%s\\%s", KeyName, de->d_name ) ;
						ReadSpecialMenu( SubMenu, buffer, nbitem, 0 ) ;
						unmungestr( de->d_name, buffer, MAX_PATH ) ;
						AppendMenu( menu, MF_POPUP, (UINT_PTR)SubMenu, buffer ) ;
						}
					/*if( stat( buffer, &statBuf ) != -1 ) {
						if( ( statBuf.st_mode & S_IFMT) == S_IFDIR ) {
							sprintf( buffer, "%s\\%s", KeyName, de->d_name ) ;
							ReadSpecialMenu( menu, buffer, nbitem, separator ) ;
							}
						}*/
					}
				}
			rewinddir( dir ) ;

			nb = (*nbitem) ;
			while( ( de = readdir(dir) ) != NULL ) { // Recherche de clé
				if( strcmp(de->d_name,".") && strcmp(de->d_name,"..") ) {
					sprintf( buffer, "%s\\%s", fullpath, de->d_name ) ;
					if( !(GetFileAttributes( buffer ) & FILE_ATTRIBUTE_DIRECTORY) ) {
						if( ( fp=fopen(buffer,"rb")) != NULL ){
							while( fgets( buffer, 4096, fp )!=NULL ){
								while( (buffer[strlen(buffer)-1]=='\n')
									||(buffer[strlen(buffer)-1]=='\r') ) buffer[strlen(buffer)-1]='\0';
								if( buffer[strlen(buffer)-1]=='\\' ) {
									buffer[strlen(buffer)-1]='\0' ;
									
									if( (p=strstr(buffer,"\\"))!=NULL ){
										p[0]='\0';
				AppendMenu(menu, MF_ENABLED, IDM_USERCMD+nb, buffer ) ;
				SpecialMenu[nb]=(char*)malloc( strlen( p+1 ) + 1 ) ;
				strcpy( SpecialMenu[nb], p+1 ) ;
				nb++ ;
				local_nb++ ;
									}
									}
								}
							fclose(fp) ;
							}
						}
						/*
					if( stat( buffer, &statBuf ) != -1 ) {
						if( ( statBuf.st_mode & S_IFMT ) == S_IFREG ) {
							
							}
						}*/
					}
				}
			(*nbitem)=nb ;
			closedir( dir ) ;
			}
		}
	return local_nb ;
	}

void InitSpecialMenu( HMENU m, Config cfg ) {
	char KeyName[1024], buffer[1024] ;
	int nbitem = 0 ;

	HMENU menu ;
	menu = CreateMenu() ;
	
	if( IniFileFlag == SAVEMODE_DIR ) {
		strcpy( KeyName, "Commands" ) ;
		ReadSpecialMenu( menu, KeyName, &nbitem, 0 ) ;
		
		mungestr( cfg.folder, buffer ) ;
		sprintf( KeyName, "Folders\\%s\\Commands", buffer ) ;
		ReadSpecialMenu( menu, KeyName, &nbitem, 1 ) ;
		
		mungestr( cfg.sessionname, buffer ) ;
		sprintf( KeyName, "Sessions_Commands\\%s", buffer ) ;
		ReadSpecialMenu( menu, KeyName, &nbitem, 1 ) ;

		}
	else {
		sprintf( KeyName, "%s\\Commands", TEXT(PUTTY_REG_POS) ) ;
		ReadSpecialMenu( menu, KeyName, &nbitem, 0 ) ;
		
		mungestr( cfg.folder, buffer ) ;
		sprintf( KeyName, "%s\\Folders\\%s\\Commands", TEXT(PUTTY_REG_POS), buffer ) ;
		ReadSpecialMenu( menu, KeyName, &nbitem, 1 ) ;

		mungestr( cfg.sessionname, buffer ) ;
		sprintf( KeyName, "%s\\Sessions\\%s\\Commands", TEXT(PUTTY_REG_POS), buffer ) ;
		ReadSpecialMenu( menu, KeyName, &nbitem, 1 ) ;
		}

	if( GetMenuItemCount( menu ) > 0 )
		AppendMenu( m, MF_POPUP, (UINT_PTR)menu, "&User Command" ) ;

	}

void ManageSpecialCommand( HWND hwnd, int menunum, Config cfg ) {
	char buffer[4096] ;
	FILE *fp ;
	if( menunum < NB_MENU_MAX ) {
	if( SpecialMenu[menunum] != NULL ) 
		if( strlen( SpecialMenu[menunum] ) > 0 ) {
			if( ( fp=fopen( SpecialMenu[menunum], "r") ) != NULL ) {
				while( fgets( buffer, 4095, fp ) != NULL ) {
					SendKeyboardPlus( hwnd, buffer ) ;
					}
				fclose( fp ) ; 
				}
			else SendKeyboardPlus( hwnd, SpecialMenu[menunum] ) ;
			}
		}
	}


// Decompte du nombre de fenêtres en cours de KiTTY
static int NbWindows = 0 ;

BOOL CALLBACK EnumWindowsProc( HWND hwnd, LPARAM lParam ) {
	char buffer[256] ;
	GetClassName( hwnd, buffer, 256 ) ;
	
	if( (!strcmp( buffer, appname )) || (!strcmp( buffer, "PuTTYConfigBox" )) ) NbWindows++ ;
	
	return TRUE ;
	}

// Decompte le nombre de fenetre de la meme classe que KiTTY
int WindowsCount( HWND hwnd ) {
	char buffer[256] ;
	NbWindows = 0 ;
	
	if( GetClassName( hwnd, buffer, 256 ) == 0 ) NbWindows = 1 ;
	else {
		if( !strcmp( buffer, "" ) ) NbWindows = 1 ;
		}

	EnumWindows( EnumWindowsProc, 0 ) ;
	return NbWindows ;
	}

// Mettre la liste des port forward dans le presse-papier et l'afficher à l'écran
int ShowPortfwd( HWND hwnd ) {
	char pf[2048], *p ;
	int i = 0 ;
	p = cfg.portfwd ;
	while( (p[i]!='\0')||(p[i+1]!='\0') ) {
		if( p[i]=='\0' ) pf[i]='\n' ;
		else pf[i]=p[i] ;
		i++ ;
		}
	pf[i] = '\0';
	MessageBox( NULL, pf, "Port forwarding", MB_OK ) ;
	return SetTextToClipboard( pf ) ;
	}

// Procedures de generation du dump "memoire" (/savedump)
#include "kitty_savedump.c"

int InternalCommand( HWND hwnd, char * st ) {
	char buffer[1024] ;

	if( strstr( st, "/message " ) == st ) { MessageBox( hwnd, st+9, "Info", MB_OK ) ; return 1 ; }
	else if( !strcmp( st, "/copytoputty" ) ) {
		RegDelTree (HKEY_CURRENT_USER, "Software\\SimonTatham\\PuTTY\\Sessions" ) ;
		sprintf( buffer, "%s\\Sessions", PUTTY_REG_POS ) ;
		RegCopyTree( HKEY_CURRENT_USER, buffer, "Software\\SimonTatham\\PuTTY\\Sessions" ) ;
		sprintf( buffer, "%s\\SshHostKeys", PUTTY_REG_POS ) ;
		RegCopyTree( HKEY_CURRENT_USER, buffer, "Software\\SimonTatham\\PuTTY\\SshHostKeys" ) ;
		RegCleanPuTTY() ;
		return 1 ;
		}
	else if( !strcmp( st, "/copytokitty" ) ) 
		{ RegCopyTree( HKEY_CURRENT_USER, "Software\\SimonTatham\\PuTTY", PUTTY_REG_POS ) ; return 1 ; }
	else if( !strcmp( st, "/backgroundimage" ) ) { BackgroundImageFlag = abs( BackgroundImageFlag - 1 ) ; return 1 ; }
	else if( !strcmp( st, "/debug" ) ) { debug_flag = abs( debug_flag - 1 ) ; return 1 ; }
	else if( !strcmp( st, "/savedump" ) ) { SaveDump() ; return 1 ; }
	else if( !strcmp( st, "/savereg" ) ) { chdir( InitialDirectory ) ; SaveRegistryKey() ; return 1 ; }
	else if( !strcmp( st, "/savesessions" ) ) { 
		chdir( InitialDirectory ) ; 
		sprintf( buffer, "%s\\Sessions", PUTTY_REG_POS ) ;
		SaveRegistryKeyEx( HKEY_CURRENT_USER, buffer, "kitty.ses" ) ; 
		return 1 ; 
		}
	else if( !strcmp( st, "/loadinitscript" ) ) { ReadInitScript( NULL ) ; return 1 ; }
	else if( strstr( st, "/loadinitscript " ) == st ) { ReadInitScript( st+16 ) ; return 1 ; }
	else if( !strcmp( st, "/loadreg" ) ) { chdir( InitialDirectory ) ; LoadRegistryKey(NULL) ; return 1 ; }
	else if( !strcmp( st, "/delreg" ) ) { RegDelTree (HKEY_CURRENT_USER, TEXT(PUTTY_REG_PARENT)) ; return 1 ; }
	else if( strstr( st, "/delfolder " ) == st ) { StringList_Del( FolderList, st+11 ) ; return 1 ; }
	else if( !strcmp( st, "/noshortcuts" ) ) { ShortcutsFlag = 0 ; return 1 ; }
	else if( !strcmp( st, "/icon" ) ) { 
		//sprintf( buffer, "%d / %d = %d", IconeNum, NB_ICONES, NumberOfIcons );
		//MessageBox( hwnd, buffer, "info", MB_OK ) ;
		IconeFlag = abs( IconeFlag - 1 ) ; cfg.icone = IconeNum ; return 1 ; }
	else if( !strcmp( st, "/savemode" ) ) {
		//IniFileFlag = abs( IniFileFlag - 1 ) ;
		IniFileFlag++ ; if( IniFileFlag>SAVEMODE_DIR ) IniFileFlag = 0 ;
		if( IniFileFlag == SAVEMODE_REG )  {
			delINI( KittyIniFile, INIT_SECTION, "savemode" ) ;
			MessageBox( NULL, "Savemode is \"registry\"", "Info", MB_OK ) ;
			}
		else if( IniFileFlag == SAVEMODE_FILE ) {
			if(!NoKittyFileFlag) writeINI( KittyIniFile, INIT_SECTION, "savemode", "file" ) ;
			MessageBox( NULL, "Savemode is \"file\"", "Info", MB_OK ) ;
			}
		else if( IniFileFlag == SAVEMODE_DIR ) {
			delINI( KittyIniFile, INIT_SECTION, "savemode" ) ;
			MessageBox( NULL, "Savemode is \"dir\"", "Info", MB_OK ) ;
			}
		return 1 ;
		}
	else if( !strcmp( st, "/capslock" ) ) { CapsLockFlag = abs( CapsLockFlag - 1 ) ; return 1 ; }
	else if( !strcmp( st, "/size" ) ) { SizeFlag = abs( SizeFlag - 1 ) ; set_title( NULL, cfg.wintitle ) ; return 1 ; }
	else if( !strcmp( st, "/transparency" ) ) {
#ifndef NO_TRANSPARENCY
		if( TransparencyNumber == -1 ) {
			SetWindowLongPtr(MainHwnd, GWL_EXSTYLE, GetWindowLong(MainHwnd, GWL_EXSTYLE) | WS_EX_LAYERED ) ;
			SetWindowPos( MainHwnd, 0, 0, 0, 0, 0, SWP_FRAMECHANGED|SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER ) ;
			
			TransparencyNumber = 255 ;
			cfg.transparencynumber = 0 ;
			//SetLayeredWindowAttributes(hwnd, 0, TransparencyNumber, LWA_ALPHA) ;
			SetTransparency( MainHwnd, TransparencyNumber ) ;
			SetForegroundWindow( hwnd ) ;
			}
		else {
			SetTransparency( MainHwnd, 255 ) ;
			SetWindowLongPtr(MainHwnd, GWL_EXSTYLE, GetWindowLong(hwnd, GWL_EXSTYLE) & ~WS_EX_LAYERED ) ;
			RedrawWindow(MainHwnd, NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_FRAME | RDW_ALLCHILDREN);
			SetWindowPos( MainHwnd, 0, 0, 0, 0, 0, SWP_FRAMECHANGED|SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER ) ;
			SetForegroundWindow( hwnd ) ;
			TransparencyNumber = -1 ;
			cfg.transparencynumber = -1 ;
			}
#endif
		return 1 ;
		}
	else if( !strcmp( st, "/bcdelay" ) ) { between_char_delay=3 ; return 1 ; }
	else if( strstr( st, "/bcdelay " ) == st ) { between_char_delay=atoi( st+9 ) ; return 1 ; }
	else if( strstr( st, "/title " ) == st ) { set_title( NULL, st+7 ) ; return 1 ; }
	else if( !strcmp( st, "/session" ) ) {
		if( strlen( cfg.sessionname) > 0 ) {
			char buffer[1024] ;
			sprintf( buffer, "Your session name is\n-%s-", cfg.sessionname ) ;
			MessageBox( hwnd, buffer, "Session name", MB_OK|MB_ICONWARNING ) ;
			}
		else
			MessageBox( hwnd, "No session name.", "Session name", MB_OK|MB_ICONWARNING ) ;
		return 1 ;
		}
	else if( !strcmp( st, "/passwd" ) ) {
		if( strlen( cfg.password ) > 0 ) {
			char buffer[1024] ;
			MASKPASS(cfg.password);
			sprintf( buffer, "Your password is\n-%s-", cfg.password ) ;
			MASKPASS(cfg.password);
			MessageBox( hwnd, buffer, "Password", MB_OK|MB_ICONWARNING ) ;
			memset(buffer,0,strlen(buffer));
			}
		else
			MessageBox( hwnd, "No password.", "Password", MB_OK|MB_ICONWARNING ) ;
		return 1 ;
		}
	else if( !strcmp( st, "/configpassword" ) ) {
		strcpy( PasswordConf, "" ) ;
		RegDelValue( HKEY_CURRENT_USER, TEXT(PUTTY_REG_POS), "password" ) ;
		delINI( KittyIniFile, INIT_SECTION, "password" ) ;
		SaveRegistryKey() ;
		MessageBox( NULL, "At next launch,\ndon't forget to check your configuration save mode\n(file or registry ?)", "Info", MB_OK );
		return 1 ;
		}
	else if( strstr( st, "/configpassword " ) == st ) {
		strcpy( PasswordConf, st+16 ) ;
		if( strlen(PasswordConf) > 0 ) {
			strcpy( buffer, PasswordConf ) ;
			WriteParameter( INIT_SECTION, "password", PasswordConf ) ;
			SaveRegistryKey() ;
			cryptstring( buffer, MASTER_PASSWORD ) ;
			// On passe automatiquement en mode de sauvegarde par fichier
			if(!NoKittyFileFlag) writeINI( KittyIniFile, INIT_SECTION, "savemode", "file" ) ;
			IniFileFlag = SAVEMODE_FILE ;
			if(!NoKittyFileFlag) writeINI( KittyIniFile, INIT_SECTION, "password", buffer ) ;
			}
		return 1 ;
		}
	else if( !strcmp( st, "/-configpassword" ) ) {
		if( ReadParameter( INIT_SECTION, "password", buffer ) ) {
			decryptstring( buffer, MASTER_PASSWORD ) ;
			MessageBox( hwnd, buffer, "Your password is ...", MB_OK|MB_ICONWARNING ) ;
			}
		return 1 ;
		}
	else if( !strcmp( st, "/redraw" ) ) { InvalidateRect( MainHwnd, NULL, TRUE ) ; return 1 ; }
	else if( strstr( st, "/PrintCharSize " ) == st ) { PrintCharSize=atoi( st+15 ) ; return 1 ; }
	else if( strstr( st, "/PrintMaxLinePerPage " ) == st ) { PrintMaxLinePerPage=atoi( st+21) ; return 1 ; }
	else if( strstr( st, "/PrintMaxCharPerLine " ) == st ) { PrintMaxCharPerLine=atoi( st+21 ) ; return 1 ; }
	else if( !strcmp( st, "/initlauncher" ) ) { InitLauncherRegistry() ; return 1 ; }
	else if( !strcmp( st, "/wintitle" ) ) { TitleBarFlag = abs(TitleBarFlag -1) ; return 1 ; }
	else if( strstr( st, "/command " ) == st ) { SendCommandAllWindows( hwnd, st+9 ) ; return 1 ; }
	else if( !strcmp( st, "/sizeall" ) ) { ResizeWinList( hwnd, cfg.width, cfg.height ) ; return 1 ; }
	return 0 ;
	}


// Recherche le chemin vers le programme cthelper.exe
int SearchCtHelper( void ) {
	char buffer[1024] ;
	if( CtHelperPath!=NULL ) { free(CtHelperPath) ; CtHelperPath=NULL ; }
	if( ReadParameter( INIT_SECTION, "CtHelperPath", buffer ) != 0 ) {
		if( existfile( buffer ) ) { 
			CtHelperPath = (char*) malloc( strlen(buffer) + 1 ) ; 
			strcpy( CtHelperPath, buffer ) ; 
			set_env( "CTHELPER_PATH", CtHelperPath ) ;
			return 1 ;
			}
		else { DelParameter( INIT_SECTION, "CtHelperPath" ) ; }
		}
	sprintf( buffer, "%s\\cthelper.exe", InitialDirectory ) ;
	if( existfile( buffer ) ) { 
		CtHelperPath = (char*) malloc( strlen(buffer) + 1 ) ; 
		strcpy( CtHelperPath, buffer ) ; 
		set_env( "CTHELPER_PATH", CtHelperPath) ;
		WriteParameter( INIT_SECTION, "CtHelperPath", CtHelperPath ) ;
		return 1 ;
		}
	return 0 ;
	}
	
// Recherche le chemin vers le programme WinSCP
int SearchWinSCP( void ) {
	char buffer[1024] ;
	if( WinSCPPath!=NULL) { free(WinSCPPath) ; WinSCPPath = NULL ; }
	if( ReadParameter( INIT_SECTION, "WinSCPPath", buffer ) != 0 ) {
		if( existfile( buffer ) ) { 
			WinSCPPath = (char*) malloc( strlen(buffer) + 1 ) ; strcpy( WinSCPPath, buffer ) ; return 1 ;
			}
		else { DelParameter( INIT_SECTION, "WinSCPPath" ) ; }
		}
	//strcpy( buffer, "C:\\Program Files\\WinSCP\\WinSCP.exe" ) ;
	sprintf( buffer, "%s\\WinSCP\\WinSCP.exe", getenv("ProgramFiles") ) ;
	if( existfile( buffer ) ) { 
		WinSCPPath = (char*) malloc( strlen(buffer) + 1 ) ; strcpy( WinSCPPath, buffer ) ; 
		WriteParameter( INIT_SECTION, "WinSCPPath", WinSCPPath ) ;
		return 1 ;
		}
	//strcpy( buffer, "C:\\Program Files\\WinSCP3\\WinSCP3.exe" ) ;
	sprintf( buffer, "%s\\WinSCP3\\WinSCP3.exe", getenv("ProgramFiles") ) ;
	if( existfile( buffer ) ) { 
		WinSCPPath = (char*) malloc( strlen(buffer) + 1 ) ; strcpy( WinSCPPath, buffer ) ; 
		WriteParameter( INIT_SECTION, "WinSCPPath", WinSCPPath ) ;
		return 1 ;
		}
	sprintf( buffer, "%s\\WinSCP.exe", InitialDirectory ) ;
	if( existfile( buffer ) ) { 
		WinSCPPath = (char*) malloc( strlen(buffer) + 1 ) ; strcpy( WinSCPPath, buffer ) ; 
		WriteParameter( INIT_SECTION, "WinSCPPath", WinSCPPath ) ;
		return 1 ;
		}
	if( ReadParameter( INIT_SECTION, "winscpdir", buffer ) ) {
		strcat( buffer, "\\" ) ; strcat( buffer, "WinSCP.exe" ) ;
		if( existfile( buffer ) ) { 
			WinSCPPath = (char*) malloc( strlen(buffer) + 1 ) ; strcpy( WinSCPPath, buffer ) ; 
			WriteParameter( INIT_SECTION, "WinSCPPath", WinSCPPath ) ;
			return 1 ;
			}
		}
	return 0 ;
	}

// Lance WinSCP à partir de la sesson courante eventuellement dans le repertoire courant
/* ALIAS UNIX A DEFINIR POUR DEMARRER WINSCP Dans le repertoire courant
winscp()
{
echo "\033]0;__ws:"`pwd`"\007"
}
Il faut ensuite simplement taper: winscp
C'est traité dans KiTTY par la fonction ManageLocalCmd
*/	
// winscp.exe [(sftp|ftp|scp)://][user[:password]@]host[:port][/path/[file]] [/privatekey=key_file]
void StartWinSCP( HWND hwnd, char * directory ) {
	char cmd[4096], shortpath[1024], buffer[256] ;

	if( directory == NULL ) { directory = kitty_current_dir(); } 
	if( WinSCPPath==NULL ) {
		if( IniFileFlag == SAVEMODE_REG ) return ;
		else if( !SearchWinSCP() ) return ;
		}
	if( !existfile( WinSCPPath ) ) {
		if( IniFileFlag == SAVEMODE_REG ) return ;
		else if( !SearchWinSCP() ) return ;
		}
		
	if( !GetShortPathName( WinSCPPath, shortpath, 4095 ) ) return ;

	if( cfg.protocol == PROT_SSH ) {
		//sprintf( cmd, "start %s sftp://%s", shortpath, cfg.username ) ;
		sprintf( cmd, "%s sftp://%s", shortpath, cfg.username ) ;
		if( strlen( cfg.password ) > 0 ) 
			{ strcat( cmd, ":" ); MASKPASS(cfg.password);strcat(cmd,cfg.password);MASKPASS(cfg.password); }
		strcat( cmd, "@" ) ; strcat( cmd, cfg.host ) ; 
		strcat( cmd, ":" ) ; sprintf( buffer, "%d", cfg.port ); strcat( cmd, buffer ) ;
		if( directory!=NULL ) if( strlen(directory)>0 ) {
			strcat( cmd, directory ) ;
			if( directory[strlen(directory)-1]!='/' ) strcat( cmd, "/" ) ;
			}
		if( strlen( cfg.keyfile.path ) > 0 ) {
				if( GetShortPathName( cfg.keyfile.path, shortpath, 4095 ) ) {
				strcat( cmd, " /privatekey=" ) ;
				strcat( cmd, shortpath ) ;
				}
			}
		}
	else {
		//sprintf( cmd, "start %s ftp://%s", shortpath, cfg.username ) ;
		sprintf( cmd, "%s ftp://%s", shortpath, cfg.username ) ;
		if( strlen( cfg.password ) > 0 ) 
			{ strcat( cmd, ":" ); MASKPASS(cfg.password);strcat(cmd,cfg.password );MASKPASS(cfg.password); }
		strcat( cmd, "@" ) ; strcat( cmd, cfg.host ) ; 
		strcat( cmd, ":21" ) ; //sprintf( buffer, "%d", cfg.port ); strcat( cmd, buffer ) ;
		if( directory!=NULL ) if( strlen(directory)>0 ) {
			strcat( cmd, directory ) ;
			if( directory[strlen(directory)-1]!='/' ) strcat( cmd, "/" ) ;
			}
		}

	//set_debug_text( cmd ) ;
	RunCommand( hwnd, cmd ) ;
	memset(cmd,0,strlen(cmd));
	}

	
// Recherche le chemin vers le programme PSCP
int SearchPSCP( void ) {
	char buffer[1024], ki[10]="kscp.exe", pu[10]="pscp.exe" ;

	if( PSCPPath!=NULL ) { free(PSCPPath) ; PSCPPath = NULL ; }
	// Dans la base de registre
	if( ReadParameter( INIT_SECTION, "PSCPPath", buffer ) != 0 ) {
		if( existfile( buffer ) ) { 
			PSCPPath = (char*) malloc( strlen(buffer) + 1 ) ; strcpy( PSCPPath, buffer ) ; return 1 ;
			}
		else { DelParameter( INIT_SECTION, "PSCPPath" ) ; }
		}

	// Dans le fichier ini
	if( ReadParameter( INIT_SECTION, "pscpdir", buffer ) ) {
		strcat( buffer, "\\" ) ; strcat( buffer, ki ) ;
		if( existfile( buffer ) ) { 
			PSCPPath = (char*) malloc( strlen(buffer) + 1 ) ; strcpy( PSCPPath, buffer ) ; 
			WriteParameter( INIT_SECTION, "PSCPPath", PSCPPath ) ;
			return 1 ;
			}
		else {
			ReadParameter( INIT_SECTION, "pscpdir", buffer ) ;
			strcat( buffer, "\\" ) ; strcat( buffer, pu ) ;
			if( existfile( buffer ) ) { 
				PSCPPath = (char*) malloc( strlen(buffer) + 1 ) ; strcpy( PSCPPath, buffer ) ; 
				WriteParameter( INIT_SECTION, "PSCPPath", PSCPPath ) ;
				return 1 ;
				}
			}
		}

#ifndef FDJ
	// kscp dans le meme repertoire
	sprintf( buffer, "%s\\%s", InitialDirectory, ki ) ;
	if( existfile( buffer ) ) { 
		PSCPPath = (char*) malloc( strlen(buffer) + 1 ) ; strcpy( PSCPPath, buffer ) ; 
		WriteParameter( INIT_SECTION, "PSCPPath", PSCPPath ) ;
		return 1 ;
		}
#endif

	// pscp dans le repertoire normal de PuTTY
	sprintf( buffer, "%s\\PuTTY\\%s", getenv("ProgramFiles"), pu ) ;
	if( existfile( buffer ) ) { 
		PSCPPath = (char*) malloc( strlen(buffer) + 1 ) ; strcpy( PSCPPath, buffer ) ; 
		WriteParameter( INIT_SECTION, "PSCPPath", PSCPPath ) ;
		return 1 ;
		}

	// pscp dans le meme repertoire
	sprintf( buffer, "%s\\%s", InitialDirectory, pu ) ;
	if( existfile( buffer ) ) { 
		PSCPPath = (char*) malloc( strlen(buffer) + 1 ) ; strcpy( PSCPPath, buffer ) ; 
		WriteParameter( INIT_SECTION, "PSCPPath", PSCPPath ) ;
		return 1 ;
		}

	return 0 ;
	}
	
// Gestion du drap and drop
void recupNomFichierDragDrop(HWND hwnd, HDROP* leDrop, char* listeResult){
        HDROP hDropInfo=*leDrop;
        int nb,taille,i;
        taille=0;
        nb=0;
        nb=DragQueryFile(hDropInfo, 0xFFFFFFFF, NULL, 0 );
        /*if(nb==0)
            PB1("un appel inutil à la fonction BVisuel::recupNomFichierDragDrop");*/
        char fic[500];
        listeResult[0]='\0' ;
        for( i = 0; i < nb; i++ )
        {
                taille=DragQueryFile(hDropInfo, i, NULL, 0 )+1;
                DragQueryFile(hDropInfo, i, fic, taille );
		if( !strcmp( fic+strlen(fic)-10,"\\kitty.ini" ) ) {
			char buffer[1024]="", shortname[1024]="" ;
			if( GetModuleFileName( NULL, (LPTSTR)buffer, 1023 ) ) 
				if( GetShortPathName( buffer, shortname, 1023 ) ) {
					sprintf( buffer, "%s -ed %s", shortname, fic ) ;
					RunCommand( hwnd, buffer ) ;
					}
			//bl_WinMain(NULL, NULL, fic, SW_SHOWNORMAL) ;
			}
                else { SendOneFile( hwnd, "", fic, NULL ); }
                    //strcat(listeResult,fic);strcat(listeResult,"\n");
                 }
	DragFinish(hDropInfo);  //vidage de la mem...
        *leDrop=hDropInfo;  //TOCHECK : transmistion de param...
	}

void OnDropFiles(HWND hwnd, HDROP hDropInfo) {
        char listeFicSrces[32768];
	//if( cfg.protocol == PROT_SSH ) {
	if( cfg.protocol != PROT_SSH ) {
		MessageBox( hwnd, "This function is only available with SSH connections.", "Error", MB_OK|MB_ICONERROR ) ;
		return ;
		}
        recupNomFichierDragDrop(hwnd, &hDropInfo, listeFicSrces);
	//MessageBox(NULL,listeFicSrces,"Liste :",MB_OK|MB_ICONINFORMATION);
	}

// Appel d'une DLL
/*
typedef int (CALLBACK* LPFNDLLFUNC1)(int,char**); 
int calldll( HWND hwnd, char * filename, char * functionname ) {
	int return_code = 0 ;
	char buffer[1024] ;
	HMODULE lphDLL ;               // Handle to DLL
	LPFNDLLFUNC1 lpfnDllFunc1 ;    // Function pointer
	
	lphDLL = LoadLibrary( TEXT(filename) ) ;
	if( lphDLL == NULL ) {
		//print_error( "Unable to load library %s\n", filename ) ;
		sprintf( buffer, "Unable to load library %s\n", filename ) ;
		MessageBox( hwnd, buffer, "Error" , MB_OK|MB_ICONERROR ) ;
		return -1 ;
		}
		
	if( !( lpfnDllFunc1 = (LPFNDLLFUNC1) GetProcAddress( lphDLL, TEXT(functionname) ) ) ) {
		//print_error( "Unable to load function %s from library %s (%d)\n", functionname, filename, GetLastError() );
		sprintf(buffer,"Unable to load function %s from library %s (%d)\n", functionname, filename, (int)GetLastError() ) ;
		MessageBox( hwnd, buffer, "Error" , MB_OK|MB_ICONERROR ) ;
		FreeLibrary( lphDLL ) ;
		return -1 ;
		}
	
	char **tab ;
	tab=(char**)malloc( 10*sizeof(char* ) ) ;
	int i ;
	for(i=0;i<10;i++) tab[i]=(char*)malloc(256) ;
	strcpy( tab[0], "pscp.exe" ) ; 
	strcpy( tab[1], "-2" ) ;
	strcpy( tab[2], "-scp" ) ;
	strcpy( tab[3], "c:\\tmp\\putty.exe" ) ;
	strcpy( tab[4], "xxxxxx@xxxxxx.xxx.xx:." ) ;
	int tabn = 5 ;
		
	return_code = (lpfnDllFunc1) ( tabn, tab ) ;
	
	for(i=0;i<10;i++) free(tab[i]) ;
	free(tab);
	
	FreeLibrary( lphDLL ) ;
	
	return return_code ;
	}
*/

// Gestion du script au lancement
void ManageInitScript( const char * input_str, const int len ) {
	int i, l ;
	char * st = NULL ;


	if( ScriptFileContent==NULL ) return ;
	if( strlen( ScriptFileContent ) == 0 ) { free( ScriptFileContent ) ; ScriptFileContent = NULL ; return ; }

	st = (char*) malloc( len+2 ) ;
	memcpy( st, input_str, len+1 ) ;
	for( i=0 ; i<len ; i++ ) if( st[i]=='\0' ) st[i]=' ' ;
	
	if( debug_flag ) { debug_log( ">%d|", len ) ; debug_log( "%s|\n", st ) ; }

	if( strstr( st, ScriptFileContent ) != NULL ) {
		SendKeyboardPlus( MainHwnd, ScriptFileContent+strlen(ScriptFileContent)+1 ) ;
		l = strlen( ScriptFileContent ) + strlen( ScriptFileContent+strlen(ScriptFileContent)+1 ) + 2 ;
		
		if( debug_flag ) { debug_log( "<%d|", l ) ; debug_log( "%s|\n", ScriptFileContent+strlen(ScriptFileContent)+1 ) ; }
		
		ScriptFileContent[0]=ScriptFileContent[l] ;
		i = 0 ;
		do {
			i++ ;
			ScriptFileContent[i]=ScriptFileContent[i+l] ;
			}
		while( (ScriptFileContent[i]!='\0')||(ScriptFileContent[i-1]!='\0') ) ;
		}
		
	free( st ) ;
	}
	
void ReadInitScript( const char * filename ) {
	char * pst, buffer[4096], *name=NULL ;
	FILE *fp ;
	int l ; 

	if( filename != NULL )
		if( strlen( filename ) > 0 ) {
			name = (char*) malloc( strlen( filename ) + 1 ) ;
			strcpy( name, filename ) ;
			}

	if( name == NULL ) 
		if( cfg.scriptfile.path != NULL )
		if( strlen( cfg.scriptfile.path ) > 0 ) {
			name = (char*) malloc( strlen( cfg.scriptfile.path ) + 1 ) ;
			strcpy( name, cfg.scriptfile.path ) ;
			}

	if( name != NULL ) {
	if( existfile( name ) ) {
		if( ( fp = fopen( name,"rb") ) != NULL ) {
			if( ScriptFileContent!= NULL ) free( ScriptFileContent ) ;
			l = 0 ;

			ScriptFileContent = (char*) malloc( filesize(name)+10 ) ;
			ScriptFileContent[0] = '\0' ;
			pst=ScriptFileContent ;
			while( fgets( buffer, 1024, fp ) != NULL ) {
				while( (buffer[strlen(buffer)-1]=='\n')||(buffer[strlen(buffer)-1]=='\r') ) buffer[strlen(buffer)-1]='\0' ;
				if( strlen( buffer ) > 0 ) {
					strcpy( pst, buffer ) ;
					pst = pst + strlen( pst ) + 1 ;
					l = l + strlen( buffer ) + 1 ;
					}
				}
			pst[0] = '\0' ;
			l++ ;
			fclose( fp ) ;
			bcrypt_string_base64( ScriptFileContent, buffer, l, MASTER_PASSWORD, 0 ) ;
			WriteParameter( INIT_SECTION, "KiCrSt", buffer ) ;
			}
		}
	else {
		strcpy( buffer, name ) ;
		l = decryptstring( buffer, MASTER_PASSWORD ) ;
		if( ScriptFileContent!= NULL ) free( ScriptFileContent ) ;
		ScriptFileContent = (char*) malloc( l + 1 ) ;
		memcpy( ScriptFileContent, buffer, l ) ;
		}
		}
	}
	
#include "kitty_launcher.c"
	
// Creer une arborescence de repertoire à partir du registre
void MakeDirTree( const char * Directory, const char * s, const char * sd ) {
	char buffer[MAX_VALUE_NAME], fullpath[MAX_VALUE_NAME] ;
	HKEY hKey;
	int retCode, i ;
	unsigned char lpData[1024] ;
	DWORD lpType, dwDataSize = 1024, cchValue = MAX_VALUE_NAME ;
	FILE * fp ;

	TCHAR achClass[MAX_PATH] = TEXT(""), achKey[MAX_KEY_LENGTH], achValue[MAX_VALUE_NAME] ;
	DWORD cchClassName = MAX_PATH, cSubKeys=0, cbMaxSubKey, cchMaxClass, cValues, cchMaxValue, cbMaxValueData, cbSecurityDescriptor, cbName;
	FILETIME ftLastWriteTime; 
	
	sprintf( fullpath, "%s\\%s", Directory, sd ) ; 
	MakeDir( fullpath ) ;
	
	sprintf( buffer, "%s\\%s", TEXT(PUTTY_REG_POS), s ) ;

	if( RegOpenKeyEx( HKEY_CURRENT_USER, TEXT(buffer), 0, KEY_READ, &hKey) == ERROR_SUCCESS ) {
		if( RegQueryInfoKey(hKey,achClass,&cchClassName,NULL,&cSubKeys,&cbMaxSubKey
			,&cchMaxClass,&cValues,&cchMaxValue,&cbMaxValueData,&cbSecurityDescriptor,&ftLastWriteTime) == ERROR_SUCCESS ) {
			if (cSubKeys) for (i=0; i<cSubKeys; i++) {
				cbName = MAX_KEY_LENGTH;
				retCode = RegEnumKeyEx(hKey, i, achKey, &cbName, NULL, NULL, NULL, &ftLastWriteTime) ;
				sprintf( buffer, "%s\\%s", s, achKey ) ;
				sprintf( fullpath, "%s\\%s", sd, achKey ) ;
				MakeDirTree( Directory, buffer, fullpath ) ;
				}
			retCode = ERROR_SUCCESS ;
			if(cValues) for (i=0, retCode=ERROR_SUCCESS; i<cValues; i++) {
				cchValue = MAX_VALUE_NAME; 
				achValue[0] = '\0'; 
				if( (retCode = RegEnumValue(hKey, i, achValue, &cchValue, NULL, NULL,NULL,NULL) ) == ERROR_SUCCESS ){
					dwDataSize = 1024 ;
					RegQueryValueEx( hKey, TEXT( achValue ), 0, &lpType, lpData, &dwDataSize ) ;
					if( (int)lpType == REG_SZ ) {
						mungestr( achValue, buffer ) ;
						sprintf( fullpath, "%s\\%s\\%s", Directory, sd, buffer ) ;
						if( ( fp=fopen( fullpath, "wb") ) != NULL ) {
							fprintf( fp, "%s\\%s\\",achValue,lpData );
							fclose(fp);
							}
						}
					}
				}
			}
		RegCloseKey( hKey ) ;
		}
	}

// Convertit la base de registre en répertoire pour le mode savemode=dir
int Convert2Dir( const char * Directory ) {
	char buffer[MAX_VALUE_NAME], fullpath[MAX_VALUE_NAME] ;
	HKEY hKey;
	int retCode, i, delkeyflag=0 ;
	FILE *fp ;

	unsigned char lpData[1024] ;
	TCHAR achClass[MAX_PATH] = TEXT(""), achKey[MAX_KEY_LENGTH], achValue[MAX_VALUE_NAME] ; 
	DWORD cchClassName = MAX_PATH, cSubKeys=0, cbMaxSubKey, cchMaxClass, cValues, cchMaxValue, cbMaxValueData, cbSecurityDescriptor, cbName,cchValue = MAX_VALUE_NAME , dwDataSize, lpType;
	FILETIME ftLastWriteTime; 
	
	if( !RegTestKey( HKEY_CURRENT_USER, TEXT(PUTTY_REG_POS) ) ) // Si la clé de KiTTY n'existe pas on récupère celle de PuTTY
		{ TestRegKeyOrCopyFromPuTTY( HKEY_CURRENT_USER, TEXT(PUTTY_REG_POS) ) ; delkeyflag = 1 ; }
	
	sprintf( buffer, "%s\\Commands", Directory ) ; DelDir( buffer) ; MakeDirTree( Directory, "Commands", "Commands" ) ;
	sprintf( buffer, "%s\\Launcher", Directory ) ; DelDir( buffer) ; MakeDirTree( Directory, "Launcher", "Launcher" ) ;
	sprintf( buffer, "%s\\Folders", Directory ) ; DelDir( buffer) ; MakeDirTree( Directory, "Folders", "Folders" ) ;
	
	sprintf( buffer, "%s\\Sessions", Directory ) ; DelDir( buffer) ; MakeDir( buffer ) ;
	sprintf( buffer, "%s\\Sessions", TEXT(PUTTY_REG_POS) ) ;
		
	sprintf( fullpath, "%s\\Sessions_Commands", Directory ) ; DelDir( fullpath ) ; MakeDir( fullpath ) ;

	if( RegOpenKeyEx( HKEY_CURRENT_USER, TEXT(buffer), 0, KEY_READ, &hKey) == ERROR_SUCCESS ) {
		if( RegQueryInfoKey(hKey,achClass,&cchClassName,NULL,&cSubKeys,&cbMaxSubKey
			,&cchMaxClass,&cValues,&cchMaxValue,&cbMaxValueData,&cbSecurityDescriptor,&ftLastWriteTime) == ERROR_SUCCESS ) {
			if (cSubKeys) for (i=0; i<cSubKeys; i++) {
				cbName = MAX_KEY_LENGTH;
				retCode = RegEnumKeyEx(hKey, i, achKey, &cbName, NULL, NULL, NULL, &ftLastWriteTime) ;
				unmungestr( achKey, buffer,MAX_PATH) ;
				IniFileFlag = SAVEMODE_REG ;
				load_settings( buffer, &cfg ) ;
				IniFileFlag = SAVEMODE_DIR ;
				SetInitialSessPath() ;
				SetCurrentDirectory( Directory ) ;
				
				if( DirectoryBrowseFlag ) if( strcmp(cfg.folder, "Default")&&strcmp(cfg.folder, "") ) {
					//sprintf( fullpath, "%s\\Commands\\%s", Directory, cfg.folder ) ;
					sprintf( fullpath, "%s\\Sessions\\%s", Directory, cfg.folder ) ;
					MakeDir( fullpath ) ;
					SetSessPath( cfg.folder ) ; 
					}
				
				save_settings( buffer, &cfg) ;

				sprintf( buffer, "%s\\Sessions\\%s\\Commands", TEXT(PUTTY_REG_POS), achKey ) ;
				if( RegTestKey( HKEY_CURRENT_USER, buffer ) ){
					sprintf( buffer, "Sessions\\%s\\Commands", achKey ) ;
					sprintf( fullpath, "Sessions_Commands\\%s", achKey ) ;
					MakeDirTree( Directory, buffer, fullpath ) ;
					}
				}
			}
		RegCloseKey( hKey ) ;
		}
	
	
	sprintf( buffer, "%s\\SshHostKeys", Directory ) ; DelDir( buffer) ; MakeDir( buffer ) ;
	sprintf( buffer, "%s\\SshHostKeys", TEXT(PUTTY_REG_POS) ) ;
	if( RegOpenKeyEx( HKEY_CURRENT_USER, TEXT(buffer), 0, KEY_READ, &hKey) == ERROR_SUCCESS ) {
		if( RegQueryInfoKey(hKey,achClass,&cchClassName,NULL,&cSubKeys,&cbMaxSubKey
			,&cchMaxClass,&cValues,&cchMaxValue,&cbMaxValueData,&cbSecurityDescriptor,&ftLastWriteTime) == ERROR_SUCCESS ) {
			retCode = ERROR_SUCCESS ;
			if(cValues) for (i=0, retCode=ERROR_SUCCESS; i<cValues; i++) {
				cchValue = MAX_VALUE_NAME; 
				achValue[0] = '\0'; 
				if( (retCode = RegEnumValue(hKey, i, achValue, &cchValue, NULL, NULL,NULL,NULL) ) == ERROR_SUCCESS ){
					dwDataSize = 1024 ;
					RegQueryValueEx( hKey, TEXT( achValue ), 0, &lpType, lpData, &dwDataSize ) ;
					if( (int)lpType == REG_SZ ) {
						mungestr( achValue, buffer ) ;
						sprintf( fullpath, "%s\\SshHostKeys\\%s", Directory, buffer ) ;
						if( ( fp=fopen( fullpath, "wb") ) != NULL ) {
							fprintf( fp, "%s",lpData ) ;
							fclose(fp);
							}
						}
					}
				}
				
			}
		RegCloseKey( hKey ) ;
		}
		
	if( delkeyflag ) { RegDelTree (HKEY_CURRENT_USER, TEXT(PUTTY_REG_PARENT) ) ; }
	
	return 0;
	}

// Convertit une sauvegarde en mode savemode=dir vers la base de registre
void ConvertDir2Reg( const char * Directory, HKEY hKey, char * path )  {
	char directory[MAX_VALUE_NAME], buffer[MAX_VALUE_NAME], session[MAX_VALUE_NAME] ;
	DIR * dir ;
	struct dirent * de ;
	sprintf( directory, "%s\\Sessions\\%s", Directory, path ) ;
	if( ( dir = opendir( directory ) ) != NULL ) {
		while( (de=readdir(dir)) != NULL ) 
		if( strcmp(de->d_name,".")&&strcmp(de->d_name,"..")  ) {
			sprintf( buffer, "%s\\%s", directory, de->d_name ) ;
			if( GetFileAttributes( buffer ) & FILE_ATTRIBUTE_DIRECTORY ) {
				if( strlen(path)>0 ) sprintf( buffer, "%s\\%s", path, de->d_name ) ;
				else strcpy( buffer, de->d_name ) ;
				ConvertDir2Reg( Directory, hKey, buffer ) ;
				}
			else {
				SetSessPath( path ) ;
				IniFileFlag = SAVEMODE_DIR ;
				unmungestr( de->d_name, session, MAX_PATH) ;
				load_settings( session, &cfg ) ;
				//mungestr(de->d_name, session) ;
				IniFileFlag = SAVEMODE_REG ;
				strcpy( cfg.folder, path ) ;
				save_settings( session, &cfg) ;
				}
			}
		closedir( dir ) ;
		}
	}
 
int Convert2Reg( const char * Directory ) {
	char buffer[MAX_VALUE_NAME] ;
	HKEY hKey;
 
	if( RegTestKey( HKEY_CURRENT_USER, TEXT(PUTTY_REG_POS) ) ) 
		{ RegDelTree (HKEY_CURRENT_USER, TEXT(PUTTY_REG_POS) ) ; }
 
	SetCurrentDirectory( Directory ) ;
	sprintf( buffer, "%s\\Sessions", TEXT(PUTTY_REG_POS) ) ;
	RegTestOrCreate( HKEY_CURRENT_USER, buffer, NULL, NULL ) ;
 
	if( RegOpenKeyEx( HKEY_CURRENT_USER, TEXT(buffer), 0, KEY_READ, &hKey) == ERROR_SUCCESS ) {
		strcpy( buffer, "" ) ;
		ConvertDir2Reg( Directory, hKey, buffer ) ;
		RegCloseKey( hKey ) ;
		}
 
	return 0 ;
	}

#if (defined IMAGEPORT) && (!defined FDJ)
// Gestion de l'image viewer
int ManageViewer( HWND hwnd, WORD wParam ) { // Gestion du mode image
	if( wParam==VK_BACK ) 
		{ if( PreviousBgImage( hwnd ) ) InvalidateRect(hwnd, NULL, TRUE) ; 
		set_title(NULL, cfg.wintitle) ; 
		return 1 ; 
		}
	else if( wParam==VK_SPACE ) 
		{ if( NextBgImage( hwnd ) ) InvalidateRect(hwnd, NULL, TRUE) ;
		set_title(NULL, cfg.wintitle) ; 
		return 1 ; 
		}
	else if( wParam == VK_DOWN ) 	// Augmenter l'opacité de l'image de fond
		{ if( cfg.bg_type != 0 )
		{ cfg.bg_opacity += 5 ; if( cfg.bg_opacity>100 ) cfg.bg_opacity = 0 ;
		RefreshBackground( hwnd ) ;
		return 1 ; }
		}
	else if( wParam == VK_UP ) 		// Diminuer l'opacité de l'image de fond
		{ if( cfg.bg_type != 0 )
		{ cfg.bg_opacity -= 5 ; if( cfg.bg_opacity<0 ) cfg.bg_opacity = 100 ;
		RefreshBackground( hwnd ) ; 
		return 1 ; }
		}
	else if( wParam == VK_RETURN ) {
		  if (IsZoomed(hwnd)) { ShowWindow(hwnd, SW_RESTORE); } 
		  else { ShowWindow(hwnd, SW_MAXIMIZE); }
		return 1 ;
		}
	return 0 ;
	}
#endif
	
// GESTION DES RACCOURCI
int DefineShortcuts( char * buf ) {
	char * pst = buf ;
	int key = 0 ;
	while( (strstr(pst,"{SHIFT}")==pst) || (strstr(pst,"{CONTROL}")==pst) || (strstr(pst,"{ALT}")==pst) || (strstr(pst,"{ALTGR}")==pst) || (strstr(pst,"{WIN}")==pst) ) {
		while( strstr(pst,"{ALT}")==pst ) { key += ALTKEY ; pst += 5 ; }
		while( strstr(pst,"{ALTGR}")==pst ) { key += ALTGRKEY ; pst += 7 ; }
		while( strstr(pst,"{WIN}")==pst ) { key += WINKEY ; pst += 5 ; }
		while( strstr(pst,"{SHIFT}")==pst ) { key += SHIFTKEY ; pst += 7 ; }
		while( strstr(pst,"{CONTROL}")==pst ) { key += CONTROLKEY ; pst += 9 ; }
		}
	
	if( strstr( pst, "{F12}" )==pst ) { key = key + VK_F12 ; pst += 5 ; }
	else if( strstr( pst, "{F11}" )==pst ) { key = key + VK_F11 ; pst += 5 ; }
	else if( strstr( pst, "{F10}" )==pst ) { key = key + VK_F10 ; pst += 5 ; }
	else if( strstr( pst, "{F9}" )==pst ) { key = key + VK_F9 ; pst += 4 ; }
	else if( strstr( pst, "{F8}" )==pst ) { key = key + VK_F8 ; pst += 4 ; }
	else if( strstr( pst, "{F7}" )==pst ) { key = key + VK_F7 ; pst += 4 ; }
	else if( strstr( pst, "{F6}" )==pst ) { key = key + VK_F6 ; pst += 4 ; }
	else if( strstr( pst, "{F5}" )==pst ) { key = key + VK_F5 ; pst += 4 ; }
	else if( strstr( pst, "{F4}" )==pst ) { key = key + VK_F4 ; pst += 4 ; }
	else if( strstr( pst, "{F3}" )==pst ) { key = key + VK_F3 ; pst += 4 ; }
	else if( strstr( pst, "{F2}" )==pst ) { key = key + VK_F2 ; pst += 4 ; }
	else if( strstr( pst, "{F1}" )==pst ) { key = key + VK_F1 ; pst += 4 ; }
	else if( strstr( pst, "{RETURN}" )==pst ) { key = key + VK_RETURN ; pst += 8 ; }
	else if( strstr( pst, "{ESCAPE}" )==pst ) { key = key + VK_ESCAPE ; pst += 8 ; }
	else if( strstr( pst, "{SPACE}" )==pst ) { key = key + VK_SPACE ; pst += 7 ; }
	else if( strstr( pst, "{PRINT}" )==pst ) { key = key + VK_SNAPSHOT ; pst += 7 ; }
	else if( strstr( pst, "{PAUSE}" )==pst ) { key = key + VK_PAUSE ; pst += 7 ; }
	else if( strstr( pst, "{PRIOR}" )==pst ) { key = key + VK_PRIOR ; pst += 7 ; }
	else if( strstr( pst, "{RIGHT}" )==pst ) { key = key + VK_RIGHT ; pst += 7 ; }
	else if( strstr( pst, "{LEFT}" )==pst ) { key = key + VK_LEFT ; pst += 6 ; }
	else if( strstr( pst, "{NEXT}" )==pst ) { key = key + VK_NEXT ; pst += 6 ; }
	else if( strstr( pst, "{BACK}" )==pst ) { key = key + VK_BACK ; pst += 6 ; }
	else if( strstr( pst, "{HOME}" )==pst ) { key = key + VK_HOME ; pst += 6 ; }
	else if( strstr( pst, "{DOWN}" )==pst ) { key = key + VK_DOWN ; pst += 6 ; }
	else if( strstr( pst, "{ATTN}" )==pst ) { key = key + VK_ATTN ; pst += 6 ; }
	else if( strstr( pst, "{END}" )==pst ) { key = key + VK_END ; pst += 5 ; }
	else if( strstr( pst, "{TAB}" )==pst ) { key = key + VK_TAB ; pst += 5 ; }
	else if( strstr( pst, "{INS}" )==pst ) { key = key + VK_INSERT ; pst += 5 ; }
	else if( strstr( pst, "{DEL}" )==pst ) { key = key + VK_DELETE ; pst += 5 ; }
	else if( strstr( pst, "{UP}" )==pst ) { key = key + VK_UP ; pst += 4 ; }
	else if( strstr( pst, "{NUMPAD0}" )==pst ) { key = key + VK_NUMPAD0 ; pst += 9 ; }
	else if( strstr( pst, "{NUMPAD1}" )==pst ) { key = key + VK_NUMPAD1 ; pst += 9 ; }
	else if( strstr( pst, "{NUMPAD2}" )==pst ) { key = key + VK_NUMPAD2 ; pst += 9 ; }
	else if( strstr( pst, "{NUMPAD3}" )==pst ) { key = key + VK_NUMPAD3 ; pst += 9 ; }
	else if( strstr( pst, "{NUMPAD4}" )==pst ) { key = key + VK_NUMPAD4 ; pst += 9 ; }
	else if( strstr( pst, "{NUMPAD5}" )==pst ) { key = key + VK_NUMPAD5 ; pst += 9 ; }
	else if( strstr( pst, "{NUMPAD6}" )==pst ) { key = key + VK_NUMPAD6 ; pst += 9 ; }
	else if( strstr( pst, "{NUMPAD7}" )==pst ) { key = key + VK_NUMPAD7 ; pst += 9 ; }
	else if( strstr( pst, "{NUMPAD8}" )==pst ) { key = key + VK_NUMPAD8 ; pst += 9 ; }
	else if( strstr( pst, "{NUMPAD9}" )==pst ) { key = key + VK_NUMPAD9 ; pst += 9 ; }
	else if( strstr( pst, "{BREAK}" )==pst ) { key = key + VK_CANCEL ; pst += 7 ; }
	
	else if( pst[0] == '{' ) {key = 0 ; }
	else { key = key + toupper(pst[0]) ; }
	
	return key ;
	}

void TranslateShortcuts( char * st ) {
	int i,j,k,r ;
	char *buffer;
	if( st==NULL ) return ;
	if( strlen(st)==0 ) return ;
	buffer = (char*) malloc( strlen(st)+1 ) ;
	
	for( i=0 ; i<strlen(st) ; i++ ) {
		if( st[i]=='{' ) {
			if( ( j=poss( "}", st+i ) ) > 0 ) {
				for( k=i ; k<(i+j) ; k++ ) {
					buffer[k-i]=st[k];
					buffer[k-i+1]='\0';
					}
				r=DefineShortcuts( buffer );
				del(st,i+1,j-1);
				st[i]=r ;
				}
			}
/*
		else if( (st[i]=='\\')&&(st[i+1]=='\0') ) { i++ ; }
		else if( (st[i]=='\\')&&(st[i+2]=='\0') ) { i++ ; }
		else if( (st[i]=='\\')&&(st[i+1]!='\\') ) {
			buffer[0]=st[i+1];
			buffer[1]=st[i+2];
			buffer[2]='\0' ;
			sscanf( buffer, "%X", &r );
			del( st, i+1, 2 ) ;
			st[i]=r ;
			}
*/
		}
	free(buffer);
	}
	
void InitShortcuts( void ) {
	char buffer[256], list[2048], *pl ;
	int i, t=0 ;
	if( !readINI(KittyIniFile,"Shortcuts","editor",buffer) || ( (shortcuts_tab.editor=DefineShortcuts(buffer))<=0 ) )
		shortcuts_tab.editor = SHIFTKEY+VK_F2 ;
	if( !readINI(KittyIniFile,"Shortcuts","winscp",buffer) || ( (shortcuts_tab.winscp=DefineShortcuts(buffer))<=0 ) )
		shortcuts_tab.winscp = SHIFTKEY+VK_F3 ;
	if( !readINI(KittyIniFile,"Shortcuts","print",buffer) || ( (shortcuts_tab.print=DefineShortcuts(buffer))<=0 ) )
		shortcuts_tab.print = SHIFTKEY+VK_F7 ;
	if( !readINI(KittyIniFile,"Shortcuts","printall",buffer) || ( (shortcuts_tab.printall=DefineShortcuts(buffer))<=0 ) )
		shortcuts_tab.printall = VK_F7 ;
	if( !readINI(KittyIniFile,"Shortcuts","inputm",buffer) || ( (shortcuts_tab.inputm=DefineShortcuts(buffer))<=0 ) )
		shortcuts_tab.inputm = SHIFTKEY+VK_F8 ;
	if( !readINI(KittyIniFile,"Shortcuts","viewer",buffer) || ( (shortcuts_tab.viewer=DefineShortcuts(buffer))<=0 ) )
		shortcuts_tab.viewer = SHIFTKEY+VK_F11 ;
	if( !readINI(KittyIniFile,"Shortcuts","autocommand",buffer) || ( (shortcuts_tab.autocommand=DefineShortcuts(buffer))<=0 ) ) 
		shortcuts_tab.autocommand = SHIFTKEY+VK_F12 ;

	if( !readINI(KittyIniFile,"Shortcuts","script",buffer) || ( (shortcuts_tab.script=DefineShortcuts(buffer))<=0 ) )
		shortcuts_tab.script = CONTROLKEY+VK_F2 ;
	if( !readINI(KittyIniFile,"Shortcuts","sendfile",buffer) || ( (shortcuts_tab.sendfile=DefineShortcuts(buffer))<=0 ) )
		shortcuts_tab.sendfile = CONTROLKEY+VK_F3 ;
	if( !readINI(KittyIniFile,"Shortcuts","getfile",buffer) || ( (shortcuts_tab.getfile=DefineShortcuts(buffer))<=0 ) )
		shortcuts_tab.getfile = CONTROLKEY+VK_F4 ;
	if( !readINI(KittyIniFile,"Shortcuts","command",buffer) || ( (shortcuts_tab.command=DefineShortcuts(buffer))<=0 ) )
		shortcuts_tab.command = CONTROLKEY+VK_F5 ;
	if( !readINI(KittyIniFile,"Shortcuts","tray",buffer) || ( (shortcuts_tab.tray=DefineShortcuts(buffer))<=0 ) )
		shortcuts_tab.tray = CONTROLKEY+VK_F6 ;
	if( !readINI(KittyIniFile,"Shortcuts","visible",buffer) || ( (shortcuts_tab.visible=DefineShortcuts(buffer))<=0 ) )
		shortcuts_tab.visible = CONTROLKEY+VK_F7 ;
	if( !readINI(KittyIniFile,"Shortcuts","input",buffer) || ( (shortcuts_tab.input=DefineShortcuts(buffer))<=0 ) )
		shortcuts_tab.input = CONTROLKEY+VK_F8 ;
	if( !readINI(KittyIniFile,"Shortcuts","protect",buffer) || ( (shortcuts_tab.protect=DefineShortcuts(buffer))<=0 ) )
		shortcuts_tab.protect = CONTROLKEY+VK_F9 ;
	if( !readINI(KittyIniFile,"Shortcuts","imagechange",buffer) || ( (shortcuts_tab.imagechange=DefineShortcuts(buffer))<=0 ) )
		shortcuts_tab.imagechange = CONTROLKEY+VK_F11 ;
	if( !readINI(KittyIniFile,"Shortcuts","rollup",buffer) || ( (shortcuts_tab.rollup=DefineShortcuts(buffer))<=0 ) )
		shortcuts_tab.rollup = CONTROLKEY+VK_F12 ;

	if( ReadParameter( "Shortcuts", "list", list ) ) {
		pl=list ;
		while( strlen(pl) > 0 ) {
			i=0;
			while( (i<strlen(pl))&&(pl[i]!=' ') ) { i++ ; }
			if( pl[i]==' ' ) { pl[i]='\0' ; t=1 ; }
			if( ReadParameter( "Shortcuts", pl, buffer ) ) {
				if( (pl[0]<'0')||(pl[0]>'9') ) {
					shortcuts_tab2[NbShortCuts].num = DefineShortcuts( pl );
					}
				else shortcuts_tab2[NbShortCuts].num = atoi(pl) ;

				TranslateShortcuts( buffer ) ;
			
				shortcuts_tab2[NbShortCuts].st=(char*)malloc( strlen(buffer)+1 ) ;
				strcpy( shortcuts_tab2[NbShortCuts].st, buffer ) ;
				NbShortCuts++;
				}
			if( t==1 ) { pl[i]=' ' ; t = 0 ; pl=pl+i+1 ;}
			else pl=pl+i ;

			while( pl[0]==' ' ) pl++ ;
			}
		}
	}

int ManageShortcuts( HWND hwnd, int key_num, int shift_flag, int control_flag, int alt_flag, int altgr_flag, int win_flag ) {
	int key, i ;
	key = key_num ;
	if( alt_flag ) key = key + ALTKEY ;
	if( altgr_flag ) key = key + ALTGRKEY ;
	if( shift_flag ) key = key + SHIFTKEY ;
	if( control_flag ) key = key + CONTROLKEY ;
	if( win_flag ) key = key + WINKEY ;

//if( (key_num!=VK_SHIFT)&&(key_num!=VK_CONTROL) ) {char b[256] ; sprintf( b, "alt=%d altgr=%d shift=%d control=%d key=%d print=%d", alt_flag, altgr_flag, shift_flag, control_flag, key, shortcuts_tab.print ); MessageBox(hwnd, b, "Info", MB_OK);}
	
	if( key == shortcuts_tab.protect )				// Protection
		{ SendMessage( hwnd, WM_COMMAND, IDM_PROTECT, 0 ) ; InvalidateRect( hwnd, NULL, TRUE ) ; return 1 ; }
	if( key == shortcuts_tab.rollup ) 				// Fonction winrol
			{ SendMessage( hwnd, WM_COMMAND, IDM_WINROL, 0 ) ; return 1 ; }

	if( (ProtectFlag == 1) || (WinHeight != -1) ) return 1 ;
		
	if( NbShortCuts ) {
		for( i=0 ; i<NbShortCuts ; i++ )
		if( shortcuts_tab2[i].num == key ) 
			{ SendKeyboardPlus( hwnd, shortcuts_tab2[i].st ) ; return 1 ; }
		}
	
#if (defined IMAGEPORT) && (!defined FDJ)
	if( BackgroundImageFlag && ImageViewerFlag ) { // Gestion du mode image
		if( ManageViewer( hwnd, key_num ) ) return 1 ;
		}
#endif

	if( control_flag && shift_flag && (key_num==VK_F12) ) {
		ResizeWinList( hwnd, cfg.width, cfg.height ) ; return 1 ; 
		} // Retaille toutes les autres fenetres a la dimension de celle-ci

	if( key == shortcuts_tab.printall ) {
		SendMessage( hwnd, WM_COMMAND, IDM_COPYALL, 0 ) ;
		SendMessage( hwnd, WM_COMMAND, IDM_PRINT, 0 ) ;
		return 1 ;
		}

	if( ( IniFileFlag != SAVEMODE_DIR ) && shift_flag && control_flag ) {		
		if( ( key_num >= 'A' ) && ( key_num <= 'Z' ) ) // Raccourci commandes spéciales (SpecialMenu)
			{ SendMessage( hwnd, WM_COMMAND, IDM_USERCMD+key_num-'A', 0 ) ; return 1 ; }
		}
		
	if( key == shortcuts_tab.editor ) 			// Lancement d'un putty-ed
		{ RunPuttyEd( hwnd ) ; return 1 ; }
	else if( key == shortcuts_tab.winscp )			// Lancement de WinSCP
		{ SendMessage( hwnd, WM_COMMAND, IDM_WINSCP, 0 ) ; return 1 ; }
	else if( key == shortcuts_tab.autocommand ) 		// Rejouer la commande de demarrage
		{ RenewPassword( &cfg ) ; 
			//SendAutoCommand( hwnd, cfg.autocommand ) ; 
			SetTimer(hwnd, TIMER_AUTOCOMMAND, (int)(autocommand_delay*1000), NULL) ;
			logevent(NULL,"Send automatic command");
			
			return 1 ; }
	if( key == shortcuts_tab.print ) 	// Impression presse papier
		{ SendMessage( hwnd, WM_COMMAND, IDM_PRINT, 0 ) ; return 1 ; }
#ifndef FDJ
	if( key == shortcuts_tab.inputm )	 		// Fenetre de controle
			{ 
			MainHwnd = hwnd ; _beginthread( routine_inputbox_multiline, 0, (void*)&hwnd ) ;
			return 1 ;
			}
#endif
	if( key == shortcuts_tab.viewer ) 			// Switcher le mode visualiseur d'image
		{ ImageViewerFlag = abs(ImageViewerFlag-1) ; set_title(NULL, cfg.wintitle) ; return 1 ; }

	if( key == shortcuts_tab.script ) 			// Chargement d'un fichier de script
		{ OpenAndSendScriptFile( hwnd ) ; return 1 ; }
	else if( key == shortcuts_tab.sendfile ) 		// Envoi d'un fichier par SCP
		{ SendMessage( hwnd, WM_COMMAND, IDM_PSCP, 0 ) ; return 1 ; }
	else if( key == shortcuts_tab.getfile ) 		// Reception d'un fichier par SCP
		{ GetFile( hwnd ) ; return 1 ; }
	else if( key == shortcuts_tab.command )			// Execution d'une commande locale
			{ RunCmd( hwnd ) ; return 1 ; }
	else if( key == shortcuts_tab.tray ) 		// Send to tray
		{ SendMessage( hwnd, WM_COMMAND, IDM_TOTRAY, 0 ) ; return 1 ; }
	else if( key == shortcuts_tab.visible )  		// Always visible 
			{ SendMessage( hwnd, WM_COMMAND, IDM_VISIBLE, 0 ) ; return 1 ; }
#ifndef FDJ
	else if( key == shortcuts_tab.input ) 			// Fenetre de controle
			{ MainHwnd = hwnd ; _beginthread( routine_inputbox, 0, (void*)&hwnd ) ;
			InvalidateRect( hwnd, NULL, TRUE ) ; return 1 ;
			}
#endif

#if (defined IMAGEPORT) && (!defined FDJ)
	else if( BackgroundImageFlag && (key == shortcuts_tab.imagechange) ) 		// Changement d'image de fond
		{ if( NextBgImage( hwnd ) ) InvalidateRect(hwnd, NULL, TRUE) ; return 1 ; }
#endif

	if( control_flag ) {
		if( (TransparencyNumber!=-1)&&((key_num == VK_SUBTRACT)||(key_num == VK_DOWN)) ) //Augmenter la transaparence
			{ SendMessage( hwnd, WM_COMMAND, IDM_TRANSPARDOWN, 0 ) ; return 1 ; }
		if( (TransparencyNumber!=-1)&&((key_num == VK_ADD)||(key_num == VK_UP)) ) //Diminuer la transaparence
			{ SendMessage( hwnd, WM_COMMAND, IDM_TRANSPARUP, 0 ) ; return 1 ; }
		if (key_num == VK_LEFT ) //Fenetre KiTTY precedente
			{ SendMessage( hwnd, WM_COMMAND, IDM_GOPREVIOUS, 0 ) ; return 1 ; }
		if (key_num == VK_RIGHT ) //Fenetre KiTTY Suivante
			{ SendMessage( hwnd, WM_COMMAND, IDM_GONEXT, 0 ) ; return 1 ; }
		}
	return 0 ;
	}

// shift+bouton droit => paste ameliore pour serveur "lent"
// Le paste utilise la methode "autocommand"
void SetPasteCommand( void ) {
	if( !PasteCommandFlag ) return ;
	if( PasteCommand != NULL ) { free( PasteCommand ) ; PasteCommand = NULL ; }
	char *pst = NULL ;
	if( OpenClipboard(NULL) ) {
		HGLOBAL hglb ;
		if( (hglb = GetClipboardData( CF_TEXT ) ) != NULL ) {
			if( ( pst = GlobalLock( hglb ) ) != NULL ) {
				PasteCommand = (char*) malloc( strlen(pst)+1 ) ;
				strcpy( PasteCommand, pst ) ;
				SetTimer(hwnd, TIMER_AUTOPASTE, (int)(autocommand_delay*1000), NULL) ;
				logevent(NULL,"Sent paste command");
				GlobalUnlock( hglb ) ;
				}
			}
		CloseClipboard();
		}
	}
	
// Initialisation des parametres à partir du fichier kitty.ini
#if (defined IMAGEPORT) && (!defined FDJ)
void SetShrinkBitmapEnable(int) ;
#endif

void LoadParameters( void ) {
	char buffer[256] ;
	if( ReadParameter( INIT_SECTION, "configdir", buffer ) ) { 
		if( strlen( buffer ) > 0 ) { if( existdirectory(buffer) ) SetConfigDirectory( buffer ) ; }
		}
	if( ReadParameter( INIT_SECTION, "initdelay", buffer ) ) { 
		init_delay = atof( buffer ) ;
		if( init_delay < 0. ) init_delay = 2.0 ; 
		}
	if( ReadParameter( INIT_SECTION, "commanddelay", buffer ) ) { autocommand_delay = atof( buffer ) ; }
#if (defined IMAGEPORT) && (!defined FDJ)
	if( ReadParameter( INIT_SECTION, "backgroundimage", buffer ) ) { if( !stricmp( buffer, "NO" ) ) BackgroundImageFlag = 0 ; }
	if( ReadParameter( INIT_SECTION, "backgroundimage", buffer ) ) { if( !stricmp( buffer, "YES" ) ) BackgroundImageFlag = 1 ; }
#endif
	if( ReadParameter( INIT_SECTION, "bcdelay", buffer ) ) { between_char_delay = atoi( buffer ) ; }
	if( ReadParameter( INIT_SECTION, "browsedirectory", buffer ) ) { 
		if( !stricmp( buffer, "NO" ) ) DirectoryBrowseFlag = 0 ; 
		else if( (!stricmp( buffer, "YES" )) && (IniFileFlag==SAVEMODE_DIR) ) DirectoryBrowseFlag = 1 ;
		}
	if( ReadParameter( INIT_SECTION, "capslock", buffer ) ) { if( !stricmp( buffer, "YES" ) ) CapsLockFlag = 1 ; }
	if( ReadParameter( INIT_SECTION, "conf", buffer ) ) { if( !stricmp( buffer, "NO" ) ) NoKittyFileFlag = 1 ; }
	if( ReadParameter( INIT_SECTION, "debug", buffer ) ) { if( !stricmp( buffer, "YES" ) ) debug_flag = 1 ; }
	if( ReadParameter( INIT_SECTION, "icon", buffer ) ) { if( !stricmp( buffer, "YES" ) ) IconeFlag = 1 ; }
	if( ReadParameter( INIT_SECTION, "iconfile", buffer ) ) {
		if( existfile( buffer ) ) {
			if( IconFile != NULL ) free( IconFile ) ;
			IconFile = (char*) malloc( strlen(buffer)+1 ) ;
			strcpy( IconFile, buffer ) ;
			if( ReadParameter( INIT_SECTION, "numberoficons", buffer ) ) { NumberOfIcons = atof( buffer ) ; }
			}
		}
	if( ReadParameter( INIT_SECTION, "internaldelay", buffer ) ) { 
		internal_delay = atoi( buffer ) ; 
		if( internal_delay < 1 ) internal_delay = 1 ;
		}
	if( ReadParameter( INIT_SECTION, "KiPP", buffer ) != 0 ) {
		if( decryptstring( buffer, MASTER_PASSWORD ) ) ManagePassPhrase( buffer ) ;
		}
	if( ReadParameter( INIT_SECTION, "sav", buffer ) ) { 
		if( strlen( buffer ) > 0 ) {
			if( KittySavFile!=NULL ) free( KittySavFile ) ;
			KittySavFile=(char*)malloc( strlen(buffer)+1 ) ;
			strcpy( KittySavFile, buffer) ;
			}
		}
	if( ReadParameter( INIT_SECTION, "antiidle", buffer ) ) { strcpy( AntiIdleStr, buffer ) ; }
	if( ReadParameter( INIT_SECTION, "antiidledelay", buffer ) ) 
		{ AntiIdleCountMax = (int) (atoi( buffer )/10) ; if( AntiIdleCountMax<=0 ) AntiIdleCountMax =1 ; }
	if( ReadParameter( INIT_SECTION, "shortcuts", buffer ) ) { if( !stricmp( buffer, "NO" ) ) ShortcutsFlag = 0 ; }
	if( ReadParameter( INIT_SECTION, "size", buffer ) ) { if( !stricmp( buffer, "YES" ) ) SizeFlag = 1 ; }
	if( ReadParameter( INIT_SECTION, "slidedelay", buffer ) ) { ImageSlideDelay = atoi( buffer ) ; }
	if( ReadParameter( INIT_SECTION, "sshversion", buffer ) ) { set_sshver( buffer ) ; }
	if( ReadParameter( INIT_SECTION, "wintitle", buffer ) ) {  if( !stricmp( buffer, "NO" ) ) TitleBarFlag = 0 ; }
	if( ReadParameter( INIT_SECTION, "paste", buffer ) ) {  if( !stricmp( buffer, "YES" ) ) PasteCommandFlag = 1 ; }
	
	if( ReadParameter( INIT_SECTION, "WinSCPPath", buffer ) ) {
		if( existfile( buffer ) ) { 
			if( WinSCPPath!=NULL) { free(WinSCPPath) ; WinSCPPath = NULL ; }
			WinSCPPath = (char*) malloc( strlen(buffer) + 1 ) ; strcpy( WinSCPPath, buffer ) ;
			}
		}
		
#ifndef NO_TRANSPARENCY
	if( ReadParameter( INIT_SECTION, "transparency", buffer ) ) { 
		if( !stricmp( buffer, "YES" ) ) {
			TransparencyNumber = 255 ; 
			cfg.transparencynumber = 0 ;
			}
		}
	if( ReadParameter( INIT_SECTION, "transparency", buffer ) ) { 
		if( !stricmp( buffer, "NO" ) ) {
			TransparencyNumber = -1 ; 
			cfg.transparencynumber = -1 ;
			}
		}
	if( TransparencyNumber != -1 )
		if( ReadParameter( INIT_SECTION, "transparencyvalue", buffer ) ) {
			TransparencyNumber = 255 - atoi( buffer ) ;
			cfg.transparencynumber = atoi( buffer ) ;
			}
#endif

#if (defined IMAGEPORT) && (!defined FDJ)
	if( ReadParameter( INIT_SECTION, "shrinkbitmap", buffer ) ) { if( !stricmp( buffer, "NO" ) ) SetShrinkBitmapEnable(0) ; }
#endif
	if( readINI( KittyIniFile, "ConfigBox", "height", buffer ) ) {
		ConfigBoxHeight = atoi( buffer ) ;
		}
	if( readINI( KittyIniFile, "ConfigBox", "windowheight", buffer ) ) {
		ConfigBoxWindowHeight = atoi( buffer ) ;
		}
	if( readINI( KittyIniFile, "ConfigBox", "filter", buffer ) ) {
		if( !stricmp( buffer, "NO" ) ) SessionFilterFlag = 0 ;
		}
	if( readINI( KittyIniFile, "Print", "height", buffer ) ) {
		PrintCharSize = atoi( buffer ) ;
		}
	if( readINI( KittyIniFile, "Print", "maxline", buffer ) ) {
		PrintMaxLinePerPage = atoi( buffer ) ;
		}
	if( readINI( KittyIniFile, "Print", "maxchar", buffer ) ) {
		PrintMaxCharPerLine = atoi( buffer ) ;
		}
	if( readINI( KittyIniFile, "Folder", "del", buffer ) ) {
		StringList_Del( FolderList, buffer ) ;
		delINI( KittyIniFile, "Folder", "del" ) ;
		}

#ifdef CYGTERMPORT
	if( ReadParameter( INIT_SECTION, "cygterm", buffer ) ) {
		if( !stricmp( buffer, "YES" ) ) cygterm_set_flag( 1 ) ; 
		}
#endif
#ifdef ZMODEMPORT
	if( ReadParameter( INIT_SECTION, "zmodem", buffer ) ) {  if( !stricmp( buffer, "NO" ) ) ZModemFlag = 0 ; }
#endif
	}

// Initialisation de noms de fichiers de configuration kitty.ini et kitty.sav
// APPDATA = 	C:\Documents and Settings\U502190\Application Data sur XP
//		C:\Users\Cyril\AppData\Roaming sur Vista
void InitNameConfigFile( void ) {
	char buffer[4096] ;
	if( KittyIniFile != NULL ) free( KittyIniFile ) ; KittyIniFile=NULL ;

	sprintf( buffer, "%s\\%s", InitialDirectory, DEFAULT_INIT_FILE ) ;
#ifdef PORTABLE
	if( !existfile( buffer ) ) {
		sprintf( buffer, "%s\\putty.ini", InitialDirectory ) ;
		if( !existfile( buffer ) ) {
			sprintf( buffer, "%s\\%s", InitialDirectory, DEFAULT_INIT_FILE ) ;
#else
	if( !existfile( buffer ) ) {
		sprintf( buffer, "%s\\putty.ini", InitialDirectory ) ;
		if( !existfile( buffer ) ) {
			sprintf( buffer, "%s\\%s\\%s", getenv("APPDATA"), INIT_SECTION, DEFAULT_INIT_FILE ) ;
			if( !existfile( buffer ) ) {
				sprintf( buffer, "%s\\%s", getenv("APPDATA"), INIT_SECTION ) ;
				CreateDirectory( buffer, NULL ) ;
				sprintf( buffer, "%s\\%s\\%s", getenv("APPDATA"), INIT_SECTION, DEFAULT_INIT_FILE ) ;
				}
#endif
			}
		}

	KittyIniFile=(char*)malloc( strlen( buffer)+2 ) ; strcpy( KittyIniFile, buffer) ;

	if( KittySavFile != NULL ) free( KittySavFile ) ; KittySavFile=NULL ;
	sprintf( buffer, "%s\\%s", InitialDirectory, DEFAULT_SAV_FILE ) ;
#ifndef PORTABLE
	if( !existfile( buffer ) ) {
		sprintf( buffer, "%s\\%s\\%s", getenv("APPDATA"), INIT_SECTION, DEFAULT_SAV_FILE ) ;
		if( !existfile( buffer ) ) {
			sprintf( buffer, "%s\\%s", getenv("APPDATA"), INIT_SECTION ) ;
			CreateDirectory( buffer, NULL ) ;
			sprintf( buffer, "%s\\%s\\%s", getenv("APPDATA"), INIT_SECTION, DEFAULT_SAV_FILE ) ;
			}
		}
#endif
	KittySavFile=(char*)malloc( strlen( buffer)+2 ) ; strcpy( KittySavFile, buffer) ;
	
	sprintf( buffer, "%s\\kitty.dft", InitialDirectory ) ;
	if( existfile( KittyIniFile ) && existfile( buffer ) )  unlink( buffer ) ;
	if( !existfile( KittyIniFile ) )
		if( existfile( buffer ) ) rename( buffer, KittyIniFile ) ;
	}
	
// Ecriture de l'increment de compteurs
void WriteCountUpAndPath( void ) {
	// Sauvegarde la liste des folders
	SaveFolderList() ;
		
	// Incrémente le compteur d'utilisation
	CountUp() ;

	// Positionne la version du binaire
	WriteParameter( INIT_SECTION, "Build", BuildVersionTime ) ;
	
	// Recherche cthelper.exe s'il existe
	SearchCtHelper() ;
		
	// Recherche pscp s'il existe
	SearchPSCP() ;
	
	// Recherche WinSCP s'il existe
	SearchWinSCP() ;
	}

// Initialisation spécifique à KiTTY
void InitWinMain( void ) {
	char buffer[4096];
	int i ;
	
#ifdef FDJ
	CreateSSHHandler();
#endif

	// Initialisation de la version binaire
	sprintf( BuildVersionTime, "%s.%d @ %s", BUILD_VERSION, BUILD_SUBVERSION, BUILD_TIME ) ;
#ifdef PORTABLE
	sprintf( BuildVersionTime, "%s.%dp @ %s", BUILD_VERSION, BUILD_SUBVERSION, BUILD_TIME ) ;
#endif
#ifdef NO_TRANSPARENCY
	sprintf( BuildVersionTime, "%s.%dn @ %s", BUILD_VERSION, BUILD_SUBVERSION, BUILD_TIME ) ;
#endif	
	
	//sprintf( BuildVersionTime, "%s.%d @ %s", "0.60", 60, "07/02/2008-22:07:31(GMT)" ) ; // Pour compilation CygWin

	// Initialisation de la librairie de cryptage
	bcrypt_init( 0 ) ;
	
	// Récupere le repertoire de depart et le repertoire de la configuration pour savemode=dir
	GetInitialDirectory( InitialDirectory ) ;
	
	// Initialise les noms des fichier de configuration kitty.ini et kitty.sav
	InitNameConfigFile() ;

	// Initialisation du nom de la classe
	strcpy( KiTTYClassName, appname ) ;
#ifndef FDJ
	if( ReadParameter( INIT_SECTION, "KiClassName", buffer ) ) 
		{ if( strlen( buffer ) > 0 ) strcpy( KiTTYClassName, buffer ) ; }
	appname = KiTTYClassName ;
#endif

	// Initialiste le tableau des menus
	for( i=0 ; i < NB_MENU_MAX ; i++ ) SpecialMenu[i] = NULL ;
	
	// Test le mode de fonctionnement de la sauvegarde des sessions
	GetSaveMode() ;

	// Initialisation des parametres à partir du fichier kitty.ini
	LoadParameters() ;
	
	// Initialisation des shortcuts
	InitShortcuts() ;

	// Chargement de la base de registre si besoin
	if( IniFileFlag == SAVEMODE_REG ) { // Mode de sauvegarde registry
		// Si la clé n'existe pas ...
		if( !RegTestKey( HKEY_CURRENT_USER, TEXT(PUTTY_REG_POS) ) ) { 
			if( existfile( KittySavFile ) ) {// ... et que le fichier kitty.sav existe on le charge ...
				HWND hdlg = InfoBox( hinst, NULL ) ;
				InfoBoxSetText( hdlg, "Initializing registry" ) ;
				InfoBoxSetText( hdlg, "Loading saved sessions" ) ;
				LoadRegistryKey( hdlg ) ; 
				InfoBoxClose( hdlg ) ;
				}
			else // Sinon on regarde si il y a la clé de PuTTY et on la récupère
				TestRegKeyOrCopyFromPuTTY( HKEY_CURRENT_USER, TEXT(PUTTY_REG_POS) ) ; 
			}
		}
	else if( IniFileFlag == SAVEMODE_FILE ){ // Mode de sauvegarde fichier
		if( !RegTestKey( HKEY_CURRENT_USER, TEXT(PUTTY_REG_POS) ) ) { // la clé de registre n'existe pas 
			HWND hdlg = InfoBox( hinst, NULL ) ;
			InfoBoxSetText( hdlg, "Initializing registry" ) ;
			InfoBoxSetText( hdlg, "Loading saved sessions" ) ;
			LoadRegistryKey( hdlg ) ; 
			InfoBoxClose( hdlg ) ;
			}
		else { // la clé de registre existe déjà
			if( WindowsCount( MainHwnd ) == 1 ) { // Si c'est le 1er kitty on sauvegarde la clé de registre avant de charger le fichier kitty.sav
				HWND hdlg = InfoBox( hinst, NULL ) ;
				InfoBoxSetText( hdlg, "Initializing registry" ) ;
				RegRenameTree( hdlg, HKEY_CURRENT_USER, TEXT(PUTTY_REG_POS), TEXT(PUTTY_REG_POS_SAVE) ) ;
				InfoBoxSetText( hdlg, "Loading saved sessions" ) ;
				LoadRegistryKey( hdlg ) ;
				InfoBoxClose( hdlg ) ;
				}
			}
		}
	else if( IniFileFlag == SAVEMODE_DIR ){ // Mode de sauvegarde directory
		}

	// Creer les clés nécessaires au programme
	sprintf( buffer, "%s\\%s", TEXT(PUTTY_REG_POS), "Commands" ) ;
	if( (IniFileFlag == SAVEMODE_REG)||( IniFileFlag == SAVEMODE_FILE) ) 
		RegTestOrCreate( HKEY_CURRENT_USER, buffer, NULL, NULL ) ;
	
	// Initialisation du launcher
	sprintf( buffer, "%s\\%s", TEXT(PUTTY_REG_POS), "Launcher" ) ;
	if( (IniFileFlag == SAVEMODE_REG)||( IniFileFlag == SAVEMODE_FILE) )  
		if( !RegTestKey( HKEY_CURRENT_USER, buffer ) ) { InitLauncherRegistry() ; }

	// Initialise la liste des folders
	InitFolderList() ;
	
	// Incremente et ecrit les compteurs
	if( IniFileFlag == SAVEMODE_REG ) {
		WriteCountUpAndPath() ;
		}
		
	// Initialise la gestion des icones depuis la librairie kitty.dll si elle existe
	if( !PuttyFlag ) {
		if( IconFile != NULL )
		if( existfile( IconFile ) ) 
			{ HMODULE hDll ; if( ( hDll = LoadLibrary( TEXT(IconFile) ) ) != NULL ) hInstIcons = hDll ; }
		if( hInstIcons==NULL )
		if( existfile( "kitty.dll" ) ) 
			{ HMODULE hDll ; if( ( hDll = LoadLibrary( TEXT("kitty.dll") ) ) != NULL ) hInstIcons = hDll ; }
		}

	// Teste la presence d'une note et l'affiche
	if( GetValueData( HKEY_CURRENT_USER, TEXT(PUTTY_REG_POS), "Notes", buffer ) ) 
		{ if( strlen( buffer ) > 0 ) MessageBox( NULL, buffer, "Notes", MB_OK ) ; }
		
	// Genère un fichier d'initialisation de toute les Sessions
	sprintf( buffer, "%s\\kitty.ses.updt", InitialDirectory ) ;
	if( existfile( buffer ) ) { InitAllSessions( HKEY_CURRENT_USER, TEXT(PUTTY_REG_POS), "Sessions", buffer ) ; }
	}


/* Pour compilation 64bits */
/*
void bzero (void *s, size_t n){ memset (s, 0, n); }
void bcopy (const void *src, void *dest, size_t n){ memcpy (dest, src, n); }
int bcmp (const void *s1, const void *s2, size_t n){ return memcmp (s1, s2, n); }
*/


	
//`colours'
int return_offset_height(void) { return offset_height ; }
int return_offset_width(void) { return offset_width ; }
int return_font_height(void) { return font_height ; }
int return_font_width(void) { return font_width ; }
COLORREF return_colours258(void) { return colours[258]; }