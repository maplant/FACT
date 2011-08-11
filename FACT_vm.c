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

static void push_constant (char *);
static inline unsigned long get_seg_addr (char *);
static FACT_t get_either_noerror (FACT_scope_t, char *);

/* Threading and stacks:                         */
FACT_thread_t threads;     /* Thread data.       */
FACT_thread_t curr_thread; /* Thread being run.  */
unsigned long num_threads; /* Number of threads. */
pthread_mutex_t progm_lock = PTHREAD_MUTEX_INITIALIZER; 

/* Error recovery:                                      */
jmp_buf handle_err; /* Jump to the error handler.       */
jmp_buf recover;    /* When there are no other options. */

/* The machine:                                         */
static char **progm;     /* Program being run.          */
static char **next_inst; /* Next available instruction. */

void
Furlow_add_instruction (char *new) /* Add an instruction to the progm. */
{
  unsigned long nsize;
  static char halt_nowrite[1] = { HALT }; 

#ifdef ADVANCED_THREADING
  /* Wait for control. */
  while (pthread_mutex_trylock (progrm_lock) == EBUSY);
#endif /* ADVANCED_THREADING */  
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
#ifdef ADVANCED_THREADING
  pthread_mutex_unlock (progm_lock);
#endif /* ADVANCED_THREADING */
}

void
Furlow_add_instructions (char **new, unsigned long n) /* Add a swath of instructions. */
{
  unsigned long nsize, i;
  static char halt_nowrite[1] = { HALT };
  
#ifdef ADVANCED_THREADING
  /* Wait for control. */
  while (pthread_mutex_trylock (progrm_lock) == EBUSY);
#endif /* ADVANCED_THREADING */
  assert (n != 0);
  if (progm == NULL)
    {
      next_inst = progm = FACT_malloc (sizeof (char *) * (n + 1));
      for (i = 0; i < n; i++)
	progm[i] = new[i];
      next_inst = progm + n;
      *++next_inst = halt_nowrite;
    }
  else
    {
      nsize = next_inst - progm + n + 1;
      progm = FACT_realloc (progm, sizeof (char *) * nsize);
      for (i = 0; i < n; i++)
	next_inst[i] = new[i];
      next_inst = progm + n;
      *++next_inst = halt_nowrite;
    }
#ifdef ADVANCED_THREADING
  pthread_mutex_unlock (progm_lock);
#endif /* ADVANCED_THREADING */
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
  
  if (curr_thread->vstack_size)
     curr_thread->vstack = FACT_realloc (curr_thread->vstack, sizeof (FACT_t) * curr_thread->vstack_size);
  else
    {
      FACT_free (curr_thread->vstack);
      curr_thread->vstack = NULL;
    }
  
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
  
  if (curr_thread->cstack_size)
    curr_thread->cstack = FACT_realloc (curr_thread->cstack, sizeof (struct cstack_t) * curr_thread->cstack_size);
  else
    {
      FACT_free (curr_thread->cstack);
      curr_thread->cstack = NULL;
    }
  
  return res;
}

void
push_v (FACT_t n) /* Push to the variable stack. */
{
  curr_thread->vstack_size++;
  curr_thread->vstack = FACT_realloc (curr_thread->vstack, sizeof (FACT_t) * (curr_thread->vstack_size));
  curr_thread->vstack[curr_thread->vstack_size - 1].type = n.type;
  curr_thread->vstack[curr_thread->vstack_size - 1].ap = n.ap;
}

void
push_c (unsigned long nip, FACT_scope_t nthis) /* Push to the call stack. */
{
  curr_thread->cstack_size++;
  curr_thread->cstack = FACT_realloc (curr_thread->cstack, sizeof (struct cstack_t) * (curr_thread->cstack_size));
  curr_thread->cstack[curr_thread->cstack_size - 1].ip = nip;
  curr_thread->cstack[curr_thread->cstack_size - 1].this = nthis;
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
	  if (curr_thread->registers[R_TID].type != VAR_TYPE)
	    {
	      curr_thread->registers[R_TID].type = VAR_TYPE;
	      curr_thread->registers[R_TID].ap = FACT_alloc_var ();
	    }
	  mpc_set_ui (((FACT_var_t) curr_thread->registers[R_TID].ap)->value,
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
		      reg_number, des == VAR_TYPE ? "scope, expected var" : "var, expected scope");

  return reg->ap;
}

