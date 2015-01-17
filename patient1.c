#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#define PORT "21515" // the port client will be connecting to 
#define MAXDATASIZE 100 // max number of bytes we can get at once 
#define MAXCREDSIZE 20 // max number of bytes we need for holding credentials

// get sockaddr, IPv4 or IPv6:
// This function has been taken from Beej's tutorial 
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}
int main(int argc, char *argv[])
{
    int sockfd, numbytes; 
    char buf[MAXDATASIZE];
    struct addrinfo hints, *servinfo, *p;
    int rv;
    struct sockaddr_in my_addr;
    socklen_t my_addr_len;
    char s[INET_ADDRSTRLEN];
    FILE *credentials=NULL;
    char *userName, *password, credentialLine[2*MAXCREDSIZE];
    char **availabilities;
    memset(&hints, 0, sizeof hints);
    memset(&userName, 0, MAXCREDSIZE);
    memset(&password, 0, MAXCREDSIZE);

    // the credentials for patient1 are read from patient1.txt

    credentials = fopen("patient1.txt","r");
    if(NULL == credentials)
    {
	fprintf(stderr, "patient1.txt file not found.\n");
	return 1;
    }
    // reading credentials from file - one line at a time
    while(!feof(credentials))
    {
	fgets(credentialLine, MAXDATASIZE, credentials);
    }
    userName = strtok(credentialLine, " ");
    password = strtok(NULL, " \n"); 
    // close the file after reading the credentials 
    fclose(credentials);

    // The code for creating a socket has been taken from Beej's tutorial
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if ((rv = getaddrinfo("nunki.usc.edu", PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }
    // loop through all the results and connect to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }
        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("client: connect");
            continue;
        }
        break;
    }
    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        return 2;
    }
    freeaddrinfo(servinfo); // all done with this structure
    my_addr_len = sizeof (my_addr);
    
    // getsockname() is used to retrieve the dynamic TCP port and IP address of the client
    int getsock_check = getsockname(sockfd, (struct sockaddr *)&my_addr, (socklen_t *)&my_addr_len);
    if(getsock_check == -1){
	perror("getsockname");
	exit(1);
    }
    printf("Phase 1: Patient 1 has TCP port number %d and IP address %s.\n",(int)ntohs(my_addr.sin_port), inet_ntoa(my_addr.sin_addr));
    // preparing authentication data to be sent to the server - "authenticate username password" 
    memset(buf, 0, MAXDATASIZE);
    strcat(buf, "authenticate ");
    strcat(buf, userName);
    strcat(buf, " ");
    strcat(buf, password);
    if ((numbytes = send(sockfd, buf, MAXDATASIZE-1, 0)) == -1) {
        perror("send");
        exit(1);
    }
    printf("Phase 1: Authentication request from Patient 1 with username %s and password %s has been sent to the Health Center Server.\n", userName, password);
    memset(buf, 0, MAXDATASIZE);
    if((numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0)) == -1)
    {
	perror("recv");
	exit(1);
    }
    buf[numbytes] = '\0';
    printf("Phase 1: Patient 1 authentication result: %s\n", buf);
    if(strcmp(buf, "failure") == 0)
    {
	close(sockfd);
	exit(0);
    }
    printf("Phase 1: End of Phase 1 for Patient 1.\n");

    // sending 'available' to the healthcenterserver
    memset(buf, 0, MAXDATASIZE);
    strcpy(buf, "available");
    if((numbytes = send(sockfd, buf, MAXDATASIZE-1, 0)) == -1) {
	perror("send");
	exit(1);
    }

    // buf has been reused for each send and receive between patient and server
    memset(buf, 0, MAXDATASIZE);
    if((numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0)) == -1)
    {
        perror("recv");
        exit(1);
    }
    buf[numbytes] = '\0';
    
    /* availabilities is used to hold the slots received from the server
	on a per line basis, that is, availabilities[0] represents the
	first slot of the form '1 Tue 03pm' */ 
    availabilities = (char **) malloc (sizeof(char *) * 6);
    availabilities[0] = (char *)malloc(MAXCREDSIZE);
    strcpy(availabilities[0], strtok(buf, "\n"));
    int index = 0; 
    int numberOfEntries=6;
    printf("Phase 2: The following appointments are available for Patient 1:\n");
    printf("%s\n", availabilities[0]);
    for(index = 1; index < 6; index++){
	availabilities[index] = (char *) malloc(MAXCREDSIZE);
	char *token = strtok(NULL, "\n");

	/* We read the received string from the server tokenizing on
	   '\n'. If the next token is NULL, we break out of the loop - 
	   the number of received availabilities can be less than 6.
	*/
	if(token != NULL){
		strcpy(availabilities[index], token);
		printf("%s\n", availabilities[index]);
	}
	else{
		numberOfEntries = index;
		break;
	}
    }
    printf("Please enter the preferred appointment index and press enter: ");
    char* choice;
    int match = 0;
    choice = (char *)malloc(1);
    scanf("%c", choice);

    /* check for validity of the user input - user should enter a time slot only
	from the displayed slots */
    while(1)
    {
	for(index = 0; index < numberOfEntries; index++)
    	{
		if(strncmp(availabilities[index], choice, 1) == 0)
		{
			match = 1;
			break;
		}
    	}
	if(match == 1)
		break;
	else
	{
		printf("Invalid choice. Please enter a valid appointment index and press enter: ");
		scanf("%c", choice);
	}
    }
    memset(buf, 0, MAXDATASIZE);
    strcpy(buf, "selection ");
    strncat(buf, choice, 1);
    buf[strlen(buf)] = '\0';
    // sending the selected time slot to the server - 'selection choice'
    if((numbytes = send(sockfd, buf, MAXDATASIZE-1, 0)) == -1)
    {
    	perror("send");
    	exit(1);
    }
    memset(buf, 0, MAXDATASIZE);
    if((numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0)) == -1)
    {
	perror("recv");
	exit(1);
    }
    buf[numbytes] = '\0';

    // closing the socket - no further communication with healthservercenter needed
    close(sockfd);
    if(strcmp(buf, "notavailable") == 0){
	printf("Phase 2: The requested appointment from Patient 1 is not available. Exiting...\n");
	// exiting the process if 'notavailable' received from server
	exit(0);
    }

    // the port for doctor is 5 digit long and the last index is reserved for '\0'
    char *doc_port = (char *)malloc(6);

    // the doc_name is 4 digit long and the last index is reserved for '\0'
    char *doc_name = (char *)malloc(5);

    strcpy(doc_name, strtok(buf, " "));
    doc_name[4] = '\0';
    strcpy(doc_port, strtok(NULL,""));
    printf("Phase 2: The requested appointment is available and reserved to Patient 1. The assigned doctor port number is %s.\n", doc_port);

    // read the patient's insurance from patient1insurance.txt
    FILE *insuranceFile;
    insuranceFile = fopen("patient1insurance.txt", "r");
    fgets(buf, MAXDATASIZE, insuranceFile);
    buf[strlen(buf)-1] = '\0';
  
    // code for UDP socket creation has been taken from Beej's tutorial
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    if((rv = getaddrinfo("nunki.usc.edu", doc_port, &hints, &servinfo)) != 0){
	printf("getaddrinfo: %s\n", gai_strerror(rv));
	return 1;
    }
    for(p = servinfo; p != NULL; p = p->ai_next){
	if((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1){
		perror("socket");
		continue;
	}
	break;
    }
    if(p == NULL)
    {
	printf("Talker failed to bind socket.\n");
	return 2;
    }

    // sending the patient insurance to the doctor
    if((numbytes = sendto(sockfd, buf, strlen(buf), 0, p->ai_addr, p->ai_addrlen)) == -1){
	perror("talker: sendto");
	exit(1);
    }

    /* my_addr and my_addr_len are needed as input parameters in getsockname()
	These are used to hold the IP address and port number of the patient UDP
	socket. */ 
    memset(&my_addr, 0, sizeof my_addr);
    my_addr_len = sizeof my_addr;

    getsock_check = getsockname(sockfd, (struct sockaddr *)&my_addr, (socklen_t *)&my_addr_len);
    if(getsock_check == -1)
    {
        perror("getsockname");
        exit(1);
    }

    /* The below piece of code is used to retrieve the patient's IP address
	using gethostbyname() function */
    char *hostname = (char *)malloc(MAXDATASIZE);
    gethostname(hostname, MAXDATASIZE);
    struct hostent *my_addr_info = gethostbyname(hostname); 
    struct in_addr **addr_list;
    addr_list = (struct in_addr **)my_addr_info->h_addr_list;
    printf("Phase 3: Patient 1 has a dynamic UDP port number %d and IP address %s.\n", (int) ntohs
(my_addr.sin_port), inet_ntoa(*addr_list[0]));

    printf("Phase 3: The cost estimation request from Patient 1 with insurance plan %s has been sent to the doctor with port number %s and IP address %s.\n", buf, doc_port, inet_ntoa((*(struct sockaddr_in *)p->ai_addr).sin_addr)); 

    // servinfo is freed after printf because it is used to display doc's IP address
    freeaddrinfo(servinfo); 
    memset(buf, 0, MAXDATASIZE);
    if((numbytes = recvfrom(sockfd, buf, MAXDATASIZE, 0, p->ai_addr, &(p->ai_addrlen))) == -1){
	perror("recvfrom");
	exit(1);
    }
    buf[numbytes] = '\0';
    printf("Phase 3: Patient 1 receives %s$ estimation cost from doctor with port number %s and name %s.\n", buf, doc_port, doc_name);
    printf("End of Phase 3 for Patient 1.\n");
    close(sockfd);
    return 0;
}
