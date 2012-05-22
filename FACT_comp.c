/* This file is part of FACT.
 *
 * FACT is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * FACT is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with FACT. If not, see <http://www.gnu.org/licenses/>.
 */

#include "FACT.h"
#include "FACT_vm.h"
#include "FACT_opcodes.h"
#include "FACT_parser.h"
#include "FACT_lexer.h"
#include "FACT_mpc.h"
#include "FACT_alloc.h"

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <assert.h>

/* HC SVNT DRACONES
 * Let me elaborate: this code is really bad. A mix of gobbeldy gook and such.
 * I'm guessing it's almost impossible to understand. Someday I'll fix it.
 * Hopefully.
 */

/* Intermediete format created by the compiler. Essentially a tree. */
struct inter_node {
  enum {
    INSTRUCTION,
    GROUPING
  } node_type;
  size_t line; /* Line the instruction relates to. 0 = none. */ 
  struct inter_node *next;
  union {
    struct { /* Single instruction. */
      enum Furlow_opcode inst_val;
      struct inst_arg { /* Arguments. */
	enum {
	  NO_ARG = 0,
	  REG_VAL,
	  ADDR_VAL,
	  INT_VAL,
	  STR_VAL
	} arg_type;
	union {
	  size_t addr;
	  unsigned char reg;
	  char *str;
	} arg_val;
      } args[4]; /* Max of four arguments. */
    } inst;
    struct { /* Grouping. */
      struct inter_node **children;
      size_t num_children;
    } grouping;
  } node_val;
};

static char *strlen_dq (char *);
static char *strlen_sq (char *);
static struct inter_node *create_node ();
static struct inter_node *compile_tree (FACT_tree_t, size_t, size_t, bool);
static struct inter_node *compile_args (FACT_tree_t);
static struct inter_node *compile_array_dec (FACT_tree_t);
static struct inter_node *compile_str_dq (char *, size_t);
static struct inter_node *compile_str_sq (char *, size_t);

static struct inter_node *begin_temp_scope ();
static struct inter_node *end_temp_scope ();

static struct inter_node *set_return_val ();

static void load (struct inter_node *, struct inter_node *, const char *);
static size_t weight (struct inter_node *);
static inline void spread (char *, size_t);

static inline void push_const (struct inter_node *, char *);

void FACT_compile (FACT_tree_t tree, const char *file_name, bool set_rx)
{
#ifdef ADVANCED_THREADING
  /* Lock the program for offset consistency. */
  Furlow_lock_program ();
#endif /* ADVANCED_THREADING */

  /* Compile and load. */
  load (compile_tree (tree, 1, 0, set_rx), NULL, file_name);

#ifdef ADVANCED_THREADING
  /* Unlock the program. */
  Furlow_unlock_program ();
#endif /* ADVANCED_THREADING */
}

static inline void add_instruction (struct inter_node *group, int inst,
				    struct inst_arg arg1,
				    struct inst_arg arg2,
				    struct inst_arg arg3) /* Add an instruction to a group. */
{
  int inst_n;

  inst_n = group->node_val.grouping.num_children++;
  group->node_val.grouping.children[inst_n] = create_node ();
  group->node_val.grouping.children[inst_n]->node_type = INSTRUCTION;
  group->node_val.grouping.children[inst_n]->node_val.inst.inst_val = inst;
  group->node_val.grouping.children[inst_n]->node_val.inst.args[0] = arg1;
  group->node_val.grouping.children[inst_n]->node_val.inst.args[1] = arg2;
  group->node_val.grouping.children[inst_n]->node_val.inst.args[2] = arg3;
}

static inline void set_child (struct inter_node *group, struct inter_node *val)
{
  int inst_n;

  inst_n = group->node_val.grouping.num_children++;
  group->node_val.grouping.children[inst_n] = val;
}

/* The following functions are inlines that facilitate the passing of
 * arguments to add_instruction.
 */
static inline struct inst_arg ignore (void) /* No argument */
{
  struct inst_arg ret;
  ret.arg_type = NO_ARG;
  return ret;
}

static inline struct inst_arg reg_arg (unsigned char reg_num) /* Register value. */
{
  struct inst_arg ret;
  ret.arg_type = REG_VAL;
  ret.arg_val.reg = reg_num;
  return ret;
}

static inline struct inst_arg addr_arg (size_t node_num) /* Address value. */
{
  struct inst_arg ret;
  ret.arg_type = ADDR_VAL;
  ret.arg_val.addr = node_num;
  return ret;
}

static inline struct inst_arg int_arg (size_t num) /* Integer value. */
{
  struct inst_arg ret;
  ret.arg_type = INT_VAL;
  ret.arg_val.addr = num; /* Just use addr. I'm so sleepy. */
  return ret;
}

static inline struct inst_arg str_arg (char *str_val) /* String value. */
{
  struct inst_arg ret;
  ret.arg_type = STR_VAL;
  ret.arg_val.str = str_val;
  return ret;
}

/* Since I do not know of any compilation techniques, so it's sort of just ad-hoc.
 * TODO: add argument checking for functions.
 * Also, clean this up with some macros/inline functions.
 */
