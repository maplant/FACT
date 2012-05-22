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

#ifndef FACT_LEXER_H_
#define FACT_LEXER_H_

#include "FACT.h"
#include "FACT_error.h"

#include <setjmp.h>

/* Operator type. */
typedef enum {
  E_NE = 0,
  E_DQ,
  E_THREAD,
  E_MOD,
  E_MOD_AS,
  E_BIT_AND,
  E_AND,
  E_BIT_AND_AS,
  E_SQ,
  E_OP_PAREN,
  E_CL_PAREN,
  E_MUL,
  E_MUL_AS,
  E_ADD,
  E_ADD_AS,
  E_COMMA,
  E_SUB,
  E_SUB_AS,
  E_DIV,
  E_DIV_AS,
  E_IN,
  E_SEMI,
  E_LT,
  E_LE,
  E_SET,
  E_EQ,
  E_MT,
  E_ME,
  E_LOCAL_CHECK,
  E_GLOBAL_CHECK,
  E_OP_BRACK,
  E_CL_BRACK,
  E_BIT_XOR,
  E_BIT_XOR_AS,
  E_FUNC_DEF,
  E_BREAK,
  E_CATCH,
  E_CONST,
  E_DEFUNC,
  E_ELSE,
  E_FOR,
  E_GIVE,
  E_HANDLE,
  E_IF,
  E_NUM_DEF,
  E_RETURN,
  E_SCOPE_DEF,
  E_WHILE,
  E_OP_CURL,
  E_BIT_IOR,
  E_BIT_IOR_AS,
  E_OR,
  E_CL_CURL,
  E_VAR,
  E_NUM,
  E_FUNC_CALL,
  E_NEG,
  E_ARRAY_ELEM,
  E_STR_CONST,
  E_END,
} FACT_nterm_t; 

/* FACT_token_t - represents a single terminal. */
typedef struct {
  char *lexem;     /* Actual token's string.            */
  size_t lines;    /* Number of lines before the token. */
  FACT_nterm_t id; /* Type of the token.                */
} FACT_token_t;

/* FACT_lexed_t - set of tokens (represented by FACT_token struct) and other information
 * used by the parser.
 */
typedef struct {
  FACT_token_t *tokens;      /* Set of tokens.                               */
  jmp_buf handle_err;        /* For parser error handling.                   */
  size_t curr;               /* Current one being analyzed.                  */
  size_t line;               /* The current line number, for error handling. */
  char err[MAX_ERR_LEN + 1]; /* Error message, if there is one.              */
} FACT_lexed_t;

FACT_lexed_t FACT_lex_string (char *);     /* The lexer.                   */
const char *FACT_get_lexem (FACT_nterm_t); /* Returns a string from an ID. */

#endif /* FACT_LEXER_H_ */
