#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <syslog.h>
#include <pthread.h>
#include <time.h>
#include <sys/queue.h>
#include <stdbool.h>

/******************************************************************************
 *
 *                               DEFINITIONS
 *
 ******************************************************************************/
#define PORT "9000"
#define BACKLOG (10)

#define USE_AESD_CHAR_DEVICE (1)
#if (USE_AESD_CHAR_DEVICE == 1)
  #define WRITE_FILE "/dev/aesdchar"
#else
  #define WRITE_FILE "/var/tmp/aesdsocketdata"
#endif

#define LOG_ERROR(msg, ...) 	syslog(LOG_ERR, msg, ##__VA_ARGS__) 
#define LOG_MSG(msg, ...)	syslog(LOG_INFO, msg, ##__VA_ARGS__) 
#define LOG_WARN(msg, ...)  	syslog(LOG_WARNING, msg, ##__VA_ARGS__)  

/******************************************************************************
 *
 *                                  GLOBALS
 *
 ******************************************************************************/

static int servfd, clientfd, fd;
char *rdBuff=NULL;
int totalBytes=0;
static volatile bool globalExit = false;
pthread_t timerThread;

// Initialize mutex
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

// Thread status definition
typedef enum{
	T_COMPLETE,
	T_RUNNING
}T_STATUS;

// structure to store thread's parameters
typedef struct{
	pthread_t thread;
	int clientfd;
	T_STATUS thread_status;
}threads_data; 

// Singly linked list to manage threads
typedef struct slist_threads thread_list;

struct slist_threads{
	threads_data thread_data;
	SLIST_ENTRY(slist_threads) entries;
};
SLIST_HEAD(slisthead, slist_threads) head;

/******************************************************************************
 *
 * 				    FUNCTIONS
 *
 ******************************************************************************/ 

// Gracefull exit
void closeAll()
{
	
	// free read-write buffer
	if(rdBuff != NULL){
		free(rdBuff);
		rdBuff = NULL;
	}
		
	// close file descriptors	
	shutdown(servfd, SHUT_RDWR);
	close(servfd);
	shutdown(clientfd, SHUT_RDWR);
	close(clientfd);

	thread_list *tNode = NULL;
	
    	while(!SLIST_EMPTY(&head)){
    		tNode = SLIST_FIRST(&head);
    		SLIST_REMOVE_HEAD(&head, entries);
    		free(tNode);
	}
	tNode = NULL;
	
	
	//close file descripter
	close(fd);
	// delete the data file
	remove(WRITE_FILE);
	

	//close the logging
	closelog();
	
	// join timmer thread
	pthread_join(timerThread,NULL);
	
	pthread_mutex_destroy(&mutex);
	
	exit(0);
}


// to run as daemon 
void runAsDaemon()
{

	pid_t pid, sid;
	
	// signals to be ignored
	signal(SIGCHLD, SIG_IGN);
        signal(SIGHUP, SIG_IGN);
	
	//Fork the parent process
        pid = fork();
	
	if(pid < 0){
            perror("fork():");
            LOG_ERROR("fork(): %s", strerror(errno));
            closeAll();
            exit(EXIT_FAILURE);
        }
        
        //if no error, exit the parent process
        if(pid > 0)
            exit(EXIT_SUCCESS);

        // Change the file mode mask
        umask(0);


        // Creating new SID for the child process
        sid = setsid();
        if(sid < 0){
            perror("setsid()():");
            LOG_ERROR("setsid(): %s", strerror(errno));
            exit(EXIT_FAILURE);
        }

        // Change the current working directory
        if((chdir("/")) < 0){
            perror("chdir():"); 
            LOG_ERROR("chdir(): %s", strerror(errno));	 	
            exit(EXIT_FAILURE);
	}
	
        // redirect standard IO to /dev/null
    	int extf = open("/dev/null", O_RDWR);
   	dup2(extf, STDIN_FILENO);
    	dup2(extf, STDOUT_FILENO);
    	dup2(extf, STDERR_FILENO);
    	
        // close std file descriptors
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
        close(extf);
}