static struct inter_node *compile_tree (FACT_tree_t curr,
					size_t s_count,
					size_t l_count,
					bool set_rx) /* Compile a tree, recursively. */
{
  struct inter_node *res;
  char *dims_str, *elems_str;
  size_t i, j;
  size_t dims, elems;
  FACT_tree_t n;

  static Furlow_opc_t lookup_table [] = {
    [E_ADD] = ADD,
    [E_SUB] = SUB,
    [E_MUL] = MUL,
    [E_DIV] = DIV,
    [E_MOD] = MOD,
    [E_ADD_AS] = ADD,
    [E_SUB_AS] = SUB,
    [E_MUL_AS] = MUL,
    [E_DIV_AS] = DIV,
    [E_MOD_AS] = MOD,
    [E_NE] = CNE,
    [E_EQ] = CEQ,
    [E_MT] = CMT,
    [E_ME] = CME,
    [E_LT] = CLT,
    [E_LE] = CLE,
    [E_LOCAL_CHECK] = IS_AUTO,
    [E_GLOBAL_CHECK] = IS_DEF,
  };

  if (curr == NULL)
    return NULL;

  res = create_node ();
  res->line = curr->line;
  switch (curr->id.id) {
  case E_VAR:
    res->node_type = INSTRUCTION;
    if (!strcmp (curr->id.lexem, "this"))
      res->node_val.inst.inst_val = THIS;
    else if (!strcmp (curr->id.lexem, "lambda"))
      res->node_val.inst.inst_val = LAMBDA;
    else {
      res->node_val.inst.inst_val = VAR;
      res->node_val.inst.args[0] = str_arg (curr->id.lexem);
    }
    break;

  case E_NUM:
#if 0
    res->node_type = INSTRUCTION;
    res->node_val.inst.inst_val = CONSTS;
    res->node_val.inst.args[0] = str_arg (curr->id.lexem);
#endif
    res->node_type = GROUPING;
    res->node_val.grouping.children = FACT_malloc (sizeof (struct inter_node *));
    push_const (res, curr->id.lexem);
    break;

  case E_SQ:
  case E_DQ:
    res->node_type = GROUPING;
    res->node_val.grouping.children = FACT_malloc (sizeof (struct inter_node *) * 6);
    
    /* Push the length of the string, as a string. */
    /*
    add_instruction (res, CONST, str_arg ((curr->id.id == E_SQ)
    ? strlen_sq (curr->children[0]->id.lexem)
    : strlen_dq (curr->children[0]->id.lexem)), ignore (), ignore ());
    */
    push_const (res, ((curr->id.id == E_SQ)
		      ? strlen_sq (curr->children[0]->id.lexem)
		      : strlen_dq (curr->children[0]->id.lexem)));
    //    add_instruction (res, CONSTS, str_arg ("1")  , ignore ()    , ignore ()); /* Push one to the stack. */
    add_instruction (res, CONSTU, int_arg (1), ignore (), ignore ());
    add_instruction (res, NEW_N, reg_arg (R_POP), ignore ()    , ignore ()); /* Create an anonymous array. */      
    add_instruction (res, REF  , reg_arg (R_TOP), reg_arg (R_A), ignore ()); /* Set the A register to the array. */
    //    add_instruction (res, CONST, str_arg ("0")  , ignore ()    , ignore ()); /* Push the string index to the stack (used for setting the string). */
    add_instruction (res, CONSTU, int_arg (0), ignore (), ignore ());
    
    /* Set the array to the string. */
    set_child (res, ((curr->id.id == E_SQ)
		     ? compile_str_sq (curr->children[0]->id.lexem, 0)
		     : compile_str_dq (curr->children[0]->id.lexem, 0)));
    break;
    
  case E_LOCAL_CHECK:
  case E_GLOBAL_CHECK:
    res->node_type = INSTRUCTION;
    res->node_val.inst.inst_val = lookup_table[curr->id.id];
    res->node_val.inst.args[0] = str_arg (curr->children[0]->id.lexem);
    break;
    
  case E_NEG:
    res->node_type = GROUPING;
    res->node_val.grouping.children = FACT_malloc (sizeof (struct inter_node *) * 2);
    set_child (res, compile_tree (curr->children[0], 0, 0, set_rx));
    add_instruction (res, NEG, reg_arg (R_TOP), ignore (), ignore ());
    break;
    
  case E_ADD:
  case E_MUL:
  case E_NE:
  case E_EQ:
    res->node_type = GROUPING;
    res->node_val.grouping.children = FACT_malloc (sizeof (struct inter_node *) * 4);
      
    /* Create a temporary variable. */
    //    add_instruction (res, CONST, str_arg ("0"), ignore (), ignore ());
    add_instruction (res, CONSTU, int_arg (0), ignore (), ignore ());
    
    /* Compile the arguments. */
    set_child (res, compile_tree (curr->children[0], 0, 0, set_rx));
    set_child (res, compile_tree (curr->children[1], 0, 0, set_rx));
    add_instruction (res, lookup_table[curr->id.id], reg_arg (R_POP), reg_arg (R_POP), reg_arg (R_TOP));
    break;

  case E_SUB:
  case E_DIV:
  case E_MOD:
  case E_MT:
  case E_ME:
  case E_LT:
  case E_LE:
    res->node_type = GROUPING;
    res->node_val.grouping.children = FACT_malloc (sizeof (struct inter_node *) * 4);

    /* Create a temporary variable. */
    //    add_instruction (res, CONST, str_arg ("0"), ignore (), ignore ());
    add_instruction (res, CONSTU, int_arg (0), ignore (), ignore ());

    /* Compile the arguments. */
    set_child (res, compile_tree (curr->children[0], 0, 0, set_rx));
    set_child (res, compile_tree (curr->children[1], 0, 0, set_rx));
    add_instruction (res, lookup_table[curr->id.id], reg_arg (R_POP), reg_arg (R_POP), reg_arg (R_TOP));
    break;

  case E_ADD_AS:
  case E_SUB_AS:
  case E_MUL_AS:
  case E_DIV_AS:
  case E_MOD_AS:
    res->node_type = GROUPING;
    res->node_val.grouping.children = FACT_malloc (sizeof (struct inter_node *) * 3);
    set_child (res, compile_tree (curr->children[0], 0, 0, set_rx));
    set_child (res, compile_tree (curr->children[1], 0, 0, set_rx));
    add_instruction (res, lookup_table[curr->id.id], reg_arg (R_POP), reg_arg (R_TOP), reg_arg (R_TOP));
    break;

  case E_AND:
    res->node_type = GROUPING;
    res->node_val.grouping.children = FACT_malloc (sizeof (struct inter_node *) * 7);
    // add_instruction (res, CONST, str_arg ("0"), ignore (), ignore ());
    add_instruction (res, CONSTU, int_arg (0), ignore (), ignore ());
    set_child (res, compile_tree (curr->children[0], 0, 0, set_rx));
    add_instruction (res, JIF, reg_arg (R_POP), addr_arg (6), ignore ());
    set_child (res, compile_tree (curr->children[1], 0, 0, set_rx));
    add_instruction (res, JIF  , reg_arg (R_POP), addr_arg (6), ignore ());
    add_instruction (res, DROP , ignore  ()     , ignore ()   , ignore ());
    //    add_instruction (res, CONST, str_arg ("1")  , ignore ()   , ignore ());
    add_instruction (res, CONSTU, int_arg (1), ignore (), ignore ());
    break;

  case E_OR:
    res->node_type = GROUPING;
    res->node_val.grouping.children = FACT_malloc (sizeof (struct inter_node *) * 7);
    //    add_instruction (res, CONST, str_arg ("1"), ignore (), ignore ());
    add_instruction (res, CONSTU, int_arg (1), ignore (), ignore ());
    set_child (res, compile_tree (curr->children[0], 0, 0, set_rx));
    add_instruction (res, JIT, reg_arg (R_POP), addr_arg (6), ignore ());
    set_child (res, compile_tree (curr->children[1], 0, 0, set_rx));
    add_instruction (res, JIT  , reg_arg (R_POP), addr_arg (6), ignore ());
    add_instruction (res, DROP , ignore ()      , ignore ()   , ignore ());
    //    add_instruction (res, CONST, str_arg ("0")  , ignore ()   , ignore ());
    add_instruction (res, CONSTU, int_arg (0), ignore (), ignore ());
    break;
      
  case E_ARRAY_ELEM:
    res->node_type = GROUPING;
    res->node_val.grouping.children = FACT_malloc (sizeof (struct inter_node *) * 3);
    set_child (res, compile_tree (curr->children[0], 0, 0, set_rx));
    set_child (res, compile_tree (curr->children[1], 0, 0, set_rx));
    add_instruction (res, ELEM, reg_arg (R_POP), reg_arg (R_POP), ignore ());
    break;

  case E_OP_BRACK: 
    /* This isn't really working yet for scope arrays.
     *  0: [First element]
     *  1: jis,$top,@6
     *  2: const,%1
     *  3: dup
     *  4: new_n,$pop
     *  5: jmp,@9
     *  6: const,%1
     *  7: dup
     *  8: new_s,$pop
     *  9: ref,$top,$A
     * 10: swap
     * 11: const,%0
     * 12: elem,$A,$pop
     * 13: swap
     * 14: sto,$pop,$pop
     * 15: [The rest]
     */
    res->node_type = GROUPING;
    res->node_val.grouping.children = FACT_malloc (sizeof (struct inter_node *) * 16);
    set_child (res, compile_tree (curr->children[0], 0, 0, set_rx));

    /* If the first element is a number: */
    add_instruction (res, JIS  , reg_arg (R_TOP), addr_arg (5), ignore ());
    // add_instruction (res, CONST, str_arg ("1")  , ignore ()   , ignore ());
    add_instruction (res, CONSTU, int_arg (1), ignore (), ignore ());
    add_instruction (res, DUP  , ignore ()      , ignore ()   , ignore ());
    add_instruction (res, NEW_N, reg_arg (R_POP), ignore ()   , ignore ());
    add_instruction (res, JMP  , addr_arg (8)   , ignore ()   , ignore ());

    /* If the first element is a scope: */
    //    add_instruction (res, CONST, str_arg ("1")  , ignore (), ignore ());
    add_instruction (res, CONSTU, int_arg (1), ignore (), ignore ());
    add_instruction (res, DUP  , ignore ()      , ignore (), ignore ());
    add_instruction (res, NEW_S, reg_arg (R_POP), ignore (), ignore ());

    /* For all cases: */
    add_instruction (res, REF  , reg_arg (R_TOP), reg_arg (R_A)  , ignore ());
    add_instruction (res, SWAP , ignore ()      , ignore ()      , ignore ());
    // add_instruction (res, CONST, str_arg ("0")  , ignore ()      , ignore ());
    add_instruction (res, CONSTU, int_arg (0), ignore (), ignore ());
    add_instruction (res, ELEM , reg_arg (R_A)  , reg_arg (R_POP), ignore ());
    add_instruction (res, SWAP , ignore ()      , ignore ()      , ignore ());
    add_instruction (res, STO  , reg_arg (R_POP), reg_arg (R_POP), ignore ());
    set_child (res, compile_array_dec (curr->children[1]));
    break;

  case E_IN:
    res->node_type = GROUPING;
    res->node_val.grouping.children = FACT_malloc (sizeof (struct inter_node *) * 5);
    set_child (res, compile_tree (curr->children[0], 0, 0, set_rx));
    add_instruction (res, USE, reg_arg (R_POP), ignore (), ignore ());
    set_child (res, compile_tree (curr->children[1], 0, 0, set_rx));
    add_instruction (res, EXIT, ignore (), ignore (), ignore ());
    add_instruction (res, DROP, ignore (), ignore (), ignore ());
    break;

  case E_FUNC_CALL:
    res->node_type = GROUPING;
    res->node_val.grouping.children = FACT_malloc (sizeof (struct inter_node *) * 12);

    /* Compile the arguments being passed. */
    set_child (res, compile_tree (curr->children[0], 0, 0, set_rx));
    add_instruction (res, LAMBDA, ignore (), ignore (), ignore ()); /* Create a lambda scope. */
    /* Compile the function being called. */
    set_child (res, compile_tree (curr->children[1], 0, 0, set_rx));
    /* Set the up variable of the lambda scope to it. */
    add_instruction (res, REF  , reg_arg (R_POP), reg_arg (R_A)  , ignore ());
    add_instruction (res, USE  , reg_arg (R_POP), ignore ()      , ignore ()); /* Briefly enter the scope to do so. */
    //    add_instruction (res, CONST, str_arg ("0")  , ignore ()      , ignore ());
    add_instruction (res, CONSTU, int_arg (0), ignore (), ignore ());
    add_instruction (res, DEF_S, reg_arg (R_POP), str_arg ("up") , ignore ());
    add_instruction (res, STO  , reg_arg (R_A)  , reg_arg (R_POP), ignore ());
    add_instruction (res, EXIT , ignore ()      , ignore ()      , ignore ());
    add_instruction (res, SET_F, reg_arg (R_A)  , reg_arg (R_TOP), ignore ()); /* Set the lambda scope's code address and call it. */
    add_instruction (res, NAME , reg_arg (R_A)  , reg_arg (R_TOP), ignore ()); /* Change the lambda scope's name to the function being called. */
    add_instruction (res, CALL , reg_arg (R_POP), ignore ()      , ignore ());
    break;

  case E_FUNC_DEF:
    res->node_type = GROUPING;
    res->node_val.grouping.children = FACT_malloc (sizeof (struct inter_node *) * 7);
    add_instruction (res, JMP, addr_arg (4), ignore (), ignore ());
    set_child (res, compile_args (curr->children[1]));
    set_child (res, compile_tree (curr->children[2], 1, 0, set_rx));
    //    add_instruction (res, CONST, str_arg ("0"), ignore (), ignore ());
    add_instruction (res, CONSTU, int_arg (0), ignore (), ignore ());
    add_instruction (res, RET, ignore (), ignore (), ignore ());
    set_child (res, compile_tree (curr->children[0], 0, 0, set_rx));
    add_instruction (res, SET_C, reg_arg (R_TOP), addr_arg (1), ignore ());
    break;

  case E_DEFUNC: /* Yes, there is a difference between this and FUNC_DEF. */
    res->node_type = GROUPING;
    res->node_val.grouping.children = FACT_malloc (sizeof (struct inter_node *) * 8);
    add_instruction (res, JMP, addr_arg (4), ignore (), ignore ());
    /* This is a little messed up because of how quick this was implemented. */
    set_child (res, compile_args (curr->children[0]->children[1]));
    set_child (res, compile_tree (curr->children[0]->children[2], 1, 0, set_rx));
    // add_instruction (res, CONST, str_arg ("0"), ignore (), ignore ());
    add_instruction (res, CONSTU, int_arg (0), ignore (), ignore ());
    add_instruction (res, RET, ignore (), ignore (), ignore ());
    // add_instruction (res, CONST, str_arg ("0"), ignore (), ignore ());
    add_instruction (res, CONSTU, int_arg (0), ignore (), ignore ());
    add_instruction (res, DEF_S, reg_arg (R_POP), str_arg (curr->children[0]->id.lexem), ignore ());
    add_instruction (res, SET_C, reg_arg (R_TOP), addr_arg (1), ignore ());
    break;
    
  case E_RETURN:
    /* Make sure that s_count != 0, so that we know we are in a scope that can return. */
    assert (s_count != 0); /* Throw a compilation error here. */
    res->node_type = GROUPING;
    res->node_val.grouping.children = FACT_malloc (sizeof (struct inter_node *) * (5 + (s_count - 1) * 2));
    set_child (res, compile_tree (curr->children[0], 0, 0, set_rx));
    add_instruction (res, DUP, ignore (), ignore (), ignore ());
    add_instruction (res, SWAP, ignore (), ignore (), ignore ());
    add_instruction (res, DROP, ignore (), ignore (), ignore ());

    /* Exit out of all the lambda scopes we need to before returning. */
    for (i = 0; i < s_count - 1; i++) {
      add_instruction (res, EXIT, ignore (), ignore (), ignore ());
      add_instruction (res, DROP, ignore (), ignore (), ignore ());
    }
    add_instruction (res, RET, ignore (), ignore (), ignore ());
    break;

  case E_GIVE:
    /* Make sure that s_count != 0, so that we know we are in a scope that can return. */
    assert (s_count != 0); /* Throw a compilation error here. */
    res->node_type = GROUPING;
    res->node_val.grouping.children = FACT_malloc (sizeof (struct inter_node *) * (2 + (s_count - 1) * 2));
    set_child (res, compile_tree (curr->children[0], 0, 0, set_rx));
      
    /* Exit out of all the lambda scopes we need to before returning. */
    for (i = 0; i < s_count - 1; i++) {
      add_instruction (res, EXIT, ignore (), ignore (), ignore ());
      add_instruction (res, DROP, ignore (), ignore (), ignore ());
    }
    add_instruction (res, RET, ignore (), ignore (), ignore ());
    break;

  case E_BREAK:
    /* l_count != 0 for breaks. */
    assert (l_count != 0);
    res->node_type = GROUPING;
    res->node_val.grouping.children = FACT_malloc (sizeof (struct inter_node *)
						   * ((1 + s_count - l_count) * 2));

    /* Exit out of all the lambda scopes we need to before breaking. */
    for (i = 0; i < s_count - l_count; i++) {
      add_instruction (res, EXIT, ignore (), ignore (), ignore ());
      add_instruction (res, DROP, ignore (), ignore (), ignore ());
    }
    add_instruction (res, GOTO, reg_arg (R_POP), ignore (), ignore ());
    break;
      
  case E_SET:
    res->node_type = GROUPING;
    res->node_val.grouping.children = FACT_malloc (sizeof (struct inter_node *) * 3);
    set_child (res, compile_tree (curr->children[0], 0, 0, set_rx));
    set_child (res, compile_tree (curr->children[1], 0, 0, set_rx));
    add_instruction (res, STO, reg_arg (R_POP), reg_arg (R_TOP), ignore ());
    break;
      
  case E_NUM_DEF:
  case E_SCOPE_DEF:
    /* Count the number of dimensions the variable has. */
    dims = 0;
    for (n = curr->children[0]; n != NULL; n = n->next)
      dims++;

    res->node_type = GROUPING;
    if (dims == 0) { /* If there are no dimensions. */
      res->node_val.grouping.children = FACT_malloc (sizeof (struct inter_node *) * 2);
      //      add_instruction (res, CONST, str_arg ("0"), ignore (), ignore ());
      add_instruction (res, CONSTU, int_arg (0), ignore (), ignore ());
    } else {
      /* Convert dims to a string. */
      i = 0;
      dims_str = NULL;
      do {
	dims_str = FACT_realloc (dims_str, i + 2);
	dims_str[i++] = (dims % 10) + '0';
      } while ((dims /= 10) > 0);
      
      dims_str[i--] = '\0';
      
      /* Reverse the string. Use XOR swap because I'd don't want to declare
       * another variable in the case block.
       */
      for (j = 0; j < i; j++, i--) {
	dims_str[j] ^= dims_str[i];
	dims_str[i] ^= dims_str[j];
	dims_str[j] ^= dims_str[i];
      }
      
      res->node_val.grouping.children = FACT_malloc (sizeof (struct inter_node *) * 3);
      set_child (res, compile_tree (curr->children[0], 0, 0, set_rx));
      //      add_instruction (res, CONST, str_arg (dims_str), ignore (), ignore ());
      push_const (res, dims_str);
    }
    add_instruction (res, curr->id.id == E_NUM_DEF ? DEF_N : DEF_S,
		     reg_arg (R_POP), str_arg (curr->children[1]->id.lexem), ignore ());
    break;

  case E_CONST:
    res->node_type = GROUPING;

    if (curr->children[1] != NULL && curr->children[1]->id.id == E_SET) {
      res->node_val.grouping.children = FACT_malloc (sizeof (struct inter_node *) * 6);
      //      add_instruction (res, CONST, str_arg ("0"), ignore (), ignore ());
      add_instruction (res, CONSTU, int_arg (0), ignore (), ignore ());
      add_instruction (res, NEW_N, reg_arg (R_POP), ignore (), ignore ());
      set_child (res, compile_tree (curr->children[2], 0, 0, set_rx));
      add_instruction (res, STO, reg_arg (R_POP), reg_arg (R_TOP), ignore ());
    } else {
      res->node_val.grouping.children = FACT_malloc (sizeof (struct inter_node *) * 10);
      add_instruction (res, JMP, addr_arg (4), ignore (), ignore ());
      set_child (res, compile_args (curr->children[1]));
      set_child (res, compile_tree (curr->children[2], 1, 0, set_rx));
      //      add_instruction (res, CONST, str_arg ("0"), ignore (), ignore ());
      add_instruction (res, CONSTU, int_arg (0), ignore (), ignore ());
      add_instruction (res, RET, ignore (), ignore (), ignore ());
      //      add_instruction (res, CONST, str_arg ("0"), ignore (), ignore ());
      add_instruction (res, CONSTU, int_arg (0), ignore (), ignore ());
      add_instruction (res, NEW_S, reg_arg (R_POP), ignore (), ignore ());
      add_instruction (res, SET_C, reg_arg (R_TOP), addr_arg (1), ignore ());
    }
    add_instruction (res, LOCK, reg_arg (R_TOP), ignore (), ignore ());
    add_instruction (res, GLOBAL, reg_arg (R_TOP), str_arg (curr->children[0]->id.lexem), ignore ());      
    break;

  case E_IF:
    res->node_type = GROUPING;
    if (curr->children[2] != NULL) {
      res->node_val.grouping.children = FACT_malloc (sizeof (struct inter_node *) * 5);
      set_child (res, compile_tree (curr->children[0], 0, 0, set_rx));
      add_instruction (res, JIF, reg_arg (R_POP), addr_arg (3), ignore ());
      set_child (res, compile_tree (curr->children[1], s_count, l_count, set_rx));
      add_instruction (res, JMP, addr_arg (4), ignore (), ignore ());
      set_child (res, compile_tree (curr->children[2], s_count, l_count, set_rx));
    } else {
      res->node_val.grouping.children = FACT_malloc (sizeof (struct inter_node *) * 3);
      set_child (res, compile_tree (curr->children[0], 0, 0, set_rx));
      add_instruction (res, JIF, reg_arg (R_POP), addr_arg (2), ignore ());
      set_child (res, compile_tree (curr->children[1], s_count, l_count, set_rx));
    }
    break;

  case E_WHILE:
    res->node_type = GROUPING;
    res->node_val.grouping.children = FACT_malloc (sizeof (struct inter_node *) * 10);

    /* Set the break point. */
    add_instruction (res, JMP_PNT, addr_arg (6), ignore (), ignore ());
    set_child (res, begin_temp_scope ());
    set_child (res, compile_tree (curr->children[0], 0, 0, set_rx));

    if (curr->children[0] == NULL)
      set_child (res, NULL);
    else
      add_instruction (res, JIF, reg_arg (R_POP), addr_arg (6), ignore ());
      
    /* Do not create a new scope for brackets. */
    if (curr->children[1] == NULL) {
      set_child (res, NULL);
      set_child (res, NULL);
    } else if (curr->children[1]->id.id == E_OP_CURL) {
      set_child (res, compile_tree (curr->children[1]->children[0],
				    s_count + 1, s_count + 1, set_rx));
      set_child (res, NULL);
      
    } else {
      set_child (res, compile_tree (curr->children[1],
				    s_count + 1, s_count + 1, set_rx));
      /* Drop the return value of every statement. */
      add_instruction (res, DROP, ignore (), ignore (), ignore ());
    }

    add_instruction (res, JMP, addr_arg (2), ignore (), ignore ());
    add_instruction (res, DROP, ignore (), ignore (), ignore ());
    set_child (res, end_temp_scope ());
    set_child (res, set_return_val ());
    break;
      
  case E_FOR:
    res->node_type = GROUPING;
    res->node_val.grouping.children = FACT_malloc (sizeof (struct inter_node *) * 12);

    /* Set the break point. */
    add_instruction (res, JMP_PNT, addr_arg (8), ignore (), ignore ());
    set_child (res, begin_temp_scope ());
    set_child (res, compile_tree (curr->children[0], 0, 0, set_rx));
    set_child (res, compile_tree (curr->children[1], 0, 0, set_rx));

    if (curr->children[1] == NULL)
      set_child (res, NULL);
    else
      add_instruction (res, JIF, reg_arg (R_POP), addr_arg (8), ignore ());

    if (curr->children[3] == NULL)
      set_child (res, NULL);
    /* Do not create a new scope for brackets. */
    else if (curr->children[3]->id.id == E_OP_CURL)
      set_child (res, compile_tree (curr->children[3]->children[0], s_count + 1, s_count + 1, set_rx));
				      
    else
      set_child (res, compile_tree (curr->children[3], s_count + 1, s_count + 1, set_rx));

    set_child (res, compile_tree (curr->children[2], 0, 0, set_rx));
      
    /* Drop the return value of every statement. */
    add_instruction (res, DROP, ignore (), ignore (), ignore ());
    add_instruction (res, JMP, addr_arg (3), ignore (), ignore ());
    add_instruction (res, DROP, ignore (), ignore (), ignore ());
    set_child (res, end_temp_scope ());
    set_child (res, set_return_val ());
    break;

  case E_CATCH:
    res->node_type = GROUPING;
    res->node_val.grouping.children = FACT_malloc (sizeof (struct inter_node *) * 6);
    add_instruction (res, TRAP_B, addr_arg (3), ignore (), ignore ());
    set_child (res, compile_tree (curr->children[0], s_count, l_count, set_rx));
    add_instruction (res, TRAP_E, ignore (), ignore (), ignore ());
    add_instruction (res, JMP, addr_arg (5), ignore (), ignore ());
    add_instruction (res, TRAP_E, ignore (), ignore (), ignore ());
    set_child (res, compile_tree (curr->children[1], s_count, l_count, set_rx));
    break;

  case E_THREAD:
    res->node_type = GROUPING;
    res->node_val.grouping.children = FACT_malloc (sizeof (struct inter_node *) * 3);
    add_instruction (res, SPRT, addr_arg (2), ignore (), ignore ());
    set_child (res, compile_tree (curr->children[0], 0, 0, false));
    add_instruction (res, HALT, ignore (), ignore (), ignore ());
    break;

  case E_OP_CURL: /* This REALLY needs to be optimized. */
    res->node_type = GROUPING;
    res->node_val.grouping.children = FACT_malloc (sizeof (struct inter_node *) * 3);
    set_child (res, begin_temp_scope ());
    set_child (res, compile_tree (curr->children[0], 1 + s_count, l_count, set_rx));
    set_child (res, end_temp_scope ());
    break;

  case E_END:
  case E_SEMI:
  case E_CL_CURL:
    /* Per every expression, pop the top element off the stack and set the
     * X register to it (only set to X if set_rx is true). That way the stack
     * will never overflow and the return value can still be checked. 
     */
    res->node_type = INSTRUCTION;
    if (set_rx) {
      res->node_val.inst.inst_val = REF;
      res->node_val.inst.args[0].arg_type = REG_VAL;
      res->node_val.inst.args[0].arg_val.reg = R_POP;
      res->node_val.inst.args[1].arg_type = REG_VAL;
      res->node_val.inst.args[1].arg_val.reg = R_X;
    } else
      res->node_val.inst.inst_val = DROP;
    break;

  default:
    abort ();
  }

  /* Compile the next statement. */
  res->next = compile_tree (curr->next, s_count, l_count, set_rx);
  return res;
}

