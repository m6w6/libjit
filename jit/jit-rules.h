/*
 * jit-rules.h - Rules that define the characteristics of the back-end.
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

#ifndef	_JIT_RULES_H
#define	_JIT_RULES_H

#include "jit-cache.h"

#ifdef	__cplusplus
extern	"C" {
#endif

/*
 * Determine which backend to use.
 */
#define	JIT_BACKEND_INTERP		1
/*#define	JIT_BACKEND_X86		1*/
/*#define	JIT_BACKEND_ARM		1*/

/*
 * Information about a register.
 */
typedef struct
{
	const char *name;			/* Name of the register, for debugging */
	short		cpu_reg;		/* CPU register number */
	short		other_reg;		/* Other register for a "long" pair, or -1 */
	int			flags;			/* Flags that define the register type */

} jit_reginfo_t;

/*
 * Register information flags.
 */
#define	JIT_REG_WORD		(1 << 0)	/* Can be used for word values */
#define	JIT_REG_LONG		(1 << 1)	/* Can be used for long values */
#define	JIT_REG_FLOAT		(1 << 2)	/* Can be used for float values */
#define	JIT_REG_FRAME		(1 << 3)	/* Contains frame pointer */
#define	JIT_REG_STACK_PTR	(1 << 4)	/* Contains CPU stack pointer */
#define	JIT_REG_FIXED		(1 << 5)	/* Fixed use; not for allocation */
#define	JIT_REG_CALL_USED	(1 << 6)	/* Destroyed by a call */
#define	JIT_REG_START_STACK	(1 << 7)	/* Stack of stack-like allocation */
#define	JIT_REG_END_STACK	(1 << 8)	/* End of stack-like allocation */
#define	JIT_REG_IN_STACK	(1 << 9)	/* Middle of stack-like allocation */
#define	JIT_REG_GLOBAL		(1 << 10)	/* Candidate for global allocation */

/*
 * Include definitions that are specific to the backend.
 */
#if defined(JIT_BACKEND_INTERP)
	#include "jit-rules-interp.h"
#elif defined(JIT_BACKEND_X86)
	#include "jit-rules-x86.h"
#elif defined(JIT_BACKEND_ARM)
	#include "jit-rules-arm.h"
#else
	#error "unknown jit backend type"
#endif

/*
 * The information blocks for all registers in the system.
 */
extern jit_reginfo_t const _jit_reg_info[JIT_NUM_REGS];

/*
 * Manipulate register usage masks.  The backend may override these
 * definitions if it has more registers than can fit in a "jit_uint".
 */
#if !defined(jit_regused_init)
typedef jit_uint jit_regused_t;
#define	jit_regused_init				(0)
#define	jit_reg_is_used(mask,reg)		\
			(((mask) & (((jit_uint)1) << (reg))) != 0)
#define	jit_reg_set_used(mask,reg)		((mask) |= (((jit_uint)1) << (reg)))
#define	jit_reg_clear_used(mask,reg)	((mask) &= ~(((jit_uint)1) << (reg)))
#endif

/*
 * Information about a register's contents.
 */
#define	JIT_MAX_REG_VALUES		8
typedef struct jit_regcontents jit_regcontents_t;
struct jit_regcontents
{
	/* List of values that are currently stored in this register */
	jit_value_t		values[JIT_MAX_REG_VALUES];
	short			num_values;

	/* Flag that indicates if this register is holding the first
	   word of a double-word long value (32-bit platforms only) */
	char			is_long_start;

	/* Flag that indicates if this register is holding the second
	   word of a double-word long value (32-bit platforms only) */
	char			is_long_end;

	/* Current age of this register.  Older registers are reclaimed first */
	int				age;

	/* Remapped version of this register, when used in a stack */
	short			remap;

	/* Flag that indicates if the register holds a valid value,
	   but there are no actual "jit_value_t" objects associated */
	short			used_for_temp;
};

/*
 * Code generation information.
 */
typedef struct jit_gencode *jit_gencode_t;
struct jit_gencode
{
	jit_regused_t		permanent;	/* Permanently allocated global regs */
	jit_regused_t		touched;	/* All registers that were touched */
	jit_cache_posn		posn;		/* Current cache output position */
	jit_regcontents_t	contents[JIT_NUM_REGS]; /* Contents of each register */
	int					current_age;/* Current age value for registers */
	int					stack_map[JIT_NUM_REGS]; /* Reverse stack mappings */
#ifdef jit_extra_gen_state
	jit_extra_gen_state;			/* CPU-specific extra information */
#endif
};

/*
 * ELF machine type and ABI information.
 */
typedef struct jit_elf_info jit_elf_info_t;
struct jit_elf_info
{
	int		machine;
	int		abi;
	int		abi_version;

};

/*
 * External function defintions.
 */
void _jit_init_backend(void);
void _jit_gen_get_elf_info(jit_elf_info_t *info);
int _jit_create_entry_insns(jit_function_t func);
int _jit_create_call_setup_insns
	(jit_function_t func, jit_type_t signature,
	 jit_value_t *args, unsigned int num_args,
	 int is_nested, int nesting_level, jit_value_t *struct_return);
int _jit_setup_indirect_pointer(jit_function_t func, jit_value_t value);
int _jit_create_call_return_insns
	(jit_function_t func, jit_type_t signature,
	 jit_value_t *args, unsigned int num_args,
	 jit_value_t return_value, int is_nested);
int _jit_opcode_is_supported(int opcode);
void *_jit_gen_prolog(jit_gencode_t gen, jit_function_t func, void *buf);
void _jit_gen_epilog(jit_gencode_t gen, jit_function_t func);
void *_jit_gen_redirector(jit_gencode_t gen, jit_function_t func);
void _jit_gen_spill_reg(jit_gencode_t gen, int reg,
					    int other_reg, jit_value_t value);
void _jit_gen_free_reg(jit_gencode_t gen, int reg,
					   int other_reg, int value_used);
void _jit_gen_load_value
	(jit_gencode_t gen, int reg, int other_reg, jit_value_t value);
void _jit_gen_fix_value(jit_value_t value);
void _jit_gen_insn(jit_gencode_t gen, jit_function_t func,
				   jit_block_t block, jit_insn_t insn);
void _jit_gen_start_block(jit_gencode_t gen, jit_block_t block);
void _jit_gen_end_block(jit_gencode_t gen, jit_block_t block);
void _jit_gen_call_finally
	(jit_gencode_t gen, jit_function_t func, jit_label_t label);
void _jit_gen_unwind_stack(void *stacktop, void *catch_pc, void *object);

/*
 * Determine the byte number within a "jit_int" where the low
 * order byte can be found.
 */
int _jit_int_lowest_byte(void);

/*
 * Determine the byte number within a "jit_int" where the low
 * order short can be found.
 */
int _jit_int_lowest_short(void);

/*
 * Determine the byte number within a "jit_nint" where the low
 * order byte can be found.
 */
int _jit_nint_lowest_byte(void);

/*
 * Determine the byte number within a "jit_nint" where the low
 * order short can be found.
 */
int _jit_nint_lowest_short(void);

/*
 * Determine the byte number within a "jit_nint" where the low
 * order int can be found.
 */
int _jit_nint_lowest_int(void);

#ifdef	__cplusplus
};
#endif

#endif	/* _JIT_RULES_H */