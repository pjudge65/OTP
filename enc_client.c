//
//Peter Judge
//CS 344
//
//Starter code taken from Assignment Page Replits
//Sendall/Recvall While Loop codes adapted from Beej's Guide to Networking
//Forking/Concurrency code adapted from Chapter 60 Linux Programming Interface
//
//
//



#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>  // ssize_t
#include <sys/socket.h> // send(),recv()
#include <netdb.h>      // gethostbyname()

/**
* Client code
* 1. Create a socket and connect to the server specified in the command arugments.
* 2. Prompt the user for input and send that input as a message to the server.
* 3. Print the message received from the server and exit the program.
*/

// Error function used for reporting issues
void error(const char *msg) { 
  perror(msg); 
  exit(0); 
} 

// Set up the address struct
void setupAddressStruct(struct sockaddr_in* address, 
                        int portNumber){
 
  // Clear out the address struct
  memset((char*) address, '\0', sizeof(*address)); 

  // The address should be network capable
  address->sin_family = AF_INET;
  // Store the port number
  address->sin_port = htons(portNumber);

  // Get the DNS entry for this host name
  struct hostent* hostInfo = gethostbyname("localhost"); 
  if (hostInfo == NULL) { 
    fprintf(stderr, "CLIENT: ERROR, no such host\n"); 
    exit(0); 
  }
  // Copy the first IP address from the DNS entry to sin_addr.s_addr
  memcpy((char*) &address->sin_addr.s_addr, 
        hostInfo->h_addr_list[0],
        hostInfo->h_length);
}

