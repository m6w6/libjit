/*
 * jit-reg-alloc.c - Register allocation routines for the JIT.
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
#include "jit-reg-alloc.h"
#include <jit/jit-dump.h>
#include <stdio.h>

/*@

The @code{libjit} library provides a number of functions for
performing register allocation within basic blocks so that you
mostly don't have to worry about it:

@*/

/*@
 * @deftypefun void _jit_regs_init_for_block (jit_gencode_t gen)
 * Initialize the register allocation state for a new block.
 * @end deftypefun
@*/
void _jit_regs_init_for_block(jit_gencode_t gen)
{
	int reg;
	gen->current_age = 1;
	for(reg = 0; reg < JIT_NUM_REGS; ++reg)
	{
		/* Clear everything except permanent and fixed registers */
		if(!jit_reg_is_used(gen->permanent, reg) &&
		   (_jit_reg_info[reg].flags & JIT_REG_FIXED) == 0)
		{
			gen->contents[reg].num_values = 0;
			gen->contents[reg].is_long_start = 0;
			gen->contents[reg].is_long_end = 0;
			gen->contents[reg].age = 0;
			gen->contents[reg].remap = -1;
			gen->contents[reg].used_for_temp = 0;
		}
		gen->stack_map[reg] = -1;
	}
}

/*@
 * @deftypefun int _jit_regs_needs_long_pair (jit_type_t type)
 * Determine if a type requires a long register pair.
 * @end deftypefun
@*/
int _jit_regs_needs_long_pair(jit_type_t type)
{
#if defined(JIT_NATIVE_INT32) && !defined(JIT_BACKEND_INTERP)
	type = jit_type_normalize(type);
	if(type)
	{
		if(type->kind == JIT_TYPE_LONG || type->kind == JIT_TYPE_ULONG)
		{
			return 1;
		}
	}
	return 0;
#else
	/* We don't register pairs on 64-bit platforms or the interpreter */
	return 0;
#endif
}

/*@
 * @deftypefun int _jit_regs_get_cpu (jit_gencode_t gen, int reg, int *other_reg)
 * Get the CPU register that corresponds to a pseudo register.
 * "other_reg" will be set to the other register in a pair,
 * or -1 if the register is not part of a pair.
 * @end deftypefun
@*/
int _jit_regs_get_cpu(jit_gencode_t gen, int reg, int *other_reg)
{
	int cpu_reg, other;
	cpu_reg = gen->contents[reg].remap;
	if(cpu_reg == -1)
	{
		cpu_reg = _jit_reg_info[reg].cpu_reg;
	}
	else
	{
		cpu_reg = _jit_reg_info[cpu_reg].cpu_reg;
	}
	if(gen->contents[reg].is_long_start)
	{
		other = _jit_reg_info[reg].other_reg;
		if(gen->contents[other].remap == -1)
		{
			other = _jit_reg_info[other].cpu_reg;
		}
		else
		{
			other = _jit_reg_info[gen->contents[other].remap].cpu_reg;
		}
	}
	else
	{
		other = -1;
	}
	if(other_reg)
	{
		*other_reg = other;
	}
	return cpu_reg;
}

/*
 * Dump debug information about the register allocation state.
 */
/*#define	JIT_REG_DEBUG	1*/
#ifdef JIT_REG_DEBUG
static void dump_regs(jit_gencode_t gen, const char *name)
{
	int reg;
	unsigned int index;
	printf("%s:\n", name);
	for(reg = 0; reg < JIT_NUM_REGS; ++reg)
	{
		if(gen->contents[reg].num_values == 0 &&
		   !(gen->contents[reg].used_for_temp) &&
		   gen->contents[reg].remap == -1)
		{
			continue;
		}
		printf("\t%s: ", _jit_reg_info[reg].name);
		if(gen->contents[reg].num_values > 0)
		{
			for(index = 0; index < gen->contents[reg].num_values; ++index)
			{
				if(index)
					fputs(", ", stdout);
				jit_dump_value(stdout, jit_value_get_function
										(gen->contents[reg].values[index]),
							   gen->contents[reg].values[index], 0);
			}
			if(gen->contents[reg].used_for_temp)
			{
				printf(", used_for_temp");
			}
		}
		else if(gen->contents[reg].used_for_temp)
		{
			printf("used_for_temp");
		}
		else
		{
			printf("free");
		}
		if(gen->contents[reg].remap != -1)
		{
			printf(", remap=%d", (int)(gen->contents[reg].remap));
		}
		for(index = 0; index < JIT_NUM_REGS; ++index)
		{
			if(gen->stack_map[index] == reg)
			{
				printf(", reverse_remap=%d", (int)index);
			}
		}
		putc('\n', stdout);
	}
}
#endif

