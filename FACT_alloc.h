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

#ifndef FACT_ALLOC_H_
#define FACT_ALLOC_H_

#include "FACT.h"

#ifdef VALGRIND_DEBUG
# include <stdlib.h>
# include <string.h>
static void *FACT_malloc (size_t s)
{
  void *t = malloc (s);
  memset (t, 0, s);
}
# define FACT_malloc_atomic malloc
# define FACT_realloc realloc
# define FACT_free free
# define FACT_GC() ((void) 0)
#else
# define GC_THREADS
# include <gc/gc.h>
inline void *FACT_malloc (size_t);
inline void *FACT_malloc_atomic (size_t);
inline void *FACT_realloc (void *, size_t);
inline void FACT_free (void *);
# define FACT_GC() GC_gcollect ()
#endif

typedef struct FACT_num *FACT_num_t;
typedef struct FACT_scope *FACT_scope_t;

FACT_num_t FACT_alloc_num (void);              /* Allocate a number.            */
FACT_num_t *FACT_alloc_num_array (size_t);     /* Allocate an array of numbers. */
FACT_scope_t FACT_alloc_scope (void);          /* Allocate a scope.             */
FACT_scope_t *FACT_alloc_scope_array (size_t); /* Allocate an array of scopes.  */

#endif
