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

/* Data in FACT is either a variable or a scope. */
typedef enum 
  {
    VAR_TYPE,
    SCOPE_TYPE,
    UNSET_TYPE  /* Default type for registers. */
  } FACT_type;

/* The FACT_var structure expresses variables.
 * What probably would be a much better method would be to make
 * array_up a dynamic array and eliminate next.
 */
typedef struct FACT_var
{
  bool locked;                /* Locked variables cannot be modified. */
  mpz_t array_size;           /* Size of the current dimension.       */
  mpc_t value;                /* value held by the variable.          */
  char *name;                 /* Name of the variable.                */
  struct FACT_var **array_up; /* Points to the next dimension.        */
} *FACT_var_t;

/* The FACT_scope structure expresses scopes and functions. */ 
typedef struct FACT_scope
{
  bool *marked;        /* Prevents loops in variable searches. */
  mpz_t *array_size;   /* Size of the current dimension.       */
  unsigned long *code; /* Location of the function's body.     */

  char *name;            /* Declared name of the scope.              */
  const char *file_name; /* The file name when the function was set. */

  FACT_var_t **var_stack;           /* Variables declared in the scope. */
  struct FACT_scope ***scope_stack; /* Scopes declared in the scope.    */
  unsigned long *var_stack_size;
  unsigned long *scope_stack_size;

  void (*extrn_func)(void); /* Used with external libraries. */

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

/* FACT_error describes a thrown error. */
typedef struct FACT_error
{
  unsigned long line;       /* Line the error occurred.          */
  const char *what;         /* Description of the error.         */
  struct FACT_scope *where; /* Scope where the error was thrown. */
} FACT_error_t;

/* FACT_t is the "ambigious" structure, and is type used to pass either a
 * scope or variable arbitrarily across internal functions. It's really
 * annoying casting ap every time I want to use it. Perhaps use a union
 * instead.
 */
typedef struct
{
  FACT_type type; /* Type of the passed data.      */
  void *ap;       /* Casted var or scope pointer.  */
} FACT_t;

#endif /* FACT_TYPES_H_ */
