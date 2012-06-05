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
#include "FACT_types.h"
#include "FACT_hash.h"
#include "FACT_alloc.h"
#include "FACT_var.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <gmp.h>

static void *Furlow_thread_mask (void *);
static inline size_t get_seg_addr (char *);

/* Threading and stacks:                                        */
size_t num_threads;                 /* Number of threads.       */
FACT_thread_t threads;              /* Thread data.             */
__thread FACT_thread_t curr_thread; /* Specific data to thread. */

/* Locks: */
pthread_mutex_t progm_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t threads_lock = PTHREAD_MUTEX_INITIALIZER;

/* Error recovery:                                               */
__thread jmp_buf handle_err; /* Jump to the error handler.       */
__thread jmp_buf recover;    /* When there are no other options. */

/* The machine:                                         */
static char **progm;     /* Program being run.          */
static char **next_inst; /* Next available instruction. */

static void print_var_stack ();

/* Global variables: */
FACT_table_t Furlow_globals = {
  .buckets = NULL,
  .num_buckets = 0,
  .total_num_vars = 0,
};

void Furlow_add_instruction (char *new) /* Add an instruction to the progm. */
{
  size_t nsize;
  static char halt_nowrite[1] = { HALT }; 

  if (progm == NULL) { /* No program yet. */
    next_inst = progm = FACT_malloc (sizeof (char *) * 2);
    *progm = new;
    *++next_inst = halt_nowrite; 
  } else {
    *next_inst = new;
    nsize = next_inst - progm + 1; 
    progm = FACT_realloc (progm, sizeof (char *) * (nsize + 1)); 
    next_inst = progm + nsize;
    *next_inst = halt_nowrite;
  }
}

inline void Furlow_lock_program (void) /* Lock progm. */
{
  pthread_mutex_lock (&progm_lock);
}

inline void Furlow_lock_threads (void) /* Lock threads, not the individual threads. */
{
  pthread_mutex_lock (&threads_lock);
}

inline void Furlow_unlock_program (void) /* Unlock progm. */
{
  pthread_mutex_unlock (&progm_lock);
}

inline void Furlow_unlock_threads (void) /* Unlock threads. */
{
  pthread_mutex_unlock (&threads_lock);
}

inline size_t Furlow_offset (void) /* Get the instruction offset. */
{
  /* Return the number of instructions there are, minus the terminating HALT. */
  return (((next_inst - progm) == 0)
	  ? 0
	  : (next_inst - progm));
}
  
FACT_t pop_v () /* Pop the variable stack. */
{
  FACT_t res;
  size_t diff;

#ifdef SAFE
  if (curr_thread->vstackp < curr_thread->vstack)
    FACT_throw_error (CURR_THIS, "illegal POP on empty var stack");
#endif /* SAFE */

  diff = curr_thread->vstackp - curr_thread->vstack;
  if (diff > 0 &&
      curr_thread->vstack_size % diff == 0 &&
      curr_thread->vstack_size / diff == 4) {
    curr_thread->vstack_size >>= 1; /* Divide by two. */
    curr_thread->vstack = FACT_realloc (curr_thread->vstack, sizeof (FACT_t) * curr_thread->vstack_size);
    curr_thread->vstackp = curr_thread->vstack + diff;
  }

  res = *curr_thread->vstackp;
  curr_thread->vstackp->ap = NULL;
  curr_thread->vstackp--;
  
  return res;
}

struct cstack_t pop_c () /* Pop the current call stack. */
{
  size_t diff;
  struct cstack_t res;

#ifdef SAFE
  if (curr_thread->cstackp < curr_thread->cstack)
    FACT_throw_error (CURR_THIS, "illegal POP on empty call stack");
#endif /* SAFE */

  diff = curr_thread->cstackp - curr_thread->cstack;
  if (diff > 0 &&
      curr_thread->cstack_size % diff == 0 &&
      curr_thread->cstack_size / diff == 4) {
    curr_thread->cstack_size >>= 1;
    curr_thread->cstack = FACT_realloc (curr_thread->cstack, sizeof (struct cstack_t) * curr_thread->cstack_size);
    curr_thread->cstackp = curr_thread->cstack + diff;
  }

  res = *curr_thread->cstackp;
  curr_thread->cstackp->this = NULL;
  curr_thread->cstackp--;

  return res;
}

size_t *pop_t () /* Pop the trap stack. */
{
  size_t *res;
  
#ifdef SAFE
  if (curr_thread->traps == NULL)
    FACT_throw_error (CURR_THIS, "no traps currently set.");
#endif /* SAFE */

  res = FACT_malloc (sizeof (size_t) * 2);
  curr_thread->num_traps--;
  memcpy (res, curr_thread->traps[curr_thread->num_traps], sizeof (size_t [2]));
  
  if (curr_thread->num_traps)
    curr_thread->traps = FACT_realloc (curr_thread->traps, sizeof (size_t [2]) * curr_thread->num_traps);
  else {
    FACT_free (curr_thread->traps);
    curr_thread->traps = NULL;
  }

  return res;
}

void push_v (FACT_t n) /* Push to the variable stack. */
{
  size_t diff;

  curr_thread->vstackp++;  
  diff = curr_thread->vstackp - curr_thread->vstack;
  if (diff >= curr_thread->vstack_size) {
    if (curr_thread->vstack_size == 0) {
      curr_thread->vstack_size = 1;
      diff = 0;
    } else
      curr_thread->vstack_size <<= 1; /* Square the size of the var stack. */
    curr_thread->vstack = FACT_realloc (curr_thread->vstack, sizeof (FACT_t) * (curr_thread->vstack_size));
    curr_thread->vstackp = curr_thread->vstack + diff;
  }
  
  *curr_thread->vstackp = n;
}

