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

#ifndef FACT_H_
#define FACT_H_

#define FACT_VERSION "0.0.1" /* Furlow VM version number.        */
#define FACT_STDLIB_PATH "/usr/share/FACT/FACT_stdlib.ft"

#include <stdint.h>
#include <limits.h>
#include <stddef.h>
#include <stdbool.h>

#define Max(o1, o2) ((o1) > (o2) ? (o1) : (o2))
#define Min(o1, o2) ((o1) < (o2) ? (o1) : (o2))

#endif /* FACT_H_ */
