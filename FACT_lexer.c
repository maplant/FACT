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
static bool is_num (char *);

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
	  goto alloc_token;
	}
      else if (str_follow == S_STR && *end != '\'')
	{
	  while (*end != '\0' && (*end != '\'' || *(end - 1) == '\\'))
	    end++;
	  goto alloc_token;
	}

      /* Skip all insignificant whitespace, tally the newlines. */
      if (*end == '\t' || *end == '\n' || *end == ' ')
	{
	  if (*end == '\n')
	    lines++;
	  start = ++end;
	  continue;
	}

      /* Check for EOI after the newlines. */
      // if (*end == '\0' )
      // goto alloc_token;

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
      
      /* Operators and other things. */
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

	case '?':
	  follow1 = '?';
	  continue;

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

	case '#':
	  /* Skip until a newline or nul terminator is reached. */
	  for (end++; *end != '\n' && *end != '\0'; end++);
	  /*
	  if (*end == '\n')
	    end++;
	  */
	  start = end;
	  continue;
	  
	default:
	  end++;
	  break;
	}

      /* Create a new token. */
    alloc_token:
      follow1 = follow2 = -1;
      res.tokens = FACT_realloc (res.tokens, sizeof (FACT_token_t) * (res.curr + 1));
      res.tokens[res.curr].lines = lines;
      lines = 0;
      
      if ((end - start) > 0)
	{
	  /* Allocate the token. */
	  res.tokens[res.curr].lexem = FACT_malloc_atomic ((end - start + 1) * sizeof (char));
	  memcpy (res.tokens[res.curr].lexem, start, (end - start));
	  res.tokens[res.curr].lexem[end - start] = '\0';
	  res.tokens[res.curr].id = ((str_follow == N_STR || *start == '"' || *start == '\'')
				     ? get_type (res.tokens[res.curr].lexem)
				     : E_VAR);
	  start = end;
	  res.curr++;
	}
      else
	{
	  res.tokens[res.curr].id = E_END;
	  break;
	}
    }

  if (res.curr == 0 || res.tokens[res.curr - 1].id != E_END)
    {
      /* If the last token is not E_END, make it so. */
      res.tokens = FACT_realloc (res.tokens, sizeof (FACT_token_t) * (res.curr + 1));
      res.tokens[res.curr].id = E_END;
      res.tokens[res.curr].lines = lines;
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
    "?",
    "??",
    "[", "]",
    "^",
    "^=",
    "@",
    "break",
    "catch",
    "else",
    "for",
    "give",
    "handle",
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
    "variable name",      /* E_VAR        */
    "numerical value",    /* E_NUM        */
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
	return i;
    }

  if (is_num (token))
    return E_NUM;
  return E_VAR;
}

static bool
is_num (char *tok) /* Check if a token is a valid number. */
{
  bool hex, flp;
  size_t i;

  hex = ((tok[0] == '0' && tolower ((int) tok[1]) == 'x')
	 ? true
	 : false);

  for (i = hex ? 2 : 0, flp = false; tok[i] != '\0'; i++)
    {
      if (tok[i] == '.')
	{
	  if (flp || tok[i + 1] == '\0')
	    return false;
	  flp = true;
	}
      else if (!isdigit ((int) tok[i]))
        {
          if (!hex || tolower ((int) tok[i]) < 'a' || tolower ((int) tok[i]) > 'f')
            return false;
        }
    }
  return ((hex && tok[2] == '\0')
	  ? false
	  : true);
}