void push_c (size_t nip, FACT_scope_t nthis) /* Push to the call stack. */
{
  size_t diff;
  
  curr_thread->cstackp++;
  diff = curr_thread->cstackp - curr_thread->cstack;
  if (diff >= curr_thread->cstack_size) {
    /* Square the size of the call stack. The call stack should never be NULL. */
    //      printf ("ALLOCATION: %zu >= %zu\n", diff, curr_thread->cstack_size);
    curr_thread->cstack_size <<= 1;
    curr_thread->cstack = FACT_realloc (curr_thread->cstack, sizeof (struct cstack_t) * (curr_thread->cstack_size));
    curr_thread->cstackp = curr_thread->cstack + diff;
  }
  
  curr_thread->cstackp->ip = nip;
  curr_thread->cstackp->this = nthis;
}

void push_t (size_t n1, size_t n2) /* Push to the trap stack. */
{
  curr_thread->num_traps++;
  curr_thread->traps = FACT_realloc (curr_thread->traps, sizeof (size_t [2]) * (curr_thread->num_traps));
  curr_thread->traps[curr_thread->num_traps - 1][0] = n1;
  curr_thread->traps[curr_thread->num_traps - 1][1] = n2;
}

FACT_t *Furlow_register (int reg_number) /* Access a Furlow machine register. */
{
  static const void *jmp_table[] = {
    &&h_rPOP,
    &&h_rTOP,
    &&h_rTID,
    &&h_rGEN
  };

  goto *jmp_table[Min (reg_number, R_I)]; /* R_I is the first general register. */

 h_rPOP:
  curr_thread->registers[R_POP] = pop_v ();
  return &curr_thread->registers[R_POP];

 h_rTOP:
  if (curr_thread->vstackp < curr_thread->vstack)
    FACT_throw_error (CURR_THIS, "illegal TOP on empty stack");
  return curr_thread->vstackp;

 h_rTID:
  if (curr_thread->registers[R_TID].type != NUM_TYPE) {
    curr_thread->registers[R_TID].type = NUM_TYPE;
    curr_thread->registers[R_TID].ap = FACT_alloc_num ();
  }
  mpc_set_ui (((FACT_num_t) curr_thread->registers[R_TID].ap)->value,
	      curr_thread - threads);
  /* Fall through. */

 h_rGEN:
  return &curr_thread->registers[reg_number];
  
#if 0
  switch (reg_number) {
  case R_POP:
    curr_thread->registers[R_POP] = pop_v ();
    break;
    
  case R_TOP:
    if (curr_thread->vstackp < curr_thread->vstack)
      FACT_throw_error (CURR_THIS, "illegal TOP on empty stack");
    return curr_thread->vstackp;
    
  case R_TID:
    if (curr_thread->registers[R_TID].type != NUM_TYPE) {
      curr_thread->registers[R_TID].type = NUM_TYPE;
      curr_thread->registers[R_TID].ap = FACT_alloc_num ();
    }
    mpc_set_ui (((FACT_num_t) curr_thread->registers[R_TID].ap)->value,
		curr_thread - threads);
    break;
    
  default:
    break;
  }

  /* Probably best to add some extra checks here. */
  return &curr_thread->registers[reg_number];
#endif
}

void *Furlow_reg_val (int reg_number, FACT_type des) /* Safely access a register's value. */
{
  FACT_t *reg;

  reg = Furlow_register (reg_number);
  
  if (reg->type == UNSET_TYPE)
    FACT_throw_error (CURR_THIS, "unset register, $r%d", reg_number);
  /* If des is UNSET_TYPE, just return the register. */
  if (des != UNSET_TYPE && reg->type != des)
    FACT_throw_error (CURR_THIS, "register $r%d is type %s",
		      reg_number, des == NUM_TYPE ? "scope, expected number" : "number, expected scope");

  return reg->ap;
}

