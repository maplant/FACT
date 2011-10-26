/* This file is part of Furlow VM.
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

#include <FACT.h>

static inline size_t get_seg_addr (char *);

/* Threading and stacks:                         */
FACT_thread_t threads;     /* Thread data.       */
FACT_thread_t curr_thread; /* Thread being run.  */
size_t num_threads;        /* Number of threads. */
#ifdef ADVANCED_THREADING
pthread_mutex_t progm_lock = PTHREAD_MUTEX_INITIALIZER; 
#endif /* ADVANCED_THREADING */

/* Error recovery:                                      */
jmp_buf handle_err; /* Jump to the error handler.       */
jmp_buf recover;    /* When there are no other options. */

/* The machine:                                         */
static char **progm;     /* Program being run.          */
static char **next_inst; /* Next available instruction. */

void
Furlow_add_instruction (char *new) /* Add an instruction to the progm. */
{
  size_t nsize;
  static char halt_nowrite[1] = { HALT }; 

  if (progm == NULL) /* No program yet. */
    {
      next_inst = progm = FACT_malloc (sizeof (char *) * 2);
      *progm = new;
      *++next_inst = halt_nowrite; 
    }
  else
    {
      *next_inst = new;
      nsize = next_inst - progm + 1; 
      progm = FACT_realloc (progm, sizeof (char *) * (nsize + 1)); 
      next_inst = progm + nsize;
      *next_inst = halt_nowrite;
    }
}

#ifdef ADVANCED_THREADING
inline void
Furlow_lock_program (void) /* Lock progm. */
{
  pthread_mutex_lock (progm_lock);
}

inline void
Furlow_unlock_program (void) /* Unlock progm. */
{
  pthread_mutex_unlock (progm_lock);
}
#endif /* ADVANCED_THREADING */

inline size_t
Furlow_offset (void) /* Get the instruction offset. */
{
  /* Return the number of instructions there are, minus the terminating HALT. */
  return (((next_inst - progm) == 0)
	  ? 0
	  : (next_inst - progm));
}
  
FACT_t
pop_v () /* Pop the variable stack. */
{
  FACT_t res;

#ifdef SAFE
  /* Make sure that the stack is not empty. This can only occur due to an 
   * error in user-created BAS code, so we do not make it a required check.
   */
  if (curr_thread->vstack == NULL)
    FACT_throw_error (CURR_THIS, "illegal POP on empty var stack.");
#endif /* SAFE */

  curr_thread->vstack_size--;
  res.ap = curr_thread->vstack[curr_thread->vstack_size].ap;
  res.type = curr_thread->vstack[curr_thread->vstack_size].type;
  curr_thread->vstack[curr_thread->vstack_size].ap = NULL;
  curr_thread->vstack[curr_thread->vstack_size].type = UNSET_TYPE;
  
  return res;
}

struct cstack_t
pop_c () /* Pop the current call stack. */
{
  struct cstack_t res;

#ifdef SAFE
  if (curr_thread->cstack == NULL)
    FACT_throw_error (CURR_THIS, "illegal POP on empty call stack.");
#endif /* SAFE */

  curr_thread->cstack_size--;
  memcpy (&res, &curr_thread->cstack[curr_thread->cstack_size], sizeof (struct cstack_t));
  curr_thread->cstack[curr_thread->cstack_size].this = NULL;
  curr_thread->cstack[curr_thread->cstack_size].ip = 0;

  return res;
}

size_t *
pop_t () /* Pop the trap stack. */
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
  else
    {
      FACT_free (curr_thread->traps);
      curr_thread->traps = NULL;
    }

  return res;
}

