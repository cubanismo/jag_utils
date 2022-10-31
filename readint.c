#include "osbind.h"

/* Why does VC++ make this so hard? Punting. */
#if defined(__MSDOS__) || defined(_WIN32)
#define SWAP_BYTES 1
#else
#include <endian.h>
#if BYTE_ORDER == LITTLE_ENDIAN
#define SWAP_BYTES 1
#endif
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
unsigned short s;

	Fread( fhand, 2L, &s );
#ifdef SWAP_BYTES
	s = (s << 8) | (s >> 8);
#endif
	return( s );
}

unsigned long readlong( int fhand )
{
unsigned long l;
#ifdef SWAP_BYTES
unsigned long a, b, c, d;
#endif

	Fread( fhand, 4L, &l );
#ifdef SWAP_BYTES
	a = (l >> 24) & 0x000000ffL;
	b = (l >> 8) & 0x0000ff00L;
	c = (l << 8) & 0x00ff0000L;
	d = (l << 24) & 0xff000000L;
	return( a | b | c | d );
#else
	return( l );
#endif
}

long writelong( int fhand, long lval )
{
unsigned l;
#ifdef SWAP_BYTES
unsigned long a, b, c, d;

	a = (lval >> 24) & 0x000000ffL;
	b = (lval >> 8) & 0x0000ff00L;
	c = (lval << 8) & 0x00ff0000L;
	d = (lval << 24) & 0xff000000L;
	l = (a | b | c | d);
#else
	l = lval;
#endif
	return Fwrite( fhand, 4L, &l );
}
	
long writeshort( int fhand, short sval )
{
#ifdef SWAP_BYTES
	sval = (sval << 8) | (sval >> 8);
#endif
	return Fwrite( fhand, 2L, &sval );
}
