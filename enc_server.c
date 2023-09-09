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
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>

// Error function used for reporting issues
void error(const char *msg) {
  perror(msg);
  exit(1);
} 

// Set up the address struct for the server socket
void setupAddressStruct(struct sockaddr_in* address, 
                        int portNumber){
 
  // Clear out the address struct
  memset((char*) address, '\0', sizeof(*address)); 

  // The address should be network capable
  address->sin_family = AF_INET;
  // Store the port number
  address->sin_port = htons(portNumber);
  // Allow a client at any address to connect to this server
  address->sin_addr.s_addr = INADDR_ANY;
}

int main(int argc, char *argv[]){
  int connectionSocket;
  char *buffer = NULL;

  // buffers for storing the plaintext, keytext and ciphertext
  char *plaintext = NULL;
  char *keytext = NULL;
  char *ciphertext = NULL;

  // array of allowed characters used in input, key and encryption
  char const allowed[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ ";
  buffer = calloc(256, sizeof(char));

  // keeps count of child Processes 
  int countChildren = 0;
  
  // stores size of plaintext file and items used for sendall functionality 
  long plaintextLength, keyLength, charsRead, totalTrans, bytesRemaining;
  struct sockaddr_in serverAddress, clientAddress;
  socklen_t sizeOfClientInfo = sizeof(clientAddress);

  // Check usage & args
  if (argc < 2) { 
    fprintf(stderr,"USAGE: %s port\n", argv[0]); 
    exit(1);
  } 
  
  // Create the socket that will listen for connections
  int listenSocket = socket(AF_INET, SOCK_STREAM, 0);
  if (listenSocket < 0) {
    error("ERROR opening socket");
  }

  // Set up the address struct for the server socket
  setupAddressStruct(&serverAddress, atoi(argv[1]));

  // Associate the socket to the port
  if (bind(listenSocket, 
          (struct sockaddr *)&serverAddress, 
          sizeof(serverAddress)) < 0){
    error("ERROR on binding");
  }

  // Start listening for connetions. Allow up to 5 connections to queue up
  listen(listenSocket, 5); 
  
  // Accept a connection, blocking if one is not available until one connects
  while(1){

    // maintins the maximum concurrent processes at 5
    // reaps children when that limit is reached
    while (countChildren >= 5){
      while (waitpid(-1, NULL, WNOHANG)>0){
        countChildren--;
      }
    }

    // Accept the connection request which creates a connection socket
    connectionSocket = accept(listenSocket, 
                (struct sockaddr *)&clientAddress, 
                &sizeOfClientInfo); 
    if (connectionSocket < 0){
      error("ERROR on accept");
    }


    //---------------FORKING----------------------------
 
    pid_t spawnPid = fork();
    switch(spawnPid){
      case -1:
        // close if fork failed
        perror("fork() failed\n");
        exit(1);
        break;
      case 0:
        // CHILD PROCESS

        // close the extra file descriptor created by the fork
        close(listenSocket);

        //----------------RECV HANDSHAKE FROM CLIENT-----------
        memset(buffer, '\0', 256);
        totalTrans = 0;
        bytesRemaining = 10;

        // receives client name from the client. Sends validation
        // token if it matches the server name or not. Partial sends
        // by network data loss are recovered in the while loop
        while(totalTrans < 10){
          charsRead = recv(connectionSocket, buffer+totalTrans, bytesRemaining, 0);
          if (charsRead < 0){
            error("ERROR reading handshake");
            break;
          }
          totalTrans += charsRead;
          bytesRemaining -= charsRead;
        }

        // if the name doesn't match, send the rejection token
        if (strcmp(buffer, "enc_client")!=0){
          
          totalTrans = 0;
          bytesRemaining = 6;
          char reject[] = "REJECT";

          // send rejection token using "sendall" functionality
          while (totalTrans < 6){
            charsRead = send(connectionSocket, reject+totalTrans, bytesRemaining, 0);
            if (charsRead < 0){
              error("ERROR writing to socket");
              break;
            }
            totalTrans += charsRead;
            bytesRemaining -= charsRead;
          }
          exit(2); 

        } else {
          // if the name does match, send the accept token
          totalTrans = 0;
          bytesRemaining = 6;
          char accept[] = "ACCEPT";

          // send accept token using the "sendall" functionality
          while (totalTrans < 6){
            charsRead = send(connectionSocket, accept+totalTrans, bytesRemaining, 0);
            if (charsRead < 0){
              error("ERROR writing to socket");
              break;
            }
            totalTrans += charsRead;
            bytesRemaining -= charsRead;
          }
        }

        // ---------------RECV LENGTH OF PLAINTEXT---------

        // Reset the buffer to receive the length of the plaintext
        memset(buffer, '\0', 256);
        
        totalTrans = 0;
        bytesRemaining = 255;

        // receives the length of the plaintext. If data-loss, continues receiving until 
        // all expected characters are received
        while (totalTrans < 255){
          charsRead = recv(connectionSocket, buffer+totalTrans, bytesRemaining, 0);
          if (charsRead < 0){
            error("ERROR reading from socket141");
          }
          totalTrans += charsRead;
          bytesRemaining -= charsRead;
        }

        // saves the length of the plaintext received as an integer
        plaintextLength = atoi(buffer);



        // ------------RECEIVE PLAINTEXT FILE OF THAT LENGTH-----------
        
        // creates buffer allocation for plaintext and ciphertext
        plaintext = calloc(plaintextLength + 1, sizeof(char));
        ciphertext = calloc(plaintextLength + 1, sizeof(char));
        
        totalTrans = 0;
        bytesRemaining = plaintextLength;

        // receives the entire plaintext file in a buffer. If connection is
        // dropped, transmission continues until every expected character is sent
        while (totalTrans < plaintextLength){
          charsRead = recv(connectionSocket, plaintext + totalTrans, bytesRemaining, 0);
          if (charsRead < 0){
            error("ERROR reading from socket");
            break;
          }
          totalTrans += charsRead;
          bytesRemaining -= charsRead;
        }


        //----------- STEP 4 RECEIVE MYKEY FILE OF THAT LENGTH-------------
        totalTrans = 0;
        bytesRemaining = plaintextLength;
    
        // allocate memory for the keytext
        keytext = calloc(plaintextLength + 1, sizeof(char));

        // receives the keytext file that matches the length of the plaintext.
        // If connection is dropped, transmission continues until every expected
        // character is sent.
        while(totalTrans < plaintextLength){
          charsRead = recv(connectionSocket, keytext+totalTrans, bytesRemaining, 0);
          if (charsRead < 0){
            error("ERROR reading from socket");
            break;
          }
          totalTrans += charsRead;
          bytesRemaining -= charsRead;
        }
        

        // ---------------ENCODE PLAINTEXT WITH KEY-----------------------

        // loop through each char of the plaintext buffer
        int i=0;
        while (plaintext[i] != '\0'){

          char keyIdx;
          // sets the index of the key 
          // special case if it is a space, otherwise use ASCII value to calculate
          if (keytext[i]==32){
            keyIdx = 26;
          } else{
            keyIdx = (keytext[i] - 65);
          }

          char plainIdx;

          // if server has received bad input, print to error and continue
          if (plaintext[i]!=32 && !(plaintext[i] >= 65 && plaintext[i] <= 90)){
            fprintf(stderr, "BAD INPUT... continuing\n"); 
          } else {

            // encrypts the plaintext character with the key's character
            // adds the plaintext idx to the key's idx
            // modulo 27 if result is over 26
            // This produces the index of the cipher's character
            if (plaintext[i]==32) {
              ciphertext[i] = allowed[(26 + keyIdx) % 27];
            } else { 
              ciphertext[i] = allowed[((plaintext[i] - 65) + keyIdx) % 27];
            }
          }
          i++;
        }
        
        // -------------SEND CIPHERTEXT BACK TO CLIENT----------------
        totalTrans = 0;
        bytesRemaining = plaintextLength;

        // sends the cipher back to the client for output. If network connection
        // interrupted, send continues until all characters accounted for
        while (totalTrans < plaintextLength){
          charsRead = send(connectionSocket, ciphertext+totalTrans, bytesRemaining, 0);
          if (charsRead < 0){
            error("SERVER: ERROR WRITING TO SOCKET");
            break;
          }
          totalTrans += charsRead;
          bytesRemaining -= charsRead;
        }

        // Close the connection socket for this client
        close(connectionSocket); 
        exit(0);
        break;

      default:
        // PARENT FORK
        // closes excess child connection and increments the count of children
        close(connectionSocket); 
        countChildren++;
        break;
    }
  }
  // Close the listening socket
  close(listenSocket); 
  return 0;
}