/*
 * Spill all registers between two end points.
 */
static void spill_all_between(jit_gencode_t gen, int first, int last)
{
	int reg, posn, other_reg, real_reg;
	int first_stack_reg = 0;
	jit_value_t value;
	int value_used;

#ifdef JIT_REG_DEBUG
	dump_regs(gen, "enter spill_all_between");
#endif

	/* Handle the non-stack registers first, as they are easy to spill */
	for(reg = first; reg <= last; ++reg)
	{
		/* Skip this register if it is permanent or fixed */
		if(jit_reg_is_used(gen->permanent, reg) ||
		   (_jit_reg_info[reg].flags & JIT_REG_FIXED) != 0)
		{
			continue;
		}

		/* Remember this register if it is the start of a stack */
		if((_jit_reg_info[reg].flags & JIT_REG_START_STACK) != 0)
		{
			first_stack_reg = reg;
		}

		/* Skip this register if there is nothing in it */
		if(gen->contents[reg].num_values == 0 &&
		   !(gen->contents[reg].used_for_temp))
		{
			continue;
		}

		/* If this is a stack register, then we need to find the
		   register that contains the top-most stack position,
		   because we must spill stack registers from top down.
		   As we spill each one, something else will become the top */
		if((_jit_reg_info[reg].flags & JIT_REG_IN_STACK) != 0)
		{
			real_reg = gen->stack_map[first_stack_reg];
		}
		else
		{
			real_reg = reg;
		}

		/* Get the other register in a long pair, if there is one */
		if(gen->contents[real_reg].is_long_start)
		{
			other_reg = _jit_reg_info[real_reg].other_reg;
		}
		else
		{
			other_reg = -1;
		}

		/* Spill all values that are associated with the register */
		value_used = 0;
		if(gen->contents[real_reg].num_values > 0)
		{
			for(posn = gen->contents[real_reg].num_values - 1;
				posn >= 0; --posn)
			{
				value = gen->contents[real_reg].values[posn];
				if(!(value->in_frame))
				{
					if((_jit_reg_info[reg].flags & JIT_REG_IN_STACK) == 0)
					{
						_jit_gen_spill_reg(gen, real_reg, other_reg, value);
					}
					else
					{
						/* The back end needs to think that we are spilling
						   the first register in the stack, regardless of
						   what "real_reg" might happen to be */
						_jit_gen_spill_reg(gen, first_stack_reg, -1, value);
					}
					value->in_frame = 1;
					value_used = 1;
				}
				value->in_register = 0;
				value->reg = -1;
			}
		}

		/* Free the register */
		_jit_regs_free_reg(gen, real_reg, value_used);
	}

#ifdef JIT_REG_DEBUG
	dump_regs(gen, "leave spill_all_between");
#endif
}

/*
 * Spill a specific register.  If it is in a stack, then all registers
 * above the specific register must also be spilled.
 */
static void spill_register(jit_gencode_t gen, int reg)
{
	if((_jit_reg_info[reg].flags & JIT_REG_IN_STACK) == 0)
	{
		return spill_all_between(gen, reg, reg);
	}
	else
	{
		int first_reg;
		reg = gen->contents[reg].remap;
		first_reg = reg;
		while((_jit_reg_info[first_reg].flags & JIT_REG_START_STACK) == 0)
		{
			--first_reg;
		}
		spill_all_between(gen, first_reg, reg);
	}
}

/*
 * Spill all stack registers of a specific type.
 */
static void spill_all_stack(jit_gencode_t gen, int reg)
{
	int first_reg;
	while((_jit_reg_info[reg].flags & JIT_REG_START_STACK) == 0)
	{
		--reg;
	}
	first_reg = reg;
	while((_jit_reg_info[reg].flags & JIT_REG_END_STACK) == 0)
	{
		++reg;
	}
	spill_all_between(gen, first_reg, reg);
}

/*@
 * @deftypefun void _jit_regs_spill_all (jit_gencode_t gen)
 * Spill all of the temporary registers to memory locations.
 * Normally used at the end of a block, but may also be used in
 * situations where a value must be in a certain register and
 * it is too hard to swap things around to put it there.
 * @end deftypefun
@*/
void _jit_regs_spill_all(jit_gencode_t gen)
{
	spill_all_between(gen, 0, JIT_NUM_REGS - 1);
}

