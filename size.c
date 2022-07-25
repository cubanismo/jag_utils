
/*
	SIZE.C

	Written by Mike Fulton
	Last Modified: 12:05pm, December 12 1994

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

#define MAJOR_VERSION (2)
#define MINOR_VERSION (24)

#define SORT_BY_NAME	(0)
#define SORT_BY_VALUE	(1)
#define SORT_NONE	(2)

/**************************************************************************/
/**************************************************************************/
/**************************************************************************/

ABS_HDR theHeader, tmpHeader;
COF_HDR coff_header;
RUN_HDR run_header;
SEC_HDR txt_header, dta_header, bss_header;
BSD_Object bsd_object;
BSD_Symbol *coff_symbols;

int show_symbols = 0;
int skip_duplicates = 1;
int sort_options = SORT_BY_VALUE;
int opt_skip_line_numbers = 0;

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
	
	if( sort_options == SORT_BY_NAME )
	{
		for( i = 0; i < 8; i++ )	/* Check byte by byte so it fails faster! */
		{
			if( *aa > *bb )
			  return(1);
			else if( *aa < *bb )
			  return(-1);

			aa++;
			bb++;
		}
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
	if( sort_options == SORT_BY_NAME )
	{
		offset = sym1->name_offset;
		str1 = &coff_symbol_name_strings[offset] - 4;
		offset = sym2->name_offset;
		str2 = &coff_symbol_name_strings[offset] - 4;
	
		i = strcmp(str1, str2);
	}
	else if( sort_options == SORT_BY_VALUE )
	{
		if( sym1->value > sym2->value )
		  return(1);
		else if( sym1->value < sym2->value )
		  return(-1);
		else
		  i = 0;
	}
	return(i);
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
char *ptr, original_fname[256];

/* Save a copy of the original filename, then strip off the filename extension. */

	strncpy( original_fname, fname, 256 );
	if((ptr = strchr(fname,'.')) != NULL)
	  *ptr=0;

	theHeader.magic = readshort( in_handle );
	
/* BSD Objects have a LONG magic number, so move to start, read it, */
/* then move back to where we started. (At 2 bytes into file). */

	Fseek( 0L, in_handle, 0 );
	bsd_object.magic = readlong(in_handle);
	Fseek( 2L, in_handle, 0 );

/* See if the magic number indicates a DRI/Alcyon-format executable or object module file */
	
	if( theHeader.magic == 0x601b || theHeader.magic == 0x601a )
	{			
		read_dri_header( in_handle );
		print_dri_info();
		if( show_symbols )
		  print_dri_symbols( in_handle );
		return(1);
	}

/* Not DRI-format, so test for BSD/COFF. */
	
	else if( theHeader.magic == 0x0150 ||
		(theHeader.magic == 0x0000 && bsd_object.magic == 0x00000107L) )
	{
		read_coff_header( in_handle );
		print_coff_info();
		if( show_symbols )
		  print_coff_symbols( in_handle );
		return(1);
	}

/* Sorry, don't know what it is. */

	else
	{
		printf("Error: Wrong file type.  Magic number = 0x%04x\n", theHeader.magic );
		Fclose(in_handle);
		exit(1);
	}
	return(0);
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

	if( theHeader.magic == 0x601b )		/* If it's an executable... */
	{
		printf("DRI/Alcyon format absolute location executable file detected \n");
		theHeader.res1 = readlong(in_handle);		/* not used... */
		theHeader.tbase = readlong(in_handle);
		theHeader.relocflag = readshort(in_handle);	/* not used... */
		theHeader.dbase = readlong(in_handle);
		theHeader.bbase = readlong(in_handle);
	}
	else					/* If it's an OBJECT MODULE... */
	{
		printf("DRI/Alcyon format relocatable object module file detected \n");
	}
}

/**************************************************************************/
/**************************************************************************/
/**************************************************************************/

