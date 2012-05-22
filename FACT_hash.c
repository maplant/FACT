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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "FACT.h"
#include "FACT_hash.h"
#include "FACT_alloc.h"
#include "FACT_types.h"
#include "FACT_var.h"

static inline ulong Zend_DJBX33A (char *, uint);

FACT_t *FACT_find_in_table (FACT_table_t *table, char *key)
{
  register struct _entry *p;

  if (table->num_buckets == 0)
    return NULL;

  for (p = table->buckets + Zend_DJBX33A (key, strlen (key)) % table->num_buckets;
       p != NULL && p->data->type != UNSET_TYPE;
       p = p->next) {
    if (!strcmp (key, FACT_var_name (*p->data)))
      return p->data;
  }

  return NULL;
}

FACT_t *FACT_add_to_table (FACT_table_t *table, FACT_t key)
{
  size_t i, d;
  char *var_name;
  FACT_t k;
  struct _entry *p, *e;

  if (table->buckets == NULL) {
    table->buckets = FACT_malloc (sizeof (struct _entry) * START_NUM_ENTRIES);
    table->num_buckets = START_NUM_ENTRIES;
    table->total_num_vars = 1;
    for (i = 0; i < START_NUM_ENTRIES; i++)
      table->buckets[i].data->type = UNSET_TYPE;
  } else if (++table->total_num_vars % table->num_buckets == 0 &&
	     table->total_num_vars / table->num_buckets == 2) {
    /* If the load factor is 2, then double the number of buckets and rehash. */
    table->buckets = FACT_realloc (table->buckets, sizeof (struct _entry) * (table->num_buckets << 1));
    for (i = 0; i < table->num_buckets; i++) {
      for (p = table->buckets + i; p != NULL && p->data->type != UNSET_TYPE;) {
	d = Zend_DJBX33A (FACT_var_name (*p->data), strlen (FACT_var_name (*p->data)))
	  % (table->num_buckets << 1);
	if (d != i) {
	  for (e = table->buckets + d; e->data->type != UNSET_TYPE; e = e->next)
	    ;
	  *e->data = *p->data;
	  e->next = FACT_malloc (sizeof (struct _entry));
	  e->next->data->type = UNSET_TYPE;
	  e->next->next = NULL;
	  if (p->next != NULL) {
	    *p->data = *p->next->data;
	    p->next = p->next->next;
	    continue;
	  }
	}
	p = p->next;
      }
    }
    table->num_buckets <<= 1;
  }

  var_name = FACT_var_name (key);
  for (p = table->buckets + Zend_DJBX33A (var_name, strlen (var_name)) % table->num_buckets;
       p->data->type != UNSET_TYPE;
       p = p->next)
    ;
  *p->data = key;
  p->next = FACT_malloc (sizeof (struct _entry));
  p->next->data->type = UNSET_TYPE;
  p->next->next = NULL;
  return p->data;
}

static void sqsort (char **, size_t, size_t);

void FACT_table_digest (FACT_table_t *table)
{
  size_t i, k;
  char **items;
  struct _entry *p;

  if (table == NULL || table->total_num_vars == 0)
    return;

  items = FACT_malloc (sizeof (char *) * table->total_num_vars);

  for (i = k = 0; i < table->num_buckets; i++) {
    for (p = table->buckets + i; p->data->type != UNSET_TYPE; p = p->next)
      items[k++] = FACT_var_name (*p->data);
  }

  /* Sort the entries. */
  sqsort (items, 0, table->total_num_vars - 1);

  /* Print out the entries. */
  for (i = 0; i < table->total_num_vars - 1; i++)
    printf ("%s, ", items[i]);
  printf ("%s ", items[i]);
}

static void sqsort (char **v, size_t left, size_t right) /* Thanks K&R! */
{
  char *temp;
  size_t i, last;

  if (left >= right)
    return;

  temp = v[left];
  v[left] = v[(left + right) / 2];
  v[(left + right) / 2] = temp;
  last = left;
  for (i = left + 1; i <= right; i++) {
    if (strcmp (v[i], v[left]) < 0) {
      temp = v[++last];
      v[last] = v[i];
      v[i] = temp;
    }
  }
  temp = v[left];
  v[left] = v[last];
  v[last] = temp;
  if (last != 0)
    sqsort (v, left, last - 1);
  sqsort (v, last + 1, right);
}

/* DJBX33A, taken from PHP's Zend source code. */
static inline ulong Zend_DJBX33A (char *arKey, uint nKeyLength)
{
  register ulong hash = 5381;
  
  /* variant with the hash unrolled eight times */
  for (; nKeyLength >= 8; nKeyLength -= 8) {
    hash = ((hash << 5) + hash) + *arKey++;
    hash = ((hash << 5) + hash) + *arKey++;
    hash = ((hash << 5) + hash) + *arKey++;
    hash = ((hash << 5) + hash) + *arKey++;
    hash = ((hash << 5) + hash) + *arKey++;
    hash = ((hash << 5) + hash) + *arKey++;
    hash = ((hash << 5) + hash) + *arKey++;
    hash = ((hash << 5) + hash) + *arKey++;
  }
  switch (nKeyLength) {
  case 7: hash = ((hash << 5) + hash) + *arKey++; /* fallthrough... */
  case 6: hash = ((hash << 5) + hash) + *arKey++; /* fallthrough... */
  case 5: hash = ((hash << 5) + hash) + *arKey++; /* fallthrough... */
  case 4: hash = ((hash << 5) + hash) + *arKey++; /* fallthrough... */
  case 3: hash = ((hash << 5) + hash) + *arKey++; /* fallthrough... */
  case 2: hash = ((hash << 5) + hash) + *arKey++; /* fallthrough... */
  case 1: hash = ((hash << 5) + hash) + *arKey++; break;
  case 0: break;
  default:
    break;
  }
  return hash;
}
