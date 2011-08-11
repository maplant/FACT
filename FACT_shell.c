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

static void sh_help ();

/* Shell commands. All are preceded by a ':' when entered. */
static struct
{
  const char *name;
  void (*func)(void);
} shell_commands[] =
  {
    {
      "help" ,
      &sh_help
    },
    {
      "registers",
      &Furlow_print_registers
    },
    {
      "state",
      &Furlow_print_state
    },
  };

#define NUM_COMMANDS sizeof (shell_commands) / sizeof (shell_commands[0]) 

static void
sh_help () /* Print out a list of shell comands. */
{
  printf (":help      Show a list of available commands.\n"
	  ":registers Print the values of the VM's reigsters.\n"
	  ":state     Print the VM's current state.\n");
}

static char *
readline () /* Read a single line of input from stdin. */
{
  int c;
  char *res;
  unsigned long i;

  /* Perhaps replace this with some ncurses or termios routine,
   * like readline.
   */
  res = NULL;
  for (i = 0; (c = getchar ()) != EOF && c != '\n'; i++)
    {
      if (c == '\\')
	{
	  c = getchar ();
	  if (c != '\n')
	    ungetc (c, stdin);
	  else
	    {
	      i--;
	      continue;
	    }
	}
      
      /* Break on newline. */
      if (c == '\n')
	break;
      
      /* Skip all repeated spaces. */
      if (c == ' ')
	{
	  while ((c = getchar ()) == ' ');
	  if (c == '\n' || c == EOF)
	    goto end;
	  ungetc (c, stdin);
	  c = ' ';
	}
      
      res = FACT_realloc (res, sizeof (char) * (i + 1));
      res[i] = c;
    }

 end:
  if (res != NULL)
    {
      /* Add the null terminator. */
      res = FACT_realloc (res, sizeof (char) * (i + 1));
      res[i] = '\0';
    }

  return res;
}

void
FACT_shell (void)
{
  unsigned long i;
  char *input;

  /* Print shell info and initialize the VM. Eventually these should be moved
   * to the main function.
   */
  printf ("Furlow VM version %s\n", FACT_VERSION);
  Furlow_init_vm ();
  FACT_init_interrupt ();
  FACT_add_BIFs (CURR_THIS);

  if (setjmp (recover))
    {
      printf ("There was an error: %s\n", curr_thread->curr_err.what);
      return;
    }

  for (;;)
    {
      /* Print the prompt: */
      printf ("BAS %lu> ", CURR_IP);
      input = readline ();

      if (input == NULL)
	break;

      /* If it's a shell command, run the cmomand. */
      if (input[0] == ':')
	{
	  /* TODO: add bin search here. */
	  for (i = 0; i < NUM_COMMANDS; i++)
	    {
	      if (!strcmp (shell_commands[i].name, input + 1))
		break;
	    }
	  if (i == NUM_COMMANDS)
	    fprintf (stderr, "No command of name %s, try :help.\n", input + 1);
	  else
	    shell_commands[i].func ();
	  continue;
	}

      /* Assemble the code. */
      FACT_assembler (input);

      /* Run the code. */
      Furlow_run ();
    }
}

/* For testing. To be replaced. */
main ()
{
  FACT_shell ();
  return 0;
}
