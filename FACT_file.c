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

int
FACT_load_file (const char *file_name) /* Load a file into the VM. */
{
  int c;
  FILE *fp;
  char *file;
  size_t i;
  FACT_tree_t parsed;
  FACT_lexed_t tokenized;

  /* Allocate the entire file into memory. */ 
  fp = fopen (file_name, "r"); /* Open the file for reading. */
  assert (fp != NULL);

  for (file = NULL, i = 0; (c = getc (fp)) != EOF; i++)
    {
      if (c == '\0') /* Put some more bounds here. */
	i--;
      else if (c == '#') /* Skip over a comment. */
	{
	  while ((c = getc (fp)) != EOF && c != '\n')
	    ;
	}
      else
	{
	  file = FACT_realloc (file, (i + 2));
	  file[i] = c;
	}
    }

  if (file != NULL)
    {
      /* NUL terminator. */
      file[i] = '\0';
      
      /* Tokenize, parse, compile and load. */
      tokenized = FACT_lex_string (file);
      tokenized.line = 1;
      
      if (setjmp (tokenized.handle_err))
	{
	  /* There was a parsing error, print it and return fail. */
	  printf ("=> Parsing error (%s:%zu): %s.\n", file_name, tokenized.line, tokenized.err);
	  return -1;
	}
      
      parsed = FACT_parse (&tokenized);
      FACT_compile (parsed, file_name);
    }
  
  fclose (fp);
  return 0;
}
