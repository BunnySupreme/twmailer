#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include <string>
#include <iostream>
#include <vector>

///////////////////////////////////////////////////////////////////////////////

#define BUF 1024
#define USER_LENGTH 8
#define PASSWORD_LENGTH 80
#define SUBJECT_LENGTH 80
#define PORT 6543

using namespace std;

///////////////////////////////////////////////////////////////////////////////

void checkCommand(string &message);
