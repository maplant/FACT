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
#include "FACT_num.h"
#include "FACT_scope.h"
#include "FACT_types.h"
#include "FACT_hash.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define GC_THREADS
#include <gc/gc.h>
#include <assert.h>

#ifndef VALGRIND_DEBUG
inline void *FACT_malloc (size_t alloc_size)
{
  void *temp;

  temp = GC_malloc (alloc_size);

  /* Check for NULL pointer. */
  if (temp == NULL) {
    fprintf (stderr, "Failed to allocate block of size %zu, aborting.\n", alloc_size);
    abort ();
  }

  return temp;
}

inline void *FACT_malloc_atomic (size_t alloc_size)
{
  void *temp;

  temp = GC_malloc_atomic (alloc_size);

  /* Check for NULL pointer. */
  if (temp == NULL) {
    fprintf (stderr, "Failed to allocate block of size %zu, aborting.\n", alloc_size);
    abort ();
  }
  
  memset (temp, 0, alloc_size);
  return temp;
}

inline void *FACT_realloc (void *old, size_t new_size)
{
  void *temp;

  temp = GC_realloc (old, new_size);

  /* Check for NULL pointer. */
  if (temp == NULL) {
    fprintf (stderr, "Failed to reallocate block of size %zu, aborting.\n", new_size);
    abort ();
  }

  return temp;
}

inline void FACT_free (void *p)
{
  GC_free (p);
}
#else
# include "FACT_alloc.h"
#endif
 
FACT_num_t FACT_alloc_num (void) /* Allocate and initialize a num type. */
{
  FACT_num_t temp;

  temp = FACT_malloc (sizeof (struct FACT_num));
  mpc_init (temp->value);

  return temp;
}

FACT_num_t *FACT_alloc_num_array (size_t n)
{
  FACT_num_t *temp;

  assert (n != 0);
  temp = FACT_malloc (sizeof (FACT_num_t) * n); /* Allocate the nodes. */

  /* Initialize all the nodes. */
  do {
    n--;
    temp[n] = FACT_alloc_num ();
  } while (n > 0);

  return temp;
}

FACT_scope_t FACT_alloc_scope (void) /* Allocate and initialize a scope type. */
{
  FACT_scope_t temp;

  /* Allocate the memory. */
  temp = FACT_malloc (sizeof (struct FACT_scope));
  temp->marked = FACT_malloc_atomic (sizeof (bool));
  temp->array_size = FACT_malloc_atomic (sizeof (size_t));
  temp->code = FACT_malloc_atomic (sizeof (size_t));
  temp->vars = FACT_malloc (sizeof (FACT_table_t ));
  temp->array_up = FACT_malloc (sizeof (FACT_scope_t **));
  temp->name = "lambda";
  temp->lock_stat = UNLOCKED;
  
  return temp;
}

FACT_scope_t *FACT_alloc_scope_array (size_t n)
{
  FACT_scope_t *temp;

  assert (n != 0);
  temp = FACT_malloc (sizeof (FACT_scope_t) * n); /* Allocate the nodes. */

  /* Initialize all the nodes. */
  do {
    n--;
    temp[n] = FACT_alloc_scope ();
  } while (n > 0);

  return temp;
}