/*
 * Free a register within a stack, and renumber the other stack registers
 * to compensate for the change.
 */
static void free_stack_reg(jit_gencode_t gen, int reg)
{
	int remap;

#ifdef JIT_REG_DEBUG
	dump_regs(gen, "enter free_stack_reg");
#endif

	/* Shift everything after this register up by one position */
	remap = gen->contents[reg].remap;
	if((_jit_reg_info[remap].flags & JIT_REG_END_STACK) == 0)
	{
		++remap;
		for(;;)
		{
			if(gen->stack_map[remap] == -1)
			{
				/* There are no more active values in this stack */
				gen->stack_map[remap - 1] = -1;
				break;
			}
			else if((_jit_reg_info[remap].flags & JIT_REG_END_STACK) != 0)
			{
				/* This is the last register in the stack */
				--(gen->contents[gen->stack_map[remap]].remap);
				gen->stack_map[remap - 1] = gen->stack_map[remap];
				gen->stack_map[remap] = -1;
				break;
			}
			else
			{
				/* Shift this stack entry up by one */
				--(gen->contents[gen->stack_map[remap]].remap);
				gen->stack_map[remap - 1] = gen->stack_map[remap];
				++remap;
			}
		}
	}

	/* Clear the remapping for the register */
	gen->contents[reg].remap = -1;

#ifdef JIT_REG_DEBUG
	dump_regs(gen, "leave free_stack_reg");
#endif
}

/*
 * Make space for a new stack register in a particular stack.
 * Returns the pseudo register number of the newly allocated register.
 */
static int create_stack_reg(jit_gencode_t gen, int reg, int roll_down)
{
	int first_stack_reg;
	int temp_reg, remap;

#ifdef JIT_REG_DEBUG
	dump_regs(gen, "enter create_stack_reg");
#endif

	/* Find the first pseudo register in the stack */
	while((_jit_reg_info[reg].flags & JIT_REG_START_STACK) == 0)
	{
		--reg;
	}
	first_stack_reg = reg;

	/* Find a free pseudo register in the stack */
	for(;;)
	{
		if(gen->contents[reg].num_values == 0 &&
		   !(gen->contents[reg].used_for_temp))
		{
			break;
		}
		if((_jit_reg_info[reg].flags & JIT_REG_END_STACK) != 0)
		{
			/* None of the registers are free, so we have to spill them all */
			spill_all_between(gen, first_stack_reg, reg);
			reg = first_stack_reg;
			break;
		}
		++reg;
	}

	/* Roll the stack remappings down to make room at the top */
	if(roll_down)
	{
		temp_reg = first_stack_reg - 1;
		do
		{
			++temp_reg;
			remap = gen->contents[temp_reg].remap;
			if(remap != -1)
			{
				/* Change the register's position in the stack */
				gen->contents[temp_reg].remap = remap + 1;
				gen->stack_map[remap + 1] = temp_reg;

				/* Mark the rolled-down register position as touched */
				jit_reg_set_used(gen->touched, remap + 1);
			}
		}
		while((_jit_reg_info[temp_reg].flags & JIT_REG_END_STACK) == 0);
		gen->contents[reg].remap = first_stack_reg;
		gen->stack_map[first_stack_reg] = reg;
	}

	/* Mark the register as touched, in case it needs to be saved */
	jit_reg_set_used(gen->touched, reg);

#ifdef JIT_REG_DEBUG
	dump_regs(gen, "leave create_stack_reg");
#endif

	/* Return the free register to the caller */
	return reg;
}

/*
 * Free a register, and optionally spill its value.
 */
