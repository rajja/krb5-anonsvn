/*
 * k5-platform.h
 *
 * Copyright 2003  by the Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 *   require a specific license from the United States Government.
 *   It is the responsibility of any person or organization contemplating
 *   export to obtain such a license before exporting.
 * 
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.	Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 * 
 *
 * Some platform-dependent definitions to sync up the C support level.
 * Some to a C99-ish level, some related utility code.
 *
 * Currently:
 * + make "static inline" work
 * + 64-bit types and load/store code
 * + SIZE_MAX
 * + shared library init/fini hooks
 */

#ifndef K5_PLATFORM_H
#define K5_PLATFORM_H

#if !defined(inline)
# if __STDC_VERSION__ >= 199901L
/* C99 supports inline, don't do anything.  */
# elif defined(__GNUC__)
#  define inline __inline__ /* this form silences -pedantic warnings */
# elif defined(__mips) && defined(__sgi)
#  define inline __inline /* IRIX used at MIT does inline but not c99 yet */
# elif defined(__sun) && __SUNPRO_C >= 0x540
/* The Forte Developer 7 C compiler supports "inline".  */
# elif defined(_WIN32)
#  define inline __inline
# else
#  define inline /* nothing, just static */
# endif
#endif

#include "autoconf.h"


/* Initialization and finalization function support for libraries.

   At top level, before the functions are defined or even declared:
   MAKE_INIT_FUNCTION(init_fn);
   MAKE_FINI_FUNCTION(fini_fn);
   int init_fn(void) { ... }
   void fini_fn(void) { if (INITIALIZER_RAN(init_fn)) ... }

   In code, in the same file:
   err = CALL_INIT_FUNCTION(init_fn);

   To trigger or verify the initializer invocation from another file,
   an additional function must be created.

   The init_fn and fini_fn names should be chosen such that any
   exported names staring with those names, and optionally followed by
   additional characters, fits in with any namespace constraints on
   the library in question.


   Implementation outline:

   Windows: MAKE_FINI_FUNCTION creates a symbol with a magic name that
   is sought at library build time, and code is added to invoke the
   function when the library is unloaded.  MAKE_INIT_FUNCTION does
   likewise, but the function is invoked when the library is loaded,
   and an extra variable is declared to hold an error code and a "yes
   the initializer ran" flag.  CALL_INIT_FUNCTION blows up if the flag
   isn't set, otherwise returns the error code.

   UNIX: MAKE_INIT_FUNCTION creates and initializes a variable with a
   name derived from the function name, containing a k5_once_t
   (pthread_once_t or int), an error code, and a pointer to the
   function.  The function itself is declared static, but the
   associated variable has external linkage.  CALL_INIT_FUNCTION
   ensures thath the function is called exactly once (pthread_once or
   just check the flag) and returns the stored error code (or the
   pthread_once error).

   UNIX, with compiler support: MAKE_FINI_FUNCTION declares the
   function as a destructor, and the run time linker support or
   whatever will cause it to be invoked when the library is unloaded,
   the program ends, etc.

   UNIX, with linker support: MAKE_FINI_FUNCTION creates a symbol with
   a magic name that is sought at library build time, and linker
   options are used to mark it as a finalization function for the
   library.  The symbol must be exported.

   UNIX, no library finalization support: The finalization function
   never runs, and we leak memory.  Tough.



   For maximum flexibility in defining the macros, the function name
   parameter should be a simple name, not even a macro defined as
   another name.  The function should have a unique name, and should
   conform to whatever namespace is used by the library in question.

   If the macro expansion needs the function to have been declared, it
   must include a declaration.  If it is not necessary for the symbol
   name to be exported from the object file, the macro should declare
   it as "static".  Hence the signature must exactly match "void
   foo(void)".  (ANSI C allows a static declaration followed by a
   non-static one; the result is internal linkage.)  The macro
   expansion has to come before the function, because gcc apparently
   won't act on "__attribute__((constructor))" if it comes after the
   function definition.

   This is going to be compiler- and environment-specific, and may
   require some support at library build time, and/or "asm"
   statements.

   It's okay for this code to require that the library be built
   with the same compiler and compiler options throughout, but
   we shouldn't require that the library and application use the
   same compiler.

   For static libraries, we don't really care about cleanup too much,
   since it's all memory handling and mutex allocation which will all
   be cleaned up when the program exits.  Thus, it's okay if gcc-built
   static libraries don't play nicely with cc-built executables when
   it comes to static constructors, just as long as it doesn't cause
   linking to fail.

   For dynamic libraries on UNIX, we'll use pthread_once-type support
   to do delayed initialization, so if finalization can't be made to
   work, we'll only have memory leaks in a load/use/unload cycle.  If
   anyone (like, say, the OS vendor) complains about this, they can
   tell us how to get a shared library finalization function invoked
   automatically.  */

/* Helper macros.  */

# define JOIN4_2(A,B,C,D) A ## B ## C ## D
# define JOIN4(A,B,C,D) JOIN4_2(A,B,C,D)
# define JOIN3_2(A,B,C) A ## B ## C
# define JOIN3(A,B,C) JOIN3_2(A,B,C)
# define JOIN2_2(A,B) A ## B
# define JOIN2(A,B) JOIN2_2(A,B)

