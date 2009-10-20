#ifndef HEADERS_H_
#define HEADERS_H_

#define STARTED 1
#define TERMINATED 0

// General purpose ...
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/time.h>
#include <fcntl.h>
#include <semaphore.h>
#include <mysql.h>
#include <signal.h>

// Network ...
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <ifaddrs.h>

// STL based ...
#include <string>
#include <vector>
#include <map>
#include <queue>
using namespace std;

#include "Server.h"

#endif /*HEADERS_H_*/