static void
math_call (char *inst, void (*mfunc)(mpc_t, mpc_t, mpc_t), int itype) /* Call a math function and check for errors. */
{
  FACT_var_t arg1, arg2, dest; /* For arithmetic instructions. */

  assert (itype >= 0 && itype <= 3);

  /* Get the arguments and the destination: */
  arg1 = (FACT_var_t) Furlow_reg_val (inst[1], VAR_TYPE);
  arg2 = (FACT_var_t) Furlow_reg_val (inst[2], VAR_TYPE);
  dest = (FACT_var_t) Furlow_reg_val (inst[3], VAR_TYPE);

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
  FACT_t res;            /* Generic type result.    */
  FACT_t *r1, *r2;       /* Generic type arguments. */
  FACT_var_t v1, v2, v3; /* Variable arguments.     */ 
  FACT_scope_t s1, s2;   /* Scope arguments.        */
  FACT_thread_t next;
  unsigned long tnum;
  
  curr_thread = threads; /* I'm not thinking this all the way through. */
  
 eval:
  /* Set the error handler. */
  if (setjmp (handle_err))
    {
      /* An error has been caught. If there are no available traps, jump to
       * recover. Otherwise, set the ip to the current trap handler.
       */
      if (curr_thread->num_traps == 0)
	longjmp (recover, 1);

      CURR_IP = curr_thread->traps[curr_thread->num_traps - 1];
      goto eval; /* Go back to eval to reset the error handler. */
    }

  /* FIXME */
  for (;; curr_thread++)
    {
      /* Run the scheduler to find the next live thread. */
      if (curr_thread - threads >= num_threads)
	curr_thread = threads;
      if (progm[CURR_IP][0] == HALT)
	{
	  /* The thread is dead, find a live one or exit. */
	  for (next = curr_thread + 1;; next++)
	    {
	      if (next - threads >= num_threads)
		next = threads;
	      if (next == curr_thread) /* There were no live threads. */
		{
		  curr_thread = threads;
		  return;
		}
	      if (progm[IP_OF (next)][0] != HALT)
		break;
	    }
	  curr_thread = next;
	}

      /* Execute the next instruction. 
       * Eventually I should put these in alphabetical order. Better yet,
       * just make a jump table. Giant switches are sort of unnecessary.
       * One way this may be structured is to use a bunch of goto labels,
       * and GNU C unary && operator.
       */
      switch (progm[CURR_IP][0])
	{
	case VAR:
	  FACT_get_either (progm[CURR_IP] + 1);
	  break;

	case DEF_V:
	  FACT_def_var (progm[CURR_IP] + 1, false);
	  break;

	case DEF_S:
	  FACT_def_scope (progm[CURR_IP] + 1, false);
	  break;

	case ELEM:
	  res = *Furlow_register (progm[CURR_IP][1]);
	  if (res.type == VAR_TYPE)
	    FACT_get_var_elem ((FACT_var_t) res.ap, progm[CURR_IP] + 2);
	  else
	    FACT_get_scope_elem ((FACT_scope_t) res.ap, progm[CURR_IP] + 2);
	  break;

	case REF:
	  r1 = Furlow_register (progm[CURR_IP][1]);
	  r2 = Furlow_register (progm[CURR_IP][2]);
	  r2->type = r1->type;
	  r2->ap = r1->ap;
	  break;

	case STO:
	  r2 = Furlow_register (progm[CURR_IP][1]);
	  r1 = Furlow_register (progm[CURR_IP][2]);
	  if (r1->type == VAR_TYPE)
	    {
	      if (r2->type == SCOPE_TYPE)
		FACT_throw_error (CURR_THIS, "cannot set a scope to a variable");
 	      mpc_set (((FACT_var_t) r1->ap)->value, ((FACT_var_t) r2->ap)->value);
	    }
	  else
	    {
	      if (r2->type == VAR_TYPE)
		FACT_throw_error (CURR_THIS, "cannot set a variable to a scope");
	      hold_name = ((FACT_scope_t) r1->ap)->name;
	      memcpy (r1->ap, r2->ap, sizeof (struct FACT_scope));
	      ((FACT_scope_t) r2->ap)->name = hold_name;
	    }
	  break;

	case SET_C:
	  s1 = Furlow_reg_val (progm[CURR_IP][1], SCOPE_TYPE);
	  *s1->code = get_seg_addr (progm[CURR_IP] + 2);
	  break;

	case SET_F:
	  s2 = Furlow_reg_val (progm[CURR_IP][1], SCOPE_TYPE);
	  s1 = Furlow_reg_val (progm[CURR_IP][2], SCOPE_TYPE);
	  s1->code = s2->code;
	  break;

	case USE:
	  s1 = Furlow_reg_val (progm[CURR_IP][1], SCOPE_TYPE);
	  push_c (CURR_IP, s1);
	  break;

	case CALL:
	  s1 = Furlow_reg_val (progm[CURR_IP][1], SCOPE_TYPE);
	  push_c (*s1->code - 1, s1);

	  /* Check if extrn_func is set, and if so call it. */
	  if (s1->extrn_func == NULL)
	    break;
	  s1->extrn_func ();
	  /* Continue to RET. */

	case RET:
	  pop_c ();
	  break;

	case THIS:
	  res.ap = pop_c ().this;
	  res.type = SCOPE_TYPE;
	  push_v (res);
	      
	case CONST:
	  /* Push a constant value to the stack. */
	  push_constant (progm[CURR_IP] + 1);
	  break;

	case INC:
	  /* Increment a register. */
	  v1 = Furlow_reg_val (progm[CURR_IP][1], VAR_TYPE);
	  mpc_add_ui (v1->value, v1->value, 1);
	  break;

	case DEC:
	  /* Decrement a register. */
	  v1 = Furlow_reg_val (progm[CURR_IP][1], VAR_TYPE);
	  mpc_sub_ui (v1->value, v1->value, 1);
	  break;

	case ADD:
	  math_call (progm[CURR_IP], &mpc_add, 0);
	  break;

	case SUB:
	  math_call (progm[CURR_IP], &mpc_sub, 0);
	  break;

	case MUL:
	  math_call (progm[CURR_IP], &mpc_mul, 0);
	  break;

	case DIV:
	  math_call (progm[CURR_IP], &mpc_div, 1);
	  break;

	case MOD:
	  math_call (progm[CURR_IP], &mpc_mod, 2);
	  break;

	case AND:
	  math_call (progm[CURR_IP], &mpc_and, 3);
	  break;

	case IOR:
	  math_call (progm[CURR_IP], &mpc_ior, 3);
	  break;
	  
	case XOR:
	  math_call (progm[CURR_IP], &mpc_xor, 3);
	  break;

	case CMT:
	  v1 = Furlow_reg_val (progm[CURR_IP][1], VAR_TYPE);
	  v2 = Furlow_reg_val (progm[CURR_IP][2], VAR_TYPE);
	  v3 = Furlow_reg_val (progm[CURR_IP][3], VAR_TYPE);
	  mpc_set_ui (v3->value, ((mpc_cmp (v1->value, v2->value) > 0)
				  ? 1
				  : 0));
	  break;
	  
	case CME:
	  v1 = Furlow_reg_val (progm[CURR_IP][1], VAR_TYPE);
	  v2 = Furlow_reg_val (progm[CURR_IP][2], VAR_TYPE);
	  v3 = Furlow_reg_val (progm[CURR_IP][3], VAR_TYPE);
	  mpc_set_ui (v3->value, ((mpc_cmp (v1->value, v2->value) >= 0)
				  ? 1
				  : 0));
	  break;
	 
	case CLT:
	  v1 = Furlow_reg_val (progm[CURR_IP][1], VAR_TYPE);
	  v2 = Furlow_reg_val (progm[CURR_IP][2], VAR_TYPE);
	  v3 = Furlow_reg_val (progm[CURR_IP][3], VAR_TYPE);
	  mpc_set_ui (v3->value, ((mpc_cmp (v1->value, v2->value) < 0)
				  ? 1
				  : 0));
	  break;

	case CLE:
	  v1 = Furlow_reg_val (progm[CURR_IP][1], VAR_TYPE);
	  v2 = Furlow_reg_val (progm[CURR_IP][2], VAR_TYPE);
	  v3 = Furlow_reg_val (progm[CURR_IP][3], VAR_TYPE);
	  mpc_set_ui (v3->value, ((mpc_cmp (v1->value, v2->value) <= 0)
				  ? 1
				  : 0));
	  break;
	  
	case CEQ:
	  v1 = Furlow_reg_val (progm[CURR_IP][1], VAR_TYPE);
	  v2 = Furlow_reg_val (progm[CURR_IP][2], VAR_TYPE);
	  v3 = Furlow_reg_val (progm[CURR_IP][3], VAR_TYPE);
	  mpc_set_ui (v3->value, ((mpc_cmp (v1->value, v2->value) == 0)
				  ? 1
				  : 0));
	  break;
	  
	case CNE:
	  v1 = Furlow_reg_val (progm[CURR_IP][1], VAR_TYPE);
	  v2 = Furlow_reg_val (progm[CURR_IP][2], VAR_TYPE);
	  v3 = Furlow_reg_val (progm[CURR_IP][3], VAR_TYPE);
	  mpc_set_ui (v3->value, ((mpc_cmp (v1->value, v2->value) != 0)
				  ? 1
				  : 0));
	  break;
	  
	case JMP:
	  /* Unconditional jump. */
	  CURR_IP = get_seg_addr (progm[CURR_IP] + 1) - 1;
	  break;

	case JIT:
	  /* Jump on true. */
	  res.ap = Furlow_reg_val (progm[CURR_IP][1], VAR_TYPE);
	  if (mpc_cmp_ui (((FACT_var_t) res.ap)->value, 0))
	    CURR_IP = get_seg_addr (progm[CURR_IP] + 2) - 1;
	  break;

	case JIF:
	  /* Jump on false. */
	  res.ap = Furlow_reg_val (progm[CURR_IP][1], VAR_TYPE);
	  if (!mpc_cmp_ui (((FACT_var_t) res.ap)->value, 0))
	    CURR_IP = get_seg_addr (progm[CURR_IP] + 2) - 1;
	  break;

	case SPRT:
	  /* Create a new thread and jump. */
	  /* Allocate and initialize the new thread. */
	  tnum = curr_thread - threads;
	  threads = FACT_realloc (threads, sizeof (struct FACT_thread) * ++num_threads);
	  curr_thread = threads + tnum;
	  threads[num_threads - 1].cstack_size++;
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
	  
	case NEW_V:
	  /* Create a new anonymous variable. */
	  FACT_def_var (progm[CURR_IP] + 1, true);
	  break;

	case NEW_S:
	  /* Create a new anonymous scope. */
	  FACT_def_scope (progm[CURR_IP] + 1, true);
	  break;
	  
	case NEW_L:
	  /* Do nothing. */
	  break;
	}

      /* Go to the next instruction. */
      CURR_IP++;
    }
}