/* XXX Should test USE_LINKER_INIT_OPTION early, and if it's set,
   always provide a function by the expected name, even if we're
   delaying initialization.  */

#if defined(DELAY_INITIALIZER)

/* Run the initialization code during program execution, at the latest
   possible moment.  This means multiple threads may be active.  */
# include "k5-thread.h"
typedef struct { k5_once_t once; int error, did_run; void (*fn)(void); } k5_init_t;
# ifdef USE_LINKER_INIT_OPTION
#  define MAYBE_DUMMY_INIT(NAME)		\
	void JOIN2(NAME, __auxinit) () { }
# else
#  define MAYBE_DUMMY_INIT(NAME)
# endif
# define MAKE_INIT_FUNCTION(NAME)				\
	static int NAME(void);					\
	MAYBE_DUMMY_INIT(NAME)					\
	/* forward declaration for use in initializer */	\
	static void JOIN2(NAME, __aux) (void);			\
	static k5_init_t JOIN2(NAME, __once) =			\
		{ K5_ONCE_INIT, 0, 0, JOIN2(NAME, __aux) };	\
	static void JOIN2(NAME, __aux) (void)			\
	{							\
	    JOIN2(NAME, __once).did_run = 1;			\
	    JOIN2(NAME, __once).error = NAME();			\
	}							\
	/* so ';' following macro use won't get error */	\
	static int NAME(void)
# define CALL_INIT_FUNCTION(NAME)	\
	k5_call_init_function(& JOIN2(NAME, __once))
static inline int k5_call_init_function(k5_init_t *i)
{
    int err;
    err = k5_once(&i->once, i->fn);
    if (err)
	return err;
    assert (i->did_run != 0);
    return i->error;
}
/* This should be called in finalization only, so we shouldn't have
   multiple active threads mucking around in our library at this
   point.  So ignore the once_t object and just look at the flag.

   XXX Could we have problems with memory coherence between
   processors if we don't invoke mutex/once routines?  */
# define INITIALIZER_RAN(NAME)	\
	(JOIN2(NAME, __once).did_run && JOIN2(NAME, __once).error == 0)

# define PROGRAM_EXITING()		(0)

#elif defined(__GNUC__) && !defined(_WIN32) && defined(CONSTRUCTOR_ATTR_WORKS)

/* Run initializer at load time, via GCC/C++ hook magic.  */

# ifdef USE_LINKER_INIT_OPTION
#  define MAYBE_DUMMY_INIT(NAME)		\
	void JOIN2(NAME, __auxinit) () { }
# else
#  define MAYBE_DUMMY_INIT(NAME)
# endif

typedef struct { int error; unsigned char did_run; } k5_init_t;
# define MAKE_INIT_FUNCTION(NAME)		\
	MAYBE_DUMMY_INIT(NAME)			\
	static k5_init_t JOIN2(NAME, __ran)	\
		= { 0, 2 };			\
	static void JOIN2(NAME, __aux)(void)	\
	    __attribute__((constructor));	\
	static int NAME(void);			\
	static void JOIN2(NAME, __aux)(void)	\
	{					\
	    JOIN2(NAME, __ran).error = NAME();	\
	    JOIN2(NAME, __ran).did_run = 3;	\
	}					\
	static int NAME(void)
# define CALL_INIT_FUNCTION(NAME)		\
	(JOIN2(NAME, __ran).did_run == 3	\
	 ? JOIN2(NAME, __ran).error		\
	 : (abort(),0))
# define INITIALIZER_RAN(NAME)	(JOIN2(NAME,__ran).did_run == 3 && JOIN2(NAME, __ran).error == 0)

#elif defined(USE_LINKER_INIT_OPTION)

/* Run initializer at load time, via linker magic.  */
typedef struct { int error; unsigned char did_run; } k5_init_t;
# define MAKE_INIT_FUNCTION(NAME)		\
	static k5_init_t JOIN2(NAME, __ran)	\
		= { 0, 2 };			\
	static int NAME(void);			\
	void JOIN2(NAME, __auxinit)		\
	{					\
	    JOIN2(NAME, __ran).error = NAME();	\
	    JOIN2(NAME, __ran).did_run = 3;	\
	}					\
	static int NAME(void)
# define CALL_INIT_FUNCTION(NAME)		\
	(JOIN2(NAME, __ran).did_run == 3	\
	 ? JOIN2(NAME, __ran).error		\
	 : (abort(),0))
# define INITIALIZER_RAN(NAME)	\
	(JOIN2(NAME, __ran).error == 0)

# define PROGRAM_EXITING()		(0)

#else

# error "Don't know how to do load-time initializers for this configuration."

# define PROGRAM_EXITING()		(0)

#endif



#ifdef USE_LINKER_FINI_OPTION
/* If we're told the linker option will be used, it doesn't really
   matter what compiler we're using.  Do it the same way
   regardless.  */

