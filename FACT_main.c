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

#include "FACT.h"
#include "FACT_shell.h"
#include "FACT_alloc.h"
#include "FACT_types.h"
#include "FACT_vm.h"
#include "FACT_file.h"
#include "FACT_error.h"
#include "FACT_opcodes.h"

#include <stdio.h>
#include <stdlib.h>
#include <gc/gc.h>

void *gmp_realloc_wrapper (void *op1, size_t uop, size_t op2)
{
  /* Ignore uop. */
  return FACT_realloc (op1, op2);
}

void gmp_free_wrapper (void *op1, size_t op2)
{
  /* Ignore op2. */
  FACT_free (op1);
}

void cleanup (void)
{
  GC_gcollect ();
}

int main (int argc, char **argv)
{
  int i, j;
  bool disasm;
  bool shell_on, load_stdlib;
  FACT_t res;
  unsigned long q_size;
  char *stdlib_path;
  char **file_queue; /* List of files to be run. */
  struct cstack_t frame;
  static char nop_inst[] = { NOP };

  struct {
    char short_opt, *long_opt;
  } flags[] = { /* List of valid flags. */
    { 's', "shell=yes"       }, /* 0 */
    { 'n', "shell=no"        }, /* 1 */
    {  0 , "load-stdlib=yes" }, /* 2 */
    {  0 , "load-stdlib=no"  }, /* 3 */
    { 'f', "file"            }, /* 4 */
    { 'h', "help"            }, /* 5 */
    { 'v', "version"         }, /* 6 */
    { 'd', "disasm"          }, /* 7 */
  };

  /* Set exit routines. */
  atexit (cleanup);

#ifndef VALGRIND_DEBUG
  /* Start the garbage collector */
  GC_INIT ();
  //  GC_set_warn_proc (&GC_ignore_warnings);
#endif

  /* Set the GMP memory allocation functions to the GC
   * allocation functions.
   */
  mp_set_memory_functions (&FACT_malloc,
			   &gmp_realloc_wrapper,
			   &gmp_free_wrapper);

  /* Initialize the virtual machine. */
  Furlow_init_vm();
  Furlow_add_instruction(nop_inst); /* In case there is no STDLIB. */
  FACT_init_interrupt();
  FACT_add_BIFs();

  q_size = 0;
  file_queue = NULL;

  /* Ignore the first argument. */
  argv++;
  argc--;

  /* Set the default values to shell_on, load_stdlib, and disasm. */
  disasm = false;
  load_stdlib = true;
  shell_on = ((argc == 0)
	      ? true
	      : false);

  for (i = 0; i < argc; i++) {
    int opt_t, flag;
      
    /* Check to see if it's a long or short option. */
    opt_t = ((argv[i][0] == '-')
	     ? ((argv[i][1] == '-')
		? 3  /* Long  */
		: 2) /* Short */
	     : 1);   /* File  */

    if (opt_t == 1) {
      /* Put the file in the queue. */
      file_queue = FACT_realloc (file_queue, sizeof (char *) * (q_size + 1));
      file_queue[q_size] = argv[i];
      q_size++;
      continue;
    } else if (opt_t == 2) {
      /* Short option. If the '-' is alone in the argument, skip
       * the rest of the command line.
       */
      if (argv[i][1] == '\0')
	break;

      /* Skip the '-' */
      argv[i]++;

      /* Get the flag. */
      flag = -1;
      for (j = 0; j < (sizeof (flags) / sizeof (flags[0])); j++) {
	if (flags[j].short_opt == argv[i][0]) {
	  flag = j;
	  break;
	}
      }

      /* If the flag was not found, print an error and exit. */
      if (flag == -1) {
	fprintf (stderr, "FACT: Invalid option -%c. Use -h or --help to show valid options.\n", argv[i][0]);
	goto exit;
      }
    } else { 
      /* Long option. */
      argv[i] += 2; /* Skip the '--' */

      /* If the argument is soley a '--', we set the flag equal
       * to 4 (file).
       */
      if (argv[i][0] == '\0')
	flag = 4;
      else {
	/* Get the flag. */
	flag = -1;
	for (j = 0; j < (sizeof (flags) / sizeof (flags[0])); j++) { 
	  if (!strcmp (flags[j].long_opt, argv[i])) {
	    flag = j;
	    break;
	  }
	}

	if (flag == -1) {
	  fprintf (stderr, "FACT: Invalid option --%s. Use -h or --help to show valid options.\n", argv[i]);
	  goto exit;
	}
      }
    }

    /* Evaluate the flag. */
    switch (flag) {
    case 0: /* shell=yes       */
      shell_on = true;
      break;

    case 1: /* shell=no        */
      shell_on = false;
      break;

    case 2: /* load-stdlib=yes */
      load_stdlib = true;
      break;

    case 3: /* load-stdlib=no  */
      load_stdlib = false;
      break;

    case 4: /* file            */
      /* Add the file to the queue. If no file is provided, throw
       * an error and exit.
       */
      if (argv[i + 1] == NULL) {
	fprintf (stderr, "FACT: No file provided.\n");
	goto exit;
      }
	  
      /* Put the file in the queue. */
      file_queue = FACT_realloc (file_queue, sizeof (char *) * (q_size + 1));
      file_queue[q_size] = argv[i + 1];
      q_size++;
      i++;
      continue;

    case 5: /* help            */
      /* Print help message and exit. */
      printf ("usage: FACT -snhvf [ long options ] [ files ]\n"
	      "Options and their arguments:\n"
	      "-s          : force the FACT shell (default behaviour with no arguments)\n"
	      "-n          : force FACT to not enter interactive mode\n"
	      "-h          : print this message and exit\n"
	      "-v          : print the version number and exit\n"
	      "-f [ file ] : run the specified file\n"
	      "Long options:\n"
	      "--file                 : analagous to -f\n"
	      "--shell=<yes|no>       : force the shell to enter or not to enter.\n"
	      "--load-stdlib=<yes|no> : force the loading or the ignoring of the FACT standard library.\n"
	      "--help                 : analagous to -h\n"
	      "--version              : analagous to -v\n");
      if (opt_t != 2 || argv[i][1] == '\0')
	goto exit;
      break;

    case 6: /* version         */
      /* Print version and exit. */
      printf ("FACT version %s compiled on %s with %s\n",
	      FACT_VERSION, __TIMESTAMP__, __VERSION__);
      if (opt_t != 2 || argv[i][1] == '\0')
	goto exit;
      break;

    case 7:
      disasm = true;
      break;

    default: /* DOESNOTREACH   */
      abort ();
      break;
    }

    if (opt_t == 2 && argv[i][1] != '\0') {
      /* If it's a short option in the form of -abcd..., that is, there
       * are multiple flags following the '-', we set current flag to
       * '-' so that it will be skipped over next iteration.
       */
      argv[i][0] = '-';
      i--; /* So that we evaluate the command again. */
    }
  }

  /* Set the error handler before running any files. */
  if (setjmp (recover)) {
    /* Print out the error and a stack trace. */
    fprintf (stderr, "Caught unhandled error: %s\n", curr_thread->curr_err.what);
    while (curr_thread->cstackp - curr_thread->cstack >= 0) {
      frame = pop_c ();
      if (FACT_is_BIF (frame.this->extrn_func))
	fprintf (stderr, "\tat built-in function %s\n", frame.this->name);
      else
	fprintf (stderr, "\tat scope %s (%s:%zu)\n", frame.this->name, FACT_get_file (frame.ip), FACT_get_line (frame.ip));
    }
    /* Exit unsuccessully. */
    exit (1);
  }
  
  /* Run the standard library, if it's desired. */
  if (load_stdlib) {
    /* Get the FACTPATH environmental variable. */
    stdlib_path = getenv ("FACTPATH");
    if (FACT_load_file ((stdlib_path == NULL) ? FACT_STDLIB_PATH : stdlib_path) == -1)
      goto exit;
  }

  /* Go through every file in the queue and run them. */
  for (i = 0; i < q_size; i++) {
    if (FACT_load_file (file_queue[i]) == -1)
      goto exit; /* There was an error. */
  }

  if (shell_on) {
    if (load_stdlib || q_size > 0)
      Furlow_run ();
    FACT_shell ();
  } else      
    Furlow_run ();

  if (disasm)
    Furlow_disassemble();
  
 exit:
  /* Clean up and exit. */
  if (file_queue != NULL)
    FACT_free (file_queue);
  
  exit (0);
}
