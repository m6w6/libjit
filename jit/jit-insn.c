/*
 * jit-insn.c - Functions for manipulating instructions.
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
#include "jit-rules.h"
#include <config.h>
#if HAVE_ALLOCA_H
	#include <alloca.h>
#endif
#ifdef JIT_WIN32_PLATFORM
	#include <malloc.h>
	#ifndef alloca
		#define	alloca	_alloca
	#endif
#endif

/*@

@cindex jit-insn.h

@*/

/*
 * Opcode description blocks.  These describe the alternative opcodes
 * and intrinsic functions to use for various kinds of arguments.
 */
typedef struct
{
	unsigned short	ioper;					/* Primary operator for "int" */
	unsigned short	iuoper;					/* Primary operator for "uint" */
	unsigned short	loper;					/* Primary operator for "long" */
	unsigned short	luoper;					/* Primary operator for "ulong" */
	unsigned short	foper;					/* Primary operator for "float32" */
	unsigned short	doper;					/* Primary operator for "float64" */
	unsigned short	nfoper;					/* Primary operator for "nfloat" */
	void		   *ifunc;					/* Function for "int" */
	const char	   *iname;					/* Intrinsic name for "int" */
	const jit_intrinsic_descr_t *idesc;		/* Descriptor for "int" */
	void		   *iufunc;					/* Function for "uint" */
	const char	   *iuname;					/* Intrinsic name for "uint" */
	const jit_intrinsic_descr_t *iudesc; 	/* Descriptor for "uint" */
	void		   *lfunc;					/* Function for "long" */
	const char	   *lname;					/* Intrinsic name for "long" */
	const jit_intrinsic_descr_t *ldesc;		/* Descriptor for "long" */
	void		   *lufunc;					/* Function for "ulong" */
	const char	   *luname;					/* Intrinsic name for "ulong" */
	const jit_intrinsic_descr_t *ludesc;	/* Descriptor for "ulong" */
	void		   *ffunc;					/* Function for "float32" */
	const char	   *fname;					/* Intrinsic name for "float32" */
	const jit_intrinsic_descr_t *fdesc;		/* Descriptor for "float32" */
	void		   *dfunc;					/* Function for "float64" */
	const char	   *dname;					/* Intrinsic name for "float64" */
	const jit_intrinsic_descr_t *ddesc;		/* Descriptor for "float64" */
	void		   *nffunc;					/* Function for "nfloat" */
	const char	   *nfname;					/* Intrinsic name for "nfloat" */
	const jit_intrinsic_descr_t *nfdesc;	/* Descriptor for "nfloat" */

} jit_opcode_descr;
#define	jit_intrinsic(name,descr)		(void *)name, #name, &descr
#define	jit_no_intrinsic				0, 0, 0

/*
 * Some common intrinsic descriptors that are used in this file.
 */
static jit_intrinsic_descr_t const descr_i_ii = {
	(jit_type_t)&_jit_type_int_def,
	0,
	(jit_type_t)&_jit_type_int_def,
	(jit_type_t)&_jit_type_int_def
};
static jit_intrinsic_descr_t const descr_e_pi_ii = {
	(jit_type_t)&_jit_type_int_def,
	(jit_type_t)&_jit_type_int_def,
	(jit_type_t)&_jit_type_int_def,
	(jit_type_t)&_jit_type_int_def
};
static jit_intrinsic_descr_t const descr_i_iI = {
	(jit_type_t)&_jit_type_int_def,
	0,
	(jit_type_t)&_jit_type_int_def,
	(jit_type_t)&_jit_type_uint_def
};
static jit_intrinsic_descr_t const descr_i_i = {
	(jit_type_t)&_jit_type_int_def,
	0,
	(jit_type_t)&_jit_type_int_def,
	0
};
static jit_intrinsic_descr_t const descr_I_II = {
	(jit_type_t)&_jit_type_uint_def,
	0,
	(jit_type_t)&_jit_type_uint_def,
	(jit_type_t)&_jit_type_uint_def
};
static jit_intrinsic_descr_t const descr_e_pI_II = {
	(jit_type_t)&_jit_type_uint_def,
	(jit_type_t)&_jit_type_uint_def,
	(jit_type_t)&_jit_type_uint_def,
	(jit_type_t)&_jit_type_uint_def
};
static jit_intrinsic_descr_t const descr_I_I = {
	(jit_type_t)&_jit_type_uint_def,
	0,
	(jit_type_t)&_jit_type_uint_def,
	0
};
static jit_intrinsic_descr_t const descr_i_II = {
	(jit_type_t)&_jit_type_int_def,
	0,
	(jit_type_t)&_jit_type_uint_def,
	(jit_type_t)&_jit_type_uint_def
};
static jit_intrinsic_descr_t const descr_l_ll = {
	(jit_type_t)&_jit_type_long_def,
	0,
	(jit_type_t)&_jit_type_long_def,
	(jit_type_t)&_jit_type_long_def
};
static jit_intrinsic_descr_t const descr_e_pl_ll = {
	(jit_type_t)&_jit_type_long_def,
	(jit_type_t)&_jit_type_long_def,
	(jit_type_t)&_jit_type_long_def,
	(jit_type_t)&_jit_type_long_def
};
static jit_intrinsic_descr_t const descr_l_lI = {
	(jit_type_t)&_jit_type_long_def,
	0,
	(jit_type_t)&_jit_type_long_def,
	(jit_type_t)&_jit_type_uint_def
};
static jit_intrinsic_descr_t const descr_l_l = {
	(jit_type_t)&_jit_type_long_def,
	0,
	(jit_type_t)&_jit_type_long_def,
	0
};
static jit_intrinsic_descr_t const descr_i_ll = {
	(jit_type_t)&_jit_type_int_def,
	0,
	(jit_type_t)&_jit_type_long_def,
	(jit_type_t)&_jit_type_long_def
};
static jit_intrinsic_descr_t const descr_i_l = {
	(jit_type_t)&_jit_type_int_def,
	0,
	(jit_type_t)&_jit_type_long_def,
	0
};
static jit_intrinsic_descr_t const descr_L_LL = {
	(jit_type_t)&_jit_type_ulong_def,
	0,
	(jit_type_t)&_jit_type_ulong_def,
	(jit_type_t)&_jit_type_ulong_def
};
static jit_intrinsic_descr_t const descr_e_pL_LL = {
	(jit_type_t)&_jit_type_ulong_def,
	(jit_type_t)&_jit_type_ulong_def,
	(jit_type_t)&_jit_type_ulong_def,
	(jit_type_t)&_jit_type_ulong_def
};
static jit_intrinsic_descr_t const descr_L_LI = {
	(jit_type_t)&_jit_type_ulong_def,
	0,
	(jit_type_t)&_jit_type_ulong_def,
	(jit_type_t)&_jit_type_uint_def
};
static jit_intrinsic_descr_t const descr_L_L = {
	(jit_type_t)&_jit_type_ulong_def,
	0,
	(jit_type_t)&_jit_type_ulong_def,
	0
};
static jit_intrinsic_descr_t const descr_i_LL = {
	(jit_type_t)&_jit_type_int_def,
	0,
	(jit_type_t)&_jit_type_ulong_def,
	(jit_type_t)&_jit_type_ulong_def
};
static jit_intrinsic_descr_t const descr_f_ff = {
	(jit_type_t)&_jit_type_float32_def,
	0,
	(jit_type_t)&_jit_type_float32_def,
	(jit_type_t)&_jit_type_float32_def
};
static jit_intrinsic_descr_t const descr_f_f = {
	(jit_type_t)&_jit_type_float32_def,
	0,
	(jit_type_t)&_jit_type_float32_def,
	0
};
static jit_intrinsic_descr_t const descr_i_ff = {
	(jit_type_t)&_jit_type_int_def,
	0,
	(jit_type_t)&_jit_type_float32_def,
	(jit_type_t)&_jit_type_float32_def
};
static jit_intrinsic_descr_t const descr_i_f = {
	(jit_type_t)&_jit_type_int_def,
	0,
	(jit_type_t)&_jit_type_float32_def,
	0
};
static jit_intrinsic_descr_t const descr_d_dd = {
	(jit_type_t)&_jit_type_float64_def,
	0,
	(jit_type_t)&_jit_type_float64_def,
	(jit_type_t)&_jit_type_float64_def
};
static jit_intrinsic_descr_t const descr_d_d = {
	(jit_type_t)&_jit_type_float64_def,
	0,
	(jit_type_t)&_jit_type_float64_def,
	0
};
static jit_intrinsic_descr_t const descr_i_dd = {
	(jit_type_t)&_jit_type_int_def,
	0,
	(jit_type_t)&_jit_type_float64_def,
	(jit_type_t)&_jit_type_float64_def
};
static jit_intrinsic_descr_t const descr_i_d = {
	(jit_type_t)&_jit_type_int_def,
	0,
	(jit_type_t)&_jit_type_float64_def,
	0
};
static jit_intrinsic_descr_t const descr_D_DD = {
	(jit_type_t)&_jit_type_nfloat_def,
	0,
	(jit_type_t)&_jit_type_nfloat_def,
	(jit_type_t)&_jit_type_nfloat_def
};
static jit_intrinsic_descr_t const descr_D_D = {
	(jit_type_t)&_jit_type_nfloat_def,
	0,
	(jit_type_t)&_jit_type_nfloat_def,
	0
};
static jit_intrinsic_descr_t const descr_i_DD = {
	(jit_type_t)&_jit_type_int_def,
	0,
	(jit_type_t)&_jit_type_nfloat_def,
	(jit_type_t)&_jit_type_nfloat_def
};
static jit_intrinsic_descr_t const descr_i_D = {
	(jit_type_t)&_jit_type_int_def,
	0,
	(jit_type_t)&_jit_type_nfloat_def,
	0
};

/*
 * Apply a unary operator.
 */
static jit_value_t apply_unary
		(jit_function_t func, int oper, jit_value_t value1,
		 jit_type_t result_type)
{
	jit_value_t dest;
	jit_insn_t insn;
	if(!value1)
	{
		return 0;
	}
	if(!_jit_function_ensure_builder(func))
	{
		return 0;
	}
	insn = _jit_block_add_insn(func->builder->current_block);
	if(!insn)
	{
		return 0;
	}
	dest = jit_value_create(func, result_type);
	if(!dest)
	{
		return 0;
	}
	jit_value_ref(func, value1);
	insn->opcode = (short)oper;
	insn->dest = dest;
	insn->value1 = value1;
	return dest;
}

/*
 * Apply a binary operator.
 */
static jit_value_t apply_binary
		(jit_function_t func, int oper, jit_value_t value1,
		 jit_value_t value2, jit_type_t result_type)
{
	jit_value_t dest;
	jit_insn_t insn;
	if(!value1 || !value2)
	{
		return 0;
	}
	if(!_jit_function_ensure_builder(func))
	{
		return 0;
	}
	insn = _jit_block_add_insn(func->builder->current_block);
	if(!insn)
	{
		return 0;
	}
	dest = jit_value_create(func, result_type);
	if(!dest)
	{
		return 0;
	}
	jit_value_ref(func, value1);
	jit_value_ref(func, value2);
	insn->opcode = (short)oper;
	insn->dest = dest;
	insn->value1 = value1;
	insn->value2 = value2;
	return dest;
}

/*
 * Create a note instruction, which doesn't have a result.
 */
static int create_note
		(jit_function_t func, int oper, jit_value_t value1,
		 jit_value_t value2)
{
	jit_insn_t insn;
	if(!value1 || !value2)
	{
		return 0;
	}
	if(!_jit_function_ensure_builder(func))
	{
		return 0;
	}
	insn = _jit_block_add_insn(func->builder->current_block);
	if(!insn)
	{
		return 0;
	}
	jit_value_ref(func, value1);
	jit_value_ref(func, value2);
	insn->opcode = (short)oper;
	insn->value1 = value1;
	insn->value2 = value2;
	return 1;
}

/*
 * Create a unary note instruction, which doesn't have a result.
 */
static int create_unary_note
		(jit_function_t func, int oper, jit_value_t value1)
{
	jit_insn_t insn;
	if(!value1)
	{
		return 0;
	}
	if(!_jit_function_ensure_builder(func))
	{
		return 0;
	}
	insn = _jit_block_add_insn(func->builder->current_block);
	if(!insn)
	{
		return 0;
	}
	jit_value_ref(func, value1);
	insn->opcode = (short)oper;
	insn->value1 = value1;
	return 1;
}

/*
 * Create a note instruction with no arguments, which doesn't have a result.
 */
static int create_noarg_note(jit_function_t func, int oper)
{
	jit_insn_t insn;
	if(!_jit_function_ensure_builder(func))
	{
		return 0;
	}
	insn = _jit_block_add_insn(func->builder->current_block);
	if(!insn)
	{
		return 0;
	}
	insn->opcode = (short)oper;
	return 1;
}

/*
 * Create a note instruction with only a destination.
 */
static jit_value_t create_dest_note
	(jit_function_t func, int oper, jit_type_t type)
{
	jit_insn_t insn;
	jit_value_t value;
	if(!_jit_function_ensure_builder(func))
	{
		return 0;
	}
	value = jit_value_create(func, type);
	if(!value)
	{
		return 0;
	}
	insn = _jit_block_add_insn(func->builder->current_block);
	if(!insn)
	{
		return 0;
	}
	jit_value_ref(func, value);
	insn->opcode = (short)oper;
	insn->dest = value;
	return value;
}

/*
 * Get the common type to use for a binary operator.
 */
static jit_type_t common_binary(jit_type_t type1, jit_type_t type2,
								int int_only, int float_only)
{
	type1 = jit_type_promote_int(jit_type_normalize(type1));
	type2 = jit_type_promote_int(jit_type_normalize(type2));
	if(!float_only)
	{
		if(type1 == jit_type_int)
		{
			if(type2 == jit_type_int || type2 == jit_type_uint)
			{
				return jit_type_int;
			}
			else if(type2 == jit_type_long || type2 == jit_type_ulong)
			{
				return jit_type_long;
			}
		}
		else if(type1 == jit_type_uint)
		{
			if(type2 == jit_type_int || type2 == jit_type_uint ||
			   type2 == jit_type_long || type2 == jit_type_ulong)
			{
				return type2;
			}
		}
		else if(type1 == jit_type_long)
		{
			if(type2 == jit_type_int || type2 == jit_type_uint ||
			   type2 == jit_type_long || type2 == jit_type_ulong)
			{
				return jit_type_long;
			}
		}
		else if(type1 == jit_type_ulong)
		{
			if(type2 == jit_type_int || type2 == jit_type_long)
			{
				return jit_type_long;
			}
			else if(type2 == jit_type_uint || type2 == jit_type_ulong)
			{
				return jit_type_ulong;
			}
		}
		if(int_only)
		{
			return jit_type_long;
		}
	}
	if(type1 == jit_type_nfloat || type2 == jit_type_nfloat)
	{
		return jit_type_nfloat;
	}
	else if(type1 == jit_type_float64 || type2 == jit_type_float64)
	{
		return jit_type_float64;
	}
	else if(type1 == jit_type_float32 || type2 == jit_type_float32)
	{
		return jit_type_float32;
	}
	else
	{
		/* Probably integer arguments when "float_only" is set */
		return jit_type_nfloat;
	}
}

/*
 * Apply an intrinsic.
 */
static jit_value_t apply_intrinsic
		(jit_function_t func, const jit_opcode_descr *descr,
		 jit_value_t value1, jit_value_t value2, jit_type_t result_type)
{
	if(result_type == jit_type_int)
	{
		return jit_insn_call_intrinsic
			(func, descr->iname, descr->ifunc, descr->idesc, value1, value2);
	}
	else if(result_type == jit_type_uint)
	{
		return jit_insn_call_intrinsic
			(func, descr->iuname, descr->iufunc, descr->iudesc, value1, value2);
	}
	else if(result_type == jit_type_long)
	{
		return jit_insn_call_intrinsic
			(func, descr->lname, descr->lfunc, descr->ldesc, value1, value2);
	}
	else if(result_type == jit_type_ulong)
	{
		return jit_insn_call_intrinsic
			(func, descr->luname, descr->lufunc, descr->ludesc, value1, value2);
	}
	else if(result_type == jit_type_float32)
	{
		return jit_insn_call_intrinsic
			(func, descr->fname, descr->ffunc, descr->fdesc, value1, value2);
	}
	else if(result_type == jit_type_float64)
	{
		return jit_insn_call_intrinsic
			(func, descr->dname, descr->dfunc, descr->ddesc, value1, value2);
	}
	else
	{
		return jit_insn_call_intrinsic
			(func, descr->nfname, descr->nffunc, descr->nfdesc, value1, value2);
	}
}

/*
 * Apply a unary arithmetic operator, after coercing the
 * argument to a suitable numeric type.
 */
static jit_value_t apply_unary_arith
		(jit_function_t func, const jit_opcode_descr *descr,
		 jit_value_t value1, int int_only, int float_only,
		 int overflow_check)
{
	int oper;
	jit_type_t result_type;
	if(!value1)
	{
		return 0;
	}
	result_type = common_binary
		(value1->type, value1->type, int_only, float_only);
	if(result_type == jit_type_int)
	{
		oper = descr->ioper;
	}
	else if(result_type == jit_type_uint)
	{
		oper = descr->iuoper;
	}
	else if(result_type == jit_type_long)
	{
		oper = descr->loper;
	}
	else if(result_type == jit_type_ulong)
	{
		oper = descr->luoper;
	}
	else if(result_type == jit_type_float32)
	{
		oper = descr->foper;
	}
	else if(result_type == jit_type_float64)
	{
		oper = descr->doper;
	}
	else
	{
		oper = descr->nfoper;
	}
	value1 = jit_insn_convert(func, value1, result_type, overflow_check);
	if(_jit_opcode_is_supported(oper))
	{
		return apply_unary(func, oper, value1, result_type);
	}
	else
	{
		return apply_intrinsic(func, descr, value1, 0, result_type);
	}
}

/*
 * Apply a binary arithmetic operator, after coercing both
 * arguments to a common type.
 */
static jit_value_t apply_arith
		(jit_function_t func, const jit_opcode_descr *descr,
		 jit_value_t value1, jit_value_t value2,
		 int int_only, int float_only, int overflow_check)
{
	int oper;
	jit_type_t result_type;
	if(!value1 || !value2)
	{
		return 0;
	}
	result_type = common_binary
		(value1->type, value2->type, int_only, float_only);
	if(result_type == jit_type_int)
	{
		oper = descr->ioper;
	}
	else if(result_type == jit_type_uint)
	{
		oper = descr->iuoper;
	}
	else if(result_type == jit_type_long)
	{
		oper = descr->loper;
	}
	else if(result_type == jit_type_ulong)
	{
		oper = descr->luoper;
	}
	else if(result_type == jit_type_float32)
	{
		oper = descr->foper;
	}
	else if(result_type == jit_type_float64)
	{
		oper = descr->doper;
	}
	else
	{
		oper = descr->nfoper;
	}
	value1 = jit_insn_convert(func, value1, result_type, overflow_check);
	value2 = jit_insn_convert(func, value2, result_type, overflow_check);
	if(_jit_opcode_is_supported(oper))
	{
		return apply_binary(func, oper, value1, value2, result_type);
	}
	else
	{
		return apply_intrinsic(func, descr, value1, value2, result_type);
	}
}

/*
 * Apply a binary shift operator, after coercing both
 * arguments to suitable types.
 */
static jit_value_t apply_shift
		(jit_function_t func, const jit_opcode_descr *descr,
		 jit_value_t value1, jit_value_t value2)
{
	int oper;
	jit_type_t result_type;
	jit_type_t count_type;
	if(!value1 || !value2)
	{
		return 0;
	}
	result_type = common_binary(value1->type, value1->type, 1, 0);
	if(result_type == jit_type_int)
	{
		oper = descr->ioper;
	}
	else if(result_type == jit_type_uint)
	{
		oper = descr->iuoper;
	}
	else if(result_type == jit_type_long)
	{
		oper = descr->loper;
	}
	else if(result_type == jit_type_ulong)
	{
		oper = descr->luoper;
	}
	else
	{
		/* Shouldn't happen */
		oper = descr->loper;
	}
	count_type = jit_type_promote_int(jit_type_normalize(value2->type));
	if(count_type != jit_type_int)
	{
		count_type = jit_type_uint;
	}
	value1 = jit_insn_convert(func, value1, result_type, 0);
	value2 = jit_insn_convert(func, value2, count_type, 0);
	if(_jit_opcode_is_supported(oper))
	{
		return apply_binary(func, oper, value1, value2, result_type);
	}
	else
	{
		return apply_intrinsic(func, descr, value1, value2, result_type);
	}
}

/*
 * Apply a binary comparison operator, after coercing both
 * arguments to a common type.
 */
static jit_value_t apply_compare
		(jit_function_t func, const jit_opcode_descr *descr,
		 jit_value_t value1, jit_value_t value2, int float_only)
{
	int oper;
	jit_type_t result_type;
	if(!value1 || !value2)
	{
		return 0;
	}
	result_type = common_binary(value1->type, value2->type, 0, float_only);
	if(result_type == jit_type_int)
	{
		oper = descr->ioper;
	}
	else if(result_type == jit_type_uint)
	{
		oper = descr->iuoper;
	}
	else if(result_type == jit_type_long)
	{
		oper = descr->loper;
	}
	else if(result_type == jit_type_ulong)
	{
		oper = descr->luoper;
	}
	else if(result_type == jit_type_float32)
	{
		oper = descr->foper;
	}
	else if(result_type == jit_type_float64)
	{
		oper = descr->doper;
	}
	else
	{
		oper = descr->nfoper;
	}
	value1 = jit_insn_convert(func, value1, result_type, 0);
	value2 = jit_insn_convert(func, value2, result_type, 0);
	if(_jit_opcode_is_supported(oper))
	{
		return apply_binary(func, oper, value1, value2, jit_type_int);
	}
	else
	{
		return apply_intrinsic(func, descr, value1, value2, result_type);
	}
}

/*
 * Apply a unary test operator, after coercing the
 * argument to an appropriate type.
 */
static jit_value_t apply_test
		(jit_function_t func, const jit_opcode_descr *descr,
		 jit_value_t value1, int float_only)
{
	int oper;
	jit_type_t result_type;
	if(!value1)
	{
		return 0;
	}
	result_type = common_binary(value1->type, value1->type, 0, float_only);
	if(result_type == jit_type_int)
	{
		oper = descr->ioper;
	}
	else if(result_type == jit_type_uint)
	{
		oper = descr->iuoper;
	}
	else if(result_type == jit_type_long)
	{
		oper = descr->loper;
	}
	else if(result_type == jit_type_ulong)
	{
		oper = descr->luoper;
	}
	else if(result_type == jit_type_float32)
	{
		oper = descr->foper;
	}
	else if(result_type == jit_type_float64)
	{
		oper = descr->doper;
	}
	else
	{
		oper = descr->nfoper;
	}
	value1 = jit_insn_convert(func, value1, result_type, 0);
	if(_jit_opcode_is_supported(oper))
	{
		return apply_unary(func, oper, value1, jit_type_int);
	}
	else
	{
		return apply_intrinsic(func, descr, value1, 0, result_type);
	}
}

/*@
 * @deftypefun int jit_insn_get_opcode (jit_insn_t insn)
 * Get the opcode that is associated with an instruction.
 * @end deftypefun
@*/
int jit_insn_get_opcode(jit_insn_t insn)
{
	if(insn)
	{
		return insn->opcode;
	}
	else
	{
		return 0;
	}
}

/*@
 * @deftypefun jit_value_t jit_insn_get_dest (jit_insn_t insn)
 * Get the destination value that is associated with an instruction.
 * Returns NULL if the instruction does not have a destination.
 * @end deftypefun
@*/
jit_value_t jit_insn_get_dest(jit_insn_t insn)
{
	if(insn && (insn->flags & JIT_INSN_DEST_OTHER_FLAGS) == 0)
	{
		return insn->dest;
	}
	else
	{
		return 0;
	}
}

/*@
 * @deftypefun jit_value_t jit_insn_get_value1 (jit_insn_t insn)
 * Get the first argument value that is associated with an instruction.
 * Returns NULL if the instruction does not have a first argument value.
 * @end deftypefun
@*/
jit_value_t jit_insn_get_value1(jit_insn_t insn)
{
	if(insn && (insn->flags & JIT_INSN_VALUE1_OTHER_FLAGS) == 0)
	{
		return insn->value1;
	}
	else
	{
		return 0;
	}
}

/*@
 * @deftypefun jit_value_t jit_insn_get_value2 (jit_insn_t insn)
 * Get the second argument value that is associated with an instruction.
 * Returns NULL if the instruction does not have a second argument value.
 * @end deftypefun
@*/
jit_value_t jit_insn_get_value2(jit_insn_t insn)
{
	if(insn && (insn->flags & JIT_INSN_VALUE2_OTHER_FLAGS) == 0)
	{
		return insn->value2;
	}
	else
	{
		return 0;
	}
}

/*@
 * @deftypefun jit_label_t jit_insn_get_label (jit_insn_t insn)
 * Get the label for a branch target from an instruction.
 * Returns NULL if the instruction does not have a branch target.
 * @end deftypefun
@*/
jit_label_t jit_insn_get_label(jit_insn_t insn)
{
	if(insn && (insn->flags & JIT_INSN_DEST_IS_LABEL) != 0)
	{
		return (jit_label_t)(insn->dest);
	}
	else
	{
		return 0;
	}
}

/*@
 * @deftypefun jit_function_t jit_insn_get_function (jit_insn_t insn)
 * Get the function for a call instruction.  Returns NULL if the
 * instruction does not refer to a called function.
 * @end deftypefun
@*/
jit_function_t jit_insn_get_function(jit_insn_t insn)
{
	if(insn && (insn->flags & JIT_INSN_DEST_IS_FUNCTION) != 0)
	{
		return (jit_function_t)(insn->dest);
	}
	else
	{
		return 0;
	}
}

/*@
 * @deftypefun {void *} jit_insn_get_native (jit_insn_t insn)
 * Get the function pointer for a native call instruction.
 * Returns NULL if the instruction does not refer to a native
 * function call.
 * @end deftypefun
@*/
void *jit_insn_get_native(jit_insn_t insn)
{
	if(insn && (insn->flags & JIT_INSN_DEST_IS_NATIVE) != 0)
	{
		return (void *)(insn->dest);
	}
	else
	{
		return 0;
	}
}

/*@
 * @deftypefun {const char *} jit_insn_get_name (jit_insn_t insn)
 * Get the diagnostic name for a function call.  Returns NULL
 * if the instruction does not have a diagnostic name.
 * @end deftypefun
@*/
const char *jit_insn_get_name(jit_insn_t insn)
{
	if(insn && (insn->flags & JIT_INSN_VALUE1_IS_NAME) != 0)
	{
		return (const char *)(insn->value1);
	}
	else
	{
		return 0;
	}
}

