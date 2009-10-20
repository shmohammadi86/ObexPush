#ifndef UTILS_H_
#define UTILS_H_

#include <headers.h>

vector<HCI *> EnumerateHCI (const char *);
char *get_minor_device_name(int major, int minor);

#endif
