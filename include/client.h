#ifndef CLIENT_H_
#define CLIENT_H_
#include <headers.h>
#include <mytypes.h>
#include <obex_main.h>
#include <obex_macros.h>

enum CLIENT_STAT
{
	NONE,
	STOPPED,
	STARTED,
	INITIATED
};


enum USER_STAT
{
	USR_INIT,
	USR_FREE,
	USR_BUSY
};

struct Network {
	int client_fd;
	FILE *client_in;
	FILE *client_out;	
};

class Client
{
public:
	Client(const char *server, int port);
	int Run();
	virtual ~Client();


private:
	static void *server_interface(void *);
	static void *macq_handler(void *args);
	static void *nameq_handler(void *args);
	static void *chanq_handler(void *args);
	static void *ressq_handler(void *args);
	static void *scan(void *args);
	static int getDongleAndCMD(st_dongle* dongle, st_cmd* cmd);
	static int senderMainLoop();
	static int onesend(void* ptr);
	int sock_fd;
};

struct user_t
{
	char name[256];
	int channel;
	char type[256];
	USER_STAT status;
	int ttl;
};

#endif /*CLIENT_H_*/
