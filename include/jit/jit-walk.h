/*
 * jit-walk.h - Functions for walking stack frames.
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

#ifndef	_JIT_WALK_H
#define	_JIT_WALK_H

#ifdef	__cplusplus
extern	"C" {
#endif

/*
 * Get the frame address for a frame which is "n" levels up the stack.
 * A level value of zero indicates the current frame.
 */
void *_jit_get_frame_address(void *start, unsigned int n);
#if defined(__GNUC__)
#define	jit_get_frame_address(n)	\
		(_jit_get_frame_address(__builtin_frame_address(0), (n)))
#else
#define	jit_get_frame_address(n)	(_jit_get_frame_address(0, (n)))
#endif

/*
 * Get the frame address for the current frame.  May be more efficient
 * than using "jit_get_frame_address(0)".
 */
#if defined(__GNUC__)
#define	jit_get_current_frame()		(__builtin_frame_address(0))
#else
#define	jit_get_current_frame()		(jit_get_frame_address(0))
#endif

/*
 * Get the next frame up the stack from a specified frame.
 * Returns NULL if it isn't possible to retrieve the next frame.
 */
void *jit_get_next_frame_address(void *frame);

/*
 * Get the return address for a specific frame.
 */
void *_jit_get_return_address(void *frame, void *frame0, void *return0);
#if defined(__GNUC__)
#define	jit_get_return_address(frame)	\
		(_jit_get_return_address	\
			((frame), __builtin_frame_address(0), __builtin_return_address(0)))
#else
#define	jit_get_return_address(frame)	\
		(_jit_get_return_address((frame), 0, 0))
#endif

/*
 * Get the return address for the current frame.  May be more efficient
 * than using "jit_get_return_address(0)".
 */
#if defined(__GNUC__)
#define	jit_get_current_return()	(__builtin_return_address(0))
#else
#define	jit_get_current_return()	\
			(jit_get_return_address(jit_get_current_frame()))
#endif

/*
 * Declare a stack crawl mark variable.  The address of this variable
 * can be passed to "jit_frame_contains_crawl_mark" to determine
 * if a frame contains the mark.
 */
typedef struct { void * volatile mark; } jit_crawl_mark_t;
#define	jit_declare_crawl_mark(name)	jit_crawl_mark_t name = {0}

/*
 * Determine if the stack frame just above "frame" contains a
 * particular crawl mark.
 */
int jit_frame_contains_crawl_mark(void *frame, jit_crawl_mark_t *mark);

#ifdef	__cplusplus
};
#endif

#endif /* _JIT_WALK_H */