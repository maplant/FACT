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

#ifndef FACT_THREADS_H_
#define FACT_THREADS_H_

#include "FACT.h"

typedef struct FACT_num *FACT_num_t;

void FACT_send_message (FACT_num_t, size_t); /* Send a message. */
FACT_scope_t FACT_get_next_message (void); /* Recieve a message. */

#endif /* FACT_THREADS_H_ */
