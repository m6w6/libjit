/*
 * jit-alloc.c - Memory allocation routines.
 *
 * Copyright (C) 2004  Southern Storm Software, Pty Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "jit-internal.h"
#include <config.h>
#ifdef HAVE_STDLIB_H
	#include <stdlib.h>
#endif
#ifndef JIT_WIN32_PLATFORM
#ifdef HAVE_SYS_TYPES_H
	#include <sys/types.h>
#endif
#ifdef HAVE_UNISTD_H
	#include <unistd.h>
#endif
#ifdef HAVE_SYS_MMAN_H
	#include <sys/mman.h>
#endif
#ifdef HAVE_FCNTL_H
	#include <fcntl.h>
#endif
#else /* JIT_WIN32_PLATFORM */
	#include <windows.h>
	#include <io.h>
#endif

/*@
 * @section Memory allocation
 *
 * The @code{libjit} library provides an interface to the traditional
 * system @code{malloc} routines.  All heap allocation in @code{libjit}
 * goes through these functions.  If you need to perform some other kind
 * of memory allocation, you can replace these functions with your
 * own versions.
@*/

/*@
 * @deftypefun {void *} jit_malloc ({unsigned int} size)
 * Allocate @code{size} bytes of memory from the heap.
 * @end deftypefun
 *
 * @deftypefun {type *} jit_new (type)
 * Allocate @code{sizeof(type)} bytes of memory from the heap and
 * cast the return pointer to @code{type *}.  This is a macro that
 * wraps up the underlying @code{jit_malloc} function and is less
 * error-prone when allocating structures.
 * @end deftypefun
@*/
void *jit_malloc(unsigned int size)
{
	return malloc(size);
}

/*@
 * @deftypefun {void *} jit_calloc ({unsigned int} num, {unsigned int} size)
 * Allocate @code{num * size} bytes of memory from the heap and clear
 * them to zero.
 * @end deftypefun
 *
 * @deftypefun {type *} jit_cnew (type)
 * Allocate @code{sizeof(type)} bytes of memory from the heap and
 * cast the return pointer to @code{type *}.  The memory is cleared
 * to zero.
 * @end deftypefun
@*/
void *jit_calloc(unsigned int num, unsigned int size)
{
	return calloc(num, size);
}

/*@
 * @deftypefun {void *} jit_realloc ({void *} ptr, {unsigned int} size)
 * Re-allocate the memory at @code{ptr} to be @code{size} bytes in size.
 * The memory block at @code{ptr} must have been allocated by a previous
 * call to @code{jit_malloc}, @code{jit_calloc}, or @code{jit_realloc}.
 * @end deftypefun
@*/
void *jit_realloc(void *ptr, unsigned int size)
{
	return realloc(ptr, size);
}

/*@
 * @deftypefun void jit_free ({void *} ptr)
 * Free the memory at @code{ptr}.  It is safe to pass a NULL pointer.
 * @end deftypefun
@*/
void jit_free(void *ptr)
{
	if(ptr)
	{
		free(ptr);
	}
}

/*@
 * @deftypefun {void *} jit_malloc_exec ({unsigned int} size)
 * Allocate a block of memory that is read/write/executable.  Such blocks
 * are used to store JIT'ed code, function closures, and other trampolines.
 * The size should be a multiple of @code{jit_exec_page_size()}.
 *
 * This will usually be identical to @code{jit_malloc}.  However,
 * some systems may need special handling to create executable code
 * segments, so this function must be used instead.
 *
 * You must never mix regular and executable segment allocation.  That is,
 * do not use @code{jit_free} to free the result of @code{jit_malloc_exec}.
 * @end deftypefun
@*/
void *jit_malloc_exec(unsigned int size)
{
	return malloc(size);
}

