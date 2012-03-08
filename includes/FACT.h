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

#ifndef FACT_H_
#define FACT_H_

#define SAFE                 /* Use extra caution.               */
#define DEBUG                /* Print extra debug info.          */
#define USE_GC               /* Use the BDW garbage collector.   */
#define USE_ATOMIC           /* Improves speed a small amount.   */
// #define VM_DEBUG             /* Print out extra debug data.      */
#define FACT_VERSION "0.0.1" /* Furlow VM version number.        */
#define FACT_STDLIB_PATH "/usr/share/FACT/FACT_stdlib.ft"

#ifdef USE_GC
# define GC_THREADS
# include <gc/gc.h>
#endif

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <setjmp.h>
#include <assert.h>
#include <stdarg.h>
#include <signal.h>
#include <limits.h>
#include <pthread.h>
#include <math.h>

#include <gmp.h>
#include <dlfcn.h>
#include <pthread.h>
#include <histedit.h>

/* Source header files. */
#include <FACT_mpc.h>
#include <FACT_types.h>
#include <FACT_error.h>   /* Error handling.                 */
#include <FACT_lexer.h>   /* Lexer functions.                */
#include <FACT_parser.h>  /* Parser of the FACT grammar.     */
#include <FACT_comp.h>    /* Bytecode compiling and parsing. */
#include <FACT_signals.h> /* Interrupt/signal handling.      */
#include <FACT_file.h>    /* File interface.                 */
#include <FACT_alloc.h>   /* Memory allocation.              */
#include <FACT_strs.h>    /* String utility functions.       */               
#include <FACT_opcodes.h> /* Instruction listings.           */
#include <FACT_scope.h>   /* Scope handling routines.        */
#include <FACT_num.h>     /* Number handling routines.       */
#include <FACT_var.h>     /* Variable look-ups.              */
#include <FACT_vm.h>      /* Virtual machine functions.      */
#include <FACT_threads.h> /* Thread handling.                */
#include <FACT_basm.h>    /* Bytecode assembler routines.    */
#include <FACT_libs.h>    /* Module loading.                 */
#include <FACT_BIFs.h>    /* Built In Functions.             */
#include <FACT_debug.h>   /* Print debug information.        */ 
#include <FACT_shell.h>   /* Terminal interface.             */

#endif /* FACT_H_ */