static char *strlen_dq (char *str) /* Get the length of a string and return it as a string of digits in base 10. */
{
  size_t i, j;
  size_t res_len;
  char hold;
  char *res;

  res = FACT_malloc (2);
  res_len = 2;
  res[0] = '0';
  res[1] = '\0';

  for (i = 0; str[i] != '\0'; i++) {
    /* Adjust for escape sequences. */
    if (str[i] == '\\'
	&& (str[i + 1] == '"'
	    || str[i + 1] == 'r'
	    || str[i + 1] == 'n'
	    || str[i + 1] == 't'
	    || str[i + 1] == '\\'))
      i++;
    
    /* Carry the one over. */ 
    for (j = 0; j < res_len - 1; j++) {
      if (res[j] != '9') {
	res[j]++;
	break;
      } else
	res[j] = '0'; /* Continue to carry over. */
    }
    if (j == res_len - 1) {
      /* A new digit is needed. */
      res = FACT_realloc (res, ++res_len);
      res[j] = '1';
      res[j + 1] = '\0';
    }
  }
  
  /* Reverse the string. */
  for (i = 0, j = res_len - 2; i < j; i++, j--) {
    hold = res[j];
    res[j] = res[i];
    res[i] = hold;
  }
  
  return res;
}

static char *strlen_sq (char *str) /* Get the length of a string and return it as a string of digits in base 10.
				    * I know, that's pretty stupid.
				    */
{
  size_t i, j;
  size_t res_len;
  char hold;
  char *res;

  res = FACT_malloc (2);
  res_len = 2;
  res[0] = '0';
  res[1] = '\0';

  for (i = 0; str[i] != '\0'; i++) {
    /* Adjust for escape sequences. */
    if (str[i] == '\\'
	&& (str[i + 1] == '\'' || str[i + 1] == '\\'))
      i++;
    
    /* Carry the one over. */ 
    for (j = 0; j < res_len - 1; j++) {
      if (res[j] != '9') {
	res[j]++;
	break;
      } else
	res[j] = '0'; /* Continue to carry over. */
    }
    if (j == res_len - 1) {
      /* A new digit is needed. */
      res = FACT_realloc (res, ++res_len);
      res[j] = '1';
      res[j + 1] = '\0';
    }
  }
  
  /* Reverse the string. */
  for (i = 0, j = res_len - 2; i < j; i++, j--) {
    hold = res[j];
    res[j] = res[i];
    res[i] = hold;
  }
  
  return res;
}

