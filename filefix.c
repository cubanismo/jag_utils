
/*
	filefix.c

	Written by Mike Fulton
	Version 7 recreated by James Jones based on SIZE.C by Mike Fulton
	Last Modified: 12:03am, April 2 2022

	This program should take a file name with or without extension
	read in the .ABS or .COF and produce .SYM, .TXT, and .DTA files,
	and a .DB script.
*/

/**************************************************************************/
/**************************************************************************/
/**************************************************************************/

#include "size.h"
#include "proto.h"

#include <inttypes.h>

/**************************************************************************/
/**************************************************************************/
/**************************************************************************/

#define DEBUG	(0)

#define MAJOR_VERSION (7)
#define MINOR_VERSION (2)

#define SEC_TEXT	(0)
#define SEC_DATA	(1)

#define ROM_BASE	(0x800000)
#define ROM_HDR_SIZE	(0x2000)
#define ROM_START	(ROM_BASE + ROM_HDR_SIZE)
#define ROM_END		(0xE00000)

/**************************************************************************/
/**************************************************************************/
/**************************************************************************/

ABS_HDR theHeader, tmpHeader;
COF_HDR coff_header;
RUN_HDR run_header;
SEC_HDR txt_header, dta_header, bss_header;
BSD_Object bsd_object;
BSD_Symbol *coff_symbols;

static short quiet = 0;
static short use_fread = 0;
static short rom_db_script = 0;
static short no_header = 0;
static size_t align_size = 0;
static uint8_t pad_byte = 0xff;
static const char *romfile = NULL;

char *coff_symbol_name_strings;

static uint32_t dri_symbol_value( const void *s )
{
	const uint8_t *uptr = s;
	uptr += 10;

	return ((uint32_t)uptr[0] << 24) |
		((uint32_t)uptr[1] << 16) |
		((uint32_t)uptr[2] << 8) |
		(uint32_t)uptr[3];
}

/**************************************************************************/
/**************************************************************************/
/**************************************************************************/
/* Returns -1 if a<b, 0 if a=b, or 1 if a>b */

int dri_symbol_compare( const void *a, const void *b )
{
char HUGE *aa, HUGE *bb;
int i;
uint32_t sym_a_val, sym_b_val;

	aa = (char HUGE *)a;
	bb = (char HUGE *)b;
	
	for( i = 0; i < 8; i++ )	/* Check byte by byte so it fails faster! */
	{
		if( *aa > *bb )
			return(1);
		else if( *aa < *bb )
			return(-1);

		aa++;
		bb++;
	}

/* If sorting by name, but they are the same, then check value. */
/* Or if we are sorting by value instead... */

	sym_a_val = dri_symbol_value(a);
	sym_b_val = dri_symbol_value(b);
	
	if( sym_a_val > sym_b_val )
	  return(1);
	else if( sym_a_val < sym_b_val )
	  return(-1);
	else
	  return(0);
}

/**************************************************************************/
/**************************************************************************/
/**************************************************************************/

/* Returns -1 if a<b, 0 if a=b, or 1 if a>b */

int coff_symbol_compare( const void *a, const void *b )
{
BSD_Symbol HUGE *sym1, HUGE *sym2;
int32_t offset;
char HUGE *str1, HUGE *str2;
int i;

	sym1 = (BSD_Symbol *)a;
	sym2 = (BSD_Symbol *)b;

	i = 0;
	offset = sym1->name_offset;
	str1 = &coff_symbol_name_strings[offset] - 4;
	offset = sym2->name_offset;
	str2 = &coff_symbol_name_strings[offset] - 4;

	i = strcmp(str1, str2);

	if (i) return i;

	if( sym1->value > sym2->value ) return 1;
	else if( sym1->value < sym2->value ) return -1;
	else return 0;
}

/**************************************************************************/
/**************************************************************************/
/**************************************************************************/

/* Read in a BSD/COFF SEC_HDR structure from the file. */
/* For Jaguar we don't really care about all of the fields, */
/* but we still have to read them all! */

