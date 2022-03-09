#ifdef __MSDOS__
#include "osbind.h"
#else
#include <osbind.h>
#endif

/************************************************************************/
/************************************************************************/
/************************************************************************/

unsigned char readbyte( int fhand )
{
char chr;

	Fread( fhand, 1L, &chr );
	return( chr );
}

unsigned short readshort( int fhand )
{
short s;

	Fread( fhand, 2L, &s );
#ifdef __MSDOS__
	s = (s << 8) | (s >> 8);
#endif
	return( s );
}

unsigned long readlong( int fhand )
{
unsigned long l;
#ifdef __MSDOS__
unsigned long a, b, c, d;
#endif

	Fread( fhand, 4L, &l );
#ifdef __MSDOS__
	a = (l >> 24) & 0x000000ffL;
	b = (l >> 8) & 0x0000ff00L;
	c = (l << 8) & 0x00ff0000L;
	d = (l << 24) & 0xff000000L;
	return( a | b | c | d );
#else
	return( l );
#endif
}