static struct inter_node *compile_str_dq (char *str, size_t i) /* Compile a double-quotation string. */
{
  char new;
  char *c_val;
  struct inter_node *res;

  if (str[i] == '\0') {
    /* End of the string. Drop the index. */
    res = create_node ();
    res->node_type = INSTRUCTION;
    res->node_val.inst.inst_val = DROP;
    return res;
  }
  
  if (str[i] == '\\') { /* Fix escape sequences. */
    ++i;
    switch (str[i]) {
    case '\0':
    default:
      --i;
      /* Fall through. */
    case '\\':
      new = '\\';
      break;
      
    case '"':
      new = '"';
      break;
      
    case 'n': /* Newline. */
      new = '\n';
      break;
      
    case 'r': /* Carraige return. */
      new = '\r';
      break;
      
    case 't': /* Tab. */
      new = '\t';
      break;
    }
  } else
    new = str[i];

  /* Convert the character value to a string. */
  c_val = FACT_malloc (4);
  c_val[0] = (new / 100 % 10) + '0';
  c_val[1] = (new / 10 % 10) + '0';
  c_val[2] = (new % 10) + '0';
  c_val[3] = '\0';
  
  res = create_node ();
  res->node_type = GROUPING;
  res->node_val.grouping.children = FACT_malloc (sizeof (struct inter_node *) * 6);
  add_instruction (res, DUP, ignore (), ignore (), ignore ());
  add_instruction (res, ELEM, reg_arg (R_A), reg_arg (R_POP), ignore ());
  //  add_instruction (res, CONST, str_arg (c_val), ignore (), ignore ());
  push_const (res, c_val);
  add_instruction (res, STO, reg_arg (R_POP), reg_arg (R_POP), ignore ());
  add_instruction (res, INC, reg_arg (R_TOP), ignore (), ignore ());
  /* Increase the index one and compile the next character. */
  set_child (res, compile_str_dq (str, i + 1));

