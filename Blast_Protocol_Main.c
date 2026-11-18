#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <stdbool.h>
#include <string.h> // for memset()
#include <sys/socket.h> // Socket libraries
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>


typedef enum {
    UNDEFINED = 0,
    RECIEVER,
    SENDER
} ProtocolEnd;

typedef struct {
    ProtocolEnd userType;
    int headerSize;
} UserConfig;

typedef void (*CommandFunc) (UserConfig *arg1, const char* arg2);

typedef struct {
	const char *name;
	CommandFunc func;
} CommandEntry;




void setUtype (UserConfig* c, const char* ut_string){
	ProtocolEnd u_type;
	if(strcmp(ut_string, "RECIEVER") == 0) {
		u_type = RECIEVER;
	} else {
		u_type = SENDER;
	}
	c->userType = u_type;
	return;
}

void setHsize (UserConfig* c, const char* hs_string) {
	int h_size = atoi(hs_string);
	c->headerSize = h_size;
	return;
}

static const CommandEntry commands[] ={
	{"-t", setUtype},
	{"-type", setUtype},
	{"-s", setHsize},
	{"-size", setHsize}
};



void procCommand(UserConfig* c, const char* cmd, const char* arg);
void setupApp(char** argv, int argc, UserConfig* c);


typedef struct { // Packet sent by SENDER (initiates the transfer)
    long fileSize;
    int blockSize; // 256, 512, or 1024
    char filename[256];
} FILE_HDR;

typedef struct { // Packet sent by RECIEVER (are they ready to recieve?)
    int status; // 1 = all good, 0 = error
} FILE_HDR_ACK;


// Selection of header size is essential, and needs to be the same between both users.
int main(int argc, char** argv) {
    UserConfig conf; // The configuration we'll use for the program.
    
    setupApp(argv, argc, &conf);

    printf("Header Size: %d\n", conf.headerSize);
    printf("Setup Complete - BLAST Protocol initializing...\n\n");
    
    if(conf.userType == RECIEVER) {
        printf("User Type: RECIEVER\n");
        int sockfd = socket(AF_INET, SOCK_DGRAM, 0); // Create UDP socket
        if (sockfd < 0) {
            perror("Socket creation failed");
            exit(EXIT_FAILURE);
        }

        struct sockaddr_in receiver_addr, sender_addr; 
        memset(&receiver_addr, 0, sizeof(receiver_addr));
        memset(&sender_addr, 0, sizeof(sender_addr));
        
        receiver_addr.sin_family = AF_INET; // Reciever's Address listens on port 8080
        receiver_addr.sin_port = htons(8080); 
        receiver_addr.sin_addr.s_addr = INADDR_ANY; // Use any available network interface

        if (bind(sockfd, (const struct sockaddr *)&receiver_addr, sizeof(receiver_addr)) < 0) { // Bind socket to port (so the OS routes port 8080 traffic correctly)
            perror("Bind failed");
            exit(EXIT_FAILURE);
        }

        FILE_HDR header;
        socklen_t len = sizeof(sender_addr);
        printf("Listening on port 8080 for FILE_HDR...\n");

        recvfrom(sockfd, &header, sizeof(header), 0, (struct sockaddr *)&sender_addr, &len); // Block & Wait (sender's set up packet)

        printf("FILE_HDR received!\n");
        printf("  -> Filename: %s\n", header.filename);
        printf("  -> File Size: %ld bytes\n", header.fileSize);
        printf("  -> Block Size: %d bytes\n", header.blockSize);

        FILE_HDR_ACK ack; // Send ACK to sender (1 is all good!)
        ack.status = 1; 

        printf("Sending FILE_HDR_ACK back to sender...\n");
        sendto(sockfd, &ack, sizeof(ack), 0, (const struct sockaddr *)&sender_addr, len);

        printf("Phase 1 connection established.\n");
        
        close(sockfd); // Close socket (Need to start phase 2 here ***)
    }

    else {
        printf("User Type: SENDER\n"); 
        
        // Create UDP socket
        int sockfd = socket(AF_INET, SOCK_DGRAM, 0); // AF_INET = IPv4, SOCK_DGRAM = UDP
        if (sockfd < 0) {
            perror("Socket creation failed");
            exit(EXIT_FAILURE);
        }

        struct sockaddr_in receiver_addr; // Configure Reciever's Address
        memset(&receiver_addr, 0, sizeof(receiver_addr));
        receiver_addr.sin_family = AF_INET;
        receiver_addr.sin_port = htons(8080); // Port 8080***
        receiver_addr.sin_addr.s_addr = inet_addr("127.0.0.1"); // Used localhoust to test ***

        FILE_HDR header;
        memset(&header, 0, sizeof(header));
        header.fileSize = 1048576; // Test: 1 MB***
        header.blockSize = conf.headerSize; // 256
        strcpy(header.filename, "test_file.txt"); // Test file name (change later)

        printf("BLAST-ing FILE_HDR to receiver...\n"); // Send file to reciever
        sendto(sockfd, &header, sizeof(header), 0, (const struct sockaddr *)&receiver_addr, sizeof(receiver_addr));

        FILE_HDR_ACK ack; // Wait for ack from reciever (FILE_HDR_ACK)
        socklen_t len = sizeof(receiver_addr);
        printf("Waiting for FILE_HDR_ACK...\n");

        recvfrom(sockfd, &ack, sizeof(ack), 0, (struct sockaddr *)&receiver_addr, &len); // Block & Wait (Need to add timeout later***)

        if (ack.status == 1) {
            printf("FILE_HDR_ACK received! Phase 1 connection established.\n");
        }

        close(sockfd); // Close socket (need to add Phase 2***)
    }

    return 0;
}

void procCommand(UserConfig* c, const char* cmd, const char* arg) {
	bool ran = false;
	for(int i = 0; i <(sizeof(commands)/sizeof(commands[0])); i++) {
		if(strcmp(cmd, commands[i].name) == 0){
			commands[i].func(c, arg);
			ran = true;
		} else
			continue;
	}

	if(!ran)
		warn("FLAG: %s, NOT VALID, USING DEFAULT VALUE FOR FLAG",cmd );	
	return;
}

void setupApp(char** argv, int argc, UserConfig* c) {
	for(int i = 1; i < argc; ++i) {
		if((i + 1) >= ( argc )  ) {
			continue;
		} else {
			procCommand(c, argv[i], argv[i + 1]);
			++i;	
		} 	
	} 

	ProtocolEnd p = (*c).userType;
	int h = (*c).headerSize;
	if((p) == UNDEFINED)
		c->userType = RECIEVER;
	if((h) == 0)
		c->headerSize = 256;
	return;
}
