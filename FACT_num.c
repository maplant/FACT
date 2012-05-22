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
#include "FACT_hash.h"
#include "FACT_error.h"
#include "FACT_types.h"
#include "FACT_alloc.h"

#include <string.h>

static FACT_num_t *make_num_array (char *, size_t, size_t *, size_t);
static FACT_num_t copy_num (FACT_num_t);
static void free_num (FACT_num_t);

FACT_num_t FACT_add_num (FACT_scope_t curr, char *name) /* Add a number variable to a scope. */
{
  size_t i;
  FACT_t new;
  FACT_t *check;

  if (curr->lock_stat != UNLOCKED)
    FACT_throw_error (curr, "variables may not be defined in a locked scope");

  /* Check if the variable already exists. */
  check = FACT_find_in_table (curr->vars, name);
  
  if (check != NULL) { /* The variable already exists. */
    if (check->type == NUM_TYPE) {
      /* It already exists as a number, clear and return it. */
      FACT_num_t temp;

      temp = check->ap;
      if (temp->locked)
	FACT_throw_error (curr, "num %s already exists and is locked", name);

      mpc_set_ui (temp->value, 0);

      for (i = 0; i < temp->array_size; i++)
	free_num (temp->array_up[i]);
      
      FACT_free (temp->array_up);
      temp->array_up = NULL;
      temp->array_size = 0;
      return temp;
    } else /* If it's already a scope, however, just throw an error. */
      FACT_throw_error (curr, "local scope %s already exists", name);
  }

  new.ap = FACT_alloc_num ();
  new.type = NUM_TYPE;
  ((FACT_num_t) new.ap)->name = name;
  FACT_add_to_table (curr->vars, new);
  return new.ap;  
}

void FACT_def_num (char *args, bool anonymous) /* Define a local or anonymous number variable. */
{
  mpc_t elem_value;
  FACT_t push_val;
  size_t i;
  size_t *dim_sizes; /* Size of each dimension. */
  size_t dimensions; /* Number of dimensions.   */

  /* Get the number of dimensions. TODO: add checking here. */
  dimensions = mpc_get_ui (((FACT_num_t) Furlow_reg_val (args[0], NUM_TYPE))->value);

  /* Add or allocate the variable. */
  push_val.ap = (anonymous
		 ? FACT_alloc_num () /* Perhaps add to a heap? */
		 : FACT_add_num (CURR_THIS, args + 1));
  push_val.type = NUM_TYPE;

  if (!dimensions)
    /* If the variable has no dimensions, simply push it and return. */
    goto end;

  /* Add the dimensions. */
  dim_sizes = NULL;
  
  /* Keep popping the stack until we have all the dimension sizes. */ 
  for (i = 0; i < dimensions; i++) {
    dim_sizes = FACT_realloc (dim_sizes, sizeof (size_t *) * (i + 1));
    /* Pop the stack and get the value. */
    elem_value[0] = ((FACT_num_t) Furlow_reg_val (R_POP, NUM_TYPE))->value[0];
    if (mpc_is_float (elem_value))
      FACT_throw_error (CURR_THIS, "dimension size must be a positive integer");
    /* Check to make sure we aren't grossly out of range. */
    if (mpc_cmp_ui (elem_value, ULONG_MAX) > 0 ||
	mpz_sgn (elem_value->intv) < 0)
      FACT_throw_error (CURR_THIS, "out of bounds error"); 
    dim_sizes[i] = mpc_get_ui (elem_value);
    if (dim_sizes[i] == 0) {
      FACT_free (dim_sizes);
      FACT_throw_error (CURR_THIS, "dimension size must be larger than 0");
    }
  }
  
  /* Make the variable an array. */
  ((FACT_num_t) push_val.ap)->array_up = make_num_array (((FACT_num_t) push_val.ap)->name, dimensions, dim_sizes, 0);
  ((FACT_num_t) push_val.ap)->array_size = dim_sizes[0];
  FACT_free (dim_sizes);

  /* Push the variable and return. */
 end:
  push_v (push_val);
}

void FACT_get_num_elem (FACT_num_t base, char *args)
{
  mpc_t elem_value;
  FACT_t push_val;

  /* Get the element index. */
  elem_value[0] = *((FACT_num_t) Furlow_reg_val (args[0], NUM_TYPE))->value;

  if (mpc_is_float (elem_value))
    FACT_throw_error (CURR_THIS, "index value must be a positive integer");
  
  /* Check to make sure we aren't out of bounds. */
  if (mpc_cmp_ui (elem_value, ULONG_MAX) > 0
      || mpz_sgn (elem_value->intv) < 0
      || base->array_size <= mpc_get_ui (elem_value))
    FACT_throw_error (CURR_THIS, "out of bounds error"); /* should elaborate here. */

  /* Get the element and push it to the stack. */
  push_val.ap = base->array_up[mpc_get_ui (elem_value)];
  push_val.type = NUM_TYPE;

  push_v (push_val);
}

