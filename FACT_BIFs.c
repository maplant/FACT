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
#include "FACT_mpc.h"
#include "FACT_types.h"
#include "FACT_vm.h"
#include "FACT_error.h"
#include "FACT_alloc.h"
#include "FACT_hash.h"
#include "FACT_threads.h"
#include "FACT_strs.h"

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

/* Macros for declaring FACT BIFs. */
#define FBIF(name) { #name, &FBIF_##name }
#define FBIF_DEC(name) static void FBIF_##name (void)

static void *get_arg (FACT_type);

FBIF_DEC (floor);
FBIF_DEC (print);
FBIF_DEC (str);
FBIF_DEC (size);
FBIF_DEC (error);
FBIF_DEC (throw);
FBIF_DEC (send);
FBIF_DEC (receive);
FBIF_DEC (parcels);
FBIF_DEC (exit);
FBIF_DEC (load);
FBIF_DEC (ID);

static const struct {
  char *name;
  void (*phys)(void);
} BIF_list[] = {
  FBIF (floor),
  FBIF (print),
  FBIF (str),
  FBIF (size),
  FBIF (error),
  FBIF (throw),
  FBIF (send),
  FBIF (receive),
  FBIF (ID),
  FBIF (exit),
  FBIF (load),
};

#define NUM_FBIF ((sizeof BIF_list) / (sizeof BIF_list[0]))

#define GET_ARG_NUM() ((FACT_num_t) get_arg (NUM_TYPE))
#define GET_ARG_SCOPE() ((FACT_scope_t) get_arg (SCOPE_TYPE))

void FACT_add_BIFs (void) /* Add the built-in functions to the global table. */
{
  int i;
  FACT_t tvar;
  FACT_scope_t temp;

  tvar.type = SCOPE_TYPE;
  
  /* Add each of the functions. */
  for (i = 0; i < NUM_FBIF; i++) {
    temp = FACT_alloc_scope ();
    temp->name = BIF_list[i].name;
    temp->lock_stat = HARD_LOCK;
    temp->extrn_func = BIF_list[i].phys;
    tvar.ap = temp;
    FACT_add_to_table (&Furlow_globals, tvar);
  }

  /* Well that was pretty easy. */
}

bool FACT_is_BIF (void *func_addr)
{
  int i;

  for (i = 0; i < NUM_FBIF; i++) {
    if (BIF_list[i].phys == func_addr)
      return true;
  }
  return false;
}

static void FBIF_floor (void) /* Round a variable down. */
{
  FACT_t push_val;
  FACT_num_t res, arg;

  arg = GET_ARG_NUM ();

  if (arg->value->fp) {
    res = FACT_alloc_num ();
    mpz_set_f (res->value->intv, arg->value->fltv);
    push_val.ap = res;
  } else
    push_val.ap = arg;
  
  push_val.type = NUM_TYPE;
  push_v (push_val);
}

static void FBIF_print (void) /* Print an ASCII string or numerical value. */
{
  int len;
  FACT_num_t arg;
  //  static pthread_mutex_t print_lock = PTHREAD_MUTEX_INITIALIZER;

  arg = GET_ARG_NUM();
  // pthread_mutex_lock(&print_lock);
  /* If the argument is an array, print it as a string. Otherwise, print the
   * numerical value.
   */
  if (arg->array_size == 0) /* Print a newline, as it is a number */
    len = printf("%s\n", mpc_get_str(arg->value)) - 1;
  else
    len = printf("%s", FACT_natos(arg));
  fflush(stdout);
  // pthread_mutex_unlock(&print_lock);
  push_constant_ui(len);
}

static void FBIF_str (void) /* Convert a number to a string. */
{
  FACT_t push_val;

  push_val.type = NUM_TYPE;
  push_val.ap = FACT_stona (mpc_get_str (GET_ARG_NUM ()->value)); 
  push_v (push_val);
}

static void FBIF_size (void) /* Return the size of an array. */
{
  FACT_t push_val;
  FACT_num_t res;

  res = FACT_alloc_num ();
  mpc_set_ui (res->value, GET_ARG_NUM ()->array_size);

  push_val.type = NUM_TYPE;
  push_val.ap = res;

  push_v (push_val);
}

static void FBIF_error (void) /* Return the current error message. */
{
  FACT_t push_val;

  push_val.type = NUM_TYPE;
  push_val.ap = FACT_stona ((char *) curr_thread->curr_err.what);
  push_v (push_val);
}

static void FBIF_throw (void) /* Throw an error. */
{
  FACT_num_t msg;

  msg = GET_ARG_NUM ();
  FACT_throw_error (CURR_THIS->caller, FACT_natos (msg)); 
}

static void FBIF_send (void) /* Send a message to a thread. */
{
  FACT_num_t dest, msg;

  //  printf("entered\n");
  msg = GET_ARG_NUM ();
  dest = GET_ARG_NUM ();

  FACT_send_message (msg, mpc_get_ui (dest->value));
  push_constant_ui (0);
}

static void FBIF_receive (void) /* Pop the current thread's message queue. */
{
  FACT_t res;

  res.type = SCOPE_TYPE;
  res.ap = FACT_get_next_message ();

  push_v (res);
}

static void FBIF_ID(void)
{
  push_constant_ui(curr_thread->thread_num);
}

static void FBIF_exit (void) /* Exit. */
{
  exit (mpc_get_si (GET_ARG_NUM ()->value));
}

static void FBIF_load (void) /* Run a file in the current scope. */
{
  FACT_load_file (FACT_natos (GET_ARG_NUM ()));
  push_constant_ui (0);
}

static void *get_arg (FACT_type type_of_arg) /* Get an argument. */
{
  FACT_t pop_res;

  pop_res = pop_v ();
  /* Throw an error if argument types do not match. */
  if (type_of_arg != pop_res.type)
    FACT_throw_error (CURR_THIS, "argument types do not match");

  return pop_res.ap;
}
