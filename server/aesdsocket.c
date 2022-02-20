#include<stdio.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<string.h>
#include<netdb.h>
#include<fcntl.h>
#include<unistd.h>
#include<stdlib.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<syslog.h>
#include<signal.h>
#include<sys/stat.h>


#define port 9000

//using global for handlling signals
int socFD, client_socFD;

static void sigintHandler(int sig)
{
   syslog(LOG_NOTICE, "Caught signal, exiting");
   
   remove("/var/tmp/aesdsocketdata");   //Deleting the file  from fs
   close(socFD);                        //and closing the open sockets
   close(client_socFD);
}

int main()
{

  openlog(NULL, 0, LOG_USER);
  char s[INET6_ADDRSTRLEN];
  int rc;

  //Open the server socket
  socFD= socket(AF_INET,SOCK_STREAM,0);
  if(socFD == -1){
   perror("create socket:");
   return -1;
  }
  
  //using sockaddr_in struct to hold address both the server and client
  struct sockaddr_in server, client;
  socklen_t Client_len;
  
  
  memset((void *)&server,0, sizeof(server));
  server.sin_family=AF_INET;
  server.sin_addr.s_addr=INADDR_ANY;
  server.sin_port=htons(port);
 
  // Adding Socket options to allow reuse of Socket
  // reference: Beej's guide
  int yes=1;
  if (setsockopt(socFD,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof yes) == -1) {
    perror("setsockopt");
    return -1;
    }
    


  //Binding  
  rc=bind(socFD, (struct sockaddr *)&server, sizeof(server));
  if(rc == -1)
  {
    close(socFD);
    printf("Error binding: \n");
    perror("bind error:");
    return -1;
  }


    // forking to run the program as daemon
    pid_t pd;
    pd=fork();
    
    if(pd<0)
      return -1;
    else if(pd>0){
      printf("%d", pd);
      return 0;
      }
      
    if( setsid() <0)
     return -1;
     
     
   signal(SIGCHLD, SIG_IGN);
   signal(SIGHUP, SIG_IGN);
 
  
   umask(0);
   chdir("/");
    
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO); 

 
    if(signal(SIGINT, sigintHandler) == SIG_ERR);

    char Data_Byte;
    int count=0;
    int fd;
 
     //opening up file in both read and write mode
     fd= open("/var/tmp/aesdsocketdata", O_TRUNC|O_CREAT|O_RDWR, 0777);
     if(fd<0)
      printf("error opening the file:\n");
      

      
 while(1){
     
     rc=listen(socFD, 5);   
     if(rc == -1)
     {
          perror("listen:");
          return (-1);
     }
     
     
     Client_len = sizeof(client);
     client_socFD = accept(socFD, (struct sockaddr *)&client, &(Client_len));
     //error conditionss 
     if(client_socFD == -1)
     {
       perror("accept:");
       return(-1);
     }
     
     //printing IP address
     inet_ntop(client.sin_family, (struct sockaddr *)&client,s,sizeof s );
     //printf("server: got connection from %s\n", s);
     syslog(LOG_NOTICE, "Accepted connection from %s", s);

     char * str_to_append = (char *) malloc(sizeof(char)) ;
     int realloc_flag;
     int send_file=0;
    
    realloc_flag=0;
   
   while(!send_file) 
   {  
     if(realloc_flag)
     {
       str_to_append=realloc(str_to_append, count+1);
       realloc_flag=0;
       
     }
     
     rc = recv(client_socFD, &Data_Byte, 1, 0);
     
     if(rc==1){
       realloc_flag =1;
       *(str_to_append + count) = Data_Byte;
       count++;
     }
     else 
       printf("error reading:\n");
   
     if(Data_Byte == '\n')
     send_file=1;
   } 
   
         lseek(fd, 0, SEEK_END);
         rc=write(fd, str_to_append, count);
      
         if(rc != count )
            perror("unsuccesful write:\n");
       
         free(str_to_append);// freeing up the allocated memory
         count=0;            //resetting the length
               
         char return_data;
         int success=0;
         
         lseek(fd, 0, SEEK_SET);
         
         while( read(fd, &return_data, 1) != 0)
         {
           //printf("%c \n", return_data);
            while(!success)  //retransmit if transmission failed
            {
               //send return data byte over the socket
               success=send(client_socFD, &return_data, 1, 0);
                if(success == 1){
                 //printf("sent: %c \n", return_data);
                 success=1;  
                 }
                else
                 success=0;
            
            }
            success=0;
            
         }
         send_file=0;
         
         shutdown(client_socFD, 2);
         rc=close(client_socFD);
    if(rc == 0)
      syslog(LOG_NOTICE, "Closed connection from %s", s);
     
    
  }    
  
    
   // shutting down and cllosing up the socket
   shutdown(socFD, 2);
   close(socFD);
   close(fd);
   
      
}
