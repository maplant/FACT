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

static FACT_scope_t *make_scope_array (char *, size_t, size_t *, size_t);

FACT_scope_t
FACT_add_scope (FACT_scope_t curr, char *name) /* Add a local scope. */
{
  size_t i;
  char *hold_name;
  FACT_t *check;
  FACT_scope_t up;

  /* Check if the scope already exists. */
  check = FACT_get_local (curr, name);
  
  if (check != NULL) /* It already exists. */
    {
      if (check->type == SCOPE_TYPE)
	{	  
	  /* If it does, reset the scope. Do not free any data. */
	  FACT_scope_t temp;
	  temp = check->ap;

	  hold_name = temp->name;
	  memset (check->ap, 0, sizeof (struct FACT_scope));

	  /* Reallocate everything. */
	  temp->marked = FACT_malloc_atomic (sizeof (bool));
	  temp->array_size = FACT_malloc_atomic (sizeof (size_t));
	  temp->code = FACT_malloc_atomic (sizeof (size_t));
	  temp->var_table = FACT_malloc (sizeof (FACT_t *));
	  temp->num_vars = FACT_malloc_atomic (sizeof (size_t));
	  temp->array_up = FACT_malloc (sizeof (FACT_scope_t **));

	  temp->name = hold_name;
	  
	  /* Initialize the memory, if we need to. */
#ifndef USE_GC
	  *temp->array_size = 0;
	  *temp->code = 0;
	  *temp->marked = false;
	  *temp->var_table = NULL;
	  *temp->num_vars = 0;
	  temp->extrn_func = NULL;
	  temp->caller = NULL;
	  *temp->array_up = NULL;
	  temp->variadic = NULL;
#endif /* USE_GC */

	  /* Add the "up" scope here, unless we are already in the process of doing so. */
	  if (strcmp (name, "up"))
	    {
	      up = FACT_add_scope (temp, "up");
	      memcpy (up, curr, sizeof (struct FACT_scope));
	      up->name = "up";
	    }
	  else
	    curr->up = temp;

	  return temp;
	}
      else /* It's already a number. Through an error. */ 
	FACT_throw_error (curr, "local variable %s already exists; use \"del\" before redefining", name);
    }

  /* Reallocate the var table and add the variable. */
  *curr->var_table = FACT_realloc (*curr->var_table, sizeof (FACT_t) * (*curr->num_vars + 1));
  (*curr->var_table)[*curr->num_vars].ap = FACT_alloc_scope ();
  (*curr->var_table)[*curr->num_vars].type = SCOPE_TYPE;
  ((FACT_scope_t) (*curr->var_table)[*curr->num_vars].ap)->name = name;

  /* Move the var to its correct alphanumerical position. */
  for (i = (*curr->num_vars)++; i > 0; i--)
    {
      if (strcmp (FACT_var_name ((*curr->var_table)[i - 1]), name) > 0)
	{
	  /* Swap the elements. */
	  FACT_t hold;
	  
	  hold = (*curr->var_table)[i - 1];
	  (*curr->var_table)[i - 1] = (*curr->var_table)[i];
	  (*curr->var_table)[i] = hold;
	}
      else
	break;
    }

  /* Add the "up" scope here, unless we are already in the process of doing so. */
  if (strcmp (name, "up"))
    {
      up = FACT_add_scope ((*curr->var_table)[i].ap, "up");
      memcpy (up, curr, sizeof (struct FACT_scope));
      up->name = "up";     
    }
  else
    curr->up = FACT_cast_to_scope ((*curr->var_table)[i]);

  return (*curr->var_table)[i].ap;
}

