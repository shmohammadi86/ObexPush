#ifndef OBEX_SOCKET_H_INCLUDED
#define OBEX_SOCKET_H_INCLUDED
#include <headers.h>
#include <obex_macros.h>
#include <obex_main.h>

struct cobex_context {
	bdaddr_t btaddr;
	char *addr;
	int devid, chan;
	const char *portname;
	int timeout;
	int rfd, wfd;
	char inputbuf[4096];
	struct termios oldtio, newtio;
};

struct cobex_context *cobex_open(int devid, const char * port, int timeout);
struct cobex_context *cobex_setsocket(const int fd);
int cobex_getsocket(struct cobex_context *gt);
void cobex_close(struct cobex_context *gt);
int cobex_listen(obex_t * handle, void *userdata);
int cobex_connect(obex_t * handle, void *userdata);
int cobex_disconnect(obex_t * handle, void *userdata);
int cobex_write(obex_t * handle, void *userdata, uint8_t * buffer, int length);
int cobex_handle_input(obex_t * handle, void *userdata, int timeout);

#endif				/* OBEX_SOCKET_H_INCLUDED */
