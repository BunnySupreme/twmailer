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
#include <ldap.h>

#include <queue>
#include <thread>
#include <condition_variable>
#include <atomic>
#include <string>
#include <vector>
#include <iostream>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <memory>
#include <map>

///////////////////////////////////////////////////////////////////////////////

#define BUF 1024
#define PORT 6543
#define USER_LENGTH 8
#define PASSWORD_LENGTH 80
#define SUBJECT_LENGTH 80
#define SUBJECT_BUFFER_LENGTH 89
#define RECV_DIR 16
#define BLACKLIST "blacklist.txt"

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
bool checkBlacklist(std::string username);
bool checkLdap(std::string username, std::string password);