#ifndef __AMF_H__
#define __AMF_H__

/* Name of package */
#define PACKAGE "libamf"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT "marc.noirot@gmail.com"

/* Define to the full name of this package. */
#define PACKAGE_NAME "libamf"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "libamf 0.2.0"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "libamf"

/* Define to the version of this package. */
#define PACKAGE_VERSION "0.2.0"

/* The size of `double', as computed by sizeof. */
#define SIZEOF_DOUBLE 8

/* The size of `float', as computed by sizeof. */
#define SIZEOF_FLOAT 4

/* The size of `long double', as computed by sizeof. */
#define SIZEOF_LONG_DOUBLE 16

/* Version number of package */
#define VERSION "0.2.0"

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H

/* Define to 1 if you have the <stddef.h> header file. */
#define HAVE_STDDEF_H

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H

/* Define to 1 if your processor stores words with the most significant byte
   first (like Motorola and SPARC, unlike Intel and VAX). */
/* #undef WORDS_BIGENDIAN */

/* Define to the type of an integer type of width exactly 16 bits if
   such a type exists and the standard includes do not define it. */
/* #undef int16_t */

/* Define to the type of an integer type of width exactly 32 bits if
   such a type exists and the standard includes do not define it. */
/* #undef int32_t */

/* Define to the type of an integer type of width exactly 64 bits if
   such a type exists and the standard includes do not define it. */
/* #undef int64_t */

/* Define to the type of an integer type of width exactly 8 bits if
   such a type exists and the standard includes do not define it. */
/* #undef int8_t */

/* Define to the type of an unsigned integer type of width exactly 16 bits if
   such a type exists and the standard includes do not define it. */
/* #undef uint16_t */

/* Define to the type of an unsigned integer type of width exactly 32 bits if
   such a type exists and the standard includes do not define it. */
/* #undef uint32_t */

/* Define to the type of an unsigned integer type of width exactly 64 bits if
   such a type exists and the standard includes do not define it. */
/* #undef uint64_t */

/* Define to the type of an unsigned integer type of width exactly 8 bits if
   such a type exists and the standard includes do not define it. */
/* #undef uint8_t */

#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#endif

#include <stdlib.h>

/* AMF number */
typedef
#if SIZEOF_FLOAT == 8
float
#elif SIZEOF_DOUBLE == 8
double
#elif SIZEOF_LONG_DOUBLE == 8
long double
#else
uint64_t
#endif
number64_t;

/* custom read/write function type */
typedef size_t (*amf_read_proc_t)(void * out_buffer, size_t size, void * user_data);
typedef size_t (*amf_write_proc_t)(const void * in_buffer, size_t size, void * user_data);

#endif /* __AMF_H__ */
