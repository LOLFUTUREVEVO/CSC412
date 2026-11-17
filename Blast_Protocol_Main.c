#include <stdio.h>
#include <stdlib.h>
#include <string.h> // for memset()
#include <sys/socket.h> // Socket libraries
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

typedef enum {
    RECIEVER,
    SENDER
} ProtocolEnd;

typedef struct {
    ProtocolEnd userType;
    int headerSize;
} UserConfig;

typedef struct { // Packet sent by SENDER (initiates the transfer)
    long fileSize;
    int blockSize; // 256, 512, or 1024
    char filename[256];
} FILE_HDR;

typedef struct { // Packet sent by RECIEVER (are they ready to recieve?)
    int status; // 1 = all good, 0 = error
} FILE_HDR_ACK;

void setupApp(int argc, char** argv, UserConfig* c);

// Selection of header size is essential, and needs to be the same between both users.
int main(int argc, char** argv) {
    UserConfig conf; // The configuration we'll use for the program.
    
    setupApp(argc, argv, &conf);
    
    if(conf.userType == RECIEVER) {
        printf("User Type: RECIEVER\n");
        // Need to create UDP socket / bind, wait for file and send ack ***
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

    printf("Header Size: %d\n", conf.headerSize);
    printf("Setup Complete - BLAST Protocol initializing...\n\n");
    return 0;
}

void setupApp(int argc, char** argv, UserConfig* c) {
    if(argc <= 1) { // Check argc without using sizeof() to avoid array decay
        c->userType = RECIEVER;
        c->headerSize = 256; // Default header size
    } else {
        c->userType = SENDER; // If arguments, then SENDER
        c->headerSize = 256; 
    }
}