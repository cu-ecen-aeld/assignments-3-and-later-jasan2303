#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <signal.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <sys/queue.h>
#include <pthread.h>
#include <time.h>

#define port 9000



pthread_mutex_t lock;

typedef struct node
{
  char ip_str[INET6_ADDRSTRLEN];
  int socFD;
  pthread_t thread;
  bool thread_complete_status;
  TAILQ_ENTRY(node) nodes;
  
}thread_data_t;

typedef TAILQ_HEAD(head_s, node) head_t;

//using global for handlling signals
int socFD, fd;
head_t head;


void * threadfunc(void* thread_param)
{
    thread_data_t* client_data = ( thread_data_t *) thread_param;
     
     char * str_to_append = (char *) malloc(sizeof(char)) ;
     int realloc_flag=0;
     int send_file=0;
     int rc; 
     char Data_Byte;
     int count=0;
     
     printf("started thread\n");
        
        client_data->thread_complete_status=false;
        while(!send_file) 
        {  
         if(realloc_flag)
         {
          str_to_append=realloc(str_to_append, count+1);
          realloc_flag=0;
       
         }
     
        rc = recv(client_data->socFD, &Data_Byte, 1, 0);
        if(rc==1)
        {
         realloc_flag =1;
         *(str_to_append + count) = Data_Byte;
         count++;
        }
        else
        { 
          printf("error reading:\n");
          //pthread_exit(NULL);         //exiting the thread as the read failed
        }
     
        if(Data_Byte == '\n')
        send_file=1;
       } 
         
       if(pthread_mutex_lock(&lock) !=0) //locking the mutexa s thread uses the shared resource
       {
         perror("mutex lock");
       }  
         
         rc=write(fd, str_to_append, count);
         if(rc != count )
         {
            perror("unsuccesful write:\n");
            //pthread_exit(NULL);
         }
        
         free(str_to_append);// freeing up the allocated memory
         count=0;            //resetting the length
               
         char return_data;
         int success=0;
         
         lseek(fd, 0, SEEK_SET);
         printf("reading from the file\n");
         while( read(fd, &return_data, 1) != 0)
         {
           //printf("%c \n", return_data);
            while(!success)  //retransmit if transmission failed
            {
               //send return data byte over the socket
               success=send(client_data->socFD, &return_data, 1, 0);
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
         if(pthread_mutex_unlock(&lock) != 0)   //unlock the mutex as the thread finished using shared reesource
           perror("mutex unlock");
         
         shutdown(client_data->socFD, 2);
         rc=close(client_data->socFD);
         if(rc == 0)
           syslog(LOG_NOTICE, "Closed connection from %s", client_data->ip_str);
      
      client_data->thread_complete_status=true;
      
      //pthread_exit(NULL); //as client completes the connection
      return thread_param;
}

void append_time_str()
{
   time_t ct;
   time(&ct);
   char tm_str[100];
   
   strftime(tm_str, sizeof(tm_str), "timestamp: %a, %d %b %Y %T %z\r\n", localtime(&ct));
   printf("appending: %s",tm_str);
   lseek(fd, 0, SEEK_END);
   int count=strlen(tm_str);
   int rc=write(fd, tm_str, count);
         if(rc != count )
            perror("unsuccesful write:\n");
   

}

void graceful_exit()
{
   thread_data_t * e = NULL;
    //freeing all the nodes      
    while (!TAILQ_EMPTY(&head))
    {
        e = TAILQ_FIRST(&head);
        TAILQ_REMOVE(&head, e, nodes);
        free(e);
        e = NULL;
    }
}

static void sigintHandler(int sig)
{
   if(sig == SIGALRM)
   {
      printf("caught alrm %d %d \n", sig, SIGALRM);
      append_time_str();
      alarm(10);
   }
   
   if( sig == SIGINT || sig == SIGTERM)
   {
      syslog(LOG_NOTICE, "Caught signal, exiting");
      close(fd);
      remove("/var/tmp/aesdsocketdata");   //Deleting the file  from fs
      close(socFD);                        //and closing the open sockets
      
      //freeing the allocated memories to threads
      graceful_exit();
      
   }
   
}

int main(int argc, char **argv)
{

  openlog(NULL, 0, LOG_USER);
  
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

   if(argc > 1){
   if( !strcmp(argv[1], "-d"))
    { 
      
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
    }
    }
    
    signal(SIGINT, sigintHandler);
    signal(SIGALRM, sigintHandler);
    signal(SIGTERM, sigintHandler);
    
    
    
     //opening up file in both read and write mode
     fd= open("/var/tmp/aesdsocketdata", O_TRUNC|O_CREAT|O_RDWR, 0777);
     if(fd<0)
      printf("error opening the file:\n");
      
    if(pthread_mutex_init(&lock, NULL) != 0)
    {
        printf("\n mutex init failed\n");
        //return 1;
    }
     
     TAILQ_INIT(&head);
     
     bool start_alrm=false;
     printf("starting socket\n");
      
   while(1){
     
     rc=listen(socFD, 5);   
     if(rc == -1)
     {
          perror("listen:");
          return (-1);
     }
     
     thread_data_t *client_data= (thread_data_t *)malloc(sizeof(thread_data_t));
     
     
     
     Client_len = sizeof(client);
     client_data->socFD = accept(socFD, (struct sockaddr *)&client, &(Client_len));
     //error conditionss 
     if(client_data->socFD == -1)
     {
       perror("accept:");
       return(-1);
     }
     
     //printing IP address
     inet_ntop(client.sin_family, (struct sockaddr *)&client,client_data->ip_str,sizeof client_data->ip_str );
     printf("server: got connection from %s\n", client_data->ip_str);
     syslog(LOG_NOTICE, "Accepted connection from %s", client_data->ip_str);
     
     
     if(pthread_create(&(client_data->thread), NULL, &threadfunc, (void *)client_data) != 0)
       perror("pthread create");
     
     //inserting the thread node into the linkedlist
     TAILQ_INSERT_TAIL(&head, client_data, nodes);
     client_data=NULL;
     //free(client_data);
     
     //giving the first alarm call
     if(!start_alrm)
     {
       start_alrm=true;
       printf("starting alarm\n");
       alarm(10);
     }
     
     thread_data_t * e =NULL;       //tracing the running threads to join and freeup
     TAILQ_FOREACH(e, &head, nodes)
     {
        printf("checking for join:\n");
        pthread_join(e->thread,NULL);
        if(e->thread_complete_status)
        {
           //
           TAILQ_REMOVE(&head, e, nodes);
           free(e);
           break;
           //e = NULL;
        }
    
     }
     printf("done checking\n");    
     
   } 
   // shutting down and cllosing up the socket
   shutdown(socFD, 2);
   close(socFD);
   close(fd);
   
      
}
