#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <uuid/uuid.h>
#include <cstring>  // For memset
#include <dirent.h>

#include <string>
#include <vector>
#include <iostream>
#include <sstream>
#include <fstream>

///////////////////////////////////////////////////////////////////////////////

#define BUF 1024
#define PORT 6543
#define USER_LENGTH 8
#define PASSWORD_LENGTH 80
#define SUBJECT_LENGTH 80
#define SUBJECT_BUFFER_LENGTH 89
#define RECV_DIR 16

using namespace std;

///////////////////////////////////////////////////////////////////////////////

void clientCommunication(void *data);
void signalHandler(int sig);
bool login(int *current_socket, string &username, string baseDirectory);
void send(int* current_socket, string username, string baseDirectory);
void list(int* current_socket, string username, string baseDirectory);
void read(int* current_socket, string username, string baseDirectory);
void del(int* current_socket, string username, string baseDirectory);
void respond(int *current_socket, string response);
string findFile(int *current_socket, string path, int position);
void createDirIfNotCreated(string username, string baseDirectory);