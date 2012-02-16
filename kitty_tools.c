#include "kitty_tools.h"

int strgrep( const char * pattern, const char * str ) {
	int return_code = 1 ;
	regex_t preg ;

	if( (return_code = regcomp (&preg, pattern, REG_NOSUB | REG_EXTENDED ) ) == 0 ) {
		return_code = regexec( &preg, str, 0, NULL, 0 ) ;
		regfree( &preg ) ;
		}

	return return_code ;
	}

char *stristr (const char *meule_de_foin, const char *aiguille) {
	char *c1, *c2, *res = NULL ; int i ;
	c1=(char*)malloc( strlen(meule_de_foin) + 1 ) ; strcpy( c1, meule_de_foin ) ;
	c2=(char*)malloc( strlen(aiguille) + 1 ) ; strcpy( c2, aiguille ) ;
	if( strlen(c1)>0 ) {for( i=0; i<strlen(c1); i++ ) c1[i]=toupper( c1[i] ) ;}
	if( strlen(c2)>0 ) {for( i=0; i<strlen(c2); i++ ) c2[i]=toupper( c2[i] ) ;}
	res=strstr(c1,c2);
	if( res!=NULL ) res = (char*)(meule_de_foin+( res-c1 )) ;
	free( c2 ) ;
	free( c1 ) ;
	return res ;
	}

/* Fonction permettant d'inserer une chaine dans une autre */
int insert( char * ch, const char * c, const int ipos ) {
	int i = ipos, len = strlen( c ), k ;
	if( ( ch == NULL ) || ( c == NULL ) ) return -1 ;
	if( len > 0 ) {
		if( (size_t) i > ( strlen( ch ) + 1 ) ) i = strlen( ch ) + 1 ;
		for( k = strlen( ch ) ; k >= ( i - 1 ) ; k-- ) ch[k + len] = ch[k] ;
		for( k = 0 ; k < len ; k++ ) ch[k + i - 1] = c[k] ; 
		}
	return strlen( ch ) ; 
	}

/* Fonction permettant de supprimer une partie d'une chaine de caracteres */
int del( char * ch, const int start, const int length ) {
	int k, len = strlen( ch ) ;
	if( ch == NULL ) return -1 ;
	if( ( start == 1 ) && ( length >= len ) ) { ch[0] = '\0' ; len = 0 ; }
	if( ( start > 0 ) && ( start <= len ) && ( length > 0 ) ) {
		for( k = start - 1 ; k < ( len - length ) ; k++ ) {
			if( k < ( len - length ) ) ch[k] = ch[ k + length ] ;
			else ch = '\0' ; 
			}
		k = len - length ;
		if( ( start + length ) > len ) k = start - 1 ;
		ch[k] = '\0' ; 
		}
	return strlen( ch ) ; 
	}

/* Fonction permettant de retrouver la position d'une chaine dans une autre chaine */
int poss( const char * c, const char * ch ) {
	char * c1 , * ch1 , * cc ;
	int res ;
	if( ( ch == NULL ) || ( c == NULL ) ) return -1 ;
	if( ( c1 = (char *) malloc( strlen( c ) + 1 ) ) == NULL ) return -2 ;
	if( ( ch1 = (char *) malloc( strlen( ch ) + 1 ) ) == NULL ) { free( c1 ) ; return -3 ; }
	strcpy( c1, c ) ; strcpy( ch1, ch ) ;
	cc = (char *) strstr( ch1, c1 ) ;
	if( cc == NULL ) res = 0 ;
	else res = (int) ( cc - ch1 ) + 1 ;
	if( (size_t) res > strlen( ch ) ) res = 0 ;
	free( ch1 ) ;
	free( c1 ) ;
	return res ; 
	}

// Teste l'existance d'un fichier
int existfile( const char * filename ) {
	struct _stat statBuf ;
	
	if( filename == NULL ) return 0 ;
	if( strlen(filename)==0 ) return 0 ;
	if( _stat( filename, &statBuf ) == -1 ) return 0 ;
	
	if( ( statBuf.st_mode & _S_IFMT ) == _S_IFREG ) { return 1 ; }
	else { return 0 ; }
	}
	
