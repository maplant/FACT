CC = gcc
LIBS = -lgmp -lgc -ledit -ltermcap -lpthread -lm -Wall
PROG = FACT
CFLAGS = -c -g3
LDFLAGS = 
INSTALL_DIR = /usr/bin
SRCS =	FACT_alloc.c FACT_shell.c FACT_vm.c  FACT_mpc.c    \
	FACT_num.c FACT_scope.c FACT_error.c FACT_BIFs.c   \
	FACT_signals.c FACT_lexer.c FACT_var.c FACT_parser.c FACT_comp.c \
	FACT_file.c FACT_strs.c FACT_main.c FACT_threads.c FACT_hash.c

OBJS = $(SRCS:.c=.o)

all: $(SRCS) $(PROG)

$(PROG):	$(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) $(LIBS) -o $@

.c.o:
	$(CC) $(CFLAGS) $< -o $@

install:
	rm -f $(INSTALL_DIR)/$(PROG)      ; \
	cp $(PROG) $(INSTALL_DIR)         ; \
	rm -rf /usr/share/FACT            ; \
	mkdir /usr/share/FACT             ; \
	cp FACT_stdlib.ft /usr/share/FACT ; \
	chmod 644 /usr/share/FACT/FACT_stdlib.ft

check-syntax:
	 cc $(CFLAGS) -S ${CHK_SOURCES}

#install-devel:
#	rm -rf /usr/include/FACT
#	mkdir /usr/include/FACT
#	cp -r API_includes/. /usr/include/FACT 

clean:
	rm *.o     ; \
	rm $(PROG)
