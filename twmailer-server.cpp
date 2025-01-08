#include "twmailer-server.h"
#include <queue>
#include <thread>
#include <vector>
#include <condition_variable>
#include <atomic>
#include <map>
#include <string>
#include <memory>
#include <filesystem>

//#define ENABLE_MUTEX_TESTING

namespace fs = std::filesystem;

int abortRequested = 0;
int create_socket = -1;
int new_socket = -1;
std::map<std::string, std::unique_ptr<std::mutex>> individualEmailLocks;


const int THREAD_POOL_SIZE = 4;

// Global Thread Pool Variables
std::vector<std::thread> threadPool;
std::queue<int> taskQueue; //shared resource, we lock when accessing it
                           //this taskQueue will store all clientSockets
std::mutex queueMutex; //for locking aforementioned taskQueue
std::condition_variable condition; //synchronization variable, until condition modified and notfied
std::atomic<bool> serverRunning(true); // can be accessed by multiple threads without race condition

std::mutex directoryMutex; //this is for locking the whole Email directory access

void threadWorker()
{
    while (serverRunning)
    {
      std::unique_lock<std::mutex> lock(queueMutex);
      //this lock is only for accessing the shared taskQueue

      //queueMutex simulates a queue. All threads are waiting here, and one gets woken up by condition.notify_one
      //

      //while waiting, unlocks mutex
      //When woken, Checks if there is a task or Server stopped running
      //also reaquires mutex
      condition.wait(lock, [] { return !taskQueue.empty() || !serverRunning; });

      if (!serverRunning && taskQueue.empty())
      {
            return; // Exit thread
      }

      // Get the next client socket
      int clientSocket = taskQueue.front();
      taskQueue.pop();
         lock.unlock(); // Unlock the shared taskQueue mutex while processing the client


        // Handle client communication
        clientCommunication(&clientSocket);

         if(clientSocket != -1)
         {
            // Close client socket
            shutdown(clientSocket, SHUT_RDWR);
            close(clientSocket);
         }
        
    }
}

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

   // create directory for emails
   const char *directoryName = "Emails";
   struct stat st;
   if(stat(directoryName, &st) != 0)
   {
      mkdir(directoryName, 0700);
   }

   ////////////////////////////////////////////////////////////////////////////
    // Initialize Thread Pool
    for (int i = 0; i < THREAD_POOL_SIZE; ++i)
    {
        threadPool.emplace_back(threadWorker);
    }

   while (serverRunning)
   {
      /////////////////////////////////////////////////////////////////////////
      // ignore errors here... because only information message
      // https://linux.die.net/man/3/printf
      printf("Waiting for connections...\n");

      /////////////////////////////////////////////////////////////////////////
      // ACCEPTS CONNECTION SETUP
      // blocking, might have an accept-error on ctrl+c
      addrlen = sizeof(struct sockaddr_in);


      new_socket = accept(create_socket, (struct sockaddr *)&cliaddress, &addrlen);
      if (new_socket  == -1)
        {
            if (serverRunning)
            {
                perror("accept error");
            }
            continue; // Skip and continue accepting connections
        }

      /////////////////////////////////////////////////////////////////////////
      // START CLIENT
      // ignore printf error handling
      printf("Client connected from %s:%d...\n",
             inet_ntoa(cliaddress.sin_addr),
             ntohs(cliaddress.sin_port));
      // Add task to the queue
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            taskQueue.push(new_socket);
        }//lock_guard out of scope, unlocks
        condition.notify_one(); // Notify one worker thread
    }

    ////////////////////////////////////////////////////////////////////////////
    // Cleanup
    close(create_socket);
    serverRunning = false; // Signal threads to shut down
    condition.notify_all(); // Wake up all threads

    // Join all threads
    for (std::thread &t : threadPool)
    {
        if (t.joinable())
        {
            t.join();
        }
    }

    printf("Server shut down.\n");
    return EXIT_SUCCESS;
}

//====================================================================================================================

void mutexDelayForTesting(string username)
{
   std::cout << "Thread ID: " << std::this_thread::get_id() << " is holding the mutex for " << username << std::endl;
   std::this_thread::sleep_for(std::chrono::seconds(3)); // Delay for 3 seconds
}

void mutexUnlockedMessage(string username)
{
   std::cout << "Thread ID: " << std::this_thread::get_id() << " is releasing the mutex for " << username << std::endl;
}