void
push_v (FACT_t n) /* Push to the variable stack. */
{
  size_t alloc_size;
  
  curr_thread->vstack_size++;
  if (curr_thread->vstack_size >= curr_thread->vstack_max)
    {
      if (curr_thread->vstack_max == 0)
	curr_thread->vstack_max = 1;
      else
	curr_thread->vstack_max <<= 1; /* Sqaure the size of the var stack. */
      curr_thread->vstack = FACT_realloc (curr_thread->vstack, sizeof (FACT_t) * (curr_thread->vstack_max));
    }

  curr_thread->vstack[curr_thread->vstack_size - 1].type = n.type;
  curr_thread->vstack[curr_thread->vstack_size - 1].ap = n.ap;
}

void
push_c (size_t nip, FACT_scope_t nthis) /* Push to the call stack. */
{
  curr_thread->cstack_size++;
  if (curr_thread->cstack_size >= curr_thread->cstack_max)
    {
      /* Sqaure the size of the call stack. The call stack will never be NULL. */
      curr_thread->cstack_max <<= 1;
      curr_thread->cstack = FACT_realloc (curr_thread->cstack, sizeof (FACT_t) * (curr_thread->cstack_max));
    }

  curr_thread->cstack[curr_thread->cstack_size - 1].ip = nip;
  curr_thread->cstack[curr_thread->cstack_size - 1].this = nthis;
}

void
push_t (size_t n1, size_t n2) /* Push to the trap stack. */
{
  curr_thread->num_traps++;
  curr_thread->traps = FACT_realloc (curr_thread->traps, sizeof (size_t [2]) * (curr_thread->num_traps));
  curr_thread->traps[curr_thread->num_traps - 1][0] = n1;
  curr_thread->traps[curr_thread->num_traps - 1][1] = n2;
}

FACT_t *
Furlow_register (int reg_number) /* Access a Furlow machine register. */
{
  /* Check for special registers. */
  if (reg_number < S_REGISTERS)
    {
      switch (reg_number)
	{
	case R_POP:
	  /* Store the result in the registers array. */
	  curr_thread->registers[R_POP] = pop_v ();
	  break;

	case R_TOP:
	  if (curr_thread->vstack_size == 0)
	    FACT_throw_error (CURR_THIS, "illegal TOP on empty stack");
	  return &curr_thread->vstack[curr_thread->vstack_size - 1];

	case R_TID:
	  if (curr_thread->registers[R_TID].type != NUM_TYPE)
	    {
	      curr_thread->registers[R_TID].type = NUM_TYPE;
	      curr_thread->registers[R_TID].ap = FACT_alloc_num ();
	    }
	  mpc_set_ui (((FACT_num_t) curr_thread->registers[R_TID].ap)->value,
		      curr_thread - threads);
	  break;

	default: /* NOTREACHED */
	  abort ();
	}
    }

  /* Probably best to add some extra checks here. */
  return &curr_thread->registers[reg_number];
}

void *
Furlow_reg_val (int reg_number, FACT_type des) /* Safely access a register's value. */
{
  FACT_t *reg;

  reg = Furlow_register (reg_number);
  
  if (reg == NULL)
    FACT_throw_error (CURR_THIS, "unset register, $r%d", reg_number);
  if (reg->type != des)
    FACT_throw_error (CURR_THIS, "register $r%d is type %s",
		      reg_number, des == NUM_TYPE ? "scope, expected number" : "number, expected scope");

  return reg->ap;
}

static void
math_call (char *inst, void (*mfunc)(mpc_t, mpc_t, mpc_t), int itype) /* Call a math function and check for errors. */
{
  FACT_num_t arg1, arg2, dest; /* For arithmetic instructions. */

  assert (itype >= 0 && itype <= 3);

  /* Get the arguments and the destination: */
  arg2 = (FACT_num_t) Furlow_reg_val (inst[1], NUM_TYPE);
  arg1 = (FACT_num_t) Furlow_reg_val (inst[2], NUM_TYPE);
  dest = (FACT_num_t) Furlow_reg_val (inst[3], NUM_TYPE);

  if ((itype == 1 || itype == 2)
      /* Division or mod, check for division by zero error. */
      && !mpc_cmp_ui (arg2->value, 0))
    FACT_throw_error (CURR_THIS, "%s by zero error", itype == 1 ? "division" : "mod");
  else if (itype == 3
	   /* Bitwise, check for floating point. */
	   && (arg1->value->precision || arg2->value->precision))
    FACT_throw_error (CURR_THIS, "arguments to bitwise operators cannot be floating point");

  mfunc (dest->value, arg1->value, arg2->value);
}