void read_sec_hdr( int in_handle, SEC_HDR *section )
{
	Fread(in_handle, 8L, &section->name);

	section->start_address = readlong(in_handle);
	section->start_address_2 = readlong(in_handle);

	section->size = readlong(in_handle);

	section->offset = readlong(in_handle);
	section->relocation_data = readlong(in_handle);
	section->debug_info = readlong(in_handle);
	section->num_reloc_entries = readshort(in_handle);
	section->num_debug_entries = readshort(in_handle);
	section->flags = readlong(in_handle);
}

/**************************************************************************/
/**************************************************************************/
/**************************************************************************/

/* Read the header, determine if the file is BSD/COFF or DRI format, then */
/* do the appropriate steps to write out the TEXT, DATA, and maybe the */
/* SYMBOLS file */

short process_abs_file( char *fname, int in_handle )
{
char *ptr, original_fname[260];

/* Save a copy of the original filename, then strip off the filename extension. */

	strncpy( original_fname, fname, 255 );
	if((ptr = strchr(fname,'.')) != NULL)
	  *ptr=0;

	theHeader.magic = readshort( in_handle );
	
/* BSD Objects have a LONG magic number, so move to start, read it, */
/* then move back to where we started. (At 2 bytes into file). */

	Fseek( 0L, in_handle, 0 );
	bsd_object.magic = readlong(in_handle);
	Fseek( 2L, in_handle, 0 );

/* See if the magic number indicates a DRI/Alcyon-format executable or object module file */
	
	if( theHeader.magic == 0x601b )
	{			
		read_dri_header( in_handle );
		if ( !quiet )
		  print_dri_info();
	}

/* Not DRI-format, so test for BSD/COFF. */
	
	else if( theHeader.magic == 0x0150 )
	{
		read_coff_header( in_handle );
		if ( !quiet )
		  print_coff_info();
	}

/* Sorry, don't know what it is. */

	else
	{
		printf("Error: Wrong file type.  Magic number = 0x%04x\n", theHeader.magic );
		Fclose(in_handle);
		exit(1);
	}

	if ( romfile )
	{
		if ( ( theHeader.tbase < ROM_START ) ||
		     ( theHeader.tbase >= ROM_END ) )
		{
			printf("This program does not execute from ROM space.\n"
			       "The ROM image file created may not be usable, but we\n"
			       "will create it anyway!\n\n");
		}
		else if ( theHeader.tbase != ROM_START )
		{
			/* ... we will will create it ... [sic] */
			printf("This program does not start at the proper address of 0x%x\n"
			       "The ROM image file created may not be executable, but we will\n"
			       "will create it anyway!\n\n", ROM_START);
		}

		write_rom_file( in_handle );
		if ( rom_db_script )
		{
			write_rom_script();
		}
	}
	else
	{
/* Write out the data section file */

		if ( theHeader.dsize > 0 )
		{
			write_sec_file( fname, in_handle, SEC_DATA );
		}
		else if ( !quiet )
		{
			printf("Data Segment empty, no DTA file written.\n");
		}

/* Write out the text section file */

		if ( theHeader.tsize > 0 )
		{
			write_sec_file( fname, in_handle, SEC_TEXT );
		}
		else if ( !quiet )
		{
			printf("Text Segment empty, no TX file written.\n");
		}

		write_db_file( fname, in_handle );

		if( theHeader.magic == 0x601b )
		{
			write_sym_file( fname, in_handle );
		}
	}

	return(1);
}

#define CHUNK_SIZE 256

