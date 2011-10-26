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
    ADD = 0, /* Addition.                                      */
    AND,     /* Bitwise AND.                                   */
    APPEND,  /* Append a variable to another.                  */
    CALL,    /* Push to the call stack and jump to a function. */
    CEQ,     /* Equal.                                         */
    CLE,     /* Less than, equal.                              */
    CLT,     /* Less than.                                     */
    CME,     /* More than, equal.                              */
    CMT,     /* More than.                                     */
    CNE,     /* Not equal.                                     */
    CONST,   /* Retrieve a constant value and push it.         */
    DEC,     /* Decrement a register by 1.                     */
    DEF_N,   /* Define a new number in the this scope.         */
    DEF_S,   /* Define a new scope in the this scope.          */
    DIV,     /* Division.                                      */
    DROP,    /* Drop the first item on the var stack.          */
    DUP,     /* Duplicate the first element on the var stack.  */ 
    ELEM,    /* Get the element of an array.                   */
    EXIT,    /* Like ret, except the ip is left unchanged.     */
    GOTO,    /* Jump to a function but do not push.            */
    GROUP,   /* Group elements on the var stack into an array. */
    HALT,    /* Halt execution.                                */
    INC,     /* Increment a register by 1.                     */
    IOR,     /* Bitwise inclusive OR.                          */
    IS_AUTO, /* Checks if a variable is defined locally.       */
    IS_DEF,  /* Checks if a variable is defined globablly.     */
    JMP,     /* Unconditional jump.                            */
    JMP_PNT, /* Push a scope with an address to the var stack. */
    JIF,     /* Jump on false.                                 */
    JIN,     /* Jump on type `number'.                         */
    JIS,     /* Jump on type `scope'.                          */
    JIT,     /* Jump on true.                                  */
    LAMBDA,  /* Push a lambda scope to the stack.              */
    MOD,     /* Modulo.                                        */
    MUL,     /* Multiplication.                                */
    NEG,     /* Negative.                                      */    
    NEW_N,   /* Allocate a num and push it to the var stack.   */
    NEW_S,   /* Allocate a scope and push it to the var stack. */
    NOP,     /* No operator.                                   */
    PURGE,   /* Remove all items from the var stack.           */
    REF,     /* Create a reference.                            */
    RET,     /* Pop the call stack.                            */
    SET_C,   /* Set the jump address of a function.            */
    SET_F,   /* Set the function data of a scope.              */
    SET_N,   /* Set the name of a scope.                       */    
    SPRT,    /* Create a new thread and unconditionally jump.  */    
    STO,     /* Copy one var to the other.                     */
    SUB,     /* Subraction.                                    */    
    SWAP,    /* Swap the first two elements on the var stack.  */
    THIS,    /* Push the this scope to the variable stack.     */
    TRAP_B,  /* Push to the trap stack.                        */
    TRAP_E,  /* Pop the trap stack.                            */
    USE,     /* Push to the call stack.                        */
    VAR,     /* Retrieve and push a variable to the stack.     */
    XOR,     /* Bitwise exclusive OR.                          */
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
    { "add"     , ADD     , "rrr" },
    { "append"  , APPEND  , "rr"  },
    { "call"    , CALL    , "r"   },
    { "ceq"     , CEQ     , "rrr" },
    { "cle"     , CLE     , "rrr" },
    { "clt"     , CLT     , "rrr" },
    { "cme"     , CME     , "rrr" },
    { "cmt"     , CMT     , "rrr" },
    { "cne"     , CNE     , "rrr" },
    { "const"   , CONST   , "s"   },
    { "dec"     , DEC     , "r"   },
    { "def_n"   , DEF_N   , "rs"  },
    { "def_s"   , DEF_S   , "rs"  },
    { "div"     , DIV     , "rrr" },
    { "drop"    , DROP    , ""    },
    { "dup"     , DUP     , ""    },
    { "elem"    , ELEM    , "rr"  },
    { "exit"    , EXIT    , ""    },
    { "goto"    , GOTO    , "r"   },
    { "halt"    , HALT    , ""    },
    { "inc"     , INC     , "r"   },
    { "is_auto" , IS_AUTO , "s"   },
    { "is_def"  , IS_DEF  , "s"   },
    { "jmp"     , JMP     , "a"   },
    { "jmp_pnt" , JMP_PNT , "a"   },
    { "jif"     , JIF     , "ra"  },
    { "jit"     , JIT     , "ra"  },
    { "lambda"  , LAMBDA  , ""    },
    { "mod"     , MOD     , "rrr" },
    { "mul"     , MUL     , "rrr" },
    { "neg"     , NEG     , "r"   },
    { "new_n"   , NEW_N   , "r"   },
    { "new_s"   , NEW_S   , "r"   },
    { "purge"   , PURGE   , ""    },
    { "ref"     , REF     , "rr"  },
    { "ret"     , RET     , ""    },
    { "set_c"   , SET_C   , "ra"  },
    { "set_f"   , SET_F   , "rr"  },
    { "set_n"   , SET_N   , "rr"  },
    { "sprt"    , SPRT    , "a"   },
    { "sto"     , STO     , "rr"  },
    { "sub"     , SUB     , "rrr" },
    { "swap"    , SWAP    , ""    },
    { "this"    , THIS    , ""    },
    { "trap_b"  , TRAP_B  , "a"   },
    { "trap_e"  , TRAP_E  , ""    },
    { "use"     , USE     , "r"   },
    { "var"     , VAR     , "s"   },
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