void Furlow_run () /* Run the program until a HALT is reached. */ 
{
  int i;
  char *hold_name;
  size_t tnum;
  FACT_thread_t next;
  struct cstack_t cs_arg;
  register FACT_t args[4];      /* Maximum of three arguments plus the result per opcode. */
  register FACT_t *reg_args[4]; /* For register operations.                               */
  static const void *inst_jump_table[] = { /* Jump table to each instruction. */    
#define ENTRY(n) [n] = &&INST_##n  
    ENTRY (ADD),
    ENTRY (AND),
    ENTRY (APPEND),
    ENTRY (CALL),
    ENTRY (CEQ),
    ENTRY (CLE),
    ENTRY (CLT),
    ENTRY (CME),
    ENTRY (CMT),
    ENTRY (CNE),
    ENTRY (CONSTS),
    ENTRY (CONSTI),
    ENTRY (CONSTU),
    ENTRY (DEC),
    ENTRY (DEF_N),
    ENTRY (DEF_S),
    ENTRY (DIV),
    ENTRY (DROP),
    ENTRY (DUP),
    ENTRY (ELEM),
    ENTRY (EXIT),
    ENTRY (GLOBAL),
    ENTRY (GOTO),
    /* ENTRY (GROUP), */
    ENTRY (HALT),
    ENTRY (INC),
    ENTRY (IOR),
    ENTRY (IS_AUTO),
    ENTRY (IS_DEF),
    ENTRY (JMP),
    ENTRY (JMP_PNT),
    ENTRY (JIF),
    ENTRY (JIN),
    ENTRY (JIS),
    ENTRY (JIT),
    ENTRY (LAMBDA),
    ENTRY (LOCK),
    ENTRY (MOD),
    ENTRY (MUL),
    ENTRY (NAME),
    ENTRY (NEG),
    ENTRY (NEW_N),
    ENTRY (NEW_S),
    ENTRY (NOP),
    ENTRY (PURGE),
    ENTRY (REF),
    ENTRY (RET),
    ENTRY (SET_C),
    ENTRY (SET_F),
    ENTRY (SPRT),
    ENTRY (STO),
    ENTRY (SUB),
    ENTRY (SWAP),
    ENTRY (THIS),
    ENTRY (TRAP_B),
    ENTRY (TRAP_E),
    ENTRY (USE),
    ENTRY (VAR),
    ENTRY (VA_ADD),
    ENTRY (XOR)
  };
  
/* Define an instruction's code segment. */
#define SEG(x) INST_##x: do { 0; } while (0)
#define END_SEG() do { goto *inst_jump_table[progm[++CURR_IP][0]]; } while (0)
#define NEXT_INST() END_SEG()

  curr_thread->run_flag = T_LIVE; /* The thread is now live. */
  
 eval:
  /* Set the error handler. */
  if (setjmp (handle_err)) {
    /* An error has been caught. If there are no available traps, jump to
     * recover. Otherwise, set the ip to the current trap handler.
     */
    if (curr_thread->num_traps == 0)
      longjmp (recover, 1);
    
    /* Destroy the unecessary stacks and set the ip. */
    while ((curr_thread->cstackp - curr_thread->cstack + 1)
	   > curr_thread->traps[curr_thread->num_traps - 1][1])
      pop_c ();
    
    CURR_IP = curr_thread->traps[curr_thread->num_traps - 1][0];
    curr_thread->run_flag = T_LIVE;
    goto eval; /* Go back to eval to reset the error handler. */
  }

  /* Jump to the first instruction. */
  goto *inst_jump_table [progm[CURR_IP][0]];
    
  /* Instructions are divided into "segments." Each segment contains the
   * code executed by one instruction. The macro SEG (n) creates a goto
   * label for the instruction n, and the macro END_SEG jumps to the
   * next instruction. Slightly confusing, but decently fast.
   */
  SEG (ADD);
  {
    args[0].ap = Furlow_reg_val (progm[CURR_IP][1], NUM_TYPE);
    args[1].ap = Furlow_reg_val (progm[CURR_IP][2], NUM_TYPE);
    args[2].ap = Furlow_reg_val (progm[CURR_IP][3], NUM_TYPE);
    if (FACT_cast_to_num (args[2])->locked)
      FACT_throw_error (CURR_THIS, "cannot set immutable variable");
    mpc_add (((FACT_num_t) args[2].ap)->value,
	     ((FACT_num_t) args[1].ap)->value,
	     ((FACT_num_t) args[0].ap)->value);
  }
  END_SEG ();
    
  SEG (AND);
  {
    args[0].ap = Furlow_reg_val (progm[CURR_IP][1], NUM_TYPE);
    args[1].ap = Furlow_reg_val (progm[CURR_IP][2], NUM_TYPE);
    args[2].ap = Furlow_reg_val (progm[CURR_IP][3], NUM_TYPE);
    if (FACT_cast_to_num (args[2])->locked)
      FACT_throw_error (CURR_THIS, "cannot set immutable variable");
    if (mpc_is_float (((FACT_num_t) args[0].ap)->value) ||
	mpc_is_float (((FACT_num_t) args[1].ap)->value))
      FACT_throw_error (CURR_THIS, "arguments to bitwise operators cannot be floating point");
    mpc_and (((FACT_num_t) args[2].ap)->value,
	     ((FACT_num_t) args[1].ap)->value,
	     ((FACT_num_t) args[0].ap)->value);
  }
  END_SEG ();
    
  SEG (APPEND);
  {
    args[0] = *Furlow_register (progm[CURR_IP][1]);
    args[1] = *Furlow_register (progm[CURR_IP][2]);
    if (args[1].type == NUM_TYPE) {
      if (args[0].type == SCOPE_TYPE)
	FACT_throw_error (CURR_THIS, "cannot append a scope to a number");
      FACT_append_num (args[1].ap, args[0].ap);
    } else {
      if (args[0].type == NUM_TYPE)
	FACT_throw_error (CURR_THIS, "cannot append a number to a scope");
      FACT_append_scope (args[1].ap, args[0].ap);
    }
  }
  END_SEG ();
    
  SEG (CALL);
  {
    args[0].ap = Furlow_reg_val (progm[CURR_IP][1], SCOPE_TYPE);
    push_c (*(FACT_cast_to_scope (args[0])->code) - 1, args[0].ap);
      
    /* Check if extrn_func is set, and if so call it. */
    if (FACT_cast_to_scope (args[0])->extrn_func == NULL)
      NEXT_INST ();
    FACT_cast_to_scope (args[0])->extrn_func ();
      
    /* Close any open trap regions. */
    while (curr_thread->num_traps != 0
	   && curr_thread->traps[curr_thread->num_traps][1] == curr_thread->cstack_size)
      pop_t ();
      
    /* Pop the call stack. */
    pop_c ();
  }
  END_SEG ();
    
  SEG (CEQ);
  {
    args[0].ap = Furlow_reg_val (progm[CURR_IP][1], NUM_TYPE);
    args[1].ap = Furlow_reg_val (progm[CURR_IP][2], NUM_TYPE);
    args[2].ap = Furlow_reg_val (progm[CURR_IP][3], NUM_TYPE);
    if (FACT_cast_to_num (args[2])->locked)
      FACT_throw_error (CURR_THIS, "cannot set immutable variable");
    mpc_set_ui (FACT_cast_to_num (args[2])->value,
		(FACT_compare_num (args[1].ap, args[0].ap) == 0
		 ? 1
		 : 0));
  }
  END_SEG ();
    
  SEG (CLE);
  {
    args[0].ap = Furlow_reg_val (progm[CURR_IP][1], NUM_TYPE);
    args[1].ap = Furlow_reg_val (progm[CURR_IP][2], NUM_TYPE);
    args[2].ap = Furlow_reg_val (progm[CURR_IP][3], NUM_TYPE);
    if (FACT_cast_to_num (args[2])->locked)
      FACT_throw_error (CURR_THIS, "cannot set immutable variable");
    mpc_set_ui (FACT_cast_to_num (args[2])->value,
		(FACT_compare_num (args[1].ap, args[0].ap) <= 0
		 ? 1
		 : 0));
  }
  END_SEG ();
      
  SEG (CLT);
  {
    args[0].ap = Furlow_reg_val (progm[CURR_IP][1], NUM_TYPE);
    args[1].ap = Furlow_reg_val (progm[CURR_IP][2], NUM_TYPE);
    args[2].ap = Furlow_reg_val (progm[CURR_IP][3], NUM_TYPE);
    if (FACT_cast_to_num (args[2])->locked)
      FACT_throw_error (CURR_THIS, "cannot set immutable variable");
    mpc_set_ui (FACT_cast_to_num (args[2])->value,
		(FACT_compare_num (args[1].ap, args[0].ap) < 0
		 ? 1
		 : 0));
  }
  END_SEG ();
      
  SEG (CME);
  {
    args[0].ap = Furlow_reg_val (progm[CURR_IP][1], NUM_TYPE);
    args[1].ap = Furlow_reg_val (progm[CURR_IP][2], NUM_TYPE);
    args[2].ap = Furlow_reg_val (progm[CURR_IP][3], NUM_TYPE);
    if (FACT_cast_to_num (args[2])->locked)
      FACT_throw_error (CURR_THIS, "cannot set immutable variable");
    mpc_set_ui (FACT_cast_to_num (args[2])->value,
		(FACT_compare_num (args[1].ap, args[0].ap) >= 0
		 ? 1
		 : 0));
  }
  END_SEG ();

  SEG (CMT);
  {
    args[0].ap = Furlow_reg_val (progm[CURR_IP][1], NUM_TYPE);
    args[1].ap = Furlow_reg_val (progm[CURR_IP][2], NUM_TYPE);
    args[2].ap = Furlow_reg_val (progm[CURR_IP][3], NUM_TYPE);
    if (FACT_cast_to_num (args[2])->locked)
      FACT_throw_error (CURR_THIS, "cannot set immutable variable");
    mpc_set_ui (FACT_cast_to_num (args[2])->value,
		(FACT_compare_num (args[1].ap, args[0].ap) > 0
		 ? 1
		 : 0));
  }
  END_SEG ();
	  
  SEG (CNE);
  {
    args[0].ap = Furlow_reg_val (progm[CURR_IP][1], NUM_TYPE);
    args[1].ap = Furlow_reg_val (progm[CURR_IP][2], NUM_TYPE);
    args[2].ap = Furlow_reg_val (progm[CURR_IP][3], NUM_TYPE);
    if (FACT_cast_to_num (args[2])->locked)
      FACT_throw_error (CURR_THIS, "cannot set immutable variable");
    mpc_set_ui (FACT_cast_to_num (args[2])->value,
		(FACT_compare_num (args[1].ap, args[0].ap) != 0
		 ? 1
		 : 0));
  }
  END_SEG ();

  SEG (CONSTS);
  {
    push_constant_str (progm[CURR_IP] + 1);
  }
  END_SEG ();

  SEG (CONSTI);
  {
    push_constant_si (get_seg_addr (progm[CURR_IP] + 1));
  }
  END_SEG ();

  SEG (CONSTU);
  {
    push_constant_ui (get_seg_addr (progm[CURR_IP] + 1));
  }
  END_SEG ();

  SEG (DEC);
  {
    /* Decrement a register. */
    args[0].ap = Furlow_reg_val (progm[CURR_IP][1], NUM_TYPE);
    if (FACT_cast_to_num (args[0])->locked)
      FACT_throw_error (CURR_THIS, "cannot set immutable variable");
    mpc_sub_ui (FACT_cast_to_num (args[0])->value,
		FACT_cast_to_num (args[0])->value, 1);
  }
  END_SEG ();

  SEG (DEF_N);
  {
    /* Declare a number variable. */
    FACT_def_num (progm[CURR_IP] + 1, false);
  }
  END_SEG ();

  SEG (DEF_S);
  {
    /* Declare a scope variable. */
    FACT_def_scope (progm[CURR_IP] + 1, false);
  }
  END_SEG ();

  SEG (DIV);
  {
    args[0].ap = Furlow_reg_val (progm[CURR_IP][1], NUM_TYPE);
    args[1].ap = Furlow_reg_val (progm[CURR_IP][2], NUM_TYPE);
    args[2].ap = Furlow_reg_val (progm[CURR_IP][3], NUM_TYPE);
    if (FACT_cast_to_num (args[2])->locked)
      FACT_throw_error (CURR_THIS, "cannot set immutable variable");
    if (!mpc_cmp_ui (((FACT_num_t) args[0].ap)->value, 0))
      FACT_throw_error (CURR_THIS, "division by zero error");
    mpc_div (((FACT_num_t) args[2].ap)->value,
	     ((FACT_num_t) args[1].ap)->value,
	     ((FACT_num_t) args[0].ap)->value);
  }
  END_SEG ();

  SEG (DROP);
  {
    /* Remove the first item on the variable stack. */
    if (curr_thread->vstackp >= curr_thread->vstack)
      pop_v (); /* Just pop and ignore. */
  }
  END_SEG ();

  SEG (DUP);
  {
    /* Duplicate the first item on the stack. */
    args[0] = *Furlow_register (R_TOP);
    if (args[0].type == SCOPE_TYPE)
      /* To duplicate a scope we simply push it back onto the stack. */
      push_v (args[0]);
    else {
      /* Copy the number and push it on to the stack. */
      args[1].ap = FACT_alloc_num ();
      FACT_set_num (args[1].ap, args[0].ap);
      args[1].type = NUM_TYPE;
      push_v (args[1]);
    }
  }
  END_SEG ();

  SEG (ELEM);
  {
    /* Get an element of an array. */
    args[0] = *Furlow_register (progm[CURR_IP][1]);
    if (args[0].type == NUM_TYPE)
      FACT_get_num_elem (args[0].ap, progm[CURR_IP] + 2);
    else
      FACT_get_scope_elem (args[0].ap, progm[CURR_IP] + 2);
  }
  END_SEG ();

  SEG (EXIT);
  {
    /* Close any open trap regions. */
    while (curr_thread->num_traps != 0
	   && (curr_thread->traps[curr_thread->num_traps][1]
	       == (curr_thread->cstackp - curr_thread->cstack + 1)))
      pop_t ();
	
    /* Exit the current scope. */
    cs_arg = pop_c ();
    CURR_IP = cs_arg.ip;
    args[0].ap = cs_arg.this;
    args[0].type = SCOPE_TYPE;
    push_v (args[0]);
  }
  END_SEG ();

  SEG (GLOBAL);
  {
    args[0] = *Furlow_register (progm[CURR_IP][1]);
    if (progm[CURR_IP][2] != '\0') {
      if (args[0].type == NUM_TYPE)
	FACT_cast_to_num (args[0])->name = progm[CURR_IP] + 2;
      else
	FACT_cast_to_scope (args[0])->name = progm[CURR_IP] + 2;
    }
    FACT_add_to_table (&Furlow_globals, args[0]);
  }
  END_SEG ();
    
  SEG (GOTO);
  {
    args[0].ap = Furlow_reg_val (progm[CURR_IP][1], SCOPE_TYPE);
    CURR_IP = *(FACT_cast_to_scope (args[0])->code) - 1;
  }
  END_SEG ();

  SEG (HALT);
  {
    curr_thread->run_flag = T_DEAD; /* The thread is dead, for now. */
    return; /* Exit. */
  }
  END_SEG ();

  SEG (INC);
  {
    /* Increment a register. */
    args[0].ap = Furlow_reg_val (progm[CURR_IP][1], NUM_TYPE);
    if (FACT_cast_to_num (args[0])->locked)
      FACT_throw_error (CURR_THIS, "cannot set immutable variable");
    mpc_add_ui (FACT_cast_to_num (args[0])->value,
		FACT_cast_to_num (args[0])->value, 1);
  }
  END_SEG ();

  SEG (IOR);
  {
    args[0].ap = Furlow_reg_val (progm[CURR_IP][1], NUM_TYPE);
    args[1].ap = Furlow_reg_val (progm[CURR_IP][2], NUM_TYPE);
    args[2].ap = Furlow_reg_val (progm[CURR_IP][3], NUM_TYPE);
    if (FACT_cast_to_num (args[2])->locked)
      FACT_throw_error (CURR_THIS, "cannot set immutable variable");
    if (mpc_is_float (((FACT_num_t) args[0].ap)->value) ||
	mpc_is_float (((FACT_num_t) args[1].ap)->value))
      /*
	if (((FACT_num_t) args[0].ap)->value->precision
	|| ((FACT_num_t) args[1].ap)->value->precision) */
      FACT_throw_error (CURR_THIS, "arguments to bitwise operators cannot be floating point");
    mpc_ior (((FACT_num_t) args[2].ap)->value,
	     ((FACT_num_t) args[1].ap)->value,
	     ((FACT_num_t) args[0].ap)->value);
  }
  END_SEG ();
	

  SEG (IS_AUTO);
  {
    push_constant_ui (FACT_get_local (CURR_THIS, progm[CURR_IP] + 1) == NULL
		      ? 0
		      : 1);
  }
  END_SEG ();

  SEG (IS_DEF);
  {
    push_constant_ui (FACT_get_global (CURR_THIS, progm[CURR_IP] + 1) == NULL
		      ? 0
		      : 1);
  }
  END_SEG ();

  SEG (JMP);
  {
    /* Unconditional jump. */
    CURR_IP = get_seg_addr (progm[CURR_IP] + 1) - 1;
  }
  END_SEG ();

  SEG (JMP_PNT);
  {
    /* Push a scope to the stack with code = to the address. */
    args[0].ap = FACT_alloc_scope ();
    *FACT_cast_to_scope (args[0])->code = get_seg_addr (progm[CURR_IP] + 1);
    args[0].type = SCOPE_TYPE;
    push_v (args[0]);
  }
  END_SEG ();

  SEG (JIF);
  {
    /* Jump on false. */
    args[0].ap = Furlow_reg_val (progm[CURR_IP][1], NUM_TYPE);
    if (!mpc_cmp_ui (((FACT_num_t) args[0].ap)->value, 0))
      CURR_IP = get_seg_addr (progm[CURR_IP] + 2) - 1;
  }
  END_SEG ();

  SEG (JIN);
  {
    /* Jump on type `number'. */
    args[0] = *Furlow_register (progm[CURR_IP][1]);
    if (args[0].type == NUM_TYPE)
      CURR_IP = get_seg_addr (progm[CURR_IP] + 2) - 1;
  }
  END_SEG ();

  SEG (JIS);
  {
    /* Jump on type `scope'. */
    args[0] = *Furlow_register (progm[CURR_IP][1]);
    if (args[0].type == SCOPE_TYPE)
      CURR_IP = get_seg_addr (progm[CURR_IP] + 2) - 1;
  }
  END_SEG ();

  SEG (JIT);
  {
    /* Jump on true. */
    args[0].ap = Furlow_reg_val (progm[CURR_IP][1], NUM_TYPE);
    if (mpc_cmp_ui (((FACT_num_t) args[0].ap)->value, 0))
      CURR_IP = get_seg_addr (progm[CURR_IP] + 2) - 1;
  }
  END_SEG ();

  SEG (LAMBDA);
  {
    /* Push a lambda scope to the stack. */
    args[0].ap = FACT_alloc_scope ();
    args[0].type = SCOPE_TYPE;
    push_v (args[0]);
  }
  END_SEG ();

  SEG (LOCK);
  {
    args[0] = *Furlow_register (progm[CURR_IP][1]);
    if (args[0].type == NUM_TYPE)
      FACT_lock_num (args[0].ap);
    else
      FACT_cast_to_scope (args[0])->lock_stat = HARD_LOCK;
  }
  END_SEG ();

  SEG (MOD);
  {
    args[0].ap = Furlow_reg_val (progm[CURR_IP][1], NUM_TYPE);
    args[1].ap = Furlow_reg_val (progm[CURR_IP][2], NUM_TYPE);
    args[2].ap = Furlow_reg_val (progm[CURR_IP][3], NUM_TYPE);
    if (FACT_cast_to_num (args[2])->locked)
      FACT_throw_error (CURR_THIS, "cannot set immutable variable");
    if (!mpc_cmp_ui (((FACT_num_t) args[0].ap)->value, 0))
      FACT_throw_error (CURR_THIS, "mod by zero error");
    mpc_mod (((FACT_num_t) args[2].ap)->value,
	     ((FACT_num_t) args[1].ap)->value,
	     ((FACT_num_t) args[0].ap)->value);
  }
  END_SEG ();

  SEG (MUL);
  {
    args[0].ap = Furlow_reg_val (progm[CURR_IP][1], NUM_TYPE);
    args[1].ap = Furlow_reg_val (progm[CURR_IP][2], NUM_TYPE);
    args[2].ap = Furlow_reg_val (progm[CURR_IP][3], NUM_TYPE);
    if (FACT_cast_to_num (args[2])->locked)
      FACT_throw_error (CURR_THIS, "cannot set immutable variable");
    mpc_mul (((FACT_num_t) args[2].ap)->value,
	     ((FACT_num_t) args[1].ap)->value,
	     ((FACT_num_t) args[0].ap)->value);
  }
  END_SEG ();

  SEG (NAME);
  {
    args[0].ap = Furlow_reg_val (progm[CURR_IP][1], SCOPE_TYPE);
    args[1].ap = Furlow_reg_val (progm[CURR_IP][2], SCOPE_TYPE);
    FACT_cast_to_scope (args[1])->name = FACT_cast_to_scope (args[0])->name;
  }
  END_SEG ();

  SEG (NEG);
  {
    args[0].ap = Furlow_reg_val (progm[CURR_IP][1], NUM_TYPE);
    if (FACT_cast_to_num (args[0])->locked)
      FACT_throw_error (CURR_THIS, "cannot set immutable variable");
    mpc_neg (FACT_cast_to_num (args[0])->value,
	     FACT_cast_to_num (args[0])->value);
  }
  END_SEG ();

  SEG (NEW_N);
  {
    /* Create a new anonymous number. */
    FACT_def_num (progm[CURR_IP] + 1, true);
  }
  END_SEG ();

  SEG (NEW_S);
  {
    /* Create a new anonymous scope. */
    FACT_def_scope (progm[CURR_IP] + 1, true);
  }
  END_SEG ();

  SEG (NOP);
  {
    /* Do nothing. */
  }
  END_SEG ();

  SEG (PURGE);
  {
    /* Remove all items from the variable stack. */
    if (curr_thread->vstackp >= curr_thread->vstack) {
      /* Only purge if there actually are items in the var stack. */
      FACT_free (curr_thread->vstack);
      curr_thread->vstackp = curr_thread->vstack = NULL;
    }
  }
  END_SEG ();

  SEG (REF);
  {
    /* Set a register to the reference of another. */ 
    reg_args[0] = Furlow_register (progm[CURR_IP][1]);
    reg_args[1] = Furlow_register (progm[CURR_IP][2]);
    reg_args[1]->type = reg_args[0]->type;
    reg_args[1]->ap = reg_args[0]->ap;
    reg_args[1]->home = reg_args[0]->home;
  }
  END_SEG ();

  SEG (RET);
  {
    /* Close any open trap regions. */
    while (curr_thread->num_traps != 0
	   && (curr_thread->traps[curr_thread->num_traps][1]
	       == (curr_thread->cstackp - curr_thread->cstack + 1)))
      pop_t ();
    /* Pop the call stack. */
    pop_c ();
  }
  END_SEG ();

  SEG (SET_C);
  {
    args[0].ap = Furlow_reg_val (progm[CURR_IP][1], SCOPE_TYPE);
    *FACT_cast_to_scope (args[0])->code = get_seg_addr (progm[CURR_IP] + 2);
  }
  END_SEG ();

  SEG (SET_F);
  {
    args[0].ap = Furlow_reg_val (progm[CURR_IP][1], SCOPE_TYPE);
    args[1].ap = Furlow_reg_val (progm[CURR_IP][2], SCOPE_TYPE);
    FACT_cast_to_scope (args[1])->code = FACT_cast_to_scope (args[0])->code;
    FACT_cast_to_scope (args[1])->extrn_func = FACT_cast_to_scope (args[0])->extrn_func;
  }
  END_SEG ();

  SEG (SPRT);
  {
    FACT_thread_t curr;

    /* Gain control of threads. */
    Furlow_lock_threads ();

    /* Find the next open spot in the linked list. */
    for (curr = threads; curr->next != NULL; curr = curr->next);

    /* Allocate and initialize the new thread. */
    curr->next = FACT_malloc (sizeof (struct FACT_thread));
    curr = curr->next;
    curr->thread_num = num_threads++;
    curr->curr_err.what = DEF_ERR_MSG;
    curr->cstack_size = 1;
    curr->cstackp = curr->cstack = FACT_malloc (sizeof (struct cstack_t));
    curr->root_message = NULL;
    curr->num_messages = 0;
    pthread_mutex_init (&curr->queue_lock, NULL);
    pthread_cond_init (&curr->msg_block, NULL);
	
    /* Set the top scope and the IP of the thread. */
    THIS_OF (curr) = FACT_alloc_scope ();
    THIS_OF (curr)->name = "main<thread>";
    IP_OF (curr) = CURR_IP + 1;

    /* Initialize the registers. */
    for (i = 0; i < T_REGISTERS; i++)
      curr->registers[i].type = UNSET_TYPE;

    /* Push the TID of the new thread to the var stack. */
    push_constant_ui (curr->thread_num);
	
    /* Run the thread and give up control of threads. */
    pthread_create (&curr->thread_id, NULL, Furlow_thread_mask, curr);
    Furlow_unlock_threads ();

    /* Jump. */
    CURR_IP = get_seg_addr (progm[CURR_IP] + 1) - 1;
  }
  END_SEG ();

  SEG (STO);
  {
    /* STO,$A,$B : $B <- $A */
    args[0] = *Furlow_register (progm[CURR_IP][1]);
    args[1] = *Furlow_register (progm[CURR_IP][2]);

    if (args[1].type == UNSET_TYPE) {
      FACT_t v;
      if (args[0].type == UNSET_TYPE) {
	FACT_t *t;
	if ((t = FACT_find_in_table_nohash (args[0].home, args[0].ap)) == NULL)
	  FACT_throw_error (CURR_THIS, "value is unset");
	args[0] = *t;
      }
      args[1].type = args[0].type;
      if (args[0].type == NUM_TYPE) {
	FACT_num_t t = FACT_alloc_num ();
	t->name = args[1].ap;
	args[1].ap = t;
      } else {
	FACT_scope_t t = FACT_alloc_scope ();
	t->name = args[1].ap;
	args[1].ap = t;
      }
      FACT_add_to_table (args[1].home, args[1]);
    }
    if (args[1].type == NUM_TYPE) {
      if (args[0].type == UNSET_TYPE) {
	FACT_t *t;
	if ((t = FACT_find_in_table_nohash (args[0].home, args[0].ap)) == NULL)
	  FACT_throw_error (CURR_THIS, "value is unset");
	args[0] = *t;
      }
      if (args[0].type == SCOPE_TYPE)
	FACT_throw_error (CURR_THIS, "cannot set a number to a scope");
      if (FACT_cast_to_num (args[1])->locked)
	FACT_throw_error (CURR_THIS, "cannot set immutable variable");
      FACT_set_num (args[1].ap, args[0].ap);
    } else {
      struct FACT_scope temp[1];
    
      if (args[0].type == UNSET_TYPE) {
	FACT_t *t;
	if ((t = FACT_find_in_table_nohash (args[0].home, args[0].ap)) == NULL)
	  FACT_throw_error (CURR_THIS, "value is unset");
	args[0] = *t;
      }
      if (args[0].type == NUM_TYPE)
	FACT_throw_error (CURR_THIS, "cannot set a scope to a number");
      if (FACT_cast_to_scope (args[1])->lock_stat == HARD_LOCK)
	FACT_throw_error (CURR_THIS, "cannot set immutable variable");

      hold_name = ((FACT_scope_t) args[1].ap)->name;
      memcpy (args[1].ap, args[0].ap, sizeof (struct FACT_scope));
      ((FACT_scope_t) args[1].ap)->name = hold_name;

      if (FACT_cast_to_scope (args[1])->lock_stat == HARD_LOCK)
	FACT_cast_to_scope (args[1])->lock_stat = SOFT_LOCK;
    }
  }
  END_SEG ();

  SEG (SUB);
  {
    args[0].ap = Furlow_reg_val (progm[CURR_IP][1], NUM_TYPE);
    args[1].ap = Furlow_reg_val (progm[CURR_IP][2], NUM_TYPE);
    args[2].ap = Furlow_reg_val (progm[CURR_IP][3], NUM_TYPE);
    if (FACT_cast_to_num (args[2])->locked)
      FACT_throw_error (CURR_THIS, "cannot set immutable variable");
    mpc_sub (((FACT_num_t) args[2].ap)->value,
	     ((FACT_num_t) args[1].ap)->value,
	     ((FACT_num_t) args[0].ap)->value);
  }
  END_SEG ();

  SEG (SWAP);
  {
    /* Swap the first two elements on the var stack. */
    if (curr_thread->vstackp >= curr_thread->vstack + 1) {
      /* Only swap if there are at least two elements on the var stack. We don't
       * throw an error otherwise, simply nothing occurs. Perhaps this shouldn't
       * be the case?
       */
      FACT_t hold;
      hold = *curr_thread->vstackp;
      *curr_thread->vstackp = *(curr_thread->vstackp - 1);
      *(curr_thread->vstackp - 1) = hold;
    }
  }
  END_SEG ();

  SEG (THIS);
  {
    args[0].ap = CURR_THIS;
    args[0].type = SCOPE_TYPE;
    push_v (args[0]);
  }
  END_SEG ();

  SEG (TRAP_B);
  {
    /* Set a new trap region. */
    push_t (get_seg_addr (progm[CURR_IP] + 1),
	    (curr_thread->cstackp - curr_thread->cstack + 1));
  }
  END_SEG ();

  SEG (TRAP_E);
  {
    /* End a trap region. */
    pop_t ();
  }
  END_SEG ();

  SEG (USE);
  {
    args[0].ap = Furlow_reg_val (progm[CURR_IP][1], SCOPE_TYPE);
    push_c (CURR_IP, args[0].ap);
  }
  END_SEG ();

  SEG (VAR);
  {
    /* Load a variable. */
    FACT_get_var (progm[CURR_IP] + 1);
  }
  END_SEG ();

  SEG (VA_ADD);
  {
    struct FACT_va_list *curr;
      
    args[0] = *Furlow_register (progm[CURR_IP][1]);
    args[1].ap = Furlow_reg_val (progm[CURR_IP][2], SCOPE_TYPE);
      
    if (FACT_cast_to_scope (args[1])->variadic == NULL) {
      curr = FACT_cast_to_scope (args[1])->variadic = FACT_malloc (sizeof (struct FACT_va_list));
      curr->var = args[0];
      curr->next = NULL;
    } else {
      for (curr = FACT_cast_to_scope (args[1])->variadic;
	   curr->next != NULL;
	   curr = curr->next)
	;
      curr->next = FACT_malloc (sizeof (struct FACT_va_list));
      curr->next->var = args[0];
      curr->next->next = NULL;
    }
  }
  END_SEG ();

  SEG (XOR);
  {
    args[0].ap = Furlow_reg_val (progm[CURR_IP][1], NUM_TYPE);
    args[1].ap = Furlow_reg_val (progm[CURR_IP][2], NUM_TYPE);
    args[2].ap = Furlow_reg_val (progm[CURR_IP][3], NUM_TYPE);
    if (FACT_cast_to_num (args[2])->locked)
      FACT_throw_error (CURR_THIS, "cannot set immutable variable");
    if (mpc_is_float (((FACT_num_t) args[0].ap)->value) ||
	mpc_is_float (((FACT_num_t) args[1].ap)->value))/*
							  if (((FACT_num_t) args[0].ap)->value->precision
							  || ((FACT_num_t) args[1].ap)->value->precision) */
      FACT_throw_error (CURR_THIS, "arguments to bitwise operators cannot be floating point");
    mpc_xor (((FACT_num_t) args[2].ap)->value,
	     ((FACT_num_t) args[1].ap)->value,
	     ((FACT_num_t) args[0].ap)->value);
  }
  END_SEG ();
}