# define MAKE_FINI_FUNCTION(NAME)	\
	void NAME(void)

#elif defined(__GNUC__) && !defined(_WIN32) && defined(DESTRUCTOR_ATTR_WORKS)
/* If we're using gcc, if the C++ support works, the compiler should
   build executables and shared libraries that support the use of
   static constructors and destructors.  The C compiler supports a
   function attribute that makes use of the same facility as C++.

   XXX How do we know if the C++ support actually works?  */
# define MAKE_FINI_FUNCTION(NAME)	\
	static void NAME(void) __attribute__((destructor))

#elif !defined(SHARED)

/* In this case, we just don't care about finalization.

   The code will still define the function, but we won't do anything
   with it.  Annoying: This may generate unused-function warnings.  */

# define MAKE_FINI_FUNCTION(NAME)	\
	static void NAME(void)

#else

# error "Don't know how to do unload-time finalization for this configuration."

#endif


/* 64-bit support: krb5_ui_8 and krb5_int64.

   This should move to krb5.h eventually, but without the namespace
   pollution from the autoconf macros.  */
#if defined(HAVE_STDINT_H) || defined(HAVE_INTTYPES_H)
# ifdef HAVE_STDINT_H
#  include <stdint.h>
# endif
# ifdef HAVE_INTTYPES_H
#  include <inttypes.h>
# endif
# define INT64_TYPE int64_t
# define UINT64_TYPE uint64_t
#elif defined(_WIN32)
# define INT64_TYPE signed __int64
# define UINT64_TYPE unsigned __int64
#else /* not Windows, and neither stdint.h nor inttypes.h */
# define INT64_TYPE signed long long
# define UINT64_TYPE unsigned long long
#endif

#include <limits.h>
#ifndef SIZE_MAX
# define SIZE_MAX ((size_t)((size_t)0 - 1))
#endif

/* Read and write integer values as (unaligned) octet strings in
   specific byte orders.

   Add per-platform optimizations later if needed.  (E.g., maybe x86
   unaligned word stores and gcc/asm instructions for byte swaps,
   etc.)  */

static inline void
store_16_be (unsigned int val, unsigned char *p)
{
    p[0] = (val >>  8) & 0xff;
    p[1] = (val      ) & 0xff;
}
static inline void
store_16_le (unsigned int val, unsigned char *p)
{
    p[1] = (val >>  8) & 0xff;
    p[0] = (val      ) & 0xff;
}
static inline void
store_32_be (unsigned int val, unsigned char *p)
{
    p[0] = (val >> 24) & 0xff;
    p[1] = (val >> 16) & 0xff;
    p[2] = (val >>  8) & 0xff;
    p[3] = (val      ) & 0xff;
}
static inline void
store_32_le (unsigned int val, unsigned char *p)
{
    p[3] = (val >> 24) & 0xff;
    p[2] = (val >> 16) & 0xff;
    p[1] = (val >>  8) & 0xff;
    p[0] = (val      ) & 0xff;
}
static inline void
store_64_be (UINT64_TYPE val, unsigned char *p)
{
    p[0] = (unsigned char)((val >> 56) & 0xff);
    p[1] = (unsigned char)((val >> 48) & 0xff);
    p[2] = (unsigned char)((val >> 40) & 0xff);
    p[3] = (unsigned char)((val >> 32) & 0xff);
    p[4] = (unsigned char)((val >> 24) & 0xff);
    p[5] = (unsigned char)((val >> 16) & 0xff);
    p[6] = (unsigned char)((val >>  8) & 0xff);
    p[7] = (unsigned char)((val      ) & 0xff);
}
static inline void
store_64_le (UINT64_TYPE val, unsigned char *p)
{
    p[7] = (unsigned char)((val >> 56) & 0xff);
    p[6] = (unsigned char)((val >> 48) & 0xff);
    p[5] = (unsigned char)((val >> 40) & 0xff);
    p[4] = (unsigned char)((val >> 32) & 0xff);
    p[3] = (unsigned char)((val >> 24) & 0xff);
    p[2] = (unsigned char)((val >> 16) & 0xff);
    p[1] = (unsigned char)((val >>  8) & 0xff);
    p[0] = (unsigned char)((val      ) & 0xff);
}
static inline unsigned short
load_16_be (unsigned char *p)
{
    return (p[1] | (p[0] << 8));
}
static inline unsigned short
load_16_le (unsigned char *p)
{
    return (p[0] | (p[1] << 8));
}
static inline unsigned int
load_32_be (unsigned char *p)
{
    return (p[3] | (p[2] << 8) | (p[1] << 16) | (p[0] << 24));
}
static inline unsigned int
load_32_le (unsigned char *p)
{
    return (p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
}
static inline UINT64_TYPE
load_64_be (unsigned char *p)
{
    return ((UINT64_TYPE)load_32_be(p) << 32) | load_32_be(p+4);
}
static inline UINT64_TYPE
load_64_le (unsigned char *p)
{
    return ((UINT64_TYPE)load_32_le(p+4) << 32) | load_32_le(p);
}

#endif /* K5_PLATFORM_H */
