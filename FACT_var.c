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
#include "FACT_types.h"
#include "FACT_hash.h"
#include "FACT_vm.h"
#include "FACT_error.h"

/* Lexically global - not thread global. */
FACT_t *FACT_get_global (FACT_scope_t env, char *name)
{
  FACT_t *r;
   
  for (; env != NULL && env->lock_stat == UNLOCKED; env = env->up) {
    if ((r = FACT_find_in_table (env->vars, name)) != NULL)
      return r;
  }
  
  /* As a last ditch effort, try the global variables table. */
  return FACT_find_in_table (&Furlow_globals, name);    
}

void FACT_get_var (char *name) /* Search all relevent scopes for a variable and push it to the stack. */
{
  FACT_t *res;

  /* Get the variable. If it doesn't exist, throw an error. */
  res = FACT_get_global (CURR_THIS, name);
  if (res == NULL)
    FACT_throw_error (CURR_THIS, "undefined variable: %s", name);
  /* Push the variable to the var stack. */
  push_v (*res);
}

bool FACT_is_circular (FACT_scope_t env)
{
  bool r;
  
  if (env == NULL)
    return false;
  if (*env->marked)
    return true;
  *env->marked = true;
  r = FACT_is_circular (env->up);
  *env->marked = false;
  return r;
}
