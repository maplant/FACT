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

#ifndef FACT_TYPES_H_
#define FACT_TYPES_H_

/* Data in FACT is either a number or a scope. */
typedef enum 
  {
    NUM_TYPE,
    SCOPE_TYPE,
    UNSET_TYPE  /* Default type for registers. */
  } FACT_type;

/* FACT_t is the "ambigious" structure, and is type used to pass either a
 * scope or number arbitrarily across internal functions. It's really
 * annoying casting ap every time I want to use it. Perhaps use a union
 * instead.
 */
typedef struct
{
  FACT_type type; /* Type of the passed data.      */
  void *ap;       /* Casted num or scope pointer.  */
} FACT_t;

/* The FACT_num structure expresses real numbers. */
typedef struct FACT_num
{
  bool locked;                /* Locked variables are immutable.      */
  mpc_t value;                /* value held by the variable.          */
  char *name;                 /* Name of the variable.                */
  size_t array_size;          /* Size of the current dimension.       */
  struct FACT_num **array_up; /* Points to the next dimension.        */
} *FACT_num_t;

/* The FACT_scope structure expresses scopes and functions. */ 
typedef struct FACT_scope
{
  bool *marked;       /* Prevents loops in variable searches. */
  size_t *array_size; /* Size of the current dimension.       */
  size_t *code;       /* Location of the function's body.     */

  char *name; /* Declared name of the scope. */

  FACT_t **var_table; /* Variables declared in the scope. */
  size_t *num_vars;   /* Number of variables.             */

  void (*extrn_func)(void); /* Used with external libraries. */

  struct FACT_scope *up;         /* Points to the next scope up.    */
  struct FACT_scope *caller;     /* Points to the calling function. */
  struct FACT_scope ***array_up; /* The next dimension up.          */

  /* FACT_mixed expresses a variadic argument list. */
  struct FACT_mixed
  {
    FACT_type curr_type;     /* The current node's type.     */
    void *node_p;            /* Casted var or scope pointer. */ 
    struct FACT_mixed *next; /* Next argument in the list.   */
  } *variadic;
} *FACT_scope_t;

/* For casting generic types. */
#define FACT_cast_to_num(v)   ((FACT_num_t)   (v).ap)
#define FACT_cast_to_scope(v) ((FACT_scope_t) (v).ap)

#define DEF_ERR_MSG "no error"

/* FACT_error describes a thrown error. */
typedef struct FACT_error
{
  size_t line;              /* Line the error occurred.          */
  const char *what;         /* Description of the error.         */
  struct FACT_scope *where; /* Scope where the error was thrown. */
} FACT_error_t;

#endif /* FACT_TYPES_H_ */