void write_sec_file( const char *base_fname, int in_handle, short sec_type )
{
char outfile[260];
size_t bytes_left;
off_t offset;
int out_handle;
const char is_cof = (theHeader.magic == 0x0150);

	strncpy(outfile, base_fname, 255);
	outfile[255] = '\0';

	switch ( sec_type )
	{
	case SEC_TEXT:
		strcat(outfile, ".tx");
		if ( is_cof )
		{
			offset = txt_header.offset;
		}
		else
		{
			offset = PACKED_SIZEOF(ABS_HDR);
		}
		bytes_left = theHeader.tsize;
		break;

	case SEC_DATA:
		strcat(outfile, ".dta");
		if ( is_cof )
		{
			offset = dta_header.offset;
		}
		else
		{
			offset = PACKED_SIZEOF(ABS_HDR) + theHeader.tsize;
		}
		bytes_left = theHeader.dsize;
		break;

	default:
		printf("Unknown section type\n");
		exit(-1);
	}

	out_handle = Fopen( outfile, FO_WRONLY | FO_CREATE | FO_BINARY );

	if ( out_handle < 0 )
	{
		printf( "Can't create %s\n", outfile );
		exit(-1);
	}

	write_sec( out_handle, in_handle, offset, bytes_left );
	Fclose( out_handle );
}

size_t write_sec( int out_handle, int in_handle, off_t offset, size_t bytes_left )
{
uint8_t buf[CHUNK_SIZE];
size_t count;
size_t bytes_written = 0;

	if ( Fseek( offset, in_handle, 0 ) == -1 )
	{
		printf( "Could not seek to section in file\n" );
		exit(-1);
	}

	while ( bytes_left > 0 )
	{
		count = (bytes_left > CHUNK_SIZE) ? CHUNK_SIZE : bytes_left;

		if ( Fread( in_handle, count, buf ) != count )
		{
			printf( "Can't read section from file\n" );
			exit(-1);
		}

		if ( Fwrite( out_handle, count, buf ) != count )
		{

			printf( "Can't write section to file\n" );
			exit(-1);
		}

		bytes_left -= count;
		bytes_written += count;
	}

	return bytes_written;
}

static size_t pad( int out_handle, size_t cur_offset, size_t target_offset )
{
uint8_t buf[CHUNK_SIZE];
size_t bytes_written = 0;

	memset( buf, pad_byte, CHUNK_SIZE );

	while ( ( cur_offset + CHUNK_SIZE ) <= target_offset )
	{
		Fwrite( out_handle, CHUNK_SIZE, buf );
		cur_offset += CHUNK_SIZE;
		bytes_written += CHUNK_SIZE;
	}

	while ( cur_offset < target_offset )
	{
		Fwrite( out_handle, 1, &pad_byte );
		cur_offset++;
		bytes_written++;
	}

	return bytes_written;
}

static void pad_up( int out_handle, size_t cur_offset)
{
size_t target_size;
const size_t hdr_bytes = no_header ? 0 : ROM_HDR_SIZE;

	if ( !align_size )
		return;
	
	if ( !quiet )
	  printf("Wrote %zu bytes to file so far...\n", cur_offset);

	cur_offset += hdr_bytes;

	// This doesn't really do the right thing when tbase is not
	// equal to ROM_START, but it matches what v6.81 does.
	target_size = (cur_offset + (align_size - 1)) & ~(align_size - 1);

	if ( !quiet )
	  printf("Padding end of ROM image file with %zu %s bytes\n",
		 target_size - cur_offset, pad_byte ? "$FF" : "ZERO" );

	pad( out_handle, cur_offset, target_size );
}

void write_rom_script( void )
{
char strbuf[256];
size_t fnamelen;
int out_handle;
int i;

	strncpy(strbuf, romfile, 255);
	strbuf[255] = '\0';
	fnamelen = strlen(strbuf);

	for ( i = fnamelen - 1; i >= 0; i-- )
	{
		if ( strbuf[i] == '.' )
		{
			strbuf[i] = '\0';
			break;
		}
	}

	if ( i < 0 )
	{
		// No extension. Just append the new extension
		i = fnamelen;
	}

	if ( i + 3 < 256 )
	{
		strbuf[i++] = '.';
		strbuf[i++] = 'd';
		strbuf[i++] = 'b';
		strbuf[i] = '\0';
	}

	out_handle = Fopen( strbuf, FO_WRONLY | FO_CREATE );

	if ( out_handle < 0 )
	{
		printf( "Can't create %s\n", strbuf );
		exit(-1);
	}

	sprintf( strbuf, "%s %s %x\n", (use_fread != 0) ? "fread" : "read",
		 romfile, theHeader.tbase );
	Fwrite( out_handle, strlen(strbuf), strbuf );
	sprintf( strbuf, "xpc %x\n", theHeader.tbase );
	Fwrite( out_handle, strlen(strbuf), strbuf );
	sprintf( strbuf, "g\n" );
	Fwrite( out_handle, strlen(strbuf), strbuf );

	Fclose( out_handle );
}