// Teste l'existance d'un repertoire
int existdirectory( const char * filename ) {
	struct _stat statBuf ;
	
	if( filename == NULL ) return 0 ;
	if( strlen(filename)==0 ) return 0 ;
	if( _stat( filename, &statBuf ) == -1 ) return 0 ;
	
	if( ( statBuf.st_mode & _S_IFMT ) == _S_IFDIR ) { return 1 ; }
	else { return 0 ; }
	}

/* Donne la taille d'un fichier */
long filesize( const char * filename ) {
	FILE * fp ;
	long length ;

	if( filename == NULL ) return 0 ;
	if( strlen( filename ) <= 0 ) return 0 ;
	
	if( ( fp = fopen( filename, "r" ) ) == 0 ) return 0 ;
	
	fseek( fp, 0L, SEEK_END ) ;
	length = ftell( fp ) ;
	
	fclose( fp ) ;
	return length ;
	}

// Supprime les double anti-slash
void DelDoubleBackSlash( char * st ) {
	int i=0,j ;
	while( st[i] != '\0' ) {
		if( (st[i] == '\\' )&&(st[i+1]=='\\' ) ) {
			for( j=i+1 ; j<strlen( st ) ; j++ ) st[j]=st[j+1] ;
			}
		else i++ ;
		}
	}

// Ajoute une chaine dans une liste de chaines
int StringList_Add( char **list, const char * name ) {
	int i = 0 ;
	if( name == NULL ) return 1 ;
	while( list[i] != NULL ) {
		if( !stricmp( name, list[i] ) ) return 1 ;
		i++ ;
		}
	if( ( list[i] = (char*) malloc( strlen( name ) + 1 ) ) == NULL ) return 0 ;
	strcpy( list[i], name ) ;
	list[i+1] = NULL ;
	return 1 ;
	}

// Test si une chaine existe dans une liste de chaines
int StringList_Exist( const char **list, const char * name ) {
	int i = 0 ;
	while( list[i] != NULL ) {
		if( strlen( list[i] ) > 0 )
			if( !strcmp( list[i], name ) ) return 1 ;
		i++ ;
		}
	return 0 ;
	}
	
// Supprime une chaine d'une liste de chaines
void StringList_Del( char **list, const char * name ) {
	int i = 0 ;
	while( list[i] != NULL ) {
		if( strlen( list[i] ) > 0 )
			if( !strcmp( list[i], name ) ) {
				strcpy( list[i], "" ) ;
				}
		i++;
		}
	}

// Reorganise l'ordre d'une liste de chaines en montant la chaine selectionnee d'un cran
void StringList_Up( char **list, const char * name ) {
	char *buffer ;
	int i = 0 ;
	while( list[i] != NULL ) {
		if( !strcmp( list[i], name ) ) {
			if( i > 0 ) {
				buffer=(char*)malloc( strlen(list[i-1])+1 ) ;
				strcpy( buffer, list[i-1] ) ;
				free( list[i-1] ) ; list[i-1] = NULL ;
				list[i-1]=(char*)malloc( strlen(list[i])+1 ) ;
				strcpy( list[i-1], list[i] ) ;
				free( list[i] ) ;
				list[i] = (char*) malloc( strlen( buffer ) +1 ) ;
				strcpy( list[i], buffer );
				free( buffer );
				}
			return ;
			}
		i++ ;
		}
	}

// Positionne l'environnement
int putenv (const char *string) ;
int set_env( char * name, char * value ) {
	int res = 0 ;
	char * buffer = NULL ;
	if( (buffer = (char*) malloc( strlen(name)+strlen(value)+2 ) ) == NULL ) return -1 ;
	sprintf( buffer,"%s=%s", name, value ) ; 
	res = putenv( (const char *) buffer ) ;
	free( buffer ) ;
	return res ;
	}
int add_env( char * name, char * value ) {
	int res = 0 ;
	char * npst = getenv( name ), * vpst = NULL ;
	if( npst==NULL ) { res = set_env( name, value ) ; }
	else {
		vpst = (char*) malloc( strlen(npst)+strlen(value)+20 ) ; 
		sprintf( vpst, "%s=%s;%s", name, npst, value ) ;
		res = set_env( name, vpst ) ;
		free( vpst ) ;
		}
	return res ;
	}