int main(int argc, char *argv[]) {
  int socketFD, portNumber;
  struct sockaddr_in serverAddress;
  char buffer[256];

  // Pointers to read in plaintext and key files
  FILE *plainFile;
  FILE *keyFile;
  char *plainBuffer = NULL;
  char *keyBuffer = NULL;

  //length of plaintext file and key file
  long plainLength;              
  long keyLength;
  size_t plainReadLen;
  size_t keyReadLen;
  int temp_len = 0;

  // values to track progress of sendall loops
  long totalTrans, bytesRemaining, charsWritten, charsRead;
  char *temp_str = NULL;

    // Check usage & args
  if (argc < 4) { 
    fprintf(stderr,"USAGE: %s plaintext keygen port\n", argv[0]); 
    exit(0); 
  } 

  // Create a socket
  socketFD = socket(AF_INET, SOCK_STREAM, 0); 
  if (socketFD < 0){
    fprintf(stderr, "CLIENT: ERROR opening socket on Port %s", argv[3]);
    exit(2);
  }

   // Set up the server address struct
   // argv[3] is port number
  setupAddressStruct(&serverAddress, atoi(argv[3]));

  // Connect to server
  if (connect(socketFD, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0){
    fprintf(stderr, "CLIENT: ERROR connecting to Port %s", argv[3]);
    exit(2);
  }

  // --------------SENDING HANDSHAKE TO SERVER------------------
  char handshake[] = "enc_client";
  totalTrans = 0;
  bytesRemaining = 10;

  // Send client name to server for validation, if the name of the client 
  // doesn't match the name of the server we exit. 
  while (totalTrans < 10){
    charsWritten = send(socketFD, handshake+totalTrans, bytesRemaining, 0);
    if (charsWritten < 0){
      error("CLIENT: ERROR writing to socket");
      break;
    }
    totalTrans += charsWritten;
    bytesRemaining -= charsWritten;
  }
  // RECEIVING CONFIRMATION/REJECTION MESSAGE BACK FROM SERVER
  // If it isn't validated by server, terminate immediately
  memset(buffer, '\0', sizeof(buffer));
  totalTrans = 0;
  bytesRemaining = 6;
  while (totalTrans < 6){
    charsRead = recv(socketFD, buffer+totalTrans, 6, 0); 
    if (charsRead < 0){
      error("CLIENT: ERROR reading from socket");
      break;
    }
    totalTrans += charsRead;
    bytesRemaining -= charsRead;
  }

  // IF REJECTED BY SERVER, TERMINATE THE CLIENT 
  if (strcmp(buffer, "ACCEPT") != 0){
    fprintf(stderr, "CLIENT: ERROR: Connecting to wrong server");
    exit(2);
  }

  // ------------READING plaintext file to buffer-----------
  //opening plaintext file descriptor
  plainFile = fopen(argv[1], "r");
  if (plainFile == NULL){
    fprintf(stderr, "Error opening plaintext file");
    exit(1);
  }
  // puts the file indicator at the last character in file
  if (fseek(plainFile, 0L, SEEK_END) != 0){
    fprintf(stderr, "Error occured on fseek");
    exit(1);
  }
  // the location of the file stream (given by ftell) is equal 
  // to length of the file. 
  plainLength = ftell(plainFile);
  if (plainLength == -1){
    fprintf(stderr, "Error occured on ftell operation");
    exit(1);
  }
  //creates the appropriate space for the amount to read (including null-terminator)
  plainBuffer = malloc((plainLength + 1) * sizeof(char));

  //puts the file stream back to the start of the file
  if (fseek(plainFile, 0L, SEEK_SET) != 0){
    fprintf(stderr, "Error occured on fseek");
    exit(1);
  }
  // reads the num of chars we calculated above in
  // to buffer. Doesn't include null terminator.
  plainReadLen = fread(plainBuffer, sizeof(char), plainLength, plainFile);
  if (ferror(plainFile)){
    fprintf(stderr, "Error on fread");
    exit(1);
  }
  // null terminates the buffer just incase
  plainBuffer[strcspn(plainBuffer, "\n")] = '\0';
  plainLength -= 1;
  plainReadLen -= 1;
  fclose(plainFile);

  // IF CLIENT RECEIVES BAD CHARACTERS AS INPUT,
  // TERMINATE, ERR MESSAGE, EXIT(1)
  for (int i=0; i<plainReadLen; i++){
    if (plainBuffer[i] != 32 && !(plainBuffer[i] >= 65 && plainBuffer[i] <= 90)){

      fprintf(stderr, "ERROR: CLIENT: Received bad characters in input file");
      exit(1);
    }
  }


  //---------READING KEYGEN FILE INTO BUFFER-------------
  //opens the file descriptor
  keyFile = fopen(argv[2], "r");
  if (keyFile == NULL) 
  {
    fprintf(stderr, "Error opening keygen file");
    exit(1);
  };
  // moves the file stream to the last character in file
  if (fseek(keyFile, 0L, SEEK_END) != 0){
    fprintf(stderr, "Error occured on fseek");
    exit(1);
  };

  // ftell will tell us our position in the file, which
  // will be at the end, which is equivalent to number
  // of bytes to be read
  keyLength = ftell(keyFile);
  if (keyLength == -1) {
    fprintf(stderr, "Error occured on ftell operation");
    exit(1);
  }

  // IF KEY FILE SHORTER THAN PLAINTEXT TERMINATE PROGRAM
  if ((keyLength-1) < plainLength){
    fprintf(stderr, "CLIENT: ERROR: Length of key file less than plaintext");
    exit(1);
  }
  // creates space for the file to be read into
  keyBuffer = calloc((plainLength + 1), sizeof(char));

  // resets the file stream to the start of the file
  if (fseek(keyFile, 0L, SEEK_SET) != 0){
    fprintf(stderr, "Error occured on fseek");
    exit(1);
  };
  // reads the num of characters calculated above into 
  // buffer, returns number of characters read
  keyReadLen = fread(keyBuffer, sizeof(char), plainLength, keyFile);
  if (ferror(keyFile)){
    fprintf(stderr, "Error on fread");
    exit(1);
  }

  // null terminates buffer and close file descriptor
  //replaces the final \n with a null terminator
  keyBuffer[strcspn(keyBuffer, "\n")] = '\0';
  fclose(keyFile);

  


 
  // ---------------SENDING PLAINTEXT LENGTH AS STRING-----------

  
  //resets the buffer to read the length into
  memset(buffer, '\0', sizeof(buffer));

  // converts plainLength to a string and saves to buffer
  temp_len = snprintf(NULL, 0, "%zu", plainLength);
  temp_str = realloc(temp_str, temp_len + 1);
  snprintf(buffer, temp_len+1, "%zu", plainLength);
    
  // sends the plaintext length as a string to the server 
  totalTrans = 0;
  bytesRemaining = 255;
  while (totalTrans < 255){
    charsWritten = send(socketFD, buffer+totalTrans, bytesRemaining, 0);
    if (charsWritten < 0){
      error("CLIENT: ERROR writing to socket");
      break;
    }
    totalTrans += charsWritten;
    bytesRemaining -= charsWritten;
  }


  //---------------SENDING PLAINTEXT BUFFER---------------------------

  totalTrans = 0;                               
  bytesRemaining = plainLength;            
  
  // sending the entire plaintext file to the server
  // if partial transmission, the loop continues to send until every
  // expected character is sent
  while (totalTrans < plainLength){
    charsWritten = send(socketFD, plainBuffer + totalTrans, bytesRemaining, 0);
    if (charsWritten < 0){
      error("CLIENT: ERROR writing to socket");
      break;
    }
    totalTrans += charsWritten;
    bytesRemaining -= charsWritten;
  }
  memset(buffer, '\0', sizeof(buffer));



  //-------------SENDING KEY TEXT BUFFER--------------------------------

  totalTrans = 0;
  bytesRemaining = plainLength;

  // sends the equal amount of chars that are in plaintext from keytext
  // to server. If partial send, loop continues until expected chars are met
  while (totalTrans < plainLength){
    charsWritten = send(socketFD, keyBuffer+totalTrans, bytesRemaining, 0);
    if (charsWritten < 0){
      error("CLIENT: ERROR writing to socket");
      break;
    }
    totalTrans += charsWritten;
    bytesRemaining -= charsWritten;
  }

  //-------------RECEIVE CIPHERTEXT BUFFER-------------------------
  
  totalTrans = 0;
  bytesRemaining = plainLength;
  memset(plainBuffer, '\0', plainLength+1);

  // Receives the entire ciphertext buffer from the server. If partially 
  // received, loop will continue receiving until expected character count
  // met
  while (totalTrans < plainLength){
    charsRead = recv(socketFD, plainBuffer+totalTrans, bytesRemaining, 0);
    if (charsRead < 0){
      error("CLIENT: ERROR reading from socket");
      break;
    }
    totalTrans += charsRead;
    bytesRemaining -= charsRead;
  }

  // output the ciphertext to stdout with a trailing newline
  printf("%s\n", plainBuffer);


  // Close the socket
  close(socketFD); 
  return 0;
}