void clientCommunication(void *data)
{
   char buffer[BUF];
   int size;
   int *current_socket = (int *)data;
   bool logged_in = false;
   const char* baseDirectory = "Emails";

   string username;
   string password;

   ////////////////////////////////////////////////////////////////////////////
   // SEND welcome message
   strcpy(buffer, "Welcome to myserver!\r\nPlease enter your commands...\r\n");
   respond(current_socket, buffer);
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

      cout << endl << "Message recieved: " << endl << buffer << endl << endl;

      //Get first line
      string firstLine = strtok(buffer, "\n");

      // Handle the command
      if(firstLine == "LOGIN") 
      {
         logged_in = login(current_socket, username, std::string(baseDirectory));
      }

      else if(firstLine == "QUIT")
      {
            shutdown(*current_socket, SHUT_RDWR);
            close(*current_socket);
            *current_socket = -1;
            printf("Server closed socket\n");
            return; // Exit the function after handling QUIT
      }
      
      else if(!logged_in)
      {
         respond(current_socket, "ERR\n");
      }

      else if(firstLine == "SEND")
      {
         send(current_socket, username, baseDirectory);
      }
      else
      {
         std::lock_guard<std::mutex> lock(*individualEmailLocks[username]);
         #ifdef ENABLE_MUTEX_TESTING
         mutexDelayForTesting(username);
         #endif
         
         if(firstLine == "LIST")
         {
            list(current_socket, username, baseDirectory);
         }
         else if(firstLine == "READ")
         {
            read(current_socket, username, baseDirectory);
         }
         else if(firstLine == "DEL")
         {
            del(current_socket, username, baseDirectory);
         }
         else
         {
            respond(current_socket, "ERR\n");
         }
         #ifdef ENABLE_MUTEX_TESTING
         mutexUnlockedMessage(username);
         #endif
      }
      

      memset(buffer, 0, BUF);

   }
   while (!abortRequested);

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
}

//====================================================================================================================
void createDirIfNotCreated(string username, string baseDirectory)
{
   string path = baseDirectory + "/" + username;
   
   std::lock_guard<std::mutex> lock(directoryMutex);  // Lock the mutex
   #ifdef ENABLE_MUTEX_TESTING
   mutexDelayForTesting("whole email directory");
   #endif

   DIR *dir = opendir(path.c_str());
   if(dir == NULL)
   {
      try
      {
         if (fs::create_directory(path))
         {
            std::cout << "Directory created: " << path << std::endl;
            individualEmailLocks.try_emplace(username, std::make_unique<std::mutex>());
         }
         else
         {
            std::cout << "Directory already exists or failed to create." << std::endl;
         }
      }
      catch (const fs::filesystem_error &e)
      {
         std::cerr << "Error: " << e.what() << std::endl;
      }
   }
   else
   {
      closedir(dir); // Don't forget to close the directory if it exists
   }
   #ifdef ENABLE_MUTEX_TESTING
   mutexUnlockedMessage("whole email directory");
   #endif
}


bool login(int *current_socket, std::string &username, std::string baseDirectory)
{
   bool logged_in = false;
   string password;

   // Ensure we don't read more than the allocated space
   username = strtok(NULL, "\n");
   password = strtok(NULL, "\n");
   if (username != "" && password != "")
   {
      cout << "Username set to: " << username << endl;
      cout << "Password set to: " << password << endl;

      logged_in = true;
      cout << "User is now logged in" << endl;

      createDirIfNotCreated(username, baseDirectory);

      respond(current_socket, "OK\n");
   }
   else
   {
      respond(current_socket, "ERR\n");
   }

   return logged_in;
}

//====================================================================================================================

void send(int *current_socket, std::string username, std::string baseDirectory)
{
   char *receiver;
   char *subject;
   char *line;
   string message;
   struct stat st;
   memset(&st, 0, sizeof(st));
   

   receiver = strtok(NULL, "\n");
   //if directory for receiver does not exist, create directory
   createDirIfNotCreated(receiver, baseDirectory);
   string receiverDir = baseDirectory + "/" + receiver;
   printf("Directory exists\n");
   fflush(stdout);
   // Extract subject
   subject = strtok(NULL, "\n");
   printf("subject parsed\n");
   fflush(stdout);

   // Get the message until a line containing only '.' is received
   message = "";
   do
   {
      line = strtok(NULL, "\n");
      if (strcmp(line, ".") == 0)
      {
         printf("End of message received.\n");
         break;
      }
      message = message + "\n" + line;     
   }
   while (line!= NULL);

   if (!subject || message.empty())
   {
      respond(current_socket, "ERR\n");
      return;
   }

   printf("Message and subject parsed\n");
   fflush(stdout);

   // Enclose the FILE* declaration in a block to limit its scope
   {
      // Generate a random GUID for the filename
      uuid_t uuid;
      char uuid_str[37];
      uuid_generate(uuid);
      uuid_unparse(uuid, uuid_str);
      std::string uuidString(uuid_str);

      // Generate file path
      string file_path = receiverDir + "/" + uuidString;
      //open file
      std::lock_guard<std::mutex> lock(*individualEmailLocks[receiver]);
      #ifdef ENABLE_MUTEX_TESTING
      mutexDelayForTesting(receiver);
      #endif
      FILE *file = fopen(file_path.c_str(), "w");
      if (!file) {
         perror("fopen failed");
         respond(current_socket, "ERR\n");
         return;
      }

      //write and close file
      fprintf(file, "Sender: %s\n", username.c_str());
      fprintf(file, "Subject: %s\n", subject);
      fprintf(file, "Message:%s\n", message.c_str());
      fclose(file);
   }
   #ifdef ENABLE_MUTEX_TESTING
   mutexUnlockedMessage(receiver);
   #endif
   // Send success response
   respond(current_socket, "OK\n");
}

