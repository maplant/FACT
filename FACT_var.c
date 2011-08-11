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

static FACT_var_t *make_var_array (char *, unsigned long, unsigned long *, unsigned long);
static FACT_var_t get_element (FACT_var_t, unsigned long, unsigned long *, unsigned long);

FACT_var_t
FACT_get_local_var (FACT_scope_t curr, char *name) /* Search for a variable. */
{
  int res;
  FACT_var_t *low, *mid, *high;

  low = *curr->var_stack;
  high = low + *curr->var_stack_size;

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

  /* No variable was found. */
  return NULL;
}

FACT_var_t
FACT_add_var (FACT_scope_t curr, char *name) /* Add a variable to a scope. */
{
  unsigned long i;
  
  /* Check if the variable already exists. */
  if (FACT_get_local_var (curr, name) != NULL)
    /* It already exists, throw an error. */
    FACT_throw_error (curr, "local variable %s already exists; use \"del\" before redefining", name);

  if (FACT_get_local_scope (curr, name) != NULL)
    FACT_throw_error (curr, "local scope %s already exists; use \"del\" before redefining", name);

  /* Reallocate the var stack and add the variable. */
  *curr->var_stack = FACT_realloc (*curr->var_stack, sizeof (FACT_var_t **) * (*curr->var_stack_size + 1));
  (*curr->var_stack)[*curr->var_stack_size] = FACT_alloc_var ();
  (*curr->var_stack)[*curr->var_stack_size]->name = name;

  /* Move the var back to the correct place. */
  for (i = (*curr->var_stack_size)++; i > 0; i--)
    {
      if (strcmp ((*curr->var_stack)[i - 1]->name, name) > 0)
	{
	  /* Swap the elements. */
	  FACT_var_t hold;
	  
	  hold = (*curr->var_stack)[i - 1];
	  (*curr->var_stack)[i - 1] = (*curr->var_stack)[i];
	  (*curr->var_stack)[i] = hold;
	}
      else
	break;
    }

  return (*curr->var_stack)[i];
}

void
FACT_def_var (char *args, bool anonymous) /* Define a local or anonymous variable. */
{
  FACT_t push_val;
  unsigned long i;
  unsigned long *dim_sizes; /* Size of each dimension. */
  unsigned long dimensions; /* Number of dimensions.   */

  /* Get the number of dimensions. */
  dimensions = mpc_get_ui (((FACT_var_t) Furlow_reg_val (args[0], VAR_TYPE))->value);

  /* Add or allocate the variable. */
  push_val.ap = (anonymous
		 ? FACT_alloc_var () /* Perhaps add to a heap? */
		 : FACT_add_var (CURR_THIS, args + 1));
  push_val.type = VAR_TYPE;

  if (!dimensions)
    /* If the variable has no dimensions, simply push it and return. */
    goto end;

  /* Add the dimensions. */
  dim_sizes = NULL;
  
  /* Keep popping the stack until we have all the dimension sizes. */ 
  for (i = 0; i < dimensions; i++)
    {
      dim_sizes = FACT_realloc (dim_sizes, sizeof (long *) * (i + 1));
      /* Pop the stack and get the value. */
      dim_sizes[i] = mpc_get_ui (((FACT_var_t) Furlow_reg_val (R_POP, VAR_TYPE))->value);
      /* Todo: add some extra checks here before converting to an unsigned
       * long. Negative and float values will produce valid results, when 
       * they clearly should not.
       */
      if (dim_sizes[i] <= 1)
	{
	  FACT_free (dim_sizes);
	  FACT_throw_error (CURR_THIS, "dimension size must be larger than 1");
	}
    }

  /* Make the variable an array. */
  ((FACT_var_t) push_val.ap)->array_up = make_var_array (((FACT_var_t) push_val.ap)->name, dimensions, dim_sizes, 0);
  mpz_set_ui (((FACT_var_t) push_val.ap)->array_size, dim_sizes[0]);
  FACT_free (dim_sizes);

  /* Push the variable and return. */
 end:
  push_v (push_val);
}

void
FACT_get_var_elem (FACT_var_t base, char *args)
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
       * long. Negative and float values will produce results, when they
       * clearly should not.
       */
    }

  push_val.type = VAR_TYPE;
  push_val.ap = get_element (base, dimensions, elems, 0);

  FACT_free (elems);

  push_v (push_val);
}
  
static FACT_var_t *
make_var_array (char *name, unsigned long dims, unsigned long *dim_sizes, unsigned long curr_dim)
{
  FACT_var_t *root;
  unsigned long i;
  
  if (curr_dim >= dims)
    return NULL;

  root = FACT_alloc_var_array (dim_sizes[curr_dim]);

  for (i = 0; i < dim_sizes[curr_dim]; i++)
    {
      root[i]->name = name;
      root[i]->array_up = make_var_array (name, dims, dim_sizes, curr_dim + 1);
      /* Could be optimized not to check every single time. */
      if (root[i]->array_up != NULL)
	mpz_set_ui (root[i]->array_size, dim_sizes[curr_dim + 1]);
    }

  return root;
}

static FACT_var_t
get_element (FACT_var_t base, unsigned long dims, unsigned long *elems, unsigned long curr_dim)
{
  if (curr_dim >= dims)
    return base;

  if (mpz_cmp_ui (base->array_size, elems[curr_dim]) <= 0)
    {
      FACT_free (elems);
      FACT_throw_error (CURR_THIS, "out of bounds error; expected value in [0, %lu), but value is %lu",
			mpz_get_ui (base->array_size), elems[curr_dim]);
    }

  if (!mpz_cmp_ui (base->array_size, 1))
    {
      FACT_free (elems);
      if (curr_dim + 1 != dims)
	FACT_throw_error (CURR_THIS, "out of bounds error; unexpected dimensions");
      return base;
    }

  base = base->array_up[elems[curr_dim]];

  return get_element (base, dims, elems, curr_dim + 1);
}
      