static void free_reg_and_spill
	(jit_gencode_t gen, int reg, int value_used, int spill)
{
	int other_reg, posn;
	jit_value_t value;

#ifdef JIT_REG_DEBUG
	dump_regs(gen, "enter free_reg_and_spill");
#endif

	/* Find the other register in a long pair */
	if(gen->contents[reg].is_long_start)
	{
		other_reg = _jit_reg_info[reg].other_reg;
		gen->contents[reg].is_long_start = 0;
		gen->contents[other_reg].is_long_end = 0;
	}
	else if(gen->contents[reg].is_long_end)
	{
		gen->contents[reg].is_long_end = 0;
		other_reg = reg;
		for(reg = 0; reg < JIT_NUM_REGS; ++reg)
		{
			if(other_reg == _jit_reg_info[reg].other_reg)
			{
				gen->contents[reg].is_long_start = 0;
				break;
			}
		}
	}
	else
	{
		other_reg = -1;
	}

	/* Spill the register's contents to the local variable frame */
	if(spill && gen->contents[reg].num_values > 0)
	{
		for(posn = gen->contents[reg].num_values - 1; posn >= 0; --posn)
		{
			value = gen->contents[reg].values[posn];
			if(!(value->in_frame))
			{
				if((_jit_reg_info[reg].flags & JIT_REG_IN_STACK) == 0)
				{
					_jit_gen_spill_reg(gen, reg, other_reg, value);
				}
				else
				{
					_jit_gen_spill_reg
						(gen, gen->contents[reg].remap, -1, value);
				}
				value->in_frame = 1;
				value_used = 1;
			}
			value->in_register = 0;
			value->reg = -1;
		}
	}

	/* The registers do not contain values any more */
	gen->contents[reg].num_values = 0;
	gen->contents[reg].used_for_temp = 0;
	if(other_reg != -1)
	{
		gen->contents[other_reg].num_values = 0;
		gen->contents[other_reg].used_for_temp = 0;
	}

	/* If the registers are members of a stack, then readjust the
	   stack mappings to compensate for the change */
	if(gen->contents[reg].remap != -1)
	{
		free_stack_reg(gen, reg);
	}
	if(other_reg != -1 && gen->contents[other_reg].remap != -1)
	{
		free_stack_reg(gen, other_reg);
	}

#ifdef JIT_REG_DEBUG
	dump_regs(gen, "leave free_reg_and_spill");
#endif

	/* Free the register using CPU-specific code */
	_jit_gen_free_reg(gen, reg, other_reg, value_used);
}

/*@
 * @deftypefun void _jit_regs_want_reg (jit_gencode_t gen, int reg)
 * Tell the register allocator that we want a particular register
 * for a specific purpose.  The current contents of the register
 * are spilled.  If @code{reg} is part of a register pair, then the
 * other register in the pair will also be spilled.  If @code{reg}
 * is a stack register, then it should be the first one.
 *
 * This is typically used for x86 instructions that require operands
 * to be in certain registers (especially multiplication and division),
 * and we want to make sure that the register is free before we clobber it.
 * It is also used to make space in the x86 FPU for floating-point returns.
 *
 * This may return a different pseudo register number if @code{reg}
 * was a member of a stack and some other register was made free.
 * @end deftypefun
@*/
int _jit_regs_want_reg(jit_gencode_t gen, int reg, int for_long)
{
	int other_reg;
	if((_jit_reg_info[reg].flags & JIT_REG_IN_STACK) == 0)
	{
		/* Spill an ordinary register and its pair register */
		free_reg_and_spill(gen, reg, 0, 1);
		if(for_long)
		{
			other_reg = _jit_reg_info[reg].other_reg;
			if(other_reg != -1)
			{
				free_reg_and_spill(gen, other_reg, 0, 1);
			}
		}
		else
		{
			other_reg = -1;
		}

		/* Mark the register as touched and return it */
		jit_reg_set_used(gen->touched, reg);
		if(other_reg != -1)
		{
			jit_reg_set_used(gen->touched, other_reg);
		}
		return reg;
	}
	else
	{
		/* If we want a stack register, all we have to do is roll
		   everything down to make room for the new value.  If the
		   stack is full, then we spill the entire stack */
		return create_stack_reg(gen, reg, 0);
	}
}

/*@
 * @deftypefun void _jit_regs_free_reg (jit_gencode_t gen, int reg, int value_used)
 * Free the contents of a pseudo register, without spilling.  Used when
 * the contents of a register becomes invalid.  If @code{value_used}
 * is non-zero, then it indicates that the value has already been
 * used.  On some systems, an explicit instruction is needed to free
 * a register whose value hasn't been used yet (e.g. x86 floating point
 * stack registers).
 * @end deftypefun
@*/
void _jit_regs_free_reg(jit_gencode_t gen, int reg, int value_used)
{
	free_reg_and_spill(gen, reg, value_used, 0);
}

