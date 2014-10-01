#ifndef BT_SDP_H
#define BT_SDP_H

#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <bluetooth/sdp.h>
#include <bluetooth/sdp_lib.h>

#include <netinet/in.h>

int find_opush_channel(int dev_id, char* bdadd_str);

#endif //BT_SDP_H
