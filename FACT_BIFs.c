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

/* Macros for declaring FACT BIFs. */
#define FBIF(name) { #name, &FBIF_##name }
#define FBIF_DEC(name) static void FBIF_##name (void)

static void *get_arg (FACT_type);

FBIF_DEC (floor);
FBIF_DEC (print_n);
FBIF_DEC (putchar);
FBIF_DEC (size);
FBIF_DEC (error);
FBIF_DEC (throw);
FBIF_DEC (send);
FBIF_DEC (recieve);
FBIF_DEC (parcels);

static const struct
{
  char *name;
  void (*phys)(void);
} BIF_list[] =
  {
    FBIF (floor),
    FBIF (print_n),
    FBIF (putchar),
    FBIF (size),
    FBIF (error),
    FBIF (throw),
    FBIF (send),
    FBIF (recieve),
    FBIF (parcels),
  };

#define NUM_FBIF ((sizeof BIF_list) / (sizeof BIF_list[0]))

#define GET_ARG_NUM() ((FACT_num_t) get_arg (NUM_TYPE))
#define GET_ARG_SCOPE() ((FACT_scope_t) get_arg (SCOPE_TYPE))

void
FACT_add_BIFs (FACT_scope_t curr) /* Add the built-in functions to a scope. */
{
  int i;
  FACT_scope_t temp;

  /* Add each of the functions. */
  for (i = 0; i < NUM_FBIF; i++)
    {
      temp = FACT_add_scope (curr, BIF_list[i].name);
      temp->extrn_func = BIF_list[i].phys;
    }

  /* Well that was pretty easy. */
}

static void
FBIF_putchar (void)
{
  putchar (mpc_get_si (GET_ARG_NUM ()->value));
  push_constant_ui (0);
}

static void
FBIF_floor (void) /* Round a variable down. */
{
  FACT_t push_val;
  FACT_num_t res;

  res = FACT_alloc_num ();
  /* Get the argument. */
  mpc_set (res->value, GET_ARG_NUM ()->value);

  while (res->value->precision > 0)
    {
      mpz_div_ui (res->value->value, res->value->value, 10);
      res->value->precision--;
    }

  push_val.type = NUM_TYPE;
  push_val.ap = res;

  push_v (push_val);
}

static void
FBIF_print_n (void) /* Print a number. */
{
  printf ("%s\n", mpc_get_str (GET_ARG_NUM ()->value));
  push_constant_ui (0);
}

static void
FBIF_size (void) /* Return the size of an array. */
{
  FACT_t push_val;
  FACT_num_t res;

  res = FACT_alloc_num ();
  mpc_set_ui (res->value, GET_ARG_NUM ()->array_size);

  push_val.type = NUM_TYPE;
  push_val.ap = res;

  push_v (push_val);
}

static void
FBIF_error (void) /* Return the current error message. */
{
  FACT_t push_val;

  push_val.type = NUM_TYPE;
  push_val.ap = FACT_stona ((char *) curr_thread->curr_err.what);
  push_v (push_val);
}

static void
FBIF_throw (void) /* Throw an error. */
{
  FACT_num_t msg;

  msg = GET_ARG_NUM ();
  FACT_throw_error (CURR_THIS, FACT_natos (msg)); /* Should probably use "caller" instead. */
}

static void
FBIF_send (void) /* Send a message to a thread. */
{
  FACT_num_t dest, msg;

  msg = GET_ARG_NUM ();
  dest = GET_ARG_NUM ();

  FACT_send_message (msg, mpc_get_ui (dest->value));
  push_constant_ui (0);
}

static void
FBIF_recieve (void) /* Pop the current thread's message queue. */
{
  FACT_t res;

  res.type = SCOPE_TYPE;
  res.ap = FACT_get_next_message ();

  push_v (res);
}

static void
FBIF_parcels (void) /* Get the number of items in the message queue. */
{
  pthread_mutex_lock (&curr_thread->queue_lock);
  push_constant_ui (curr_thread->num_messages);
  pthread_mutex_unlock (&curr_thread->queue_lock);
}

static void *
get_arg (FACT_type type_of_arg) /* Get an argument. */
{
  FACT_t pop_res;

  pop_res = pop_v ();
  /* Throw an error if argument types do not match. */
  if (type_of_arg != pop_res.type)
    FACT_throw_error (CURR_THIS, "argument types do not match");

  return pop_res.ap;
}