// receives and sends throught the sockets
void *socketHandler(void *data)
{
	
	char buffer[1024];
	
	threads_data *thread_data = (threads_data*)data;
	
	rdBuff = malloc(sizeof(char));
	
	int pe = pthread_mutex_lock(&mutex);
	if(pe != 0){
		LOG_ERROR("Error in mutex lock: %s",strerror(errno));
		closeAll();
		exit(EXIT_FAILURE);
	}
	
	
	// Receive and write to WRITE_FILE
	fd =open(WRITE_FILE,O_WRONLY);
	if(fd < 0){
		LOG_ERROR("Error opening %s",WRITE_FILE);
		closeAll();
		exit(1);
	}
	
        lseek(fd,0,SEEK_END);
        unsigned long index = 0;
	while(1){
	
		ssize_t rBytes = recv(thread_data->clientfd, buffer, sizeof(buffer), 0);
		if(rBytes == -1){
			LOG_ERROR("recv():%s", strerror(errno));
			closeAll();
			exit(1);
		}
		rdBuff = realloc(rdBuff,index+rBytes);
		memcpy(rdBuff+index,buffer,rBytes);
		index += rBytes;
		totalBytes += rBytes;
		if(buffer[rBytes-1] == '\n'){
			
			if(write(fd, rdBuff, index) == -1){
				LOG_ERROR("write():%s", strerror(errno));
				closeAll();
				exit(1);
			}
			break;
		}
	}
	close(fd);	
	free(rdBuff);
	rdBuff=NULL;	
	
	// Send complete data from WRITE_FILE to the client
	fd =open(WRITE_FILE,O_RDONLY);
	if(fd < 0){
		LOG_ERROR("Error opening %s",WRITE_FILE);
		closeAll();
		exit(1);
	}
	ssize_t sendBytes = totalBytes;
	rdBuff = realloc(rdBuff,sendBytes);
	lseek(fd,0,SEEK_SET);
	while(1){		
		
		int rBytes = read(fd, rdBuff,sendBytes);
		if(rBytes == -1){
			LOG_ERROR("read():%s", strerror(errno));
			closeAll();
			exit(1);
		}
		if(send(thread_data->clientfd, rdBuff, rBytes, 0) == -1){
			LOG_ERROR("send():%s", strerror(errno));
			closeAll();
			exit(1);
		}
		sendBytes -= rBytes;
		if(sendBytes == 0)
			break;
	}
	free(rdBuff);
	rdBuff=NULL;
	close(fd);
	close(thread_data->clientfd);
	thread_data->thread_status = T_COMPLETE;
	
	pe = pthread_mutex_unlock(&mutex);
	if(pe != 0){
		LOG_ERROR("Error in mutex unlock: %s", strerror(errno));
		closeAll();
		exit(EXIT_FAILURE);
	}

	//shutdown(clientfd, SHUT_RDWR);
	
	pthread_exit(NULL);
}

#if (USE_AESD_CHAR_DRIVER == 0)
// timer thread
void *insert_timestamp()
{
	while(!globalExit){
	
		sleep(10);
		char outstr[100];
		time_t t;
	   	struct tm *tmp;

		t = time(NULL);
		tmp = localtime(&t);
	   	
	   	if (tmp == NULL) {
		       LOG_ERROR("localtime error: %s",strerror(errno));
		       exit(EXIT_FAILURE);
	    	}

		int len_outstr = strftime(outstr, sizeof(outstr),"timestamp: %Y %b %e %H:%M:%S\n", tmp);
	   	if ( len_outstr == 0) {
		       fprintf(stderr, "strftime returned 0");
		       exit(EXIT_FAILURE);
		}
		
		int pe = pthread_mutex_lock(&mutex);
		if(pe != 0){
			LOG_ERROR("Error in mutex lock: %s",strerror(errno));
			closeAll();
			exit(EXIT_FAILURE);
		}
		
		//write to file
		fd =open(WRITE_FILE,O_WRONLY);
		if(fd < 0){
			LOG_ERROR("Error opening %s",WRITE_FILE);
			closeAll();
			exit(1);
		}
		
		lseek(fd,0,SEEK_END);

		
		if(write(fd, outstr, len_outstr) == -1){
			LOG_ERROR("write():%s", strerror(errno));
			closeAll();
			exit(1);
		}
		close(fd);
		//free(rdBuff);
		totalBytes += len_outstr;
			
		pe = pthread_mutex_unlock(&mutex);
		if(pe != 0){
			LOG_ERROR("Error in mutex unlock: %s",strerror(errno));
			closeAll();
			exit(EXIT_FAILURE);
		}
		
	}
	
	pthread_exit(NULL);
}
#endif

// SIGNIT and SIGTERM signal handler
void signal_handler(int signo)
{
	LOG_WARN("Caught signal, exiting\n");
	globalExit = true;
    	closeAll();
	exit(0);
    
}
  
// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

/******************************************************************************
 *
 * 		                 MAIN FUNCTIONS
 *
 ******************************************************************************/ 