static inline unsigned long
get_seg_addr (char *arg) /* Convert a segment address to a ulong. */
{
  unsigned long n;

  n = (unsigned char) arg[0];
  n = (n << 8) + (unsigned char) arg[1];
  n = (n << 8) + (unsigned char) arg[2];
  n = (n << 8) + (unsigned char) arg[3];

  return n;
}

static void
push_constant (char *cval) /* Push a constant to the var stack. */
{
  /* TODO: add caching. */
  FACT_t push_val;

  push_val.type = VAR_TYPE;
  push_val.ap = FACT_alloc_var ();

  /* Check for hexidecimal. */
  if (cval[0] == '0' && tolower (cval[1]) == 'x')
    mpc_set_str (((FACT_var_t) push_val.ap)->value, cval + 2, 16);
  else
    mpc_set_str (((FACT_var_t) push_val.ap)->value, cval, 10);

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
  threads->cstack = FACT_malloc (sizeof (struct cstack_t));
  CURR_THIS = FACT_alloc_scope ();
  CURR_IP = 0;

  for (i = 0; i < T_REGISTERS; i++)
    threads->registers[i].type = UNSET_TYPE;
}

void
FACT_get_either (char *name) /* Search all relevent scopes for a variable. */
{
  FACT_t res;

  /* Search for the variable. */
  res = get_either_noerror (CURR_THIS, name);

  /* If it doesn't exist, throw an error. Otherwise, push it to the var
   * stack.
   */
  if (res.ap == NULL)
    FACT_throw_error (CURR_THIS, "undefined variable: %s", name);

  push_v (res);
}

static FACT_t
get_either_noerror (FACT_scope_t curr, char *name) /* Search for a var, but don't throw an error. */
{
  FACT_t res;
  
  /* If the scope doesn't exist or is marked. */ 
  if (curr == NULL || *curr->marked)
    {
      res.ap = NULL;
      return res;
    }

  /* Mark the scope. */
  *curr->marked = true;

  /* Search for the variable. */
  res.ap = FACT_get_local_var (curr, name);
  if (res.ap != NULL)
    res.type = VAR_TYPE;
  else
    {
      res.ap = FACT_get_local_scope (curr, name);
      if (res.ap != NULL)
	res.type = SCOPE_TYPE;
      else
	/* If the var isn't local, search the next scope up. */
	res = get_either_noerror (FACT_get_local_scope (curr, "up"), name);
    }

  /* Unmark the scope. */
  *curr->marked = false;

  return res;
}
