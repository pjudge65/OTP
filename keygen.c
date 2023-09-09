#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>


int main(int argc, char *argv[])
{
  if (argc < 2){
    fprintf(stderr, "Error: missing length argument");
    exit(1);
  }
  // saves the command line argument for number of keys to generate
  int length = atoi(argv[1]);

  // arrows of what characters are allowed to generate
  char const allowed[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ ";

  // seeds the randomization algorithm
  srand(time(NULL));

  int randomChar = 0;

  // generates "length" amount of random characters
  for (int i=0; i < length; i++){
    // generates random number between 0 to 26
    randomChar = (rand() % 27);

    // the character at that index is our generated key
    printf("%c", allowed[randomChar]);
  }
  printf("\n");

  return 0;




}
