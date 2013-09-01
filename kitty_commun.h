#ifndef KITTY_COMMUN
#define KITTY_COMMUN

#include <stdlib.h>
#include <stdio.h>
#include <windows.h>

// Flag permettant d'activer l'acces a du code particulier permettant d'avoir plus d'info dans le kitty.dmp
static int debug_flag = 0 ;

// Répertoire de sauvegarde de la configuration (savemode=dir)
extern char * ConfigDirectory ;

char * GetConfigDirectory( void ) ;

int stricmp(const char *s1, const char *s2) ;
int readINI( const char * filename, const char * section, const char * key, char * pStr) ;
char * SetSessPath( const char * dec ) ;

// Nettoie les noms de folder en remplaçant les "/" par des "\" et les " \ " par des " \"
void CleanFolderName( char * folder ) ;

// Supprime une arborescence
void DelDir( const char * directory ) ;

/* test if we are in portable mode by looking for putty.ini or kitty.ini in running directory */
int IsPortableMode( void ) ;

// Positionne un flag permettant de determiner si on est connecte
extern int backend_connected ;

void SetSSHConnected( void ) ;

// Affichage d'un message dans l'event log
void debug_logevent( const char *fmt, ... ) ;

PVOID SecureZeroMemory( PVOID ptr, SIZE_T cnt) ;
#endif