/*@
 * @deftypefun void _jit_regs_set_value (jit_gencode_t gen, int reg, jit_value_t value, int still_in_frame)
 * Set pseudo register @code{reg} to record that it currently holds the
 * contents of @code{value}.  The value is assumed to already be in
 * the register and no spill occurs.  If @code{still_in_frame} is
 * non-zero, then the value is still in the stack frame; otherwise the
 * value is exclusively in the register.
 * @end deftypefun
@*/
void _jit_regs_set_value
	(jit_gencode_t gen, int reg, jit_value_t value, int still_in_frame)
{
	int other_reg;
	int first_stack_reg;

#ifdef JIT_REG_DEBUG
	dump_regs(gen, "enter set_value");
#endif

	/* Get the other register in a pair */
	if(_jit_regs_needs_long_pair(value->type))
	{
		other_reg = _jit_reg_info[reg].other_reg;
	}
	else
	{
		other_reg = -1;
	}

	/* Mark the register as touched */
	jit_reg_set_used(gen->touched, reg);
	if(other_reg != -1)
	{
		jit_reg_set_used(gen->touched, other_reg);
	}

	/* Adjust the allocation state to reflect that "reg" contains "value" */
	gen->contents[reg].values[0] = value;
	gen->contents[reg].num_values = 1;
	gen->contents[reg].age = gen->current_age;
	if(other_reg == -1)
	{
		gen->contents[reg].is_long_start = 0;
		gen->contents[reg].is_long_end = 0;
		gen->contents[reg].used_for_temp = 0;
	}
	else
	{
		gen->contents[reg].is_long_start = 1;
		gen->contents[reg].is_long_end = 0;
		gen->contents[reg].used_for_temp = 0;
		gen->contents[other_reg].num_values = 0;
		gen->contents[other_reg].is_long_start = 0;
		gen->contents[other_reg].is_long_end = 1;
		gen->contents[other_reg].age = gen->current_age;
		gen->contents[other_reg].used_for_temp = 0;
	}
	(gen->current_age)++;

	/* Set the stack mappings for this register */
	if((_jit_reg_info[reg].flags & JIT_REG_IN_STACK) != 0)
	{
		first_stack_reg = reg;
		while((_jit_reg_info[first_stack_reg].flags & JIT_REG_START_STACK) == 0)
		{
			--first_stack_reg;
		}
		gen->contents[reg].remap = first_stack_reg;
		gen->stack_map[first_stack_reg] = reg;
	}

#ifdef JIT_REG_DEBUG
	dump_regs(gen, "leave set_value");
#endif

	/* Adjust the value to reflect that it is in "reg", and maybe the frame */
	value->in_register = 1;
	value->in_frame = still_in_frame;
	value->reg = (short)reg;
}

/*@
 * @deftypefun void _jit_regs_set_incoming (jit_gencode_t gen, int reg, jit_value_t value)
 * Set pseudo register @code{reg} to record that it currently holds the
 * contents of @code{value}.  If the register was previously in use,
 * then spill its value first.
 * @end deftypefun
@*/
void _jit_regs_set_incoming(jit_gencode_t gen, int reg, jit_value_t value)
{
	/* Eject any values that are currently in the register */
	reg = _jit_regs_want_reg(gen, reg, _jit_regs_needs_long_pair(value->type));

	/* Record that the value is in "reg", but not in the frame */
	_jit_regs_set_value(gen, reg, value, 0);
}

/*@
 * @deftypefun void _jit_regs_set_outgoing (jit_gencode_t gen, int reg, jit_value_t value)
 * Load the contents of @code{value} into pseudo register @code{reg},
 * spilling out the current contents.  This is used to set up outgoing
 * parameters for a function call.
 * @end deftypefun
@*/
void _jit_regs_set_outgoing(jit_gencode_t gen, int reg, jit_value_t value)
{
	/* TODO */
}

/*@
 * @deftypefun int _jit_regs_is_top (jit_gencode_t gen, jit_value_t value)
 * Determine if @code{value} is currently the in top-most position
 * in the appropriate register stack.  Always returns non-zero if
 * @code{value} is in a register, but that register is not part of a
 * register stack.  This is used to check if an operand value is
 * already in the right position for a unary operation.
 * @end deftypefun
@*/
int _jit_regs_is_top(jit_gencode_t gen, jit_value_t value)
{
	int reg, remap;
	if(!(value->in_register))
	{
		return 0;
	}
	reg = value->reg;
	if((_jit_reg_info[reg].flags & JIT_REG_IN_STACK) == 0)
	{
		return 1;
	}
	remap = gen->contents[reg].remap;
	if(remap != -1 && (_jit_reg_info[remap].flags & JIT_REG_START_STACK) != 0)
	{
		return 1;
	}
	return 0;
}