void write_rom_file( int in_handle )
{
const char is_cof = (theHeader.magic == 0x0150);
size_t cur_offset = 0;
size_t sec_offset = is_cof ? txt_header.offset : PACKED_SIZEOF(ABS_HDR);
int out_handle;

	if ( !quiet )
	  printf( "Creating ROM image file: %s\n", romfile );

	out_handle = Fopen( romfile, FO_WRONLY | FO_CREATE | FO_BINARY );

	if ( out_handle < 0 )
	{
		printf( "Can't create %s\n", romfile );
		exit(-1);
	}

	/*
	 * When tbase > ROM_START, it would probably be better to pad here and
	 * make corresponding adjustments to pad_up() and the logic to pad to
	 * dbase below as well. However, this code matches what v6.81 does.
	 */

	cur_offset += write_sec( out_handle, in_handle,
				 sec_offset, theHeader.tsize );

	if ( theHeader.dsize > 0 )
	{
		if ( is_cof )
		{
			sec_offset = dta_header.offset;
		}
		else
		{
			sec_offset = PACKED_SIZEOF(ABS_HDR) + theHeader.tsize;
		}

		cur_offset += pad( out_handle, cur_offset,
				   theHeader.dbase - theHeader.tbase );

		cur_offset += write_sec( out_handle, in_handle,
					 sec_offset, theHeader.dsize );
	}

	pad_up( out_handle, cur_offset );

	Fclose( out_handle );
}

void write_db_file( const char *base_fname, int in_handle)
{
char outfile[260];
char strbuf[277];
int out_handle;
const char is_cof = (theHeader.magic == 0x0150);

	strncpy(outfile, base_fname, 255);
	outfile[255] = '\0';

	strcat(outfile, ".db");

	out_handle = Fopen( outfile, FO_WRONLY | FO_CREATE );

	if ( out_handle < 0 )
	{
		printf( "Can't create %s\n", outfile );
		exit(-1);
	}

	sprintf( strbuf, "#Created with FILEFIX v%d.%d\n", MAJOR_VERSION, MINOR_VERSION );
	Fwrite( out_handle, strlen(strbuf), strbuf );

	sprintf( strbuf, "gag on\n" );
	Fwrite( out_handle, strlen(strbuf), strbuf );

	if ( theHeader.tsize > 0 )
	{
		sprintf( strbuf, "%s %s.tx %" PRIx32 "\n",
			 (use_fread != 0) ? "fread" : "read",
			 base_fname, theHeader.tbase );
		Fwrite( out_handle, strlen(strbuf), strbuf );
	}

	if ( theHeader.dsize > 0 )
	{
		sprintf( strbuf, "%s %s.dta %" PRIx32 "\n",
			 (use_fread != 0) ? "fread" : "read",
			 base_fname, theHeader.dbase );
		Fwrite( out_handle, strlen(strbuf), strbuf );
	}

	/* If it's a DRI/Alcyon file... */
	sprintf( strbuf, "getsym %s.%s\n", base_fname,
		 is_cof ? "cof" : "sym" );
	Fwrite( out_handle, strlen(strbuf), strbuf );

	sprintf( strbuf, "echo \"Symbols loaded\"\n" );
	Fwrite( out_handle, strlen(strbuf), strbuf );

	sprintf( strbuf, "gag off\n" );
	Fwrite( out_handle, strlen(strbuf), strbuf );

	sprintf( strbuf, "echo \"\tstart\tsize\tend\"\n" );
	Fwrite( out_handle, strlen(strbuf), strbuf );

	sprintf( strbuf, "echo \"text\t%" PRIx32 "\t%" PRIx32 "\t%" PRIx32 "\"\n",
		 theHeader.tbase, theHeader.tsize,
		 (theHeader.tsize > 0) ?
		 theHeader.tbase + theHeader.tsize - 1 :
		 theHeader.tbase );
	Fwrite( out_handle, strlen(strbuf), strbuf );

	sprintf( strbuf, "echo \"data\t%" PRIx32 "\t%" PRIx32 "\t%" PRIx32 "\"\n",
		 theHeader.dbase, theHeader.dsize,
		 (theHeader.dsize > 0) ?
		 theHeader.dbase + theHeader.dsize - 1 :
		 theHeader.dbase );
	Fwrite( out_handle, strlen(strbuf), strbuf );

	sprintf( strbuf, "echo \"bss\t%" PRIx32 "\t%" PRIx32 "\t%" PRIx32 "\"\n",
		 theHeader.bbase, theHeader.bsize,
		 theHeader.bbase + theHeader.bsize - 1 );
	Fwrite( out_handle, strlen(strbuf), strbuf );

	sprintf( strbuf, "xpc %" PRIx32 "\n",
		 is_cof ? run_header.entry : theHeader.tbase);
	Fwrite( out_handle, strlen(strbuf), strbuf );

	Fclose( out_handle );
}

