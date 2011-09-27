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

inline void *
FACT_malloc (size_t alloc_size)
{
  void *temp;

#ifdef USE_GC
  temp = GC_malloc (alloc_size);
#else
  temp = malloc (alloc_size);
#endif /* USE_GC */

  /* Check for NULL pointer. */
  if (temp == NULL)
    {
      fprintf (stderr, "Failed to allocate block of size %d, aborting.\n", alloc_size);
      abort ();
    }

  return temp;
}

inline void *
FACT_realloc (void *old, size_t new_size)
{
  void *temp;

#ifdef USE_GC
  temp = GC_realloc (old, new_size);
#else
  temp = realloc (old, new_size);
#endif /* USE_GC */

  /* Check for NULL pointer. */
  if (temp == NULL)
    {
      fprintf (stderr, "Failed to reallocate block of size %d, aborting.\n", new_size);
      abort ();
    }

  return temp;
}

inline void
FACT_free (void *p)
{
#ifdef USE_GC
  GC_free (p);
#else
  free (p);
#endif /* USE_GC */
}
 
FACT_num_t
FACT_alloc_num (void) /* Allocate and initialize a num type. */
{
  FACT_num_t temp;

  temp = FACT_malloc (sizeof (struct FACT_num));
#ifndef USE_GC
  /* GC automatically sets the data it allocates to 0, so we only have to
   * memset if we aren't using it.
   */
  memset (temp, 0, sizeof (struct FACT_num));
#endif /* USE_GC */
  mpc_init (temp->value);

  return temp;
}

FACT_num_t *
FACT_alloc_num_array (size_t n)
{
  FACT_num_t *temp;

  assert (n != 0);
  temp = FACT_malloc (sizeof (FACT_num_t) * n); /* Allocate the nodes. */

  /* Initialize all the nodes. */
  do
    {
      n--;
      temp[n] = FACT_alloc_num ();
    }
  while (n > 0);

  return temp;
}

FACT_scope_t
FACT_alloc_scope (void) /* Allocate and initialize a scope type. */
{
  FACT_scope_t temp;

  /* Allocate the memory. */
  temp = FACT_malloc (sizeof (struct FACT_scope));
  temp->marked = FACT_malloc (sizeof (bool));
  temp->array_size = FACT_malloc (sizeof (size_t));
  temp->code = FACT_malloc (sizeof (size_t));
  temp->var_table = FACT_malloc (sizeof (FACT_t *));
  temp->num_vars = FACT_malloc (sizeof (size_t));
  temp->array_up = FACT_malloc (sizeof (FACT_scope_t **));
  temp->name = "lambda";
  
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

  return temp;
}

FACT_scope_t *
FACT_alloc_scope_array (size_t n)
{
  FACT_scope_t *temp;

  assert (n != 0);
  temp = FACT_malloc (sizeof (FACT_scope_t) * n); /* Allocate the nodes. */

  /* Initialize all the nodes. */
  do
    {
      n--;
      temp[n] = FACT_alloc_scope ();
    }
  while (n > 0);

  return temp;
}
