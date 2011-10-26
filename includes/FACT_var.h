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

#ifndef FACT_VAR_H_
#define FACT_VAR_H_

/* Retrieving variables:                                                                           */
void FACT_get_var (char *);                     /* Search for a variable and push it to the stack. */
FACT_t *FACT_get_local (FACT_scope_t, char *);  /* Search for a local variable.                    */
FACT_t *FACT_get_global (FACT_scope_t, char *); /* Search for a global variable.                   */

static inline char *
FACT_var_name (FACT_t v)
{
  if (v.type == UNSET_TYPE)
    return NULL;
  
  return (v.type == NUM_TYPE
	  ? ((FACT_num_t) v.ap)->name
	  : ((FACT_scope_t) v.ap)->name);
}

#endif /* FACT_VAR_H_ */
