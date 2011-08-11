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

static FACT_scope_t *make_scope_array (char *, unsigned long, unsigned long *, unsigned long);
static FACT_scope_t get_element (FACT_scope_t, unsigned long, unsigned long *, unsigned long);

FACT_scope_t
FACT_get_local_scope (FACT_scope_t curr, char *name) /* Search for a local scope. */
{
  int res;
  FACT_scope_t *low, *mid, *high;

  low = *curr->scope_stack;
  high = low + *curr->scope_stack_size;

  while (low < high)
    {
      mid = low + (high - low) / 2;
      res = strcmp (name, (*mid)->name);

      if (res < 0)
	high = mid;
      else if (res > 0)
	low = mid + 1;
      else
	return *mid;
    }

  /* No scope was found. */
  return NULL;
}

FACT_scope_t
FACT_add_scope (FACT_scope_t curr, char *name) /* Add a local scope. */
{
  unsigned long i;
  FACT_scope_t up;

  /* Check if the scope already exists. */
  if (FACT_get_local_scope (curr, name) != NULL)
    /* If it does, throw an error. */
    FACT_throw_error (curr, "local scope %s already exists; use \"del\" before redefining", name);

  if (FACT_get_local_var (curr, name) != NULL)
    FACT_throw_error (curr, "local variable %s already exists; use \"del\" before redefining", name);

  /* Reallocate the scope stack and add the variable. */
  *curr->scope_stack = FACT_realloc (*curr->scope_stack, sizeof (FACT_scope_t **) * (*curr->scope_stack_size + 1));
  (*curr->scope_stack)[*curr->scope_stack_size] = FACT_alloc_scope ();
  (*curr->scope_stack)[*curr->scope_stack_size]->name = name;

  /* Move the var back to the correct place. */
  for (i = (*curr->scope_stack_size)++; i > 0; i--)
    {
      if (strcmp ((*curr->scope_stack)[i - 1]->name, name) > 0)
	{
	  /* Swap the elements. */
	  FACT_scope_t hold;
	  
	  hold = (*curr->scope_stack)[i - 1];
	  (*curr->scope_stack)[i - 1] = (*curr->scope_stack)[i];
	  (*curr->scope_stack)[i] = hold;
	}
      else
	break;
    }

  /* Add the "up" scope here, unless we are already in the process of doing so. */
  if (strcmp (name, "up"))
    {
      up = FACT_add_scope ((*curr->scope_stack)[i], "up");
      memcpy (up, curr, sizeof (struct FACT_scope));
      up->name = "up";
    }

  return (*curr->scope_stack)[i];
}

void
FACT_def_scope (char *args, bool anonymous) /* Define a local or anonymous scope. */
{
  FACT_t push_val;
  unsigned long i;
  unsigned long *dim_sizes;
  unsigned long dimensions;

  dimensions = mpc_get_ui (((FACT_var_t) Furlow_reg_val (args[0], VAR_TYPE))->value);

  /* Add the local scope or anonymous. */
  push_val.ap = (anonymous
		 ? FACT_alloc_scope ()
		 : FACT_add_scope (CURR_THIS, args + 1));
  push_val.type = SCOPE_TYPE;

  if (!dimensions)
    goto end;

  dim_sizes = NULL;
  for (i = 0; i < dimensions; i++)
    {
      dim_sizes = FACT_realloc (dim_sizes, sizeof (long *) * (i + 1));
      dim_sizes[i] = mpc_get_ui (((FACT_var_t) Furlow_reg_val (R_POP, VAR_TYPE))->value);
      /* Todo: as in FACT_def_var, extra checks need to be added here. */
      if (dim_sizes[i] <= 1)
	{
	  FACT_free (dim_sizes);
	  FACT_throw_error (CURR_THIS, "dimension size must be larger than 1");
	}
    }

  /* Make the scope array. */
  *((FACT_scope_t) push_val.ap)->array_up = make_scope_array (((FACT_scope_t) push_val.ap)->name, dimensions, dim_sizes, 0);
  mpz_set_ui (*((FACT_scope_t) push_val.ap)->array_size, dim_sizes[0]);
  FACT_free (dim_sizes);

 end:
  push_v (push_val);
}

void
FACT_get_scope_elem (FACT_scope_t base, char *args)
{
  FACT_t push_val;
  unsigned long i;
  unsigned long *elems;     /* element of each dimension to access. */
  unsigned long dimensions; /* Number of dimensions.                */

  /* Get the number of dimensions. */
  dimensions = mpc_get_ui (((FACT_var_t) Furlow_reg_val (args[0], VAR_TYPE))->value);
  
  /* Add the dimensions. */
  elems = NULL;
  
  /* Keep popping the stack until we have all the dimension sizes. */ 
  for (i = 0; i < dimensions; i++)
    {
      elems = FACT_realloc (elems, sizeof (long *) * (i + 1));
      /* Pop the stack and get the value. */
      elems[i] = mpc_get_ui (((FACT_var_t) Furlow_reg_val (R_POP, VAR_TYPE))->value);
      /* Todo: add some extra checks here before converting to an unsigned
       * long. Negative and float values will produce valid results, when
       * they clearly should not.
       */
    }

  push_val.type = SCOPE_TYPE;
  push_val.ap = get_element (base, dimensions, elems, 0);

  FACT_free (elems);

  push_v (push_val);
}

static FACT_scope_t *
make_scope_array (char *name, unsigned long dims, unsigned long *dim_sizes, unsigned long curr_dim)
{
  FACT_scope_t *root, up;
  unsigned long i;

  if (curr_dim >= dims)
    return NULL;

  root = FACT_alloc_scope_array (dim_sizes[curr_dim]);

  for (i = 0; i < dim_sizes[curr_dim]; i++)
    {
      root[i]->name = name;
      *root[i]->array_up = make_scope_array (name, dims, dim_sizes, curr_dim + 1);
      /* This also could be optimized. */
      if (*root[i]->array_up != NULL)
	mpz_set_ui (*(root[i]->array_size), dim_sizes[curr_dim + 1]);

      /* Add the up scope. */
      up = FACT_add_scope (root[i], "up");
      memcpy (up, CURR_THIS, sizeof (struct FACT_scope));
    }

  return root;
}

static FACT_scope_t
get_element (FACT_scope_t base, unsigned long dims, unsigned long *elems, unsigned long curr_dim)
{
  if (curr_dim >= dims)
    return base;

  if (mpz_cmp_ui (*base->array_size, elems[curr_dim]) <= 0)
    {
      FACT_free (elems);
      FACT_throw_error (CURR_THIS, "out of bounds error; expected value in [0, %lu), but value is %lu",
			mpz_get_ui (*base->array_size), elems[curr_dim]);
    }

  if (!mpz_cmp_ui (*base->array_size, 1))
    {
      FACT_free (elems);
      if (curr_dim + 1 != dims)
	FACT_throw_error (CURR_THIS, "out of bounds error; unexpected dimensions");
      return base;
    }

  base = *base->array_up[elems[curr_dim]];

  return get_element (base, dims, elems, curr_dim + 1);
}