/*@
 * @deftypefun void jit_free_exec ({void *} ptr, {unsigned int} size)
 * Free a block of memory that was previously allocated by
 * @code{jit_malloc_exec}.  The @code{size} must be identical to the
 * original allocated size, as some systems need to know this information
 * to be able to free the block.
 * @end deftypefun
@*/
void jit_free_exec(void *ptr, unsigned int size)
{
	if(ptr)
	{
		free(ptr);
	}
}

/*@
 * @deftypefun void jit_flush_exec ({void *} ptr, {unsigned int} size)
 * Flush the contents of the block at @code{ptr} from the CPU's
 * data and instruction caches.  This must be used after the code is
 * written to an executable code segment, but before the code is
 * executed, to prepare it for execution.
 * @end deftypefun
@*/
void jit_flush_exec(void *ptr, unsigned int size)
{
#if defined(__GNUC__)
#if defined(PPC)

	/* Flush the CPU cache on PPC platforms */
	register unsigned char *p;

	/* Flush the data out of the data cache */
	p = (unsigned char *)ptr;
	while(size > 0)
	{
		__asm__ __volatile__ ("dcbst 0,%0" :: "r"(p));
		p += 4;
		size -= 4;
	}
	__asm__ __volatile__ ("sync");

	/* Invalidate the cache lines in the instruction cache */
	p = (unsigned char *)ptr;
	while(size > 0)
	{
		__asm__ __volatile__ ("icbi 0,%0; isync" :: "r"(p));
		p += 4;
		size -= 4;
	}
	__asm__ __volatile__ ("isync");

#elif defined(__sparc)

	/* Flush the CPU cache on sparc platforms */
	register unsigned char *p = (unsigned char *)ptr;
	__asm__ __volatile__ ("stbar");
	while(size > 0)
	{
		__asm__ __volatile__ ("flush %0" :: "r"(p));
		p += 4;
		size -= 4;
	}
	__asm__ __volatile__ ("nop; nop; nop; nop; nop");

#elif (defined(__arm__) || defined(__arm)) && defined(linux)

	/* ARM Linux has a "cacheflush" system call */
	/* R0 = start of range, R1 = end of range, R3 = flags */
	/* flags = 0 indicates data cache, flags = 1 indicates both caches */
	__asm __volatile ("mov r0, %0\n"
	                  "mov r1, %1\n"
					  "mov r2, %2\n"
					  "swi 0x9f0002       @ sys_cacheflush"
					  : /* no outputs */
					  : "r" (ptr),
					    "r" (((int)ptr) + (int)size),
						"r" (0)
					  : "r0", "r1", "r3" );

#elif (defined(__ia64) || defined(__ia64__)) && defined(linux)
	void *end = (char*)ptr + size;
	while(ptr < end)
	{
		asm volatile("fc %0" :: "r"(ptr));
		ptr = (char*)ptr + 32;
	}
	asm volatile(";;sync.i;;srlz.i;;");
#endif
#endif	/* __GNUC__ */
}

/*@
 * @deftypefun {unsigned int} jit_exec_page_size (void)
 * Get the page allocation size for the system.  This is the preferred
 * unit when making calls to @code{jit_malloc_exec}.  It is not
 * required that you supply a multiple of this size when allocating,
 * but it can lead to better performance on some systems.
 * @end deftypefun
@*/
unsigned int jit_exec_page_size(void)
{
#ifndef JIT_WIN32_PLATFORM
	/* Get the page size using a Unix-like sequence */
	#ifdef HAVE_GETPAGESIZE
		return (unsigned long)getpagesize();
	#else
		#ifdef NBPG
			return NBPG;
		#else
			#ifdef PAGE_SIZE
				return PAGE_SIZE;
			#else
				return 4096;
			#endif
		#endif
	#endif
#else
	/* Get the page size from a Windows-specific API */
	SYSTEM_INFO sysInfo;
	GetSystemInfo(&sysInfo);
	return (unsigned long)(sysInfo.dwPageSize);
#endif
}