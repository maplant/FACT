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

#ifndef FACT_NUM_H_
#define FACT_NUM_H_

FACT_num_t FACT_get_local_num (FACT_scope_t, char *);
FACT_num_t FACT_add_num (FACT_scope_t, char *);

void FACT_def_num (char *, bool);
void FACT_get_num_elem (FACT_num_t, char *);

void FACT_set_num (FACT_num_t, FACT_num_t);

void FACT_append_num (FACT_num_t, FACT_num_t);

#endif /* FACT_NUM_H_ */
