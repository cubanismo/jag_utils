#ifndef __FILEFIX_H_
#define __FILEFIX_H_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#if __MSDOS__
#include <alloc.h>
#include "osbind.h"

#define HUGE huge

#else

#include "osbind.h"

#define PTR_TYPE
#define HUGE

#define farmalloc(x) (void *)Malloc(x)
#define farfree(x) Mfree(x)

#define my_qsort(a,b,c,d) qsort(a,b,c,d)

#endif

typedef struct
{
	short	magic;
	long	tsize;
	long	dsize;
	long	bsize;
	long	ssize;
	long	res1;
	long	tbase;
	short	relocflag;
	long	dbase;
	long	bbase;
} ABS_HDR;

typedef struct
{
	short	magic;		/* Should be 0x0150 */
	short	num_sections;   /* Normally 3 (Text, Data, Symbols) */
	long	date;
	long	sym_offset;
	long	num_symbols;
	short	opt_hdr_size;
	short	flags;		/* Executable flags... */
} COF_HDR;

typedef struct
{
	long	magic;		/* Should be 0x00000107L */
	long	tsize;
	long	dsize;
	long	bsize;
	long	entry;		/* Start of executable   (????) */
	long	tbase;		/* Start of text segment */
	long	dbase;		/* Start of data segment */
} RUN_HDR;

typedef unsigned long SECFLAGS;

typedef struct
{
	char	name[8];		/* Name of section */
	long	start_address;		/* Starting address of section */
	long	start_address_2;	/* Same as 'start_address' as far as I can tell... */
	long	size;			/* Size of section */
	long	offset;			/* Offset within file to section's data */
	long	relocation_data;	/* Offset within file to relocation data for section */
	long	debug_info;		/* Offset within file to debug info for section */
	short	num_reloc_entries;	/* Number of relocation entries */
	short	num_debug_entries;	/* Number of debug entries */
	SECFLAGS	flags;		/* Flags: 0x20 = TEXT, 0x40 = DATA, 0x80 = BSS */
} SEC_HDR;

/**************************************************************************/
/**************************************************************************/
/**************************************************************************/

typedef struct
{
	unsigned long	name_offset;
	unsigned char	type;
	unsigned char	other;
	unsigned short	description;
	unsigned long	value;
} BSD_Symbol;

typedef struct
{
	char		name[8];
	unsigned short	type;
	long		value;
} DRI_Symbol;

typedef struct
{
	short	magic;
	long	tsize;
	long	dsize;
	long	bsize;
	long	ssize;
	char	reserved[10];
} DRI_Object;

typedef struct
{
	long	magic;		/* Should be 0x00000107L */
	long	tsize;		/* Size of text segment */
	long	dsize;		/* Size of data segment */
	long	bsize;		/* Size of Bss segment */
	long	ssize;		/* Size of symbol table */
	long	entry;		/* Start of executable   (????) */
	long	trsize;		/* Size of text segment relocation info */
	long	drsize;		/* Size of data segment relocation info */
} BSD_Object;

#endif /* __FILEFIX_H_ */