//====================================================================================================================

void list(int *current_socket, string username, string baseDirectory)
{
   string path = baseDirectory + "/" + username;

   DIR *dir = opendir(path.c_str());
   if(dir == NULL)
   {
      respond(current_socket, "ERR\n");
      return;
   };
   

   struct dirent *entry;
   struct stat st;
   int file_number = 0;
   int message_index = 1;
   string response;

   while((entry = readdir(dir)) != NULL)
   {
      string currentFile = path + "/" + entry->d_name;
      if (stat(currentFile.c_str(), &st) == -1)
      {
         perror("stat failed");
         continue; // Skip this entry and proceed to the next
      }
      if(S_ISREG(st.st_mode))
      {
         ifstream file(currentFile);
         string line;

         //file must exist
         if(!file.is_open())
         {
            perror("unable to open file");
            respond(current_socket, "ERR\n");
            return;
         }

         for(int i = 0; i < 2; i++)
         {
            getline(file, line);
         }

         size_t delpos = line.find(" ");
         string subject = line.substr(delpos + 1);

         file.close();
         response = response + to_string(message_index) + ": " + subject + "\n";
         message_index++;
         file_number++;
      }
   }

   closedir(dir);
   respond(current_socket, "Number of emails: " + to_string(file_number) + "\n" + response);
}

//====================================================================================================================

void read(int* current_socket, string username, string baseDirectory)
{
   int msnr = atoi(strtok(NULL, "\n"));
   string filepath = findFile(current_socket, baseDirectory + "/" + username, msnr);

   string line;
   ifstream file(filepath);

   //file must exist
   if(!file.is_open())
   {
      perror("unable to open file");
      respond(current_socket, "ERR\n");
      return;
   }
   
   getline(file, line);
   string response = line;
   while(getline(file, line))
   {
      response = response + "\n" + line;
   }
   file.close();

   respond(current_socket, response);
}

//====================================================================================================================

void del(int* current_socket, string username, string baseDirectory)
{
   int msnr = atoi(strtok(NULL, "\n"));
   string filepath = findFile(current_socket, baseDirectory + "/" + username, msnr);

   int status = remove(filepath.c_str());
   if(status != 0)
   {
      perror("could not delete file");
      respond(current_socket, "ERR\n");
      return;
   }

   respond(current_socket, "OK\n");
}

//====================================================================================================================

void signalHandler(int sig)
{
    if (sig == SIGINT)
    {
        printf("Abort Requested...\n");
        serverRunning = false;  // Set serverRunning to false to notify threads
        condition.notify_all();  // Wake up all waiting threads

        // Gracefully shut down all active sockets
        if (new_socket != -1)
        {
            if (shutdown(new_socket, SHUT_RDWR) == -1)
            {
                perror("shutdown new_socket failed");
            }
            if (close(new_socket) == -1)
            {
                perror("close new_socket failed");
            }
            new_socket = -1;
        }

        if (create_socket != -1)
        {
            if (shutdown(create_socket, SHUT_RDWR) == -1)
            {
                perror("shutdown create_socket failed");
            }
            if (close(create_socket) == -1)
            {
                perror("close create_socket failed");
            }
            create_socket = -1;
        }
    }
    else
    {
        exit(sig);  // For other signals, perform regular exit
    }
}



//====================================================================================================================

void respond(int *current_socket, string response)
{
   if (send(*current_socket, response.c_str(), response.size(), 0) == -1)
   {
      perror("send response failed");
   }
   else
   {
      printf("response successfully sent\n"); // ignore error
   }
}

//====================================================================================================================

string findFile(int *current_socket, string path, int position)
{
    string filename = "";

    // filecount starts at 1
    if (position == 0)
    {
        perror("bad message number");
        respond(current_socket, "ERR\n");
        return filename;
    }

    // directory must exist
    DIR *dir = opendir(path.c_str());
    if (dir == NULL)
    {
        perror("directory does not exist");
        respond(current_socket, "ERR\n");
        return filename;
    }

    struct dirent *entry;
    struct stat st;

    int i = 0;
    while ((entry = readdir(dir)) != NULL)
    {
        // Skip "." and ".." entries
        if (string(entry->d_name) == "." || string(entry->d_name) == "..")
        {
            continue;
        }

        string currentFile = path + "/" + entry->d_name;
        if (stat(currentFile.c_str(), &st) == -1)
         {
            perror("stat failed");
            continue; // Skip this entry and proceed to the next
         }

        if (S_ISREG(st.st_mode))
        {
            i++;
            if (i == position)
            {
                filename = currentFile;
                break;
            }
        }
    }

    // file not found
    if (filename.empty())
    {
        perror("file not found");
        respond(current_socket, "ERR\n");
    }

    closedir(dir);
    return filename;
}

