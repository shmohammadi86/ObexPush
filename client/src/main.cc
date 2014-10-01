#include <headers.h>
#include <client.h>
	
int main(int argc, char **argv) {

	char ip[16]; int port = -1;  
	switch(argc){
		case(1):
			port = 2000;
			strcpy(ip, "127.0.0.1");
			break;
		case(2):
			port = 2000;
			strcpy(ip, argv[1]);
			break;
		case(3):
			port = atoi(argv[2]);
			strcpy(ip, argv[1]);
			break;			
		default:
			printf("Invalide arguments. Syntax: %s [server address] [port number]\n", argv[0]); 
	}

	Client *me = new Client(ip, port);
	me->Run();
	
	delete me;
	return 0;	
}
