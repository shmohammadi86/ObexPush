#include <client.h>
#include <utils.h>

#define SDP_NO_CHANNEL -10
#define SDP_CONNECTION_ERROR -20
#define SDP_SEARCH_ERROR -30

#define LOG
#define DEBUG

int findOPUSH(int hci_no, const char* mac);

uuid_t svc_uuid;
uint16_t class16 = 0x1105;
uint32_t range = 0x0000ffff;

map <int, int> HCI2Sender;

int Check_names = 0;

Network *conn = new Network;
int done, stop;

queue <string> mac_queue;
pthread_mutex_t mac_queue_lock;
sem_t macq_sem;

queue <string> name_queue; 
pthread_mutex_t name_queue_lock; 
sem_t nameq_sem;


queue <string> chan_queue;
pthread_mutex_t chan_queue_lock;
sem_t chanq_sem;


map <string, user_t> MacDB;
int scanner_hci_no;

//pthread_mutex_t *SDP_lock; 

//////////sender////////////
filemap_t files_m;

dongleq_t dongles_q;
cmdq_t cmds_q;
resq_t ress_q;

pthread_mutex_t donglesq_mutex;
pthread_mutex_t ressq_mutex;
pthread_mutex_t cmdsq_mutex;

pthread_mutex_t client_out_mutex;

sem_t donglesq_sem;
sem_t ressq_sem;
sem_t cmdsq_sem;
//////////eo sender/////////

int releaseTTL;
int releaseCounter;
int scan_wait;
int obex_timeout;

pthread_mutex_t macmap_mutex;


FILE* obex_outfile;
FILE* SDP_outfile;
FILE* qhandler_outfile;
FILE* scanner_outfile;
FILE* sinterface_outfile;
pthread_mutex_t obex_out_mutex;
pthread_mutex_t sdp_out_mutex;
pthread_mutex_t qh_out_mutex;
pthread_mutex_t scn_out_mutex;
pthread_mutex_t sif_out_mutex;

map<FILE*, pthread_mutex_t*> file2mutex;


pthread_t interface_thread, macq_thread, nameq_thread, ressq_thread, scanner_thread, sending_thread, chanq_thread;


int log(FILE *fd, int level, char* fmt, ...)
{
	pthread_mutex_t* fd_mutex = file2mutex[fd];

	va_list ap;
	va_start(ap, fmt); 
	
	time_t ct = time(NULL);
	char time_str[256];
	sprintf(time_str, "%s", ctime(&ct));
	time_str[strlen(time_str)-1]='\0';
	pthread_mutex_lock(fd_mutex);
	fprintf(fd, "%s: ", time_str);
	for(register int i=0; i < level; i++) 
		fprintf(fd, "\t");
	int retval = vfprintf(fd, fmt, ap);
	fflush(fd);
	pthread_mutex_unlock(fd_mutex);

	va_end(ap);
	return retval;
}

int reset() {
	map <string, user_t>::iterator it;
	
	for(it = MacDB.begin(); it != MacDB.end(); it++) {
		it->second.status = USR_FREE;
		it->second.ttl = 0;
	}		
	
	return 0;
}