/*@
 * @deftypefun jit_type_t jit_insn_get_signature (jit_insn_t insn)
 * Get the signature for a function call instruction.  Returns NULL
 * if the instruction is not a function call.
 * @end deftypefun
@*/
jit_type_t jit_insn_get_signature(jit_insn_t insn)
{
	if(insn && (insn->flags & JIT_INSN_VALUE2_IS_SIGNATURE) != 0)
	{
		return (jit_type_t)(insn->value2);
	}
	else
	{
		return 0;
	}
}

/*@
 * @deftypefun int jit_insn_dest_is_value (jit_insn_t insn)
 * Returns a non-zero value if the destination for @code{insn} is
 * actually a source value.  This can happen with instructions
 * such as @code{jit_insn_store_relative} where the instruction
 * needs three source operands, and the real destination is a
 * side-effect on one of the sources.
 * @end deftypefun
@*/
int jit_insn_dest_is_value(jit_insn_t insn)
{
	if(insn && (insn->flags & JIT_INSN_DEST_IS_VALUE) != 0)
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

/*@
 * @deftypefun void jit_insn_label (jit_function_t func, {jit_label_t *} label)
 * Start a new block within the function @code{func} and give it the
 * specified @code{label}.  Returns zero if out of memory.
 *
 * If the contents of @code{label} are @code{jit_label_undefined}, then this
 * function will allocate a new label for this block.  Otherwise it will
 * reuse the specified label from a previous branch instruction.
 * @end deftypefun
@*/
int jit_insn_label(jit_function_t func, jit_label_t *label)
{
	jit_block_t current;
	jit_insn_t last;
	if(!_jit_function_ensure_builder(func))
	{
		return 0;
	}
	current = func->builder->current_block;
	last = _jit_block_get_last(current);
	if(current->label == jit_label_undefined && !last)
	{
		/* We just started a new block after a branch instruction,
		   so don't bother creating another new block */
		if(*label == jit_label_undefined)
		{
			*label = (func->builder->next_label)++;
		}
		current->label = *label;
		current->entered_via_branch = 1;
		if(!_jit_block_record_label(current))
		{
			return 0;
		}
	}
	else
	{
		/* Create a new block */
		jit_block_t block = _jit_block_create(func, label);
		if(!block)
		{
			return 0;
		}

		/* The label indicates that something is branching to us */
		block->entered_via_branch = 1;

		/* Does the last block contain instructions? */
		if(last)
		{
			/* We will be entered via the top if the last block
			   did not end in an unconditional branch and it
			   is not explicitly marked as "dead" */
			if(last->opcode != JIT_OP_BR && !(current->ends_in_dead))
			{
				block->entered_via_top = 1;
			}
		}
		else
		{
			/* We will be entered via the top if the last empty
			   block was entered via any mechanism */
			block->entered_via_top =
				(current->entered_via_top ||
				 current->entered_via_branch);
		}

		/* Set the new block as the current one */
		func->builder->current_block = block;
	}
	return 1;
}

int _jit_load_opcode(int base_opcode, jit_type_t type,
					 jit_value_t value, int no_temps)
{
	type = jit_type_normalize(type);
	if(!type)
	{
		return 0;
	}
	switch(type->kind)
	{
		case JIT_TYPE_SBYTE:
		{
			return base_opcode;
		}
		/* Not reached */

		case JIT_TYPE_UBYTE:
		{
			return base_opcode + 1;
		}
		/* Not reached */

		case JIT_TYPE_SHORT:
		{
			return base_opcode + 2;
		}
		/* Not reached */

		case JIT_TYPE_USHORT:
		{
			return base_opcode + 3;
		}
		/* Not reached */

		case JIT_TYPE_INT:
		case JIT_TYPE_UINT:
		{
			if(no_temps && value && (value->is_temporary || value->is_local))
			{
				return 0;
			}
			return base_opcode + 4;
		}
		/* Not reached */

		case JIT_TYPE_LONG:
		case JIT_TYPE_ULONG:
		{
			if(no_temps && value && (value->is_temporary || value->is_local))
			{
				return 0;
			}
			return base_opcode + 5;
		}
		/* Not reached */

		case JIT_TYPE_FLOAT32:
		{
			if(no_temps && value && (value->is_temporary || value->is_local))
			{
				return 0;
			}
			return base_opcode + 6;
		}
		/* Not reached */

		case JIT_TYPE_FLOAT64:
		{
			if(no_temps && value && (value->is_temporary || value->is_local))
			{
				return 0;
			}
			return base_opcode + 7;
		}
		/* Not reached */

		case JIT_TYPE_NFLOAT:
		{
			if(no_temps && value && (value->is_temporary || value->is_local))
			{
				return 0;
			}
			return base_opcode + 8;
		}
		/* Not reached */

		case JIT_TYPE_STRUCT:
		case JIT_TYPE_UNION:
		{
			return base_opcode + 9;
		}
		/* Not reached */
	}
	return 0;
}

int _jit_store_opcode(int base_opcode, int small_base, jit_type_t type)
{
	/* Copy instructions are in two ranges: adjust for them */
	if(small_base)
	{
		base_opcode -= 2;
	}
	else
	{
		small_base = base_opcode;
	}

	/* Determine which opcode to use */
	type = jit_type_normalize(type);
	switch(type->kind)
	{
		case JIT_TYPE_SBYTE:
		case JIT_TYPE_UBYTE:
		{
			return small_base;
		}
		/* Not reached */

		case JIT_TYPE_SHORT:
		case JIT_TYPE_USHORT:
		{
			return small_base + 1;
		}
		/* Not reached */

		case JIT_TYPE_INT:
		case JIT_TYPE_UINT:
		{
			return base_opcode + 2;
		}
		/* Not reached */

		case JIT_TYPE_LONG:
		case JIT_TYPE_ULONG:
		{
			return base_opcode + 3;
		}
		/* Not reached */

		case JIT_TYPE_FLOAT32:
		{
			return base_opcode + 4;
		}
		break;

		case JIT_TYPE_FLOAT64:
		{
			return base_opcode + 5;
		}
		break;
		/* Not reached */

		case JIT_TYPE_NFLOAT:
		{
			return base_opcode + 6;
		}
		/* Not reached */

		case JIT_TYPE_STRUCT:
		case JIT_TYPE_UNION:
		{
			return base_opcode + 7;
		}
		/* Not reached */

		default:
		{
			/* Shouldn't happen, but do something sane anyway */
			return base_opcode + 2;
		}
		/* Not reached */
	}
}

/*@
 * @deftypefun jit_value_t jit_insn_load (jit_function_t func, jit_value_t value)
 * Load the contents of @code{value} into a new temporary, essentially
 * duplicating the value.  Constants are not duplicated.
 * @end deftypefun
@*/
jit_value_t jit_insn_load(jit_function_t func, jit_value_t value)
{
	if(!value)
	{
		return 0;
	}
	else if(value->is_constant)
	{
		return value;
	}
	else
	{
		int opcode = _jit_load_opcode
			(JIT_OP_COPY_LOAD_SBYTE, value->type, value, 0);
		return apply_unary(func, opcode, value, value->type);
	}
}

/*@
 * @deftypefun jit_value_t jit_insn_dup (jit_function_t func, jit_value_t value)
 * This is the same as @code{jit_insn_load}, but the name may better
 * reflect how it is used in some front ends.
 * @end deftypefun
@*/
jit_value_t jit_insn_dup(jit_function_t func, jit_value_t value)
{
	return jit_insn_load(func, value);
}

/*@
 * @deftypefun jit_value_t jit_insn_load_small (jit_function_t func, jit_value_t value)
 * If @code{value} is of type @code{sbyte}, @code{byte}, @code{short},
 * @code{ushort}, a structure, or a union, then make a copy of it and
 * return the temporary copy.  Otherwise return @code{value} as-is.
 *
 * This is useful where you want to use @code{value} directly without
 * duplicating it first.  However, certain types usually cannot
 * be operated on directly without first copying them elsewhere.
 * This function will do that whenever necessary.
 * @end deftypefun
@*/
jit_value_t jit_insn_load_small(jit_function_t func, jit_value_t value)
{
	if(!value)
	{
		return 0;
	}
	else if(value->is_constant)
	{
		return value;
	}
	else
	{
		int opcode = _jit_load_opcode
			(JIT_OP_COPY_LOAD_SBYTE, value->type, value, 1);
		if(opcode)
		{
			return apply_unary(func, opcode, value, value->type);
		}
		else
		{
			return value;
		}
	}
}

/*@
 * @deftypefun void jit_insn_store (jit_function_t func, jit_value_t dest, jit_value_t value)
 * Store the contents of @code{value} at the location referred to by
 * @code{dest}.
 * @end deftypefun
@*/
int jit_insn_store(jit_function_t func, jit_value_t dest, jit_value_t value)
{
	jit_insn_t insn;
	if(!dest || !value)
	{
		return 0;
	}
	if(!_jit_function_ensure_builder(func))
	{
		return 0;
	}
	value = jit_insn_convert(func, value, dest->type, 0);
	if(!value)
	{
		return 0;
	}
	insn = _jit_block_add_insn(func->builder->current_block);
	if(!insn)
	{
		return 0;
	}
	jit_value_ref(func, dest);
	jit_value_ref(func, value);
	insn->opcode = (short)_jit_store_opcode
		(JIT_OP_COPY_INT, JIT_OP_COPY_STORE_BYTE, value->type);
	insn->dest = dest;
	insn->value1 = value;
	return 1;
}

/*
 * Scan back through the current block, looking for a relative adjustment
 * that involves "value" as its destination.  Returns NULL if no such
 * instruction was found, or it is blocked by a later use of "value".
 * "addrof" will be set to a non-NULL value if the instruction just before
 * the relative adjustment took the address of a local frame variable.
 * This instruction is a candidate for being moved down to where the
 * "load_relative" or "store_relative" occurs.
 */
static jit_insn_t previous_relative(jit_function_t func, jit_value_t value,
									jit_insn_t *addrof)
{
	jit_insn_iter_t iter;
	jit_insn_t insn;
	jit_insn_t insn2;
	jit_insn_t insn3;

	/* Clear "addrof" first */
	*addrof = 0;

	/* If the value is not temporary, then it isn't a candidate */
	if(!(value->is_temporary))
	{
		return 0;
	}

	/* Iterate back through the block looking for a suitable adjustment */
	jit_insn_iter_init_last(&iter, func->builder->current_block);
	while((insn = jit_insn_iter_previous(&iter)) != 0)
	{
		if(insn->opcode == JIT_OP_ADD_RELATIVE && insn->dest == value)
		{
			/* See if the instruction just before the "add_relative"
			   is an "address_of" that is being used by the add */
			insn3 = jit_insn_iter_previous(&iter);
			if(insn3)
			{
				jit_insn_iter_next(&iter);
				if(insn3->opcode != JIT_OP_ADDRESS_OF ||
				   insn3->dest != insn->value1 ||
				   !(insn3->dest->is_temporary))
				{
					insn3 = 0;
				}
			}

			/* Scan forwards to ensure that "insn->value1" is not
			   used anywhere in the instructions that follow */
			jit_insn_iter_next(&iter);
			while((insn2 = jit_insn_iter_next(&iter)) != 0)
			{
				if(insn2->dest == insn->value1 ||
				   insn2->value1 == insn->value1 ||
				   insn2->value2 == insn->value1)
				{
					return 0;
				}
				if(insn3)
				{
					/* We may need to disable the "address_of" instruction
					   if any of its values are used further on */
					if(insn2->dest == insn3->dest ||
				       insn2->value1 == insn3->dest ||
				       insn2->value2 == insn3->dest ||
					   insn2->dest == insn3->value1 ||
				       insn2->value1 == insn3->value1 ||
				       insn2->value2 == insn3->value1)
					{
						insn3 = 0;
					}
				}
			}
			if(insn3)
			{
				*addrof  = insn3;
			}
			return insn;
		}
		if(insn->dest == value || insn->value1 == value ||
		   insn->value2 == value)
		{
			/* This instruction uses "value" in some way, so it
			   blocks any previous "add_relative" instructions */
			return 0;
		}
	}
	return 0;
}

/*@
 * @deftypefun jit_value_t jit_insn_load_relative (jit_function_t func, jit_value_t value, jit_nint offset, jit_type_t type)
 * Load a value of the specified @code{type} from the effective address
 * @code{(value + offset)}, where @code{value} is a pointer.
 * @end deftypefun
@*/
jit_value_t jit_insn_load_relative
		(jit_function_t func, jit_value_t value,
		 jit_nint offset, jit_type_t type)
{
	jit_insn_t insn;
	jit_insn_t addrof;
	if(!value)
	{
		return 0;
	}
	if(!_jit_function_ensure_builder(func))
	{
		return 0;
	}
	insn = previous_relative(func, value, &addrof);
	if(insn)
	{
		/* We have a previous "add_relative" instruction for this
		   pointer.  Remove it from the instruction stream and
		   adjust the current offset accordingly */
		offset += jit_value_get_nint_constant(insn->value2);
		value = insn->value1;
		insn->opcode = JIT_OP_NOP;
		insn->dest = 0;
		insn->value1 = 0;
		insn->value2 = 0;
		if(addrof)
		{
			/* Shift the "address_of" instruction down too, to make
			   it easier for the code generator to handle field
			   accesses within local and global variables */
			value = jit_insn_address_of(func, addrof->value1);
			if(!value)
			{
				return 0;
			}
			addrof->opcode = JIT_OP_NOP;
			addrof->dest = 0;
			addrof->value1 = 0;
			addrof->value2 = 0;
		}
	}
	return apply_binary
		(func, _jit_load_opcode(JIT_OP_LOAD_RELATIVE_SBYTE, type, 0, 0), value,
		 jit_value_create_nint_constant(func, jit_type_nint, offset), type);
}

/*@
 * @deftypefun int jit_insn_store_relative (jit_function_t func, jit_value_t dest, jit_nint offset, jit_value_t value)
 * Store @code{value} at the effective address @code{(dest + offset)},
 * where @code{dest} is a pointer.
 * @end deftypefun
@*/
int jit_insn_store_relative
		(jit_function_t func, jit_value_t dest,
		 jit_nint offset, jit_value_t value)
{
	jit_insn_t insn;
	jit_insn_t addrof;
	jit_value_t offset_value;
	if(!dest || !value)
	{
		return 0;
	}
	if(!_jit_function_ensure_builder(func))
	{
		return 0;
	}
	insn = previous_relative(func, dest, &addrof);
	if(insn)
	{
		/* We have a previous "add_relative" instruction for this
		   pointer.  Remove it from the instruction stream and
		   adjust the current offset accordingly */
		offset += jit_value_get_nint_constant(insn->value2);
		dest = insn->value1;
		insn->opcode = JIT_OP_NOP;
		insn->dest = 0;
		insn->value1 = 0;
		insn->value2 = 0;
		if(addrof)
		{
			/* Shift the "address_of" instruction down too, to make
			   it easier for the code generator to handle field
			   accesses within local and global variables */
			dest = jit_insn_address_of(func, addrof->value1);
			if(!dest)
			{
				return 0;
			}
			addrof->opcode = JIT_OP_NOP;
			addrof->dest = 0;
			addrof->value1 = 0;
			addrof->value2 = 0;
		}
	}
	offset_value = jit_value_create_nint_constant(func, jit_type_nint, offset);
	if(!offset_value)
	{
		return 0;
	}
	insn = _jit_block_add_insn(func->builder->current_block);
	if(!insn)
	{
		return 0;
	}
	jit_value_ref(func, dest);
	jit_value_ref(func, value);
	insn->opcode = (short)_jit_store_opcode
		(JIT_OP_STORE_RELATIVE_BYTE, 0, value->type);
	insn->flags = JIT_INSN_DEST_IS_VALUE;
	insn->dest = dest;
	insn->value1 = value;
	insn->value2 = offset_value;
	return 1;
}

/*@
 * @deftypefun jit_value_t jit_insn_add_relative (jit_function_t func, jit_value_t value, jit_nint offset)
 * Add the constant @code{offset} to the specified pointer @code{value}.
 * This is functionally identical to calling @code{jit_insn_add}, but
 * the JIT can optimize the code better if it knows that the addition
 * is being used to perform a relative adjustment on a pointer.
 * In particular, multiple relative adjustments on the same pointer
 * can be collapsed into a single adjustment.
 * @end deftypefun
@*/
jit_value_t jit_insn_add_relative
		(jit_function_t func, jit_value_t value, jit_nint offset)
{
	jit_insn_t insn;
	jit_insn_t addrof;
	if(!value)
	{
		return 0;
	}
	if(!_jit_function_ensure_builder(func))
	{
		return 0;
	}
	insn = previous_relative(func, value, &addrof);
	if(insn)
	{
		/* Back-patch the "add_relative" instruction to adjust the offset */
		insn->value2 = jit_value_create_nint_constant
			(func, jit_type_nint,
			 jit_value_get_nint_constant(insn->value2) + offset);
		return value;
	}
	else
	{
		/* Create a new "add_relative" instruction */
		return apply_binary(func, JIT_OP_ADD_RELATIVE, value,
							jit_value_create_nint_constant
								(func, jit_type_nint, offset),
							jit_type_void_ptr);
	}
}

/*@
 * @deftypefun int jit_insn_check_null (jit_function_t func, jit_value_t value)
 * Check @code{value} to see if it is NULL.  If it is, then throw the
 * built-in @code{JIT_RESULT_NULL_REFERENCE} exception.
 * @end deftypefun
@*/
int jit_insn_check_null(jit_function_t func, jit_value_t value)
{
	if(!_jit_function_ensure_builder(func))
	{
		return 0;
	}
	func->builder->may_throw = 1;
	return create_unary_note(func, JIT_OP_CHECK_NULL, value);
}

int _jit_insn_check_is_redundant(const jit_insn_iter_t *iter)
{
	jit_insn_iter_t new_iter = *iter;
	jit_insn_t insn;
	jit_value_t value;

	/* Back up to find the "check_null" instruction of interest */
	insn = jit_insn_iter_previous(&new_iter);
	value = insn->value1;

	/* The value must be temporary or local, and not volatile or addressable.
	   Otherwise the value could be vulnerable to aliasing side-effects that
	   could make it NULL again even after we have checked it */
	if(!(value->is_temporary) || !(value->is_local))
	{
		return 0;
	}
	if(value->is_volatile || value->is_addressable)
	{
		return 0;
	}

	/* Search back for a previous "check_null" instruction */
	while((insn = jit_insn_iter_previous(&new_iter)) != 0)
	{
		if(insn->opcode == JIT_OP_CHECK_NULL && insn->value1 == value)
		{
			/* This is the previous "check_null" that we were looking for */
			return 1;
		}
		if(insn->opcode >= JIT_OP_STORE_RELATIVE_BYTE &&
		   insn->opcode <= JIT_OP_STORE_RELATIVE_STRUCT)
		{
			/* This stores to the memory referenced by the destination,
			   not to the destination itself, so it cannot affect "value" */
			continue;
		}
		if(insn->dest == value)
		{
			/* The value was used as a destination, so we must check */
			return 0;
		}
	}

	/* There was no previous "check_null" instruction for this value */
	return 0;
}

/*@
 * @deftypefun jit_value_t jit_insn_add (jit_function_t func, jit_value_t value1, jit_value_t value2)
 * Add two values together and return the result in a new temporary value.
 * @end deftypefun
@*/
jit_value_t jit_insn_add
		(jit_function_t func, jit_value_t value1, jit_value_t value2)
{
	static jit_opcode_descr const add_descr = {
		JIT_OP_IADD,
		JIT_OP_IADD,
		JIT_OP_LADD,
		JIT_OP_LADD,
		JIT_OP_FADD,
		JIT_OP_DADD,
		JIT_OP_NFADD,
		jit_intrinsic(jit_int_add, descr_i_ii),
		jit_intrinsic(jit_uint_add, descr_I_II),
		jit_intrinsic(jit_long_add, descr_l_ll),
		jit_intrinsic(jit_ulong_add, descr_L_LL),
		jit_intrinsic(jit_float32_add, descr_f_ff),
		jit_intrinsic(jit_float64_add, descr_d_dd),
		jit_intrinsic(jit_nfloat_add, descr_D_DD)
	};
	return apply_arith(func, &add_descr, value1, value2, 0, 0, 0);
}

/*@
 * @deftypefun jit_value_t jit_insn_add_ovf (jit_function_t func, jit_value_t value1, jit_value_t value2)
 * Add two values together and return the result in a new temporary value.
 * Throw an exception if overflow occurs.
 * @end deftypefun
@*/
jit_value_t jit_insn_add_ovf
		(jit_function_t func, jit_value_t value1, jit_value_t value2)
{
	static jit_opcode_descr const add_ovf_descr = {
		JIT_OP_IADD_OVF,
		JIT_OP_IADD_OVF_UN,
		JIT_OP_LADD_OVF,
		JIT_OP_LADD_OVF_UN,
		JIT_OP_FADD,
		JIT_OP_DADD,
		JIT_OP_NFADD,
		jit_intrinsic(jit_int_add_ovf, descr_e_pi_ii),
		jit_intrinsic(jit_uint_add_ovf, descr_e_pI_II),
		jit_intrinsic(jit_long_add_ovf, descr_e_pl_ll),
		jit_intrinsic(jit_ulong_add_ovf, descr_e_pL_LL),
		jit_intrinsic(jit_float32_add, descr_f_ff),
		jit_intrinsic(jit_float64_add, descr_d_dd),
		jit_intrinsic(jit_nfloat_add, descr_D_DD)
	};
	return apply_arith(func, &add_ovf_descr, value1, value2, 0, 0, 1);
}

/*@
 * @deftypefun jit_value_t jit_insn_sub (jit_function_t func, jit_value_t value1, jit_value_t value2)
 * Subtract two values and return the result in a new temporary value.
 * @end deftypefun
@*/
jit_value_t jit_insn_sub
		(jit_function_t func, jit_value_t value1, jit_value_t value2)
{
	static jit_opcode_descr const sub_descr = {
		JIT_OP_ISUB,
		JIT_OP_ISUB,
		JIT_OP_LSUB,
		JIT_OP_LSUB,
		JIT_OP_FSUB,
		JIT_OP_DSUB,
		JIT_OP_NFSUB,
		jit_intrinsic(jit_int_sub, descr_i_ii),
		jit_intrinsic(jit_uint_sub, descr_I_II),
		jit_intrinsic(jit_long_sub, descr_l_ll),
		jit_intrinsic(jit_ulong_sub, descr_L_LL),
		jit_intrinsic(jit_float32_sub, descr_f_ff),
		jit_intrinsic(jit_float64_sub, descr_d_dd),
		jit_intrinsic(jit_nfloat_sub, descr_D_DD)
	};
	return apply_arith(func, &sub_descr, value1, value2, 0, 0, 0);
}

/*@
 * @deftypefun jit_value_t jit_insn_sub_ovf (jit_function_t func, jit_value_t value1, jit_value_t value2)
 * Subtract two values and return the result in a new temporary value.
 * Throw an exception if overflow occurs.
 * @end deftypefun
@*/
jit_value_t jit_insn_sub_ovf
		(jit_function_t func, jit_value_t value1, jit_value_t value2)
{
	static jit_opcode_descr const sub_ovf_descr = {
		JIT_OP_ISUB_OVF,
		JIT_OP_ISUB_OVF_UN,
		JIT_OP_LSUB_OVF,
		JIT_OP_LSUB_OVF_UN,
		JIT_OP_FSUB,
		JIT_OP_DSUB,
		JIT_OP_NFSUB,
		jit_intrinsic(jit_int_sub_ovf, descr_e_pi_ii),
		jit_intrinsic(jit_uint_sub_ovf, descr_e_pI_II),
		jit_intrinsic(jit_long_sub_ovf, descr_e_pl_ll),
		jit_intrinsic(jit_ulong_sub_ovf, descr_e_pL_LL),
		jit_intrinsic(jit_float32_sub, descr_f_ff),
		jit_intrinsic(jit_float64_sub, descr_d_dd),
		jit_intrinsic(jit_nfloat_sub, descr_D_DD)
	};
	return apply_arith(func, &sub_ovf_descr, value1, value2, 0, 0, 1);
}

/*@
 * @deftypefun jit_value_t jit_insn_mul (jit_function_t func, jit_value_t value1, jit_value_t value2)
 * Multiply two values and return the result in a new temporary value.
 * @end deftypefun
@*/
jit_value_t jit_insn_mul
		(jit_function_t func, jit_value_t value1, jit_value_t value2)
{
	static jit_opcode_descr const mul_descr = {
		JIT_OP_IMUL,
		JIT_OP_IMUL,
		JIT_OP_LMUL,
		JIT_OP_LMUL,
		JIT_OP_FMUL,
		JIT_OP_DMUL,
		JIT_OP_NFMUL,
		jit_intrinsic(jit_int_mul, descr_i_ii),
		jit_intrinsic(jit_uint_mul, descr_I_II),
		jit_intrinsic(jit_long_mul, descr_l_ll),
		jit_intrinsic(jit_ulong_mul, descr_L_LL),
		jit_intrinsic(jit_float32_mul, descr_f_ff),
		jit_intrinsic(jit_float64_mul, descr_d_dd),
		jit_intrinsic(jit_nfloat_mul, descr_D_DD)
	};
	return apply_arith(func, &mul_descr, value1, value2, 0, 0, 0);
}

/*@
 * @deftypefun jit_value_t jit_insn_mul_ovf (jit_function_t func, jit_value_t value1, jit_value_t value2)
 * Multiply two values and return the result in a new temporary value.
 * Throw an exception if overflow occurs.
 * @end deftypefun
@*/
jit_value_t jit_insn_mul_ovf
		(jit_function_t func, jit_value_t value1, jit_value_t value2)
{
	static jit_opcode_descr const mul_ovf_descr = {
		JIT_OP_IMUL_OVF,
		JIT_OP_IMUL_OVF_UN,
		JIT_OP_LMUL_OVF,
		JIT_OP_LMUL_OVF_UN,
		JIT_OP_FMUL,
		JIT_OP_DMUL,
		JIT_OP_NFMUL,
		jit_intrinsic(jit_int_mul_ovf, descr_e_pi_ii),
		jit_intrinsic(jit_uint_mul_ovf, descr_e_pI_II),
		jit_intrinsic(jit_long_mul_ovf, descr_e_pl_ll),
		jit_intrinsic(jit_ulong_mul_ovf, descr_e_pL_LL),
		jit_intrinsic(jit_float32_mul, descr_f_ff),
		jit_intrinsic(jit_float64_mul, descr_d_dd),
		jit_intrinsic(jit_nfloat_mul, descr_D_DD)
	};
	return apply_arith(func, &mul_ovf_descr, value1, value2, 0, 0, 1);
}

/*@
 * @deftypefun jit_value_t jit_insn_div (jit_function_t func, jit_value_t value1, jit_value_t value2)
 * Divide two values and return the quotient in a new temporary value.
 * Throws an exception on division by zero or arithmetic error
 * (an arithmetic error is one where the minimum possible signed
 * integer value is divided by -1).
 * @end deftypefun
@*/
jit_value_t jit_insn_div
		(jit_function_t func, jit_value_t value1, jit_value_t value2)
{
	static jit_opcode_descr const div_descr = {
		JIT_OP_IDIV,
		JIT_OP_IDIV_UN,
		JIT_OP_LDIV,
		JIT_OP_LDIV_UN,
		JIT_OP_FDIV,
		JIT_OP_DDIV,
		JIT_OP_NFDIV,
		jit_intrinsic(jit_int_div, descr_e_pi_ii),
		jit_intrinsic(jit_uint_div, descr_e_pI_II),
		jit_intrinsic(jit_long_div, descr_e_pl_ll),
		jit_intrinsic(jit_ulong_div, descr_e_pL_LL),
		jit_intrinsic(jit_float32_div, descr_f_ff),
		jit_intrinsic(jit_float64_div, descr_d_dd),
		jit_intrinsic(jit_nfloat_div, descr_D_DD)
	};
	return apply_arith(func, &div_descr, value1, value2, 0, 0, 0);
}

/*@
 * @deftypefun jit_value_t jit_insn_rem (jit_function_t func, jit_value_t value1, jit_value_t value2)
 * Divide two values and return the remainder in a new temporary value.
 * Throws an exception on division by zero or arithmetic error
 * (an arithmetic error is one where the minimum possible signed
 * integer value is divided by -1).
 * @end deftypefun
@*/
jit_value_t jit_insn_rem
		(jit_function_t func, jit_value_t value1, jit_value_t value2)
{
	static jit_opcode_descr const rem_descr = {
		JIT_OP_IREM,
		JIT_OP_IREM_UN,
		JIT_OP_LREM,
		JIT_OP_LREM_UN,
		JIT_OP_FREM,
		JIT_OP_DREM,
		JIT_OP_NFREM,
		jit_intrinsic(jit_int_rem, descr_e_pi_ii),
		jit_intrinsic(jit_uint_rem, descr_e_pI_II),
		jit_intrinsic(jit_long_rem, descr_e_pl_ll),
		jit_intrinsic(jit_ulong_rem, descr_e_pL_LL),
		jit_intrinsic(jit_float32_rem, descr_f_ff),
		jit_intrinsic(jit_float64_rem, descr_d_dd),
		jit_intrinsic(jit_nfloat_rem, descr_D_DD)
	};
	return apply_arith(func, &rem_descr, value1, value2, 0, 0, 0);
}

/*@
 * @deftypefun jit_value_t jit_insn_rem_ieee (jit_function_t func, jit_value_t value1, jit_value_t value2)
 * Divide two values and return the remainder in a new temporary value.
 * Throws an exception on division by zero or arithmetic error
 * (an arithmetic error is one where the minimum possible signed
 * integer value is divided by -1).  This function is identical to
 * @code{jit_insn_rem}, except that it uses IEEE rules for computing
 * the remainder of floating-point values.
 * @end deftypefun
@*/
jit_value_t jit_insn_rem_ieee
		(jit_function_t func, jit_value_t value1, jit_value_t value2)
{
	static jit_opcode_descr const rem_ieee_descr = {
		JIT_OP_IREM,
		JIT_OP_IREM_UN,
		JIT_OP_LREM,
		JIT_OP_LREM_UN,
		JIT_OP_FREM_IEEE,
		JIT_OP_DREM_IEEE,
		JIT_OP_NFREM_IEEE,
		jit_intrinsic(jit_int_rem, descr_e_pi_ii),
		jit_intrinsic(jit_uint_rem, descr_e_pI_II),
		jit_intrinsic(jit_long_rem, descr_e_pl_ll),
		jit_intrinsic(jit_ulong_rem, descr_e_pL_LL),
		jit_intrinsic(jit_float32_ieee_rem, descr_f_ff),
		jit_intrinsic(jit_float64_ieee_rem, descr_d_dd),
		jit_intrinsic(jit_nfloat_ieee_rem, descr_D_DD)
	};
	return apply_arith(func, &rem_ieee_descr, value1, value2, 0, 0, 0);
}

/*@
 * @deftypefun jit_value_t jit_insn_neg (jit_function_t func, jit_value_t value1)
 * Negate a value and return the result in a new temporary value.
 * @end deftypefun
@*/
jit_value_t jit_insn_neg
		(jit_function_t func, jit_value_t value1)
{
	static jit_opcode_descr const neg_descr = {
		JIT_OP_INEG,
		JIT_OP_INEG,
		JIT_OP_LNEG,
		JIT_OP_LNEG,
		JIT_OP_FNEG,
		JIT_OP_DNEG,
		JIT_OP_NFNEG,
		jit_intrinsic(jit_int_neg, descr_i_i),
		jit_intrinsic(jit_uint_neg, descr_I_I),
		jit_intrinsic(jit_long_neg, descr_l_l),
		jit_intrinsic(jit_ulong_neg, descr_L_L),
		jit_intrinsic(jit_float32_neg, descr_f_f),
		jit_intrinsic(jit_float64_neg, descr_d_d),
		jit_intrinsic(jit_nfloat_neg, descr_D_D)
	};
	return apply_unary_arith(func, &neg_descr, value1, 0, 0, 0);
}

/*@
 * @deftypefun jit_value_t jit_insn_and (jit_function_t func, jit_value_t value1, jit_value_t value2)
 * Bitwise AND two values and return the result in a new temporary value.
 * @end deftypefun
@*/
jit_value_t jit_insn_and
		(jit_function_t func, jit_value_t value1, jit_value_t value2)
{
	static jit_opcode_descr const and_descr = {
		JIT_OP_IAND,
		JIT_OP_IAND,
		JIT_OP_LAND,
		JIT_OP_LAND,
		0, 0, 0,
		jit_intrinsic(jit_int_and, descr_i_ii),
		jit_intrinsic(jit_uint_and, descr_I_II),
		jit_intrinsic(jit_long_and, descr_l_ll),
		jit_intrinsic(jit_ulong_and, descr_L_LL),
		jit_no_intrinsic,
		jit_no_intrinsic,
		jit_no_intrinsic
	};
	return apply_arith(func, &and_descr, value1, value2, 1, 0, 0);
}

/*@
 * @deftypefun jit_value_t jit_insn_or (jit_function_t func, jit_value_t value1, jit_value_t value2)
 * Bitwise OR two values and return the result in a new temporary value.
 * @end deftypefun
@*/
jit_value_t jit_insn_or
		(jit_function_t func, jit_value_t value1, jit_value_t value2)
{
	static jit_opcode_descr const or_descr = {
		JIT_OP_IOR,
		JIT_OP_IOR,
		JIT_OP_LOR,
		JIT_OP_LOR,
		0, 0, 0,
		jit_intrinsic(jit_int_or, descr_i_ii),
		jit_intrinsic(jit_uint_or, descr_I_II),
		jit_intrinsic(jit_long_or, descr_l_ll),
		jit_intrinsic(jit_ulong_or, descr_L_LL),
		jit_no_intrinsic,
		jit_no_intrinsic,
		jit_no_intrinsic
	};
	return apply_arith(func, &or_descr, value1, value2, 1, 0, 0);
}

/*@
 * @deftypefun jit_value_t jit_insn_xor (jit_function_t func, jit_value_t value1, jit_value_t value2)
 * Bitwise XOR two values and return the result in a new temporary value.
 * @end deftypefun
@*/
jit_value_t jit_insn_xor
		(jit_function_t func, jit_value_t value1, jit_value_t value2)
{
	static jit_opcode_descr const xor_descr = {
		JIT_OP_IXOR,
		JIT_OP_IXOR,
		JIT_OP_LXOR,
		JIT_OP_LXOR,
		0, 0, 0,
		jit_intrinsic(jit_int_xor, descr_i_ii),
		jit_intrinsic(jit_uint_xor, descr_I_II),
		jit_intrinsic(jit_long_xor, descr_l_ll),
		jit_intrinsic(jit_ulong_xor, descr_L_LL),
		jit_no_intrinsic,
		jit_no_intrinsic,
		jit_no_intrinsic
	};
	return apply_arith(func, &xor_descr, value1, value2, 1, 0, 0);
}

/*@
 * @deftypefun jit_value_t jit_insn_not (jit_function_t func, jit_value_t value1)
 * Bitwise NOT a value and return the result in a new temporary value.
 * @end deftypefun
@*/
jit_value_t jit_insn_not
		(jit_function_t func, jit_value_t value1)
{
	static jit_opcode_descr const not_descr = {
		JIT_OP_INOT,
		JIT_OP_INOT,
		JIT_OP_LNOT,
		JIT_OP_LNOT,
		0, 0, 0,
		jit_intrinsic(jit_int_not, descr_i_i),
		jit_intrinsic(jit_uint_not, descr_I_I),
		jit_intrinsic(jit_long_not, descr_l_l),
		jit_intrinsic(jit_ulong_not, descr_L_L),
		jit_no_intrinsic,
		jit_no_intrinsic,
		jit_no_intrinsic
	};
	return apply_unary_arith(func, &not_descr, value1, 1, 0, 0);
}

/*@
 * @deftypefun jit_value_t jit_insn_shl (jit_function_t func, jit_value_t value1, jit_value_t value2)
 * Perform a bitwise left shift on two values and return the
 * result in a new temporary value.
 * @end deftypefun
@*/
jit_value_t jit_insn_shl
		(jit_function_t func, jit_value_t value1, jit_value_t value2)
{
	static jit_opcode_descr const shl_descr = {
		JIT_OP_ISHL,
		JIT_OP_ISHL,
		JIT_OP_LSHL,
		JIT_OP_LSHL,
		0, 0, 0,
		jit_intrinsic(jit_int_shl, descr_i_iI),
		jit_intrinsic(jit_uint_shl, descr_I_II),
		jit_intrinsic(jit_long_shl, descr_l_lI),
		jit_intrinsic(jit_ulong_shl, descr_L_LI),
		jit_no_intrinsic,
		jit_no_intrinsic,
		jit_no_intrinsic
	};
	return apply_shift(func, &shl_descr, value1, value2);
}

/*@
 * @deftypefun jit_value_t jit_insn_shr (jit_function_t func, jit_value_t value1, jit_value_t value2)
 * Perform a bitwise right shift on two values and return the
 * result in a new temporary value.  This performs a signed shift
 * on signed operators, and an unsigned shift on unsigned operands.
 * @end deftypefun
@*/
jit_value_t jit_insn_shr
		(jit_function_t func, jit_value_t value1, jit_value_t value2)
{
	static jit_opcode_descr const shr_descr = {
		JIT_OP_ISHR,
		JIT_OP_ISHR_UN,
		JIT_OP_LSHR,
		JIT_OP_LSHR_UN,
		0, 0, 0,
		jit_intrinsic(jit_int_shr, descr_i_iI),
		jit_intrinsic(jit_uint_shr, descr_I_II),
		jit_intrinsic(jit_long_shr, descr_l_lI),
		jit_intrinsic(jit_ulong_shr, descr_L_LI),
		jit_no_intrinsic,
		jit_no_intrinsic,
		jit_no_intrinsic
	};
	return apply_shift(func, &shr_descr, value1, value2);
}

/*@
 * @deftypefun jit_value_t jit_insn_ushr (jit_function_t func, jit_value_t value1, jit_value_t value2)
 * Perform a bitwise right shift on two values and return the
 * result in a new temporary value.  This performs an unsigned
 * shift on both signed and unsigned operands.
 * @end deftypefun
@*/
jit_value_t jit_insn_ushr
		(jit_function_t func, jit_value_t value1, jit_value_t value2)
{
	static jit_opcode_descr const ushr_descr = {
		JIT_OP_ISHR_UN,
		JIT_OP_ISHR_UN,
		JIT_OP_LSHR_UN,
		JIT_OP_LSHR_UN,
		0, 0, 0,
		jit_intrinsic(jit_uint_shr, descr_I_II),
		jit_intrinsic(jit_uint_shr, descr_I_II),
		jit_intrinsic(jit_ulong_shr, descr_L_LI),
		jit_intrinsic(jit_ulong_shr, descr_L_LI),
		jit_no_intrinsic,
		jit_no_intrinsic,
		jit_no_intrinsic
	};
	return apply_shift(func, &ushr_descr, value1, value2);
}

/*@
 * @deftypefun jit_value_t jit_insn_sshr (jit_function_t func, jit_value_t value1, jit_value_t value2)
 * Perform a bitwise right shift on two values and return the
 * result in a new temporary value.  This performs an signed
 * shift on both signed and unsigned operands.
 * @end deftypefun
@*/
jit_value_t jit_insn_sshr
		(jit_function_t func, jit_value_t value1, jit_value_t value2)
{
	static jit_opcode_descr const sshr_descr = {
		JIT_OP_ISHR,
		JIT_OP_ISHR,
		JIT_OP_LSHR,
		JIT_OP_LSHR,
		0, 0, 0,
		jit_intrinsic(jit_int_shr, descr_i_iI),
		jit_intrinsic(jit_int_shr, descr_i_iI),
		jit_intrinsic(jit_long_shr, descr_l_lI),
		jit_intrinsic(jit_long_shr, descr_l_lI),
		jit_no_intrinsic,
		jit_no_intrinsic,
		jit_no_intrinsic
	};
	return apply_shift(func, &sshr_descr, value1, value2);
}

/*@
 * @deftypefun jit_value_t jit_insn_eq (jit_function_t func, jit_value_t value1, jit_value_t value2)
 * Compare two values for equality and return the result
 * in a new temporary value.
 * @end deftypefun
@*/
jit_value_t jit_insn_eq
		(jit_function_t func, jit_value_t value1, jit_value_t value2)
{
	static jit_opcode_descr const eq_descr = {
		JIT_OP_IEQ,
		JIT_OP_IEQ,
		JIT_OP_LEQ,
		JIT_OP_LEQ,
		JIT_OP_FEQ,
		JIT_OP_DEQ,
		JIT_OP_NFEQ,
		jit_intrinsic(jit_int_eq, descr_i_ii),
		jit_intrinsic(jit_uint_eq, descr_i_II),
		jit_intrinsic(jit_long_eq, descr_i_ll),
		jit_intrinsic(jit_ulong_eq, descr_i_LL),
		jit_intrinsic(jit_float32_eq, descr_i_ff),
		jit_intrinsic(jit_float64_eq, descr_i_dd),
		jit_intrinsic(jit_nfloat_eq, descr_i_DD)
	};
	return apply_compare(func, &eq_descr, value1, value2, 0);
}

/*@
 * @deftypefun jit_value_t jit_insn_ne (jit_function_t func, jit_value_t value1, jit_value_t value2)
 * Compare two values for inequality and return the result
 * in a new temporary value.
 * @end deftypefun
@*/
jit_value_t jit_insn_ne
		(jit_function_t func, jit_value_t value1, jit_value_t value2)
{
	static jit_opcode_descr const ne_descr = {
		JIT_OP_INE,
		JIT_OP_INE,
		JIT_OP_LNE,
		JIT_OP_LNE,
		JIT_OP_FNE,
		JIT_OP_DNE,
		JIT_OP_NFNE,
		jit_intrinsic(jit_int_ne, descr_i_ii),
		jit_intrinsic(jit_uint_ne, descr_i_II),
		jit_intrinsic(jit_long_ne, descr_i_ll),
		jit_intrinsic(jit_ulong_ne, descr_i_LL),
		jit_intrinsic(jit_float32_ne, descr_i_ff),
		jit_intrinsic(jit_float64_ne, descr_i_dd),
		jit_intrinsic(jit_nfloat_ne, descr_i_DD)
	};
	return apply_compare(func, &ne_descr, value1, value2, 0);
}

/*@
 * @deftypefun jit_value_t jit_insn_lt (jit_function_t func, jit_value_t value1, jit_value_t value2)
 * Compare two values for less than and return the result
 * in a new temporary value.
 * @end deftypefun
@*/
jit_value_t jit_insn_lt
		(jit_function_t func, jit_value_t value1, jit_value_t value2)
{
	static jit_opcode_descr const lt_descr = {
		JIT_OP_ILT,
		JIT_OP_ILT_UN,
		JIT_OP_LLT,
		JIT_OP_LLT_UN,
		JIT_OP_FLT,
		JIT_OP_DLT,
		JIT_OP_NFLT,
		jit_intrinsic(jit_int_lt, descr_i_ii),
		jit_intrinsic(jit_uint_lt, descr_i_II),
		jit_intrinsic(jit_long_lt, descr_i_ll),
		jit_intrinsic(jit_ulong_lt, descr_i_LL),
		jit_intrinsic(jit_float32_lt, descr_i_ff),
		jit_intrinsic(jit_float64_lt, descr_i_dd),
		jit_intrinsic(jit_nfloat_lt, descr_i_DD)
	};
	return apply_compare(func, &lt_descr, value1, value2, 0);
}

/*@
 * @deftypefun jit_value_t jit_insn_le (jit_function_t func, jit_value_t value1, jit_value_t value2)
 * Compare two values for less than or equal and return the result
 * in a new temporary value.
 * @end deftypefun
@*/
jit_value_t jit_insn_le
		(jit_function_t func, jit_value_t value1, jit_value_t value2)
{
	static jit_opcode_descr const le_descr = {
		JIT_OP_ILE,
		JIT_OP_ILE_UN,
		JIT_OP_LLE,
		JIT_OP_LLE_UN,
		JIT_OP_FLE,
		JIT_OP_DLE,
		JIT_OP_NFLE,
		jit_intrinsic(jit_int_le, descr_i_ii),
		jit_intrinsic(jit_uint_le, descr_i_II),
		jit_intrinsic(jit_long_le, descr_i_ll),
		jit_intrinsic(jit_ulong_le, descr_i_LL),
		jit_intrinsic(jit_float32_le, descr_i_ff),
		jit_intrinsic(jit_float64_le, descr_i_dd),
		jit_intrinsic(jit_nfloat_le, descr_i_DD)
	};
	return apply_compare(func, &le_descr, value1, value2, 0);
}

/*@
 * @deftypefun jit_value_t jit_insn_gt (jit_function_t func, jit_value_t value1, jit_value_t value2)
 * Compare two values for greater than and return the result
 * in a new temporary value.
 * @end deftypefun
@*/
jit_value_t jit_insn_gt
		(jit_function_t func, jit_value_t value1, jit_value_t value2)
{
	static jit_opcode_descr const gt_descr = {
		JIT_OP_IGT,
		JIT_OP_IGT_UN,
		JIT_OP_LGT,
		JIT_OP_LGT_UN,
		JIT_OP_FGT,
		JIT_OP_DGT,
		JIT_OP_NFGT,
		jit_intrinsic(jit_int_gt, descr_i_ii),
		jit_intrinsic(jit_uint_gt, descr_i_II),
		jit_intrinsic(jit_long_gt, descr_i_ll),
		jit_intrinsic(jit_ulong_gt, descr_i_LL),
		jit_intrinsic(jit_float32_gt, descr_i_ff),
		jit_intrinsic(jit_float64_gt, descr_i_dd),
		jit_intrinsic(jit_nfloat_gt, descr_i_DD)
	};
	return apply_compare(func, &gt_descr, value1, value2, 0);
}

/*@
 * @deftypefun jit_value_t jit_insn_ge (jit_function_t func, jit_value_t value1, jit_value_t value2)
 * Compare two values for greater than or equal and return the result
 * in a new temporary value.
 * @end deftypefun
@*/
jit_value_t jit_insn_ge
		(jit_function_t func, jit_value_t value1, jit_value_t value2)
{
	static jit_opcode_descr const ge_descr = {
		JIT_OP_IGE,
		JIT_OP_IGE_UN,
		JIT_OP_LGE,
		JIT_OP_LGE_UN,
		JIT_OP_FGE,
		JIT_OP_DGE,
		JIT_OP_NFGE,
		jit_intrinsic(jit_int_ge, descr_i_ii),
		jit_intrinsic(jit_uint_ge, descr_i_II),
		jit_intrinsic(jit_long_ge, descr_i_ll),
		jit_intrinsic(jit_ulong_ge, descr_i_LL),
		jit_intrinsic(jit_float32_ge, descr_i_ff),
		jit_intrinsic(jit_float64_ge, descr_i_dd),
		jit_intrinsic(jit_nfloat_ge, descr_i_DD)
	};
	return apply_compare(func, &ge_descr, value1, value2, 0);
}

/*@
 * @deftypefun jit_value_t jit_insn_cmpl (jit_function_t func, jit_value_t value1, jit_value_t value2)
 * Compare two values, and return a -1, 0, or 1 result.  If either
 * value is "not a number", then -1 is returned.
 * @end deftypefun
@*/
jit_value_t jit_insn_cmpl
		(jit_function_t func, jit_value_t value1, jit_value_t value2)
{
	static jit_opcode_descr const cmpl_descr = {
		JIT_OP_ICMP,
		JIT_OP_ICMP_UN,
		JIT_OP_LCMP,
		JIT_OP_LCMP_UN,
		JIT_OP_FCMPL,
		JIT_OP_DCMPL,
		JIT_OP_NFCMPL,
		jit_intrinsic(jit_int_cmp, descr_i_ii),
		jit_intrinsic(jit_uint_cmp, descr_i_II),
		jit_intrinsic(jit_long_cmp, descr_i_ll),
		jit_intrinsic(jit_ulong_cmp, descr_i_LL),
		jit_intrinsic(jit_float32_cmpl, descr_i_ff),
		jit_intrinsic(jit_float64_cmpl, descr_i_dd),
		jit_intrinsic(jit_nfloat_cmpl, descr_i_DD)
	};
	return apply_compare(func, &cmpl_descr, value1, value2, 0);
}

/*@
 * @deftypefun jit_value_t jit_insn_cmpg (jit_function_t func, jit_value_t value1, jit_value_t value2)
 * Compare two values, and return a -1, 0, or 1 result.  If either
 * value is "not a number", then 1 is returned.
 * @end deftypefun
@*/
jit_value_t jit_insn_cmpg
		(jit_function_t func, jit_value_t value1, jit_value_t value2)
{
	static jit_opcode_descr const cmpg_descr = {
		JIT_OP_ICMP,
		JIT_OP_ICMP_UN,
		JIT_OP_LCMP,
		JIT_OP_LCMP_UN,
		JIT_OP_FCMPG,
		JIT_OP_DCMPG,
		JIT_OP_NFCMPG,
		jit_intrinsic(jit_int_cmp, descr_i_ii),
		jit_intrinsic(jit_uint_cmp, descr_i_II),
		jit_intrinsic(jit_long_cmp, descr_i_ll),
		jit_intrinsic(jit_ulong_cmp, descr_i_LL),
		jit_intrinsic(jit_float32_cmpg, descr_i_ff),
		jit_intrinsic(jit_float64_cmpg, descr_i_dd),
		jit_intrinsic(jit_nfloat_cmpg, descr_i_DD)
	};
	return apply_compare(func, &cmpg_descr, value1, value2, 0);
}

/*@
 * @deftypefun jit_value_t jit_insn_to_bool (jit_function_t func, jit_value_t value1)
 * Convert a value into a boolean 0 or 1 result of type @code{jit_type_int}.
 * @end deftypefun
@*/
jit_value_t jit_insn_to_bool(jit_function_t func, jit_value_t value1)
{
	jit_type_t type;
	jit_block_t block;
	jit_insn_t last;
	int opcode;

	/* Bail out if the parameters are invalid */
	if(!value1)
	{
		return 0;
	}

	/* Ensure that we have a builder for this function */
	if(!_jit_function_ensure_builder(func))
	{
		return 0;
	}

	/* If the previous instruction was a comparison, then there is
	   nothing that we need to do to make the value boolean */
	block = func->builder->current_block;
	last = _jit_block_get_last(block);
	if(value1->is_temporary && last && last->dest == value1)
	{
		opcode = last->opcode;
		if(opcode >= JIT_OP_IEQ && opcode <= JIT_OP_NFGE_INV)
		{
			return value1;
		}
	}

	/* Perform a comparison to determine if the value is non-zero */
	type = jit_type_promote_int(jit_type_normalize(value1->type));
	if(type == jit_type_int || type == jit_type_uint)
	{
		return jit_insn_ne
			(func, value1,
			 jit_value_create_nint_constant(func, jit_type_int, 0));
	}
	else if(type == jit_type_long || type == jit_type_ulong)
	{
		return jit_insn_ne
			(func, value1,
			 jit_value_create_long_constant(func, jit_type_long, 0));
	}
	else if(type == jit_type_float32)
	{
		return jit_insn_ne
			(func, value1,
			 jit_value_create_float32_constant
			 	(func, jit_type_float32, (jit_float32)0.0));
	}
	else if(type == jit_type_float64)
	{
		return jit_insn_ne
			(func, value1,
			 jit_value_create_float64_constant
			 	(func, jit_type_float64, (jit_float64)0.0));
	}
	else
	{
		return jit_insn_ne
			(func, value1,
			 jit_value_create_nfloat_constant
			 	(func, jit_type_nfloat, (jit_nfloat)0.0));
	}
}

/*@
 * @deftypefun jit_value_t jit_insn_to_not_bool (jit_function_t func, jit_value_t value1)
 * Convert a value into a boolean 1 or 0 result of type @code{jit_type_int}
 * (i.e. the inverse of @code{jit_insn_to_bool}).
 * @end deftypefun
@*/
jit_value_t jit_insn_to_not_bool(jit_function_t func, jit_value_t value1)
{
	jit_type_t type;
	jit_block_t block;
	jit_insn_t last;
	int opcode;

	/* Bail out if the parameters are invalid */
	if(!value1)
	{
		return 0;
	}

	/* Ensure that we have a builder for this function */
	if(!_jit_function_ensure_builder(func))
	{
		return 0;
	}

	/* If the previous instruction was a comparison, then all
	   we have to do is invert the comparison opcode */
	block = func->builder->current_block;
	last = _jit_block_get_last(block);
	if(value1->is_temporary && last && last->dest == value1)
	{
		opcode = last->opcode;
		if(opcode >= JIT_OP_IEQ && opcode <= JIT_OP_NFGE_INV)
		{
			switch(opcode)
			{
				case JIT_OP_IEQ:		opcode = JIT_OP_INE;      break;
				case JIT_OP_INE:		opcode = JIT_OP_IEQ;      break;
				case JIT_OP_ILT:		opcode = JIT_OP_IGE;      break;
				case JIT_OP_ILT_UN:		opcode = JIT_OP_IGE_UN;   break;
				case JIT_OP_ILE:		opcode = JIT_OP_IGT;      break;
				case JIT_OP_ILE_UN:		opcode = JIT_OP_IGT_UN;   break;
				case JIT_OP_IGT:		opcode = JIT_OP_ILE;      break;
				case JIT_OP_IGT_UN:		opcode = JIT_OP_ILE_UN;   break;
				case JIT_OP_IGE:		opcode = JIT_OP_ILT;      break;
				case JIT_OP_IGE_UN:		opcode = JIT_OP_ILT_UN;   break;
				case JIT_OP_LEQ:		opcode = JIT_OP_LNE;      break;
				case JIT_OP_LNE:		opcode = JIT_OP_LEQ;      break;
				case JIT_OP_LLT:		opcode = JIT_OP_LGE;      break;
				case JIT_OP_LLT_UN:		opcode = JIT_OP_LGE_UN;   break;
				case JIT_OP_LLE:		opcode = JIT_OP_LGT;      break;
				case JIT_OP_LLE_UN:		opcode = JIT_OP_LGT_UN;   break;
				case JIT_OP_LGT:		opcode = JIT_OP_LLE;      break;
				case JIT_OP_LGT_UN:		opcode = JIT_OP_LLE_UN;   break;
				case JIT_OP_LGE:		opcode = JIT_OP_LLT;      break;
				case JIT_OP_LGE_UN:		opcode = JIT_OP_LLT_UN;   break;
				case JIT_OP_FEQ:		opcode = JIT_OP_FNE_INV;  break;
				case JIT_OP_FNE:		opcode = JIT_OP_FEQ_INV;  break;
				case JIT_OP_FLT:		opcode = JIT_OP_FGE_INV;  break;
				case JIT_OP_FLE:		opcode = JIT_OP_FGT_INV;  break;
				case JIT_OP_FGT:		opcode = JIT_OP_FLE_INV;  break;
				case JIT_OP_FGE:		opcode = JIT_OP_FLT_INV;  break;
				case JIT_OP_FEQ_INV:	opcode = JIT_OP_FNE;      break;
				case JIT_OP_FNE_INV:	opcode = JIT_OP_FEQ;      break;
				case JIT_OP_FLT_INV:	opcode = JIT_OP_FGE;      break;
				case JIT_OP_FLE_INV:	opcode = JIT_OP_FGT;      break;
				case JIT_OP_FGT_INV:	opcode = JIT_OP_FLE;      break;
				case JIT_OP_FGE_INV:	opcode = JIT_OP_FLT;      break;
				case JIT_OP_DEQ:		opcode = JIT_OP_DNE_INV;  break;
				case JIT_OP_DNE:		opcode = JIT_OP_DEQ_INV;  break;
				case JIT_OP_DLT:		opcode = JIT_OP_DGE_INV;  break;
				case JIT_OP_DLE:		opcode = JIT_OP_DGT_INV;  break;
				case JIT_OP_DGT:		opcode = JIT_OP_DLE_INV;  break;
				case JIT_OP_DGE:		opcode = JIT_OP_DLT_INV;  break;
				case JIT_OP_DEQ_INV:	opcode = JIT_OP_DNE;      break;
				case JIT_OP_DNE_INV:	opcode = JIT_OP_DEQ;      break;
				case JIT_OP_DLT_INV:	opcode = JIT_OP_DGE;      break;
				case JIT_OP_DLE_INV:	opcode = JIT_OP_DGT;      break;
				case JIT_OP_DGT_INV:	opcode = JIT_OP_DLE;      break;
				case JIT_OP_DGE_INV:	opcode = JIT_OP_DLT;      break;
				case JIT_OP_NFEQ:		opcode = JIT_OP_NFNE_INV; break;
				case JIT_OP_NFNE:		opcode = JIT_OP_NFEQ_INV; break;
				case JIT_OP_NFLT:		opcode = JIT_OP_NFGE_INV; break;
				case JIT_OP_NFLE:		opcode = JIT_OP_NFGT_INV; break;
				case JIT_OP_NFGT:		opcode = JIT_OP_NFLE_INV; break;
				case JIT_OP_NFGE:		opcode = JIT_OP_NFLT_INV; break;
				case JIT_OP_NFEQ_INV:	opcode = JIT_OP_NFNE;     break;
				case JIT_OP_NFNE_INV:	opcode = JIT_OP_NFEQ;     break;
				case JIT_OP_NFLT_INV:	opcode = JIT_OP_NFGE;     break;
				case JIT_OP_NFLE_INV:	opcode = JIT_OP_NFGT;     break;
				case JIT_OP_NFGT_INV:	opcode = JIT_OP_NFLE;     break;
				case JIT_OP_NFGE_INV:	opcode = JIT_OP_NFLT;     break;
			}
			last->opcode = (short)opcode;
			return value1;
		}
	}

	/* Perform a comparison to determine if the value is zero */
	type = jit_type_promote_int(jit_type_normalize(value1->type));
	if(type == jit_type_int || type == jit_type_uint)
	{
		return jit_insn_eq
			(func, value1,
			 jit_value_create_nint_constant(func, jit_type_int, 0));
	}
	else if(type == jit_type_long || type == jit_type_ulong)
	{
		return jit_insn_eq
			(func, value1,
			 jit_value_create_long_constant(func, jit_type_long, 0));
	}
	else if(type == jit_type_float32)
	{
		return jit_insn_eq
			(func, value1,
			 jit_value_create_float32_constant
			 	(func, jit_type_float32, (jit_float32)0.0));
	}
	else if(type == jit_type_float64)
	{
		return jit_insn_eq
			(func, value1,
			 jit_value_create_float64_constant
			 	(func, jit_type_float64, (jit_float64)0.0));
	}
	else
	{
		return jit_insn_eq
			(func, value1,
			 jit_value_create_nfloat_constant
			 	(func, jit_type_nfloat, (jit_nfloat)0.0));
	}
}

/*@
 * @deftypefun jit_value_t jit_insn_acos (jit_function_t func, jit_value_t value1)
 * @deftypefunx jit_value_t jit_insn_asin (jit_function_t func, jit_value_t value1)
 * @deftypefunx jit_value_t jit_insn_atan (jit_function_t func, jit_value_t value1)
 * @deftypefunx jit_value_t jit_insn_atan2 (jit_function_t func, jit_value_t value1, jit_value_t value2)
 * @deftypefunx jit_value_t jit_insn_ceil (jit_function_t func, jit_value_t value1)
 * @deftypefunx jit_value_t jit_insn_cos (jit_function_t func, jit_value_t value1)
 * @deftypefunx jit_value_t jit_insn_cosh (jit_function_t func, jit_value_t value1)
 * @deftypefunx jit_value_t jit_insn_exp (jit_function_t func, jit_value_t value1)
 * @deftypefunx jit_value_t jit_insn_floor (jit_function_t func, jit_value_t value1)
 * @deftypefunx jit_value_t jit_insn_log (jit_function_t func, jit_value_t value1)
 * @deftypefunx jit_value_t jit_insn_log10 (jit_function_t func, jit_value_t value1)
 * @deftypefunx jit_value_t jit_insn_pow (jit_function_t func, jit_value_t value1, jit_value_t value2)
 * @deftypefunx jit_value_t jit_insn_rint (jit_function_t func, jit_value_t value1)
 * @deftypefunx jit_value_t jit_insn_round (jit_function_t func, jit_value_t value1)
 * @deftypefunx jit_value_t jit_insn_sin (jit_function_t func, jit_value_t value1)
 * @deftypefunx jit_value_t jit_insn_sinh (jit_function_t func, jit_value_t value1)
 * @deftypefunx jit_value_t jit_insn_sqrt (jit_function_t func, jit_value_t value1)
 * @deftypefunx jit_value_t jit_insn_tan (jit_function_t func, jit_value_t value1)
 * @deftypefunx jit_value_t jit_insn_tanh (jit_function_t func, jit_value_t value1)
 * Apply a mathematical function to floating-point arguments.
 * @end deftypefun
@*/
jit_value_t jit_insn_acos(jit_function_t func, jit_value_t value1)
{
	static jit_opcode_descr const acos_descr = {
		0, 0, 0, 0,
		JIT_OP_FACOS,
		JIT_OP_DACOS,
		JIT_OP_NFACOS,
		jit_no_intrinsic,
		jit_no_intrinsic,
		jit_no_intrinsic,
		jit_no_intrinsic,
		jit_intrinsic(jit_float32_acos, descr_f_f),
		jit_intrinsic(jit_float64_acos, descr_d_d),
		jit_intrinsic(jit_nfloat_acos, descr_D_D)
	};
	return apply_unary_arith(func, &acos_descr, value1, 0, 1, 0);
}

jit_value_t jit_insn_asin(jit_function_t func, jit_value_t value1)
{
	static jit_opcode_descr const asin_descr = {
		0, 0, 0, 0,
		JIT_OP_FASIN,
		JIT_OP_DASIN,
		JIT_OP_NFASIN,
		jit_no_intrinsic,
		jit_no_intrinsic,
		jit_no_intrinsic,
		jit_no_intrinsic,
		jit_intrinsic(jit_float32_asin, descr_f_f),
		jit_intrinsic(jit_float64_asin, descr_d_d),
		jit_intrinsic(jit_nfloat_asin, descr_D_D)
	};
	return apply_unary_arith(func, &asin_descr, value1, 0, 1, 0);
}

jit_value_t jit_insn_atan(jit_function_t func, jit_value_t value1)
{
	static jit_opcode_descr const atan_descr = {
		0, 0, 0, 0,
		JIT_OP_FATAN,
		JIT_OP_DATAN,
		JIT_OP_NFATAN,
		jit_no_intrinsic,
		jit_no_intrinsic,
		jit_no_intrinsic,
		jit_no_intrinsic,
		jit_intrinsic(jit_float32_atan, descr_f_f),
		jit_intrinsic(jit_float64_atan, descr_d_d),
		jit_intrinsic(jit_nfloat_atan, descr_D_D)
	};
	return apply_unary_arith(func, &atan_descr, value1, 0, 1, 0);
}

jit_value_t jit_insn_atan2
		(jit_function_t func, jit_value_t value1, jit_value_t value2)
{
	static jit_opcode_descr const atan2_descr = {
		0, 0, 0, 0,
		JIT_OP_FATAN2,
		JIT_OP_DATAN2,
		JIT_OP_NFATAN2,
		jit_no_intrinsic,
		jit_no_intrinsic,
		jit_no_intrinsic,
		jit_no_intrinsic,
		jit_intrinsic(jit_float32_atan2, descr_f_ff),
		jit_intrinsic(jit_float64_atan2, descr_d_dd),
		jit_intrinsic(jit_nfloat_atan2, descr_D_DD)
	};
	return apply_arith(func, &atan2_descr, value1, value2, 0, 1, 0);
}

jit_value_t jit_insn_ceil(jit_function_t func, jit_value_t value1)
{
	static jit_opcode_descr const ceil_descr = {
		0, 0, 0, 0,
		JIT_OP_FCEIL,
		JIT_OP_DCEIL,
		JIT_OP_NFCEIL,
		jit_no_intrinsic,
		jit_no_intrinsic,
		jit_no_intrinsic,
		jit_no_intrinsic,
		jit_intrinsic(jit_float32_ceil, descr_f_f),
		jit_intrinsic(jit_float64_ceil, descr_d_d),
		jit_intrinsic(jit_nfloat_ceil, descr_D_D)
	};
	return apply_unary_arith(func, &ceil_descr, value1, 0, 1, 0);
}

jit_value_t jit_insn_cos(jit_function_t func, jit_value_t value1)
{
	static jit_opcode_descr const cos_descr = {
		0, 0, 0, 0,
		JIT_OP_FCOS,
		JIT_OP_DCOS,
		JIT_OP_NFCOS,
		jit_no_intrinsic,
		jit_no_intrinsic,
		jit_no_intrinsic,
		jit_no_intrinsic,
		jit_intrinsic(jit_float32_cos, descr_f_f),
		jit_intrinsic(jit_float64_cos, descr_d_d),
		jit_intrinsic(jit_nfloat_cos, descr_D_D)
	};
	return apply_unary_arith(func, &cos_descr, value1, 0, 1, 0);
}

jit_value_t jit_insn_cosh(jit_function_t func, jit_value_t value1)
{
	static jit_opcode_descr const cosh_descr = {
		0, 0, 0, 0,
		JIT_OP_FCOSH,
		JIT_OP_DCOSH,
		JIT_OP_NFCOSH,
		jit_no_intrinsic,
		jit_no_intrinsic,
		jit_no_intrinsic,
		jit_no_intrinsic,
		jit_intrinsic(jit_float32_cosh, descr_f_f),
		jit_intrinsic(jit_float64_cosh, descr_d_d),
		jit_intrinsic(jit_nfloat_cosh, descr_D_D)
	};
	return apply_unary_arith(func, &cosh_descr, value1, 0, 1, 0);
}

jit_value_t jit_insn_exp(jit_function_t func, jit_value_t value1)
{
	static jit_opcode_descr const exp_descr = {
		0, 0, 0, 0,
		JIT_OP_FEXP,
		JIT_OP_DEXP,
		JIT_OP_NFEXP,
		jit_no_intrinsic,
		jit_no_intrinsic,
		jit_no_intrinsic,
		jit_no_intrinsic,
		jit_intrinsic(jit_float32_exp, descr_f_f),
		jit_intrinsic(jit_float64_exp, descr_d_d),
		jit_intrinsic(jit_nfloat_exp, descr_D_D)
	};
	return apply_unary_arith(func, &exp_descr, value1, 0, 1, 0);
}

jit_value_t jit_insn_floor(jit_function_t func, jit_value_t value1)
{
	static jit_opcode_descr const floor_descr = {
		0, 0, 0, 0,
		JIT_OP_FFLOOR,
		JIT_OP_DFLOOR,
		JIT_OP_NFFLOOR,
		jit_no_intrinsic,
		jit_no_intrinsic,
		jit_no_intrinsic,
		jit_no_intrinsic,
		jit_intrinsic(jit_float32_floor, descr_f_f),
		jit_intrinsic(jit_float64_floor, descr_d_d),
		jit_intrinsic(jit_nfloat_floor, descr_D_D)
	};
	return apply_unary_arith(func, &floor_descr, value1, 0, 1, 0);
}

jit_value_t jit_insn_log(jit_function_t func, jit_value_t value1)
{
	static jit_opcode_descr const log_descr = {
		0, 0, 0, 0,
		JIT_OP_FLOG,
		JIT_OP_DLOG,
		JIT_OP_NFLOG,
		jit_no_intrinsic,
		jit_no_intrinsic,
		jit_no_intrinsic,
		jit_no_intrinsic,
		jit_intrinsic(jit_float32_log, descr_f_f),
		jit_intrinsic(jit_float64_log, descr_d_d),
		jit_intrinsic(jit_nfloat_log, descr_D_D)
	};
	return apply_unary_arith(func, &log_descr, value1, 0, 1, 0);
}

jit_value_t jit_insn_log10(jit_function_t func, jit_value_t value1)
{
	static jit_opcode_descr const log10_descr = {
		0, 0, 0, 0,
		JIT_OP_FLOG10,
		JIT_OP_DLOG10,
		JIT_OP_NFLOG10,
		jit_no_intrinsic,
		jit_no_intrinsic,
		jit_no_intrinsic,
		jit_no_intrinsic,
		jit_intrinsic(jit_float32_log10, descr_f_f),
		jit_intrinsic(jit_float64_log10, descr_d_d),
		jit_intrinsic(jit_nfloat_log10, descr_D_D)
	};
	return apply_unary_arith(func, &log10_descr, value1, 0, 1, 0);
}

jit_value_t jit_insn_pow
		(jit_function_t func, jit_value_t value1, jit_value_t value2)
{
	static jit_opcode_descr const pow_descr = {
		0, 0, 0, 0,
		JIT_OP_FPOW,
		JIT_OP_DPOW,
		JIT_OP_NFPOW,
		jit_no_intrinsic,
		jit_no_intrinsic,
		jit_no_intrinsic,
		jit_no_intrinsic,
		jit_intrinsic(jit_float32_pow, descr_f_ff),
		jit_intrinsic(jit_float64_pow, descr_d_dd),
		jit_intrinsic(jit_nfloat_pow, descr_D_DD)
	};
	return apply_arith(func, &pow_descr, value1, value2, 0, 1, 0);
}

jit_value_t jit_insn_rint(jit_function_t func, jit_value_t value1)
{
	static jit_opcode_descr const rint_descr = {
		0, 0, 0, 0,
		JIT_OP_FRINT,
		JIT_OP_DRINT,
		JIT_OP_NFRINT,
		jit_no_intrinsic,
		jit_no_intrinsic,
		jit_no_intrinsic,
		jit_no_intrinsic,
		jit_intrinsic(jit_float32_rint, descr_f_f),
		jit_intrinsic(jit_float64_rint, descr_d_d),
		jit_intrinsic(jit_nfloat_rint, descr_D_D)
	};
	return apply_unary_arith(func, &rint_descr, value1, 0, 1, 0);
}

jit_value_t jit_insn_round(jit_function_t func, jit_value_t value1)
{
	static jit_opcode_descr const round_descr = {
		0, 0, 0, 0,
		JIT_OP_FROUND,
		JIT_OP_DROUND,
		JIT_OP_NFROUND,
		jit_no_intrinsic,
		jit_no_intrinsic,
		jit_no_intrinsic,
		jit_no_intrinsic,
		jit_intrinsic(jit_float32_round, descr_f_f),
		jit_intrinsic(jit_float64_round, descr_d_d),
		jit_intrinsic(jit_nfloat_round, descr_D_D)
	};
	return apply_unary_arith(func, &round_descr, value1, 0, 1, 0);
}

jit_value_t jit_insn_sin(jit_function_t func, jit_value_t value1)
{
	static jit_opcode_descr const sin_descr = {
		0, 0, 0, 0,
		JIT_OP_FSIN,
		JIT_OP_DSIN,
		JIT_OP_NFSIN,
		jit_no_intrinsic,
		jit_no_intrinsic,
		jit_no_intrinsic,
		jit_no_intrinsic,
		jit_intrinsic(jit_float32_sin, descr_f_f),
		jit_intrinsic(jit_float64_sin, descr_d_d),
		jit_intrinsic(jit_nfloat_sin, descr_D_D)
	};
	return apply_unary_arith(func, &sin_descr, value1, 0, 1, 0);
}

jit_value_t jit_insn_sinh(jit_function_t func, jit_value_t value1)
{
	static jit_opcode_descr const sinh_descr = {
		0, 0, 0, 0,
		JIT_OP_FSINH,
		JIT_OP_DSINH,
		JIT_OP_NFSINH,
		jit_no_intrinsic,
		jit_no_intrinsic,
		jit_no_intrinsic,
		jit_no_intrinsic,
		jit_intrinsic(jit_float32_sinh, descr_f_f),
		jit_intrinsic(jit_float64_sinh, descr_d_d),
		jit_intrinsic(jit_nfloat_sinh, descr_D_D)
	};
	return apply_unary_arith(func, &sinh_descr, value1, 0, 1, 0);
}

jit_value_t jit_insn_sqrt(jit_function_t func, jit_value_t value1)
{
	static jit_opcode_descr const sqrt_descr = {
		0, 0, 0, 0,
		JIT_OP_FSQRT,
		JIT_OP_DSQRT,
		JIT_OP_NFSQRT,
		jit_no_intrinsic,
		jit_no_intrinsic,
		jit_no_intrinsic,
		jit_no_intrinsic,
		jit_intrinsic(jit_float32_sqrt, descr_f_f),
		jit_intrinsic(jit_float64_sqrt, descr_d_d),
		jit_intrinsic(jit_nfloat_sqrt, descr_D_D)
	};
	return apply_unary_arith(func, &sqrt_descr, value1, 0, 1, 0);
}

jit_value_t jit_insn_tan(jit_function_t func, jit_value_t value1)
{
	static jit_opcode_descr const tan_descr = {
		0, 0, 0, 0,
		JIT_OP_FTAN,
		JIT_OP_DTAN,
		JIT_OP_NFTAN,
		jit_no_intrinsic,
		jit_no_intrinsic,
		jit_no_intrinsic,
		jit_no_intrinsic,
		jit_intrinsic(jit_float32_tan, descr_f_f),
		jit_intrinsic(jit_float64_tan, descr_d_d),
		jit_intrinsic(jit_nfloat_tan, descr_D_D)
	};
	return apply_unary_arith(func, &tan_descr, value1, 0, 1, 0);
}

jit_value_t jit_insn_tanh(jit_function_t func, jit_value_t value1)
{
	static jit_opcode_descr const tanh_descr = {
		0, 0, 0, 0,
		JIT_OP_FTANH,
		JIT_OP_DTANH,
		JIT_OP_NFTANH,
		jit_no_intrinsic,
		jit_no_intrinsic,
		jit_no_intrinsic,
		jit_no_intrinsic,
		jit_intrinsic(jit_float32_tanh, descr_f_f),
		jit_intrinsic(jit_float64_tanh, descr_d_d),
		jit_intrinsic(jit_nfloat_tanh, descr_D_D)
	};
	return apply_unary_arith(func, &tanh_descr, value1, 0, 1, 0);
}

/*@
 * @deftypefun jit_value_t jit_insn_is_nan (jit_function_t func, jit_value_t value1)
 * @deftypefunx jit_value_t jit_insn_is_finite (jit_function_t func, jit_value_t value1)
 * @deftypefunx jit_value_t jit_insn_is_inf (jit_function_t func, jit_value_t value1)
 * Test a floating point value for not a number, finite, or infinity.
 * @end deftypefun
@*/
jit_value_t jit_insn_is_nan(jit_function_t func, jit_value_t value1)
{
	static jit_opcode_descr const is_nan_descr = {
		0, 0, 0, 0,
		JIT_OP_IS_FNAN,
		JIT_OP_IS_DNAN,
		JIT_OP_IS_NFNAN,
		jit_no_intrinsic,
		jit_no_intrinsic,
		jit_no_intrinsic,
		jit_no_intrinsic,
		jit_intrinsic(jit_float32_is_nan, descr_i_f),
		jit_intrinsic(jit_float64_is_nan, descr_i_d),
		jit_intrinsic(jit_nfloat_is_nan, descr_i_D)
	};
	return apply_test(func, &is_nan_descr, value1, 1);
}

jit_value_t jit_insn_is_finite(jit_function_t func, jit_value_t value1)
{
	static jit_opcode_descr const is_finite_descr = {
		0, 0, 0, 0,
		JIT_OP_IS_FFINITE,
		JIT_OP_IS_DFINITE,
		JIT_OP_IS_NFFINITE,
		jit_no_intrinsic,
		jit_no_intrinsic,
		jit_no_intrinsic,
		jit_no_intrinsic,
		jit_intrinsic(jit_float32_is_finite, descr_i_f),
		jit_intrinsic(jit_float64_is_finite, descr_i_d),
		jit_intrinsic(jit_nfloat_is_finite, descr_i_D)
	};
	return apply_test(func, &is_finite_descr, value1, 1);
}

jit_value_t jit_insn_is_inf(jit_function_t func, jit_value_t value1)
{
	static jit_opcode_descr const is_inf_descr = {
		0, 0, 0, 0,
		JIT_OP_IS_FINF,
		JIT_OP_IS_DINF,
		JIT_OP_IS_NFINF,
		jit_no_intrinsic,
		jit_no_intrinsic,
		jit_no_intrinsic,
		jit_no_intrinsic,
		jit_intrinsic(jit_float32_is_inf, descr_i_f),
		jit_intrinsic(jit_float64_is_inf, descr_i_d),
		jit_intrinsic(jit_nfloat_is_inf, descr_i_D)
	};
	return apply_test(func, &is_inf_descr, value1, 1);
}

/*@
 * @deftypefun jit_value_t jit_insn_abs (jit_function_t func, jit_value_t value1)
 * @deftypefunx jit_value_t jit_insn_min (jit_function_t func, jit_value_t value1, jit_value_t value2)
 * @deftypefunx jit_value_t jit_insn_max (jit_function_t func, jit_value_t value1, jit_value_t value2)
 * @deftypefunx jit_value_t jit_insn_sign (jit_function_t func, jit_value_t value1)
 * Calculate the absolute value, minimum, maximum, or sign of the
 * specified values.
 * @end deftypefun
@*/
jit_value_t jit_insn_abs(jit_function_t func, jit_value_t value1)
{
	int oper;
	void *intrinsic;
	const char *name;
	jit_type_t result_type;
	const jit_intrinsic_descr_t *descr;
	if(!value1)
	{
		return 0;
	}
	result_type = common_binary(value1->type, value1->type, 0, 0);
	if(result_type == jit_type_int)
	{
		oper = JIT_OP_IABS;
		intrinsic = (void *)jit_int_abs;
		name = "jit_int_abs";
		descr = &descr_i_i;
	}
	else if(result_type == jit_type_uint)
	{
		oper = 0;
		intrinsic = (void *)0;
		name = 0;
		descr = 0;
	}
	else if(result_type == jit_type_long)
	{
		oper = JIT_OP_LABS;
		intrinsic = (void *)jit_long_abs;
		name = "jit_long_abs";
		descr = &descr_l_l;
	}
	else if(result_type == jit_type_ulong)
	{
		oper = 0;
		intrinsic = (void *)0;
		name = 0;
		descr = 0;
	}
	else if(result_type == jit_type_float32)
	{
		oper = JIT_OP_FABS;
		intrinsic = (void *)jit_float32_abs;
		name = "jit_float32_abs";
		descr = &descr_f_f;
	}
	else if(result_type == jit_type_float64)
	{
		oper = JIT_OP_DABS;
		intrinsic = (void *)jit_float64_abs;
		name = "jit_float64_abs";
		descr = &descr_d_d;
	}
	else
	{
		oper = JIT_OP_NFABS;
		intrinsic = (void *)jit_nfloat_abs;
		name = "jit_nfloat_abs";
		descr = &descr_D_D;
	}
	value1 = jit_insn_convert(func, value1, result_type, 0);
	if(!oper)
	{
		/* Absolute value of an unsigned value */
		return value1;
	}
	if(_jit_opcode_is_supported(oper))
	{
		return apply_unary(func, oper, value1, result_type);
	}
	else
	{
		return jit_insn_call_intrinsic
			(func, name, intrinsic, descr, value1, 0);
	}
}

jit_value_t jit_insn_min
		(jit_function_t func, jit_value_t value1, jit_value_t value2)
{
	static jit_opcode_descr const min_descr = {
		JIT_OP_IMIN,
		JIT_OP_IMIN_UN,
		JIT_OP_LMIN,
		JIT_OP_LMIN_UN,
		JIT_OP_FMIN,
		JIT_OP_DMIN,
		JIT_OP_NFMIN,
		jit_intrinsic(jit_int_min, descr_i_ii),
		jit_intrinsic(jit_uint_min, descr_I_II),
		jit_intrinsic(jit_long_min, descr_l_ll),
		jit_intrinsic(jit_ulong_min, descr_L_LL),
		jit_intrinsic(jit_float32_min, descr_f_ff),
		jit_intrinsic(jit_float64_min, descr_d_dd),
		jit_intrinsic(jit_nfloat_min, descr_D_DD)
	};
	return apply_arith(func, &min_descr, value1, value2, 0, 0, 0);
}

jit_value_t jit_insn_max
		(jit_function_t func, jit_value_t value1, jit_value_t value2)
{
	static jit_opcode_descr const max_descr = {
		JIT_OP_IMAX,
		JIT_OP_IMAX_UN,
		JIT_OP_LMAX,
		JIT_OP_LMAX_UN,
		JIT_OP_FMAX,
		JIT_OP_DMAX,
		JIT_OP_NFMAX,
		jit_intrinsic(jit_int_max, descr_i_ii),
		jit_intrinsic(jit_uint_max, descr_I_II),
		jit_intrinsic(jit_long_max, descr_l_ll),
		jit_intrinsic(jit_ulong_max, descr_L_LL),
		jit_intrinsic(jit_float32_max, descr_f_ff),
		jit_intrinsic(jit_float64_max, descr_d_dd),
		jit_intrinsic(jit_nfloat_max, descr_D_DD)
	};
	return apply_arith(func, &max_descr, value1, value2, 0, 0, 0);
}

jit_value_t jit_insn_sign(jit_function_t func, jit_value_t value1)
{
	int oper;
	void *intrinsic;
	const char *name;
	jit_type_t result_type;
	const jit_intrinsic_descr_t *descr;
	if(!value1)
	{
		return 0;
	}
	result_type = common_binary(value1->type, value1->type, 0, 0);
	if(result_type == jit_type_int)
	{
		oper = JIT_OP_ISIGN;
		intrinsic = (void *)jit_int_sign;
		name = "jit_int_sign";
		descr = &descr_i_i;
	}
	else if(result_type == jit_type_uint)
	{
		return jit_insn_ne
			(func, value1,
			 jit_value_create_nint_constant(func, jit_type_uint, 0));
	}
	else if(result_type == jit_type_long)
	{
		oper = JIT_OP_LSIGN;
		intrinsic = (void *)jit_long_sign;
		name = "jit_long_sign";
		descr = &descr_i_l;
	}
	else if(result_type == jit_type_ulong)
	{
		return jit_insn_ne
			(func, value1,
			 jit_value_create_long_constant(func, jit_type_ulong, 0));
	}
	else if(result_type == jit_type_float32)
	{
		oper = JIT_OP_FSIGN;
		intrinsic = (void *)jit_float32_sign;
		name = "jit_float32_sign";
		descr = &descr_i_f;
	}
	else if(result_type == jit_type_float64)
	{
		oper = JIT_OP_DSIGN;
		intrinsic = (void *)jit_float64_sign;
		name = "jit_float64_sign";
		descr = &descr_i_d;
	}
	else
	{
		oper = JIT_OP_NFSIGN;
		intrinsic = (void *)jit_nfloat_sign;
		name = "jit_nfloat_sign";
		descr = &descr_i_D;
	}
	value1 = jit_insn_convert(func, value1, result_type, 0);
	if(_jit_opcode_is_supported(oper))
	{
		return apply_unary(func, oper, value1, result_type);
	}
	else
	{
		return jit_insn_call_intrinsic
			(func, name, intrinsic, descr, value1, 0);
	}
}

/*
 * Output instructions to handle "finally" clauses in the current context.
 */
static int handle_finally(jit_function_t func, int opcode)
{
	jit_block_eh_t eh = func->builder->current_handler;
	while(eh != 0)
	{
		if(eh->finally_on_fault)
		{
			/* The "finally" clause only applies to the "catch" block */
			if(!(eh->in_try_body) &&
			   eh->finally_label != jit_label_undefined)
			{
				return create_noarg_note(func, opcode);
			}
		}
		else if(eh->finally_label != jit_label_undefined)
		{
			/* The "finally" clause applies to "try" body and the "catch" */
			return create_noarg_note(func, opcode);
		}
		eh = eh->parent;
	}
	return 1;
}

/*@
 * @deftypefun int jit_insn_branch (jit_function_t func, {jit_label_t *} label)
 * Terminate the current block by branching unconditionally
 * to a specific label.  Returns zero if out of memory.
 * @end deftypefun
@*/
int jit_insn_branch(jit_function_t func, jit_label_t *label)
{
	jit_insn_t insn;
	jit_block_t block;
	if(!label)
	{
		return 0;
	}
	if(!_jit_function_ensure_builder(func))
	{
		return 0;
	}
	if(!handle_finally(func, JIT_OP_PREPARE_FOR_LEAVE))
	{
		return 0;
	}
	insn = _jit_block_add_insn(func->builder->current_block);
	if(!insn)
	{
		return 0;
	}
	if(*label == jit_label_undefined)
	{
		*label = (func->builder->next_label)++;
	}
	insn->opcode = (short)JIT_OP_BR;
	insn->flags = JIT_INSN_DEST_IS_LABEL;
	insn->dest = (jit_value_t)(*label);
	block = _jit_block_create(func, 0);
	if(!block)
	{
		return 0;
	}
	func->builder->current_block = block;
	return 1;
}

/*@
 * @deftypefun int jit_insn_branch_if (jit_function_t func, jit_value_t value, {jit_label_t *} label)
 * Terminate the current block by branching to a specific label if
 * the specified value is non-zero.  Returns zero if out of memory.
 *
 * If @code{value} refers to a conditional expression that was created
 * by @code{jit_insn_eq}, @code{jit_insn_ne}, etc, then the conditional
 * expression will be replaced by an appropriate conditional branch
 * instruction.
 * @end deftypefun
@*/
int jit_insn_branch_if
		(jit_function_t func, jit_value_t value, jit_label_t *label)
{
	jit_insn_t insn;
	jit_block_t block;
	jit_type_t type;
	int opcode;
	jit_value_t value2;

	/* Bail out if the parameters are invalid */
	if(!value || !label)
	{
		return 0;
	}

	/* Ensure that we have a function builder */
	if(!_jit_function_ensure_builder(func))
	{
		return 0;
	}

	/* Allocate a new label identifier, if necessary */
	if(*label == jit_label_undefined)
	{
		*label = (func->builder->next_label)++;
	}

	/* Determine if we can replace a previous comparison instruction */
	block = func->builder->current_block;
	insn = _jit_block_get_last(block);
	if(value->is_temporary && insn && insn->dest == value)
	{
		opcode = insn->opcode;
		if(opcode >= JIT_OP_IEQ && opcode <= JIT_OP_NFGE_INV)
		{
			switch(opcode)
			{
				case JIT_OP_IEQ:		opcode = JIT_OP_BR_IEQ;      break;
				case JIT_OP_INE:		opcode = JIT_OP_BR_INE;      break;
				case JIT_OP_ILT:		opcode = JIT_OP_BR_ILT;      break;
				case JIT_OP_ILT_UN:		opcode = JIT_OP_BR_ILT_UN;   break;
				case JIT_OP_ILE:		opcode = JIT_OP_BR_ILE;      break;
				case JIT_OP_ILE_UN:		opcode = JIT_OP_BR_ILE_UN;   break;
				case JIT_OP_IGT:		opcode = JIT_OP_BR_IGT;      break;
				case JIT_OP_IGT_UN:		opcode = JIT_OP_BR_IGT_UN;   break;
				case JIT_OP_IGE:		opcode = JIT_OP_BR_IGE;      break;
				case JIT_OP_IGE_UN:		opcode = JIT_OP_BR_IGE_UN;   break;
				case JIT_OP_LEQ:		opcode = JIT_OP_BR_LEQ;      break;
				case JIT_OP_LNE:		opcode = JIT_OP_BR_LNE;      break;
				case JIT_OP_LLT:		opcode = JIT_OP_BR_LLT;      break;
				case JIT_OP_LLT_UN:		opcode = JIT_OP_BR_LLT_UN;   break;
				case JIT_OP_LLE:		opcode = JIT_OP_BR_LLE;      break;
				case JIT_OP_LLE_UN:		opcode = JIT_OP_BR_LLE_UN;   break;
				case JIT_OP_LGT:		opcode = JIT_OP_BR_LGT;      break;
				case JIT_OP_LGT_UN:		opcode = JIT_OP_BR_LGT_UN;   break;
				case JIT_OP_LGE:		opcode = JIT_OP_BR_LGE;      break;
				case JIT_OP_LGE_UN:		opcode = JIT_OP_BR_LGE_UN;   break;
				case JIT_OP_FEQ:		opcode = JIT_OP_BR_FEQ;      break;
				case JIT_OP_FNE:		opcode = JIT_OP_BR_FNE;      break;
				case JIT_OP_FLT:		opcode = JIT_OP_BR_FLT;      break;
				case JIT_OP_FLE:		opcode = JIT_OP_BR_FLE;      break;
				case JIT_OP_FGT:		opcode = JIT_OP_BR_FGT;      break;
				case JIT_OP_FGE:		opcode = JIT_OP_BR_FGE;      break;
				case JIT_OP_FEQ_INV:	opcode = JIT_OP_BR_FEQ_INV;  break;
				case JIT_OP_FNE_INV:	opcode = JIT_OP_BR_FNE_INV;  break;
				case JIT_OP_FLT_INV:	opcode = JIT_OP_BR_FLT_INV;  break;
				case JIT_OP_FLE_INV:	opcode = JIT_OP_BR_FLE_INV;  break;
				case JIT_OP_FGT_INV:	opcode = JIT_OP_BR_FGT_INV;  break;
				case JIT_OP_FGE_INV:	opcode = JIT_OP_BR_FGE_INV;  break;
				case JIT_OP_DEQ:		opcode = JIT_OP_BR_DEQ;      break;
				case JIT_OP_DNE:		opcode = JIT_OP_BR_DNE;      break;
				case JIT_OP_DLT:		opcode = JIT_OP_BR_DLT;      break;
				case JIT_OP_DLE:		opcode = JIT_OP_BR_DLE;      break;
				case JIT_OP_DGT:		opcode = JIT_OP_BR_DGT;      break;
				case JIT_OP_DGE:		opcode = JIT_OP_BR_DGE;      break;
				case JIT_OP_DEQ_INV:	opcode = JIT_OP_BR_DEQ_INV;  break;
				case JIT_OP_DNE_INV:	opcode = JIT_OP_BR_DNE_INV;  break;
				case JIT_OP_DLT_INV:	opcode = JIT_OP_BR_DLT_INV;  break;
				case JIT_OP_DLE_INV:	opcode = JIT_OP_BR_DLE_INV;  break;
				case JIT_OP_DGT_INV:	opcode = JIT_OP_BR_DGT_INV;  break;
				case JIT_OP_DGE_INV:	opcode = JIT_OP_BR_DGE_INV;  break;
				case JIT_OP_NFEQ:		opcode = JIT_OP_BR_NFEQ;     break;
				case JIT_OP_NFNE:		opcode = JIT_OP_BR_NFNE;     break;
				case JIT_OP_NFLT:		opcode = JIT_OP_BR_NFLT;     break;
				case JIT_OP_NFLE:		opcode = JIT_OP_BR_NFLE;     break;
				case JIT_OP_NFGT:		opcode = JIT_OP_BR_NFGT;     break;
				case JIT_OP_NFGE:		opcode = JIT_OP_BR_NFGE;     break;
				case JIT_OP_NFEQ_INV:	opcode = JIT_OP_BR_NFEQ_INV; break;
				case JIT_OP_NFNE_INV:	opcode = JIT_OP_BR_NFNE_INV; break;
				case JIT_OP_NFLT_INV:	opcode = JIT_OP_BR_NFLT_INV; break;
				case JIT_OP_NFLE_INV:	opcode = JIT_OP_BR_NFLE_INV; break;
				case JIT_OP_NFGT_INV:	opcode = JIT_OP_BR_NFGT_INV; break;
				case JIT_OP_NFGE_INV:	opcode = JIT_OP_BR_NFGE_INV; break;
			}
			insn->opcode = (short)opcode;
			insn->flags = JIT_INSN_DEST_IS_LABEL;
			insn->dest = (jit_value_t)(*label);
			goto add_block;
		}
	}

	/* Coerce the result to something comparable and determine the opcode */
	type = jit_type_promote_int(jit_type_normalize(value->type));
	if(type == jit_type_int || type == jit_type_uint)
	{
		opcode = JIT_OP_BR_ITRUE;
		value2 = 0;
	}
	else if(type == jit_type_long || type == jit_type_ulong)
	{
		opcode = JIT_OP_BR_LTRUE;
		value2 = 0;
	}
	else if(type == jit_type_float32)
	{
		opcode = JIT_OP_BR_FNE;
		value2 = jit_value_create_float32_constant
			(func, jit_type_float32, (jit_float32)0.0);
		if(!value2)
		{
			return 0;
		}
	}
	else if(type == jit_type_float64)
	{
		opcode = JIT_OP_BR_DNE;
		value2 = jit_value_create_float64_constant
			(func, jit_type_float64, (jit_float64)0.0);
		if(!value2)
		{
			return 0;
		}
	}
	else
	{
		type = jit_type_nfloat;
		opcode = JIT_OP_BR_NFNE;
		value2 = jit_value_create_nfloat_constant
			(func, jit_type_nfloat, (jit_nfloat)0.0);
		if(!value2)
		{
			return 0;
		}
	}
	value = jit_insn_convert(func, value, type, 0);
	if(!value)
	{
		return 0;
	}

	/* Add a new branch instruction */
	insn = _jit_block_add_insn(func->builder->current_block);
	if(!insn)
	{
		return 0;
	}
	jit_value_ref(func, value);
	jit_value_ref(func, value2);
	insn->opcode = (short)opcode;
	insn->flags = JIT_INSN_DEST_IS_LABEL;
	insn->dest = (jit_value_t)(*label);
	insn->value1 = value;
	insn->value2 = value2;

add_block:
	/* Add a new block for the fall-through case */
	block = _jit_block_create(func, 0);
	if(!block)
	{
		return 0;
	}
	block->entered_via_top = 1;
	func->builder->current_block = block;
	return 1;
}

/*@
 * @deftypefun int jit_insn_branch_if_not (jit_function_t func, jit_value_t value, {jit_label_t *} label)
 * Terminate the current block by branching to a specific label if
 * the specified value is zero.  Returns zero if out of memory.
 *
 * If @code{value} refers to a conditional expression that was created
 * by @code{jit_insn_eq}, @code{jit_insn_ne}, etc, then the conditional
 * expression will be replaced by an appropriate conditional branch
 * instruction.
 * @end deftypefun
@*/
int jit_insn_branch_if_not
		(jit_function_t func, jit_value_t value, jit_label_t *label)
{
	jit_insn_t insn;
	jit_block_t block;
	jit_type_t type;
	int opcode;
	jit_value_t value2;

	/* Bail out if the parameters are invalid */
	if(!value || !label)
	{
		return 0;
	}

	/* Ensure that we have a function builder */
	if(!_jit_function_ensure_builder(func))
	{
		return 0;
	}

	/* Allocate a new label identifier, if necessary */
	if(*label == jit_label_undefined)
	{
		*label = (func->builder->next_label)++;
	}

	/* Determine if we can replace a previous comparison instruction */
	block = func->builder->current_block;
	insn = _jit_block_get_last(block);
	if(value->is_temporary && insn && insn->dest == value)
	{
		opcode = insn->opcode;
		if(opcode >= JIT_OP_IEQ && opcode <= JIT_OP_NFGE_INV)
		{
			switch(opcode)
			{
				case JIT_OP_IEQ:		opcode = JIT_OP_BR_INE;      break;
				case JIT_OP_INE:		opcode = JIT_OP_BR_IEQ;      break;
				case JIT_OP_ILT:		opcode = JIT_OP_BR_IGE;      break;
				case JIT_OP_ILT_UN:		opcode = JIT_OP_BR_IGE_UN;   break;
				case JIT_OP_ILE:		opcode = JIT_OP_BR_IGT;      break;
				case JIT_OP_ILE_UN:		opcode = JIT_OP_BR_IGT_UN;   break;
				case JIT_OP_IGT:		opcode = JIT_OP_BR_ILE;      break;
				case JIT_OP_IGT_UN:		opcode = JIT_OP_BR_ILE_UN;   break;
				case JIT_OP_IGE:		opcode = JIT_OP_BR_ILT;      break;
				case JIT_OP_IGE_UN:		opcode = JIT_OP_BR_ILT_UN;   break;
				case JIT_OP_LEQ:		opcode = JIT_OP_BR_LNE;      break;
				case JIT_OP_LNE:		opcode = JIT_OP_BR_LEQ;      break;
				case JIT_OP_LLT:		opcode = JIT_OP_BR_LGE;      break;
				case JIT_OP_LLT_UN:		opcode = JIT_OP_BR_LGE_UN;   break;
				case JIT_OP_LLE:		opcode = JIT_OP_BR_LGT;      break;
				case JIT_OP_LLE_UN:		opcode = JIT_OP_BR_LGT_UN;   break;
				case JIT_OP_LGT:		opcode = JIT_OP_BR_LLE;      break;
				case JIT_OP_LGT_UN:		opcode = JIT_OP_BR_LLE_UN;   break;
				case JIT_OP_LGE:		opcode = JIT_OP_BR_LLT;      break;
				case JIT_OP_LGE_UN:		opcode = JIT_OP_BR_LLT_UN;   break;
				case JIT_OP_FEQ:		opcode = JIT_OP_BR_FNE_INV;  break;
				case JIT_OP_FNE:		opcode = JIT_OP_BR_FEQ_INV;  break;
				case JIT_OP_FLT:		opcode = JIT_OP_BR_FGE_INV;  break;
				case JIT_OP_FLE:		opcode = JIT_OP_BR_FGT_INV;  break;
				case JIT_OP_FGT:		opcode = JIT_OP_BR_FLE_INV;  break;
				case JIT_OP_FGE:		opcode = JIT_OP_BR_FLT_INV;  break;
				case JIT_OP_FEQ_INV:	opcode = JIT_OP_BR_FNE;      break;
				case JIT_OP_FNE_INV:	opcode = JIT_OP_BR_FEQ;      break;
				case JIT_OP_FLT_INV:	opcode = JIT_OP_BR_FGE;      break;
				case JIT_OP_FLE_INV:	opcode = JIT_OP_BR_FGT;      break;
				case JIT_OP_FGT_INV:	opcode = JIT_OP_BR_FLE;      break;
				case JIT_OP_FGE_INV:	opcode = JIT_OP_BR_FLT;      break;
				case JIT_OP_DEQ:		opcode = JIT_OP_BR_DNE_INV;  break;
				case JIT_OP_DNE:		opcode = JIT_OP_BR_DEQ_INV;  break;
				case JIT_OP_DLT:		opcode = JIT_OP_BR_DGE_INV;  break;
				case JIT_OP_DLE:		opcode = JIT_OP_BR_DGT_INV;  break;
				case JIT_OP_DGT:		opcode = JIT_OP_BR_DLE_INV;  break;
				case JIT_OP_DGE:		opcode = JIT_OP_BR_DLT_INV;  break;
				case JIT_OP_DEQ_INV:	opcode = JIT_OP_BR_DNE;      break;
				case JIT_OP_DNE_INV:	opcode = JIT_OP_BR_DEQ;      break;
				case JIT_OP_DLT_INV:	opcode = JIT_OP_BR_DGE;      break;
				case JIT_OP_DLE_INV:	opcode = JIT_OP_BR_DGT;      break;
				case JIT_OP_DGT_INV:	opcode = JIT_OP_BR_DLE;      break;
				case JIT_OP_DGE_INV:	opcode = JIT_OP_BR_DLT;      break;
				case JIT_OP_NFEQ:		opcode = JIT_OP_BR_NFNE_INV; break;
				case JIT_OP_NFNE:		opcode = JIT_OP_BR_NFEQ_INV; break;
				case JIT_OP_NFLT:		opcode = JIT_OP_BR_NFGE_INV; break;
				case JIT_OP_NFLE:		opcode = JIT_OP_BR_NFGT_INV; break;
				case JIT_OP_NFGT:		opcode = JIT_OP_BR_NFLE_INV; break;
				case JIT_OP_NFGE:		opcode = JIT_OP_BR_NFLT_INV; break;
				case JIT_OP_NFEQ_INV:	opcode = JIT_OP_BR_NFNE;     break;
				case JIT_OP_NFNE_INV:	opcode = JIT_OP_BR_NFEQ;     break;
				case JIT_OP_NFLT_INV:	opcode = JIT_OP_BR_NFGE;     break;
				case JIT_OP_NFLE_INV:	opcode = JIT_OP_BR_NFGT;     break;
				case JIT_OP_NFGT_INV:	opcode = JIT_OP_BR_NFLE;     break;
				case JIT_OP_NFGE_INV:	opcode = JIT_OP_BR_NFLT;     break;
			}
			insn->opcode = (short)opcode;
			insn->flags = JIT_INSN_DEST_IS_LABEL;
			insn->dest = (jit_value_t)(*label);
			goto add_block;
		}
	}

	/* Coerce the result to something comparable and determine the opcode */
	type = jit_type_promote_int(jit_type_normalize(value->type));
	if(type == jit_type_int || type == jit_type_uint)
	{
		opcode = JIT_OP_BR_IFALSE;
		value2 = 0;
	}
	else if(type == jit_type_long || type == jit_type_ulong)
	{
		opcode = JIT_OP_BR_LFALSE;
		value2 = 0;
	}
	else if(type == jit_type_float32)
	{
		opcode = JIT_OP_BR_FEQ_INV;
		value2 = jit_value_create_float32_constant
			(func, jit_type_float32, (jit_float32)0.0);
		if(!value2)
		{
			return 0;
		}
	}
	else if(type == jit_type_float64)
	{
		opcode = JIT_OP_BR_DEQ_INV;
		value2 = jit_value_create_float64_constant
			(func, jit_type_float64, (jit_float64)0.0);
		if(!value2)
		{
			return 0;
		}
	}
	else
	{
		type = jit_type_nfloat;
		opcode = JIT_OP_BR_NFEQ_INV;
		value2 = jit_value_create_nfloat_constant
			(func, jit_type_nfloat, (jit_nfloat)0.0);
		if(!value2)
		{
			return 0;
		}
	}
	value = jit_insn_convert(func, value, type, 0);
	if(!value)
	{
		return 0;
	}

	/* Add a new branch instruction */
	insn = _jit_block_add_insn(func->builder->current_block);
	if(!insn)
	{
		return 0;
	}
	jit_value_ref(func, value);
	jit_value_ref(func, value2);
	insn->opcode = (short)opcode;
	insn->flags = JIT_INSN_DEST_IS_LABEL;
	insn->dest = (jit_value_t)(*label);
	insn->value1 = value;
	insn->value2 = value2;

add_block:
	/* Add a new block for the fall-through case */
	block = _jit_block_create(func, 0);
	if(!block)
	{
		return 0;
	}
	block->entered_via_top = 1;
	func->builder->current_block = block;
	return 1;
}

/*@
 * @deftypefun jit_value_t jit_insn_address_of (jit_function_t func, jit_value_t value1)
 * Get the address of a value into a new temporary.
 * @end deftypefun
@*/
jit_value_t jit_insn_address_of(jit_function_t func, jit_value_t value1)
{
	jit_type_t type;
	jit_value_t result;
	if(!value1)
	{
		return 0;
	}
	type = jit_type_create_pointer(jit_value_get_type(value1), 1);
	if(!type)
	{
		return 0;
	}
	jit_value_set_addressable(value1);
	result = apply_unary(func, JIT_OP_ADDRESS_OF, value1, type);
	jit_type_free(type);
	return result;
}

/*
 * Information about the opcodes for a particular conversion.
 */
typedef struct jit_convert_info
{
	int			cvt1;
	jit_type_t	type1;
	int			cvt2;
	jit_type_t	type2;
	int			cvt3;
	jit_type_t	type3;

} jit_convert_info_t;
#define	CVT(opcode,name) opcode, (jit_type_t)&_jit_type_##name##_def
#define	CVT_NONE		 0, 0

/*@
 * @deftypefun jit_value_t jit_insn_convert (jit_function_t func, jit_value_t value, jit_type_t type, int overflow_check)
 * Convert the contents of a value into a new type, with optional
 * overflow checking.
 * @end deftypefun
@*/
jit_value_t jit_insn_convert(jit_function_t func, jit_value_t value,
						     jit_type_t type, int overflow_check)
{
	jit_type_t vtype;
	const jit_convert_info_t *opcode_map;

	/* Bail out if we previously ran out of memory on this function */
	if(!value)
	{
		return 0;
	}

	/* Normalize the source and destination types */
	type = jit_type_normalize(type);
	vtype = jit_type_normalize(value->type);

	/* If the types are identical, then return the source value as-is */
	if(type == vtype)
	{
		return value;
	}

	/* If the source is a constant, then perform a constant conversion.
	   If an overflow might result, we perform the computation at runtime */
	if(jit_value_is_constant(value))
	{
		jit_constant_t const_value;
		const_value = jit_value_get_constant(value);
		if(jit_constant_convert(&const_value, &const_value,
								type, overflow_check))
		{
			return jit_value_create_constant(func, &const_value);
		}
	}

	/* Promote the source type, to reduce the number of cases in
	   the switch statement below */
	vtype = jit_type_promote_int(vtype);

	/* Determine how to perform the conversion */
	opcode_map = 0;
	switch(type->kind)
	{
		case JIT_TYPE_SBYTE:
		{
			/* Convert the value into a signed byte */
			static jit_convert_info_t const to_sbyte[] = {
				{CVT(JIT_OP_TRUNC_SBYTE, sbyte),
				 	CVT_NONE,
					CVT_NONE},
				{CVT(JIT_OP_CHECK_SBYTE, sbyte),
				 	CVT_NONE,
					CVT_NONE},
				{CVT(JIT_OP_TRUNC_SBYTE, sbyte),
					CVT_NONE,
					CVT_NONE},
				{CVT(JIT_OP_CHECK_INT, int),
				 	CVT(JIT_OP_CHECK_SBYTE, sbyte),
					CVT_NONE},
				{CVT(JIT_OP_LOW_WORD, int),
					CVT(JIT_OP_TRUNC_SBYTE, sbyte),
					CVT_NONE},
				{CVT(JIT_OP_CHECK_SIGNED_LOW_WORD, int),
				 	CVT(JIT_OP_CHECK_SBYTE, sbyte),
					CVT_NONE},
				{CVT(JIT_OP_LOW_WORD, int),
					CVT(JIT_OP_TRUNC_SBYTE, sbyte),
					CVT_NONE},
				{CVT(JIT_OP_CHECK_LOW_WORD, uint),
				 	CVT(JIT_OP_CHECK_INT, int),
					CVT(JIT_OP_CHECK_SBYTE, sbyte)},
				{CVT(JIT_OP_FLOAT32_TO_NFLOAT, nfloat),
					CVT(JIT_OP_NFLOAT_TO_INT, int),
					CVT(JIT_OP_TRUNC_SBYTE, sbyte)},
				{CVT(JIT_OP_FLOAT32_TO_NFLOAT, nfloat),
					CVT(JIT_OP_CHECK_NFLOAT_TO_INT, int),
					CVT(JIT_OP_CHECK_SBYTE, sbyte)},
				{CVT(JIT_OP_FLOAT64_TO_NFLOAT, nfloat),
					CVT(JIT_OP_NFLOAT_TO_INT, int),
					CVT(JIT_OP_TRUNC_SBYTE, sbyte)},
				{CVT(JIT_OP_FLOAT64_TO_NFLOAT, nfloat),
					CVT(JIT_OP_CHECK_NFLOAT_TO_INT, int),
					CVT(JIT_OP_CHECK_SBYTE, sbyte)},
				{CVT(JIT_OP_NFLOAT_TO_INT, int),
					CVT(JIT_OP_TRUNC_SBYTE, sbyte),
					CVT_NONE},
				{CVT(JIT_OP_CHECK_NFLOAT_TO_INT, int),
				 	CVT(JIT_OP_CHECK_SBYTE, sbyte),
					CVT_NONE}
			};
			opcode_map = to_sbyte;
		}
		break;

		case JIT_TYPE_UBYTE:
		{
			/* Convert the value into an unsigned byte */
			static jit_convert_info_t const to_ubyte[] = {
				{CVT(JIT_OP_TRUNC_UBYTE, ubyte),
					CVT_NONE,
					CVT_NONE},
				{CVT(JIT_OP_CHECK_UBYTE, ubyte),
				 	CVT_NONE,
					CVT_NONE},
				{CVT(JIT_OP_TRUNC_UBYTE, ubyte),
					CVT_NONE,
					CVT_NONE},
				{CVT(JIT_OP_CHECK_UBYTE, ubyte),
				 	CVT_NONE,
					CVT_NONE},
				{CVT(JIT_OP_LOW_WORD, int),
					CVT(JIT_OP_TRUNC_UBYTE, ubyte),
					CVT_NONE},
				{CVT(JIT_OP_CHECK_SIGNED_LOW_WORD, int),
				 	CVT(JIT_OP_CHECK_UBYTE, ubyte),
					CVT_NONE},
				{CVT(JIT_OP_LOW_WORD, int),
					CVT(JIT_OP_TRUNC_UBYTE, ubyte),
					CVT_NONE},
				{CVT(JIT_OP_CHECK_LOW_WORD, uint),
				 	CVT(JIT_OP_CHECK_UBYTE, ubyte),
					CVT_NONE},
				{CVT(JIT_OP_FLOAT32_TO_NFLOAT, nfloat),
					CVT(JIT_OP_NFLOAT_TO_INT, int),
					CVT(JIT_OP_TRUNC_UBYTE, ubyte)},
				{CVT(JIT_OP_FLOAT32_TO_NFLOAT, nfloat),
				 	CVT(JIT_OP_CHECK_NFLOAT_TO_INT, int),
					CVT(JIT_OP_CHECK_UBYTE, ubyte)},
				{CVT(JIT_OP_FLOAT64_TO_NFLOAT, nfloat),
					CVT(JIT_OP_NFLOAT_TO_INT, int),
					CVT(JIT_OP_TRUNC_UBYTE, ubyte)},
				{CVT(JIT_OP_FLOAT64_TO_NFLOAT, nfloat),
					CVT(JIT_OP_CHECK_NFLOAT_TO_INT, int),
					CVT(JIT_OP_CHECK_UBYTE, ubyte)},
				{CVT(JIT_OP_NFLOAT_TO_INT, int),
					CVT(JIT_OP_TRUNC_UBYTE, ubyte),
					CVT_NONE},
				{CVT(JIT_OP_CHECK_NFLOAT_TO_INT, int),
				 	CVT(JIT_OP_CHECK_UBYTE, ubyte),
					CVT_NONE}
			};
			opcode_map = to_ubyte;
		}
		break;

		case JIT_TYPE_SHORT:
		{
			/* Convert the value into a signed short */
			static jit_convert_info_t const to_short[] = {
				{CVT(JIT_OP_TRUNC_SHORT, short),
					CVT_NONE,
					CVT_NONE},
				{CVT(JIT_OP_CHECK_SHORT, short),
				 	CVT_NONE,
					CVT_NONE},
				{CVT(JIT_OP_TRUNC_SHORT, short),
					CVT_NONE,
					CVT_NONE},
				{CVT(JIT_OP_CHECK_INT, int),
				 	CVT(JIT_OP_CHECK_SHORT, short),
					CVT_NONE},
				{CVT(JIT_OP_LOW_WORD, int),
					CVT(JIT_OP_TRUNC_SHORT, short),
					CVT_NONE},
				{CVT(JIT_OP_CHECK_SIGNED_LOW_WORD, int),
				 	CVT(JIT_OP_CHECK_SHORT, short),
					CVT_NONE},
				{CVT(JIT_OP_LOW_WORD, int),
					CVT(JIT_OP_TRUNC_SHORT, short),
					CVT_NONE},
				{CVT(JIT_OP_CHECK_LOW_WORD, uint),
				 	CVT(JIT_OP_CHECK_INT, int),
					CVT(JIT_OP_CHECK_SHORT, short)},
				{CVT(JIT_OP_FLOAT32_TO_NFLOAT, nfloat),
					CVT(JIT_OP_NFLOAT_TO_INT, int),
					CVT(JIT_OP_TRUNC_SHORT, short)},
				{CVT(JIT_OP_FLOAT32_TO_NFLOAT, nfloat),
					CVT(JIT_OP_CHECK_NFLOAT_TO_INT, int),
					CVT(JIT_OP_CHECK_SHORT, short)},
				{CVT(JIT_OP_FLOAT64_TO_NFLOAT, nfloat),
					CVT(JIT_OP_NFLOAT_TO_INT, int),
					CVT(JIT_OP_TRUNC_SHORT, short)},
				{CVT(JIT_OP_FLOAT64_TO_NFLOAT, nfloat),
					CVT(JIT_OP_CHECK_NFLOAT_TO_INT, int),
					CVT(JIT_OP_CHECK_SHORT, short)},
				{CVT(JIT_OP_NFLOAT_TO_INT, int),
					CVT(JIT_OP_TRUNC_SHORT, short),
					CVT_NONE},
				{CVT(JIT_OP_CHECK_NFLOAT_TO_INT, int),
				 	CVT(JIT_OP_CHECK_SHORT, short),
					CVT_NONE}
			};
			opcode_map = to_short;
		}
		break;

		case JIT_TYPE_USHORT:
		{
			/* Convert the value into an unsigned short */
			static jit_convert_info_t const to_ushort[] = {
				{CVT(JIT_OP_TRUNC_USHORT, ushort),
					CVT_NONE,
					CVT_NONE},
				{CVT(JIT_OP_CHECK_USHORT, ushort),
				 	CVT_NONE,
					CVT_NONE},
				{CVT(JIT_OP_TRUNC_USHORT, ushort),
					CVT_NONE,
					CVT_NONE},
				{CVT(JIT_OP_CHECK_USHORT, ushort),
				 	CVT_NONE,
					CVT_NONE},
				{CVT(JIT_OP_LOW_WORD, int),
					CVT(JIT_OP_TRUNC_USHORT, ushort),
					CVT_NONE},
				{CVT(JIT_OP_CHECK_SIGNED_LOW_WORD, int),
				 	CVT(JIT_OP_CHECK_USHORT, ushort),
					CVT_NONE},
				{CVT(JIT_OP_LOW_WORD, int),
					CVT(JIT_OP_TRUNC_USHORT, ushort),
					CVT_NONE},
				{CVT(JIT_OP_CHECK_LOW_WORD, uint),
				 	CVT(JIT_OP_CHECK_USHORT, ushort),
					CVT_NONE},
				{CVT(JIT_OP_FLOAT32_TO_NFLOAT, nfloat),
					CVT(JIT_OP_NFLOAT_TO_INT, int),
					CVT(JIT_OP_TRUNC_USHORT, ushort)},
				{CVT(JIT_OP_FLOAT32_TO_NFLOAT, nfloat),
					CVT(JIT_OP_CHECK_NFLOAT_TO_INT, int),
					CVT(JIT_OP_CHECK_USHORT, ushort)},
				{CVT(JIT_OP_FLOAT64_TO_NFLOAT, nfloat),
					CVT(JIT_OP_NFLOAT_TO_INT, int),
					CVT(JIT_OP_TRUNC_USHORT, ushort)},
				{CVT(JIT_OP_FLOAT64_TO_NFLOAT, nfloat),
					CVT(JIT_OP_CHECK_NFLOAT_TO_INT, int),
					CVT(JIT_OP_CHECK_USHORT, ushort)},
				{CVT(JIT_OP_NFLOAT_TO_INT, int),
					CVT(JIT_OP_TRUNC_USHORT, ushort),
					CVT_NONE},
				{CVT(JIT_OP_CHECK_NFLOAT_TO_INT, int),
				 	CVT(JIT_OP_CHECK_USHORT, ushort),
					CVT_NONE}
			};
			opcode_map = to_ushort;
		}
		break;

		case JIT_TYPE_INT:
		{
			/* Convert the value into a signed int */
			static jit_convert_info_t const to_int[] = {
				{CVT(JIT_OP_COPY_INT, int),
					CVT_NONE,
					CVT_NONE},
				{CVT(JIT_OP_COPY_INT, int),
				 	CVT_NONE,
					CVT_NONE},
				{CVT(JIT_OP_TRUNC_INT, int),
					CVT_NONE,
					CVT_NONE},
				{CVT(JIT_OP_CHECK_INT, int),
					CVT_NONE,
					CVT_NONE},
				{CVT(JIT_OP_LOW_WORD, int),
					CVT(JIT_OP_TRUNC_INT, int),
					CVT_NONE},
				{CVT(JIT_OP_CHECK_SIGNED_LOW_WORD, int),
				 	CVT_NONE,
					CVT_NONE},
				{CVT(JIT_OP_LOW_WORD, int),
					CVT(JIT_OP_TRUNC_INT, int),
					CVT_NONE},
				{CVT(JIT_OP_CHECK_LOW_WORD, uint),
				 	CVT(JIT_OP_CHECK_INT, int),
					CVT_NONE},
				{CVT(JIT_OP_FLOAT32_TO_NFLOAT, nfloat),
					CVT(JIT_OP_NFLOAT_TO_INT, int),
					CVT_NONE},
				{CVT(JIT_OP_FLOAT32_TO_NFLOAT, nfloat),
				 	CVT(JIT_OP_CHECK_NFLOAT_TO_INT, int),
					CVT_NONE},
				{CVT(JIT_OP_FLOAT64_TO_NFLOAT, nfloat),
					CVT(JIT_OP_NFLOAT_TO_INT, int),
					CVT_NONE},
				{CVT(JIT_OP_FLOAT64_TO_NFLOAT, nfloat),
				 	CVT(JIT_OP_CHECK_NFLOAT_TO_INT, int),
					CVT_NONE},
				{CVT(JIT_OP_NFLOAT_TO_INT, int),
					CVT_NONE,
					CVT_NONE},
				{CVT(JIT_OP_CHECK_NFLOAT_TO_INT, int),
				 	CVT_NONE,
					CVT_NONE}
			};
			opcode_map = to_int;
		}
		break;

		case JIT_TYPE_UINT:
		{
			/* Convert the value into an unsigned int */
			static jit_convert_info_t const to_uint[] = {
				{CVT(JIT_OP_TRUNC_UINT, uint),
					CVT_NONE,
					CVT_NONE},
				{CVT(JIT_OP_CHECK_UINT, uint),
				 	CVT_NONE,
					CVT_NONE},
				{CVT(JIT_OP_COPY_INT, uint),
					CVT_NONE,
					CVT_NONE},
				{CVT(JIT_OP_COPY_INT, uint),
				 	CVT_NONE,
					CVT_NONE},
				{CVT(JIT_OP_LOW_WORD, uint),
					CVT_NONE,
					CVT_NONE},
				{CVT(JIT_OP_CHECK_LOW_WORD, uint),
				 	CVT_NONE,
					CVT_NONE},
				{CVT(JIT_OP_LOW_WORD, uint),
					CVT_NONE,
					CVT_NONE},
				{CVT(JIT_OP_CHECK_LOW_WORD, uint),
				 	CVT_NONE,
					CVT_NONE},
				{CVT(JIT_OP_FLOAT32_TO_NFLOAT, nfloat),
					CVT(JIT_OP_NFLOAT_TO_UINT, uint),
					CVT_NONE},
				{CVT(JIT_OP_FLOAT32_TO_NFLOAT, nfloat),
				 	CVT(JIT_OP_CHECK_NFLOAT_TO_UINT, uint),
					CVT_NONE},
				{CVT(JIT_OP_FLOAT64_TO_NFLOAT, nfloat),
					CVT(JIT_OP_NFLOAT_TO_UINT, uint),
					CVT_NONE},
				{CVT(JIT_OP_FLOAT64_TO_NFLOAT, nfloat),
				 	CVT(JIT_OP_CHECK_NFLOAT_TO_UINT, uint),
					CVT_NONE},
				{CVT(JIT_OP_NFLOAT_TO_UINT, uint),
					CVT_NONE,
					CVT_NONE},
				{CVT(JIT_OP_CHECK_NFLOAT_TO_UINT, uint),
				 	CVT_NONE,
					CVT_NONE}
			};
			opcode_map = to_uint;
		}
		break;

		case JIT_TYPE_LONG:
		{
			/* Convert the value into a signed long */
			static jit_convert_info_t const to_long[] = {
				{CVT(JIT_OP_EXPAND_INT, long),
					CVT_NONE,
					CVT_NONE},
				{CVT(JIT_OP_EXPAND_INT, long),
				 	CVT_NONE,
					CVT_NONE},
				{CVT(JIT_OP_EXPAND_UINT, long),
					CVT_NONE,
					CVT_NONE},
				{CVT(JIT_OP_EXPAND_UINT, long),
				 	CVT_NONE,
					CVT_NONE},
				{CVT(JIT_OP_COPY_LONG, long),
					CVT_NONE,
					CVT_NONE},
				{CVT(JIT_OP_COPY_LONG, long),
				 	CVT_NONE,
					CVT_NONE},
				{CVT(JIT_OP_COPY_LONG, long),
					CVT_NONE,
					CVT_NONE},
				{CVT(JIT_OP_CHECK_LONG, long),
				 	CVT_NONE,
					CVT_NONE},
				{CVT(JIT_OP_FLOAT32_TO_NFLOAT, nfloat),
					CVT(JIT_OP_NFLOAT_TO_LONG, long),
					CVT_NONE},
				{CVT(JIT_OP_FLOAT32_TO_NFLOAT, nfloat),
				 	CVT(JIT_OP_CHECK_NFLOAT_TO_LONG, long),
					CVT_NONE},
				{CVT(JIT_OP_FLOAT64_TO_NFLOAT, nfloat),
					CVT(JIT_OP_NFLOAT_TO_LONG, long),
					CVT_NONE},
				{CVT(JIT_OP_FLOAT64_TO_NFLOAT, nfloat),
				 	CVT(JIT_OP_CHECK_NFLOAT_TO_LONG, long),
					CVT_NONE},
				{CVT(JIT_OP_NFLOAT_TO_LONG, long),
					CVT_NONE,
					CVT_NONE},
				{CVT(JIT_OP_CHECK_NFLOAT_TO_LONG, long),
				 	CVT_NONE,
					CVT_NONE}
			};
			opcode_map = to_long;
		}
		break;

		case JIT_TYPE_ULONG:
		{
			/* Convert the value into an unsigned long */
			static jit_convert_info_t const to_ulong[] = {
				{CVT(JIT_OP_EXPAND_INT, ulong),
					CVT_NONE,
					CVT_NONE},
				{CVT(JIT_OP_CHECK_UINT, uint),
				 	CVT(JIT_OP_EXPAND_UINT, ulong),
					CVT_NONE},
				{CVT(JIT_OP_EXPAND_UINT, ulong),
					CVT_NONE,
					CVT_NONE},
				{CVT(JIT_OP_EXPAND_UINT, ulong),
				 	CVT_NONE,
					CVT_NONE},
				{CVT(JIT_OP_COPY_LONG, ulong),
					CVT_NONE,
					CVT_NONE},
				{CVT(JIT_OP_CHECK_ULONG, ulong),
				 	CVT_NONE,
					CVT_NONE},
				{CVT(JIT_OP_COPY_LONG, ulong),
					CVT_NONE,
					CVT_NONE},
				{CVT(JIT_OP_COPY_LONG, ulong),
				 	CVT_NONE,
					CVT_NONE},
				{CVT(JIT_OP_FLOAT32_TO_NFLOAT, nfloat),
					CVT(JIT_OP_NFLOAT_TO_ULONG, ulong),
					CVT_NONE},
				{CVT(JIT_OP_FLOAT32_TO_NFLOAT, nfloat),
				 	CVT(JIT_OP_CHECK_NFLOAT_TO_ULONG, ulong),
					CVT_NONE},
				{CVT(JIT_OP_FLOAT64_TO_NFLOAT, nfloat),
					CVT(JIT_OP_NFLOAT_TO_ULONG, ulong),
					CVT_NONE},
				{CVT(JIT_OP_FLOAT64_TO_NFLOAT, nfloat),
				 	CVT(JIT_OP_CHECK_NFLOAT_TO_ULONG, ulong),
					CVT_NONE},
				{CVT(JIT_OP_NFLOAT_TO_ULONG, ulong),
					CVT_NONE,
					CVT_NONE},
				{CVT(JIT_OP_CHECK_NFLOAT_TO_ULONG, ulong),
				 	CVT_NONE,
					CVT_NONE}
			};
			opcode_map = to_ulong;
		}
		break;

		case JIT_TYPE_FLOAT32:
		{
			/* Convert the value into a 32-bit float */
			static jit_convert_info_t const to_float32[] = {
				{CVT(JIT_OP_INT_TO_NFLOAT, nfloat),
					CVT(JIT_OP_NFLOAT_TO_FLOAT32, float32),
					CVT_NONE},
				{CVT(JIT_OP_INT_TO_NFLOAT, nfloat),
				 	CVT(JIT_OP_NFLOAT_TO_FLOAT32, float32),
					CVT_NONE},
				{CVT(JIT_OP_UINT_TO_NFLOAT, nfloat),
					CVT(JIT_OP_NFLOAT_TO_FLOAT32, float32),
					CVT_NONE},
				{CVT(JIT_OP_UINT_TO_NFLOAT, nfloat),
					CVT(JIT_OP_NFLOAT_TO_FLOAT32, float32),
					CVT_NONE},
				{CVT(JIT_OP_LONG_TO_NFLOAT, nfloat),
					CVT(JIT_OP_NFLOAT_TO_FLOAT32, float32),
					CVT_NONE},
				{CVT(JIT_OP_LONG_TO_NFLOAT, nfloat),
					CVT(JIT_OP_NFLOAT_TO_FLOAT32, float32),
					CVT_NONE},
				{CVT(JIT_OP_ULONG_TO_NFLOAT, nfloat),
					CVT(JIT_OP_NFLOAT_TO_FLOAT32, float32),
					CVT_NONE},
				{CVT(JIT_OP_ULONG_TO_NFLOAT, nfloat),
					CVT(JIT_OP_NFLOAT_TO_FLOAT32, float32),
					CVT_NONE},
				{CVT(JIT_OP_COPY_FLOAT32, float32),
					CVT_NONE,
					CVT_NONE},
				{CVT(JIT_OP_COPY_FLOAT32, float32),
					CVT_NONE,
					CVT_NONE},
				{CVT(JIT_OP_FLOAT64_TO_NFLOAT, nfloat),
					CVT(JIT_OP_NFLOAT_TO_FLOAT32, float32),
					CVT_NONE},
				{CVT(JIT_OP_FLOAT64_TO_NFLOAT, nfloat),
					CVT(JIT_OP_NFLOAT_TO_FLOAT32, float32),
					CVT_NONE},
				{CVT(JIT_OP_NFLOAT_TO_FLOAT32, float32),
					CVT_NONE,
					CVT_NONE},
				{CVT(JIT_OP_NFLOAT_TO_FLOAT32, float32),
					CVT_NONE,
					CVT_NONE}
			};
			opcode_map = to_float32;
		}
		break;

		case JIT_TYPE_FLOAT64:
		{
			/* Convert the value into a 64-bit float */
			static jit_convert_info_t const to_float64[] = {
				{CVT(JIT_OP_INT_TO_NFLOAT, nfloat),
					CVT(JIT_OP_NFLOAT_TO_FLOAT64, float64),
					CVT_NONE},
				{CVT(JIT_OP_INT_TO_NFLOAT, nfloat),
					CVT(JIT_OP_NFLOAT_TO_FLOAT64, float64),
					CVT_NONE},
				{CVT(JIT_OP_UINT_TO_NFLOAT, nfloat),
					CVT(JIT_OP_NFLOAT_TO_FLOAT64, float64),
					CVT_NONE},
				{CVT(JIT_OP_UINT_TO_NFLOAT, nfloat),
					CVT(JIT_OP_NFLOAT_TO_FLOAT64, float64),
					CVT_NONE},
				{CVT(JIT_OP_LONG_TO_NFLOAT, nfloat),
					CVT(JIT_OP_NFLOAT_TO_FLOAT64, float64),
					CVT_NONE},
				{CVT(JIT_OP_LONG_TO_NFLOAT, nfloat),
					CVT(JIT_OP_NFLOAT_TO_FLOAT64, float64),
					CVT_NONE},
				{CVT(JIT_OP_ULONG_TO_NFLOAT, nfloat),
					CVT(JIT_OP_NFLOAT_TO_FLOAT64, float64),
					CVT_NONE},
				{CVT(JIT_OP_ULONG_TO_NFLOAT, nfloat),
					CVT(JIT_OP_NFLOAT_TO_FLOAT64, float64),
					CVT_NONE},
				{CVT(JIT_OP_FLOAT32_TO_NFLOAT, nfloat),
					CVT(JIT_OP_NFLOAT_TO_FLOAT64, float64),
					CVT_NONE},
				{CVT(JIT_OP_FLOAT32_TO_NFLOAT, nfloat),
					CVT(JIT_OP_NFLOAT_TO_FLOAT64, float64),
					CVT_NONE},
				{CVT(JIT_OP_COPY_FLOAT64, float64),
					CVT_NONE,
					CVT_NONE},
				{CVT(JIT_OP_COPY_FLOAT64, float64),
					CVT_NONE,
					CVT_NONE},
				{CVT(JIT_OP_NFLOAT_TO_FLOAT64, float64),
					CVT_NONE,
					CVT_NONE},
				{CVT(JIT_OP_NFLOAT_TO_FLOAT64, float64),
					CVT_NONE,
					CVT_NONE}
			};
			opcode_map = to_float64;
		}
		break;

		case JIT_TYPE_NFLOAT:
		{
			/* Convert the value into a native floating-point value */
			static jit_convert_info_t const to_nfloat[] = {
				{CVT(JIT_OP_INT_TO_NFLOAT, nfloat),
					CVT_NONE,
					CVT_NONE},
				{CVT(JIT_OP_INT_TO_NFLOAT, nfloat),
					CVT_NONE,
					CVT_NONE},
				{CVT(JIT_OP_UINT_TO_NFLOAT, nfloat),
					CVT_NONE,
					CVT_NONE},
				{CVT(JIT_OP_UINT_TO_NFLOAT, nfloat),
					CVT_NONE,
					CVT_NONE},
				{CVT(JIT_OP_LONG_TO_NFLOAT, nfloat),
					CVT_NONE,
					CVT_NONE},
				{CVT(JIT_OP_LONG_TO_NFLOAT, nfloat),
					CVT_NONE,
					CVT_NONE},
				{CVT(JIT_OP_ULONG_TO_NFLOAT, nfloat),
					CVT_NONE,
					CVT_NONE},
				{CVT(JIT_OP_ULONG_TO_NFLOAT, nfloat),
					CVT_NONE,
					CVT_NONE},
				{CVT(JIT_OP_FLOAT32_TO_NFLOAT, nfloat),
					CVT_NONE,
					CVT_NONE},
				{CVT(JIT_OP_FLOAT32_TO_NFLOAT, nfloat),
					CVT_NONE,
					CVT_NONE},
				{CVT(JIT_OP_FLOAT64_TO_NFLOAT, nfloat),
					CVT_NONE,
					CVT_NONE},
				{CVT(JIT_OP_FLOAT64_TO_NFLOAT, nfloat),
					CVT_NONE,
					CVT_NONE},
				{CVT(JIT_OP_COPY_NFLOAT, nfloat),
					CVT_NONE,
					CVT_NONE},
				{CVT(JIT_OP_COPY_NFLOAT, nfloat),
					CVT_NONE,
					CVT_NONE}
			};
			opcode_map = to_nfloat;
		}
		break;
	}
	if(opcode_map)
	{
		switch(vtype->kind)
		{
			case JIT_TYPE_UINT:		opcode_map += 2; break;
			case JIT_TYPE_LONG:		opcode_map += 4; break;
			case JIT_TYPE_ULONG:	opcode_map += 6; break;
			case JIT_TYPE_FLOAT32:	opcode_map += 8; break;
			case JIT_TYPE_FLOAT64:	opcode_map += 10; break;
			case JIT_TYPE_NFLOAT:	opcode_map += 12; break;
		}
		if(overflow_check)
		{
			opcode_map += 1;
		}
		value = apply_unary
			(func, opcode_map->cvt1, value, opcode_map->type1);
		if(opcode_map->cvt2)
		{
			value = apply_unary
				(func, opcode_map->cvt2, value, opcode_map->type2);
		}
		if(opcode_map->cvt3)
		{
			value = apply_unary
				(func, opcode_map->cvt3, value, opcode_map->type3);
		}
	}
	return value;
}

/*
 * Convert the parameters for a function call into their final types.
 */
static int convert_call_parameters
	(jit_function_t func, jit_type_t signature,
	 jit_value_t *args, unsigned int num_args,
	 jit_value_t *new_args)
{
	unsigned int param;
	for(param = 0; param < num_args; ++param)
	{
		new_args[param] = jit_insn_convert
			(func, args[param],
			 jit_type_get_param(signature, param), 0);
	}
	return 1;
}

/*
 * Set up the exception frame information before a function call out.
 */
static int setup_eh_frame_for_call(jit_function_t func, int flags)
{
#if !defined(JIT_BACKEND_INTERP)
	jit_type_t type;
	jit_value_t eh_frame_info;
	jit_block_t block;
	jit_value_t args[2];
	jit_insn_t insn;
	jit_type_t params[2];
	jit_value_t struct_return;

	/* If the "nothrow" or "tail" flags are set, then we don't
	   need to worry about this */
	if((flags & (JIT_CALL_NOTHROW | JIT_CALL_TAIL)) != 0)
	{
		return 1;
	}

	/* This function may throw an exception */
	func->builder->may_throw = 1;

	/* Get the value that holds the exception frame information */
	if((eh_frame_info = func->builder->eh_frame_info) == 0)
	{
		type = jit_type_create_struct(0, 0, 0);
		if(!type)
		{
			return 0;
		}
		jit_type_set_size_and_alignment
			(type, sizeof(struct jit_backtrace), sizeof(void *));
		eh_frame_info = jit_value_create(func, type);
		jit_type_free(type);
		if(!eh_frame_info)
		{
			return 0;
		}
		func->builder->eh_frame_info = eh_frame_info;
	}

	/* Output an instruction to load the "pc" into a value */
	args[1] = jit_value_create(func, jit_type_void_ptr);
	if(!(args[1]))
	{
		return 0;
	}
	insn = _jit_block_add_insn(func->builder->current_block);
	if(!insn)
	{
		return 0;
	}
	jit_value_ref(func, args[1]);
	insn->opcode = JIT_OP_LOAD_PC;
	insn->dest = args[1];

	/* Load the address of "eh_frame_info" into another value */
	args[0] = jit_insn_address_of(func, eh_frame_info);
	if(!(args[0]))
	{
		return 0;
	}

	/* Create a signature for the prototype "void (void *, void *)" */
	params[0] = jit_type_void_ptr;
	params[1] = jit_type_void_ptr;
	type = jit_type_create_signature
		(jit_abi_cdecl, jit_type_void, params, 2, 1);
	if(!type)
	{
		return 0;
	}

	/* Set up to call the "_jit_backtrace_push" intrinsic */
	if(!_jit_create_call_setup_insns
			(func, type, args, 2, 0, 0, &struct_return))
	{
		jit_type_free(type);
		return 0;
	}

	/* Terminate the current block and then call "_jit_backtrace_push" */
	block = _jit_block_create(func, 0);
	if(!block)
	{
		jit_type_free(type);
		return 0;
	}
	block->entered_via_top = 1;
	func->builder->current_block = block;
	insn = _jit_block_add_insn(block);
	if(!insn)
	{
		jit_type_free(type);
		return 0;
	}
	insn->opcode = JIT_OP_CALL_EXTERNAL;
	insn->flags = JIT_INSN_DEST_IS_NATIVE | JIT_INSN_VALUE1_IS_NAME;
	insn->dest = (jit_value_t)(void *)_jit_backtrace_push;
	insn->value1 = (jit_value_t)"_jit_backtrace_push";

	/* Clean up after the function call */
	if(!_jit_create_call_return_insns(func, type, args, 2, 0, 0))
	{
		jit_type_free(type);
		return 0;
	}

	/* We are now ready to make the actual function call */
	jit_type_free(type);
	return 1;
#else /* JIT_BACKEND_INTERP */
	/* The interpreter handles exception frames for us */
	if((flags & (JIT_CALL_NOTHROW | JIT_CALL_TAIL)) == 0)
	{
		func->builder->may_throw = 1;
	}
	return 1;
#endif
}

/*
 * Restore the exception handling frame after a function call.
 */
static int restore_eh_frame_after_call(jit_function_t func, int flags)
{
#if !defined(JIT_BACKEND_INTERP)
	jit_type_t type;
	jit_value_t struct_return;
	jit_block_t block;
	jit_insn_t insn;

	/* If the "nothrow", "noreturn", or "tail" flags are set, then we
	   don't need to worry about this */
	if((flags & (JIT_CALL_NOTHROW | JIT_CALL_NORETURN | JIT_CALL_TAIL)) != 0)
	{
		return 1;
	}

	/* Create the signature prototype "void (void)" */
	type = jit_type_create_signature
		(jit_abi_cdecl, jit_type_void, 0, 0, 0);
	if(!type)
	{
		return 0;
	}

	/* Set up to call the "_jit_backtrace_pop" intrinsic */
	if(!_jit_create_call_setup_insns
			(func, type, 0, 0, 0, 0, &struct_return))
	{
		jit_type_free(type);
		return 0;
	}

	/* Terminate the current block and then call "_jit_backtrace_pop" */
	block = _jit_block_create(func, 0);
	if(!block)
	{
		jit_type_free(type);
		return 0;
	}
	block->entered_via_top = 1;
	func->builder->current_block = block;
	insn = _jit_block_add_insn(block);
	if(!insn)
	{
		jit_type_free(type);
		return 0;
	}
	insn->opcode = JIT_OP_CALL_EXTERNAL;
	insn->flags = JIT_INSN_DEST_IS_NATIVE | JIT_INSN_VALUE1_IS_NAME;
	insn->dest = (jit_value_t)(void *)_jit_backtrace_pop;
	insn->value1 = (jit_value_t)"_jit_backtrace_pop";

	/* Clean up after the function call */
	if(!_jit_create_call_return_insns(func, type, 0, 0, 0, 0))
	{
		jit_type_free(type);
		return 0;
	}

	/* Everything is back to where it should be */
	jit_type_free(type);
	return 1;
#else /* JIT_BACKEND_INTERP */
	/* The interpreter handles exception frames for us */
	return 1;
#endif
}

/*@
 * @deftypefun jit_value_t jit_insn_call (jit_function_t func, {const char *} name, jit_function_t jit_func, jit_type_t signature, {jit_value_t *} args, {unsigned int} num_args, int flags)
 * Call the function @code{jit_func}, which may or may not be translated yet.
 * The @code{name} is for diagnostic purposes only, and can be NULL.
 *
 * If @code{signature} is NULL, then the actual signature of @code{jit_func}
 * is used in its place.  This is the usual case.  However, if the function
 * takes a variable number of arguments, then you may need to construct
 * an explicit signature for the non-fixed argument values.
 *
 * The @code{flags} parameter specifies additional information about the
 * type of call to perform:
 *
 * @table @code
 * @vindex JIT_CALL_NOTHROW
 * @item JIT_CALL_NOTHROW
 * The function never throws exceptions.
 *
 * @vindex JIT_CALL_NORETURN
 * @item JIT_CALL_NORETURN
 * The function will never return directly to its caller.  It may however
 * return to the caller indirectly by throwing an exception that the
 * caller catches.
 *
 * @vindex JIT_CALL_TAIL
 * @item JIT_CALL_TAIL
 * Apply tail call optimizations, as the result of this function
 * call will be immediately returned from the containing function.
 * Tail calls are only appropriate when the signature of the called
 * function matches the callee, and none of the parameters point
 * to local variables.
 * @end table
 *
 * If @code{jit_func} has already been compiled, then @code{jit_insn_call}
 * may be able to intuit some of the above flags for itself.  Otherwise
 * it is up to the caller to determine when the flags may be appropriate.
 * @end deftypefun
@*/
jit_value_t jit_insn_call
	(jit_function_t func, const char *name, jit_function_t jit_func,
	 jit_type_t signature, jit_value_t *args, unsigned int num_args, int flags)
{
	int is_nested;
	int nesting_level;
	jit_function_t temp_func;
	jit_value_t *new_args;
	jit_value_t return_value;
	jit_block_t block;
	jit_insn_t insn;

	/* Bail out if there is something wrong with the parameters */
	if(!func || !jit_func)
	{
		return 0;
	}

	/* Get the default signature from "jit_func" */
	if(!signature)
	{
		signature = jit_func->signature;
	}

	/* Determine the nesting relationship with the current function */
	if(jit_func->nested_parent)
	{
		is_nested = 1;
		if(jit_func->nested_parent == func)
		{
			/* We are calling one of our children */
			nesting_level = -1;
		}
		else if(jit_func->nested_parent == func->nested_parent)
		{
			/* We are calling one of our direct siblings */
			nesting_level = 0;
		}
		else
		{
			/* Search up to find the actual nesting level */
			temp_func = func->nested_parent;
			nesting_level = 1;
			while(temp_func != 0 && temp_func != jit_func)
			{
				++nesting_level;
				temp_func = temp_func->nested_parent;
			}
		}
	}
	else
	{
		is_nested = 0;
		nesting_level = 0;
	}

	/* Convert the arguments to the actual parameter types */
	if(num_args > 0)
	{
		new_args = (jit_value_t *)alloca(sizeof(jit_value_t) * num_args);
		if(!convert_call_parameters(func, signature, args, num_args, new_args))
		{
			return 0;
		}
	}
	else
	{
		new_args = args;
	}

	/* Intuit additional flags from "jit_func" if it was already compiled */
	if(func->no_throw)
	{
		flags |= JIT_CALL_NOTHROW;
	}
	if(func->no_return)
	{
		flags |= JIT_CALL_NORETURN;
	}

	/* Set up exception frame information for the call */
	if(!setup_eh_frame_for_call(func, flags))
	{
		return 0;
	}

	/* Create the instructions to push the parameters onto the stack */
	if(!_jit_create_call_setup_insns
			(func, signature, new_args, num_args,
			 is_nested, nesting_level, &return_value))
	{
		return 0;
	}

	/* Functions that call out are not leaves */
	func->builder->non_leaf = 1;

	/* Start a new block and output the "call" instruction */
	block = _jit_block_create(func, 0);
	if(!block)
	{
		return 0;
	}
	block->entered_via_top = 1;
	func->builder->current_block = block;
	insn = _jit_block_add_insn(block);
	if(!insn)
	{
		return 0;
	}
	insn->opcode = JIT_OP_CALL;
	insn->flags = JIT_INSN_DEST_IS_FUNCTION | JIT_INSN_VALUE1_IS_NAME;
	insn->dest = (jit_value_t)jit_func;
	insn->value1 = (jit_value_t)name;

	/* If the function does not return, then end the current block.
	   The next block does not have "entered_via_top" set so that
	   it will be eliminated during later code generation */
	if((flags & JIT_CALL_NORETURN) != 0)
	{
		block = _jit_block_create(func, 0);
		if(!block)
		{
			return 0;
		}
		func->builder->current_block = block;
	}

	/* Create space for the return value, if we don't already have one */
	if(!return_value)
	{
		return_value = jit_value_create(func, jit_type_get_return(signature));
		if(!return_value)
		{
			return 0;
		}
	}

	/* Create the instructions necessary to move the return value into place */
	if(!_jit_create_call_return_insns
			(func, signature, new_args, num_args, return_value, is_nested))
	{
		return 0;
	}

	/* Restore exception frame information after the call */
	if(!restore_eh_frame_after_call(func, flags))
	{
		return 0;
	}

	/* Return the value containing the result to the caller */
	return return_value;
}

/*@
 * @deftypefun jit_value_t jit_insn_call_indirect (jit_function_t func, jit_value_t value, jit_type_t signature, {jit_value_t *} args, {unsigned int} num_args, int flags)
 * Call a function via an indirect pointer.
 * @end deftypefun
@*/
jit_value_t jit_insn_call_indirect
		(jit_function_t func, jit_value_t value, jit_type_t signature,
		 jit_value_t *args, unsigned int num_args, int flags)
{
	jit_value_t *new_args;
	jit_value_t return_value;
	jit_block_t block;
	jit_insn_t insn;

	/* Bail out if there is something wrong with the parameters */
	if(!func || !value || !signature)
	{
		return 0;
	}

	/* Convert the arguments to the actual parameter types */
	if(num_args > 0)
	{
		new_args = (jit_value_t *)alloca(sizeof(jit_value_t) * num_args);
		if(!convert_call_parameters(func, signature, args, num_args, new_args))
		{
			return 0;
		}
	}
	else
	{
		new_args = args;
	}

	/* Set up exception frame information for the call */
	if(!setup_eh_frame_for_call(func, flags))
	{
		return 0;
	}

	/* Create the instructions to push the parameters onto the stack */
	if(!_jit_create_call_setup_insns
			(func, signature, new_args, num_args, 0, 0, &return_value))
	{
		return 0;
	}

	/* Move the indirect pointer value into an appropriate register */
	if(!_jit_setup_indirect_pointer(func, value))
	{
		return 0;
	}

	/* Functions that call out are not leaves */
	func->builder->non_leaf = 1;

	/* Start a new block and output the "call_indirect" instruction */
	block = _jit_block_create(func, 0);
	if(!block)
	{
		return 0;
	}
	block->entered_via_top = 1;
	func->builder->current_block = block;
	insn = _jit_block_add_insn(block);
	if(!insn)
	{
		return 0;
	}
	jit_value_ref(func, value);
	insn->opcode = JIT_OP_CALL_INDIRECT;
	insn->value1 = value;

	/* If the function does not return, then end the current block.
	   The next block does not have "entered_via_top" set so that
	   it will be eliminated during later code generation */
	if((flags & JIT_CALL_NORETURN) != 0)
	{
		block = _jit_block_create(func, 0);
		if(!block)
		{
			return 0;
		}
		func->builder->current_block = block;
	}

	/* Create space for the return value, if we don't already have one */
	if(!return_value)
	{
		return_value = jit_value_create(func, jit_type_get_return(signature));
		if(!return_value)
		{
			return 0;
		}
	}

	/* Create the instructions necessary to move the return value into place */
	if(!_jit_create_call_return_insns
			(func, signature, new_args, num_args, return_value, 0))
	{
		return 0;
	}

	/* Restore exception frame information after the call */
	if(!restore_eh_frame_after_call(func, flags))
	{
		return 0;
	}

	/* Return the value containing the result to the caller */
	return return_value;
}

/*@
 * @deftypefun jit_value_t jit_insn_call_indirect_vtable (jit_function_t func, jit_value_t value, jit_type_t signature, {jit_value_t *} args, {unsigned int} num_args, int flags)
 * Call a function via an indirect pointer.  This version differs from
 * @code{jit_insn_call_indirect} in that we assume that @code{value}
 * contains a pointer that resulted from calling
 * @code{jit_function_to_vtable_pointer}.  Indirect vtable pointer
 * calls may be more efficient on some platforms than regular indirect calls.
 * @end deftypefun
@*/
jit_value_t jit_insn_call_indirect_vtable
	(jit_function_t func, jit_value_t value, jit_type_t signature,
	 jit_value_t *args, unsigned int num_args, int flags)
{
	jit_value_t *new_args;
	jit_value_t return_value;
	jit_block_t block;
	jit_insn_t insn;

	/* Bail out if there is something wrong with the parameters */
	if(!func || !value || !signature)
	{
		return 0;
	}

	/* Convert the arguments to the actual parameter types */
	if(num_args > 0)
	{
		new_args = (jit_value_t *)alloca(sizeof(jit_value_t) * num_args);
		if(!convert_call_parameters(func, signature, args, num_args, new_args))
		{
			return 0;
		}
	}
	else
	{
		new_args = args;
	}

	/* Set up exception frame information for the call */
	if(!setup_eh_frame_for_call(func, flags))
	{
		return 0;
	}

	/* Create the instructions to push the parameters onto the stack */
	if(!_jit_create_call_setup_insns
			(func, signature, new_args, num_args, 0, 0, &return_value))
	{
		return 0;
	}

	/* Move the indirect pointer value into an appropriate register */
	if(!_jit_setup_indirect_pointer(func, value))
	{
		return 0;
	}

	/* Functions that call out are not leaves */
	func->builder->non_leaf = 1;

	/* Start a new block and output the "call_vtable_ptr" instruction */
	block = _jit_block_create(func, 0);
	if(!block)
	{
		return 0;
	}
	block->entered_via_top = 1;
	func->builder->current_block = block;
	insn = _jit_block_add_insn(block);
	if(!insn)
	{
		return 0;
	}
	jit_value_ref(func, value);
	insn->opcode = JIT_OP_CALL_VTABLE_PTR;
	insn->value1 = value;

	/* If the function does not return, then end the current block.
	   The next block does not have "entered_via_top" set so that
	   it will be eliminated during later code generation */
	if((flags & JIT_CALL_NORETURN) != 0)
	{
		block = _jit_block_create(func, 0);
		if(!block)
		{
			return 0;
		}
		func->builder->current_block = block;
	}

	/* Create space for the return value, if we don't already have one */
	if(!return_value)
	{
		return_value = jit_value_create(func, jit_type_get_return(signature));
		if(!return_value)
		{
			return 0;
		}
	}

	/* Create the instructions necessary to move the return value into place */
	if(!_jit_create_call_return_insns
			(func, signature, new_args, num_args, return_value, 0))
	{
		return 0;
	}

	/* Restore exception frame information after the call */
	if(!restore_eh_frame_after_call(func, flags))
	{
		return 0;
	}

	/* Return the value containing the result to the caller */
	return return_value;
}

/*@
 * @deftypefun jit_value_t jit_insn_call_native (jit_function_t func, {const char *} name, {void *} native_func, jit_type_t signature, {jit_value_t *} args, {unsigned int} num_args, int exception_return, int flags)
 * Output an instruction that calls an external native function.
 * The @code{name} is for diagnostic purposes only, and can be NULL.
 * @end deftypefun
@*/
jit_value_t jit_insn_call_native
	(jit_function_t func, const char *name, void *native_func,
	 jit_type_t signature, jit_value_t *args, unsigned int num_args, int flags)
{
	jit_value_t *new_args;
	jit_value_t return_value;
	jit_block_t block;
	jit_insn_t insn;

	/* Bail out if there is something wrong with the parameters */
	if(!func || !native_func || !signature)
	{
		return 0;
	}

	/* Convert the arguments to the actual parameter types */
	if(num_args > 0)
	{
		new_args = (jit_value_t *)alloca(sizeof(jit_value_t) * num_args);
		if(!convert_call_parameters(func, signature, args, num_args, new_args))
		{
			return 0;
		}
	}
	else
	{
		new_args = args;
	}

	/* Set up exception frame information for the call */
	if(!setup_eh_frame_for_call(func, flags))
	{
		return 0;
	}

	/* Create the instructions to push the parameters onto the stack */
	if(!_jit_create_call_setup_insns
			(func, signature, new_args, num_args, 0, 0, &return_value))
	{
		return 0;
	}

	/* Functions that call out are not leaves */
	func->builder->non_leaf = 1;

	/* Start a new block and output the "call_external" instruction */
	block = _jit_block_create(func, 0);
	if(!block)
	{
		return 0;
	}
	block->entered_via_top = 1;
	func->builder->current_block = block;
	insn = _jit_block_add_insn(block);
	if(!insn)
	{
		return 0;
	}
	insn->opcode = JIT_OP_CALL_EXTERNAL;
	insn->flags = JIT_INSN_DEST_IS_NATIVE | JIT_INSN_VALUE1_IS_NAME;
	insn->dest = (jit_value_t)native_func;
	insn->value1 = (jit_value_t)name;

	/* If the function does not return, then end the current block.
	   The next block does not have "entered_via_top" set so that
	   it will be eliminated during later code generation */
	if((flags & JIT_CALL_NORETURN) != 0)
	{
		block = _jit_block_create(func, 0);
		if(!block)
		{
			return 0;
		}
		func->builder->current_block = block;
	}

	/* Create space for the return value, if we don't already have one */
	if(!return_value)
	{
		return_value = jit_value_create(func, jit_type_get_return(signature));
		if(!return_value)
		{
			return 0;
		}
	}

	/* Create the instructions necessary to move the return value into place */
	if(!_jit_create_call_return_insns
			(func, signature, new_args, num_args, return_value, 0))
	{
		return 0;
	}

	/* Restore exception frame information after the call */
	if(!restore_eh_frame_after_call(func, flags))
	{
		return 0;
	}

	/* Return the value containing the result to the caller */
	return return_value;
}

/*@
 * @deftypefun jit_value_t jit_insn_call_intrinsic (jit_function_t func, {const char *} name, {void *} intrinsic_func, {const jit_intrinsic_descr_t *} descriptor, jit_value_t arg1, jit_value_t arg2)
 * Output an instruction that calls an intrinsic function.  The descriptor
 * contains the following fields:
 *
 * @table @code
 * @item return_type
 * The type of value that is returned from the intrinsic.
 *
 * @item ptr_result_type
 * This should be NULL for an ordinary intrinsic, or the result type
 * if the intrinsic reports exceptions.
 *
 * @item arg1_type
 * The type of the first argument.
 *
 * @item arg2_type
 * The type of the second argument, or NULL for a unary intrinsic.
 * @end table
 *
 * If all of the arguments are constant, then @code{jit_insn_call_intrinsic}
 * will call the intrinsic directly to calculate the constant result.
 * If the constant computation will result in an exception, then
 * code is output to cause the exception at runtime.
 *
 * The @code{name} is for diagnostic purposes only, and can be NULL.
 * @end deftypefun
@*/
jit_value_t jit_insn_call_intrinsic
		(jit_function_t func, const char *name, void *intrinsic_func,
		 const jit_intrinsic_descr_t *descriptor,
		 jit_value_t arg1, jit_value_t arg2)
{
	jit_type_t signature;
	jit_type_t param_types[3];
	jit_value_t param_values[3];
	unsigned int num_params;
	jit_value_t return_value;
	jit_value_t temp_value;
	jit_value_t cond_value;
	jit_label_t label;
	jit_constant_t const1;
	jit_constant_t const2;
	jit_constant_t return_const;
	jit_constant_t temp_const;
	void *apply_args[3];

	/* Ensure that we have a builder for this function */
	if(!_jit_function_ensure_builder(func))
	{
		return 0;
	}

	/* Coerce the arguments to the desired types */
	arg1 = jit_insn_convert(func, arg1, descriptor->arg1_type, 0);
	if(!arg1)
	{
		return 0;
	}
	if(arg2)
	{
		arg2 = jit_insn_convert(func, arg2, descriptor->arg2_type, 0);
		if(!arg2)
		{
			return 0;
		}
	}

	/* Allocate space for a return value if the intrinsic reports exceptions */
	if(descriptor->ptr_result_type)
	{
		return_value = jit_value_create(func, descriptor->ptr_result_type);
		if(!return_value)
		{
			return 0;
		}
	}
	else
	{
		return_value = 0;
	}

	/* Construct the signature for the intrinsic */
	num_params = 0;
	if(return_value)
	{
		/* Pass a pointer to "return_value" as the first argument */
		temp_value = jit_insn_address_of(func, return_value);
		if(!temp_value)
		{
			return 0;
		}
		param_types[num_params] = jit_value_get_type(temp_value);
		param_values[num_params] = temp_value;
		++num_params;
	}
	param_types[num_params] = jit_value_get_type(arg1);
	param_values[num_params] = arg1;
	++num_params;
	if(arg2)
	{
		param_types[num_params] = jit_value_get_type(arg2);
		param_values[num_params] = arg2;
		++num_params;
	}
	signature = jit_type_create_signature
		(jit_abi_cdecl, descriptor->return_type, param_types, num_params, 1);
	if(!signature)
	{
		return 0;
	}

	/* If the arguments are constant, then invoke the intrinsic now */
	if(jit_value_is_constant(arg1) && (!arg2 || jit_value_is_constant(arg2)))
	{
		const1 = jit_value_get_constant(arg1);
		const2 = jit_value_get_constant(arg2);
		return_const.type = descriptor->ptr_result_type;
		if(return_value)
		{
			jit_int result;
			temp_const.un.ptr_value = &return_const.un;
			apply_args[0] = &temp_const.un;
			apply_args[1] = &const1.un;
			apply_args[2] = &const2.un;
			jit_apply(signature, intrinsic_func, apply_args,
					  num_params, &result);
			if(result >= 1)
			{
				/* No exception occurred, so return the constant value */
				jit_type_free(signature);
				return jit_value_create_constant(func, &return_const);
			}
		}
		else
		{
			apply_args[0] = &const1.un;
			apply_args[1] = &const2.un;
			jit_apply(signature, intrinsic_func, apply_args,
					  num_params, &return_const.un);
			jit_type_free(signature);
			return jit_value_create_constant(func, &return_const);
		}
	}

	/* Call the intrinsic as a native function */
	temp_value = jit_insn_call_native
		(func, name, intrinsic_func, signature, param_values,
		 num_params, JIT_CALL_NOTHROW);
	if(!temp_value)
	{
		jit_type_free(signature);
		return 0;
	}
	jit_type_free(signature);

	/* If no exceptions to report, then return "temp_value" as the result */
	if(!return_value)
	{
		return temp_value;
	}

	/* Determine if an exception was reported */
	cond_value = jit_insn_ge(func, temp_value,
		jit_value_create_nint_constant(func, jit_type_int, 1));
	if(!cond_value)
	{
		return 0;
	}
	label = jit_label_undefined;
	if(!jit_insn_branch_if(func, cond_value, &label))
	{
		return 0;
	}

	/* Call the "jit_exception_builtin" function to report the exception */
	param_types[0] = jit_type_int;
	signature = jit_type_create_signature
		(jit_abi_cdecl, jit_type_void, param_types, 1, 1);
	if(!signature)
	{
		return 0;
	}
	param_values[0] = temp_value;
	jit_insn_call_native
		(func, "jit_exception_builtin",
		 (void *)jit_exception_builtin, signature, param_values, 1,
		 JIT_CALL_NORETURN);
	jit_type_free(signature);

	/* The "jit_exception_builtin" function will never return */
	func->builder->current_block->ends_in_dead = 1;

	/* Execution continues here if there was no exception */
	if(!jit_insn_label(func, &label))
	{
		return 0;
	}

	/* Return the temporary that contains the result value */
	return return_value;
}

/*@
 * @deftypefun int jit_insn_incoming_reg (jit_function_t func, jit_value_t value, int reg)
 * Output an instruction that notes that the contents of @code{value}
 * can be found in the register @code{reg} at this point in the code.
 *
 * You normally wouldn't call this yourself - it is used internally
 * by the CPU back ends to set up the function's entry frame and the
 * values of registers on return from a subroutine call.
 * @end deftypefun
@*/
int jit_insn_incoming_reg(jit_function_t func, jit_value_t value, int reg)
{
	return create_note(func, JIT_OP_INCOMING_REG, value,
					   jit_value_create_nint_constant
					   		(func, jit_type_int, (jit_nint)reg));
}

/*@
 * @deftypefun int jit_insn_incoming_frame_posn (jit_function_t func, jit_value_t value, jit_nint frame_offset)
 * Output an instruction that notes that the contents of @code{value}
 * can be found in the stack frame at @code{frame_offset} at this point
 * in the code.
 *
 * You normally wouldn't call this yourself - it is used internally
 * by the CPU back ends to set up the function's entry frame.
 * @end deftypefun
@*/
int jit_insn_incoming_frame_posn
		(jit_function_t func, jit_value_t value, jit_nint frame_offset)
{
	return create_note(func, JIT_OP_INCOMING_FRAME_POSN, value,
					   jit_value_create_nint_constant
					   		(func, jit_type_int, frame_offset));
}

/*@
 * @deftypefun int jit_insn_outgoing_reg (jit_function_t func, jit_value_t value, int reg)
 * Output an instruction that copies the contents of @code{value}
 * into the register @code{reg} at this point in the code.  This is
 * typically used just before making an outgoing subroutine call.
 *
 * You normally wouldn't call this yourself - it is used internally
 * by the CPU back ends to set up the registers for a subroutine call.
 * @end deftypefun
@*/
int jit_insn_outgoing_reg(jit_function_t func, jit_value_t value, int reg)
{
	return create_note(func, JIT_OP_OUTGOING_REG, value,
					   jit_value_create_nint_constant
					   		(func, jit_type_int, (jit_nint)reg));
}

/*@
 * @deftypefun int jit_insn_return_reg (jit_function_t func, jit_value_t value, int reg)
 * Output an instruction that notes that the contents of @code{value}
 * can be found in the register @code{reg} at this point in the code.
 * This is similar to @code{jit_insn_incoming_reg}, except that it
 * refers to return values, not parameter values.
 *
 * You normally wouldn't call this yourself - it is used internally
 * by the CPU back ends to handle returns from subroutine calls.
 * @end deftypefun
@*/
int jit_insn_return_reg(jit_function_t func, jit_value_t value, int reg)
{
	return create_note(func, JIT_OP_RETURN_REG, value,
					   jit_value_create_nint_constant
					   		(func, jit_type_int, (jit_nint)reg));
}

/*@
 * @deftypefun int jit_insn_setup_for_nested (jit_function_t func, int nested_level, int reg)
 * Output an instruction to set up for a nested function call.
 * The @code{nested_level} value will be -1 to call a child, zero to call a
 * sibling of @code{func}, 1 to call a sibling of the parent, 2 to call
 * a sibling of the grandparent, etc.  If @code{reg} is not -1, then
 * it indicates the register to receive the parent frame information.
 * If @code{reg} is -1, then the frame information will be pushed on the stack.
 *
 * You normally wouldn't call this yourself - it is used internally by the
 * CPU back ends to set up the parameters for a nested subroutine call.
 * @end deftypefun
@*/
int jit_insn_setup_for_nested(jit_function_t func, int nested_level, int reg)
{
	if(nested_level < 0)
	{
		return create_unary_note
			(func, JIT_OP_SETUP_FOR_NESTED,
			 jit_value_create_nint_constant
				 (func, jit_type_int, (jit_nint)reg));
	}
	else
	{
		return create_note
			(func, JIT_OP_SETUP_FOR_SIBLING,
			 jit_value_create_nint_constant
				 (func, jit_type_int, (jit_nint)nested_level),
			 jit_value_create_nint_constant
				 (func, jit_type_int, (jit_nint)reg));
	}
}

/*@
 * @deftypefun int jit_insn_flush_struct (jit_function_t func, jit_value_t value)
 * Flush a small structure return value out of registers and back
 * into the local variable frame.  You normally wouldn't call this
 * yourself - it is used internally by the CPU back ends to handle
 * structure returns from functions.
 * @end deftypefun
@*/
int jit_insn_flush_struct(jit_function_t func, jit_value_t value)
{
	if(value)
	{
		jit_value_set_addressable(value);
	}
	return create_unary_note(func, JIT_OP_FLUSH_SMALL_STRUCT, value);
}

/*@
 * @deftypefun jit_value_t jit_insn_import (jit_function_t func, jit_value_t value)
 * Import @code{value} from an outer nested scope into @code{func}.  Returns
 * the effective address of the value for local access via a pointer.
 * If @code{value} is local to @code{func}, then it is returned as-is.
 * Returns NULL if out of memory or the value is not accessible via a
 * parent, grandparent, or other ancestor of @code{func}.
 * @end deftypefun
@*/
jit_value_t jit_insn_import(jit_function_t func, jit_value_t value)
{
	jit_function_t value_func;
	jit_function_t current_func;
	int level;

	/* Make sure that we have a builder before we start */
	if(!_jit_function_ensure_builder(func))
	{
		return 0;
	}

	/* If the value is already local, then there is nothing to do */
	value_func = jit_value_get_function(value);
	if(value_func == func)
	{
		return value;
	}

	/* Find the nesting level of the value, where 1 is our parent */
	level = 1;
	current_func = func->nested_parent;
	while(current_func != 0 && current_func != value_func)
	{
		++level;
		current_func = current_func->nested_parent;
	}
	if(!current_func)
	{
		/* The value is not accessible from this scope */
		return 0;
	}

	/* Output the relevant import instruction, which will also cause
	   it to be marked as a non-local addressable by "jit_value_ref" */
	return apply_binary
		(func, JIT_OP_IMPORT, value,
		 jit_value_create_nint_constant(func, jit_type_int, (jit_nint)value),
		 jit_type_void_ptr);
}

/*@
 * @deftypefun int jit_insn_push (jit_function_t func, jit_value_t value)
 * Push a value onto the function call stack, in preparation for a call.
 * You normally wouldn't call this yourself - it is used internally
 * by the CPU back ends to set up the stack for a subroutine call.
 * @end deftypefun
@*/
int jit_insn_push(jit_function_t func, jit_value_t value)
{
	jit_type_t type;
	if(!value)
	{
		return 0;
	}
	type = jit_type_promote_int(jit_type_normalize(jit_value_get_type(value)));
	switch(type->kind)
	{
		case JIT_TYPE_SBYTE:
		case JIT_TYPE_UBYTE:
		case JIT_TYPE_SHORT:
		case JIT_TYPE_USHORT:
		case JIT_TYPE_INT:
		case JIT_TYPE_UINT:
		{
			return create_unary_note(func, JIT_OP_PUSH_INT, value);
		}
		/* Not reached */

		case JIT_TYPE_LONG:
		case JIT_TYPE_ULONG:
		{
			return create_unary_note(func, JIT_OP_PUSH_LONG, value);
		}
		/* Not reached */

		case JIT_TYPE_FLOAT32:
		{
			return create_unary_note(func, JIT_OP_PUSH_FLOAT32, value);
		}
		/* Not reached */

		case JIT_TYPE_FLOAT64:
		{
			return create_unary_note(func, JIT_OP_PUSH_FLOAT64, value);
		}
		/* Not reached */

		case JIT_TYPE_NFLOAT:
		{
			return create_unary_note(func, JIT_OP_PUSH_NFLOAT, value);
		}
		/* Not reached */

		case JIT_TYPE_STRUCT:
		case JIT_TYPE_UNION:
		{
			return create_unary_note(func, JIT_OP_PUSH_STRUCT, value);
		}
		/* Not reached */
	}
	return 1;
}

/*@
 * @deftypefun int jit_insn_pop_stack (jit_function_t func, jit_nint num_items)
 * Pop @code{num_items} items from the function call stack.  You normally
 * wouldn't call this yourself - it is used by CPU back ends to clean up
 * the stack after calling a subroutine.  The size of an item is specific
 * to the back end (it could be bytes, words, or some other measurement).
 * @end deftypefun
@*/
int jit_insn_pop_stack(jit_function_t func, jit_nint num_items)
{
	return create_unary_note
		(func, JIT_OP_POP_STACK,
		 jit_value_create_nint_constant(func, jit_type_nint, num_items));
}

/*@
 * @deftypefun int jit_insn_return (jit_function_t func, jit_value_t value)
 * Output an instruction to return @code{value} as the function's result.
 * If @code{value} is NULL, then the function is assumed to return
 * @code{void}.  If the function returns a structure, this will copy
 * the value into the memory at the structure return address.
 * @end deftypefun
@*/
int jit_insn_return(jit_function_t func, jit_value_t value)
{
	jit_type_t type;

	/* Ensure that we have a builder for this function */
	if(!_jit_function_ensure_builder(func))
	{
		return 0;
	}

	/* This function has an ordinary return path */
	func->builder->ordinary_return = 1;

	/* Output an appropriate instruction to return to the caller */
	type = jit_type_normalize(jit_type_get_return(func->signature));
	type = jit_type_promote_int(type);
	if(!value || type == jit_type_void)
	{
		/* Handle "finally" clauses if necessary */
		if(!handle_finally(func, JIT_OP_PREPARE_FOR_RETURN))
		{
			return 0;
		}

		/* This function returns "void" */
		if(!create_noarg_note(func, JIT_OP_RETURN))
		{
			return 0;
		}
	}
	else
	{
		/* Convert the value into the desired return type */
		value = jit_insn_convert(func, value, type, 0);
		if(!value)
		{
			return 0;
		}

		/* Handle "finally" clauses if necessary */
		if(!handle_finally(func, JIT_OP_PREPARE_FOR_RETURN))
		{
			return 0;
		}

		/* Create the "return" instruction */
		switch(type->kind)
		{
			case JIT_TYPE_SBYTE:
			case JIT_TYPE_UBYTE:
			case JIT_TYPE_SHORT:
			case JIT_TYPE_USHORT:
			case JIT_TYPE_INT:
			case JIT_TYPE_UINT:
			{
				if(!create_unary_note(func, JIT_OP_RETURN_INT, value))
				{
					return 0;
				}
			}
			break;

			case JIT_TYPE_LONG:
			case JIT_TYPE_ULONG:
			{
				if(!create_unary_note(func, JIT_OP_RETURN_LONG, value))
				{
					return 0;
				}
			}
			break;

			case JIT_TYPE_FLOAT32:
			{
				if(!create_unary_note(func, JIT_OP_RETURN_FLOAT32, value))
				{
					return 0;
				}
			}
			break;

			case JIT_TYPE_FLOAT64:
			{
				if(!create_unary_note(func, JIT_OP_RETURN_FLOAT64, value))
				{
					return 0;
				}
			}
			break;

			case JIT_TYPE_NFLOAT:
			{
				if(!create_unary_note(func, JIT_OP_RETURN_NFLOAT, value))
				{
					return 0;
				}
			}
			break;

			case JIT_TYPE_STRUCT:
			case JIT_TYPE_UNION:
			{
				jit_value_t return_ptr = jit_value_get_struct_pointer(func);
				if(return_ptr)
				{
					/* Return the structure via the supplied pointer */
					/* TODO */
				}
				else
				{
					/* Return the structure via registers */
					if(!create_unary_note
							(func, JIT_OP_RETURN_SMALL_STRUCT, value))
					{
						break;
					}
				}
			}
			break;
		}
	}

	/* Mark the current block as "ends in dead" */
	func->builder->current_block->ends_in_dead = 1;

	/* Start a new block just after the "return" instruction */
	return jit_insn_label(func, 0);
}

/*@
 * @deftypefun int jit_insn_default_return (jit_function_t func)
 * Add an instruction to return a default value if control reaches this point.
 * This is typically used at the end of a function to ensure that all paths
 * return to the caller.  Returns zero if out of memory, 1 if a default
 * return was added, and 2 if a default return was not needed.
 *
 * Note: if this returns 1, but the function signature does not return
 * @code{void}, then it indicates that a higher-level language error
 * has occurred and the function should be abandoned.
 * @end deftypefun
@*/
int jit_insn_default_return(jit_function_t func)
{
	jit_block_t current;
	jit_insn_t last;

	/* Ensure that we have a builder for this function */
	if(!_jit_function_ensure_builder(func))
	{
		return 0;
	}

	/* If the last block ends in an unconditional branch, or is dead,
	   then we don't need to add a default return */
	current = func->builder->current_block;
	if(current->ends_in_dead)
	{
		/* The block ends in dead */
		return 2;
	}
	last = _jit_block_get_last(current);
	if(last)
	{
		if(last->opcode == JIT_OP_BR)
		{
			/* This block ends in an unconditional branch */
			return 2;
		}
	}
	else if(!(current->entered_via_top) && !(current->entered_via_branch))
	{
		/* This block is never entered */
		return 2;
	}

	/* Add a simple "void" return to terminate the function */
	return jit_insn_return(func, 0);
}

/*@
 * @deftypefun int jit_insn_throw (jit_function_t func, jit_value_t value)
 * Throw a pointer @code{value} as an exception object.  This can also
 * be used to "rethrow" an object from a catch handler that is not
 * interested in handling the exception.
 * @end deftypefun
@*/
int jit_insn_throw(jit_function_t func, jit_value_t value)
{
	jit_block_t block;
	if(!_jit_function_ensure_builder(func))
	{
		return 0;
	}
	func->builder->may_throw = 1;
	func->builder->non_leaf = 1;	/* May have to call out to throw */
	if(!create_unary_note(func, JIT_OP_THROW, value))
	{
		return 0;
	}
	func->builder->current_block->ends_in_dead = 1;
	block = _jit_block_create(func, 0);
	if(!block)
	{
		return 0;
	}
	func->builder->current_block = block;
	return 1;
}

/*@
 * @deftypefun jit_value_t jit_insn_get_call_stack (jit_function_t func)
 * Get an object that represents the current position in the code,
 * and all of the functions that are currently on the call stack.
 * This is equivalent to calling @code{jit_exception_get_stack_trace},
 * and is normally used just prior to @code{jit_insn_throw} to record
 * the location of the exception that is being thrown.
 * @end deftypefun
@*/
jit_value_t jit_insn_get_call_stack(jit_function_t func)
{
	jit_type_t type;
	jit_value_t value;

	/* Create a signature prototype for "void *()" */
	type = jit_type_create_signature
		(jit_abi_cdecl, jit_type_void_ptr, 0, 0, 1);
	if(!type)
	{
		return 0;
	}

	/* Call "jit_exception_get_stack_trace" to obtain the stack trace */
	value = jit_insn_call_native
		(func, "jit_exception_get_stack_trace",
		 (void *)jit_exception_get_stack_trace, type, 0, 0, 0);

	/* Clean up and exit */
	jit_type_free(type);
	return value;
}

/*@
 * @deftypefun int jit_insn_start_try (jit_function_t func, {jit_label_t *} catch_label, {jit_label_t *} finally_label, int finally_on_fault)
 * Start an exception-handling @code{try} block at the current position
 * within @code{func}.
 *
 * Whenever an exception occurs in the block, execution will jump to
 * @code{catch_label}.  When execution exits the @code{try} block's scope,
 * @code{finally_label} will be called.  If @code{finally_on_fault} is
 * non-zero, then @code{finally_label} will be called for exceptions,
 * but not when control exits the @code{try} block normally.
 *
 * The @code{finally_label} parameter may be NULL if the @code{try} block
 * does not have a @code{finally} clause associated with it.
 *
 * All of the blocks between @code{jit_insn_start_try} and
 * @code{jit_insn_start_catch} are covered by the @code{catch}
 * clause and the @code{finally} clause.  Blocks between
 * @code{jit_insn_start_catch} and @code{jit_insn_end_try} are
 * covered by the @code{finally} clause.
 *
 * Calls to @code{jit_insn_branch}, @code{jit_insn_return},
 * @code{jit_insn_throw} and @code{jit_insn_rethrow} will cause
 * additional code to be output to invoke the relevant @code{finally}
 * clauses.
 *
 * The destinations for @code{jit_insn_branch_if} and
 * @code{jit_insn_branch_if_not} must never be outside the current
 * @code{finally} context.  You should always use @code{jit_insn_branch}
 * to branch out of @code{finally} contexts.
 *
 * Note: you don't need to output calls to @code{finally} clauses
 * yourself as @code{libjit} can detect when it is necessary on its own.
 * @end deftypefun
@*/
int jit_insn_start_try
	(jit_function_t func, jit_label_t *catch_label,
	 jit_label_t *finally_label, int finally_on_fault)
{
	jit_block_eh_t eh;
	jit_block_t block;

	/* Ensure that we have a function builder */
	if(!_jit_function_ensure_builder(func) || !catch_label)
	{
		return 0;
	}

	/* Allocate the label numbers */
	if(*catch_label == jit_label_undefined)
	{
		*catch_label = (func->builder->next_label)++;
	}
	if(finally_label && *finally_label == jit_label_undefined)
	{
		*finally_label = (func->builder->next_label)++;
	}

	/* This function has a "try" block.  This flag helps native
	   back ends know when they must be careful about global
	   register allocation */
	func->builder->has_try = 1;

	/* Anything with a finally handler makes the function not a leaf,
	   because we may need to do a native "call" to invoke the handler */
	if(finally_label)
	{
		func->builder->non_leaf = 1;
	}

	/* Create a new exception handling context and populate it */
	eh = jit_cnew(struct jit_block_eh);
	if(!eh)
	{
		return 0;
	}
	eh->parent = func->builder->current_handler;
	eh->next = func->builder->exception_handlers;
	func->builder->exception_handlers = eh;
	eh->catch_label = *catch_label;
	eh->finally_label = (finally_label ? *finally_label : jit_label_undefined);
	eh->finally_on_fault = finally_on_fault;
	eh->in_try_body = 1;
	func->builder->current_handler = eh;

	/* Start a new block, for the body of the "try" */
	block = _jit_block_create(func, 0);
	if(!block)
	{
		return 0;
	}
	block->entered_via_top = 1;
	return 1;
}

/*@
 * @deftypefun jit_value_t jit_insn_start_catch (jit_function_t func, {jit_label_t *} catch_label)
 * Start the @code{catch} clause for the currently active @code{try}
 * block.  Returns a pointer @code{value} that indicates the exception
 * that is thrown.
 *
 * There can be only one @code{catch} clause per @code{try} block.
 * The front end is responsible for outputting instructions that
 * inspect the exception object, determine if it is appropriate to
 * the clause, and then either handle it or rethrow it.
 *
 * Every @code{try} block must have an associated @code{catch} clause,
 * even if all it does is rethrow the exception immediately.  Without a
 * @code{catch} clause, the correct @code{finally} logic will not be
 * performed for thrown exceptions.
 * @end deftypefun
@*/
jit_value_t jit_insn_start_catch
	(jit_function_t func, jit_label_t *catch_label)
{
	jit_block_eh_t eh;
	jit_block_eh_t new_eh;

	/* Ensure that we have a function builder */
	if(!_jit_function_ensure_builder(func))
	{
		return 0;
	}

	/* Create a new exception handler to indicate that we are in a "catch" */
	eh = func->builder->current_handler;
	if(!eh)
	{
		return 0;
	}
	new_eh = jit_cnew(struct jit_block_eh);
	if(!new_eh)
	{
		return 0;
	}
	new_eh->parent = eh->parent;
	new_eh->next = func->builder->exception_handlers;
	func->builder->exception_handlers = new_eh;
	if(eh->finally_label != jit_label_undefined)
	{
		/* We will need a mini "catch" block to call the "finally"
		   clause when an exception occurs within the "catch" */
		new_eh->catch_label = (func->builder->next_label)++;
	}
	else
	{
		new_eh->catch_label = jit_label_undefined;
	}
	new_eh->finally_label = eh->finally_label;
	new_eh->finally_on_fault = eh->finally_on_fault;
	new_eh->in_try_body = 0;
	func->builder->current_handler = new_eh;

	/* Start a new block for the "catch" clause */
	if(!jit_insn_label(func, catch_label))
	{
		return 0;
	}

	/* Output an instruction to notice the caught value */
	return create_dest_note(func, JIT_OP_ENTER_CATCH, jit_type_void_ptr);
}

/*@
 * @deftypefun int jit_insn_end_try (jit_function_t func)
 * Mark the end of the @code{try} block and its associated @code{catch}
 * clause.  This is normally followed by a call to
 * @code{jit_insn_start_finally} to define the @code{finally} clause.
 * @end deftypefun
@*/
int jit_insn_end_try(jit_function_t func)
{
	jit_block_eh_t eh;
	jit_block_t block;
	jit_value_t value;

	/* Ensure that we have a function builder */
	if(!_jit_function_ensure_builder(func))
	{
		return 0;
	}

	/* Get the current exception handling context */
	eh = func->builder->current_handler;
	if(!eh)
	{
		return 0;
	}

	/* We may need another mini "catch" block to invoke the "finally"
	   clause for exceptions that happen during the "catch" */
	if(eh->catch_label != jit_label_undefined)
	{
		/* TODO: not quite right */
		if(!jit_insn_label(func, &(eh->catch_label)))
		{
			return 0;
		}
		value = create_dest_note(func, JIT_OP_ENTER_CATCH, jit_type_void_ptr);
		if(!value)
		{
			return 0;
		}
		if(!jit_insn_throw(func, value))
		{
			return 0;
		}
	}

	/* Pop the current exception context */
	func->builder->current_handler = eh->parent;

	/* Start a new block, covered by the next outer "try" context */
	block = _jit_block_create(func, 0);
	if(!block)
	{
		return 0;
	}
	return 1;
}

/*@
 * @deftypefun int jit_insn_start_finally (jit_function_t func, {jit_label_t *} finally_label)
 * Start the @code{finally} clause for the preceding @code{try} block.
 * @end deftypefun
@*/
int jit_insn_start_finally(jit_function_t func, jit_label_t *finally_label)
{
	if(!jit_insn_label(func, finally_label))
	{
		return 0;
	}
	return create_noarg_note(func, JIT_OP_ENTER_FINALLY);
}

/*@
 * @deftypefun int jit_insn_return_from_finally (jit_function_t func)
 * Return from the @code{finally} clause to where it was called from.
 * This is usually the last instruction in a @code{finally} clause.
 * @end deftypefun
@*/
int jit_insn_return_from_finally(jit_function_t func)
{
	jit_block_t block;

	/* Mark the end of the "finally" clause */
	if(!create_noarg_note(func, JIT_OP_LEAVE_FINALLY))
	{
		return 0;
	}

	/* The current block ends in a dead instruction */
	func->builder->current_block->ends_in_dead = 1;

	/* Create a new block for the following code */
	block = _jit_block_create(func, 0);
	if(!block)
	{
		return 0;
	}
	return 1;
}

/*@
 * @deftypefun jit_value_t jit_insn_start_filter (jit_function_t func, {jit_label_t *} label, jit_type_t type)
 * Define the start of a filter.  Filters are embedded subroutines within
 * functions that are used to filter exceptions in @code{catch} blocks.
 *
 * A filter subroutine takes a single argument (usually a pointer) and
 * returns a single result (usually a boolean).  The filter has complete
 * access to the local variables of the function, and can use any of
 * them in the filtering process.
 *
 * This function returns a temporary value of the specified @code{type},
 * indicating the parameter that is supplied to the filter.
 * @end deftypefun
@*/
jit_value_t jit_insn_start_filter
	(jit_function_t func, jit_label_t *label, jit_type_t type)
{
	/* Set a label at this point to start a new block */
	if(!jit_insn_label(func, label))
	{
		return 0;
	}

	/* Create a note to load the filter's parameter at runtime */
	return create_dest_note(func, JIT_OP_ENTER_FILTER, type);
}

/*@
 * @deftypefun int jit_insn_return_from_filter (jit_function_t func, jit_value_t value)
 * Return from a filter subroutine with the specified @code{value} as
 * its result.
 * @end deftypefun
@*/
int jit_insn_return_from_filter(jit_function_t func, jit_value_t value)
{
	jit_block_t block;

	/* Mark the end of the "filter" clause */
	if(!create_unary_note(func, JIT_OP_LEAVE_FILTER, value))
	{
		return 0;
	}

	/* The current block ends in a dead instruction */
	func->builder->current_block->ends_in_dead = 1;

	/* Create a new block for the following code */
	block = _jit_block_create(func, 0);
	if(!block)
	{
		return 0;
	}
	return 1;
}

/*@
 * @deftypefun jit_value_t jit_insn_call_filter (jit_function_t func, {jit_label_t *} label, jit_value_t value, jit_type_t type)
 * Call the filter subroutine at @code{label}, passing it @code{value} as
 * its argument.  This function returns a value of the specified
 * @code{type}, indicating the filter's result.
 * @end deftypefun
@*/
jit_value_t jit_insn_call_filter
	(jit_function_t func, jit_label_t *label,
	 jit_value_t value, jit_type_t type)
{
	jit_block_t block;
	jit_insn_t insn;

	/* Ensure that we have a function builder */
	if(!_jit_function_ensure_builder(func))
	{
		return 0;
	}

	/* Allocate the label number if necessary */
	if(*label == jit_label_undefined)
	{
		*label = (func->builder->next_label)++;
	}

	/* Calling a filter makes the function not a leaf because we may
	   need to do a native "call" to invoke the handler */
	func->builder->non_leaf = 1;

	/* Add a new branch instruction to branch to the filter */
	insn = _jit_block_add_insn(func->builder->current_block);
	if(!insn)
	{
		return 0;
	}
	jit_value_ref(func, value);
	insn->opcode = (short)JIT_OP_CALL_FILTER;
	insn->flags = JIT_INSN_DEST_IS_LABEL;
	insn->dest = (jit_value_t)(*label);
	insn->value1 = value;

	/* Create a new block, and add the filter return logic to it */
	block = _jit_block_create(func, 0);
	if(!block)
	{
		return 0;
	}
	block->entered_via_top = 1;
	return create_dest_note(func, JIT_OP_CALL_FILTER_RETURN, type);
}

/*@
 * @deftypefun void jit_insn_iter_init ({jit_insn_iter_t *} iter, jit_block_t block)
 * Initialize an iterator to point to the first instruction in @code{block}.
 * @end deftypefun
@*/
void jit_insn_iter_init(jit_insn_iter_t *iter, jit_block_t block)
{
	iter->block = block;
	iter->posn = block->first_insn;
}

/*@
 * @deftypefun void jit_insn_iter_init_last ({jit_insn_iter_t *} iter, jit_block_t block)
 * Initialize an iterator to point to the last instruction in @code{block}.
 * @end deftypefun
@*/
void jit_insn_iter_init_last(jit_insn_iter_t *iter, jit_block_t block)
{
	iter->block = block;
	iter->posn = block->last_insn + 1;
}

/*@
 * @deftypefun jit_insn_t jit_insn_iter_next ({jit_insn_iter_t *} iter)
 * Get the next instruction in an iterator's block.  Returns NULL
 * when there are no further instructions in the block.
 * @end deftypefun
@*/
jit_insn_t jit_insn_iter_next(jit_insn_iter_t *iter)
{
	if(iter->posn <= iter->block->last_insn)
	{
		return iter->block->func->builder->insns[(iter->posn)++];
	}
	else
	{
		return 0;
	}
}

/*@
 * @deftypefun jit_insn_t jit_insn_iter_previous ({jit_insn_iter_t *} iter)
 * Get the previous instruction in an iterator's block.  Returns NULL
 * when there are no further instructions in the block.
 * @end deftypefun
@*/
jit_insn_t jit_insn_iter_previous(jit_insn_iter_t *iter)
{
	if(iter->posn > iter->block->first_insn)
	{
		return iter->block->func->builder->insns[--(iter->posn)];
	}
	else
	{
		return 0;
	}
}