static inline size_t get_seg_addr (char *arg) /* Convert a segment address to a ulong. */
{
  size_t n;

  n = (unsigned char) arg[0];
  n = (n << 8) + (unsigned char) arg[1];
  n = (n << 8) + (unsigned char) arg[2];
  n = (n << 8) + (unsigned char) arg[3];

  return n;
}

void *Furlow_thread_mask (void *new_thread)
{
  struct cstack_t frame;
  
  /* Set the recover jmp buffer. */
  if (setjmp (recover)) {
#ifdef DEBUG
    /* Print out the error and a stack trace. */
    fprintf (stderr, "Caught unhandled error: %s\n", curr_thread->curr_err.what);
    while (curr_thread->cstackp - curr_thread->cstack >= 0) {
      frame = pop_c ();
      fprintf (stderr, "\tat scope %s (%s:%zu)\n", frame.this->name, FACT_get_file (frame.ip), FACT_get_line (frame.ip));
    }
#endif /* DEBUG */
  }  else {
    /* Set curr_thread to new_thread and run the VM. */
    curr_thread = new_thread;
    Furlow_run ();
  }
  return NULL;
}

inline void push_constant_str (char *cval) /* Push a constant number to the var stack. */
{
  /* TODO: add caching, maybe */
  FACT_t push_val;

  push_val.type = NUM_TYPE;
  push_val.ap = FACT_alloc_num ();
  
  /* Check for hexidecimal. */
  if (cval[0] == '0' && tolower (cval[1]) == 'x')
    mpc_set_str (((FACT_num_t) push_val.ap)->value, cval + 2, 16);
  else
    mpc_set_str (((FACT_num_t) push_val.ap)->value, cval, 10);

  push_v (push_val);
}

