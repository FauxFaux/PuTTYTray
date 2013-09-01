#ifndef KITTY_REGISTRY
#define KITTY_REGISTRY

#include <stdlib.h>
#include <stdio.h>
#include <windows.h>

#ifndef MAX_KEY_LENGTH 
#define MAX_KEY_LENGTH 255
#endif
#ifndef MAX_VALUE_NAME
#define MAX_VALUE_NAME 16383
#endif

char * GetValueData(HKEY hkTopKey, char * lpSubKey, const char * lpValueName, char * rValue) ;


// Teste l'existance d'une cl�
int RegTestKey( HKEY hMainKey, LPCTSTR lpSubKey ) ;

// Retourne le nombre de sous-keys
int RegCountKey( HKEY hMainKey, LPCTSTR lpSubKey ) ;

// Teste l'existance d'une cl� ou bien d'une valeur et la cr�e sinon
void RegTestOrCreate( HKEY hMainKey, LPCTSTR lpSubKey, LPCTSTR name, LPCTSTR value ) ;
	
// Test l'existance d'une cl� ou bien d'une valeur DWORD et la cr�e sinon
void RegTestOrCreateDWORD( HKEY hMainKey, LPCTSTR lpSubKey, LPCTSTR name, DWORD value ) ;

// Initialise toutes les sessions avec une valeur
void QuerySubKey( HKEY hMainKey, LPCTSTR lpSubKey, FILE * fp_out, char * text  ) ;

// D�truit une valeur de cl� de registre 
BOOL RegDelValue (HKEY hKeyRoot, LPTSTR lpSubKey, LPTSTR lpValue ) ;

// Detruit une cl� de registre et ses sous-cl�
BOOL RegDelTree (HKEY hKeyRoot, LPCTSTR lpSubKey) ;

// Copie une cl� de registre vers une autre
void RegCopyTree( HKEY hMainKey, LPCTSTR lpSubKey, LPCTSTR lpDestKey ) ;

// Nettoie la cl� de PuTTY pour enlever les cl�s et valeurs sp�cifique � KiTTY
BOOL RegCleanPuTTY( void ) ;

// Creation du SSH Handler
void CreateSSHHandler() ;

// V�rifie l'existance de la cl� de KiTTY sinon la copie depuis PuTTY
void TestRegKeyOrCopyFromPuTTY( HKEY hMainKey, char * KeyName ) ;

void InitRegistryAllSessions( HKEY hMainKey, LPCTSTR lpSubKey, char * SubKeyName, char * filename, char * text ) ;
void InitAllSessions( HKEY hMainKey, LPCTSTR lpSubKey, char * SubKeyName, char * filename ) ;
	
#endif
