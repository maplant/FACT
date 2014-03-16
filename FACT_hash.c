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

FACT_t *FACT_find_in_table_nohash (FACT_table_t *table, char *key)
{
  register struct _entry *p;

  if (table->num_buckets == 0)
    return NULL;

  for (p = *(table->buckets + FACT_get_hash (key, strlen (key)) % table->num_buckets);
       p != NULL && p->data->type != UNSET_TYPE;
       p = p->next) {
    if (!strcmp (key, FACT_var_name (*p->data)))
      return p->data;
  }

  return NULL;
}

FACT_t *FACT_find_in_table (FACT_table_t *table, char *key, size_t hash)
{
  register struct _entry *p, **fp;

  /* TODO: add moving the found element to the front of the list. */
  if (table->num_buckets == 0 ||
      *(fp = table->buckets + hash % table->num_buckets) == NULL)
    return NULL;

  for (p = *fp;
       p->data->type != UNSET_TYPE;
       p = p->next) {
    if (!strcmp (key, FACT_var_name (*p->data))) 
      return p->data;
  }

  /* Item wasn't found. */
  return NULL;
}

/*
 * I changed my coding style drastically between the last commit. Sorry but not
 * sorry.
 */
FACT_t *
FACT_add_to_table(FACT_table_t *table, FACT_t key)
{
	char *vname;
	size_t i, d, h;
	struct _entry *p, **pp;

	if (table->buckets == NULL) {
		table->buckets = FACT_malloc(sizeof (struct _entry *)
					     * INIT_NUM_BUCKETS);
		table->num_buckets = INIT_NUM_BUCKETS;
		table->num_entries = 1;
		bzero(table->buckets, (sizeof (struct _entry *)
				       * INIT_NUM_BUCKETS));
	} else if (++table->num_entries % table->num_buckets == 0 &&
		   table->num_buckets / table->num_entries == 2) {
		/*
		 * If the load factor is two, then we double the number of
		 * buckets and rehash. Yeah, I probably should read what Knuth
		 * has to say about this at some point.
		 */
		size_t nsize;

		nsize = table->num_buckets << 1;
		table->buckets = FACT_realloc(table->buckets,
					      (sizeof (struct _entry *)
					       * nsize));
		bzero(table->buckets + table->num_buckets, nsize);
		for (i = 0; i < table->num_buckets; i++) {
			pp = (table->buckets + 1);
			for (p = *(table->buckets + i); p != NULL; ) {
				vname = FACT_var_name(*p->data);
				d = FACT_get_hash(vname, strlen(vname));
				if (d != i) {
					/*
					 * Move the entry as ist no longer
					 * belongs in the current bucket.
					 */
					*pp = p->next;
					p->next = table->buckets[d];
					p = *pp;
					continue;
				}
				pp = &p->next;
				p = p->next;
			}
		}
		table->num_buckets = nsize;
	}

	/* Now finally we add the entry. */
	vname = FACT_var_name(key);
	h = FACT_get_hash(vname, strlen(vname));
	for (p = *(table->buckets + h % table->num_buckets);
	     p != NULL && p->data->type != UNSET_TYPE;
	     p = p->next)
		;
	if (p == NULL)
		p = *(table->buckets + h % table->num_buckets)
			= FACT_malloc(sizeof (struct _entry));
	*p->data = key;
	p->next = FACT_malloc(sizeof (struct _entry));
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

  if (table == NULL || table->num_entries == 0)
    return;

  items = FACT_malloc (sizeof (char *) * table->num_entries);

  for (i = k = 0; i < table->num_buckets; i++) {
    for (p = *(table->buckets + i); p != NULL && p->data->type != UNSET_TYPE; p = p->next)
      items[k++] = FACT_var_name (*p->data);
  }

  /* Sort the entries. */
  sqsort (items, 0, table->num_entries - 1);

  /* Print out the entries. */
  for (i = 0; i < table->num_entries - 1; i++)
    printf ("%s, ", items[i]);
  printf ("%s ", items[i]);

  FACT_free(items);
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

/* FNV hash function, slightly modified from the reference implementation. */
inline size_t FACT_get_hash (char *k, size_t l) 
{
  size_t i;
  size_t hval;

#ifdef __amd64__
  hval = 0xcbf29ce484222325ULL;
#else
  hval = 0x811c9dc5;
#endif

  for (i = 0; i < l; i++) {
    /* xor the bottom with the current octet */
    hval ^= (size_t) k[i];

#ifdef __amd64__
    /* 64 bit version. */
    hval += ((hval << 1) + (hval << 4) + (hval << 5) +
	     (hval << 7) + (hval << 8) + (hval << 40));
#else
    /* 32 bit version. */
    hval += (hval<<1) + (hval<<4) + (hval<<7) + (hval<<8) + (hval<<24);
#endif
  }

  /* return our new hash value */
  return hval;
}

#if 0
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
#endif
