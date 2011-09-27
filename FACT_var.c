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

FACT_t *
FACT_get_local (FACT_scope_t env, char *name) /* Search for a variable. */
{
  int res;
  FACT_t *low, *mid, *high;

  low = *env->var_table;
  high = low + *env->num_vars;

  while (low < high)
    {
      mid = low + ((high - low) >> 1);
      res = strcmp (name, FACT_var_name (*mid));

      if (res < 0)
	high = mid;
      else if (res > 0)
	low = mid + 1;
      else
	return mid;
    }

  /* No variable was found. */
  return NULL;
}

void
FACT_get_var (char *name) /* Search all relevent scopes for a variable and push it to the stack. */
{
  FACT_t *res;
  FACT_scope_t env = CURR_THIS;

  /* Currently we do not mark the scopes. This should be done in the future
   * in order to prevent infinite lookup loops.
   */
  for (;;)
    {
      /* Search for the variable. */
      res = FACT_get_local (env, name);

      if (res == NULL) /* No such variable exists in this scope. */
	{
	  /* Go up one scope. */
	  res = FACT_get_local (env, "up");

	  /* If there is no up scope, or if it is invalid. */
	  if (res == NULL || res->type != SCOPE_TYPE)
	    goto error;
	  
	  env = res->ap;
	}
      else
	{
	  /* Push the variable. */
	  push_v (*res);
	  return; /* Exit. */
	}
    }

 error:
  /* If the variable doesn't exist, throw an error. */
  FACT_throw_error (CURR_THIS, "undefined variable: %s", name);
}
