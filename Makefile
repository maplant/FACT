CC = gcc
LIBS = -lgmp
PROG = Furlow
CFLAGS = -c -g3 -I$(INCLUDE_DIR) # -Wall
LDFLAGS = -rdynamic
INCLUDE_DIR = ./includes
SRCS =	FACT_alloc.c FACT_shell.c FACT_basm.c FACT_vm.c  FACT_mpc.c    \
	FACT_num.c FACT_scope.c FACT_error.c FACT_BIFs.c FACT_debug.c   \
	FACT_signals.c FACT_lexer.c FACT_var.c FACT_parser.c FACT_comp.c \

OBJS = $(SRCS:.c=.o)

all: $(SRCS) $(PROG)

$(PROG):	$(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) $(LIBS) -o $@

.c.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm *.o
	rm $(PROG)