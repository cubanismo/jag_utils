
/*****************************************************************************
 *	convert.c 		
 ****************************************************************************/

long cvt_long_m2i ( long l );
int cvt_int_m2i ( int i );

/*****************************************************************************
 *	size.c 		
 ****************************************************************************/

void checkpoint ( char *funcname , short ckpoint );
int dri_symbol_compare ( const void *a , const void *b );
int coff_symbol_compare ( const void *a , const void *b );
void read_sec_hdr ( short in_handle , SEC_HDR *section );
short process_abs_file ( char *fname , short in_handle );
void read_dri_header ( short in_handle );
void read_coff_header ( short in_handle );
void print_dri_info ( void );
void print_coff_info ( void );
void print_dri_symbols ( short fhand );
void print_coff_symbols( short fhand );
void show_dri_symbol_type( unsigned int symtype );
int show_bsd_symbol_type( long value, char *str, int symtype, int other, int description );
void usage(void);
void main ( short argc , char *argv []);

/*****************************************************************************
 *	readint.c
 ****************************************************************************/

unsigned char readbyte( int fhand );
unsigned short readshort ( int fhand );
unsigned long readlong ( int fhand );
long writelong( int fhand, long lval );
long writeshort( int fhand, short sval );