  return res;
}

static struct inter_node *compile_str_sq (char *str, size_t i) /* Compile a single-quotation string. */
{
  char new;
  char *c_val;
  struct inter_node *res;

  if (str[i] == '\0') {
    /* End of the string. Drop the index. */
    res = create_node ();
    res->node_type = INSTRUCTION;
    res->node_val.inst.inst_val = DROP;
    return res;
  }
  
  if (str[i] == '\\') { /* Fix escape sequences. */
    ++i;
    switch (str[i]) {
    case '\0':
    default:
      --i;
      /* Fall through. */
    case '\\':
      new = '\\';
      break;
      
    case '\'':
      new = '\'';
      break;
    }
  } else
    new = str[i];

  /* Convert the character value to a string. This is kind of dumb. */
  c_val = FACT_malloc (4);
  c_val[0] = (new / 100 % 10) + '0';
  c_val[1] = (new / 10 % 10) + '0';
  c_val[2] = (new % 10) + '0';
  c_val[3] = '\0';
  
  res = create_node ();
  res->node_type = GROUPING;
  res->node_val.grouping.children = FACT_malloc (sizeof (struct inter_node *) * 6);
  add_instruction (res, DUP, ignore (), ignore (), ignore ());
  add_instruction (res, ELEM, reg_arg (R_A), reg_arg (R_POP), ignore ());
  //  add_instruction (res, CONST, str_arg (c_val), ignore (), ignore ());
  push_const (res, c_val);
  add_instruction (res, STO, reg_arg (R_POP), reg_arg (R_POP), ignore ());
  add_instruction (res, INC, reg_arg (R_TOP), ignore (), ignore ());
  /* Increase the index one and compile the next character. */
  set_child (res, compile_str_sq (str, i + 1));

