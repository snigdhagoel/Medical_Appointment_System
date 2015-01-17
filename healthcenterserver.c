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

#define PORT "21515"  // static port that the server uses
#define BACKLOG 10
#define MAXDATASIZE 100
#define MAXCREDSIZE 20

/* struct Availability is used to hold the availability related details.
   It contains a reserved flag which is set to 1 when a particular slot 
   has been assigned to a patient. */

typedef struct Availability{
	int index;
	char *day;
	char *time;
	char *doc;
	char *port;
	int reserved;
}Availability;

// we have a maximum of 6 rows in availabilities.txt
static Availability availabilities[6];

// this code has been taken from Beej's tutorial
void sigchld_handler(int s)
{
    while(waitpid(-1, NULL, WNOHANG) > 0);
}
// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}
int main(void)
{
    int sockfd, new_fd, client_port;
    FILE *users = NULL;
    FILE *availabilitiesFile = NULL;
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr;
    socklen_t sin_size, client_size;
    struct sigaction sa;
    int yes=1;
    struct sockaddr_in my_addr;
    struct sockaddr client_addr;
    socklen_t my_addr_len;
    char s[INET_ADDRSTRLEN];
    int rv;
    char buf[MAXDATASIZE];

    // Assumption - There are only two patients in our system
    char *usersData[2];
    char *username = (char *)malloc(MAXCREDSIZE);
    char *password = (char *)malloc(MAXCREDSIZE);
    
    memset(&hints, 0, sizeof(hints));
    memset(buf, 0, MAXDATASIZE);
    users = fopen("users.txt", "r");
    if(NULL == users)
    {
	printf("users.txt file not found!\n");
	return 1;
    }
    int index = 0;
    /* reading users from users.txt
	usersData[index] is used to hold the credentials per user.
	E.g. for Patient1, usersData[0] = patient1 password111 
    */
    while(!feof(users))
    {
        usersData[index] = (char *)malloc(MAXCREDSIZE*2);
        memset(usersData[index], 0, MAXCREDSIZE*2);
        fgets(usersData[index], MAXDATASIZE, users);
	usersData[index][strlen(usersData[index])-1] = '\0';
	index++;
    }
    fclose(users);
    availabilitiesFile = fopen("availabilities.txt", "r");
    if(availabilitiesFile == NULL)
    {
	printf("availabilities.txt file not found!\n");
	return 1;
    }
    index = 0;
    char availBuffer[MAXDATASIZE];
    /* reading availabilities from availabilities.txt.
	There are 6 entries in the file.
	Each row is mapped to one struct Availability entry. 
    */
    for(index=0; index<6; index++)
    {
	memset(availBuffer, 0, MAXDATASIZE);
	fgets(availBuffer, MAXDATASIZE, availabilitiesFile);
	availabilities[index].index = index+1;
	strtok(availBuffer, " ");
	// day has 3 characters and last character is reserved for '\0'
	availabilities[index].day = (char *)malloc(4);
	strcpy(availabilities[index].day, strtok(NULL, " "));
	// time has 4 characters and last character is reserved for '\0'
	availabilities[index].time = (char *)malloc(5);
	strcpy(availabilities[index].time, strtok(NULL, " "));
	// doc has 4 characters and last character is reserved for '\0'
	availabilities[index].doc = (char *)malloc(5);
	strcpy(availabilities[index].doc, strtok(NULL, " "));
	// port has 5 characters and last character is reserved for '\0'
	availabilities[index].port = (char *)malloc(6);
	strcpy(availabilities[index].port, strtok(NULL, ""));
	availabilities[index].port[strlen(availabilities[index].port)-1] = '\0';
	availabilities[index].reserved = 0;
    } 
    // closing the availabilities.txt file
    fclose(availabilitiesFile);

    // The code for creation of TCP socket has been taken from Beej's tutorial
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP
    if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }
    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }
        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            continue;
        }
        break;
    }
    if (p == NULL)  {
        fprintf(stderr, "server: failed to bind\n");
        return 2;
    }
    my_addr_len = sizeof (my_addr);

    // gethostbyname() is used to retrieve the IP address of the server
    char *hostname = (char *)malloc(MAXDATASIZE);
    gethostname(hostname, MAXDATASIZE);
    struct hostent *my_addr_info = gethostbyname(hostname);
    struct in_addr **addr_list;
    addr_list = (struct in_addr **)my_addr_info->h_addr_list;

    printf("Phase 1: The Health Center Server has port number %s and IP address %s\n", PORT, inet_ntoa(*addr_list[0]));
    freeaddrinfo(servinfo); // all done with this structure
    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }
 sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }
    /* fork() has not been used - the patients are processed in sequential order
    */
    while(1) {  // main accept() loop
        sin_size = sizeof their_addr;
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if (new_fd == -1) {
            perror("accept");
            continue;
        }
	    int numbytes = 0;
	   
	    /* receiving authentication service request from a patient
		of the form 'authenticate username password'
	    */
            if ((numbytes = recv(new_fd, buf, MAXDATASIZE-1, 0)) == -1)
            {
		perror("recv");
		exit(1);
	    }
	    buf[numbytes] = '\0';
	    int matchFlag = 0;
	    if(strncmp("authenticate", buf, 12) == 0)
	    {
		/* Here I have removed 'authenticate' from the received
		   message and compared the remaining string of the form
		   'username password' with the user data stored in
		   usersData[index] structure. usersData[] contains the data
		   in the same format.
		*/

		char *user = strtok(buf+13, "");
		char *displayUser = (char *)malloc(MAXDATASIZE);
		strcpy(displayUser, user);
		strcpy(username, strtok(displayUser, " "));
		strcpy(password, strtok(NULL, ""));
		printf("Phase 1: The Health Center Server has received request from patient with username %s and password %s.\n", username, password);
		for(index=0; index < 2; index++)
		{
			if(strcmp(user, usersData[index]) == 0)
			{
				// if a match is found, matchFlag is set to 1
				matchFlag = 1;
				break;			
			}
		}
		memset(buf, 0, MAXDATASIZE);
		if(matchFlag == 1)
		{
			// if a match is found, 'success' is sent to the patient
			strcpy(buf, "success\0");
		}
		else
		{
			// if a match is not found, 'failure' is sent
			strcpy(buf, "failure\0");
		}
		if((numbytes = send(new_fd, buf, MAXDATASIZE-1, 0)) == -1)	
		{
			perror("send");
			exit(1);
		}
		printf("Phase 1: The Health Center Server sends the response %s to patient with username %s.\n", buf, username);
		if(matchFlag == 0){
			// if no match is found, close child connection
			close(new_fd); 
			continue;
		}
	    }
            if ((numbytes = recv(new_fd, buf, MAXDATASIZE-1, 0)) == -1)
            {
                perror("recv");
                exit(1);
            }
            buf[numbytes] = '\0';
	    if(strncmp("available", buf, 12) == 0)
	    {
		client_size = sizeof client_addr;
		getpeername(new_fd, &client_addr, &client_size);
		client_port =(int)ntohs((struct sockaddr_in *)(&client_addr))->sin_port;
		inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s, sizeof s);

		printf("Phase 2: The Health Center Server, receives a request for available time slots from patient with port number %d and IP address %s.\n", client_port, s);
		memset(buf, 0, MAXDATASIZE);
		for(index = 0; index < 6; index++)
		{
			/* For each of the rows in the availabilities matrix
			   we check if it has been reserved by some patient.
			   If not, we append index, day and time for that entry
			   in buf. Else, we leave that entry.
			   Buf is sent to the patient.
			*/

			if(availabilities[index].reserved == 0)
			{
				char* c_index = (char *) malloc (1);
				*c_index = availabilities[index].index + '0'; 
				strncat(buf, c_index, 1);
				strcat(buf, " ");
				strcat(buf, availabilities[index].day);
				strcat(buf, " ");
				strcat(buf, availabilities[index].time);
				strcat(buf, "\n");
			}
		}
		if((numbytes = send(new_fd, buf, MAXDATASIZE-1, 0)) == -1){
			perror("send");
			exit(1);
		}
		printf("Phase 2: The Health Center Server sends available time slots to patient with username %s.\n", username);
	    }
	    memset(buf, 0, MAXDATASIZE);
	    // Receiving the selected time slot from the patient
	    if((numbytes = recv(new_fd, buf, MAXDATASIZE-1, 0)) == -1){
		perror("recv");
		exit(1);
	    }
	    buf[numbytes] = '\0';
	    printf("Phase 2: The Health Center Server receives a request for appointment %s from patient with port number %d and username %s.\n", buf, client_port, username);
	    if(strncmp("selection", buf, 9) == 0)
	    {
		char *choice = strtok(buf+10, "");
		int avail_index = *choice - '0';
		avail_index--;
		memset(buf, 0, MAXDATASIZE);
		if(availabilities[avail_index].reserved == 0)
		{
			/* We check if the requested time slot hasn't already
			   been reserved by some other patient. However, since
			   we are dealing with non-concurrent processes,
			   this situation will not arise here. The patient requests
			   are processed in sequential order here.
			   The reserved flag for the requested availability is
			   set to 1.
			*/
			availabilities[avail_index].reserved = 1;
			printf("Reserved keyword set: %d\n", availabilities[avail_index].reserved);
			strcpy(buf, availabilities[avail_index].doc);
			strcat(buf, " ");
			strcat(buf, availabilities[avail_index].port);
		}
		else
		{
			strcpy(buf, "notavailable");
		}
		buf[strlen(buf)] = '\0';
		printf("Phase 2: The Health Center Server confirms/rejects the following appointment '%s' to patient with username %s.\n", buf, username);
		if((numbytes = send(new_fd, buf, MAXDATASIZE-1, 0)) == -1){
		
			perror("send");
			exit(1);
		}
	    } 
            close(new_fd);
    }
    return 0;
    
}