inline void push_constant_ui (unsigned long int n)
{
  FACT_t push_val;

  push_val.type = NUM_TYPE;
  push_val.ap = FACT_alloc_num ();

  if (n != 0)
    mpc_set_ui (((FACT_num_t) push_val.ap)->value, n);
  
  push_v (push_val);
}

inline void push_constant_si (signed long int n)
{
  FACT_t push_val;

  push_val.type = NUM_TYPE;
  push_val.ap = FACT_alloc_num ();

  if (n != 0)
    mpc_set_si (((FACT_num_t) push_val.ap)->value, n);
  
  push_v (push_val);
}

void Furlow_init_vm (void) /* Create the main scope and thread. */
{
  int i;
  
  curr_thread = threads = FACT_malloc (sizeof (struct FACT_thread));
  memset (threads, 0, sizeof (struct FACT_thread));
  num_threads = 1;
  threads->cstack_size++;
  threads->cstackp = threads->cstack = FACT_malloc (sizeof (struct cstack_t));
  threads->curr_err.what = DEF_ERR_MSG;
  threads->next = NULL;
  threads->root_message = NULL;
  threads->num_messages = 0;
  pthread_mutex_init (&threads->queue_lock, NULL);
  pthread_cond_init (&threads->msg_block, NULL);
  CURR_THIS = FACT_alloc_scope ();
  CURR_THIS->name = "main";
  CURR_IP = 0;

  for (i = 0; i < T_REGISTERS; i++)
    threads->registers[i].type = UNSET_TYPE;
}

