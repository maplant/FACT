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

FACT_exp_t
FACT_lex_string (char *start) /* The main lexer routine. Runs before yylex. */
{
  int follow1, follow2;
  unsigned long lines;
  FACT_exp_t root, curr;
  char *end;

  root = FACT_malloc (sizeof (struct FACT_exp));
  memset (root, 0, sizeof (struct FACT_exp));
  curr = root;
  follow1 = follow2 = -1;
  
  while (*end != '\0')
    {
      /* Skip all insignificant whitespace, tally the newlines. */
      while (*end == '\t' || *end == '\n' || *end == ' ')
	{
	  if (*end == '\n')
	    curr->lines++;
	  start = ++end;
	}

      /* Check for EOI after the newlines. */
      if (*end == '\0' )
	goto alloc_token;

      /* Check for follow up characters. */
      if (follow1 != -1)
	{
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
      if ((end - start) > 0)
	{
	  curr->token = FACT_malloc ((end - start + 1) * sizeof (char));
	  memcpy (curr->token, start, (end - start));
	  curr->token[end - start] = '\0';
	  start = end;
	  curr->right = FACT_malloc (sizeof (struct FACT_exp));
	  memset (curr->right, 0, sizeof (struct FACT_exp));
	  curr->right->left = curr;
	  curr = curr->right;
	}
      else
	{
	  curr->token = NULL;
	  break;
	}
    }

  /* There's like a five percent chance I managed to do this right on the
   * first try.
   */
  return root;
}  

FACT_exp_t
create_node (FACT_nterm_t t, FACT_exp_t l, FACT_exp_t r)
{
  FACT_exp_t temp;

  temp = FACT_malloc (sizeof (struct FACT_exp));
  temp->left = l;
  temp->right = r;
  temp->id = t;

  return temp;
}

static FACT_nterm_t
get_type (char *token)
{
  static char *tags[] =
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
      "break",
      "def",
      "defunc",
      "else",
      "for",
      "if",
      "return",
      "while",
      "{",
      "|",
      "|=",
      "||",
      "}"
      "", /* E_VAR      */
      "", /* E_FUNC_DEF */
      ""  /* E_NEG      */
    };
  unsigned long i;

  for (i = 0; i < (sizeof (tags) / sizeof (char *)) - 3; i++)
    {
      if (!strcmp (tags[i], token))
	return i;
    }
  
  return E_VAR;
}