void read_coff_header( int in_handle )
{
	if( theHeader.magic == 0x0150 )
	{
		printf("COFF format absolute executable program file detected.\n");
		Fseek( 0L, in_handle, 0 );

		coff_header.magic = readshort(in_handle);
		coff_header.num_sections = readshort(in_handle);
		coff_header.date = readlong(in_handle);
		coff_header.sym_offset = readlong(in_handle);
		coff_header.num_symbols = readlong(in_handle);
		coff_header.opt_hdr_size = readshort(in_handle);
		coff_header.flags = readshort(in_handle);

		run_header.magic = readlong(in_handle);
		run_header.tsize = readlong(in_handle);
		run_header.dsize = readlong(in_handle);
		run_header.bsize = readlong(in_handle);
		run_header.entry = readlong(in_handle);
		run_header.tbase = readlong(in_handle);
		run_header.dbase = readlong(in_handle);
	}
	else
	{		
		printf( "BSD format object module file detected.\n" );
		Fseek( 0L, in_handle, 0 );

		bsd_object.magic = readlong(in_handle);
		bsd_object.tsize = readlong(in_handle);
		bsd_object.dsize = readlong(in_handle);
		bsd_object.bsize = readlong(in_handle);
		bsd_object.ssize = readlong(in_handle);
		bsd_object.entry = readlong(in_handle);
		bsd_object.trsize = readlong(in_handle);
		bsd_object.drsize = readlong(in_handle);

		coff_header.num_sections = 3;

		coff_header.sym_offset = PACKED_SIZEOF(BSD_Object);
		coff_header.sym_offset += bsd_object.tsize;
		coff_header.sym_offset += bsd_object.dsize;
		coff_header.sym_offset += bsd_object.trsize;
		coff_header.sym_offset += bsd_object.drsize;
		
		coff_header.num_symbols = bsd_object.ssize / PACKED_SIZEOF(BSD_Symbol);
		coff_header.opt_hdr_size = 0L;
		coff_header.flags = 0L;
	}
	
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

	if( theHeader.magic != 0x601a )		/* If not an OBJECT module */
	{
		printf( "Absolute Address for text segment = 0x%08" PRIx32 "\n", theHeader.tbase );
		printf( "Absolute Address for data segment = 0x%08" PRIx32 "\n", theHeader.dbase );
		printf( "Absolute Address for BSS segment = 0x%08" PRIx32 "\n\n", theHeader.bbase );
	}
}

/**************************************************************************/
/**************************************************************************/
/**************************************************************************/

void print_coff_info(void)
{
	if( theHeader.magic == 0x0150 )
	{
//		printf( "%d sections specified\n", coff_header.num_sections);
//		printf( "The additional header size is %d bytes\n", coff_header.opt_hdr_size );
//		printf( "Magic Number for RUN_HDR = 0x%08" PRIx32 "\n", run_header.magic );

		printf( "Text Segment Size = 0x%08" PRIx32 "\n", run_header.tsize );
		printf( "Data Segment Size = 0x%08" PRIx32 "\n", run_header.dsize );
		printf( "BSS Segment Size = 0x%08" PRIx32 "\n", run_header.bsize );

//		printf( "Symbol Table offset = %ld (0x%08" PRIx32 ")\n", coff_header.sym_offset, coff_header.sym_offset );

		printf( "Symbol Table contains %" PRId32 " symbol entries\n", coff_header.num_symbols );

		printf( "Starting Address for executable = 0x%08" PRIx32 "\n", run_header.entry );
		printf( "Start of Text Segment = 0x%08" PRIx32 "\n", run_header.tbase );
		printf( "Start of Data Segment = 0x%08" PRIx32 "\n", run_header.dbase );
	
		printf( "Start of BSS Segment = 0x%08" PRIx32 "\n\n", bss_header.start_address );
	}
	else
	{
		printf( "Text Segment Size = 0x%08" PRIx32 "\n", bsd_object.tsize );
		printf( "Data Segment Size = 0x%08" PRIx32 "\n", bsd_object.dsize );
		printf( "BSS Segment Size = 0x%08" PRIx32 "\n", bsd_object.bsize );
	}
}

/**************************************************************************/
/**************************************************************************/
/**************************************************************************/

