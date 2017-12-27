

#ifdef __WIN32__
#define HAVE_HTONS 1
#define HAVE_HTONL 1
#define HAVE_NETWORK 1
#endif

#if defined(__APPLE__) && defined(__MACH__)
/*
   #include <TargetConditionals.h>
   #if TARGET_OS_IPHONE == 1 //ios

   #elif TARGET_OS_MAC == 1 //osx

   #endif
*/

   #define AC_APPLE_UNIVERSAL_BUILD
   #define HAVE_TIME_T_IN_TIME_H 1

#endif

#ifdef __linux__ //android falls under this too
#define HAVE_HTONS 1
#define HAVE_HTONL 1
#define HAVE_NETDB_H 1
#define HAVE_NETINET_IN_H 1
#define HAVE_NETWORK 1
#define HAVE_SOCKET 1
#define HAVE_SOCKLEN_T 1
#define HAVE_IN_ADDR_T 1

#endif

#if defined(PSP)
   #error "This platform is not currently supported."

#endif

#if defined(N3DS)
   #error "This platform is not currently supported."

#endif

#define USE_EMBEDDED 1

/* Define WORDS_BIGENDIAN to 1 if your processor stores words with the most
 significant byte first (like Motorola and SPARC, unlike Intel). */
#if defined AC_APPLE_UNIVERSAL_BUILD
   #if defined __BIG_ENDIAN__
      #define WORDS_BIGENDIAN 1
   #endif
#endif

#define HAVE_U_SHORT 1

/* Dingoo has atexit */
#define HAVE_ATEXIT 1

/* Define to 1 if you have the declaration of `sys_siglist', and to 0 if you don't. */
#define HAVE_DECL_SYS_SIGLIST 0

/* Define to 1 if you have the <dirent.h> header file, and it defines `DIR'. */
#define HAVE_DIRENT_H 1

/* Define to 1 if you have the `dirname' function. */
#define HAVE_DIRNAME 1

/* Define to 1 if you have the <errno.h> header file. */
#define HAVE_ERRNO_H 1

/* Define to 1 if you have the `getcwd' function. */
#define HAVE_GETCWD 1

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define to 1 if you have the `m' library (-lm). */
#define HAVE_LIBM 1

/* Define to 1 if you have the <limits.h> header file. */
#define HAVE_LIMITS_H 1

/* Define to 1 if you have the <math.h> header file. */
#define HAVE_MATH_H 1

/* Define to 1 if you have the `memmove' function. */
#define HAVE_MEMMOVE 1

/* Define to 1 if you have the `mkstemp' function. */
#define HAVE_MKSTEMP 1

/* Enable 1351 mouse support */
#define HAVE_MOUSE 1

/* This version provides ReSID support. */
#define HAVE_RESID 

/* Define to 1 if you have the <signal.h> header file. */
#define HAVE_SIGNAL_H 1

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* We are using stdlib.h */
#define HAVE_STDLIB 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the `strcasecmp' function. */
#define HAVE_STRCASECMP 1

/* Define to 1 if you have the `strdup' function. */
#define HAVE_STRDUP 

/* Define to 1 if you have the `strerror' function. */
#define HAVE_STRERROR 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the `strncasecmp' function. */
#define HAVE_STRNCASECMP 1

/* Define to 1 if you have the `strtok' function. */
#define HAVE_STRTOK 1

/* Define to 1 if you have the `strtok_r' function. */
#define HAVE_STRTOK_R 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/time.h> header file. */
#define HAVE_SYS_TIME_H 1

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Can we use the ZLIB compression library? */
#define HAVE_ZLIB 1

/* Define to the full name of this package. */
#define PACKAGE "vice"
/* Define to the full name and version of this package. */

/* Define as the return type of signal handlers (`int' or `void'). */
#define RETSIGTYPE void

/* The size of a `unsigned int', as computed by sizeof. */
#define SIZEOF_UNSIGNED_INT 4

/* The size of a `unsigned long', as computed by sizeof. */
#define SIZEOF_UNSIGNED_LONG 8

/* The size of a `unsigned short', as computed by sizeof. */
#define SIZEOF_UNSIGNED_SHORT 2

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Define to 1 if the X Window System is missing or not being used. */
#define X_DISPLAY_MISSING 1

/* Define to 1 if `lex' declares `yytext' as a `char *' by default, not a `char[]'. */
#define YYTEXT_POINTER 1


