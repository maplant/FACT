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

void
FACT_throw_error (FACT_scope_t scope, const char *fmt, ...)
/* Set curr_err and long jump back to the error handler. */
{
  char *buff;
  va_list args;
  FACT_error_t err;

  /* Allocate the buffer. Make sure to free it later. */
  buff = FACT_malloc (sizeof (char) * MAX_ERR_LEN);

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