/**************************************************************************/
/**************************************************************************/
/**************************************************************************/

void read_dri_header( int in_handle )
{
	theHeader.tsize = readlong(in_handle);
	theHeader.dsize = readlong(in_handle);
	theHeader.bsize = readlong(in_handle);
	theHeader.ssize = readlong(in_handle);

	if ( !quiet )
	  printf("DRI-format file detected...\n");
	theHeader.res1 = readlong(in_handle);		/* not used... */
	theHeader.tbase = readlong(in_handle);
	theHeader.relocflag = readshort(in_handle);	/* not used... */
	theHeader.dbase = readlong(in_handle);
	theHeader.bbase = readlong(in_handle);
}

void write_dri_header( int out_handle, const ABS_HDR *header )
{
	writeshort(out_handle, header->magic);
	writelong(out_handle, header->tsize);
	writelong(out_handle, header->dsize);
	writelong(out_handle, header->bsize);
	writelong(out_handle, header->ssize);
	writelong(out_handle, header->res1);
	writelong(out_handle, header->tbase);
	writeshort(out_handle, header->relocflag);
	writelong(out_handle, header->dbase);
	writelong(out_handle, header->bbase);
}

/**************************************************************************/
/**************************************************************************/
/**************************************************************************/

void read_coff_header( int in_handle )
{
	if ( !quiet )
	  printf("BSD/COFF format file detected...\n");
	Fseek( 0L, in_handle, 0 );

#define READ_PRINT_SHORT(field) do { \
	coff_header.field = readshort(in_handle); \
	printf("coff_header." #field " = 0x%04" PRIx16 "\n", \
	       coff_header.field); \
} while (0)

#define READ_PRINT_LONG(field) do { \
	coff_header.field = readlong(in_handle); \
	printf("coff_header." #field " = 0x%08" PRIx32 "\n", \
	       coff_header.field); \
} while (0)

	READ_PRINT_SHORT(magic);
	READ_PRINT_SHORT(num_sections);
	READ_PRINT_LONG(date);
	READ_PRINT_LONG(sym_offset);
	READ_PRINT_LONG(num_symbols);
	READ_PRINT_SHORT(opt_hdr_size);
	READ_PRINT_SHORT(flags);

	run_header.magic = readlong(in_handle);
	run_header.tsize = readlong(in_handle);
	run_header.dsize = readlong(in_handle);
	run_header.bsize = readlong(in_handle);
	run_header.entry = readlong(in_handle);
	run_header.tbase = readlong(in_handle);
	run_header.dbase = readlong(in_handle);
	
	read_sec_hdr(in_handle, &txt_header);
	read_sec_hdr(in_handle, &dta_header);
	read_sec_hdr(in_handle, &bss_header);

