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

static FACT_nterm_t get_type (char *);

FACT_lexed_t
FACT_lex_string (char *start) /* The main lexer routine. */
{
  int follow1, follow2;
  char *end;
  size_t lines;
  FACT_lexed_t res;

  end = start;
  res.tokens = NULL;
  follow1 = follow2 = -1;
  res.curr = lines = 0;
  
  while (*end != '\0')
    {
      /* Skip all insignificant whitespace, tally the newlines. */
      while (*end == '\t' || *end == '\n' || *end == ' ')
	{
	  if (*end == '\n')
	    lines++;
	  start = ++end;
	}

      /* Check for EOI after the newlines. */
      if (*end == '\0' )
	goto alloc_token;

      /* Check for follow up characters. */
      if (follow1 != -1)
	{
	  end++;
	  if (*end == follow1 || *end == follow2)
	    end++;
	  goto alloc_token;
	}

      /* Get variable names and numerical values. */
      if (isalnum (*end) || *end == '_' || *end == '.')
	{
	  for (end++; isalnum (*end) || *end == '_' || *end == '.'; end++);
	  goto alloc_token;
	}
      
      /* Operators. */
      switch (*end)
	{
	case '\'': /* Raw string. */
	  for (end++; *end != '\0' && (*end != '\'' || *(end - 1) == '\\'); end++);
	  break;

	case '"':
	  for (end++; *end != '\0' && (*end != '"' || *(end - 1) == '\\'); end++);
	  break;

	case '&':
	  follow1 = '=';
	  follow2 = '&';
	  continue;

	case '|':
	  follow1 = '=';
	  follow2 = '|';
	  continue;

	case '-':
	  follow2 = '>';
	case '=':
	case '+':
	case '*':
	case '/':
	case '%':
	case '<':
	case '>':
	case '!':
	  follow1 = '=';
	  continue;

	default:
	  end++;
	  break;
	}

      /* Create a new token. */
    alloc_token:
      follow1 = follow2 = -1;
      res.tokens = FACT_realloc (res.tokens, sizeof (FACT_token_t) * (res.curr + 1));
      if ((end - start) > 0)
	{
	  /* Allocate the token. */
	  res.tokens[res.curr].lexem = FACT_malloc ((end - start + 1) * sizeof (char));
	  memcpy (res.tokens[res.curr].lexem, start, (end - start));
	  res.tokens[res.curr].lexem[end - start] = '\0';
	  res.tokens[res.curr].id = get_type (res.tokens[res.curr].lexem);
	  res.tokens[res.curr].lines = lines;
	  lines = 0;
	  start = end;
	  res.curr++;
	}
      else
	{
	  res.tokens[res.curr].id = E_END;
	  res.tokens[res.curr].lines = 0;
	  break;
	}
    }

  /* There's like a five percent chance I managed to do this right on the
   * first try. Well aparently I got close!
   */
  if (res.tokens[res.curr - 1].id != E_END)
    {
      /* If the last token is not E_END, make it so. */
      res.tokens = FACT_realloc (res.tokens, sizeof (FACT_token_t) * (res.curr + 1));
      res.tokens[res.curr].id = E_END;
      res.tokens[res.curr].lines = 0;
    }

  res.curr = 0;
  return res;
}

static const char *tags[] =
  {
    "!=",
    "%",
    "%=",
    "&",
    "&&",
    "&=",
    "(", ")",
    "*",
    "*=",
    "+",
    "+=",
    ",",
    "-",
    "-=",
    "/",
    "/=",
    ":",
    ";",
    "<",
    "<=",
    "=",
    "==",
    ">",
    ">=",
    "[", "]",
    "^",
    "^=",
    "@",
    "break",
    "catch",
    "else",
    "for",
    "hold"
    "if",
    "num",
    "return",
    "scope",
    "while",
    "{",
    "|",
    "|=",
    "||",
    "}"
    "", /* E_VAR        */
    "", /* E_FUNC_CALL  */
    "", /* E_ARRAY_ELEM */
    "", /* E_NEG        */
    "", /* E_END        */
  };

const char *
FACT_get_lexem (FACT_nterm_t id)
{
  switch (id)
    {
    case E_VAR:
      return "variable";

    case E_FUNC_CALL:
      return "function call";

    case E_ARRAY_ELEM:
      return "array subsription";

    case E_NEG:
      return "negative";

    case E_END:
      return "end of statement";

    default:
      return tags[id];
    }
}

static FACT_nterm_t
get_type (char *token)
{
  size_t i;
  
  for (i = 0; tags[i][0] != '\0'; i++)
    {
      if (!strcmp (tags[i], token))
	return i;
    }
  
  return E_VAR;
}
