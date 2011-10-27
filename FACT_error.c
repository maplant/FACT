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

/* Structure to map instruction address to files and line numbers. */
static size_t num_maps = 0;
struct map
{
  size_t line;
  const char *file_name;
} *addr_map = NULL;
const struct map NO_MAP =
  {
    .line = 0,        /* Line cannot be 0 in any other context. */
    .file_name = NULL /* Same goes for file_name.               */
  };

int
FACT_add_line (const char *file, size_t line, size_t addr) /* Map an address to line number and file name. */ 
{
  size_t i;
  
  /* If the addr was already mapped, re-map it. */
  if (addr < num_maps)
    {
      addr_map[addr].line = line;
      addr_map[addr].file_name = file;
      return -1; /* return -1 on re-map. */
    }

  /* Allocate or Reallocate the addr_map. */
  if (num_maps == 0)
    {
      addr_map = FACT_malloc (sizeof (struct map) * (addr + 1));
      num_maps = 1;
    }
  else
    addr_map = FACT_realloc (addr_map, sizeof (struct map) * (addr + 1));

  /* Set the new mapping. */
  addr_map[addr].line = line;
  addr_map[addr].file_name = file;
  
  /* Set all the maps in no man's land to NO_MAP. */
  for (i = num_maps; i < addr; i++)
    addr_map[i] = NO_MAP;

  num_maps = addr + 1;
  return 0; /* Return 0 on success. */
}

size_t
FACT_get_line (size_t addr) /* Get a line number from an address. */
{
  if (addr >= num_maps)
    return 0; /* Return 0 on error. */

  while (addr_map[addr].line == 0 && addr > 0)
    addr--;

  return addr_map[addr].line;
}

const char *
FACT_get_file (size_t addr) /* Get a file name from an address. */
{
  if (addr >= num_maps)
    return NULL; /* Return NULL on error. */

  while (addr_map[addr].line == 0 && addr < num_maps)
    addr++;

  return (addr >= num_maps
	  ? NULL
	  : addr_map[addr].file_name);
}

void
FACT_throw_error (FACT_scope_t scope, const char *fmt, ...)
/* Set curr_err and long jump back to the error handler. */
{
  char *buff;
  va_list args;
  FACT_error_t err;

  /* Allocate the buffer. Make sure to free it later. */
  buff = FACT_malloc_atomic (MAX_ERR_LEN + 1);

  /* Get the formatted string. */
  va_start (args, fmt);
  vsnprintf (buff, MAX_ERR_LEN, fmt, args);
  va_end (args);
  err.what = buff;

  curr_thread->curr_err = err; /* Set the error. */
  longjmp (handle_err, 1); /* Jump back. */
}

void
FACT_print_error (FACT_error_t err) /* Print out an error to stderr. */
{
  /* ... */
}