Client::Client(const char *server, int port)
{
	printf("+ Starting up client ...\n\t- Resolving Server name ... ");fflush(stdout);
	sdp_uuid16_create (&svc_uuid, class16);

#ifdef LOG
	#ifdef DEBUG
		obex_outfile = SDP_outfile=qhandler_outfile=scanner_outfile=sinterface_outfile = stdout;
		//fopen("/dev/null", "w+");
	#else
		char cur_time_str[256];
		time_t cur_time = time(NULL);
		sprintf(cur_time_str, "%s", ctime(&cur_time));
		cur_time_str[strlen(cur_time_str)-1]='\0';
		char outname[256], cmd[256];
		sprintf(cmd, "mkdir \"log/%s\"", cur_time_str);
		int sys_res;
		sys_res = system(cmd);
		sprintf(outname, "log/%s/obex.log", cur_time_str);
		obex_outfile = fopen(outname, "w+"); //stdout;
		//obex_outfile = stdout;
		sprintf(outname, "log/%s/sdp.log", cur_time_str);
		SDP_outfile = stdout;//fopen(outname, "w+");
		//SDP_outfile = stdout;
		sprintf(outname, "log/%s/qhandler.log", cur_time_str);
		qhandler_outfile = fopen(outname, "w+");
		sprintf(outname, "log/%s/scanner.log", cur_time_str);
		scanner_outfile = fopen(outname, "w+");
		sprintf(outname, "log/%s/server_interface.log", cur_time_str);
		sinterface_outfile = fopen(outname, "w+");
	#endif
#else
	obex_outfile = fopen("/dev/null", "w+");
	SDP_outfile = stdout;//fopen("/dev/null", "w+");
	qhandler_outfile = fopen("/dev/null", "w+");
	scanner_outfile = fopen("/dev/null", "w+");
	sinterface_outfile = fopen("/dev/null", "w+");
#endif


	pthread_mutex_init (&obex_out_mutex, NULL);
	pthread_mutex_init (&sdp_out_mutex, NULL);
	pthread_mutex_init (&qh_out_mutex, NULL);
	pthread_mutex_init (&scn_out_mutex, NULL);
	pthread_mutex_init (&sif_out_mutex, NULL);
	file2mutex[obex_outfile] = &obex_out_mutex;
	file2mutex[SDP_outfile] = &sdp_out_mutex;
	file2mutex[qhandler_outfile] = &qh_out_mutex;
	file2mutex[scanner_outfile] = &scn_out_mutex;
	file2mutex[sinterface_outfile] = &sif_out_mutex;


	pthread_mutex_init (&name_queue_lock, NULL); pthread_mutex_init (&mac_queue_lock, NULL);
	//sem_init(&queue_sem, 0, 0);
	
	sem_init(&macq_sem, 0, 0);
	sem_init(&nameq_sem, 0, 0);
	pthread_mutex_init(&macmap_mutex, NULL);

	pthread_mutex_init(&client_out_mutex, NULL);

	//////////sender///////////
	pthread_mutex_init(&donglesq_mutex, NULL);
	pthread_mutex_init(&ressq_mutex, NULL);
	pthread_mutex_init(&cmdsq_mutex, NULL);

	pthread_mutex_init(&chan_queue_lock, NULL);
	
	sem_init(&donglesq_sem, 0, 0);
	sem_init(&ressq_sem, 0, 0);
	sem_init(&chanq_sem, 0, 0);

	sem_init(&cmdsq_sem, 0, 0);

	////////eo sender///////////
	
	struct hostent *serv_hostent = gethostbyname(server);
	if (serv_hostent == NULL) {
		fprintf(stderr, "Server hostname cannot be resolved");fflush(stderr);
		exit(-1);
	}
	printf("Done.\n");fflush(stdout);

	printf("\t- Opening Socket ... ");fflush(stdout);
	sock_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (sock_fd < 0) {
		fprintf(stderr, "Could not create the socket");fflush(stderr);
		exit(-1);
	}	
	printf("Done.\n");fflush(stdout);

	struct sockaddr_in serv_addr;
	serv_addr.sin_family = AF_INET;
	memcpy(&serv_addr.sin_addr.s_addr, serv_hostent->h_addr, serv_hostent->h_length);
	serv_addr.sin_port = htons(port);
	printf("\t- Connecting to %s on port %d ...", server, port); fflush(stdout);
	while(connect(sock_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
		sleep(1);

	//if(sock_fd) .... 
	conn->client_fd = sock_fd;	
	conn->client_in = fdopen(sock_fd, "r");
	conn->client_out = fdopen(sock_fd, "w");	

	printf("Connected.\n");fflush(stdout);
}


int Client::Run() {		
	printf("\t+ Starting server interface thread ... \n"); fflush(stdout); done = 0;
	pthread_create(&interface_thread, NULL, &server_interface, this);
	pthread_create(&macq_thread, NULL, &macq_handler, this);
	pthread_create(&nameq_thread, NULL, &nameq_handler, this);
	pthread_create(&ressq_thread, NULL, &ressq_handler, this);
	pthread_create(&chanq_thread, NULL, &chanq_handler, this);
	pthread_join(chanq_thread, NULL);
	pthread_join(interface_thread, NULL);
	pthread_join(macq_thread, NULL);
	pthread_join(nameq_thread, NULL);
	pthread_join(ressq_thread, NULL);
	
	return 0;	
}

int addCmd2Q(int file_id, char* send_mac)
{
	log(sinterface_outfile, 1, "pushing send command to queue: send %s %d... \n", send_mac, file_id);

	st_cmd cmd;
	cmd.file_id = file_id;
	strcpy(cmd.dev_mac, send_mac);

	pthread_mutex_lock(&cmdsq_mutex);
	cmds_q.push(cmd);
	sem_post(&cmdsq_sem);
	pthread_mutex_unlock(&cmdsq_mutex);

	return 0;
}

int initHCI(const char* dev_name, int thread_no)
{

	vector<HCI *> HCI_List = EnumerateHCI(dev_name);
	
	if(!HCI_List.size()) { 
		fprintf(stderr, "\t\t- No Bluetooth dongle is attached.\n"); fflush(stderr);
		return -1;
	}

	scanner_hci_no = HCI_List[0]->id;
	st_dongle dongle;
	sem_init(&donglesq_sem, 0, 0);
	while(!dongles_q.empty())
		dongles_q.pop();

	for(register int i = 0; i < thread_no; i++) {
		for(register int j = 1; j < (int)HCI_List.size(); j++) {
			HCI2Sender[HCI_List[j]->id] = j-1;
			dongle.thread_num = i;
			dongle.hci_num = HCI_List[j]->id;
			dongles_q.push(dongle);
			sem_post(&donglesq_sem);
		}
	}	
	
	
	return HCI_List.size();
}

int Init()
{
	char buffer[1024];
	char devname[256];
	st_file* file;
	char temp_add[256];
	int file_id;

	releaseTTL = 5;
	int threadnum = 3;

	while(true)
	{
		if(fgets(buffer, 1024, conn->client_in) == NULL) {
			log(sinterface_outfile, 2, "Server connection lost.\n");
			break;
		}
		
		if(!strncmp(buffer, "file", 4))
		{
			file = new st_file;
			sscanf(buffer+5, "%d\t%[^\t]\t%[^\n]\n", &file_id, temp_add, file->f_name);
			sprintf(file->f_address, "../uploaded/%s", temp_add);
			files_m[file_id] = file;
			log(sinterface_outfile, 3, "New file configuration: %d \"%s\" \"%s\"\n", file_id, files_m[file_id]->f_address, files_m[file_id]->f_name);
			continue;
		}
		else if(!strncmp(buffer, "thread#", 7))
		{
			sscanf(buffer+8, "%d\n", &threadnum);
			log(sinterface_outfile, 3, "Total number of threads per HCI = %d\n", threadnum);
			continue;
		}
		else if(!strncmp(buffer, "devname", 7))
		{
			int client_id;
			sscanf(buffer+8, "%[^\t]\t%d\n", temp_add, &client_id);
			sprintf(devname, "%s_%d", temp_add, client_id);
			log(sinterface_outfile, 3, "Device name = %s\n", devname);
			continue;
		}


		else if(!strncmp(buffer, "ObexTimeout", 11))
		{
			sscanf(buffer+12, "%d\n", &obex_timeout);
			log(sinterface_outfile, 3, "Obex Timeout = %d\n", obex_timeout);
			continue;
		}
		else if(!strncmp(buffer, "TTL", 3))
		{
			sscanf(buffer+4, "%d\n", &releaseTTL);
			log(sinterface_outfile, 3, "User TTL = %d\n", releaseTTL);
			continue;
		}
		else if(!strncmp(buffer, "ScanDelay", 9))
		{
			sscanf(buffer+10, "%d\n", &scan_wait);
			log(sinterface_outfile, 3, "Scan Delay = %d\n", scan_wait);
			continue;
		}
		else if(!strncmp(buffer, "time", 4))
		{
			struct timeval now;
			sscanf(buffer+5, "%ld\t%ld\n", &now.tv_sec, &now.tv_usec);
			settimeofday(&now, NULL);
			/*(tm* cur_time = localtime(&ct);
			char cmd[256];
			sprintf(cmd, "sudo date -s \"%d-%d-%d %d:%d:%d\"", 
					cur_time->tm_year+1900, cur_time->tm_mon+1, cur_time->tm_mday,
					cur_time->tm_hour, cur_time->tm_min, cur_time->tm_sec);
			int sys_res;
			sys_res = system(cmd);*/
		}

		else if(!strncmp(buffer, "end", 3))
		{
			log(sinterface_outfile, 3, "End of options.\n");
			break;
		}

	}
	
	int dongleNo = initHCI(devname, threadnum);
	return dongleNo;
}

void *Client::server_interface(void *args) {
	Client *me = (Client *)args; 
	char buffer[1024];
	char send_mac[20]; int file_id = -1;

	CLIENT_STAT status = NONE;

	while (!done) {
			if(fgets(buffer, 1024, conn->client_in) == NULL) {
				fprintf(stderr, "\t\t- Server connection lost.\n");fflush(stderr);
				break;
			}

			log(sinterface_outfile, 1, "RAW message: %s", buffer);
			
			
			if(!strncmp(buffer, "quit", 4)) {
				log(sinterface_outfile, 1, "got <quit> message\n");
				done = 1;	
				sem_post(&macq_sem);
				sem_post(&nameq_sem);
				sem_post(&ressq_sem);
				if(status == STARTED)
				{
					stop = 1;
				}
				continue;
			}
			else if(!strncmp(buffer, "get", 3)) { //if anything else than options should have a get command, add additinal parsers inside this if ...
				log(sinterface_outfile, 1, "got <get options> message\n");
				if(status == STARTED) {
					log(sinterface_outfile, 2, "Stopping client ... \n");fflush(stdout);
					status = STOPPED;
					stop = 1;
					sleep(1);
					reset();
				}
				log(sinterface_outfile, 2, "Getting options ... \n");
				int dongleNo = Init(); //Takes arguments from server, and initalize HCI(s)
				if(0 < dongleNo) {
					log(sinterface_outfile, 3, "Total number of bluetooth dongles = %d\n", dongleNo);
					status = INITIATED;
				}
				else {
					
					status = NONE;				
				}
				continue;
			}
			else if(!strncmp(buffer, "send", 4)) {
				log(sinterface_outfile, 1, "got <send> message\n");
				if(status == STARTED) {
					sscanf(buffer, "send %s\t%d", send_mac, &file_id);
					//fprintf(stderr, "\t\t Enqueue SEND command <%s, %d>\n", send_mac, file_id);
					addCmd2Q(file_id, send_mac);
				}
				else
				{
					log(sinterface_outfile, 2, "Client is not started. \n");
				}
				continue;
			}
			else if(!strncmp(buffer, "start", 5)) {
				log(sinterface_outfile, 1, "got <start> message\n");
				if(status == INITIATED || status == STOPPED) {
					log(sinterface_outfile, 2, "Starting client ... \n");
					status = STARTED;
					stop = 0;
					pthread_create(&scanner_thread, NULL, &scan, me);
					pthread_create(&sending_thread, NULL, (void*(*)(void*)) &senderMainLoop, NULL);
				}
				else if (status == NONE)
				{
					log(sinterface_outfile, 2, "Client is not initialized. \n");
				}
				else
				{
					log(sinterface_outfile, 2, "Client is already started. \n");
				}
				continue;
			}
			else if(!strncmp(buffer, "stop", 4)) {
				log(sinterface_outfile, 1, "got <stop> message\n");
				if(status == STARTED) {
					log(sinterface_outfile, 2, "Stopping client ... \n");
					status = STOPPED;
					stop = 1;
					sleep(1);
					reset();
				}
				else {
					log(sinterface_outfile, 2, "Client is already stopped. \n");
				}
				continue;
			}
			else if(!strncmp(buffer, "release", 7))
			{
				log(sinterface_outfile, 1, "got <release> message\n");
				if(status == STARTED) {			
					releaseCounter--;
					char mac[256];
					sscanf(buffer, "release %s",  mac);
					log(sinterface_outfile, 4, "Releasing %s, total number of busy users = %d\n", mac, releaseCounter);
					pthread_mutex_lock(&macmap_mutex);
					MacDB[mac].status = USR_FREE;
					MacDB[mac].ttl = 0;
					pthread_mutex_unlock(&macmap_mutex);
				}
				else
				{
					printf("\t\t* Client is not started.\n");fflush(stdout);
				}
				continue;
			}
			else {
				log(sinterface_outfile, 1, "got UNKNOWN message\n");
				fprintf(stderr, "\t\t- Unknown command: %s\n", buffer);fflush(stderr);
			}					
	}	
	
	printf("Done.\n");fflush(stdout);
	pthread_exit(NULL);	
}

void *Client::macq_handler(void *args) {
	char mac[256];
	char type[256];
	
	while(!done)
	{
 		while (sem_wait(&macq_sem) != 0);
 		
		if(mac_queue.size())
		{
			pthread_mutex_lock(&mac_queue_lock);
			strcpy(mac, mac_queue.front().c_str());
			mac_queue.pop();	
			pthread_mutex_unlock(&mac_queue_lock);			

			pthread_mutex_lock(&macmap_mutex);
			strcpy(type, MacDB[mac].type);
			pthread_mutex_unlock(&macmap_mutex);

			log(qhandler_outfile, 2, "Sending <mac> %s (%s), total number of busy devices %d\n", mac, type, releaseCounter);
			
			pthread_mutex_lock(&client_out_mutex);
			releaseCounter++;
			fprintf(conn->client_out, "mac %s\t%s\n", mac, type);
			fflush(conn->client_out);
			pthread_mutex_unlock(&client_out_mutex);

		}
	}

//	printf("\t- Terminating mac queue handler ...\n");fflush(stdout);
	log(qhandler_outfile, 1, "Terminating mac queue handler ...\n");
	pthread_exit(NULL);	
}

void *Client::nameq_handler(void *args) {
	char name[256];
	char mac[256];

	while(!done)
	{
 		while (sem_wait(&nameq_sem) != 0);
 		
		if(name_queue.size()) {
			//if(!done){
			pthread_mutex_lock(&name_queue_lock);
			strcpy(mac, name_queue.front().c_str());
			name_queue.pop();	
			pthread_mutex_unlock(&name_queue_lock);			

			pthread_mutex_lock(&macmap_mutex);
			strncmp(name, MacDB[mac].name, 256);
			pthread_mutex_unlock(&macmap_mutex);

			
			log(qhandler_outfile, 2, "Sending <Name> message:\t%s\tname: %s\n", mac, name);
			pthread_mutex_lock(&client_out_mutex);
			fprintf(conn->client_out, "name %s\t%s\n",  mac, name);
			fflush(conn->client_out);
			pthread_mutex_unlock(&client_out_mutex);
		}
	}

//	printf("- Terminating name queue handler ...\n");fflush(stdout);
	log(qhandler_outfile, 1, "Terminating name queue handler ...\n");
	pthread_exit(NULL);	
}



void *Client::chanq_handler(void *args) {
	char mac[256];
	int channel;
	
	while(!done)
	{
 		while (sem_wait(&chanq_sem) != 0);
 		
		if(chan_queue.size()) {
			pthread_mutex_lock(&chan_queue_lock);
			strcpy(mac, chan_queue.front().c_str());
			chan_queue.pop();	
			pthread_mutex_unlock(&chan_queue_lock);			

			pthread_mutex_lock(&macmap_mutex);
			channel = MacDB[mac].channel;
			pthread_mutex_unlock(&macmap_mutex);

			
			log(qhandler_outfile, 2, "Sending <Channel> message: channel %s\t%d\n", mac, channel);
			pthread_mutex_lock(&client_out_mutex);
			fprintf(conn->client_out, "channel %s\t%d\n",  mac, channel);
			fflush(conn->client_out);
			pthread_mutex_unlock(&client_out_mutex);
		}
	}

//	printf("- Terminating name queue handler ...\n");fflush(stdout);
	log(qhandler_outfile, 1, "Terminating channel queue handler ...\n");
	pthread_exit(NULL);	
}

void *Client::ressq_handler(void *args) {
	st_res cur_res;
	
	while(!done)
	{
 		while (sem_wait(&ressq_sem) != 0);
 		
		if(ress_q.size())
		{
		
			pthread_mutex_lock(&ressq_mutex);
			cur_res = ress_q.front();
			ress_q.pop();
			pthread_mutex_unlock(&ressq_mutex);
			
			log(qhandler_outfile, 2, "Sending <Result> message:\t%s\t%d\t%d\n", cur_res.dev_mac, cur_res.file_id, cur_res.send_res_stat);
			
			pthread_mutex_lock(&client_out_mutex);
			fprintf(conn->client_out, "res %s\t%d\t%d\t%d\t%ld\n",  
					cur_res.dev_mac, cur_res.file_id, cur_res.hci_num, cur_res.send_res_stat, cur_res.send_time);
			fflush(conn->client_out);
			pthread_mutex_unlock(&client_out_mutex);
		}
	}

//	printf("- Terminating res queue handler ...\n");fflush(stdout);
	log(qhandler_outfile, 1, "Terminating res queue handler ...\n");
	pthread_exit(NULL);	
}


void *Client::scan(void *args) {
	int ctl;
	
	if ((ctl = socket(AF_BLUETOOTH, SOCK_RAW, BTPROTO_HCI)) < 0) {
		perror("Can't open HCI socket.");
		exit(1);
	}

	int hci_sock = hci_open_dev( scanner_hci_no );
	if (hci_sock < 0) {
		fprintf(stderr, "\t\t\t\t *Error opening bluetooth socket for HCI%d.\n", scanner_hci_no);fflush(stderr);
		log(scanner_outfile, 1, "Error opening bluetooth socket for HCI%d.\n", scanner_hci_no);
		exit(-1);
	}

	inquiry_info *inq_list;
	printf("\t\t\t- Allocating memory ...");fflush(stdout);
	inq_list = new inquiry_info[128];
	if(inq_list == NULL) {
		fprintf(stderr, "\t\t\t\tOops! Error allocating memory.\n");fflush(stderr);
		log(scanner_outfile, 1, "Oops! Error allocating memory.\n");
		exit(-1);
	}
	else {
		printf("done.");fflush(stdout);
	}


	char mac_addr[20], name[256];	
	while(!stop)
	{
		printf("\n\t\t\t- Querying bluetooth devices ..."); fflush(stdout);
		int num_rsp = hci_inquiry(scanner_hci_no, 8, 128, NULL, &inq_list, IREQ_CACHE_FLUSH);
		if( num_rsp < 0 )
		{
			fprintf(stderr, "Error inquiring devices.\n");fflush(stderr);
			log(scanner_outfile, 2, "Error inquiring devices.\n");
		}

		
		printf("\n\t\t\t\t- Found %d devices ...", num_rsp); fflush(stdout);
		for (register int i = 0; i < num_rsp; i++)
		{
			ba2str(&(inq_list+i)->bdaddr, mac_addr);

			
			log(scanner_outfile, 3, "Device MAC =  %s\n", mac_addr);

			pthread_mutex_lock(&macmap_mutex);
			map <string, user_t>::iterator itr = MacDB.find(mac_addr);
			map <string, user_t>::iterator end = MacDB.end();
			pthread_mutex_unlock(&macmap_mutex);
			
			
			if( itr == end )
			{
				user_t user;
				strncpy(user.type, get_minor_device_name(inq_list[i].dev_class[1] & 0x1f, inq_list[i].dev_class[0] >> 2), 256);
				log(scanner_outfile, 4, "%s is a %s\n", mac_addr, user.type);
				if((inq_list[i].dev_class[1] & 0x1f) == 1) { // Type filtering ....
					int minor = inq_list[i].dev_class[0] >> 2;
					if(minor == 1 || minor == 2 || minor == 3) {
						//printf("\t\t\t\t\t- Avoid transmitting to computer device\n"); fflush(stdout);
						continue;
					}
				}
				user.channel = -1;
				strcpy(user.name, "unknown");
				user.status = USR_BUSY;
	
				//printf("\t\t\t\t\t- New device ... Adding to map.\n");
				pthread_mutex_lock(&macmap_mutex);
				MacDB[mac_addr] = user;
				pthread_mutex_unlock(&macmap_mutex);


				pthread_mutex_lock(&mac_queue_lock);
				mac_queue.push(mac_addr);
				sem_post(&macq_sem);
				pthread_mutex_unlock(&mac_queue_lock);
			}
			else {
				pthread_mutex_lock(&macmap_mutex);
				USER_STAT status = itr->second.status;
				pthread_mutex_unlock(&macmap_mutex);

				if(status == USR_FREE) {
					//printf("\t\t\t\t\t- Known device, status = FREE.\n"); fflush(stdout);

					pthread_mutex_lock(&macmap_mutex);
					itr->second.status = USR_BUSY;
					pthread_mutex_unlock(&macmap_mutex);
				
					pthread_mutex_lock(&mac_queue_lock);
					mac_queue.push(mac_addr);
					sem_post(&macq_sem);
					pthread_mutex_unlock(&mac_queue_lock);				
				}
				else {
					//printf("\t\t\t\t\t- Known device, status = Busy.\n"); fflush(stdout);

					pthread_mutex_lock(&macmap_mutex);
					itr->second.ttl ++;
					if(itr->second.ttl > releaseTTL)
					{
						itr->second.ttl = 0;
						itr->second.status = USR_FREE;
						log(scanner_outfile, 4, "TTL passed limit, Releasing device \t%s\n", mac_addr);
					}
					pthread_mutex_unlock(&macmap_mutex);				
				}
			}
		}


		//send the MAC list to server .... then:
		if(Check_names) {
			log(scanner_outfile, 2, "Checking for names ...\n");
			for (register int i = 0; i < num_rsp; i++)
			{
				ba2str(&(inq_list+i)->bdaddr, mac_addr);
				
				pthread_mutex_lock(&macmap_mutex);	
				if(strcmp(MacDB[mac_addr].name, "unknown") == 0)
				{
					pthread_mutex_unlock(&macmap_mutex);	
				
					log(scanner_outfile, 3, "Getting device name for %s\n", mac_addr);
					if ( 0 <= hci_read_remote_name(hci_sock, &(inq_list+i)->bdaddr, sizeof(name), name, 0)) {
						log(scanner_outfile, 4, "MAC =  %s -> name = %s\n", mac_addr, name);

						pthread_mutex_lock(&macmap_mutex);
						strcpy(MacDB[mac_addr].name, name);					
						pthread_mutex_unlock(&macmap_mutex);
					}


					pthread_mutex_lock(&name_queue_lock);
					name_queue.push(mac_addr);
					sem_post(&nameq_sem);
					pthread_mutex_unlock(&name_queue_lock);
				} 
				else
				{
					pthread_mutex_unlock(&macmap_mutex);	
					//printf("\t\t\t\t- MAP: MAC =  %s -> name = %s\n", mac_addr, MacDB[mac_addr].name);
				}
			}
		}
		sleep(scan_wait);
	}


	//printf("\t\t\t- Free the used memory.\n");
	delete [] inq_list;


	printf("- Terminating scanner thread ...\n");fflush(stdout);
	log(scanner_outfile, 1, "Terminating scanner thread ...\n");
	pthread_exit(NULL);	
}

///////////////////////////////
///////////sender//////////////
///////////////////////////////
int findOPUSH(int hci_no, const char* mac) {
	int channel = SDP_NO_CHANNEL;	

	bdaddr_t target;
	str2ba (mac, &target);
	
	bdaddr_t interface;
	hci_devba(hci_no, &interface);


    sdp_session_t *session = sdp_connect( &interface, &target, SDP_RETRY_IF_BUSY );
	if (!session) {
		fprintf(SDP_outfile, "Failed to connect to SDP server on %s: %s\n", mac, strerror(errno)); fflush(SDP_outfile);
		return SDP_CONNECTION_ERROR;
	}

    

	sdp_list_t *response_list = NULL, *search_list, *attrid_list;
	search_list = sdp_list_append (NULL, &svc_uuid);
	attrid_list = sdp_list_append (NULL, &range);
	
	int err = sdp_service_search_attr_req (session, search_list, SDP_ATTR_REQ_RANGE, attrid_list, &response_list);
	if(err) {
		fprintf(SDP_outfile, "\t\tService search failed for device %s: %s\n", mac, strerror(errno)); fflush(SDP_outfile);
		sdp_close(session);
		return SDP_SEARCH_ERROR;
	}


	sdp_list_t *r = response_list;
	// go through each of the service records
	for (; r; r = r->next) {
		sdp_record_t *rec = (sdp_record_t *) r->data;
		sdp_list_t *proto_list;

		// get a list of the protocol sequences
		if (sdp_get_access_protos (rec, &proto_list) == 0) {
			sdp_list_t *p = proto_list;

			// go through each protocol sequence
			for (; p; p = p->next) {
				sdp_list_t *pds = (sdp_list_t *) p->data;

				// go through each protocol list of the protocol sequence
				for (; pds; pds = pds->next) {

					// check the protocol attributes
					sdp_data_t *d =
						(sdp_data_t *) pds->data;
					int proto = 0;
					for (; d; d = d->next) {
						switch (d->dtd) {
						case SDP_UUID16:
						case SDP_UUID32:
						case SDP_UUID128:
							proto = sdp_uuid_to_proto (&d->val.uuid);
							break;
						case SDP_UINT8:
							if (proto ==
							    RFCOMM_UUID) {
								channel = d->val.int8;
								printf("\t\t%s has OPUSH on channel %d\n", mac, channel);
							}
							break;
						}
					}
				}
				sdp_list_free ((sdp_list_t *) p->data, 0);
			}
			sdp_list_free (proto_list, 0);

		}
		sdp_record_free (rec);
	}
	sdp_close (session);

	return channel;
}

int Client::getDongleAndCMD(st_dongle *dongle, st_cmd *cmd)
{

	sem_wait(&donglesq_sem);
	pthread_mutex_lock(&donglesq_mutex);
	dongle->hci_num = dongles_q.front().hci_num;
	dongle->thread_num = dongles_q.front().thread_num;
	dongles_q.pop();
	pthread_mutex_unlock(&donglesq_mutex);

	sem_wait(&cmdsq_sem);
	pthread_mutex_lock(&cmdsq_mutex);
	strncpy(cmd->dev_mac, cmds_q.front().dev_mac, 18);
	cmd->file_id = cmds_q.front().file_id;
	cmds_q.pop();
	pthread_mutex_unlock(&cmdsq_mutex);

	return 0;
}

int Client::senderMainLoop()
{
	st_dongle* dongle;
	st_cmd* cmd;
	void** ptr = new void*[2];
	while(!stop)
	{
		dongle = new st_dongle;
		cmd = new st_cmd;
		//fprintf(stderr, "\tSEND WAIT FOR QUEUE\n");
		getDongleAndCMD(dongle, cmd);
		//fprintf(stderr, "\tSEND AFTER WAIT QUEUE\n");

		printf("\t\t >>>>>> In SenderMainLoop send file id %d to %s using HCI%d\n", cmd->file_id, cmd->dev_mac, dongle->hci_num); fflush(stdout);
		ptr[0] = (void*)dongle;
		ptr[1] = (void*)cmd;


		pthread_t onesendthread;
		pthread_create(&onesendthread, NULL, (void*(*)(void*)) &onesend, (void*)ptr);
	}
	delete[] ptr;
	pthread_exit(0);
}


int Client::onesend(void* ptr)
{
	int channel = -1;
	
	void** pptr = (void**)ptr;
	
	st_dongle* dongle = (st_dongle*)pptr[0];
	int hci_id = dongle->hci_num;
	int hci_th = dongle->thread_num;
	//printf("\t\t*\n\t\t*\n\t\t*\n\t\t\thci_num: %d\n\t\t*\n\t\t*\n\t\t*\n", dongle->hci_num);
	
	st_cmd* cur_cmd = (st_cmd*)pptr[1];
	char* dev_mac = cur_cmd->dev_mac;
	int file_id = cur_cmd->file_id;
	
	log(obex_outfile, 1, "[one send] Sending %d to %s using hci%d-%d\n", file_id, dev_mac, hci_id, hci_th);

	int timeout = obex_timeout;
	
	int send_res_status = 0;

	filemapitr_t itr = files_m.find(file_id);
	
	if(itr != files_m.end()) //file exist
	{
		st_file* send_file = itr->second;
		char* file_add = send_file->f_address;
		char* file_name= send_file->f_name;

		//printf("\t ------ Sending file %s to %s ...\n", file_add, dev_mac);
		//printf("------ Connecting to SDP server on %s using HCI%d\n", dev_mac, hci_id); fflush(stdout);
		 //findOPUSH(hci_id,dev_mac);//MacDB[dev_mac].channel; //find_opush_channel(hci_id, dev_mac);
//		sleep(1);

		pthread_mutex_lock(&macmap_mutex);
		channel = MacDB[dev_mac].channel;
		pthread_mutex_unlock(&macmap_mutex);
		
		if (channel < 0) { // OPUSH channel unknown: a new user, or a previous user which had error finding OPUSH before...
			channel = findOPUSH(hci_id, dev_mac);

			if (0 < channel) {
				printf("\t -------->>>> %s has OPUSH Channel %d\n", dev_mac, channel); fflush(stdout);
				pthread_mutex_lock(&macmap_mutex);
				MacDB[dev_mac].channel = channel;
				pthread_mutex_unlock(&macmap_mutex);			

				pthread_mutex_lock(&chan_queue_lock);
				chan_queue.push(dev_mac);
				sem_post(&chanq_sem);
				pthread_mutex_unlock(&chan_queue_lock);
			}		
		}


		printf("I'm still alive!\n");
		
		if(0 < channel)//found channel
		{
			char mac_chan[100];
			sprintf(mac_chan, "%s@%d", dev_mac, channel);
			//sprintf(mac_chan, "%s@", dev_mac);
			printf("CMD = %s\n", mac_chan);
			int opush_res = obex_push(hci_id, mac_chan, file_add, file_name, timeout);
			if(opush_res == 0) //success
			{
				log(obex_outfile, 2, "Sent to %d (%s) to %s successfully!\n", file_id, file_add, dev_mac);
				send_res_status = 0;
			}
			else //failure
			{
				send_res_status = opush_res;
				log(obex_outfile, 2, "send ERROR! sending %d (%s) to %s failed, res = %d\n", 
						file_id, file_add, dev_mac, opush_res);
			}
		}
		else //can't find chanel
		{
			send_res_status = channel; //SDP Errors
			//printf("error! can't find channel for mac: %s! channel is: %d\n", dev_mac, channel);
		}
	}
	else //file does not exist
	{
		send_res_status = 2;
		log(obex_outfile, 2, "FATAL Send error: Can't find file_id %d\n", file_id);
	}
	
	//fprintf(stderr, "AFTERSEND:\t%s\t%d\t%d\n", dev_mac, file_id, send_res_status);

	st_res result;
	strcpy(result.dev_mac, dev_mac);
	result.file_id = file_id;
	result.hci_num = hci_id;
	result.thread_num = hci_th;
	result.send_res_stat = send_res_status;
	result.send_time = time(NULL);

	///*
	pthread_mutex_lock(&donglesq_mutex);
	dongles_q.push(*dongle);
	sem_post(&donglesq_sem);
	pthread_mutex_unlock(&donglesq_mutex);
	//*/

	///*
	pthread_mutex_lock(&ressq_mutex);
	ress_q.push(result);
	sem_post(&ressq_sem);
	pthread_mutex_unlock(&ressq_mutex);
	//*/
	
	//delete dongle;
	//delete cur_cmd;

	//pthread_exit(0);
	//printf("\t\t---------- returning from onesend to MAC %s\n", dev_mac);
	return 0;
}

/////////////////////////////
///////////eo send///////////
/////////////////////////////


Client::~Client()
{

	pthread_mutex_destroy(&donglesq_mutex);
	pthread_mutex_destroy(&ressq_mutex);
	pthread_mutex_destroy(&cmdsq_mutex);

	sem_destroy(&donglesq_sem);
	sem_destroy(&ressq_sem);
	sem_destroy(&cmdsq_sem);
	
	pthread_mutex_destroy(&name_queue_lock);
	pthread_mutex_destroy(&mac_queue_lock);
	pthread_mutex_destroy(&chan_queue_lock);
	pthread_mutex_destroy(&macmap_mutex);
	//sem_destroy(&queue_sem);
	sem_destroy(&macq_sem);
	sem_destroy(&chanq_sem);
	sem_destroy(&nameq_sem);
	
	pthread_mutex_destroy(&client_out_mutex);

	pthread_mutex_destroy(&obex_out_mutex);
	pthread_mutex_destroy(&sdp_out_mutex);
	pthread_mutex_destroy(&qh_out_mutex);
	pthread_mutex_destroy(&scn_out_mutex);
	pthread_mutex_destroy(&sif_out_mutex);

#ifdef LOG
	#ifndef DEBUG
	fclose(obex_outfile);
	fclose(SDP_outfile);
	fclose(qhandler_outfile);
	fclose(scanner_outfile);
	fclose(sinterface_outfile);
	#endif
#endif

	close(sock_fd);
}

