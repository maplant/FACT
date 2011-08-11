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

/* Operator type. */
typedef enum
  {
    E_NQ,
    E_MOD,
    E_MOD_AS,
    E_BIT_AND,
    E_AND,
    E_BIT_AND_AS,
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
    E_LT,
    E_LE,
    E_SET,
    E_EQ,
    E_MT,
    E_ME,
    E_OP_BRACK,
    E_CL_BRACK,
    E_BIT_XOR,
    E_BIT_XOR_AS,
    E_BREAK,
    E_DEF,
    E_DEFUNC,
    E_ELSE,
    E_FOR,
    E_IF,
    E_RETURN,
    E_WHILE,
    E_OP_CURL,
    E_BIT_IOR,
    E_BIT_IOR_AS,
    E_OR,
    E_CL_CURL,
    E_VAR,
    E_FUNC_DEF,
    E_NEG,
  } FACT_nterm_t; 

/* FACT_exp - used by the lexer to divide input into tokens, and by the
 * parser to create an expression tree.
 */
typedef struct FACT_exp
{
  FACT_nterm_t id;               /* Type of the token.                */
  unsigned long lines;           /* Number of lines before the token. */
  char *token;                   /* Actual token's string.            */
  struct FACT_exp *left, *right; /* Left and right to the token.      */
  struct FACT_exp *spec;         /* Operator specific node.           */
} *FACT_exp_t;

FACT_exp_t FACT_lex_string (char *); /* The lexer. */
FACT_exp_t create_node (FACT_nterm_t, FACT_exp_t, FACT_exp_t);

#endif /* FACT_LEXER_H_ */
