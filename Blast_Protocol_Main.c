#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
	RECIEVER,
	SENDER
} ProtocolEnd;

typedef struct {
	ProtocolEnd userType;
	int headerSize;
} UserConfig;

typedef void (*CommandFunc) (const char *arg);

typedef struct {
	const char *name;
	CommandFunc func;
} CommandEntry;




void set_utype (UserConfig* c, ProtocolEnd utype){
	c->userType = utype;
}

void set_hsize (UserConfig* c, int hsize) {
	c->headerSize = hsize;
}

static const CommandEntry commands[] ={
	{"-t", set_utype},
	{"-type", set_utype},
	{"-s", set_hsize},
	{"-size", set_hsize}
}


void setupApp(char** argv, UserConfig* c);


// Selection of header size is essential, and needs to be the same between both users.
int main(int argc, char** argv) {
	UserConfig conf; // The configuration we'll use for the program.
	
	setupApp(argv, &conf);
	

	if(conf.userType == RECIEVER)
		printf("User Type: RECIEVER\n");
	else
		printf("User Type: SENDER\n");

	printf("Header Size: %d\n", conf.headerSize);
	printf("Setup Complete - BLAST Protocol initializing...\n\n");
	return 0;
}

void setupApp(char** argv, UserConfig* c) {
	if( sizeof(argv)/sizeof(argv[0]) <= 1) {
		c->userType = RECIEVER;
		c->headerSize = 256; // Default header size.
	} else {
		for(int i = 1; i< ( sizeof(argv) / sizeof(argv[0]) ); ++i) {
				
		} 
	}
}
