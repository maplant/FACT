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

/* I should come up with a nice macro system for this. */

static void *get_arg (FACT_type); 
static void FBIF_floor (void);
static void FBIF_print_n (void);
static void FBIF_putchar (void);
static void FBIF_size (void);
static void FBIF_error (void);
static void FBIF_throw (void);

#define GET_ARG_NUM() ((FACT_num_t) get_arg (NUM_TYPE))
#define GET_ARG_SCOPE() ((FACT_scope_t) get_arg (SCOPE_TYPE))

void
FACT_add_BIFs (FACT_scope_t curr) /* Add the built-in functions to a scope. */
{
  FACT_scope_t temp;

  /* Add each of the functions. */
  temp = FACT_add_scope (curr, "floor");
  temp->extrn_func = &FBIF_floor;
  temp = FACT_add_scope (curr, "print_n");
  temp->extrn_func = &FBIF_print_n;
  temp = FACT_add_scope (curr, "putchar");
  temp->extrn_func = &FBIF_putchar;
  temp = FACT_add_scope (curr, "size");
  temp->extrn_func = &FBIF_size;
  temp = FACT_add_scope (curr, "error");
  temp->extrn_func = &FBIF_error;
  temp = FACT_add_scope (curr, "throw");
  temp->extrn_func = &FBIF_throw;
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
