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
  enum
  {
    N_STR = 0,
    S_STR,
    D_STR,
  } str_follow;

  end = start;
  res.tokens = NULL;
  follow1 = follow2 = -1;
  res.curr = lines = 0;
  str_follow = N_STR;
  
  while (*end != '\0')
    {
      if (str_follow == D_STR && *end != '"')
	{
	  while (*end != '\0' && (*end != '"' || *(end - 1) == '\\'))
	    end++;
	  //	  if (*end == '"')
	  //  end--;
	  goto alloc_token;
	}
      else if (str_follow == S_STR && *end != '\'')
	{
	  while (*end != '\0' && (*end != '\'' || *(end - 1) == '\\'))
	    end++;
	  // if (*end == '\'')
	  //  end--;
	  goto alloc_token;
	}

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
	  end++;
	  str_follow = ((str_follow != S_STR)
			? S_STR
			: N_STR);
	  break;

	case '"':
	  end++;
	  str_follow = ((str_follow != D_STR)
			? D_STR
			: N_STR);
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
	  res.tokens[res.curr].id = ((str_follow == N_STR || *start == '"' || *start == '\'')
				     ? get_type (res.tokens[res.curr].lexem)
				     : E_VAR);
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
    "\"",
    "%",
    "%=",
    "&",
    "&&",
    "&=",
    "'",
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
    "hold",
    "if",
    "num",
    "return",
    "scope",
    "while",
    "{",
    "|",
    "|=",
    "||",
    "}",
    "variable",           /* E_VAR        */
    "function call",      /* E_FUNC_CALL  */
    "array subscription", /* E_ARRAY_ELEM */
    "unary `-'",          /* E_NEG        */
    "end of statement",   /* E_END        */
  };

const char *
FACT_get_lexem (FACT_nterm_t id)
{
  return tags[id];
}

static FACT_nterm_t
get_type (char *token)
{
  size_t i;
  
  for (i = 0; i < E_VAR; i++)
    {
      if (!strcmp (tags[i], token))
	break;
    }
  
  return i;
}
