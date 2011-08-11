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

void *
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

void *
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

void
FACT_free (void *dead)
{
#ifdef USE_GC
  GC_free (dead);
#else
  free (dead);
#endif /* USE_GC */
}

FACT_var_t
FACT_alloc_var (void) /* Allocate and initialize a var type. */
{
  FACT_var_t temp;

  temp = FACT_malloc (sizeof (struct FACT_var));
#ifndef USE_GC
  /* GC automatically sets the data it allocates to 0, so we only have to
   * memset if we aren't using it.
   */
  memset (temp, 0, sizeof (struct FACT_var));
#endif /* USE_GC */
  mpz_init (temp->array_size);
  mpc_init (temp->value);

  return temp;
}

FACT_var_t *
FACT_alloc_var_array (unsigned long n)
{
  FACT_var_t *temp;

  assert (n != 0);
  temp = FACT_malloc (sizeof (FACT_var_t) * n); /* Allocate the nodes. */

  /* Initialize all the nodes. */
  do
    {
      n--;
      temp[n] = FACT_alloc_var ();
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
  temp->array_size = FACT_malloc (sizeof (mpz_t));
  temp->code = FACT_malloc (sizeof (long));
  temp->var_stack = FACT_malloc (sizeof (FACT_var_t *));
  temp->scope_stack = FACT_malloc (sizeof (FACT_scope_t *));
  temp->var_stack_size = FACT_malloc (sizeof (long));
  temp->scope_stack_size = FACT_malloc (sizeof (long));
  temp->array_up = FACT_malloc (sizeof (FACT_scope_t **));

  /* Initialize the memory, if we need to. */
  mpz_init (*temp->array_size);
  *temp->code = 0;
#ifndef USE_GC
  temp->name = "Witch Woman Jenka";
  /* Did you know that this scope had a brother? */ 
  temp->file_name = NULL;
  *temp->marked = false;
  *temp->var_stack = NULL;
  *temp->scope_stack = NULL;
  *temp->var_stack_size = 0;
  *temp->scope_stack_size = 0;
  temp->extrn_func = NULL;
  temp->caller = NULL;
  *temp->array_up = NULL;
  temp->variadic = NULL;
#endif /* USE_GC */

  return temp;
}

FACT_scope_t *
FACT_alloc_scope_array (unsigned long n)
{
  FACT_scope_t *temp;

  assert (n != 0);
  temp = FACT_malloc (sizeof (FACT_var_t) * n); /* Allocate the nodes. */

  /* Initialize all the nodes. */
  do
    {
      n--;
      temp[n] = FACT_alloc_scope ();
    }
  while (n > 0);

  return temp;
}
