#include "headers.h"
	
int main(int argc, char **argv) {

	int port = -1;
	switch(argc){
		case(1):
			port = 2000;
			break;		
		case(2):
			port = atoi(argv[1]);
			break;			
		default:
			printf("Invalide arguments. Syntax: %s [port number]\n", argv[0]); 
	}

	Server *me = new Server(port);	
	me->Run();
	
	delete me;
	return 0;	
}

