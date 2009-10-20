#ifndef SERVER_H_
#define SERVER_H_

#include "headers.h"
#define MAX_CLIENTS 256
#define USR_BUSY 0
#define USR_FREE 1

struct Command {
	int type;
	char str[1024];
};

struct Client_struct {
	int client_id;
	int client_fd;
	FILE *client_in;
	FILE *client_out;
	FILE* recv_log;
	FILE* send_log;
	pthread_mutex_t recv_log_mutex;
	pthread_mutex_t send_log_mutex;
	queue <Command*> cmd_q;
	pthread_t client_handler;
};
	
struct Event {
	char buffer[1024];
	int client_id;
};

class Server
{
public:
	Server(int port);
	int Run();
	virtual ~Server();
	
private:
	static void *listener(void *);
	static int updateMaps(int client_id);
	static void *scanner_handler(void *args);
	static void *new_client(void *);
	static void *console(void *);
//	static void *parse(void *args);
	static int parse(Event *event);
	FILE *client_in, *client_out;
	int server_socket;
	int port;	
};

#endif /*SERVER_H_*/
