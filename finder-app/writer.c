#include <stdio.h>
#include <syslog.h>
#include <errno.h>
#include <string.h>

int main(int argc, char** argv)
{
 
 
 openlog(NULL, 0, LOG_USER); 
  
 FILE *fp;
  if(argc != 3) 
{
  syslog(LOG_ERR, "Invalid number of arguments: %d ", argc);
  printf("Invalid number of arguments.\n");
  printf("Total number of arguments should be 2.\n");
  printf("\t 1. Path to the file \n");
  printf("\t 2. String to be written in the specified file\n");
  return 1;
}

 fp= fopen(argv[1], "w");

 if(fp == NULL)
 {
   syslog(LOG_ERR, "Error opening file %s: %s\n", argv[1], strerror(errno));
   printf(" File directory not found \n");
   return 1;
 }

if( fputs(argv[2], fp) == EOF)
  syslog(LOG_ERR, "Error  writing into file %s: %s\n", argv[1], strerror(errno));
else
  syslog(LOG_DEBUG, "Writing %s to ", argv[2]); 


 if(fclose(fp) == EOF )
 syslog(LOG_ERR, "Error closing file %s: %s\n", argv[1], strerror(errno));

 closelog();
  return 0;
}
