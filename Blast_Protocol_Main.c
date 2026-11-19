#include <stdio.h>
#include <stdlib.h>
#include <err.h>
#include <stdbool.h>
#include <string.h> // for memset()
#include <sys/socket.h> // Socket libraries
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#define MAX_RECORDS_PER_PKT 16
#define MAX_SEGMENTS 16 // Max distinct (st_rec, end_rect) segments per packet           
#define MAX_RECORD_SIZE 1024
#define DEFAULT_BLAST_SIZE 200 // Default M (200 - 10000 records per blast)
#define MAX_MISS_RANGES 500 // Max missing intervals per REC_MISS packet

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
    int blastSize; // M records per blast (200 - 10000)
    char filename[256];
} FILE_HDR;

typedef struct { // Packet sent by RECIEVER (are they ready to recieve?)
    int status; // 1 = all good, 0 = error
} FILE_HDR_ACK;

typedef enum { // Packet types and structs (for Phase 2)
    PKT_DATA = 1,
    PKT_IS_BLAST_OVER,
    PKT_REC_MISS
} PacketType;

typedef struct {
    int st_rec;
    int end_rec;
} RecRange;

typedef struct {
    PacketType type;
    int numSegments; // N >= 1
    RecRange segments[MAX_SEGMENTS];
    char payload[MAX_RECORDS_PER_PKT * MAX_RECORD_SIZE];
} DATA_PKT;

typedef struct {
    PacketType type;
    int M_st;
    int M_fin;
} IS_BLAST_OVER;

typedef struct {
    PacketType type;
    int numRanges;
    RecRange missing[MAX_MISS_RANGES];
} REC_MISS;

// Selection of header size is essential, and needs to be the same between both users.
int main(int argc, char** argv) {
    UserConfig conf = {UNDEFINED, 0}; // The configuration we'll use for the program.
    
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
        printf("  -> Blast Size (M): %d records\n", header.blastSize);

        FILE_HDR_ACK ack; // Send ACK to sender (1 is all good!)
        ack.status = 1; 

        printf("Sending FILE_HDR_ACK back to sender...\n");
        sendto(sockfd, &ack, sizeof(ack), 0, (const struct sockaddr *)&sender_addr, len);

        printf("Phase 1 connection established.\n");
        
        // Receiver File I/O & Blast Buffer setup (for Phase 2)
        char outFilename[300];
        snprintf(outFilename, sizeof(outFilename), "recv_%s", header.filename); // Prefix to prevent overwriting local test file
        FILE *outFile = fopen(outFilename, "wb");
        if (!outFile) {
            perror("Failed to create output file");
            close(sockfd);
            exit(EXIT_FAILURE);
        }

        int totalRecords = (header.fileSize + header.blockSize - 1) / header.blockSize;
        char *blastBuf = malloc(header.blastSize * header.blockSize);
        bool *received = calloc(header.blastSize, sizeof(bool));
        if (!blastBuf || !received) {
            perror("Receiver buffer allocation failed");
            fclose(outFile);
            close(sockfd);
            exit(EXIT_FAILURE);
        }
        printf("Phase 2 Receiver ready: expecting %d total records into '%s'.\n", totalRecords, outFilename);
        // Receive DATA_PKT loop, IS_BLAST_OVER, REC_MISS, and flush blastBuf to disk

        free(blastBuf);
        free(received);
        fclose(outFile);

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

        const char *filename = "test_file.txt"; // Open file & calculate size
        FILE *inFile = fopen(filename, "rb");
        if (!inFile) {
            perror("Failed to open input file (create test_file.txt first)");
            close(sockfd);
            exit(EXIT_FAILURE);
        }
        fseek(inFile, 0, SEEK_END);
        long actualFileSize = ftell(inFile);
        rewind(inFile);

        FILE_HDR header;
        memset(&header, 0, sizeof(header));
        header.fileSize = actualFileSize; // (Original test was 1 MB)
        header.blockSize = conf.headerSize; // 256
        header.blastSize = DEFAULT_BLAST_SIZE; // M records per blast
        strcpy(header.filename, "test_file.txt");

        printf("BLAST-ing FILE_HDR to receiver...\n"); // Send file to reciever
        sendto(sockfd, &header, sizeof(header), 0, (const struct sockaddr *)&receiver_addr, sizeof(receiver_addr));

        FILE_HDR_ACK ack; // Wait for ack from reciever (FILE_HDR_ACK)
        socklen_t len = sizeof(receiver_addr);
        printf("Waiting for FILE_HDR_ACK...\n");

        recvfrom(sockfd, &ack, sizeof(ack), 0, (struct sockaddr *)&receiver_addr, &len); // Block & Wait (Need to add timeout later***)

        if (ack.status == 1) {
            printf("FILE_HDR_ACK received! Phase 1 connection established.\n");

            // Sender Blast Buffer & Blast Loop (for Phase 2)
            int totalRecords = (header.fileSize + header.blockSize - 1) / header.blockSize;
            char *blastBuf = malloc(header.blastSize * header.blockSize);
            if (!blastBuf) {
                perror("Sender buffer allocation failed");
                fclose(inFile);
                close(sockfd);
                exit(EXIT_FAILURE);
            }

            int M_st = 1;
            while (M_st <= totalRecords) {
                int M_fin = M_st + header.blastSize - 1;
                if (M_fin > totalRecords) {
                    M_fin = totalRecords;
                }
                int recordsInBlast = M_fin - M_st + 1;

                // Read up to M records from disk to blast buffer
                memset(blastBuf, 0, header.blastSize * header.blockSize);
                fread(blastBuf, 1, recordsInBlast * header.blockSize, inFile);

                printf("Blasting records %d to %d...\n", M_st, M_fin);

                // Pack up to MAX_RECORDS_PER_PKT (16) records per DATA_PKT and send
                int currRec = M_st; 
                while (currRec <= M_fin) {
                    int endRec = currRec + MAX_RECORDS_PER_PKT - 1;
                    if (endRec > M_fin) {
                        endRec = M_fin;
                    }
                    int count = endRec - currRec + 1;

                    DATA_PKT pkt;
                    memset(&pkt, 0, sizeof(pkt));
                    pkt.type = PKT_DATA;
                    pkt.numSegments = 1;
                    pkt.segments[0].st_rec = currRec;
                    pkt.segments[0].end_rec = endRec;

                    int bufOffset = (currRec - M_st) * header.blockSize;
                    memcpy(pkt.payload, blastBuf + bufOffset, count * header.blockSize);

                    sendto(sockfd, &pkt, sizeof(pkt), 0, (const struct sockaddr *)&receiver_addr, sizeof(receiver_addr));
                    currRec = endRec + 1;
                }

                // Send IS_BLAST_OVER, wait for REC_MISS with timeout, and retransmit missing records
                M_st = M_fin + 1;
            }

            printf("Phase 2 Sender finished blasting %d total records.\n", totalRecords);
            free(blastBuf);
        }

        fclose(inFile); 
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