/*@
 * @deftypefun int _jit_regs_is_top_two (jit_gencode_t gen, jit_value_t value1, jit_value_t value2)
 * Determine if @code{value1} and @code{value2} are in the top two positions
 * in the appropriate register stack, and @code{value2} is above
 * @code{value1}.  Always returns non-zero if @code{value} and
 * @code{value2} are in registers, but those registers are not part
 * of a register stack.  This is used to check if the operand values
 * for a binary operation are already in the right positions.
 * @end deftypefun
@*/
int _jit_regs_is_top_two
	(jit_gencode_t gen, jit_value_t value1, jit_value_t value2)
{
	int reg, remap;
	if(!(value1->in_register) || !(value2->in_register))
	{
		return 0;
	}
	reg = value2->reg;
	if((_jit_reg_info[reg].flags & JIT_REG_IN_STACK) == 0)
	{
		reg = value1->reg;
		return ((_jit_reg_info[reg].flags & JIT_REG_IN_STACK) == 0);
	}
	remap = gen->contents[reg].remap;
	if(remap == -1 || (_jit_reg_info[remap].flags & JIT_REG_START_STACK) == 0)
	{
		return 0;
	}
	reg = value1->reg;
	if((_jit_reg_info[reg].flags & JIT_REG_IN_STACK) == 0)
	{
		return 1;
	}
	return (gen->contents[reg].remap == (remap + 1));
}

/*
 * Load a value into a register.
 */
static void load_value(jit_gencode_t gen, int reg, int other_reg,
					   jit_value_t value, int destroy)
{
	_jit_gen_load_value(gen, reg, other_reg, value);
	if(destroy || value->is_constant)
	{
		/* Mark the register as containing a temporary value */
		gen->contents[reg].used_for_temp = 1;
		jit_reg_set_used(gen->touched, reg);
		if(other_reg != -1)
		{
			gen->contents[reg].is_long_start = 1;
			gen->contents[other_reg].is_long_end = 1;
			gen->contents[other_reg].used_for_temp = 1;
			jit_reg_set_used(gen->touched, other_reg);
		}
	}
	else
	{
		/* Mark the register as containing the value we have loaded */
		_jit_regs_set_value(gen, reg, value, value->in_frame);
	}
}

/*@
 * @deftypefun int _jit_regs_load_value (jit_gencode_t gen, jit_value_t value, int destroy, int used_again)
 * Load a value into any register that is suitable and return that register.
 * If the value needs a long pair, then this will return the first register
 * in the pair.  Returns -1 if the value will not fit into any register.
 *
 * If @code{destroy} is non-zero, then we are about to destroy the register,
 * so the system must make sure that such destruction will not side-effect
 * @code{value} or any of the other values currently in that register.
 *
 * If @code{used_again} is non-zero, then it indicates that the value is
 * used again further down the block.
 * @end deftypefun
@*/
int _jit_regs_load_value
	(jit_gencode_t gen, jit_value_t value, int destroy, int used_again)
{
	int reg, other_reg, type;
	int suitable_reg, need_pair;
	int suitable_age;

	/* Determine if we need a long pair for this value */
	need_pair = _jit_regs_needs_long_pair(value->type);

	/* If the value is already in a register, then try to use that register */
	if(value->in_register)
	{
		reg = value->reg;
		if(destroy)
		{
			if(gen->contents[reg].num_values == 1 &&
			   (value->in_frame || !used_again))
			{
				/* We are the only value in this register, and the
				   value is duplicated in the frame, or will never
				   be used again in this block.  In this case,
				   we can disassociate the register from the value
				   and just return the register as-is */
				value->in_register = 0;
				gen->contents[reg].num_values = 0;
				gen->contents[reg].used_for_temp = 1;
				gen->contents[reg].age = gen->current_age;
				if(need_pair)
				{
					other_reg = _jit_reg_info[reg].other_reg;
					gen->contents[other_reg].used_for_temp = 1;
					gen->contents[other_reg].age = gen->current_age;
				}
				++(gen->current_age);
				return reg;
			}
			else
			{
				/* We need to spill the register and then reload it */
				spill_register(gen, reg);
			}
		}
		else
		{
			gen->contents[reg].age = gen->current_age;
			if(need_pair)
			{
				other_reg = _jit_reg_info[reg].other_reg;
				gen->contents[other_reg].age = gen->current_age;
			}
			++(gen->current_age);
			return reg;
		}
	}

	/* Determine the type of register that we need */
	switch(jit_type_normalize(value->type)->kind)
	{
		case JIT_TYPE_SBYTE:
		case JIT_TYPE_UBYTE:
		case JIT_TYPE_SHORT:
		case JIT_TYPE_USHORT:
		case JIT_TYPE_INT:
		case JIT_TYPE_UINT:
		case JIT_TYPE_NINT:
		case JIT_TYPE_NUINT:
		case JIT_TYPE_SIGNATURE:
		case JIT_TYPE_PTR:
		{
			type = JIT_REG_WORD;
		}
		break;

		case JIT_TYPE_LONG:
		case JIT_TYPE_ULONG:
		{
			if(need_pair)
				type = JIT_REG_LONG;
			else
				type = JIT_REG_WORD;
		}
		break;

		case JIT_TYPE_FLOAT32:
		case JIT_TYPE_FLOAT64:
		case JIT_TYPE_NFLOAT:
		{
			type = JIT_REG_FLOAT;
		}
		break;

		default: return -1;
	}

	/* Search for a free register, ignoring permanent global allocations.
	   We also keep track of the oldest suitable register that is not free */
	suitable_reg = -1;
	suitable_age = -1;
	for(reg = 0; reg < JIT_NUM_REGS; ++reg)
	{
		if((_jit_reg_info[reg].flags & type) != 0 &&
		   !jit_reg_is_used(gen->permanent, reg))
		{
			if((_jit_reg_info[reg].flags & JIT_REG_IN_STACK) != 0)
			{
				/* We always load stack values to the top of the stack */
				reg = create_stack_reg(gen, reg, 1);
				load_value(gen, reg, -1, value, destroy);
				return reg;
			}
			else if(!need_pair)
			{
				if(gen->contents[reg].num_values == 0 &&
				   !(gen->contents[reg].used_for_temp))
				{
					load_value(gen, reg, -1, value, destroy);
					return reg;
				}
			}
			else
			{
				other_reg = _jit_reg_info[reg].other_reg;
				if(gen->contents[reg].num_values == 0 &&
				   !(gen->contents[reg].used_for_temp) &&
				   gen->contents[other_reg].num_values == 0 &&
				   !(gen->contents[other_reg].used_for_temp))
				{
					load_value(gen, reg, other_reg, value, destroy);
					return reg;
				}
			}
			if(suitable_reg == -1 || gen->contents[reg].age < suitable_age)
			{
				/* This is the oldest suitable register of this type */
				suitable_reg = reg;
				suitable_age = gen->contents[reg].age;
			}
		}
	}

	/* If there were no suitable registers at all, then fail */
	if(suitable_reg == -1)
	{
		return -1;
	}

	/* Eject the current contents of the register */
	reg = _jit_regs_want_reg(gen, reg, need_pair);

	/* Load the value into the register */
	if(!need_pair)
	{
		load_value(gen, reg, -1, value, destroy);
	}
	else
	{
		load_value(gen, reg, _jit_reg_info[reg].other_reg, value, destroy);
	}
	return reg;
}

