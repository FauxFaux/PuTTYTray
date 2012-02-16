/*
 * Fichier contenant les procedures communes à tous les programmes putty, pscp, psftp, plink, pageant
 */

#include "kitty_commun.h"

#ifdef PERSOPORT

// Flag pour le fonctionnement en mode "portable" (gestion par fichiers)
extern int IniFileFlag ;

// Flag permettant la gestion de l'arborscence (dossier=folder) dans le cas d'un savemode=dir
extern int DirectoryBrowseFlag ;


#ifndef SAVEMODE_REG
#define SAVEMODE_REG 0
#endif
#ifndef SAVEMODE_FILE
#define SAVEMODE_FILE 1
#endif
#ifndef SAVEMODE_DIR
#define SAVEMODE_DIR 2
#endif

// Répertoire de sauvegarde de la configuration (savemode=dir)
char * ConfigDirectory = NULL ;

char * GetConfigDirectory( void ) { return ConfigDirectory ; }

int stricmp(const char *s1, const char *s2) ;
int readINI( const char * filename, const char * section, const char * key, char * pStr) ;
char * SetSessPath( const char * dec ) ;

// Nettoie les noms de folder en remplaçant les "/" par des "\" et les " \ " par des " \"
void CleanFolderName( char * folder ) {
	int i, j ;
	if( folder == NULL ) return ;
	if( strlen( folder ) == 0 ) return ;
	for( i=0 ; i<strlen(folder) ; i++ ) if( folder[i]=='/' ) folder[i]='\\' ;
	for( i=0 ; i<(strlen(folder)-1) ; i++ ) 
		if( folder[i]=='\\' ) 
			while( folder[i+1]==' ' ) for( j=i+1 ; j<strlen(folder) ; j++ ) folder[j]=folder[j+1] ;
	for( i=(strlen(folder)-1) ; i>0 ; i-- )
		if( folder[i]=='\\' )
			while( folder[i-1]==' ' ) {
				for( j=i-1 ; j<strlen(folder) ; j++ ) folder[j]=folder[j+1] ;
				i-- ;
				}
	}

#include <sys/types.h>
#include <dirent.h>
#define MAX_VALUE_NAME 16383
// Supprime une arborescence
void DelDir( const char * directory ) {
	DIR * dir ;
	struct dirent * de ;
	char fullpath[MAX_VALUE_NAME] ;

	if( (dir=opendir(directory)) != NULL ) {
		while( (de=readdir( dir ) ) != NULL ) 
		if( strcmp(de->d_name,".") && strcmp(de->d_name,"..") ) {
			sprintf( fullpath, "%s\\%s", directory, de->d_name ) ;
			if( GetFileAttributes( fullpath ) & FILE_ATTRIBUTE_DIRECTORY ) { DelDir( fullpath ) ; }
			else if( !(GetFileAttributes( fullpath ) & FILE_ATTRIBUTE_DIRECTORY) ) { unlink( fullpath ) ; }
			}
		closedir( dir ) ;
		_rmdir( directory ) ;
		}
	}


/* test if we are in portable mode by looking for putty.ini or kitty.ini in running directory */
int IsPortableMode( void ) {
	FILE * fp = NULL ;
	int ret = 0 ;
	char buffer[256] ;
		
	if( (fp = fopen( "putty.ini", "r" )) != NULL ) {
		fclose(fp ) ;
		if( readINI( "putty.ini", "PuTTY", "savemode", buffer ) ) {
			while( (buffer[strlen(buffer)-1]=='\n')||(buffer[strlen(buffer)-1]=='\r')
				||(buffer[strlen(buffer)-1]==' ')
				||(buffer[strlen(buffer)-1]=='\t') ) buffer[strlen(buffer)-1]='\0';
			if( !stricmp( buffer, "registry" ) ) IniFileFlag = SAVEMODE_REG ;
			else if( !stricmp( buffer, "file" ) ) IniFileFlag = SAVEMODE_FILE ;
			else if( !stricmp( buffer, "dir" ) ) { IniFileFlag = SAVEMODE_DIR ; DirectoryBrowseFlag = 1 ; ret = 1 ; }
			}
		if(  IniFileFlag == SAVEMODE_DIR ) {
			if( readINI( "putty.ini", "PuTTY", "browsedirectory", buffer ) ) {
				if( !stricmp( buffer, "NO" )&&(IniFileFlag==SAVEMODE_DIR) ) DirectoryBrowseFlag = 0 ; 
				else DirectoryBrowseFlag = 1 ;
				}
			if( readINI( "putty.ini", "PuTTY", "configdir", buffer ) ) { 
				if( strlen( buffer ) > 0 ) { 
					ConfigDirectory = (char*)malloc( strlen(buffer) + 1 ) ;
					strcpy( ConfigDirectory, buffer ) ;
					}
				}
			}
		else  DirectoryBrowseFlag = 0 ;
		}
	else if( (fp = fopen( "kitty.ini", "r" )) != NULL ) {
		fclose(fp ) ;
		if( readINI( "kitty.ini", "KiTTY", "savemode", buffer ) ) {
			while( (buffer[strlen(buffer)-1]=='\n')||(buffer[strlen(buffer)-1]=='\r')
				||(buffer[strlen(buffer)-1]==' ')
				||(buffer[strlen(buffer)-1]=='\t') ) buffer[strlen(buffer)-1]='\0';
			if( !stricmp( buffer, "registry" ) ) IniFileFlag = SAVEMODE_REG ;
			else if( !stricmp( buffer, "file" ) ) IniFileFlag = SAVEMODE_FILE ;
			else if( !stricmp( buffer, "dir" ) ) { IniFileFlag = SAVEMODE_DIR ; ret = 1 ; }
			}
		if(  IniFileFlag == SAVEMODE_DIR ) {
			if( readINI( "kitty.ini", "KiTTY", "browsedirectory", buffer ) ) { 
				if( !stricmp( buffer, "NO" )&&(IniFileFlag==SAVEMODE_DIR) ) DirectoryBrowseFlag = 0 ; 
				else DirectoryBrowseFlag = 1 ;
				}
			if( readINI( "kitty.ini", "KiTTY", "configdir", buffer ) ) { 
				if( strlen( buffer ) > 0 ) { 
					ConfigDirectory = (char*)malloc( strlen(buffer) + 1 ) ;
					strcpy( ConfigDirectory, buffer ) ;
					}
				}
			}
		else  DirectoryBrowseFlag = 0 ;
		}
	else { printf( "No ini file\n" ) ; }
	return ret ;
	}

// Positionne un flag permettant de determiner si on est connecte
int backend_connected = 0 ;

void SetSSHConnected( void ) { backend_connected = 1 ; }

#endif
