#ifndef KITTY_WIN
#define KITTY_WIN

#include <stdlib.h>
#include <stdio.h>
#include <windows.h>

void SetTransparency( HWND hwnd, int value ) ;
void GetOSInfo( char * version ) ;
int OpenFileName( HWND hFrame, char * filename, char * Title, char * Filter ) ;
int OpenDirName( HWND hFrame, char * dirname ) ;

// Centre un dialog au milieu de la fenetre parent
void CenterDlgInParent(HWND hDlg) ;

// Envoi vers l'imprimante
int PrintText( const char * Text ) ;

// Impression du texte dans le bloc-notes
void ManagePrint( HWND hwnd ) ;

// Met un texte dans le press-papier
int SetTextToClipboard( const char * buf ) ;

// Execute une commande	
void RunCommand( HWND hwnd, char * cmd ) ;

// Démarre l'éditeur embarqué
void RunPuttyEd( HWND hwnd ) ;

#endif
