#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>

///////////////////////////////////////////////////////////////////////////////

#define BUF 1024
#define PORT 6543
#define USER_LENGTH 8
#define PASSWORD_LENGTH 80
#define SUBJECT_LENGTH 80

///////////////////////////////////////////////////////////////////////////////

int abortRequested = 0;
int create_socket = -1;
int new_socket = -1;

typedef enum {
    LOGIN,
    SEND,
    LIST,
    READ,
    DEL,
    QUIT,
    INVALID_COMMAND
} CommandType;

///////////////////////////////////////////////////////////////////////////////

void *clientCommunication(void *data);
void signalHandler(int sig);
CommandType filterCommandType(const char *firstLine);

///////////////////////////////////////////////////////////////////////////////

int main(void)
{
   socklen_t addrlen;
   struct sockaddr_in address, cliaddress;
   int reuseValue = 1;

   ////////////////////////////////////////////////////////////////////////////
   // SIGNAL HANDLER
   // SIGINT (Interrup: ctrl+c)
   // https://man7.org/linux/man-pages/man2/signal.2.html
   if (signal(SIGINT, signalHandler) == SIG_ERR)
   {
      perror("signal can not be registered");
      return EXIT_FAILURE;
   }

   ////////////////////////////////////////////////////////////////////////////
   // CREATE A SOCKET
   // https://man7.org/linux/man-pages/man2/socket.2.html
   // https://man7.org/linux/man-pages/man7/ip.7.html
   // https://man7.org/linux/man-pages/man7/tcp.7.html
   // IPv4, TCP (connection oriented), IP (same as client)
   if ((create_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1)
   {
      perror("Socket error"); // errno set by socket()
      return EXIT_FAILURE;
   }

   ////////////////////////////////////////////////////////////////////////////
   // SET SOCKET OPTIONS
   // https://man7.org/linux/man-pages/man2/setsockopt.2.html
   // https://man7.org/linux/man-pages/man7/socket.7.html
   // socket, level, optname, optvalue, optlen
   if (setsockopt(create_socket,
                  SOL_SOCKET,
                  SO_REUSEADDR,
                  &reuseValue,
                  sizeof(reuseValue)) == -1)
   {
      perror("set socket options - reuseAddr");
      return EXIT_FAILURE;
   }

   if (setsockopt(create_socket,
                  SOL_SOCKET,
                  SO_REUSEPORT,
                  &reuseValue,
                  sizeof(reuseValue)) == -1)
   {
      perror("set socket options - reusePort");
      return EXIT_FAILURE;
   }

   ////////////////////////////////////////////////////////////////////////////
   // INIT ADDRESS
   // Attention: network byte order => big endian
   memset(&address, 0, sizeof(address));
   address.sin_family = AF_INET;
   address.sin_addr.s_addr = INADDR_ANY;
   address.sin_port = htons(PORT);

   ////////////////////////////////////////////////////////////////////////////
   // ASSIGN AN ADDRESS WITH PORT TO SOCKET
   if (bind(create_socket, (struct sockaddr *)&address, sizeof(address)) == -1)
   {
      perror("bind error");
      return EXIT_FAILURE;
   }

   ////////////////////////////////////////////////////////////////////////////
   // ALLOW CONNECTION ESTABLISHING
   // Socket, Backlog (= count of waiting connections allowed)
   if (listen(create_socket, 5) == -1)
   {
      perror("listen error");
      return EXIT_FAILURE;
   }

   while (!abortRequested)
   {
      /////////////////////////////////////////////////////////////////////////
      // ignore errors here... because only information message
      // https://linux.die.net/man/3/printf
      printf("Waiting for connections...\n");

      /////////////////////////////////////////////////////////////////////////
      // ACCEPTS CONNECTION SETUP
      // blocking, might have an accept-error on ctrl+c
      addrlen = sizeof(struct sockaddr_in);

      //around here make new threads/processes

      if ((new_socket = accept(create_socket,
                               (struct sockaddr *)&cliaddress,
                               &addrlen)) == -1)
      {
         if (abortRequested)
         {
            perror("accept error after aborted");
         }
         else
         {
            perror("accept error");
         }
         break;
      }

      /////////////////////////////////////////////////////////////////////////
      // START CLIENT
      // ignore printf error handling
      printf("Client connected from %s:%d...\n",
             inet_ntoa(cliaddress.sin_addr),
             ntohs(cliaddress.sin_port));
      clientCommunication(&new_socket); // returnValue can be ignored
      new_socket = -1;
   }

   // frees the descriptor
   if (create_socket != -1)
   {
      if (shutdown(create_socket, SHUT_RDWR) == -1)
      {
         perror("shutdown create_socket");
      }
      if (close(create_socket) == -1)
      {
         perror("close create_socket");
      }
      create_socket = -1;
   }

   return EXIT_SUCCESS;
}

void *clientCommunication(void *data)
{
   char buffer[BUF];
   int size;
   int *current_socket = (int *)data;
   bool logged_in = false;
   char username[USER_LENGTH];
   char password[PASSWORD_LENGTH];

   char *user_from_login_attempt = NULL;
   char *pass_from_login_attempt = NULL;
   

   ////////////////////////////////////////////////////////////////////////////
   // SEND welcome message
   strcpy(buffer, "Welcome to myserver!\r\nPlease enter your commands...\r\n");
   if (send(*current_socket, buffer, strlen(buffer), 0) == -1)
   {
      perror("send failed");
      return NULL;
   }
   memset(buffer, 0, BUF);

   do
   {
      /////////////////////////////////////////////////////////////////////////
      // RECEIVE
      size = recv(*current_socket, buffer, BUF - 1, 0);
      if (size == -1)
      {
         if (abortRequested)
         {
            perror("recv error after aborted");
         }
         else
         {
            perror("recv error");
         }
         break;
      }
      if (size == 0)
      {
         printf("Client closed remote socket\n"); // ignore error
         break;
      }

      printf("Message received: \n %s\n", buffer); // ignore error

      //Get first line
      char *firstLine = strtok(buffer, "\n");

      // Filter the command type
      CommandType commandType = filterCommandType(firstLine);


       // Handle the command

      switch (commandType)
      {
         case LOGIN:
            // Ensure we don't read more than the allocated space

            user_from_login_attempt = strtok(NULL, "\n");
            pass_from_login_attempt = strtok(NULL, "\n");
            if (user_from_login_attempt != NULL && pass_from_login_attempt != NULL)
            {
               // Safely copy the username and password using strncpy
               strncpy(username, user_from_login_attempt, USER_LENGTH - 1);
               username[USER_LENGTH - 1] = '\0'; // Null-terminate to be safe
               printf("Username set to: %s\n", username); 

               strncpy(password, pass_from_login_attempt, PASSWORD_LENGTH - 1);
               password[PASSWORD_LENGTH - 1] = '\0'; // Null-terminate to be safe
               printf("Password set to: %s\n", password); 

               logged_in = true;
               printf("User is now logged in\n");

               if (send(*current_socket, "OK\n", 3, 0) == -1)
               {
                  perror("send LOGIN response failed");
               }
               else
               {
                  printf("Response sent: OK\n"); // ignore error
               }
            }
            else
            {
               if (send(*current_socket, "ERR\n", 3, 0) == -1)
               {
                  perror("send ERR response failed");
               }
            }
            break;
         case SEND:
            if(!logged_in)
            {
               if (send(*current_socket, "ERR\n", 3, 0) == -1)
               {
                  perror("send ERR response failed");
               }
               break;
            }
            break;
         case QUIT:
            shutdown(*current_socket, SHUT_RDWR);
            close(*current_socket);
            *current_socket = -1;
            printf("Server closed socket\n");
            return NULL; // Exit the function after handling QUIT
            break;

         case INVALID_COMMAND:
         default:
            if (send(*current_socket, "ERR\n", 3, 0) == -1)
            {
               perror("send ERR response failed");
            }
            break;
      }

       memset(buffer, 0, BUF);

    } while (!abortRequested);

   // closes/frees the descriptor if not already
   if (*current_socket != -1)
   {
      if (shutdown(*current_socket, SHUT_RDWR) == -1)
      {
         perror("shutdown new_socket");
      }
      if (close(*current_socket) == -1)
      {
         perror("close new_socket");
      }
      *current_socket = -1;
   }

   return NULL;
}

void signalHandler(int sig)
{
   if (sig == SIGINT)
   {
      printf("abort Requested... "); // ignore error
      abortRequested = 1;
      /////////////////////////////////////////////////////////////////////////
      // With shutdown() one can initiate normal TCP close sequence ignoring
      // the reference count.
      // https://beej.us/guide/bgnet/html/#close-and-shutdownget-outta-my-face
      // https://linux.die.net/man/3/shutdown
      if (new_socket != -1)
      {
         if (shutdown(new_socket, SHUT_RDWR) == -1)
         {
            perror("shutdown new_socket");
         }
         if (close(new_socket) == -1)
         {
            perror("close new_socket");
         }
         new_socket = -1;
      }

      if (create_socket != -1)
      {
         if (shutdown(create_socket, SHUT_RDWR) == -1)
         {
            perror("shutdown create_socket");
         }
         if (close(create_socket) == -1)
         {
            perror("close create_socket");
         }
         create_socket = -1;
      }
   }
   else
   {
      exit(sig);
   }
}

CommandType filterCommandType(const char *firstLine)
{
    if (strcmp(firstLine, "LOGIN") == 0)
    {
        return LOGIN;
    }
    else if (strcmp(firstLine, "SEND") == 0)
    {
        return SEND;
    }
    else if (strcmp(firstLine, "LIST") == 0)
    {
        return LIST;
    }
    else if (strcmp(firstLine, "READ") == 0)
    {
        return READ;
    }
     else if (strcmp(firstLine, "DEL") == 0)
    {
        return DEL;
    }
    else if (strcmp(firstLine, "QUIT") == 0)
    {
        return QUIT;
    }
    else
    {
        return INVALID_COMMAND;
    }
}