void Furlow_destroy_vm (void) /* Deallocate everything and destroy every thread. */
{
  /* This function has yet to be implemented, as I need to do
   * some thinking beforehand. Nothing major hopefully.
   */
  return;
}

void Furlow_disassemble (void) /* Print the byte code currently loaded into the VM. */
{
  size_t i;
  int inst, ofs, j;

  if (progm == NULL)
    goto end;

  for (i = 0; (inst = progm[i][0]) != HALT; i++) {
    /* print out the instruction and address. */
    printf ("%zu:\t%s", i, Furlow_instructions[inst].token);
    for (j = 0, ofs = 1; Furlow_instructions[inst].args[j] != 0; j++) {
      switch (Furlow_instructions[inst].args[j]) {
      case 'r': /* Register. */
	printf (", %%%d", progm[i][ofs]);
	ofs++;
	break;
	
      case 's': /* String. */
	printf (", $%s", progm[i] + ofs);
	ofs += strlen (progm[i] + ofs);
	break;
	
      case 'a': /* Address. */
	printf (", @%zu", get_seg_addr (progm[i] + ofs));
	ofs += 4;
	break;
	
      default:
	abort ();
      }
    }
    printf ("\n");
  }
 end:
  printf ("%zu:\thalt\n", (progm != NULL) ? i : 0);
}


