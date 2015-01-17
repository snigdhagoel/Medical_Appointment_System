#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define MYPORT "41515"   // the static UDP port the patients will be connecting to
#define MAXBUFLEN 100

// The following functions have been taken from Beej's tutorial
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
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    int numbytes;
    int client_port;
    struct sockaddr_storage their_addr;
    struct sockaddr_in my_addr;
    char buf[MAXBUFLEN];
    socklen_t addr_len, my_addr_len;
    char s[INET_ADDRSTRLEN];
    FILE *docFile;
    // insurances is used to hold the information read from doc1.txt
    char **insurances;
    int index = 0;
    docFile = fopen("doc1.txt", "r");
    insurances = (char **)malloc(3 * sizeof (char *));
    for(index = 0; index < 3; index++)
    {
	/* doc1.txt contains entries of the form 'insurance1 30'
	   insurances[0] contains first line of the file and so on.
	   There are total 3 entries corresponding to 3 insurances in
	   the file.
	*/
	insurances[index] = (char *)malloc(MAXBUFLEN);
	fgets(insurances[index], MAXBUFLEN, docFile);
	insurances[index][strlen(insurances[index])-1] = '\0';
    }
    // The code for creation of a UDP socket has been taken from Beej's tutorial
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET; // set to AF_INET to force IPv4
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE; // use my IP
    if ((rv = getaddrinfo(NULL, MYPORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }
    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("listener: socket");
            continue;
        }
        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("listener: bind");
            continue;
        }
        break;
    }
    if (p == NULL) {
        fprintf(stderr, "listener: failed to bind socket\n");
        return 2;
    }

    // gethostbyname() function is used to retrieve the doctor's IP address.
    my_addr_len = sizeof (my_addr);
    char *hostname = (char *)malloc(MAXBUFLEN);
    gethostname(hostname, MAXBUFLEN);
    struct hostent *my_addr_info = gethostbyname(hostname);
    struct in_addr **addr_list;
    addr_list = (struct in_addr **)my_addr_info->h_addr_list;

    printf("Phase 3: Doctor 1 has a static UDP port %s and IP address %s.\n", MYPORT, inet_ntoa(*addr_list[0]));
    freeaddrinfo(servinfo);
    while(1){
	memset(&their_addr, 0, sizeof their_addr);
	addr_len = sizeof their_addr;
	// receiving insurance from the patient
    	if ((numbytes = recvfrom(sockfd, buf, MAXBUFLEN-1 , 0,
        	(struct sockaddr *)&their_addr, &addr_len)) == -1) {
        	perror("recvfrom");
        	exit(1);
    	}
	// this line has been added to retrieve the patient port from their_addr structure
	client_port = (int)ntohs(((struct sockaddr_in *)(&their_addr))->sin_port);
    	buf[numbytes] = '\0';
	printf("Phase 3: Doctor 1 receives the request from the patient with port number %d and the insurance plan %s.\n", client_port, buf);

	/* We compare the insurance received with the content read from
	   'doc1.txt' file. We send back the corresponding cost to the 
	   patient.
	*/
	for(index = 0; index < 3; index++)
	{
		if(strncmp(buf, insurances[index], 10) == 0)
		{
			char *insuranceMatch = (char *)malloc(strlen(insurances[index]));
			strcpy(insuranceMatch, insurances[index]);
			strtok(insuranceMatch, " ");
			memset(buf, 0, MAXBUFLEN);
			strcpy(buf, strtok(NULL, ""));
			buf[strlen(buf)] = '\0';
			break;
		}
	}
    	if((numbytes = sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr *)&their_addr, addr_len)) == -1){
        	perror("talker: sendto");
        	exit(1);
    	}
	printf("Phase 3: Doctor 1 has sent estimated price %s$ to patient with port number %d.\n",buf, client_port);
    }
    close(sockfd);
    return 0;
}
