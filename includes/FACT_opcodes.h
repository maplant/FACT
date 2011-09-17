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

#ifndef FACT_OPCODES_H_
#define FACT_OPCODES_H_

/* Furlow VM bytecode instructions. */
typedef enum Furlow_opcode 
  {
    /* Memory management:                                       */
    REF = 0,  /* Create a reference.                            */
    STO,      /* Copy one var to the other.                     */
    VAR,      /* Retrieve and push a variable to the stack.     */
    CONST,    /* Retrieve a constant value and push it.         */
    NEW_N,    /* Allocate a num and push it to the var stack.   */
    NEW_S,    /* Allocate a scope and push it to the var stack. */
    ELEM,     /* Get the element of an array.                   */
    SWAP,     /* Swap the first two elements on the var stack.  */
    DROP,     /* Drop the first item on the stack.              */
    PURGE,    /* Remove all items from the stack.               */

    /* Register manipulation.                                   */
    INC,      /* Increment a register by 1.                     */
    DEC,      /* Decrement a register by 1.                     */
    
    /* Arithmetic:                                              */
    ADD,      /* Addition.                                      */
    SUB,      /* Subraction.                                    */
    MUL,      /* Multiplication.                                */
    DIV,      /* Division.                                      */
    MOD,      /* Modulo.                                        */
    XOR,      /* Bitwise exclusive OR.                          */
    IOR,      /* Bitwise inclusive OR.                          */
    AND,      /* Bitwise AND.                                   */
    NEG,      /* Negative.                                      */

    /* Boolean operations:                                      */
    CMT,      /* More than.                                     */
    CME,      /* More than, equal.                              */
    CLT,      /* Less than.                                     */
    CLE,      /* Less than, equal.                              */
    CEQ,      /* Equal.                                         */
    CNE,      /* Not equal.                                     */

    /* Branching and jumps:                                     */
    JMP,      /* Unconditional jump.                            */
    JIT,      /* Jump on true.                                  */
    JIF,      /* Jump on false.                                 */
    SPRT,     /* Create a new thread and unconditionally jump.  */

    /* Function and scope handling:                             */
    SET_C,    /* Set the jump address of a function.            */
    SET_F,    /* Set the function data of a scope.              */
    CALL,     /* Push to the call stack and jump to a function. */
    RET,      /* Pop the call stack.                            */
    USE,      /* Push to the call stack.                        */
    EXIT,     /* Like ret, except the ip is left unchanged.     */
    THIS,     /* Push the this scope to the variable stack.     */

    /* Variable handling:                                       */
    DEF_N,    /* Define a new number in the this scope.         */
    DEF_S,    /* Define a new scope in the this scope.          */

    /* Error handling:                                          */
    TRAP,     /* Push to the trap stack.                        */
    END_TRAP, /* Pop the trap stack.                            */
    ERR_D,    /* Push the description of curr_err to the stack. */
    ERR_S,    /* Push the scope of curr_err to the stack.       */

    /* General:                                                 */
    HALT,     /* Halt execution.                                */
    NEW_L,    /* Newline; used for error messages.              */
    NOP       /* No operator.                                   */
  } Furlow_opc_t;

static struct 
{
  const char *token;         /* String identifier of the instruction. */
  enum Furlow_opcode opcode; /* Integer opcode.                       */
  const char *args;          /* Type and number of arguments taken.
			      *  r = register (1 byte)
			      *  a = segment address (4 bytes)
			      *  s = string (n bytes null terminated)
			      */
} Furlow_instructions[] =
  {
    { "add"  , ADD  , "rrr" },
    { "call" , CALL , "r"   },
    { "ceq"  , CEQ  , "rrr" },
    { "cle"  , CLE  , "rrr" },
    { "clt"  , CLT  , "rrr" },
    { "cme"  , CME  , "rrr" },
    { "cmt"  , CMT  , "rrr" },
    { "cne"  , CNE  , "rrr" },
    { "const", CONST, "s"   },
    { "dec"  , DEC,   "r"   },
    { "def_n", DEF_N, "rs"  },
    { "def_s", DEF_S, "rs"  },
    { "div"  , DIV  , "rrr" },
    { "elem" , ELEM , "rr"  },
    { "exit" , EXIT , ""    },
    { "halt" , HALT , ""    },
    { "inc"  , INC  , "r"   },
    { "jmp"  , JMP  , "a"   },
    { "jif"  , JIF  , "ra"  },
    { "jit"  , JIT  , "ra"  },
    { "mul"  , MUL  , "rrr" },
    { "neg"  , NEG  , "r"   },
    { "new_n", NEW_N, "r"   },
    { "new_s", NEW_S, "r"   },
    { "purge", PURGE, ""    },
    { "ref"  , REF  , "rr"  },
    { "ret"  , RET  , ""    },
    { "set_c", SET_C, "ra"  },
    { "set_f", SET_F, "rr"  },
    { "sprt" , SPRT , "a"   },
    { "sto"  , STO  , "rr"  },
    { "sub"  , SUB  , "rrr" },
    { "swap" , SWAP , ""    },
    { "this" , THIS , ""    },
    { "use"  , USE  , "r"   },
    { "var"  , VAR  , "s"   },
  };

#define MAX_INSTRUCTION_LEN 10
#define NUM_FURLOW_INSTRUCTIONS (sizeof (Furlow_instructions) \
				 / sizeof (Furlow_instructions[0]))

static int
Furlow_get_instruction (char *id)
{
  int res;
  int low, high, mid;

  low = 0;
  high = NUM_FURLOW_INSTRUCTIONS - 1;

  while (high >= low)
    {
      mid = low + (high - low) / 2;
      res = strcmp (id, Furlow_instructions[mid].token);
      
      if (res < 0)
	high = mid - 1;
      else if (res > 0)
	low = mid + 1;
      else
	goto found;
    }

  /* No result found. */
  return -1;

 found:
  /* The instruction was found. */
  return mid;
}

#endif /* FACT_OPCODES_H_ */