void
Furlow_run () /* Run the program until a HALT is reached. */ 
{
  /* It would probably best to rename these. */
  int i;
  char *hold_name;
  size_t tnum;
  FACT_t res;            /* Generic type result.    */
  FACT_t *r1, *r2;       /* Generic type arguments. */
  FACT_num_t n1, n2, n3; /* Number arguments.       */ 
  FACT_scope_t s1, s2;   /* Scope arguments.        */
  FACT_thread_t next;
  struct cstack_t c1;
    
  curr_thread = threads; /* I'm not thinking this through all the way. */
  
 eval:
  /* Set the error handler. */
  if (setjmp (handle_err))
    {
      /* An error has been caught. If there are no available traps, jump to
       * recover. Otherwise, set the ip to the current trap handler.
       */
      if (curr_thread->num_traps == 0)
	longjmp (recover, 1);

      /* Destroy the unecessary stacks and set the ip. */
      while (curr_thread->cstack_size > curr_thread->traps[curr_thread->num_traps - 1][1])
	pop_c ();
      
      CURR_IP = curr_thread->traps[curr_thread->num_traps - 1][0];
      curr_thread->run_flag = T_RUN;
      goto eval; /* Go back to eval to reset the error handler. */
    }

  for (;; curr_thread++)
    {
      bool recheck = false;
      
      /* Run the scheduler to find the next live thread. */
      if (curr_thread - threads >= num_threads)
	{
	  curr_thread = threads;
	  recheck = false;
	}
      
      if (progm[CURR_IP][0] == HALT
	  || curr_thread->run_flag == T_IGNORE
	  || curr_thread->run_flag == T_DEAD)
	{
	  if (curr_thread->run_flag == T_IGNORE)
	    recheck = true; /* Check this thread again later. */
	  
	  /* The thread is unrunnable, find a live one or exit. */
	  for (next = curr_thread + 1;; next++)
	    {
	      if (next - threads >= num_threads)
		next = threads;
	      if (next == curr_thread) /* There were no live threads. */
		{
		  curr_thread = threads;
		  if (!recheck)
		    return;
		}
	      if (progm[IP_OF (next)][0] != HALT)
		break;
	    }
	  curr_thread = next;
	}

      if (curr_thread->run_flag == T_HANDLE) /* Handle a thread's errors. */
	longjmp (handle_err, 1); /* Jump back. */

      /* Execute the next instruction. 
       * Eventually I should put these in alphabetical order. Better yet,
       * just make a jump table. Giant switches are sort of unnecessary.
       * One way this may be structured is to use a bunch of goto labels,
       * and GNU C unary && operator.
       */
      switch (progm[CURR_IP][0])
	{
	case ADD:
	  math_call (progm[CURR_IP], &mpc_add, 0);
	  break;

	case AND:
	  math_call (progm[CURR_IP], &mpc_and, 3);
	  break;

	case APPEND:
	  res = *Furlow_register (progm[CURR_IP][1]);
	  r2 = &res;
	  r1 = Furlow_register (progm[CURR_IP][2]);
	  if (r1->type == NUM_TYPE)
	    {
	      if (r2->type == SCOPE_TYPE)
		FACT_throw_error (CURR_THIS, "cannot append a scope to a number");
	      FACT_append_num (r1->ap, r2->ap);
	    }
	  else
	    {
	      if (r2->type == NUM_TYPE)
		FACT_throw_error (CURR_THIS, "cannot append a number to a scope");
	      FACT_append_scope (r1->ap, r2->ap);
	    }
	  break;

	case CALL:
	  s1 = Furlow_reg_val (progm[CURR_IP][1], SCOPE_TYPE);
	  push_c (*s1->code - 1, s1);

	  /* Check if extrn_func is set, and if so call it. */
	  if (s1->extrn_func == NULL)
	    break;
	  s1->extrn_func ();

	  /* Close any open trap regions. */
	  while (curr_thread->num_traps != 0
		 && curr_thread->traps[curr_thread->num_traps][1] == curr_thread->cstack_size)
	    pop_t ();
	  /* Pop the call stack. */
	  pop_c ();
	  break;

	case CEQ:
	  n2 = Furlow_reg_val (progm[CURR_IP][1], NUM_TYPE);
	  n1 = Furlow_reg_val (progm[CURR_IP][2], NUM_TYPE);
	  n3 = Furlow_reg_val (progm[CURR_IP][3], NUM_TYPE);
	  mpc_set_ui (n3->value, ((FACT_compare_num (n1, n2) == 0)
				  ? 1
				  : 0));
	  break;	  
	  
	case CLE:
	  n2 = Furlow_reg_val (progm[CURR_IP][1], NUM_TYPE);
	  n1 = Furlow_reg_val (progm[CURR_IP][2], NUM_TYPE);
	  n3 = Furlow_reg_val (progm[CURR_IP][3], NUM_TYPE);
	  mpc_set_ui (n3->value, ((FACT_compare_num (n1, n2) <= 0)
				  ? 1
				  : 0));
	  break;
	  	 
	case CLT:
	  n2 = Furlow_reg_val (progm[CURR_IP][1], NUM_TYPE);
	  n1 = Furlow_reg_val (progm[CURR_IP][2], NUM_TYPE);
	  n3 = Furlow_reg_val (progm[CURR_IP][3], NUM_TYPE);
	  mpc_set_ui (n3->value, ((FACT_compare_num (n1, n2) < 0)
				  ? 1
				  : 0));
	  break;

	case CME:
	  n2 = Furlow_reg_val (progm[CURR_IP][1], NUM_TYPE);
	  n1 = Furlow_reg_val (progm[CURR_IP][2], NUM_TYPE);
	  n3 = Furlow_reg_val (progm[CURR_IP][3], NUM_TYPE);
	  mpc_set_ui (n3->value, ((FACT_compare_num (n1, n2) >= 0)
				  ? 1
				  : 0));
	  break;

	case CMT:
	  n2 = Furlow_reg_val (progm[CURR_IP][1], NUM_TYPE);
	  n1 = Furlow_reg_val (progm[CURR_IP][2], NUM_TYPE);
	  n3 = Furlow_reg_val (progm[CURR_IP][3], NUM_TYPE);
	  mpc_set_ui (n3->value, ((FACT_compare_num (n1, n2) > 0)
				  ? 1
				  : 0));
	  break;
	  
	case CNE:
	  n2 = Furlow_reg_val (progm[CURR_IP][1], NUM_TYPE);
	  n1 = Furlow_reg_val (progm[CURR_IP][2], NUM_TYPE);
	  n3 = Furlow_reg_val (progm[CURR_IP][3], NUM_TYPE);
	  mpc_set_ui (n3->value, ((FACT_compare_num (n1, n2) != 0)
				  ? 1
				  : 0));
	  break;

	case CONST:
	  /* Push a constant value to the stack. */
	  push_constant (progm[CURR_IP] + 1);
	  break;

	case DEC:
	  /* Decrement a register. */
	  n1 = Furlow_reg_val (progm[CURR_IP][1], NUM_TYPE);
	  mpc_sub_ui (n1->value, n1->value, 1);
	  break;

	case DEF_N:
	  /* Declare a number variable. */
	  FACT_def_num (progm[CURR_IP] + 1, false);
	  break;

	case DEF_S:
	  /* Declare a scope variable. */
	  FACT_def_scope (progm[CURR_IP] + 1, false);
	  break;
	  	  
	case DIV:
	  math_call (progm[CURR_IP], &mpc_div, 1);
	  break;
	  
	case DROP:
	  /* Remove the first item on the variable stack. */
	  if (curr_thread->vstack_size >= 1)
	    pop_v (); /* Just pop and ignore. */
	  break;

	case DUP:
	  /* Duplicate the first item on the stack. */
	  r1 = Furlow_register (R_TOP);
	  if (r1->type == SCOPE_TYPE)
	    /* To duplicate a scope we simply push it back onto the stack. */
	    push_v (*r1);
	  else
	    {
	      /* Copy the number and push it on to the stack. */
	      n1 = FACT_alloc_num ();
	      FACT_set_num (n1, r1->ap);
	      res.type = NUM_TYPE;
	      res.ap = n1;
	      push_v (res);
	    }
	  break;

	case ELEM:
	  /* Get an element of an array. */
	  res = *Furlow_register (progm[CURR_IP][1]);
	  if (res.type == NUM_TYPE)
	    FACT_get_num_elem ((FACT_num_t) res.ap, progm[CURR_IP] + 2);
	  else
	    FACT_get_scope_elem ((FACT_scope_t) res.ap, progm[CURR_IP] + 2);
	  break;

	case EXIT:
	  /* Close any open trap regions. */
	  while (curr_thread->num_traps != 0
		 && curr_thread->traps[curr_thread->num_traps][1] == curr_thread->cstack_size)
	    pop_t ();
	  /* Exit the current scope. */
	  c1 = pop_c ();
	  CURR_IP = c1.ip;
	  res.ap = c1.this;
	  res.type = SCOPE_TYPE;
	  push_v (res);
	  break;

	case GOTO:
	  s1 = Furlow_reg_val (progm[CURR_IP][1], SCOPE_TYPE);
	  CURR_IP = *s1->code - 1;
	  break;

	case INC:
	  /* Increment a register. */
	  n1 = Furlow_reg_val (progm[CURR_IP][1], NUM_TYPE);
	  mpc_add_ui (n1->value, n1->value, 1);
	  break;

	case IOR:
	  math_call (progm[CURR_IP], &mpc_ior, 3);
	  break;

	case IS_AUTO:
	  push_constant (FACT_get_local (CURR_THIS, progm[CURR_IP] + 1) == NULL
			 ? "0"
			 : "1");
	  break;

	case IS_DEF:
	  push_constant (FACT_get_global (CURR_THIS, progm[CURR_IP] + 1) == NULL
			 ? "0"
			 : "1");
	  break;

	case JMP:
	  /* Unconditional jump. */
	  CURR_IP = get_seg_addr (progm[CURR_IP] + 1) - 1;
	  break;

	case JMP_PNT:
	  /* Push a scope to the stack with code = to the address. */
	  s1 = FACT_alloc_scope ();
	  *s1->code = get_seg_addr (progm[CURR_IP] + 1);
	  res.ap = s1;
	  res.type = SCOPE_TYPE;
	  push_v (res);
	  break;

	case JIF:
	  /* Jump on false. */
	  res.ap = Furlow_reg_val (progm[CURR_IP][1], NUM_TYPE);
	  if (!mpc_cmp_ui (((FACT_num_t) res.ap)->value, 0))
	    CURR_IP = get_seg_addr (progm[CURR_IP] + 2) - 1;
	  break;

	case JIN:
	  /* Jump on type `number'. */
	  r1 = Furlow_register (progm[CURR_IP][1]);
	  if (r1->type == NUM_TYPE)
	    CURR_IP = get_seg_addr (progm[CURR_IP] + 2) - 1;
	  break;

	case JIS:
	  /* Jump on type `scope'. */
	  r1 = Furlow_register (progm[CURR_IP][1]);
	  if (r1->type == SCOPE_TYPE)
	    CURR_IP = get_seg_addr (progm[CURR_IP] + 2) - 1;
	  break;
	  
	case JIT:
	  /* Jump on true. */
	  res.ap = Furlow_reg_val (progm[CURR_IP][1], NUM_TYPE);
	  if (mpc_cmp_ui (((FACT_num_t) res.ap)->value, 0))
	    CURR_IP = get_seg_addr (progm[CURR_IP] + 2) - 1;
	  break;

	case LAMBDA:
	  /* Push a lambda scope to the stack. */
	  res.ap = FACT_alloc_scope ();
	  res.type = SCOPE_TYPE;
	  push_v (res);
	  break;

	case MOD:
	  math_call (progm[CURR_IP], &mpc_mod, 2);
	  break;

	case MUL:
	  math_call (progm[CURR_IP], &mpc_mul, 0);
	  break;

	case NEG:
	  n1 = Furlow_reg_val (progm[CURR_IP][1], NUM_TYPE);
	  mpc_neg (n1->value, n1->value);
	  break;

	case NEW_N:
	  /* Create a new anonymous number. */
	  FACT_def_num (progm[CURR_IP] + 1, true);
	  break;

	case NEW_S:
	  /* Create a new anonymous scope. */
	  FACT_def_scope (progm[CURR_IP] + 1, true);
	  break;

	case NOP:
	  /* Do nothing. */
	  break;

	case PURGE:
	  /* Remove all items from the variable stack. */
	  if (curr_thread->vstack_size != 0)
	    {
	      /* Only purge if there actually are items in the var stack. */
	      curr_thread->vstack_size = 0;
	      FACT_free (curr_thread->vstack);
	      curr_thread->vstack = NULL;
	    }
	  break;

	case REF: /* REF,$A,$B : B <- A */
	  /* Set a register to the reference of another. */ 
	  r1 = Furlow_register (progm[CURR_IP][1]);
	  r2 = Furlow_register (progm[CURR_IP][2]);
	  r2->type = r1->type;
	  r2->ap = r1->ap;
	  break;

	case RET:
	  /* Close any open trap regions. */
	  while (curr_thread->num_traps != 0
		 && curr_thread->traps[curr_thread->num_traps][1] == curr_thread->cstack_size)
	    pop_t ();
	  /* Pop the call stack. */
	  pop_c ();
	  break;

	case SET_C:
	  s1 = Furlow_reg_val (progm[CURR_IP][1], SCOPE_TYPE);
	  *s1->code = get_seg_addr (progm[CURR_IP] + 2);
	  break;

	case SET_F:
	  s2 = Furlow_reg_val (progm[CURR_IP][1], SCOPE_TYPE);
	  s1 = Furlow_reg_val (progm[CURR_IP][2], SCOPE_TYPE);
	  s1->code = s2->code;
	  s1->extrn_func = s2->extrn_func;
	  break;

	case SET_N:
	  s2 = Furlow_reg_val (progm[CURR_IP][1], SCOPE_TYPE);
	  s1 = Furlow_reg_val (progm[CURR_IP][2], SCOPE_TYPE);
	  s1->name = s2->name;
	  break;

	case SPRT:
	  /* Create a new thread and jump. */
	  tnum = curr_thread - threads;
	  threads = FACT_realloc (threads, sizeof (struct FACT_thread) * ++num_threads);
	  curr_thread = threads + tnum;
	  threads[num_threads - 1].cstack_size++;
	  threads[num_threads - 1].cstack_max++;
	  threads[num_threads - 1].cstack = FACT_malloc (sizeof (struct cstack_t));

	  /* Set the 'this' scope and ip of the thread and jump. */
	  THIS_OF (threads + num_threads - 1) = FACT_alloc_scope ();
	  IP_OF (threads + num_threads - 1) = get_seg_addr (progm[CURR_IP] + 1);

	  /* Add built in functions to the new scope. */
	  FACT_add_BIFs (THIS_OF (threads + num_threads - 1));
	  
	  /* Initialize the registers. */
	  for (i = 0; i < T_REGISTERS; i++)
	    threads[num_threads - 1].registers[i].type = UNSET_TYPE;
	  break;

	case STO: /* STO,$A,$B : $B <- $A */
	  /* We need to store the register value locally so that it doesn't
	   * just become an aliased r1 after the second pop.
	   */
	  res = *Furlow_register (progm[CURR_IP][1]);
	  r2 = &res;
	  r1 = Furlow_register (progm[CURR_IP][2]);
	  if (r1->type == NUM_TYPE)
	    {
	      if (r2->type == SCOPE_TYPE)
		FACT_throw_error (CURR_THIS, "cannot set a scope to a number");
 	      FACT_set_num (r1->ap, r2->ap);
	    }
	  else
	    {
	      if (r2->type == NUM_TYPE)
		FACT_throw_error (CURR_THIS, "cannot set a number to a scope");
	      hold_name = ((FACT_scope_t) r1->ap)->name;
	      memcpy (r1->ap, r2->ap, sizeof (struct FACT_scope));
	      ((FACT_scope_t) r1->ap)->name = hold_name;
	    }
	  break;

	case SUB:
	  math_call (progm[CURR_IP], &mpc_sub, 0);
	  break;

	case SWAP:
	  /* Swap the first two elements on the var stack. */
	  if (curr_thread->vstack_size >= 2)
	    {
	      /* Only swap if there are at least two elements on the var stack. We don't
	       * throw an error otherwise, simply nothing occurs. Perhaps this shouldn't
	       * be the case?
	       */
	      FACT_t hold;
	      hold = curr_thread->vstack[curr_thread->vstack_size - 1];
	      curr_thread->vstack[curr_thread->vstack_size - 1] =
		curr_thread->vstack[curr_thread->vstack_size - 2];
	      curr_thread->vstack[curr_thread->vstack_size - 2] = hold;
	    }
	  break;

	case THIS:
	  res.ap = CURR_THIS;
	  res.type = SCOPE_TYPE;
	  push_v (res);
	  break;

	case TRAP_B:
	  /* Set a new trap region. */
	  push_t (get_seg_addr (progm[CURR_IP] + 1), curr_thread->cstack_size);
	  break;

	case TRAP_E:
	  /* End a trap region. */
	  pop_t ();
	  break; 

	case USE:
	  s1 = Furlow_reg_val (progm[CURR_IP][1], SCOPE_TYPE);
	  push_c (CURR_IP, s1);
	  break;
	  
	case VAR:
	  /* Load a variable. */
	  FACT_get_var (progm[CURR_IP] + 1);
	  break;

	case XOR:
	  math_call (progm[CURR_IP], &mpc_xor, 3);
	  break;
	}

      /* Go to the next instruction. */
      CURR_IP++;
    }
}

static inline size_t
get_seg_addr (char *arg) /* Convert a segment address to a ulong. */
{
  size_t n;

  n = (unsigned char) arg[0];
  n = (n << 8) + (unsigned char) arg[1];
  n = (n << 8) + (unsigned char) arg[2];
  n = (n << 8) + (unsigned char) arg[3];

  return n;
}

void
push_constant (char *cval) /* Push a constant to the var stack. */
{
  /* TODO: add caching. */
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

void
Furlow_init_vm () /* Create the main scope and thread. */
{
  int i;
  
  curr_thread = threads = FACT_malloc (sizeof (struct FACT_thread));
  memset (threads, 0, sizeof (struct FACT_thread));
  num_threads = 1;
  threads->cstack_size++;
  threads->cstack_max++;
  threads->cstack = FACT_malloc (sizeof (struct cstack_t));
  CURR_THIS = FACT_alloc_scope ();
  CURR_THIS->name = "main";
  CURR_IP = 0;

  for (i = 0; i < T_REGISTERS; i++)
    threads->registers[i].type = UNSET_TYPE;
}
