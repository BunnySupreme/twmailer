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
#############################################################################################

# Compiler Flags: These are only for compiling and do not include -luuid for the linker.
CFLAGS=-g -Wall -Wextra -Werror -O -std=c++17 -pthread

# Linker flags: Use this only when linking the final binary.
LDFLAGS=-luuid

rebuild: clean all
all: ./bin/server ./bin/client

clean:
	clear
	rm -f bin/* obj/*

./obj/twmailer-client.o: twmailer-client.cpp
	${CC} ${CFLAGS} -o obj/twmailer-client.o twmailer-client.cpp -c

./obj/twmailer-server.o: twmailer-server.cpp
	${CC} ${CFLAGS} -o obj/twmailer-server.o twmailer-server.cpp -c 

./bin/server: ./obj/twmailer-server.o
	${CC} ${CFLAGS} -o bin/server obj/twmailer-server.o ${LDFLAGS}

./bin/client: ./obj/twmailer-client.o
	${CC} ${CFLAGS} -o bin/client obj/twmailer-client.o
