#############################################################################################
# Makefile
#############################################################################################
# G++ is part of GCC (GNU compiler collection) and is a compiler best suited for C++
CC=g++

# Compiler Flags: https://linux.die.net/man/1/g++
#############################################################################################
# -g: produces debugging information (for gdb)
# -Wall: enables all the warnings
# -Wextra: further warnings
# -Werror: treat warnings as errors
# -O: Optimizer turned on
# -std: use the C++ 14 standard
# -c: says not to run the linker
# -pthread: Add support for multithreading using the POSIX threads library. This option sets 
#           flags for both the preprocessor and linker. It does not affect the thread safety 
#           of object code produced by the compiler or that of libraries supplied with it. 
#           These are HP-UX specific flags.
#############################################################################################
CFLAGS=-g -Wall -Wextra -Werror -O -std=c++14 -pthread

rebuild: clean all
all: twmailer-server twmailer-client

clean:
	clear
	rm -f bin/* obj/*

./obj/twmailer-client.o: twmailer-client.c
	${CC} ${CFLAGS} -o obj/twmailer-client.o twmailer-client.c -c

./obj/twmailer-server.o: twmailer-server.c
	${CC} ${CFLAGS} -o obj/twmailer-server.o twmailer-server.c -c 

server: ./obj/twmailer-server.o
	${CC} ${CFLAGS} -o server obj/twmailer-server.o

client: ./obj/twmailer-client.o
	${CC} ${CFLAGS} -o client obj/twmailer-client.o