/* OK, now we've gotta set up the DRI-format ABS header so */
/* that we can use it to write the RDBJAG script file later. */

	theHeader.tsize = txt_header.size;
	theHeader.tbase = txt_header.start_address;

	theHeader.dsize = dta_header.size;
	theHeader.dbase = dta_header.start_address;

	theHeader.bsize = bss_header.size;
	theHeader.bbase = bss_header.start_address;

	theHeader.ssize = 0;		/* We don't do anything with symbols yet... */
	theHeader.res1 = 0;		/* not used... */
	theHeader.relocflag = 0;	/* not used... */
}

/**************************************************************************/
/**************************************************************************/
/**************************************************************************/

void print_dri_info(void)
{
	printf( "Text segment size = 0x%08" PRIx32 " bytes\n", theHeader.tsize );
	printf( "Data segment size = 0x%08" PRIx32 " bytes\n", theHeader.dsize );
	printf( "BSS Segment size = 0x%08" PRIx32 " bytes\n", theHeader.bsize );

	printf( "Symbol Table size = 0x%08" PRIx32 " bytes\n", theHeader.ssize );
	printf( "Absolute Address for text segment = 0x%08" PRIx32 "\n", theHeader.tbase );
	printf( "Absolute Address for data segment = 0x%08" PRIx32 "\n", theHeader.dbase );
	printf( "Absolute Address for BSS segment = 0x%08" PRIx32 "\n", theHeader.bbase );
}

/**************************************************************************/
/**************************************************************************/
/**************************************************************************/

void print_coff_info(void)
{
	printf( "%" PRId16 " sections specified\n", coff_header.num_sections);
	printf( "Symbol Table offset = %" PRId32 "\n", coff_header.sym_offset );

	printf( "Symbol Table contains %" PRId32 " symbol entries\n", coff_header.num_symbols );
	printf( "The additional header size is %" PRId16 " bytes\n", coff_header.opt_hdr_size );
	printf( "Magic Number for RUN_HDR = 0x%08" PRIx32 "\n", run_header.magic );

	printf( "Text Segment Size = %" PRId32 "\n", run_header.tsize );
	printf( "Data Segment Size = %" PRId32 "\n", run_header.dsize );
	printf( "BSS Segment Size = %" PRId32 "\n", run_header.bsize );

	printf( "Starting Address for executable = 0x%08" PRIx32 "\n", run_header.entry );
	printf( "Start of Text Segment = 0x%08" PRIx32 "\n", run_header.tbase );
	printf( "Start of Data Segment = 0x%08" PRIx32 "\n", run_header.dbase );
}

/**************************************************************************/
/**************************************************************************/
/**************************************************************************/

