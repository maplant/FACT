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
  /* The following is adapted from the infamous Berkeley C library
   * bsearch function. It should be faster than the previous version.
   * The only thing that I will note is that: yes, I do know what a
   * register is and I'm not using the directive simply because it's
   * used in the original version.
   */
  register int res;
  register size_t lim;
  register FACT_t *base, *p;

  base = *env->var_table;

  for (lim = *env->num_vars; lim != 0; lim >>= 1)
    {
      p = base + (lim >> 1);
      res = strcmp (name, FACT_var_name (*p));

      if (res == 0)
	return p;
      if (res > 0)
	{
	  base = p + 1;
	  lim--;
	}
    }

  /* No variable was found. */
  return NULL;
}

FACT_t *
FACT_get_global (FACT_scope_t env, char *name)
{
  FACT_t *res;

  /* We assume that the env is not NULL. */
  if (!*env->marked) /* Make sure that env isn't marked */
    {
      /* Search for the variable. */
      res = FACT_get_local (env, name);

      if (res == NULL) /* No such variable exists in this scope. */
	{
	  /* Go up one scope. */
	  res = FACT_get_local (env, "up");

	  /* If the up scope does not exist or is invalid, return NULL. */
	  if (res == NULL || res->type != SCOPE_TYPE)
	    return NULL;

	  /* Mark the current environment and search the next scope up. */
	  *env->marked = true;
	  res = FACT_get_global (res->ap, name);
	  /* Unmark the scope and return. */
	  *env->marked = false;
	}
      return res;
    }
  return NULL;
}

void
FACT_get_var (char *name) /* Search all relevent scopes for a variable and push it to the stack. */
{
  FACT_t *res;

  /* Get the variable. If it doesn't exist, throw an error. */
  res = FACT_get_global (CURR_THIS, name);
  if (res == NULL)
    FACT_throw_error (CURR_THIS, "undefined variable: %s", name);
  /* Push the variable to the var stack. */
  push_v (*res);
}
