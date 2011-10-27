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

#include <FACT.h>

char *
FACT_natos (FACT_num_t arr) /* Number array to string. */
{
  size_t i;
  char *res;

  if (arr->array_size == 0)
    {
      res = FACT_malloc_atomic (1);
      *res = (char) mpc_get_si (arr->value);
    }
  else
    {
      res = FACT_malloc_atomic (arr->array_size);
      for (i = 0; i < arr->array_size; i++)
	res[i] = (char) mpc_get_si (arr->array_up[i]->value);
    }

  return res;
}

FACT_num_t 
FACT_stona (char *str) /* String to number array. */
{
  size_t i, len;
  FACT_num_t res;

  len = strlen (str);
  res = FACT_alloc_num ();

  res->array_size = len;
  res->array_up = FACT_alloc_num_array (len);
  for (i = 0; i < len; i++)
    mpc_set_si (res->array_up[i]->value, *(str++));

  return res;
}
