#include "twmailer-client.h"

///////////////////////////////////////////////////////////////////////////////

int main(int argc, char *argv[])
{
   int create_socket;
   char buffer[BUF];
   struct sockaddr_in address;
   int size;
   int isQuit;

   ////////////////////////////////////////////////////////////////////////////
   // CREATE A SOCKET
   // https://man7.org/linux/man-pages/man2/socket.2.html
   // https://man7.org/linux/man-pages/man7/ip.7.html
   // https://man7.org/linux/man-pages/man7/tcp.7.html
   // IPv4, TCP (connection oriented), IP (same as server)
   if ((create_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1)
   {
      perror("Socket error");
      return EXIT_FAILURE;
   }

   ////////////////////////////////////////////////////////////////////////////
   // INIT ADDRESS
   // Attention: network byte order => big endian
   memset(&address, 0, sizeof(address)); // init storage with 0
   address.sin_family = AF_INET;         // IPv4
   // https://man7.org/linux/man-pages/man3/htons.3.html
   if(argc < 2)
   {
      address.sin_port = htons(PORT);
   }
   else
   {
      address.sin_port = htons(atoi(argv[2]));
   }
   // https://man7.org/linux/man-pages/man3/inet_aton.3.html
   if (argc < 2)
   {
      inet_aton("127.0.0.1", &address.sin_addr);
   }
   else
   {
      inet_aton(argv[1], &address.sin_addr);
   }

   ////////////////////////////////////////////////////////////////////////////
   // CREATE A CONNECTION
   // https://man7.org/linux/man-pages/man2/connect.2.html
   if (connect(create_socket,
               (struct sockaddr *)&address,
               sizeof(address)) == -1)
   {
      // https://man7.org/linux/man-pages/man3/perror.3.html
      perror("Connect error - no server available");
      return EXIT_FAILURE;
   }

   // ignore return value of printf
   printf("Connection with server (%s) established\n",
          inet_ntoa(address.sin_addr));

   ////////////////////////////////////////////////////////////////////////////
   // RECEIVE DATA
   // https://man7.org/linux/man-pages/man2/recv.2.html
   size = recv(create_socket, buffer, BUF - 1, 0);
   if (size == -1)
   {
      perror("recv error");
   }
   else if (size == 0)
   {
      printf("Server closed remote socket\n"); // ignore error
   }
   else
   {
      buffer[size] = '\0';
      printf("%s", buffer); // ignore error
   }

   string message;

   do
   {
      printf(">> ");
      getline(cin, message, '\n');
      checkCommand(message);
      isQuit = strcmp(message.c_str(), "QUIT") == 0;

      if (message != "")
      {
         //////////////////////////////////////////////////////////////////////
         // SEND DATA
         // https://man7.org/linux/man-pages/man2/send.2.html
         // send will fail if connection is closed, but does not set
         // the error of send, but still the count of bytes sent
         if ((send(create_socket, message.c_str(), message.size(), 0)) == -1)
         {
            // in case the server is gone offline we will still not enter
            // this part of code: see docs: https://linux.die.net/man/3/send
            // >> Successful completion of a call to send() does not guarantee
            // >> delivery of the message. A return value of -1 indicates only
            // >> locally-detected errors.
            // ... but
            // to check the connection before send is sense-less because
            // after checking the communication can fail (so we would need
            // to have 1 atomic operation to check...)
            perror("send error");
            break;
         }

         //////////////////////////////////////////////////////////////////////
         // RECEIVE FEEDBACK
         // consider: reconnect handling might be appropriate in somes cases
         //           How can we determine that the command sent was received 
         //           or not? 
         //           - Resend, might change state too often. 
         //           - Else a command might have been lost.
         //
         // solution 1: adding meta-data (unique command id) and check on the
         //             server if already processed.
         // solution 2: add an infrastructure component for messaging (broker)
         //
         size = recv(create_socket, buffer, BUF - 1, 0);
         if (size == -1)
         {
            perror("recv error");
            break;
         }
         else if (size == 0)
         {
            printf("Server closed remote socket\n"); // ignore error
            break;
         }
         else
         {
            buffer[size] = '\0';
            printf("<< %s\n", buffer);
         }
      }
   } while (!isQuit);

   ////////////////////////////////////////////////////////////////////////////
   // CLOSES THE DESCRIPTOR
   if (create_socket != -1)
   {
      if (shutdown(create_socket, SHUT_RDWR) == -1)
      {
         // invalid in case the server is gone already
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

void checkCommand(string &message)
{
   string buffer;

   if(message == "LOGIN")
   {
      //Credentials
      cout << "LDAP username: ";
      getline(cin, buffer, '\n');
      if(buffer.size() > USER_LENGTH)
      {
         cout << "ERROR: Username must be no longer than 8 characters!" << endl;
         message = "";
         return;
      }
      message = message + "\n" + buffer;

      buffer = getpass();
      if(buffer.size() > PASSWORD_LENGTH)
      {
         cout << "ERROR: Username must be no longer than 80 characters!" << endl;
         message = "";
         return;
      }
      message = message + "\n" + buffer;
   }

   else if(message == "SEND")
   {
      //Sender and Reciever
      cout << "Receiver: ";
      getline(cin, buffer, '\n');
      if(buffer.size() > USER_LENGTH)
      {
         cout << "ERROR: Username must be no longer than 8 characters!" << endl;
         message = "";
         return;
      }
      message = message + "\n" + buffer;
      
      //Subject
      cout << "Subject: ";
      getline(cin, buffer, '\n');
      if(buffer.size() > SUBJECT_LENGTH)
      {
         cout << "ERROR: Subject must be shorter than 80 characters!" << endl;
         message = "";
         return;
      }
      message = message + "\n" + buffer;

      //Message
      cout << "Message: ";
      while(buffer != ".")
      {
         getline(cin, buffer, '\n');
         message = message + "\n" + buffer;
      }

      //Message length check
      if(message.size() > BUF)
      {
         cout << "ERROR: Entire message can not be longer than 1024 characters!" << endl;
      }

      return;
   }
   
   else if(message == "LIST")
   {
      return;
   }
   
   else if(message == "READ" || message == "DEL")
   {
      cout << "Message-Number: ";
      getline(cin, buffer, '\n');
      message = message + "\n" + buffer;
      return;
   }

   else if(message == "QUIT")
   {
      return;
   }

   else
   {
      message = "";
   }

   return;
}

int getch()
{
    int ch;
    struct termios t_old, t_new;
    tcgetattr(STDIN_FILENO, &t_old);
    t_new = t_old;
    t_new.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &t_new);
    ch = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &t_old);
    return ch;
}

const char *getpass()
{
    int show_asterisk = 0;

    const char BACKSPACE = 127;
    const char RETURN = 10;

    unsigned char ch = 0;
    std::string password;

    printf("Password: ");

    while ((ch = getch()) != RETURN)
    {
        if (ch == BACKSPACE)
        {
            if (password.length() != 0)
            {
                if (show_asterisk)
                {
                    printf("\b \b"); // backslash: \b
                }
                password.resize(password.length() - 1);
            }
        }
        else
        {
            password += ch;
            if (show_asterisk)
            {
                printf("*");
            }
        }
    }
    printf("\n");
    return password.c_str();
}