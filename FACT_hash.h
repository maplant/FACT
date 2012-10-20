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

#ifndef FACT_HASH_H_
#define FACT_HASH_H_

#include "FACT.h"
#include "FACT_types.h"

#include <string.h>

#define START_NUM_ENTRIES 256

struct _var_table {
  struct _entry {
    FACT_t data[1];
    struct _entry *next;
  } **buckets;
  size_t num_buckets;
  size_t total_num_vars;
};

FACT_t *FACT_find_in_table_nohash (FACT_table_t *, char *);
FACT_t *FACT_find_in_table (FACT_table_t *, char *, size_t);
FACT_t *FACT_add_to_table (FACT_table_t *, FACT_t);
void FACT_table_digest (FACT_table_t *);

inline size_t FACT_get_hash (char *, size_t);

#endif /* FACT_HASH_H_ */