  return res;
}

static struct inter_node *compile_array_dec (FACT_tree_t curr)
{
  struct inter_node *res;
  
  if (curr == NULL)
    return NULL;

  res = create_node ();
  res->node_type = GROUPING;
  res->node_val.grouping.children = FACT_malloc (sizeof (struct inter_node *) * 3);
  set_child (res, compile_tree (curr->children[0], 0, 0, false));
  add_instruction (res, APPEND, reg_arg (R_POP), reg_arg (R_TOP), ignore ());
  set_child (res, compile_array_dec (curr->children[1]));

  return res;
}
  
static struct inter_node *begin_temp_scope (void) /* Create a temporary scope with the up variable set. */
{
  struct inter_node *res;

  res = create_node ();
  res->node_type = GROUPING;
  res->node_val.grouping.children = FACT_malloc (sizeof (struct inter_node *) * 7);

  /* Set the A register to the current scope for later use. */
  add_instruction (res, THIS, ignore (), ignore (), ignore ());
  add_instruction (res, REF, reg_arg (R_POP), reg_arg (R_A), ignore ());
  add_instruction (res, LAMBDA, ignore (), ignore (), ignore ()); /* Create an anonymous scope. */
  add_instruction (res, USE, reg_arg (R_POP), ignore (), ignore ()); /* Push the scope to the call stack. */
  //  add_instruction (res, CONST, str_arg ("0"), ignore (), ignore ()); /* Create an "up" variable for the scope. */
  add_instruction (res, CONSTU, int_arg (0), ignore (), ignore ());
  add_instruction (res, DEF_S, reg_arg (R_POP), str_arg ("up"), ignore ());
  add_instruction (res, STO, reg_arg (R_A), reg_arg (R_POP), ignore ());