/*
 * Determine if "num" stack registers are free in a specific stack.
 */
static int stack_regs_free(jit_gencode_t gen, int reg, int num)
{
	int first_reg;

	/* Find the extents of the stack */
	while((_jit_reg_info[reg].flags & JIT_REG_START_STACK) == 0)
	{
		--reg;
	}
	first_reg = reg;
	while((_jit_reg_info[reg].flags & JIT_REG_END_STACK) == 0)
	{
		++reg;
	}

	/* Search for free registers */
	while(reg >= first_reg)
	{
		if(gen->contents[reg].num_values == 0 &&
		   !(gen->contents[reg].used_for_temp))
		{
			--num;
			if(num <= 0)
			{
				return 1;
			}
		}
		--reg;
	}
	return 0;
}

/*@
 * @deftypefun int _jit_regs_load_to_top (jit_gencode_t gen, jit_value_t value, int used_again, int type_reg)
 * Load the contents of @code{value} into a register that is guaranteed to
 * be at the top of its stack.  This is the preferred way to set up for a
 * unary operation on a stack-based architecture.  Returns the pseudo
 * register that contains the value.
 *
 * When @code{value} is loaded, the "destroy" flag is set so that the
 * unary operation will not affect the original contents of @code{value}.
 * The @code{used_again} flag indicates if @code{value} is used again
 * in the current basic block.
 *
 * The @code{type_reg} parameter should be set to the pseudo register
 * number of a suitable register.  This is used to determine which
 * register stack to use for the allocation.
 * @end deftypefun
@*/
int _jit_regs_load_to_top(jit_gencode_t gen, jit_value_t value, int used_again, int type_reg)
{
	int reg;

	/* Determine if the value is already in the top-most register */
	if(value->in_register)
	{
		reg = value->reg;
		if((_jit_reg_info[gen->contents[reg].remap].flags
				& JIT_REG_START_STACK) != 0)
		{
			if(value->in_frame || !used_again)
			{
				/* Disassociate the value from the register and return */
				value->in_register = 0;
				gen->contents[reg].num_values = 0;
				gen->contents[reg].used_for_temp = 1;
				gen->contents[reg].age = gen->current_age;
				++(gen->current_age);
				return reg;
			}
		}
		spill_all_stack(gen, type_reg);
	}

	/* If there are free registers of this type, then load the value now */
	if(stack_regs_free(gen, type_reg, 1))
	{
		return _jit_regs_load_value(gen, value, 1, used_again);
	}

	/* Spill the entire stack contents, to get things into a known state */
	spill_all_stack(gen, type_reg);

	/* Reload the value and return */
	return _jit_regs_load_value(gen, value, 1, used_again);
}

