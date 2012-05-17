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
FACT_t *FACT_get_global (FACT_scope_t, char *); /* Search for a global variable.                   */

static inline FACT_t *FACT_get_local (FACT_scope_t env, char *name)
{
  return FACT_find_in_table (env->vars, name);
}

#define FACT_var_name(v)			\
  ({ FACT_t _v = (v);				\
  _v.type == NUM_TYPE				\
    ? ((FACT_num_t) _v.ap)->name		\
    : _v.type == SCOPE_TYPE			\
    ? ((FACT_scope_t) _v.ap)->name		\
    : NULL; })

#endif /* FACT_VAR_H_ */