void
FACT_def_scope (char *args, bool anonymous) /* Define a local or anonymous scope. */
{
  mpc_t elem_value;
  FACT_t push_val;
  size_t i;
  size_t *dim_sizes;
  size_t dimensions;

  dimensions = mpc_get_ui (((FACT_num_t) Furlow_reg_val (args[0], NUM_TYPE))->value);

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
      dim_sizes = FACT_realloc (dim_sizes, sizeof (size_t *) * (i + 1));
      elem_value[0] = ((FACT_num_t) Furlow_reg_val (R_POP, NUM_TYPE))->value[0];
      /* Check to make sure we aren't grossly out of range. */
      if (mpc_cmp_ui (elem_value, ULONG_MAX) > 0
	  || mpz_sgn (elem_value->value) < 0)
	FACT_throw_error (CURR_THIS, "out of bounds error"); 
      dim_sizes[i] = mpc_get_ui (elem_value);
      if (dim_sizes[i] == 0)
	{
	  FACT_free (dim_sizes);
	  FACT_throw_error (CURR_THIS, "dimension size must be larger than 0");
	}
    }

  /* Make the scope array. */
  *((FACT_scope_t) push_val.ap)->array_up = make_scope_array (((FACT_scope_t) push_val.ap)->name, dimensions, dim_sizes, 0);
  *((FACT_scope_t) push_val.ap)->array_size = dim_sizes[0];
  FACT_free (dim_sizes);

 end:
  push_v (push_val);
}

void
FACT_get_scope_elem (FACT_scope_t base, char *args)
{
  mpc_t elem_value;
  FACT_t push_val;
  size_t i;
  size_t *elems;     /* element of each dimension to access. */
  size_t dimensions; /* Number of dimensions.                */

  /* Get the element index. */
  elem_value[0] = *((FACT_num_t) Furlow_reg_val (args[0], NUM_TYPE))->value;

  /* Check to make sure we aren't out of bounds. */
  if (mpc_cmp_ui (elem_value, ULONG_MAX) > 0
      || mpz_sgn (elem_value->value) < 0
      || *base->array_size <= mpc_get_ui (elem_value))
    FACT_throw_error (CURR_THIS, "out of bounds error"); /* should elaborate here. */

  /* Get the element and push it to the stack. */
  push_val.ap = (*base->array_up)[mpc_get_ui (elem_value)];
  push_val.type = SCOPE_TYPE;

  push_v (push_val);
}


void
FACT_append_scope (FACT_scope_t op1, FACT_scope_t op2)
{
  size_t offset;
  
  /* Move the op1 to an array if it isn't one already. */
  if (*op1->array_size == 0)
    {
      *op1->array_size = 1;
      *op1->array_up = FACT_alloc_scope_array (1);
      memcpy ((*op1->array_up)[0], op1, sizeof (struct FACT_scope));
      (*op1->array_up)[0]->array_size = 0;
      (*op1->array_up)[0]->array_up = NULL;
    }

  offset = *op1->array_size;
  ++*op1->array_size;
  *op1->array_up = FACT_realloc (*op1->array_up, sizeof (FACT_scope_t) * *op1->array_size);

  (*op1->array_up)[offset] = FACT_alloc_scope ();
  memcpy ((*op1->array_up)[offset], op2, sizeof (struct FACT_scope));
}

static FACT_scope_t *
make_scope_array (char *name, size_t dims, size_t *dim_sizes, size_t curr_dim)
{
  FACT_scope_t *root, up;
  size_t i;

  if (curr_dim >= dims)
    return NULL;

  root = FACT_alloc_scope_array (dim_sizes[curr_dim]);

  for (i = 0; i < dim_sizes[curr_dim]; i++)
    {
      root[i]->name = name;
      *root[i]->array_up = make_scope_array (name, dims, dim_sizes, curr_dim + 1);
      /* This also could be optimized. */
      if (*root[i]->array_up != NULL)
	*(root[i]->array_size) = dim_sizes[curr_dim + 1];

      /* Add the up scope. */
      up = FACT_add_scope (root[i], "up");
      memcpy (up, CURR_THIS, sizeof (struct FACT_scope));
      root[i]->up = up;
    }

  return root;
}
