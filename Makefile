
CEXTRA= -fprofile-arcs -ftest-coverage #-Werror
CCFLAGS=-std=gnu99 -Wall -Wextra -pedantic -g -ggdb -DTEST -fPIC $(CEXTRA)
LDFLAGS=-lelf -ldl -Wl,-rpath,.,--no-undefined -lmpfr -lgmp -lsexp -rdynamic

INCLUDES=-I . -I/repos/butcher -I /usr/include/sexpr
CC=gcc
USLDFLAGS=-L . -lunitsystem
BUTCHER=/repos/butcher/butcher -b /repos/butcher/bexec 
BFLAGS=-n

.PHONY: all
all: libnih.so

blobtree.o: blobtree.c blobtree.h /repos/butcher/bt.h
		$(CC) $(CCFLAGS) $(INCLUDES) -c blobtree.c -o blobtree.o

libnih.so: blobtree.o
		$(CC) $(CCFLAGS) $(INCLUDES) -o libnih.so blobtree.o -shared $(LDFLAGS)

chop: all
		$(BUTCHER) $(BFLAGS) libnih.so

cov: chop
		gcov blobtree.c

.PHONY: clean
clean:
		rm *.o *.so crap *.gcov *.gcda *.gcno -f