  return res;
}

static struct inter_node *end_temp_scope (void)
{
  struct inter_node *res;
  
  res = create_node ();
  res->node_type = GROUPING;
  res->node_val.grouping.children = FACT_malloc (sizeof (struct inter_node *));
  res->node_val.grouping.num_children = 1;

  /* Exit the scope. */
  add_instruction (res, EXIT, ignore (), ignore (), ignore ());

  return res;
}

static struct inter_node *set_return_val ()
{
  struct inter_node *res;

  res = create_node ();
  
  res->node_type = INSTRUCTION;
  res->node_val.inst.inst_val = REF;
  res->node_val.inst.args[0].arg_type = REG_VAL;
  res->node_val.inst.args[0].arg_val.reg = R_POP;
  res->node_val.inst.args[1].arg_type = REG_VAL;
  res->node_val.inst.args[1].arg_val.reg = R_X;

  return res;
}

#if 0
static sturct inter_node *check_args (size_t nargs)
{
}
#endif

static struct inter_node *compile_args (FACT_tree_t curr)
{
  struct inter_node *res;

  if (curr == NULL)
    return NULL;

  res = create_node ();
  res->node_type = GROUPING;
  res->node_val.grouping.children = FACT_malloc (sizeof (struct inter_node *) * 8);

  if (curr->id.id == E_NUM_DEF
      || curr->id.id == E_SCOPE_DEF) { /* Statically typed argument. */
    //    add_instruction (res, CONST, str_arg ("0"), ignore (), ignore ());
    add_instruction (res, CONSTU, int_arg (0), ignore (), ignore ());
    add_instruction (res, curr->id.id == E_NUM_DEF ? DEF_N : DEF_S,
		     reg_arg (R_POP), str_arg (curr->children[0]->id.lexem), ignore ());
  } else { /* Dynamically typed argument. */
    /* jis,%top,
     * const,$0
     * def_n,%pop,var_name
     * jmp,@2
     * const,$0
     * def_s,%pop,var_name
     */ 
    add_instruction (res, JIS, reg_arg (R_TOP), addr_arg (3), ignore ());
    //    add_instruction (res, CONST, str_arg ("0"), ignore (), ignore ());
    add_instruction (res, CONSTU, int_arg (0), ignore (), ignore ());
    add_instruction (res, DEF_N, reg_arg (R_POP), str_arg (curr->children[0]->id.lexem), ignore ());
    add_instruction (res, JMP, addr_arg (5), ignore (), ignore ());
    add_instruction (res, CONSTU, int_arg (0), ignore (), ignore ());
    //    add_instruction (res, CONST, str_arg ("0"), ignore (), ignore ());
    add_instruction (res, DEF_S, reg_arg (R_POP), str_arg (curr->children[0]->id.lexem), ignore ());
  }
  
