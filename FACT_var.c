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

static FACT_t get_var_noerror (FACT_scope_t, char *);

void
FACT_get_var (char *name) /* Search all relevent scopes for a variable. */
{
  FACT_t res;

  /* Search for the variable. */
  res = get_var_noerror (CURR_THIS, name);

  /* If it doesn't exist, throw an error. Otherwise, push it to the var
   * stack.
   */
  if (res.ap == NULL)
    FACT_throw_error (CURR_THIS, "undefined variable: %s", name);

  push_v (res);
}

static FACT_t
get_var_noerror (FACT_scope_t curr, char *name) /* Search for a var, but don't throw an error. */
{
  FACT_t res;
  
  /* If the scope doesn't exist or is marked. */ 
  if (curr == NULL || *curr->marked)
    {
      res.ap = NULL;
      return res;
    }

  /* Mark the scope. */
  *curr->marked = true;
  res.ap = NULL;

  /* Search for the variable, if there are any in the scope. */
  if (*curr->num_stack_size)
    res.ap = FACT_get_local_num (curr, name);
  if (res.ap != NULL)
    res.type = NUM_TYPE;
  else
    {
      res.ap = FACT_get_local_scope (curr, name);
      if (res.ap != NULL)
	res.type = SCOPE_TYPE;
      else
	/* If the var isn't local, search the next scope up. */
	res = get_var_noerror (FACT_get_local_scope (curr, "up"), name);
    }

  /* Unmark the scope. */
  *curr->marked = false;

  return res;
}