void write_sym_file( const char *base_fname, int fhand )
{
char HUGE *ptr;
char HUGE *dptr;
uint8_t *uptr;
int32_t longcount, skipped, offset;
void HUGE *symbuf, HUGE *a, HUGE *b;
int out_handle;
char outfile[259];

/* Read the symbols, sort them, deduplicate them, and then dump them. */
/* This sort of assumes your symbol table will fit in available */
/* memory, but this shouldn't be a big problem. */

	offset = PACKED_SIZEOF(ABS_HDR) + theHeader.tsize + theHeader.dsize;

	Fseek( offset, fhand, 0 );
	symbuf = farmalloc(theHeader.ssize);
	if( ! symbuf)
	{
		printf( "Cannot allocate sufficient memory (%" PRId32 " bytes) for buffer!\n", theHeader.ssize );
		exit(-1);
	}

/* Read the symbols one at a time (because doing it in one chunk isn't working right...) */

	printf( "Reading symbols...\n" );

	ptr = (char FAR *)symbuf;
	for( longcount = 0; longcount < theHeader.ssize; longcount += 14 )
	{
		Fread( fhand, 14L, ptr );
		ptr += 14;
	}

	printf( "Read %d symbols from file\n", longcount / 14 );

	printf( "Sorting and eliminating duplicate symbols...\n" );
	my_qsort( symbuf, (int)(theHeader.ssize/14), 14, dri_symbol_compare );

	dptr = ptr = symbuf;
	skipped = 0;
	for (longcount = 0 ; longcount < theHeader.ssize ; longcount += 14)
	{
	int show_it;
	
		a = ptr;
		b = (char HUGE *)a + 14;
		uptr = (uint8_t *)ptr;

		show_it = 1;
		if( (longcount+14) < theHeader.ssize )
		{
			show_it = dri_symbol_compare(a,b);
			if( ! show_it )
			  skipped++;
		}

		if( show_it )
		{
			if ( dptr != ptr )
			{
				memcpy( dptr, ptr, 14 );
			}
			dptr += 14;
		}
		ptr += 14;
	}

	memcpy(&tmpHeader, &theHeader, sizeof(tmpHeader));
	tmpHeader.tsize = 0;
	tmpHeader.dsize = 0;
	tmpHeader.ssize = longcount - (skipped * PACKED_SIZEOF(DRI_Symbol));

	strncpy(outfile, base_fname, 255);
	outfile[255] = '\0';
	strcat(outfile, ".sym");

	out_handle = Fopen( outfile, FO_WRONLY | FO_CREATE | FO_BINARY );

	if ( out_handle < 0 )
	{
		printf( "Can't create %s\n", outfile );
		exit(-1);
	}

	write_dri_header( out_handle, &tmpHeader );

	ptr = symbuf;
	for (longcount = 0 ; longcount < tmpHeader.ssize ; longcount += PACKED_SIZEOF(DRI_Symbol))
	{
		Fwrite( out_handle, PACKED_SIZEOF(DRI_Symbol), ptr );
		ptr += PACKED_SIZEOF(DRI_Symbol);
	}

	Fclose( out_handle );
	farfree( symbuf );
}

/**************************************************************************/
/**************************************************************************/
/**************************************************************************/

void show_dri_symbol_type( unsigned int symtype )
{
unsigned int mask, bit;

	for( mask = 0x8000; mask != 0; mask >>= 1 )
	{
		bit = mask & symtype;
		switch( bit )
		{
			case 0x8000:
				printf( "Defined " );
				break;
			case 0x4000:
				printf( "Equate " );
				break;
			case 0x2000:
				printf( "Global " );
				break;
			case 0x1000:
				printf( "Equated Register " );
				break;
			case 0x0800:
				printf( "External " );
				break;
			case 0x0400:
				printf( "Data " );
				break;
			case 0x0200:
				printf( "Text " );
				break;
			case 0x0100:
				printf( "BSS " );
				break;
		}
	}
	printf( " (0x%04x)\n", symtype );
}

/**************************************************************************/
/**************************************************************************/
/**************************************************************************/

void usage(void)
{
	printf("Filefix: Version %d.%d\n\n", MAJOR_VERSION, MINOR_VERSION );
	printf( "Usage:\n\tFILEFIX [options] <filename>\n\n" );
	printf( "<filename> = a DRI or BSD/COFF format absolute-position executable file.\n" );
	printf( "A filename extension of .COF or.ABS is assumed if none is provided\n" );

	printf( "(i.e. \"FILEFIX testprog\" will look for <testprog>, then <testprog.cof>,\n" );
	printf( "then <testprog.abs>, before giving up." );
	printf( "For Example:\n\n" );

	printf( "\tfilefix program\t<< finds 'program', 'program.cof', or 'program.abs'\n" );
	printf( "\tfilefix program.abs\n\n" );

	printf( "Option switches are:\n\n" );
	printf( "-q = Quiet mode, don't print information about executable file.\n\n" );
	printf( "-r <romfile> = Create ROM image file named <romfile> from executable\n\n" );
	printf( "-rs <romfile> = Same as -r, except also create DB script to load and run file.\n\n" );
	printf( "-p = Pad ROM file with $FF bytes to next 2mb boundary\n" );
	printf( "    (this must be used alongwith the -r or -rs switch)\n\n" );
	printf( "-p4 = Same as -p, except pads to a 4mb boundary\n" );
	printf( "    (this must be used along with the -r or -rs switch)\n\n" );
	printf( "-pn<x> = Same as -p, except pads to 2^<x> boundary, where 1 <= <x> <= 31\n" );
	printf( "    (this must be used along with the -r or -rs switch)\n\n" );
	printf( "-z = Pad unused portions with $00 bytes instead of $FF bytes\n" );
	printf( "    (this must be used along with the -p, -p4, or -pn switch)\n\n" );
	printf( "-f = Use 'fread' command in DB script, instead of 'read'\n\n" );
	printf( "-n = Assume no header: Do not subtract 8k from final size when padding.\n\n" );
	printf( "    (this must be used along with the -p, -p4, or -pn switch)\n\n" );
}

