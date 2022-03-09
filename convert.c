
long cvt_long_m2i( long l )
{
unsigned long a, b, c, d;

	a = (l >> 24L) & 0x000000ffL;
	b = (l >> 8L) & 0x0000ff00L;
	c = (l << 8L) & 0x00ff0000L;
	d = (l << 24L) & 0xff000000L;
	return( a | b | c | d );
}

int cvt_int_m2i( int i )
{
unsigned int a, b;

	a = (i >> 8) & 0x00ff;
	b = (i << 8) & 0xff00;
	return( a | b );
}


