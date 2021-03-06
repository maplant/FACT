/* This file is part of FACT.
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

#include "FACT.h"
#include "FACT_mpc.h"

/* Data in FACT is either a number or a scope. */
typedef enum {
  UNSET_TYPE = -1,  /* Default type for registers. */
  NUM_TYPE   = 0,
  SCOPE_TYPE = 1
} FACT_type;

typedef struct _var_table FACT_table_t;

/* FACT_t is the "ambigious" structure, and is used to pass either a
 * scope or number arbitrarily across internal functions. It's really
 * annoying casting ap every time I want to use it. Perhaps use a union
 * instead.
 */
typedef struct {
  void *ap;       /* Casted num or scope pointer.  */
  FACT_table_t *home; /* For unset types. */
  FACT_type type; /* Type of the passed data.      */
} FACT_t;

/* The FACT_num structure expresses real numbers. */
typedef struct FACT_num {
  bool locked;                /* Locked variables are immutable.      */
  mpc_t value;                /* value held by the variable.          */
  char *name;                 /* Name of the variable.                */
  size_t array_size;          /* Size of the current dimension.       */
  struct FACT_num **array_up; /* Points to the next dimension.        */
} *FACT_num_t;

/* The FACT_scope structure expresses scopes and functions. */ 
typedef struct FACT_scope {
  bool *marked;                  /* Prevents loops in variable searches.   */
  enum {
    UNLOCKED,                    /* No restrictions on variable.           */
    HARD_LOCK,                   /* All lock restrictions apply.           */
    SOFT_LOCK,                   /* Most lock restrictions apply.          */
  } lock_stat;
  size_t *array_size;            /* Size of the current dimension.         */
  size_t *code;                  /* Location of the function's body.       */
  char *name;                    /* Declared name of the scope.            */
  FACT_table_t *vars;            /* Table of declared variables.           */
  void (*extrn_func)(void);      /* Used with external libraries and BIFs. */
  struct FACT_scope *up;         /* Points to the next scope up.           */
  struct FACT_scope *caller;     /* Points to the calling function.        */
  struct FACT_scope ***array_up; /* The next dimension up.                 */
  struct FACT_va_list {          /* Variadic argument list.                */
    FACT_t var;                  /* Node value.                            */
    struct FACT_va_list *next;   /* Next argument in the list.             */
  } *variadic;
} *FACT_scope_t;

/* For casting generic types. */
#define FACT_cast_to_num(v)   ((FACT_num_t)   (v).ap)
#define FACT_cast_to_scope(v) ((FACT_scope_t) (v).ap)

#define DEF_ERR_MSG "no error"

/* FACT_error describes a thrown error. */
typedef struct FACT_error {
  size_t line;              /* Line the error occurred.          */
  const char *what;         /* Description of the error.         */
  struct FACT_scope *where; /* Scope where the error was thrown. */
} FACT_error_t;

#endif /* FACT_TYPES_H_ */
