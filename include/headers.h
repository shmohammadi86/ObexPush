#ifndef HEADERS_H_
#define HEADERS_H_

// General purpose ...
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <ctype.h>
#include <sys/time.h>
#include <semaphore.h>
#include <sys/time.h>
#include <sys/select.h>
#include <termios.h>
#include <stdarg.h>

// Network ...
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <ifaddrs.h>

// Bluetooth ...
#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <bluetooth/sdp.h>
#include <bluetooth/sdp_lib.h>
#include <openobex/obex.h>
#include <bluetooth/l2cap.h>

// STL based ...
#include <vector>
#include <queue>
#include <string>
#include <map>

using namespace std;

int log(FILE *fd, int level, char* fmt, ...);

struct HCI
{
	int id;
	bdaddr_t bdaddr;
	char mac[20];
};

#endif /*HEADERS_H_*/
