
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

#define MAJOR_VERSION (1)
#define MINOR_VERSION (2)

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

char *coff_symbol_name_strings;

char **symbol_name_list;		/* list of symbols whose values we want printed, 1 per line */

char *fmt_string = "%04lx";		/* format string for output */

/**************************************************************************/
/**************************************************************************/
/**************************************************************************/
/* Returns -1 if a<b, 0 if a=b, or 1 if a>b */

int dri_symbol_name_compare( const void *a, const char *b )
{
char HUGE *aa;
int i;
	aa = (char HUGE *)a;
	{
		for( i = 0; i < 8; i++ )	/* Check byte by byte so it fails faster! */
		{
			if( *aa > *b )
			  return(1);
			else if( *aa < *b )
			  return(-1);

			aa++;
			b++;
		}
	}
	return 0;
}

/**************************************************************************/
/**************************************************************************/
/**************************************************************************/

/* Returns -1 if a<b, 0 if a=b, or 1 if a>b */

int coff_symbol_name_compare( const void *a, const char *b )
{
BSD_Symbol HUGE *sym1;
char HUGE *str1;
long offset;
int i;

	sym1 = (BSD_Symbol *)a;

	i = 0;
	{
		offset = sym1->name_offset;
		str1 = &coff_symbol_name_strings[offset] - 4;
	
		i = strcmp(str1, b);
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
		  print_dri_symbols( in_handle );
		return(1);
	}

/* Not DRI-format, so test for BSD/COFF. */
	
	else if( theHeader.magic == 0x0150 ||
		(theHeader.magic == 0x0000 && bsd_object.magic == 0x00000107L) )
	{
		read_coff_header( in_handle );
		  print_coff_symbols( in_handle );
		return(1);
	}

/* Sorry, don't know what it is. */

	else
	{
		fprintf(stderr, "Error: Wrong file type.  Magic number = 0x%04x\n", theHeader.magic );
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
		theHeader.res1 = readlong(in_handle);		/* not used... */
		theHeader.tbase = readlong(in_handle);
		theHeader.relocflag = readshort(in_handle);	/* not used... */
		theHeader.dbase = readlong(in_handle);
		theHeader.bbase = readlong(in_handle);
	}
}

/**************************************************************************/
/**************************************************************************/
/**************************************************************************/

