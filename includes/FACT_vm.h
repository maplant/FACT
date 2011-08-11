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

#ifndef FACT_VM_H_
#define FACT_VM_H_

/* Register specifications:                                   */
#define T_REGISTERS 256 /* Total number of registers (G + S). */
#define G_REGISTERS 253 /* General registers.                 */
#define S_REGISTERS 3   /* Special registers.                 */

/* Definitions of special register values. The index registers (R_I, R_J,
 * etc) are not technically special, but are defined here.
 */
enum
  {
    R_POP = 0, /* Pop register.          */
    R_TOP = 1, /* Top of stack register. */
    R_TID = 2, /* Thread ID register.    */
    R_I,       /* "i" register.          */
    R_J,       /* "j" register.          */
    R_K,       /* "k" register.          */
  };

struct cstack_t 
{
  unsigned long ip;  /* Instruction pointer being used. */
  FACT_scope_t this; /* 'this' scope being used.        */
};

/* Threading is handled on the program level in the Furlow VM, for the most
 * part. Each thread has its own stacks and instruction pointer. After an
 * instruction is evaluated, the next thread's data is used.
 */
typedef struct FACT_thread
{
  /* Virtual machine stacks:                                */
  FACT_t *vstack;            /* Variable stack.             */
  struct cstack_t *cstack;   /* Call stack.                 */ 
  unsigned long vstack_size; /* Size of the variable stack. */
  unsigned long cstack_size; /* Size of the call stack.     */

  /* User traps:                                                         */
  unsigned long *traps;    /* Trap stack. Delegates user error handling. */
  unsigned long num_traps; /* Number of traps set.                       */ 
  FACT_error_t curr_err;   /* The last error thrown.                     */

  /* Virtual machine registers:                                 */
  FACT_t registers[T_REGISTERS]; /* NOT to be handled directly. */

  /* TODO: Add a message queue for ITC. */
} *FACT_thread_t;

/* Threading and stacks:                                */
extern FACT_thread_t threads;     /* Thread data.       */
extern FACT_thread_t curr_thread; /* Thread being run.  */
extern unsigned long num_threads; /* Number of threads. */

/* Error recovery:                                   */
extern jmp_buf handle_err; /* Handles thrown errors. */ 
extern jmp_buf recover;    /* When no traps are set. */

#define THIS_OF(t) (t)->cstack[(t)->cstack_size - 1].this
#define IP_OF(t)   (t)->cstack[(t)->cstack_size - 1].ip
#define CURR_THIS  curr_thread->cstack[curr_thread->cstack_size - 1].this
#define CURR_IP    curr_thread->cstack[curr_thread->cstack_size - 1].ip

/* Stack functions:                                                   */
FACT_t pop_v (void);                       /* Pop the var stack.      */
struct cstack_t pop_c (void);              /* Pop the call stack.     */
void push_v (FACT_t);                      /* Push to the var stack.  */
void push_c (unsigned long, FACT_scope_t); /* Push to the call stack. */

/* Register functions:                                              */
FACT_t *Furlow_register (int);         /* Access a register.        */
void *Furlow_reg_val (int, FACT_type); /* Safely access a register. */

/* Execution functions:                                      */
void Furlow_run (); /* Run one cycle of the current program. */ 

/* Init functions:                                         */
void Furlow_init_vm (); /* Initialize the virtual machine. */

/* Code handling functions:                                                 */
void Furlow_add_instruction (char *); /* Add an instruction to the program. */
void Furlow_add_instructions (char **, unsigned long);

/* Scope handling:                                                      */
void FACT_get_either (char *); /* Search for a variable of either type. */

#endif /* FACT_VM_H_ */