int main(int argc, char **argv)
{
	
	openlog(NULL,0,LOG_USER);

	int pe = pthread_mutex_init(&mutex,NULL);
	
	if(pe != 0){
		LOG_ERROR("Error in mutex init: %s",strerror(errno));
		closeAll();
		exit(EXIT_FAILURE);
	}	
	
	// thread list init
	SLIST_INIT(&head);
	
	
	if(signal(SIGINT, signal_handler) == SIG_ERR){
		LOG_ERROR("Error while registering SIGINT");
		closeAll();
		exit(EXIT_FAILURE);
	}


	if(signal(SIGTERM, signal_handler) == SIG_ERR){
		LOG_ERROR("Error while registering SIGTERM");
		closeAll();
		exit(EXIT_FAILURE);
	}
	
	
	
	bool flagDaemon = false;
	if(argc >=2 && (strcmp(argv[1],"-d") == 0))
		flagDaemon = true;
	else if(argc >=2){
		//printf("Invalid arguments\n");
		LOG_ERROR("Invalid arguments");
		exit(1);	
	}
	
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_storage their_addr;
	socklen_t sin_size;
	int yes = 1;
	char s[INET6_ADDRSTRLEN];
	int rv;
	
	totalBytes = 0;
	int firstThread = 0;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;     
	hints.ai_socktype = SOCK_STREAM; 
	hints.ai_flags = AI_PASSIVE;     

	// get address info
	rv = getaddrinfo(NULL, PORT , &hints, &servinfo);
	
	// get address info error check
	if (rv  != 0) {
    		fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(rv));
    		LOG_ERROR("getaddrinfo error: %s\n", gai_strerror(rv));
    		exit(1);
	}


	// loop through all the results and bind to the first we can
    	for(p = servinfo; p != NULL; p = p->ai_next) {
        	if ((servfd = socket(p->ai_family, p->ai_socktype,
                	p->ai_protocol)) == -1) {
            		perror("server: socket");
            		LOG_ERROR("server: socket: %s", strerror(errno));
            		continue;
        	}

        	if (setsockopt(servfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                	sizeof(int)) < 0 ) {
            		perror("setsockopt");
            		LOG_ERROR("setsocket: %s", strerror(errno));
            		exit(1);
        	}

        	if (bind(servfd, p->ai_addr, p->ai_addrlen) == -1) {
            		close(servfd);
            		perror("server: bind");
            		LOG_ERROR("server: bind:  %s", strerror(errno));
            		continue;
        	}

        	break;
    	}

    	freeaddrinfo(servinfo); // all done with this structure 
	
	if (p == NULL)  {
        	fprintf(stderr, "server: failed to bind\n");
        	LOG_ERROR("server:failed to bind: %s", strerror(errno));
        	exit(1);
    	}


	if(flagDaemon)
		runAsDaemon();
    	
    	if (listen(servfd, BACKLOG) == -1) {
        	perror("listen");
        	LOG_ERROR("listen: %s", strerror(errno));
        	exit(1);
    	}

	
	fd = open(WRITE_FILE, O_CREAT, 0644);	
	if(fd < 0){
		LOG_ERROR("Error opening %s",WRITE_FILE);
		closeAll();
		return -1;
	}
	close(fd);


	while(!globalExit) {  // main accept() loop
		
		// Accept connection
		sin_size = sizeof their_addr;

		clientfd = accept(servfd, (struct sockaddr *)&their_addr, &sin_size);
		if (clientfd == -1) {
		    perror("accept");
		    closeAll();
		    exit(1);
		}

		inet_ntop(their_addr.ss_family,
		    get_in_addr((struct sockaddr *)&their_addr),
		    s, sizeof s);
		    
		LOG_MSG("Accepted connection from %s", s);

		
		// create thread and append to thread list
		thread_list *tNode = (thread_list*) malloc(sizeof(thread_list));
		SLIST_INSERT_HEAD(&head, tNode, entries);
		
		tNode->thread_data.clientfd = clientfd;
		tNode->thread_data.thread_status = T_RUNNING;
		
		pthread_t thread;
		pthread_create(&thread, NULL, socketHandler, &(tNode->thread_data));
		tNode->thread_data.thread = thread;
		
		#if (USE_AESD_CHAR_DRIVER == 0)
		if(!firstThread){
			firstThread = 1;

			pthread_create(&timerThread,NULL,insert_timestamp,NULL);

		}
		#endif

		// Traverse through the thread list and remove, whichever is complete

		SLIST_FOREACH(tNode, &head, entries){
			pthread_join(tNode->thread_data.thread,NULL);
			if(tNode->thread_data.thread_status == T_COMPLETE){
				SLIST_REMOVE(&head, tNode, slist_threads, entries);
				free(tNode);
				tNode=NULL;
				LOG_MSG("Closed connection from %s", s);
				break;

			}
			
		}
    		
    	}
    	
    	
	closeAll();
	return 0;
}