void read_coff_header( int in_handle )
{
	if( theHeader.magic == 0x0150 )
	{
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
#if 0
	printf( "Text segment size = 0x%08lx bytes\n", theHeader.tsize );
	printf( "Data segment size = 0x%08lx bytes\n", theHeader.dsize );
	printf( "BSS Segment size = 0x%08lx bytes\n", theHeader.bsize );

	printf( "Symbol Table size = 0x%08lx bytes\n", theHeader.ssize );

	if( theHeader.magic != 0x601a )		/* If not an OBJECT module */
	{
		printf( "Absolute Address for text segment = 0x%08lx\n", theHeader.tbase );
		printf( "Absolute Address for data segment = 0x%08lx\n", theHeader.dbase );
		printf( "Absolute Address for BSS segment = 0x%08lx\n\n", theHeader.bbase );
	}

#endif
}

/**************************************************************************/
/**************************************************************************/
/**************************************************************************/

void print_coff_info(void)
{
#if 0
	if( theHeader.magic == 0x0150 )
	{
		printf( "Text Segment Size = 0x%08lx\n", run_header.tsize );
		printf( "Data Segment Size = 0x%08lx\n", run_header.dsize );
		printf( "BSS Segment Size = 0x%08lx\n", run_header.bsize );

		printf( "Symbol Table contains %ld symbol entries\n", coff_header.num_symbols );

		printf( "Starting Address for executable = 0x%08lx\n", run_header.entry );
		printf( "Start of Text Segment = 0x%08lx\n", run_header.tbase );
		printf( "Start of Data Segment = 0x%08lx\n", run_header.dbase );
	
		printf( "Start of BSS Segment = 0x%08lx\n\n", bss_header.start_address );
	}
	else
	{
		printf( "Text Segment Size = 0x%08lx\n", bsd_object.tsize );
		printf( "Data Segment Size = 0x%08lx\n", bsd_object.dsize );
		printf( "BSS Segment Size = 0x%08lx\n", bsd_object.bsize );
	}
#endif
}

/**************************************************************************/
/**************************************************************************/
/**************************************************************************/

void print_dri_symbols( int fhand )
{
char HUGE *ptr;
long longcount, skipped, offset;
void HUGE *symbuf, HUGE *a, HUGE *b;
char **cursymbol;

/* Read the symbols, print the ones asked for. */
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

	ptr = (char *)symbuf;
	for( longcount = 0; longcount < theHeader.ssize; longcount += 14 )
	{
		Fread( fhand, 14L, ptr );
		ptr += 14;
	}

	ptr = symbuf;

	for (cursymbol = symbol_name_list; *cursymbol; cursymbol++)
	{

		skipped = 1;
		for (longcount = 0 ; longcount < theHeader.ssize ; longcount += 14)
		{
			short i;
			long symval;

			a = ptr;

			/* if we should print this symbol, do so */
			if (!dri_symbol_name_compare(a, *cursymbol))
			{

				for( i = 0; i < 8; i++ )
				{
					if( ptr[i] == 0 )
					{
						for( ; i < 8; i++ )
						  ptr[i] = ' ';
					}
				}

/* print the values byte by byte because it works on any machine... */
				symval = 
					(((unsigned long)ptr[10] & 0xff) << 24) |
					(((unsigned long)ptr[11] & 0xff) << 16) |
					(((unsigned int)ptr[12] & 0xff) << 8) |  ((unsigned int)ptr[13] & 0xff );

				printf( fmt_string, symval );
				printf("\n");
				skipped = 0;
			}
			ptr += 14;
		}

		if (skipped)
			printf("%s: symbol not found\n", *cursymbol);
	}
	farfree( symbuf );
}

/**************************************************************************/
/**************************************************************************/
/**************************************************************************/

void print_coff_symbols( int fhand )
{
long sym, symsize, stringtable_size, offset;
long skipped, unknown_type;
char **cursymbol;

/* Calculate the size needed for the symbol table */

	symsize = coff_header.num_symbols * sizeof(BSD_Symbol);

/* Move to the symbol table, allocate memory for it */
	
	offset = coff_header.sym_offset;
	Fseek(offset,fhand,0);

	coff_symbols = (BSD_Symbol *)farmalloc( symsize );
	if( ! coff_symbols )
	{
		fprintf( stderr, "Cannot allocate memory for symbol information!\n" );
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
		fprintf( stderr, "Cannot allocate memory for symbol information!\n" );
		exit(-1);
	}

	Fread( fhand, stringtable_size, coff_symbol_name_strings );

	for ( cursymbol = symbol_name_list; *cursymbol; cursymbol++ )
	{
		for( sym = 0; sym < coff_header.num_symbols; sym++ )
		{
			long offset, value;
			int type, other, description, show_it;
	
			offset = coff_symbols[sym].name_offset - 4;	/* Adjust offset by -4 as mentioned earlier... */
			value = coff_symbols[sym].value;
			type = (int)coff_symbols[sym].type;
			other = (int)coff_symbols[sym].other;
			description = (int)coff_symbols[sym].description;

			if (!coff_symbol_name_compare(&coff_symbols[sym], *cursymbol)) {
				printf(fmt_string, value);
				printf("\n");
				skipped = 0;
				break;
			}
		}
		if (skipped) {
			printf("%s: symbol not found\n", *cursymbol);
		}
	}	

	farfree( coff_symbols );
	farfree( coff_symbol_name_strings );
}

/**************************************************************************/
/**************************************************************************/
/**************************************************************************/

void usage(void)
{
	printf( "SYMVAL: Version %d.%02d\n\n", MAJOR_VERSION, MINOR_VERSION );

	printf( "Usage:\n\tSYMVAL [-f fmtstring] filename symbol\n\n");
	exit(2);
}

/**************************************************************************/
/**************************************************************************/
/**************************************************************************/

void main( int argc, char *argv[] )
{
int in_handle;
short has_period;
char infile[256], *ptr, *filename;
int argument;

	argv++;			/* skip the program name */

	if (*argv && !strcmp(*argv, "-f")) {
		argv++;
		fmt_string = *argv;
		argv++;
		if (!fmt_string)
			usage();
	}

	if (!argv[0] || !argv[1])
		usage();

/* If you don't sort by name (sort by value), then don't skip duplicate symbols. */

	filename = argv[0];
	argv ++;
	symbol_name_list = argv;

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
			exit(1);
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
				exit(1);
			}
		}
	}
	process_abs_file(infile, in_handle);
	Fclose(in_handle);
	exit(0);
}
