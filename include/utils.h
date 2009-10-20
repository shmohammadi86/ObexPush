#ifndef UTILS_H_
#define UTILS_H_

#include <headers.h>

#define SDP_CONNECTION_ERROR -110
#define SDP_SEARCH_ERROR -120
#define SDP_NO_CHANNEL -130


map<int, HCI *> EnumerateHCI(const char*dev_name, FILE *sinterface_outfile);
char *get_minor_device_name(int major, int minor);
int findOPUSH(const HCI *src_hci, const char* mac, FILE *log);

#endif