void print_dri_symbols( int fhand )
{
char HUGE *ptr;
uint8_t *uptr;
int32_t longcount, skipped, offset;
void HUGE *symbuf, HUGE *a, HUGE *b;

	printf( "\nDump of symbols in this file:\n\n" );

/* Read the symbols, sort them, print them. */
/* This sort of assumes your symbol table will fit in available */
/* memory (MSDOS memory... less than 600K), but this shouldn't be */
/* a big problem. */

	if( theHeader.magic == 0x601b )					/* ABS executable */
	  offset = PACKED_SIZEOF(ABS_HDR) + theHeader.tsize + theHeader.dsize;
	else								/* Object Module */
	  offset = PACKED_SIZEOF(DRI_Object) + theHeader.tsize + theHeader.dsize;

	Fseek( offset, fhand, 0 );
	symbuf = farmalloc(theHeader.ssize);
	if( ! symbuf)
	{
		printf( "Cannot allocate sufficient memory (%" PRId32 " bytes) for buffer!\n", theHeader.ssize );
		exit(-1);
	}

/* Read the symbols one at a time (because doing it in one chunk isn't working right...) */

	printf( "Reading symbols from offset %" PRId32 " (0x%08" PRIx32 ")...\n", offset, offset );
	ptr = (char FAR *)symbuf;
	for( longcount = 0; longcount < theHeader.ssize; longcount += 14 )
	{
		Fread( fhand, 14L, ptr );
		ptr += 14;
	}

	if( sort_options != SORT_NONE )
	  my_qsort( symbuf, (int)(theHeader.ssize/14), 14, dri_symbol_compare );

	ptr = symbuf;
	skipped = 0;
	for (longcount = 0 ; longcount < theHeader.ssize ; longcount += 14)
	{
	int show_it;
	
		a = ptr;
		b = (char HUGE *)a + 14;
		uptr = (uint8_t *)ptr;

		show_it = 1;
		if( skip_duplicates && ( (longcount+14) < theHeader.ssize ) )
		{
			show_it = dri_symbol_compare(a,b);
			if( ! show_it )
			  skipped++;
		}

		if( show_it )
		{
		short i;

			for( i = 0; i < 8; i++ )
			{
				if( ptr[i] == 0 )
				{
					for( ; i < 8; i++ )
					  ptr[i] = ' ';
				}
			}

/* print the values byte by byte because it works on any machine... */
			
			printf( "0x%02x%02x%02x%02x",
				(unsigned int)uptr[10], (unsigned int)uptr[11],
				(unsigned int)uptr[12], (unsigned int)uptr[13] );

//printf( "0x%08" PRIx32 ": ", ptr );

			printf( "\t%c%c%c%c%c%c%c%c\t",
				ptr[0], ptr[1], ptr[2], ptr[3],
				ptr[4], ptr[5], ptr[6], ptr[7] );

//printf( "\t%02x%02x%02x%02x%02x%02x%02x%02x\n",
//	ptr[0], ptr[1], ptr[2], ptr[3],
//	ptr[4], ptr[5], ptr[6], ptr[7] );
				
			show_dri_symbol_type( ((unsigned int)uptr[8] * 256) + uptr[9] );
		}
		ptr += 14;
	}

	printf( "\n" );
	
	if( skipped )
	  printf( "%" PRId32 " duplicate symbol names were skipped.\n", skipped );

	printf( "\n\n" );
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

void print_coff_symbols( int fhand )
{
int32_t sym, symsize, stringtable_size, offset;
int32_t skipped, unknown_type;

	printf( "\nDump of symbols in this file:\n\n" );

/* Calculate the size needed for the symbol table */

	symsize = coff_header.num_symbols * sizeof(BSD_Symbol);

/* Move to the symbol table, allocate memory for it */
	
	offset = coff_header.sym_offset;
	Fseek(offset,fhand,0);

	coff_symbols = (BSD_Symbol *)farmalloc( symsize );
	if( ! coff_symbols )
	{
		printf( "Cannot allocate memory for symbol information!\n" );
		exit(-1);
	}

/* Read the symbols in one by one */

	for( sym = 0; sym < coff_header.num_symbols; sym++ )
	{
		coff_symbols[sym].name_offset = readlong( fhand );
		coff_symbols[sym].type = readbyte( fhand );
		coff_symbols[sym].other = readbyte( fhand );
		coff_symbols[sym].description = readshort( fhand );
		coff_symbols[sym].value = readlong( fhand );
	}

/* Get the size of the string table (the name strings that go with */
/* the symbols) then allocate some memory and read it in. */
/* The string table starts with a longword containing the size, so the */
/* offsets used by the symbols must be adjusted by -4 when you access */
/* the individual strings. */

	stringtable_size = readlong( fhand );
	
	coff_symbol_name_strings = farmalloc( stringtable_size );
	if( ! coff_symbol_name_strings )
	{
		printf( "Cannot allocate memory for symbol information!\n" );
		exit(-1);
	}

	Fread( fhand, stringtable_size, coff_symbol_name_strings );

	if( sort_options != SORT_NONE )
	  my_qsort( coff_symbols, (int)coff_header.num_symbols, sizeof(BSD_Symbol), coff_symbol_compare );

	skipped = unknown_type = 0;
	for( sym = 0; sym < coff_header.num_symbols; sym++ )
	{
	int32_t offset, value;
	int type, other, description, show_it;
	
		offset = coff_symbols[sym].name_offset - 4;	/* Adjust offset by -4 as mentioned earlier... */
		value = coff_symbols[sym].value;
		type = (int)coff_symbols[sym].type;
		other = (int)coff_symbols[sym].other;
		description = (int)coff_symbols[sym].description;

		show_it = 1;	/* assume we're showing it until we learn otherwise... */

		if( skip_duplicates )
		{
			/* the compare function will return 0 if the symbol names are the same */
			
			show_it = coff_symbol_compare(&coff_symbols[sym], &coff_symbols[sym+1]);
			if( ! show_it )
			  skipped++;
		}

		if( show_it )
		  unknown_type += show_bsd_symbol_type( value, &coff_symbol_name_strings[offset], type, other, description );
	}	

	printf( "\n" );
	if( skipped )
	  printf( "%" PRId32 " duplicate symbol names were skipped.\n", skipped );
	if( unknown_type )
	  printf( "%" PRId32 " symbols were special source-level debugging flags.\n", unknown_type );
	printf( "\n\n" );
	
	farfree( coff_symbols );
	farfree( coff_symbol_name_strings );
}

/**************************************************************************/
/**************************************************************************/
/**************************************************************************/

int show_bsd_symbol_type( int32_t value, char *str, int symtype, int other, int description )
{
	char *symDesc;

	switch( symtype )
	{
		case 0x80:
			if (str && (symDesc = strchr(str, ':')) && *++symDesc) {
				if ((symDesc[0] >= '0' && symDesc[0] <= '9') ||
				    symDesc[0] == '(' || symDesc[0] == '-') {
					printf( "0x%08" PRIx32 "  %-35s  Local Variable\n", value, str);
				} else {
					/* Type definition */
					printf( "0x%08" PRIx32 "  %-35s  Type Definition\n", value, str);
				}
				return 1;
			}
			/* Malformed stabs data. Fall through */
		case 0xA0:
		case 0x40:
			printf( "0x%08" PRIx32 "  %-35s  Unknown Type: 0x%02x\n", value, " " /*str*/, symtype );
			return(1);
		case 0x3c:
			printf( "0x%08" PRIx32 "  %-35s  Debugger option\n", value, str);
			return 1;
		case 0x20:
			printf( "0x%08" PRIx32 "  %-35s  Global\n", value, str );
			break;
		case 0x24:
			printf( "0x%08" PRIx32 "  %-35s  Function\n", value, str );
			break;
		case 0xC0:
			printf( "0x%08" PRIx32 "  %-35s  Left bracket/open block\n", value, str );
			return 1;
		case 0xE0:
			printf( "0x%08" PRIx32 "  %-35s  Right bracket/close block\n", value, str );
			return 1;
		case 0xE2:
			printf( "0x%08" PRIx32 "  %-35s  Begin common block\n", value, str );
			return 1;
		case 0xE4:
			printf( "0x%08" PRIx32 "  %-35s  End common block\n", value, str );
			return 1;
		case 0x64:
			printf( "0x%08" PRIx32 "  %-35s  Primary Source Code File\n", value, str );
			return 1;
		case 0x84:
			printf( "0x%08" PRIx32 "  %-35s  Included Source Code File\n", value, str );
			return 1;
/********/
		case 0x44:
			if( ! opt_skip_line_numbers )
			  printf( "0x%08" PRIx32 "  %-35s  Text Line Number: %d\n", value, str, description );
			return 1;
		case 0x48:
			if( ! opt_skip_line_numbers )
			  printf( "0x%08" PRIx32 "  %-35s  BSS Line Number\n", value, " " );
			return 1;
		case 0x4C:
			if( ! opt_skip_line_numbers )
			  printf( "0x%08" PRIx32 "  %-35s  GPU/DSP Line Number: %d\n", value, str, description );
			return 1;
/********/
		case 0x09:
			printf( "0x%08" PRIx32 " %-35s  Global BSS\n", value, str );
			break;
		case 0x08:
			printf( "0x%08" PRIx32 " %-35s  BSS\n", value, str );
			break;
/********/
		case 0x07:
			printf( "0x%08" PRIx32 "  %-35s  Global Data\n", value, str );
			break;
		case 0x06:
			printf( "0x%08" PRIx32 "  %-35s  Data\n", value, str );
			break;
/********/
		case 0x05:
			printf( "0x%08" PRIx32 "  %-35s  Global Text\n", value, str );
			break;
		case 0x04:
			printf( "0x%08" PRIx32 "  %-35s  Text\n", value, str );
			break;
/********/
		case 0x03:
			printf( "0x%08" PRIx32 "  %-35s  Global Equate or GPU/DSP Text\n", value, str );
			break;
		case 0x02:
			printf( "0x%08" PRIx32 "  %-35s  Equate or GPU/DSP Text\n", value, str );
			break;

		case 0x01:
			printf( "0x%08" PRIx32 "  %-35s  Global (Undefined Segment)\n", value, str );
			break;
/********/
		default:
			printf( "0x%08" PRIx32 "  %-35s  Unknown Type: 0x%02x\n", value, str, symtype );
			return(1);
	}
	return(0);
}

/**************************************************************************/
/**************************************************************************/
/**************************************************************************/

void usage(void)
{
	printf( "Usage:\n\tSIZE [-sd] [-v[0|1|2]] [-l] <filename>\n\n");
	printf( "\t-s  = Show list of all symbols in file\n\n" );
	printf( "\t-sd = Don't skip duplicate symbol names in listing\n\n" );

	printf( "\t-v0 = Don't sort symbols at all\n" );
	printf( "\t-v1 = Sort symbols by name (default)\n" );
	printf( "\t-v2 = Sort symbols by value\n\n" );

	printf( "\t-l  = Skip special BSD debugging info line number symbols\n\n" );
	
	printf( "\t<filename> = a DRI or BSD/COFF format absolute-position\n" );
	printf( "\texecutable file or DRI/Alcyon format object file.\n" );
	printf( "\tA filename extension of .ABS or .COF is assumed if none is\n" );
	printf( "\tspecified.  (i.e. SIZE testprog will look for <testprog> then\n" );
	printf( "\t<testprog.cof> then <testprog.abs> before giving up.\n\n" );

	printf( "For Example:\n\n" );
	printf( "\tSIZE program     << finds 'program', 'program.cof', or 'program.abs'\n" );
	printf( "\tSIZE program.abs\n\n" );
	}

/**************************************************************************/
/**************************************************************************/
/**************************************************************************/

void main( int argc, char *argv[] )
{
int in_handle;
short has_period;
char infile[256], *ptr, *filename = NULL;
int argument;

	printf( "SIZE: Version %d.%02d\n\n", MAJOR_VERSION, MINOR_VERSION );

	if(argc < 2)
	{
		usage();
		exit(-1);
	}

/* Set default options */
	
	sort_options = SORT_BY_NAME;	/* Sort symbols by name */

/* Parse command line arguments */

	for( argument = 1; argument < argc; argument++ )
	{
//		printf( "Processing argument %d: '%s'\n", argument, argv[argument] );

		if( argv[argument][0] != '-' && argv[argument][0] )
		{
			filename = argv[argument];
		}
		else if( ! strcmp( "-s", argv[argument] ) )
		{
			show_symbols = 1;		/* Show symbols */
			skip_duplicates = 1;		/* and skip duplicate names */
		}
		else if( ! strcmp( "-sd", argv[argument] ) )
		{
			show_symbols = 1;		/* Show symbols */
			skip_duplicates = 0;		/* but don't skip duplicate names */
		}
		else if( ! strcmp( "-d", argv[argument] ) )	/* Same as -sd */
		{
			show_symbols = 1;		/* Show symbols */
			skip_duplicates = 0;		/* but don't skip duplicate names */
		}
		else if( ! strcmp( "-l", argv[argument] ) )	/* Same as -sd */
		{
			opt_skip_line_numbers = 1;
		}
		else if( ! strcmp( "-v0", argv[argument] ) )
		{
			sort_options = SORT_NONE;
			skip_duplicates = 0;		/* but don't skip duplicate names */
		}
		else if( ! strcmp( "-v1", argv[argument] ) )
		{
			sort_options = SORT_BY_NAME;
		}
		else if( ! strcmp( "-v2", argv[argument] ) )
		{
			sort_options = SORT_BY_VALUE;
			skip_duplicates = 0;		/* but don't skip duplicate names */
		}
		else if( ! strcmp( "-v", argv[argument] ) )
		{
			sort_options = SORT_BY_VALUE;	/* Sort symbols by value */
		}
		else if( strncmp( "-", argv[argument], 1 ) ) /* unrecognized switch */
		{
			usage();
			exit(-1);
		}
#if 0
		else if( (argument+1) < argc )
		{
			usage();
			exit(-1);
		}
#endif
	}

	if (!filename)
	{
		usage();
		exit(-1);
	}

/* If you don't sort by name (sort by value), then don't skip duplicate symbols. */

	if( sort_options != SORT_BY_NAME )
	  skip_duplicates = 0;
	
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

