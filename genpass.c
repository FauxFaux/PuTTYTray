
/*
gcc -o genpass.exe genpass.c bcrypt.a
gcc -o unpass.exe genpass.c bcrypt.a -DUNPASS
gcc -o uncrypt.exe genpass.c bcrypt.a -DUNCRYPT -DMASTER_PASSWORD=`cat masterpassword.txt`
*/

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include "bcrypt.h"

char PassKey[1024] ="" ;
int cryptstring( char * st, const char * key ) { return bcrypt_string_base64( st, st, strlen( st ), key, 0 ) ; }
int decryptstring( char * st, const char * key ) { return buncrypt_string_base64( st, st, strlen( st ), key ) ; }

static const char hex[16] = "0123456789ABCDEF";
void mungestr(const char *in, char *out)
{
    int candot = 0;

    while (*in) {
	if (*in == ' ' || *in == '\\' || *in == '*' || *in == '?' || 
	*in ==':' || *in =='/' || *in =='\"' || *in =='<' || *in =='>' || *in =='|' ||
	    *in == '%' || *in < ' ' || *in > '~' || (*in == '.'
						     && !candot)) {
	    *out++ = '%';
	    *out++ = hex[((unsigned char) *in) >> 4];
	    *out++ = hex[((unsigned char) *in) & 15];
	} else
	    *out++ = *in;
	in++;
	candot = 1;
    }
    *out = '\0';
    return;
}

#define PUTTY_REG_POS "Software\\9bis.com\\KiTTY"
int GetSessionField( const char * session_in, const char * field, char * result ) {
	HKEY hKey ;
	char buffer[1024], session[1024], folder[1024], *p ;
	int res = 0 ;
	FILE * fp ;
	
	strcpy( result, "" ) ;
	strcpy( buffer, session_in ) ;
	if( (p = strrchr(buffer, '[')) != NULL ) *(p-1) = '\0' ;
	mungestr(buffer, session) ;
	sprintf( buffer, "%s\\Sessions\\%s", PUTTY_REG_POS, session ) ;

	if( RegOpenKeyEx( HKEY_CURRENT_USER, buffer, 0, KEY_READ, &hKey) == ERROR_SUCCESS ) {
		DWORD lpType ;
		unsigned char lpData[1024] ;
		DWORD dwDataSize = 1024 ;
		if( RegQueryValueEx( hKey, field, 0, &lpType, lpData, &dwDataSize ) == ERROR_SUCCESS ) {
			strcpy( result, lpData ) ;
			res = 1 ;
			}
		RegCloseKey( hKey ) ;
		}
	return res ;
	}

void GenerePassword( char * filename, char * pass ) {
	char password[1024]="", reg[1024], session[1024], hostname[1024], PassKey[1024]="", TerminalType[1024]="" ;
	FILE *fp ;

	if( (fp=fopen(filename,"r"))!=NULL ) {
		printf( "Windows Registry Editor Version 5.00\n\n" ) ;
		while( fgets( reg, 1024, fp ) != NULL ) {
			while( (reg[strlen(reg)-1]=='\n')||(reg[strlen(reg)-1]=='\r') ) reg[strlen(reg)-1]='\0';
			if( strlen( reg ) > 0 ) {
				GetSessionField( reg, "HostName", hostname ) ;
				GetSessionField( reg, "TerminalType", TerminalType ) ;
				sprintf( PassKey, "%s%sKiTTY", hostname, TerminalType ) ;
				mungestr( reg, session);
				printf( "[HKEY_CURRENT_USER\\Software\\9bis.com\\KiTTY\\Sessions\\%s]\n", session );
				strcpy( password, pass ) ;
				cryptstring( password, PassKey ) ;
				printf( "\"Password\"=\"%s\"\n\n", password ) ;
				}
			}
		fclose( fp ) ;
		}
	else { fprintf( stderr, "Unable to open filename %s\n", filename ); }
	}

#ifdef UNCRYPT

int main( int argc, char **argv, char **arge) {
	char st[1024] = "" ;
	int res ;
	if( argc!=2 ) {
		fprintf(stderr, "Usage: %s String\n", argv[0] );
		exit(1) ; 
		}
	strcpy( st, argv[1] ) ;

	if( ( res = buncrypt_string_base64( st, st, strlen( st ), MASTER_PASSWORD) ) > 0 )
		fwrite( st, 1, res, stdout ) ;
	printf( "\n" );
	return 0 ;
	}

#else

int main( int argc, char **argv, char **arge) {
	char password[1024]="" ;
	char hostname[1024]="" ;
	char termtype[1024]="" ;
	char PassKey[1024]="";
	int res ;
	
	if( (argc!=3) && (argc!=4) ) {
		fprintf(stderr, "Usage: %s Password Hostname [Terminal-type string]\n", argv[0] );
		exit(1);
		}
		
#ifndef UNPASS
	if( !strcmp( argv[1], "-f" ) ) {
		if( argc!=4 ) {	fprintf( stderr, "Usage: %s -f filename 'password'\n", argv[0] ) ; exit(1) ; }
		bcrypt_init( 0 ) ;
		GenerePassword( argv[2], argv[3] ) ; exit(0) ;
		}
#endif
	
	strcpy( password, argv[1] ) ; //printf( "Password ? " ) ; fgets( password, 1023, stdin ) ;
	strcpy( hostname, argv[2] ) ; //printf( "Hostname ? " ) ; fgets( hostname, 1023, stdin ) ;
	if( argc==4 ) strcpy( termtype, argv[3] ) ; //printf( "Terminal-type string [xterm] ? " ) ; fgets( termtype, 1023, stdin ) ;
	else strcpy( termtype, "xterm" ) ;
	
	while( (password[strlen(password)-1]=='\n')||(password[strlen(password)-1]=='\r') ) password[strlen(password)-1]='\0' ;
	while( (hostname[strlen(hostname)-1]=='\n')||(hostname[strlen(hostname)-1]=='\r') ) hostname[strlen(hostname)-1]='\0' ;
	while( (termtype[strlen(termtype)-1]=='\n')||(termtype[strlen(termtype)-1]=='\r') ) termtype[strlen(termtype)-1]='\0' ;
	
	if( (strlen(password)==0)||(strlen(hostname)==0) ) 
		{ fprintf(stderr, "Wrong parameter\n" );exit(2); }
	if( strlen( termtype) == 0 ) strcpy(termtype,"xterm");

	sprintf( PassKey, "%s%sKiTTY", hostname, termtype ) ;

#ifdef UNPASS
	res = decryptstring( password, PassKey ) ;
#else
	bcrypt_init( 0 ) ;
	res = cryptstring( password, PassKey ) ;
#endif
	printf("%s\n",password);

	return 0 ;
	}

#endif
