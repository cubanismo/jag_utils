#ifndef __FILEFIX_H_
#define __FILEFIX_H_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#if __MSDOS__
#include <alloc.h>
#include "osbind.h"
typedef long int32_t;
typedef unsigned long uint32_t;
typedef short int16_t;
typedef unsigned short uint16_t;
typedef char int8_t;
typedef unsigned char uint8_t;

#define HUGE huge
#define FAR far

#else

#include "osbind.h"
#include <stdint.h>

#define PTR_TYPE
#define HUGE
#define FAR

#define farmalloc(x) (void *)Malloc(x)
#define farfree(x) Mfree(x)

#define my_qsort(a,b,c,d) qsort(a,b,c,d)

#endif

#define PACKED_SIZEOF(a) PACKED_SIZE_##a

typedef struct
{
	int16_t	magic;
	int32_t	tsize;
	int32_t	dsize;
	int32_t	bsize;
	int32_t	ssize;
	int32_t	res1;
	int32_t	tbase;
	int16_t	relocflag;
	int32_t	dbase;
	int32_t	bbase;
} ABS_HDR;
#define PACKED_SIZE_ABS_HDR ( \
	sizeof(int16_t) /* magic */ \
	+ sizeof(int32_t) /* tsize */ \
	+ sizeof(int32_t) /* dsize */ \
	+ sizeof(int32_t) /* bsize */ \
	+ sizeof(int32_t) /* ssize */ \
	+ sizeof(int32_t) /* res1 */ \
	+ sizeof(int32_t) /* tbase */ \
	+ sizeof(int16_t) /* relocflag */ \
	+ sizeof(int32_t) /* dbase */ \
	+ sizeof(int32_t) /* bbase */)

typedef struct
{
	int16_t	magic;		/* Should be 0x0150 */
	int16_t	num_sections;   /* Normally 3 (Text, Data, Symbols) */
	int32_t	date;
	int32_t	sym_offset;
	int32_t	num_symbols;
	int16_t	opt_hdr_size;
	int16_t	flags;		/* Executable flags... */
} COF_HDR;
#define PACKED_SIZE_COF_HDR ( \
	sizeof(int16_t) /* magic */ \
	+ sizeof(int16_t) /* num_sections */ \
	+ sizeof(int32_t) /* dat */ \
	+ sizeof(int32_t) /* sym_offse */ \
	+ sizeof(int32_t) /* num_symbol */ \
	+ sizeof(int16_t) /* opt_hdr_siz */ \
	+ sizeof(int16_t) /* flags */)

typedef struct
{
	int32_t	magic;		/* Should be 0x00000107L */
	int32_t	tsize;
	int32_t	dsize;
	int32_t	bsize;
	int32_t	entry;		/* Start of executable   (????) */
	int32_t	tbase;		/* Start of text segment */
	int32_t	dbase;		/* Start of data segment */
} RUN_HDR;
#define PACKED_SIZE_RUN_HDR ( \
	sizeof(int32_t) /* magic */ \
	+ sizeof(int32_t) /* tsize */ \
	+ sizeof(int32_t) /* dsize */ \
	+ sizeof(int32_t) /* bsize */ \
	+ sizeof(int32_t) /* entry */ \
	+ sizeof(int32_t) /* tbase */ \
	+ sizeof(int32_t) /* dbase */)

typedef uint32_t SECFLAGS;

typedef struct
{
	int8_t	name[8];		/* Name of section */
	int32_t	start_address;		/* Starting address of section */
	int32_t	start_address_2;	/* Same as 'start_address' as far as I can tell... */
	int32_t	size;			/* Size of section */
	int32_t	offset;			/* Offset within file to section's data */
	int32_t	relocation_data;	/* Offset within file to relocation data for section */
	int32_t	debug_info;		/* Offset within file to debug info for section */
	int16_t	num_reloc_entries;	/* Number of relocation entries */
	int16_t	num_debug_entries;	/* Number of debug entries */
	SECFLAGS	flags;		/* Flags: 0x20 = TEXT, 0x40 = DATA, 0x80 = BSS */
} SEC_HDR;
#define PACKED_SIZE_SEC_HDR ( \
	(sizeof(int8_t)*8) /* name */ \
	+ sizeof(int32_t) /* start_address */ \
	+ sizeof(int32_t) /* start_address_2 */ \
	+ sizeof(int32_t) /* size */ \
	+ sizeof(int32_t) /* offset */ \
	+ sizeof(int32_t) /* relocation_data */ \
	+ sizeof(int32_t) /* debug_info */ \
	+ sizeof(int16_t) /* num_reloc_entries */ \
	+ sizeof(int16_t) /* num_debug_entries */ \
	+ sizeof(SECFLAGS) /* flags */ )


/**************************************************************************/
/**************************************************************************/
/**************************************************************************/

typedef struct
{
	uint32_t	name_offset;
	uint8_t		type;
	uint8_t		other;
	uint16_t	description;
	uint32_t	value;
} BSD_Symbol;
#define PACKED_SIZE_BSD_Symbol ( \
	sizeof(uint32_t) /* name_offset */ \
	+ sizeof(uint8_t) /* type */ \
	+ sizeof(uint8_t) /* other */ \
	+ sizeof(uint16_t) /* description */ \
	+ sizeof(uint32_t) /* value */ )

typedef struct
{
	int8_t		name[8];
	uint16_t	type;
	int32_t		value;
} DRI_Symbol;
#define PACKED_SIZE_DRI_Symbol ( \
	(sizeof(int8_t)*8) /* name */ \
	+ sizeof(uint16_t) /* type */ \
	+ sizeof(int32_t) /* value */ )


typedef struct
{
	int16_t	magic;
	int32_t	tsize;
	int32_t	dsize;
	int32_t	bsize;
	int32_t	ssize;
	int8_t	reserved[10];
} DRI_Object;
#define PACKED_SIZE_DRI_Object ( \
	sizeof(int16_t) /* magic */ \
	+ sizeof(int32_t) /* tsize */ \
	+ sizeof(int32_t) /* dsize */ \
	+ sizeof(int32_t) /* bsize */ \
	+ sizeof(int32_t) /* ssize */ \
	+ (sizeof(int8_t)*10) /* reserved */ )


typedef struct
{
	int32_t	magic;		/* Should be 0x00000107L */
	int32_t	tsize;		/* Size of text segment */
	int32_t	dsize;		/* Size of data segment */
	int32_t	bsize;		/* Size of Bss segment */
	int32_t	ssize;		/* Size of symbol table */
	int32_t	entry;		/* Start of executable   (????) */
	int32_t	trsize;		/* Size of text segment relocation info */
	int32_t	drsize;		/* Size of data segment relocation info */
} BSD_Object;
#define PACKED_SIZE_BSD_Object ( \
	sizeof(int32_t) /* magic */ \
	+ sizeof(int32_t) /* tsize */ \
	+ sizeof(int32_t) /* dsize */ \
	+ sizeof(int32_t) /* bsize */ \
	+ sizeof(int32_t) /* ssize */ \
	+ sizeof(int32_t) /* entry */ \
	+ sizeof(int32_t) /* trsize */ \
	+ sizeof(int32_t) /* drsize */ )

#endif /* __FILEFIX_H_ */
