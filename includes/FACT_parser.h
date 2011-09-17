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

#ifndef FACT_PARSER_H_
#define FACT_PARSER_H_

typedef struct FACT_tree
{
  FACT_token_t id;               /* Token of the current node.                     */
  struct FACT_tree *next;        /* Used by stmt_list.                             */
  struct FACT_tree *children[4]; /* Each node can have a maximum of four children. */
} *FACT_tree_t;

/* Add more functions to handle errors. */
FACT_tree_t FACT_parse (FACT_lexed_t); /* Parse a set of tokens. Thread safe. */

#endif /* FACT_PARSER_H_ */