/*@
 * @deftypefun int _jit_regs_load_to_top_two (jit_gencode_t gen, jit_value_t value, jit_value_t value2, int used_again1, int used_again2, int type_reg)
 * Load the contents of @code{value} and @code{value2} into registers that
 * are guaranteed to be at the top of the relevant register stack.
 * This is the preferred way to set up for a binary operation on a
 * stack-based architecture.
 *
 * Returns the pseudo register that contains @code{value}.  The pseudo
 * register that contains @code{value2} is marked as free, because it is
 * assumed that the binary operation will immediately consume its value.
 *
 * When @code{value} are @code{value2} are loaded, the "destroy" flag is
 * set so that the binary operation will not affect their original contents.
 * The @code{used_again1} and @code{used_again2} flags indicate if
 * @code{value} and @code{value2} are used again in the current basic block.
 *
 * The @code{type_reg} parameter should be set to the pseudo register
 * number of a suitable register.  This is used to determine which
 * register stack to use for the allocation.
 * @end deftypefun
@*/
int _jit_regs_load_to_top_two
	(jit_gencode_t gen, jit_value_t value, jit_value_t value2,
	 int used_again1, int used_again2, int type_reg)
{
	int reg, reg2;

	/* Determine if the values are already in the top two registers */
	if(value->in_register && value2->in_register)
	{
		reg = value->reg;
		reg2 = value->reg;
		if((_jit_reg_info[gen->contents[reg2].remap].flags
				& JIT_REG_START_STACK) != 0 &&
		   gen->contents[reg].remap == (gen->contents[reg2].remap + 1))
		{
			if((value->in_frame || !used_again1) &&
			   (value2->in_frame || !used_again2))
			{
				/* Disassociate the values from the registers and return */
				free_stack_reg(gen, reg2);
				value->in_register = 0;
				value2->in_register = 0;
				gen->contents[reg].num_values = 0;
				gen->contents[reg].used_for_temp = 1;
				gen->contents[reg].age = gen->current_age;
				gen->contents[reg2].num_values = 0;
				gen->contents[reg2].used_for_temp = 0;
				gen->contents[reg2].age = gen->current_age;
				++(gen->current_age);
				return reg;
			}
		}
		spill_all_stack(gen, type_reg);
	}

	/* If there are free registers of this type, then load the values now */
	if(stack_regs_free(gen, type_reg, 2))
	{
		reg = _jit_regs_load_value(gen, value, 1, used_again1);
		reg2 = _jit_regs_load_value(gen, value2, 1, used_again2);
		free_stack_reg(gen, reg2);
		gen->contents[reg2].used_for_temp = 0;
		return reg;
	}

	/* Spill the entire stack contents, to get things into a known state */
	spill_all_stack(gen, type_reg);

	/* Reload the values and return */
	reg = _jit_regs_load_value(gen, value, 1, used_again1);
	reg2 = _jit_regs_load_value(gen, value2, 1, used_again2);
	free_stack_reg(gen, reg2);
	gen->contents[reg2].used_for_temp = 0;
	return reg;
}

/*@
 * @deftypefun int _jit_regs_num_used (jit_gencode_t gen, int type_reg)
 * Get the number of stack registers in use within the register stack
 * indicated by @code{type_reg}.
 * @end deftypefun
@*/
int _jit_regs_num_used(jit_gencode_t gen, int type_reg)
{
	int count;
	while((_jit_reg_info[type_reg].flags & JIT_REG_START_STACK) == 0)
	{
		--type_reg;
	}
	count = 0;
	for(;;)
	{
		if(gen->contents[type_reg].num_values > 0 ||
		   gen->contents[type_reg].used_for_temp)
		{
			++count;
		}
		if((_jit_reg_info[type_reg].flags & JIT_REG_END_STACK) == 0)
		{
			break;
		}
		++type_reg;
	}
	return count;
}