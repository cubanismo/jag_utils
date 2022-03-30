
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
void read_sec_hdr ( int in_handle , SEC_HDR *section );
short process_abs_file ( char *fname , int in_handle );
void read_dri_header ( int in_handle );
void read_coff_header ( int in_handle );
void print_dri_info ( void );
void print_coff_info ( void );
void print_dri_symbols ( int fhand );
void print_coff_symbols( int fhand );
void write_sec_file( const char *base_fname, int in_handle, short section );
size_t write_sec( int out_handle, int in_handle, off_t offset, size_t bytes_left );
void write_db_file( const char *base_fname, int in_handle );
void write_sym_file( const char *base_fname, int fhand );
void write_rom_file( int in_handle );
void write_rom_script( void );
void show_dri_symbol_type( unsigned int symtype );
int show_bsd_symbol_type( int32_t value, char *str, int symtype, int other, int description );
void usage(void);
void main ( int argc , char *argv []);

/*****************************************************************************
 *	readint.c
 ****************************************************************************/

unsigned char readbyte( int fhand );
unsigned short readshort ( int fhand );
unsigned long readlong ( int fhand );
long writelong( int fhand, long lval );
long writeshort( int fhand, short sval );

