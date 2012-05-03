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

void
FACT_assembler (char *input) /* Convert BAS into bytecode. */
{
  size_t i, j;
  unsigned int seg_addr;
  char *instr;
  char buff[MAX_INSTRUCTION_LEN + 1];

  instr = NULL;
  for (i = 0; *input != '\0'; input++)
    {
      instr = FACT_realloc (instr, sizeof (char) * (i + 1));

      /* TODO: add argument checking. */
      if (*input == '$') /* Register. One byte address. */
	{
	  instr[i] = 0;
	  if (isalpha ((unsigned char) *++input))
	    {
	      /* Check for named registers. Maximum length for a register is
	       * three characters long.
	       */
	      for (j = 0; j < 3; j++)
		{
		  if (*input == ',' || isspace ((unsigned char) *input) || *input == '\0')
		    break;
		  buff[j] = tolower ((unsigned char) *input);
		  input++;
		}
	      buff[j] = '\0';

	      switch (buff[0])
		/* Switch for index, then use strcmp to check for named
		 * registers.
		 */
		{
		case 'i':
		  instr[i] = R_I;
		  break;

		case 'j':
		  instr[i] = R_J;
		  break;

		case 'k':
		  instr[i] = R_K;
		  break;

		default:
		  if (!strcmp (buff, "pop"))
		    instr[i] = R_POP;
		  else if (!strcmp (buff, "top"))
		    instr[i] = R_TOP;
		  else if (!strcmp (buff, "tid"))
		    instr[i] = R_TID;
		  else
		    {
		      fprintf (stderr, "Warning; unknown named register, defaulting to 0.\n");
		      instr[i] = 0;
		    }
		}
	    }
	  else
	    {
	      for (; isdigit ((unsigned char) *input); input++)
		instr[i] = instr[i] * 10 + (*input - '0');
	      if (*input != ',' && !isspace (*input) && *input != '\0')
		fprintf (stderr, "Warning; register value contains non-numerical characters.\n");
	    }
	}
      else if (*input == '@') /* Segment address. Four bytes long. */
	{
	  seg_addr = 0;
	  while (isdigit (*++input))
	    seg_addr = seg_addr * 10 + (*input - '0');
	  if (*input != ',' && !isspace (*input) && *input != '\0')
	    fprintf (stderr, "Warning; segment address contains non-numerical characters.\n");
	  instr = FACT_realloc (instr, sizeof (char) * (j + 4));
	  instr[i++] = (seg_addr >> 24) & 0xFF;
	  instr[i++] = (seg_addr >> 16) & 0xFF;
	  instr[i++] = (seg_addr >> 8) & 0xFF;
	  instr[i] = seg_addr & 0xFF;
	}
      else if (*input == '%')
	/* Constant value. N-length null terminated string */
	{
	  /* TODO: fix for strings. */
	  for (input++; *input != ',' && !isspace (*input) && *input != '\0'; input++)
	    {
	      instr[i++] = *input;
	      instr = FACT_realloc (instr, sizeof (char) * (i + 1));
	    }
	  instr[i] = '\0';
	}
      else if (*input == '#') /* Comment. */
	{
	  while (*input != '\n' && *input != '\0')
	    input++;
	  if (*input == '\0')
	    input--;
	}
      else /* Instruction. */
	{
	  /* TODO: Fix me. */
	  for (j = 0; j < MAX_INSTRUCTION_LEN; j++)
	    {
	      if (*input == ',' || isspace ((unsigned char) *input)
		  || *input == '\0')
		break;
	      buff[j] = tolower ((unsigned char) *input);
	      input++;
	    }
	  buff[j] = '\0';

	  /* Replace this with binary search. */ 
	  for (j = 0; j < NUM_FURLOW_INSTRUCTIONS; j++)
	    {
	      if (!strcmp (Furlow_instructions[j].token, buff))
		break;
	    }
	  if (j == (sizeof (Furlow_instructions) / sizeof (Furlow_instructions[0])))
	    {
	      fprintf (stderr, "Invalid instruction %s, defaulting to HALT.\n", buff);
	      instr[i] = HALT;
	    }
	  else
	    instr[i] = Furlow_instructions[j].opcode;
	}

      while (*input != ',' && !isspace ((unsigned char) *input)
	     && *input != '\0')
	++input;

      if (*input == ',')
	i++;
      else if (*input == '\0')
	{
	  Furlow_add_instruction (instr);
	  break;
	}
      else
	{
	  i = 0;
	  Furlow_add_instruction (instr);
	  instr = NULL;
	}
    }
}