#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <stdbool.h>



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


// Selection of header size is essential, and needs to be the same between both users.
int main(int argc, char** argv) {
	UserConfig conf = {0}; // The configuration we'll use for the program.
	
	printf("arg1: %d\n", argc);
	setupApp(argv,argc , &conf);
	

	if(conf.userType == RECIEVER)
		printf("User Type: RECIEVER\n");
	else
		printf("User Type: SENDER\n");

	printf("Header Size: %d\n", conf.headerSize);
	printf("Setup Complete - BLAST Protocol initializing...\n\n");
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
