#ifndef MYTYPES_H
#define MYTYPES_H

#include "headers.h"

using namespace std;

struct st_dongle
{
	int hci_num;
	int thread_num;
};

typedef st_dongle* st_dongle_ptr;
typedef queue <st_dongle> dongleq_t;

struct st_file
{
	char f_address[100];
	char f_name[100];
};

typedef map <int, st_file*> filemap_t;
typedef map <int, st_file*>::iterator filemapitr_t;

struct st_cmd
{
	char dev_mac[18];
	int file_id;
};
typedef st_cmd* st_cmd_ptr;
typedef queue <st_cmd> cmdq_t;

struct st_res
{
	char dev_mac[18];
	int file_id;
	int hci_num;
	int thread_num;
	int send_res_stat;
	time_t send_time;
};
typedef queue <st_res> resq_t;

#endif //MYTYPES_H