/**************************************************************************/
/**************************************************************************/
/**************************************************************************/

void main( int argc, char *argv[] )
{
int in_handle;
short has_period;
char infile[260], *ptr, *filename;
int argument;

	if(argc < 2)
	{
		usage();
		exit(-1);
	}

/* Parse command line arguments */

	for( argument = 1; argument < argc; argument++ )
	{
//		printf( "Processing argument %d: '%s'\n", argument, argv[argument] );

		if( argv[argument][0] != '-' && argv[argument][0] )
		{
			filename = argv[argument];
		}
		else if( ! strcmp( "-q", argv[argument] ) )
		{
			quiet = 1;
		}
		else if( ! strcmp( "-r", argv[argument] ) ||
			 ( ! strcmp( "-rs", argv[argument] ) &&
			     ( rom_db_script = 1 ) ) )
		{
			argument++;
			if (argument >= argc)
			{
				usage();
				exit(-1);
			}
			romfile = argv[argument];
		}
		else if( ! strcmp( "-p", argv[argument] ) )
		{
			/* Pad ROM to 2mb boundary */
			align_size = 2 * 1024 * 1024;
		}
		else if( ! strcmp( "-p4", argv[argument] ) )
		{
			/* Pad ROM to 4mb boundary */
			align_size = 4 * 1024 * 1024;
		}
		else if( ! strncmp( "-pn", argv[argument], 3 ) )
		{
			/* Pad ROM to arbitrary power of 2 boundary */
			align_size = atoi(argv[argument] + 3);
			if ( align_size < 1 || align_size > 31 )
			{
				printf("Invalid padding size\n\n");
				usage();
				exit(-1);
			}
			align_size = 1 << align_size;
		}
		else if( ! strcmp( "-z", argv[argument] ) )
		{
			pad_byte = 0x00; /* Pad with $00 instead of $ff */
		}
		else if( ! strcmp( "-f", argv[argument] ) )
		{
			use_fread = 1;
		}
		else if( ! strcmp( "-n", argv[argument] ) )
		{
			no_header = 1;
		}
		else if( strncmp( "-", argv[argument], 1 ) ) /* unrecognized switch */
		{
			usage();
			exit(-1);
		}
	}

/*	Look for FILENAME.EXT (exactly as given on commandline), if that's
	not found, and filename specified has no extension, then look
	for FILENAME.COF, and FILENAME.ABS, and do it in that order. */

	strncpy( infile, filename, 255 );
	has_period = (strchr(infile,'.') != NULL) ? 1 : 0;

	in_handle = Fopen( infile, FO_BINARY );
	if( in_handle < 0 )
	{
		/* If there's an extension specified in the input filename, */
		/* then show the FILE NOT FOUND error message and exit. */

		if( has_period )
		{
			printf( "Input file '%s' not found!\n", filename );
			exit(-1);
		}

		/* No filename extension was originally specified, so let's try .COF first. */

		strcat(infile,".cof");

		in_handle = Fopen( infile, FO_BINARY );
		if( in_handle < 0 )
		{
			/* file.COF not found, so try .ABS extension */

			if((ptr = strchr(infile,'.')) != NULL)
			  *ptr=0;
			strcat(infile,".abs");

			if((in_handle = Fopen( infile, FO_BINARY )) < 0)
			{
				printf("Error: Can't open inputfile: %s\n",filename);
				exit(-1);
			}
		}
	}
	process_abs_file(infile, in_handle);
	Fclose(in_handle);
	exit(0);
}

