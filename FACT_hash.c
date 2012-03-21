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

#include <FACT.h>

static inline ulong Zend_DJBX33A (char *, uint);

static FACT_t *find_var (struct _entry table, char *name)
{
  register int res;
  register size_t lim;
  register FACT_t *base, *p;

  base = table.data;

  for (lim = table.num_vars; lim != 0; lim >>= 1) {
    p = base + (lim >> 1);
    res = strcmp (name, FACT_var_name (*p));
    
    if (res == 0)
      return p;
    if (res > 0) {
      base = p + 1;
      lim--;
    }
  }

  return NULL;
}

FACT_t *FACT_find_in_table (FACT_table_t *table, char *key)
{
  return ((table->num_buckets == 0)
	  ? NULL
	  /* Strlenkey would be a great Russian name */
	  : find_var (table->buckets[Zend_DJBX33A (key, strlen (key)) % table->num_buckets], key));
}

FACT_t *FACT_add_to_table (FACT_table_t *table, FACT_t key)
{
  size_t i, j, k;
  char *var_name;
  struct _entry *prev, *dest;

  if (table->buckets == NULL) {
    table->buckets = FACT_malloc (sizeof (struct _entry) * START_NUM_ENTRIES);
    table->num_buckets = START_NUM_ENTRIES;
    table->total_num_vars = 1;
  } else if (++table->total_num_vars / table->num_buckets == 2) {
    /* If the load factor is 2, then double the number of buckets and rehash. */ 
    prev = table->buckets;
    table->buckets = FACT_malloc (sizeof (struct _entry) * table->num_buckets * 2);
    /* Horrible little code. Could be improved tons. */
    for (i = 0; i < table->num_buckets; i++) {
      for (j = 0; j < prev[i].num_vars; j++) {
	/* Add the item to the new table. */
	var_name = FACT_var_name (prev[i].data[j]);
	dest = &table->buckets[Zend_DJBX33A (var_name, strlen (var_name)) % (table->num_buckets * 2)];
	dest->data = FACT_realloc (dest->data, (dest->num_vars + 1) * sizeof (FACT_t));
	dest->data[dest->num_vars] = prev[i].data[j];
	/* Move the entry into the correct, alphabetical order. */
	for (k = dest->num_vars++; k > 0; k--) {
	  if (strcmp (FACT_var_name (dest->data[k - 1]), var_name) > 0) {
	    FACT_t hold;
	    hold = dest->data[k - 1];
	    dest->data[k - 1] = dest->data[k];
	    dest->data[k] = hold;
	  } else
	    break;
	}
      }
      FACT_free (prev[i].data);
    }
    FACT_free (prev);
    table->num_buckets *= 2; 
  }

  var_name = FACT_var_name (key);
  dest = &table->buckets[Zend_DJBX33A (var_name, strlen (var_name)) % table->num_buckets];
  /* Reallocate the table */
  dest->data = FACT_realloc (dest->data, (dest->num_vars + 1) * sizeof (FACT_t));
  dest->data[dest->num_vars] = key;

  for (i = dest->num_vars++; i > 0; i--) {
    if (strcmp (FACT_var_name (dest->data[i - 1]), var_name) > 0) {
      FACT_t hold;
      hold = dest->data[i - 1];
      dest->data[i - 1] = dest->data[i];
      dest->data[i] = hold;
    } else
      break;
  }
  return dest->data + i;
}

static void bucket_digest (struct _entry bucket)
{
  size_t i;

  for (i = 0; i < bucket.num_vars; i++)
    printf ("%s ", FACT_var_name (bucket.data[i]));
}

void FACT_table_digest (FACT_table_t *table)
{
  int i;

  if (table == NULL)
    return;
  
  for (i = 0; i < table->num_buckets; i++) {
    if (table->buckets[i].num_vars != 0) {
      printf ("[ ");
      bucket_digest (table->buckets[i]);
      printf ("] ");
    }
  }
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
