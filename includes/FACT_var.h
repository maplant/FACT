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

FACT_var_t FACT_get_local_var (FACT_scope_t, char *);
FACT_var_t FACT_add_var (FACT_scope_t, char *);

void FACT_def_var (char *, bool);
void FACT_get_var_elem (FACT_var_t, char *);

#endif /* FACT_VAR_H_ */
