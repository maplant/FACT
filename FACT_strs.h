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

#ifndef FACT_STRS_H_
#define FACT_STRS_H_

#include "FACT_types.h"

/* String conversion functions:                            */
char *FACT_natos (FACT_num_t);  /* Number array to string. */
FACT_num_t FACT_stona (char *); /* String to number array. */

#endif /* FACT_STRS_H_ */
