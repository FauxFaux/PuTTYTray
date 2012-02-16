#include <stdio.h>
#include <stdlib.h>

#ifndef _REGERROR_CPP
#define _REGERROR_CPP

void
regerror( char* s)
{
#ifdef ERRAVAIL
	error("regexp: %s", s);
#else
	fprintf(stderr, "regexp(3): %s", s);
	exit(1);
#endif
	/* NOTREACHED */
}

#endif