static void print_num (FACT_num_t val)
{
  size_t i;

  if (val->name != NULL)
    printf ("%s", val->name);
  if (val->array_up != NULL) {
    printf (" [");
    for (i = 0; i < val->array_size; i++) {
      if (i)
	printf (", ");
      print_num (val->array_up[i]);
    }
    printf (" ]");
  } else
    printf (" %s", mpc_get_str (val->value));
}

static void print_scope (FACT_scope_t val)
{
  size_t i;

  if (*val->marked) 
    return;

  if (val->name != NULL)
    printf ("%s", val->name);
  if (*val->array_up != NULL) {
    printf (" [");
    for (i = 0; i < *val->array_size; i++) {
      if (i)
	printf (", ");
      print_scope ((*val->array_up)[i]);
    }
    printf (" ]");
  } else {
    printf ("{ %s%s ", val->name, ((val->vars->total_num_vars == 0) ? "\0" : ":"));
    FACT_table_digest (val->vars);
    printf ("}");
  }
}

static void print_var_stack ()
{
  size_t i;
  FACT_t *p = curr_thread->vstackp;
  if (p <= curr_thread->vstack)
    return;
  printf ("-------\n");
  for (i = 0; p >= curr_thread->vstack; p--, i++) {
    printf ("%zu: ", i);
    if (p->type == NUM_TYPE)
      print_num (p->ap);
    else if (p->type == SCOPE_TYPE)
      print_scope (p->ap);
    else
      printf ("Undef");
    printf ("\n");
  }
  printf ("-------\n");
}