void FACT_set_num (FACT_num_t rop, FACT_num_t op)
{
  size_t i;

  /* Free rop. */
  for (i = 0; i < rop->array_size; i++)
    free_num (rop->array_up[i]);

  mpc_set (rop->value, op->value);
  rop->array_size = op->array_size;

  if (rop->array_size)
    rop->array_up = FACT_realloc (rop->array_up, sizeof (FACT_num_t) * op->array_size);
  else {
    FACT_free (rop->array_up);
    rop->array_up = NULL;
    return; /* Nothing left to do here. */
  }
  
  for (i = 0; i < rop->array_size; i++)
    rop->array_up[i] = copy_num (op->array_up[i]);
}

int FACT_compare_num (FACT_num_t op1, FACT_num_t op2) /* Return -1 if op1 is < op2, 0 if they are equal, and 1 if op1 is greater. */
{
  size_t i, min_size;
  int res;
  
  if (op1->array_size == 0) {
    if (op2->array_size != 0)
      return -1;
    
    res = mpc_cmp (op1->value, op2->value);
    return res;
  } else {
    if (op2->array_size == 0)
      return 1;
    
    min_size = (op1->array_size > op2->array_size ? op2->array_size : op1->array_size);
    for (i = 0; i < min_size; i++) {
      res = FACT_compare_num (op1->array_up[i], op2->array_up[i]);
      if (res != 0)
	return res;
    }
    
    /* Now just go by whichever is the bigger array, or return 0. */
    return ((op1->array_size > op2->array_size)
	    ? 1
	    : ((op1->array_size < op2->array_size)
	       ? -1
	       : 0));
    /*
    if (op1->array_size > op2->array_size)
      return 1;
    else if (op1->array_size < op2->array_size)
      return -1;
    else
      return 0;
    */
  }
}     

void FACT_append_num (FACT_num_t op1, FACT_num_t op2)
{
  size_t offset;
  
  /* Move the op1 to an array if it isn't one already. */
  if (op1->array_size == 0) {
    op1->array_size = 1;
    op1->array_up = FACT_alloc_num_array (1);
    mpc_set (op1->array_up[0]->value, op1->value);
    mpc_set_ui (op1->value, 0);
  }
  
  offset = op1->array_size;
  op1->array_size++;
  op1->array_up = FACT_realloc (op1->array_up, sizeof (FACT_num_t) * op1->array_size);

  op1->array_up[offset] = FACT_alloc_num ();
  FACT_set_num (op1->array_up[offset], op2);

  /*
   * Todo: clone this function and add the following:
   *
  if (op2->array_size == 0)
    {
      op1->array_up[offset] = FACT_alloc_num ();
      mpc_set (op1->array_up[offset]->value, op2->value);
    }
  else
    {
      for (i = 0; i < op2->array_size; i++)
	{
	  op1->array_up[offset + i] = FACT_alloc_num ();
	  FACT_set_num (op1->array_up[offset + i], op2->array_up[i]);
	}
    }
  */
}

void FACT_lock_num (FACT_num_t root)
{
  size_t i;

  root->locked = true;
  for (i = 0; i < root->array_size; i++)
    FACT_lock_num (root->array_up[i]);
}

static FACT_num_t *make_num_array (char *name, size_t dims, size_t *dim_sizes, size_t curr_dim)
{
  size_t i;
  FACT_num_t *root;
  
  if (curr_dim >= dims)
    return NULL;

  root = FACT_alloc_num_array (dim_sizes[curr_dim]);

  for (i = 0; i < dim_sizes[curr_dim]; i++) {
    root[i]->name = name;
    root[i]->array_up = make_num_array (name, dims, dim_sizes, curr_dim + 1);
    /* Could be optimized not to check every single time. */
    if (root[i]->array_up != NULL)
      root[i]->array_size = dim_sizes[curr_dim + 1];
  }
  
  return root;
}
      
static FACT_num_t copy_num (FACT_num_t root) /* Copy a number array recursively. */
{
  size_t i;
  FACT_num_t res;

  if (root == NULL)
    return NULL;
  
  res = FACT_alloc_num ();
  res->array_size = root->array_size;
  res->array_up = ((root->array_up != NULL)
		   ? FACT_malloc (sizeof (FACT_num_t) * res->array_size)
		   : NULL);
  mpc_set (res->value, root->value);

  for (i = 0; i < root->array_size; i++)
    res->array_up[i] = copy_num (root->array_up[i]);

  return res;
}

static void free_num (FACT_num_t root) /* Free a number array recursively. */
{
  size_t i;

  if (root == NULL)
    return;

  for (i = 0; i < root->array_size; i++)
    free_num (root->array_up[i]);

  mpc_clear (root->value);
  FACT_free (root->array_up);
  FACT_free (root);
}
