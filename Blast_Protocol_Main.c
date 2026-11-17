#include <stdio.h>
#include <stdlib.h>

typedef enum {
	RECIEVER,
	SENDER
} ProtocolEnd;

typedef struct {
	ProtocolEnd userType;
	int headerSize;
} UserConfig;


void setupApp(int argc, char** argv, UserConfig* c);

// Selection of header size is essential, and needs to be the same between both users.
int main(int argc, char** argv) {
	UserConfig conf; // The configuration we'll use for the program.
	
	setupApp(argc, argv, &conf);
	

	if(conf.userType == RECIEVER)
		printf("User Type: RECIEVER\n");
	else
		printf("User Type: SENDER\n");

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