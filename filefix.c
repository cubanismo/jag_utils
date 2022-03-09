
/*
	filefix.c

	Written by Mike Fulton
	Version 7 recreated by James Jones based on SIZE.C by Mike Fulton
	Last Modified: 9:12pm, March 29 2022

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

#define MAJOR_VERSION (6)
#define MINOR_VERSION (99)

#define SEC_TEXT	(0)
#define SEC_DATA	(1)

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
static int pad = 0;
static uint8_t pad_byte = 0xff;

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

	return(1);
}

#define CHUNK_SIZE 256

void write_sec_file( const char *base_fname, int in_handle, short sec_type )
{
char outfile[260];
uint8_t buf[CHUNK_SIZE];
size_t count, bytes_left;
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

	out_handle = Fopen( outfile, FO_WRONLY | FO_CREATE );

	if ( out_handle < 0 )
	{
		printf( "Can't create %s\n", outfile );
		exit(-1);
	}

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
	}

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

	offset = PACKED_SIZEOF(ABS_HDR) + theHeader.tsize + theHeader.dsize;

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
		if( (longcount+14) < theHeader.ssize )
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
	printf( "-r <romfile> = Create ROM image file named <romfile> from executable (*)\n\n" );
	printf( "-rs <romfile> = Same as -r, except also create DB script to load and run file. (*)\n\n" );
	printf( "-p = Pad ROM file with zero bytes to next 2mb boundary (*)\n" );
	printf( "    (this must be used alongwith the -r or -rs switch)\n\n" );
	printf( "-p4 = Same as -p, exceptpadsto a 4mb boundary (*)\n" );
	printf( "    (this must be used alongwith the -r or -rs switch)\n\n" );
	printf( "-z = Pad unusedportionswith $00 bytes instead of $FF bytes (*)\n" );
	printf( "    (this must be used alongwith the -p or -p4 switch)\n\n" );
	printf( "-f = Use 'fread' command in DB script, instead of 'read'\n\n" );
	printf( " (*) These options are not yet functional in version 7.\n\n" );
}

/**************************************************************************/
/**************************************************************************/
/**************************************************************************/

void main( int argc, char *argv[] )
{
int in_handle;
short has_period;
char infile[260], *ptr, *filename, *rom_filename = NULL;
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
		else if( ! strcmp( "-r", argv[argument] ) )
		{
			argument++;
			if (argument >= argc)
			{
				usage();
				exit(-1);
			}
			rom_filename = argv[argument];
		}
		else if( ! strcmp( "-rs", argv[argument] ) )
		{
			argument++;
			if (argument >= argc)
			{
				usage();
				exit(-1);
			}
			rom_filename = argv[argument];
		}
		else if( ! strcmp( "-p", argv[argument] ) )
		{
			pad = 2;		/* Pad to 2mb boundary */
		}
		else if( ! strcmp( "-p4", argv[argument] ) )
		{
			pad = 4; /* Pad to 4mb boundary */
		}
		else if( ! strcmp( "-z", argv[argument] ) )
		{
			pad_byte = 0x00; /* Pad with $00 instead of $ff */
		}
		else if( ! strcmp( "-f", argv[argument] ) )
		{
			use_fread = 1;
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

	in_handle = Fopen( infile, 0 );
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

		in_handle = Fopen( infile, 0 );
		if( in_handle < 0 )
		{
			/* file.COF not found, so try .ABS extension */

			if((ptr = strchr(infile,'.')) != NULL)
			  *ptr=0;
			strcat(infile,".abs");

			if((in_handle = Fopen(infile,0)) < 0)
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