  add_instruction (res, SWAP, ignore (), ignore (), ignore ());
  add_instruction (res, STO, reg_arg (R_POP), reg_arg (R_POP), ignore ());
  res->next = compile_args (curr->children[1]);

  return res;
}

static void load (struct inter_node *curr, struct inter_node *up, const char *file_name)
{
  char *fmt;
  size_t i, j, k;
  size_t len, addr;

  if (curr == NULL)
    return;

  /* Add the line number, if it has one. */
  if (curr->line > 0)
    FACT_add_line (file_name, curr->line, Furlow_offset ());
  
  fmt = NULL;
  if (curr->node_type == INSTRUCTION) {
    len = 1;
    fmt = FACT_malloc (1);
    fmt[0] = curr->node_val.inst.inst_val;
    for (i = 0; i < 4; i++) {
      switch (curr->node_val.inst.args[i].arg_type) {
      case REG_VAL:
	fmt = FACT_realloc (fmt, ++len);
	fmt[len - 1] = curr->node_val.inst.args[i].arg_val.reg;
	break;

      case INT_VAL:
	fmt = FACT_realloc (fmt, len + 4);
	spread (fmt + len, curr->node_val.inst.args[i].arg_val.addr);
	len += 4;
	break;
	
      case ADDR_VAL:
	if (up != NULL) {
	  for (j = 0; j < up->node_val.grouping.num_children; j++) {
	    if (up->node_val.grouping.children[j] == curr)
	      break;
	  }
	  assert (up->node_val.grouping.children[j] == curr
		  && curr->node_val.inst.args[i].arg_val.addr != j);
	  addr = Furlow_offset ();
	  if (j < curr->node_val.inst.args[i].arg_val.addr) {
	    for (k = j; k <= curr->node_val.inst.args[i].arg_val.addr; k++)
	      addr += weight (up->node_val.grouping.children[k]);
	  } else {
	    for (k = curr->node_val.inst.args[i].arg_val.addr; k < j; k++)
	      addr -= weight (up->node_val.grouping.children[k]);
	  }
	} else
	  abort ();
	fmt = FACT_realloc (fmt, len + 4);
	spread (fmt + len, addr);
	len += 4;
	break;

      case STR_VAL:
	fmt = FACT_realloc (fmt, len + strlen (curr->node_val.inst.args[i].arg_val.str) + 1);
	strcpy (fmt + len, curr->node_val.inst.args[i].arg_val.str);
	/* Fall through. */
	
      default:
	goto load_fmt;
      }
    }
    
    /* Load the instruction. */
  load_fmt:
    Furlow_add_instruction (fmt);
  } else {
    for (i = 0; i < curr->node_val.grouping.num_children; i++)
      load (curr->node_val.grouping.children[i], curr, file_name);
  }
  
  load (curr->next, NULL, file_name);
}

static size_t weight (struct inter_node *curr) /* Recursively calculate the weight of a node. */
{
  size_t i, res;

  if (curr == NULL)
    return 0;

  if (curr->node_type == INSTRUCTION)
    res = 1;
  else {
    res = 0;
    for (i = 0; i < curr->node_val.grouping.num_children; i++)
      res += weight (curr->node_val.grouping.children[i]);
  }

  return res + weight (curr->next);
}

static inline void spread (char *rop, size_t op)
{
  rop[0] = (op >> 24) & 0xFF;
  rop[1] = (op >> 16) & 0xFF;
  rop[2] = (op >> 8) & 0xFF;
  rop[3] = op & 0xFF;
}

static struct inter_node *create_node ()
{
  struct inter_node *res;

  res = FACT_malloc (sizeof (struct inter_node));
  memset (res, 0, sizeof (struct inter_node));

  return res;
}

static inline void push_const (struct inter_node *r, char *str)
{
  mpz_t temp;
  
  if (strchr (str, '.') != NULL)
    add_instruction (r, CONSTS, str_arg (str), ignore (), ignore ());
  else {
    mpz_init (temp);
    mpz_set_str (temp, str, 10);
    if (mpz_cmp_ui (temp, UINT32_MAX) <= 0)
      add_instruction (r, CONSTU, int_arg (mpz_get_ui (temp)), ignore (), ignore ());
    else
      add_instruction (r, CONSTS, str_arg (str), ignore (), ignore ());
    mpz_clear (temp);
  }
}
