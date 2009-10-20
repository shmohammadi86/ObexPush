#include "Server.h"
#include <stdarg.h>

int done;
Client_struct *Clients[MAX_CLIENTS];
pthread_mutex_t queue_lock[MAX_CLIENTS]; sem_t queue_sem[MAX_CLIENTS];

int sendsendcount = 0;

//map <int, int> FD2ID;
vector<int> R_FD;
map<string, vector<int> > MAC2SL;
map<string, int> MAC2ID;
map<string, int> MAC2Status;

map<int, int> FILEMAP;

long int checkPeriod;
int rejectAllTh;
int acceptedDT;
int rejectedDT;
int failureDT;
int acceptedTh;
int rejectedTh;
int failureTh;
MYSQL* conn;
//MYSQL* fileconn;

int releaseCounter = 0;

char cur_time_str[256];

#define SEND 0
#define RECV 1
int log(int client_id, int type, int level, char* fmt, ...) {
	FILE* fd = type?Clients[client_id]->recv_log:Clients[client_id]->send_log;
	pthread_mutex_t* fd_mutex = type?&Clients[client_id]->recv_log_mutex:&Clients[client_id]->send_log_mutex;

	time_t ct = time(NULL);
	char time_str[256];
	sprintf(time_str, "%s", ctime(&ct));
	time_str[strlen(time_str)-1] = '\0';

	va_list ap;

	va_start(ap, fmt); 
	
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

MYSQL* initLocalDB()
{
	MYSQL *myconn;
	char *server = new char[20];
	char *user = new char[20];
	char *password = new char[20];
	char *database = new char[20];
	sprintf(server, "localhost");
	sprintf(user, "root");
	sprintf(password, "12345");
	sprintf(database, "bluetoothad");
	myconn = mysql_init(NULL);
	if (!mysql_real_connect(myconn, server,
				user, password, database, 0, NULL, 0)
	   )
	{
		fprintf(stderr, "MYSQL CONNECT ERR: %s\n", mysql_error(myconn));
		return NULL;
	}
	return myconn;
}

int localInit()
{
	if(conn == NULL)
		return 0;
	///*	
	MYSQL_RES *res;
	MYSQL_ROW row;
	char querry[512];

	sprintf(querry, "select *\n\
		       	from generalconf;");

	if(mysql_query(conn, querry))
	{
		fprintf(stderr, "MYSQL ERR:\n\tquerry: %s\n\terror: %s\n", querry, mysql_error(conn));
		return 0;
	}

	res = mysql_use_result(conn);


	char sendername[100];
	int donglethreadcount=0;
	int Obex_timeout;
	int User_TTL;
	int Scanner_Delay;
	
	while ((row = mysql_fetch_row(res)) != NULL)
	{

		rejectAllTh = atoi(row[3]);
		rejectedTh = atoi(row[4]);
		failureTh = atoi(row[5]);
		acceptedTh = atoi(row[6]);
		rejectedDT = atoi(row[7]);
		failureDT = atoi(row[8]);
		acceptedDT = atoi(row[9]);
		checkPeriod = atoi(row[12]);
		//printf(" rejectAllTh: %d\n rejectedTh: %d\n failureTh: %d\n acceptedTh: %d\n rejectedDT: %d\nfailureDT: %d\n acceptedDT: %d\n checkPeriod: %ld\n",
		//		rejectAllTh, rejectedTh, failureTh, acceptedTh, rejectedDT, failureDT, acceptedDT, checkPeriod);

		donglethreadcount = atoi(row[1]);
		strcpy(sendername, row[2]);
		Obex_timeout = atoi(row[10]);
		User_TTL = atoi(row[11]);		
		Scanner_Delay = atoi(row[13]);
		break;
	}
	mysql_free_result(res);

	return 0;

}

int initDB(FILE* net, int id)
{
	/*
	MYSQL *conn = initLocalDB();
	//*/
	if(conn == NULL)
		return 0;
	///*	
	MYSQL_RES *res;
	MYSQL_ROW row;
	char querry[512];

	sprintf(querry, "select *\n\
		       	from generalconf;");

	if(mysql_query(conn, querry))
	{
		fprintf(stderr, "MYSQL ERR:\n\tquerry: %s\n\terror: %s\n", querry, mysql_error(conn));
		return 0;
	}

	res = mysql_use_result(conn);


	char sendername[100];
	int donglethreadcount=0;
	int Obex_timeout;
	int User_TTL;
	int Scanner_Delay;
	
	while ((row = mysql_fetch_row(res)) != NULL)
	{

		rejectAllTh = atoi(row[3]);
		rejectedTh = atoi(row[4]);
		failureTh = atoi(row[5]);
		acceptedTh = atoi(row[6]);
		rejectedDT = atoi(row[7]);
		failureDT = atoi(row[8]);
		acceptedDT = atoi(row[9]);
		checkPeriod = atoi(row[12]);
		//printf(" rejectAllTh: %d\n rejectedTh: %d\n failureTh: %d\n acceptedTh: %d\n rejectedDT: %d\nfailureDT: %d\n acceptedDT: %d\n checkPeriod: %ld\n",
		//		rejectAllTh, rejectedTh, failureTh, acceptedTh, rejectedDT, failureDT, acceptedDT, checkPeriod);

		donglethreadcount = atoi(row[1]);
		strcpy(sendername, row[2]);
		Obex_timeout = atoi(row[10]);
		User_TTL = atoi(row[11]);		
		Scanner_Delay = atoi(row[13]);
		break;
	}
	mysql_free_result(res);

	//time_t ct = time(NULL);
	struct timeval now;
	gettimeofday(&now, NULL);


	fprintf(net, "time %ld\t%ld\n", now.tv_sec, now.tv_usec);

	fprintf(net, "thread# %d\n", donglethreadcount);
	fflush(net);
	fprintf(net, "devname %s\t%d\n", sendername, id);
	fflush(net);
	fprintf(net, "ObexTimeout %d\n", Obex_timeout);
	fflush(net);
	fprintf(net, "TTL %d\n", User_TTL);
	fflush(net);
	fprintf(net, "ScanDelay %d\n", Scanner_Delay);
	fflush(net);


	sprintf(querry, "select * from file");
	if(mysql_query(conn, querry))
	{
		fprintf(stderr, "MYSQL ERR:\n\tquerry: %s\n\terror: %s\n", querry, mysql_error(conn));
		return 0;
	}

	res = mysql_use_result(conn);

	string file_res;
	while ((row = mysql_fetch_row(res)) != NULL)
	{
		fprintf(net, "file %s\t%s\t%s\n", row[0], row[1], row[2]);
		fflush(net);
	}
	mysql_free_result(res);
	//mysql_close(conn);


	return 0;
}



int checkMac(char* mac, char *type)
{
	//fprintf(stderr, "CHECKMAC:\t%s\n", mac);
	int mac_id = -1;
	/*
	MYSQL* conn = initLocalDB();
	//*/
	if(conn == NULL)
	{
		return mac_id;
	}
	
	MYSQL_RES *res;
	MYSQL_ROW row;
	char querry[512];

	sprintf(querry, "select id from clients where mac = '%s';", mac);

	if(mysql_query(conn, querry))
	{
		fprintf(stderr, "MYSQL ERR:\n\tquerry: %s\n\terror: %s\n", querry, mysql_error(conn));
		return -1;
	}
	res = mysql_use_result(conn);
	while ((row = mysql_fetch_row(res)) != NULL)
	{
		mac_id = atoi(row[0]);
	}
	mysql_free_result(res);

	if(mac_id == -1)
	{
		time_t ct = time(NULL);
		tm* cur_t = localtime(&ct);
		char time_str[256];
		char date_str[256];
		sprintf(time_str, "%d:%d:%d", cur_t->tm_hour, cur_t->tm_min, cur_t->tm_sec);
		sprintf(date_str, "%d-%d-%d", cur_t->tm_year+1900, cur_t->tm_mon+1, cur_t->tm_mday);

		sprintf(querry, "insert into clients (mac, type, time, date) values('%s', '%s', '%s', '%s');", mac, type, time_str, date_str);
		if(mysql_query(conn, querry))
		{
			fprintf(stderr, "MYSQL ERR:\n\tquerry: %s\n\terror: %s\n", querry, mysql_error(conn));
			return -1;
		}

		sprintf(querry, "select id from clients where mac = '%s';", mac);
		if(mysql_query(conn, querry))
		{
			fprintf(stderr, "MYSQL ERR:\n\tquerry: %s\n\terror: %s\n", querry, mysql_error(conn));
			return -1;
		}
		res = mysql_use_result(conn);
		while ((row = mysql_fetch_row(res)) != NULL)
		{
			mac_id = atoi(row[0]);
		}
		mysql_free_result(res);
	}

	//mysql_close(conn);


	return mac_id;
}

int updateMacMap(char* mac, int client_id)
{
	if(MAC2SL.find(mac) == MAC2SL.end())
	{
		vector<int> tv;
		tv.push_back(client_id);
		MAC2SL[mac] = tv;
	}
	else
	{
		vector<int> tv = MAC2SL[mac];
		int i=0;
		for(i=0; i< (int)tv.size(); i++)
			if(tv[i] == client_id)
				break;
		if(i == (int)tv.size())
			MAC2SL[mac].push_back(client_id);
	}

	return 0;
}

int updateMac(int mac_id, char* name,  int client_id)
{
	//fprintf(stderr, "UPDATEMAC:\t%d\t%s\n", mac_id, name);
	/*
	MYSQL* conn = initLocalDB();
	//*/
	if(conn == NULL)
		return mac_id;
	
	char querry[512];

	sprintf(querry, "update clients set name='%s',place='client_%d' where id = %d;", name, client_id, mac_id);

	if(mysql_query(conn, querry))
	{
		fprintf(stderr, "MYSQL ERR:\n\tquerry: %s\n\terror: %s\n", querry, mysql_error(conn));
		return 0;
	}
	//mysql_close(conn);
	return 0;
}


int updateChannel(int mac_id, int channel)
{
	//fprintf(stderr, "UPDATEMAC:\t%d\t%s\n", mac_id, name);
	/*
	MYSQL* conn = initLocalDB();
	//*/
	if(conn == NULL)
		return mac_id;
	
	char querry[512];

	sprintf(querry, "update clients set channel=%d where id = %d;", channel, mac_id);

	if(mysql_query(conn, querry))
	{
		fprintf(stderr, "MYSQL ERR:\n\tquerry: %s\n\terror: %s\n", querry, mysql_error(conn));
		return 0;
	}
	//mysql_close(conn);
	return 0;
}


int updateSend(int mac_id, int file_id, time_t send_time, int send_status, int client_id)
{
	//fprintf(stderr, "UPDATERES:\t%d\t%d\t%d\n", mac_id, file_id, send_status);
	/*
	MYSQL* conn = initLocalDB();
	//*/
	if(conn == NULL)
		return mac_id;
	
	tm* sendtime = localtime(&send_time);
	char date_str[32];
	char time_str[32];
	sprintf(date_str, "%d-%d-%d", sendtime->tm_year+1900, sendtime->tm_mon+1, sendtime->tm_mday);
	sprintf(time_str, "%d:%d:%d", sendtime->tm_hour, sendtime->tm_min, sendtime->tm_sec);

	int res = 0;
	if(send_status == -2)
	{
		res = 1;
	}
	else if(send_status != 0)
	{
		res = 2;
	}

//	int res = send_status;
	
	char querry[512];
	sprintf(querry, "insert into send (file_id, client_id, date, time, status, place) values(%d, %d, '%s', '%s', %d, 'client%d');", 
			file_id, mac_id, date_str, time_str, res, client_id);
	if(mysql_query(conn, querry))
	{
		fprintf(stderr, "MYSQL ERR:\n\tquerry: %s\n\terror: %s\n", querry, mysql_error(conn));
		return 0;
	}
	//printf("update send!\n");
	sprintf(querry, "insert into temp_send (file_id, client_id, date, time, status, place) values(%d, %d, '%s', '%s', %d, 'client%d');", 
			file_id, mac_id, date_str, time_str, res, client_id);
	if(mysql_query(conn, querry))
	{
		fprintf(stderr, "MYSQL ERR:\n\tquerry: %s\n\terror: %s\n", querry, mysql_error(conn));
		return 0;
	}
	//printf("update temp send!\n");


	//mysql_close(conn);

	return 0;
}

int compareDate(int fy, int fm, int fd, int sy, int sm, int sd)
{
	if(fy > sy)
		return 1;
	else if(fy < sy)
		return -1;
	if(fm > sm)
		return 1;
	else if(fm < sm)
		return -1;
	if(fd > sd)
		return 1;
	else if(fd < sd)
		return -1;

	return 0;
}

int compareTime(int fh, int fm, int fs, int sh, int sm, int ss)
{
	if(fh > sh)
		return 1;
	else if(fh < sh)
		return -1;
	if(fm > sm)
		return 1;
	else if(fm < sm)
		return -1;
	if(fs > ss)
		return 1;
	else if(fs < ss)
		return -1;

	return 0;
}

bool checkTime(time_t c_time, tm* curtime, char* begtime, char* endtime, char* begdate, char* enddate, char* week)
{
	//printf("week: %s beg: %s %s end: %s %s\n", week, begdate, begtime, enddate, endtime);
	//printf("cur time: %s\n", asctime(curtime));
	bool res = true;

	int wd = curtime->tm_wday;
	wd = wd+1;
	if(wd > 6)
		wd -= 7;

	if(week[wd] == '0')
	{
		res = false;
	}
	
	if(res)
	{
		int cy = curtime->tm_year+1900;
		int cm = curtime->tm_mon + 1;
		int cd = curtime->tm_mday;

		int by, bm, bd;
		sscanf(begdate, "%d-%d-%d", &by, &bm, &bd);

		int ey, em, ed;
		sscanf(enddate, "%d-%d-%d", &ey, &em, &ed);

		if(	compareDate(by, bm, bd, cy, cm, cd) == 1 ||
			compareDate(cy, cm, cd, ey, em, ed) == 1 )
			res = false;
	}

	if(res)
	{
		int ch = curtime->tm_hour;
		int cm = curtime->tm_min;
		int cs = curtime->tm_sec;

		int bh, bm, bs;
		sscanf(begtime, "%d:%d:%d", &bh, &bm, &bs);

		int eh, em, es;
		sscanf(endtime, "%d:%d:%d", &eh, &em, &es);

		if(	compareTime(bh, bm, bs, ch, cm, cs) == 1 ||
			compareTime(ch, cm, cs, eh, em, es) == 1 )
			res = false;
	}

	return res;
}

void computeFilesList(int delta_t)
{
	printf("\n\t- Updating filelist ... ");
	int maxPr = 4;

	///*
	MYSQL *fileconn = initLocalDB();
	//*/
	if(fileconn == NULL)
		return;

	///*	
	MYSQL_RES *res;
	MYSQL_ROW row;
	char querry[512];

	sprintf(querry, "select id, file_id, priority, begtime, endtime, begdate, enddate, week from priority");
	if(mysql_query(fileconn, querry))
	{
		fprintf(stderr, "MYSQL ERR:\n\tquerry: %s\n\terror: %s\n", querry, mysql_error(fileconn));
		return;
	}
	res = mysql_use_result(fileconn);

	time_t c_time;
	time(&c_time);
	tm* curtime;
	curtime = localtime(&c_time);
	FILEMAP.clear();

	//printf("\n\n\n");
	int file_id, priority;
	while ((row = mysql_fetch_row(res)) != NULL)
	{
		file_id = atoi(row[1]);
		priority = atoi(row[2]);
//		printf("id: %s file_id: %d priority: %d\n", row[0], file_id, priority);
		if(checkTime(c_time, curtime, row[3], row[4], row[5], row[6], row[7]))
		{
			FILEMAP[file_id] = maxPr - priority;
		}
	}
	//printf("\n\n\n");
	mysql_free_result(res);
	mysql_close(fileconn);

	printf("%d file imported successfully\n\t- Enter command: ", FILEMAP.size()); fflush(stdout);
	alarm(60);

	return;
}

int filterMac(MYSQL* myconn, int mac_id)
{
	MYSQL_RES *res;
	MYSQL_ROW row;
	char querry[512];

	sprintf(querry, "select count(*) from temp_send\n\
			where status = 1 and client_id = %d;",
			mac_id);
	
	if(mysql_query(myconn, querry))
	{
		fprintf(stderr, "MYSQL ERR:\n\tquerry: %s\n\terror: %s\n", querry, mysql_error(myconn));
		return 0;
	}
	res = mysql_use_result(myconn);

	int count = 0;
	while ((row = mysql_fetch_row(res)) != NULL)
	{
		count = atoi(row[0]);
	}
	mysql_free_result(res);

	return count;
}

int deselectFile(MYSQL* myconn, int mac_id, int deltatime, int status, int threshold, map<int, int>& localmap)
{
	if(localmap.size() == 0)
	{
		//printf("status: %d, nothing to deselect!\n", status);
		return 0;
	}
	
	MYSQL_RES *res;
	MYSQL_ROW row;
	char querry[512];

	if(deltatime == 0)
	{
		sprintf(querry, "select file_id, count(file_id) from temp_send\n\
				where status = %d and client_id = %d group by file_id;",
				status, mac_id);
	}
	else
	{
		time_t c_time = time(NULL);
		//printf("%s\n", asctime(localtime(&c_time)));
		c_time = c_time - deltatime;
		//printf("%s\n", asctime(localtime(&c_time)));

		tm* curtime = localtime(&c_time);

		char Date[32];
		char Time[32];
		sprintf(Date, "%d-%d-%d", curtime->tm_year+1900, curtime->tm_mon+1, curtime->tm_mday);
		sprintf(Time, "%d:%d:%d", curtime->tm_hour, curtime->tm_min, curtime->tm_sec);


		sprintf(querry, "select file_id, count(file_id) from temp_send\n\
				where status = %d and client_id = %d and (date > '%s' or(date='%s' and time > '%s')) group by file_id;",
				status, mac_id, Date, Date, Time);

		//printf("querry: %s\n", querry);
	}

	if(mysql_query(myconn, querry))
	{
		fprintf(stderr, "MYSQL ERR:\n\tquerry: %s\n\terror: %s\n", querry, mysql_error(myconn));
		return 0;
	}
	res = mysql_use_result(myconn);

	int count = 0;
	int file_id = 0;
	map<int, int>::iterator itr;
	while ((row = mysql_fetch_row(res)) != NULL)
	{
		count = atoi(row[1]);
		file_id = atoi(row[0]);
		//fprintf(stderr, "\t\t Deselecting files for MAC: %d FILE: %d, COUNT: %d, status = %d, threshold = %d, dt = %d\n", 
		//		mac_id, file_id, count, status, threshold, deltatime);
		if(count >= threshold)
		{
			itr = localmap.find(file_id);
			if(itr != localmap.end())
				localmap.erase(itr);
		}
	}
	//printf("\n\n\n");
	mysql_free_result(res);

	return 0;
}

int selectFile(int mac_id)
{
	int status = 0;

	map<int, int> localmap = FILEMAP;
	map<int, int>::iterator itr;

	fprintf(stderr, "SELECT FILE\n");
	
	fprintf(stderr, "\t\t filelist has %d elements\n", localmap.size());
	for(itr = localmap.begin(); itr!=localmap.end(); itr++)
	{
		fprintf(stderr, "ID: %d\n", itr->first);
	}
	

	int rejectCountMac = filterMac(conn, mac_id);
	//printf("Number of Rejects for client id %d = %d\n", mac_id, rejectCountMac);
	//fprintf(stderr, "REJECT COUNT:\nmac: %d count: %d\n", mac_id, rejectCountMac);
	if(rejectCountMac >= rejectAllTh)
	{
		//mysql_close(conn);
		return -1;
	}

	//accepted
	//fprintf(stderr, "ACCEPTED:\n");
	
	status = 0;
	deselectFile(conn, mac_id, acceptedDT, status, acceptedTh, localmap);
	fprintf(stderr, "\t\t filelist after Deselecting Accept has %d elements\n", localmap.size());
	for(itr = localmap.begin(); itr!=localmap.end(); itr++)
	{
		fprintf(stderr, "---- ID: %d\n", itr->first);
	}
	//rejected
	//fprintf(stderr, "REJECTED:\n");
	status = 1;
	deselectFile(conn, mac_id, rejectedDT, status, rejectedTh, localmap);
	//failed
	//fprintf(stderr, "FAILED:\n");
	status = 2;
	deselectFile(conn, mac_id, failureDT, status, failureTh, localmap);

	/*
	fprintf(stderr, "SECOND LIST\n");
	for(itr = localmap.begin(); itr!=localmap.end(); itr++)
	{
		fprintf(stderr, "ID: %d\n", itr->first);
	}
	*/
	//fprintf(stderr, "EO SELECT FILE\n");

	if(localmap.size() == 0)
	{
		//printf("\t\t No file in the final selection for client id %d ... return file_if = -1\n", mac_id);
		return -1;
	}

	int file_id = -1;

	long int sum = 0;
	for(itr = localmap.begin(); itr!=localmap.end(); itr++)
	{
		sum += itr->second;
	}

	long int r = rand()%sum;
	long int localsum = 0;

	for(itr = localmap.begin(); itr!=localmap.end(); itr++)
	{
		localsum += itr->second;
		if(r < localsum)
		{
			file_id = itr->first;
			break;
		}
	}

	//printf("\t\t Done selecting file for client id %d ... returning file_id = %d\n", mac_id, file_id);
	return file_id;
}

int sendReleaseCMD(char* mac)
{
	//printf("\t\t Releasing mac %s\n", mac); fflush(stdout);
	map<string, vector<int> >::iterator itr = MAC2SL.find(mac);
	if(itr == MAC2SL.end())
	{
		fprintf(stderr, "\tRELEASE: can't find mac:\t%s\n", mac);
		fflush(stderr);
		return -1;
	}
	else
	{
		vector<int> tv = itr->second;
		int size = (int)tv.size();
		for(int i=0; i < size; i++)
		{
			Command *new_cmd = new Command();
			new_cmd->type = 2;
			sprintf(new_cmd->str, "release %s", mac);
			//log(tv[i], SEND, 1, "enqueue RELEASE:\t%s\t%d\n", mac, tv[i]);

			pthread_mutex_lock(&queue_lock[tv[i]]);
			Clients[tv[i]]->cmd_q.push(new_cmd);
			//printf("****** Sending Sem_Post from release ******\n"); fflush(stdout);
			sem_post(&queue_sem[tv[i]]);
			pthread_mutex_unlock(&queue_lock[tv[i]]);
		}
		MAC2SL[mac].clear();
		MAC2Status[mac] = USR_FREE;
	}
	return 0;
}

int cloneDB(long int dt)
{
	/*
	MYSQL* conn = initLocalDB();
	//*/
	if(conn == NULL)
		return 0;

	time_t c_time = time(NULL);
	c_time = c_time - dt;
	tm* curtime = localtime(&c_time);

	char date_str[32];
	char time_str[32];
	sprintf(date_str, "%d-%d-%d", curtime->tm_year+1900, curtime->tm_mon+1, curtime->tm_mday);
	sprintf(time_str, "%d:%d:%d", curtime->tm_hour, curtime->tm_min, curtime->tm_sec);
	
	char querry[512];

	sprintf(querry, "delete from temp_send;");

	if(mysql_query(conn, querry))
	{
		fprintf(stderr, "MYSQL ERR:\n\tquerry: %s\n\terror: %s\n", querry, mysql_error(conn));
		return 0;
	}

	sprintf(querry, "insert into temp_send (select * from send where (date > '%s' or (date = '%s' and time > '%s') ) );", 
			date_str, date_str, time_str);
	//printf("clone db query: %s\n", querry);

	if(mysql_query(conn, querry))
	{
		fprintf(stderr, "MYSQL ERR:\n\tquerry: %s\n\terror: %s\n", querry, mysql_error(conn));
		return 0;
	}

	//mysql_close(conn);

	return 0;
}

int readConfig()
{
/*	checkPeriod = 24*3600;
	rejectAllTh = 10;
	acceptedDT=0;
       	rejectedDT=3600;
	failureDT =3600;
	acceptedTh=1;
	rejectedTh=5;
	failureTh =5;
*/
	localInit();
	return 0;
}



Server::Server(int server_port)
{
	time_t ct = time(NULL);
	sprintf(cur_time_str, "%s", ctime(&ct));
	cur_time_str[strlen(cur_time_str)-1] = '\0';
	char cmd[256];
	sprintf(cmd, "mkdir \"log/%s\"", cur_time_str);
	int sys_res;
	sys_res = system(cmd);

	conn = initLocalDB();
/*	if(conn == NULL)
		return -1;
*/
	
	printf("+ Starting up Server ...\n");
	this->port = server_port;
	for(int i = 0; i < MAX_CLIENTS; i++) {
		sem_init(&queue_sem[i], 0, 0);
		pthread_mutex_init (&queue_lock[i], NULL);
	}
		
	printf("\t- Opening server socket ... ");
	server_socket = socket(PF_INET, SOCK_STREAM, 0);
	if (server_socket < 0) {
		printf("Can't open <Main Server> socket.\n");
		exit(-1);
	}
	printf("Done.\n");


	printf("\t- Binding to port %d ... ", port);
	int tr=1;
	if (setsockopt(server_socket,SOL_SOCKET,SO_REUSEADDR,&tr,sizeof(int)) == -1) {
	    printf("\t\t* Can't change the socket options.\n");
	}
	struct sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = INADDR_ANY;
	server_addr.sin_port = (unsigned short) htons(port);
	if (bind(server_socket, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
		printf("Can't bind to port %d.\n", port);
		exit(-1);
	}
	printf("Done.\n");

	readConfig();
	cloneDB(checkPeriod);

	//fileconn = initLocalDB();
	//conn = initLocalDB();

	srand ( time(NULL) );


	struct sigaction timaction, old_timaction;
	timaction.sa_handler = 	computeFilesList;
	sigemptyset (&timaction.sa_mask);
	timaction.sa_flags = 0;
	sigaction(SIGALRM, &timaction, &old_timaction);
}

int Server::Run() {
	pthread_t listener_thread, scanner_thread, console_thread;

	printf("\t- Listening on port %d ... ", port);
	if (listen(server_socket, 16) < 0) {
		printf("Can't listen on the <Main Server> socket.\n");
		exit(-1);
	}
	printf("Done.\n");
	for(int i = 0; i < MAX_CLIENTS; i++) {
		Clients[i] = NULL;	
	}
	
	pthread_create(&listener_thread, NULL, &listener, this);
	pthread_create(&scanner_thread, NULL, &scanner_handler, this);
	pthread_create(&console_thread, NULL, &console, this);
	pthread_join(console_thread, NULL);
	pthread_join(listener_thread, NULL);
	pthread_join(scanner_thread, NULL);
	
		
	return 0;	
}

void *Server::listener(void *args) {
	register int i;
	struct sockaddr_in client_addr;
	socklen_t client_addr_len;
	struct Client_struct *c;

	Server *me = (Server *)args;
	int maxsock = me->server_socket;
	fd_set socks;
	struct timeval timeout;
	int res;

	
	printf("\t+ Waiting for Clients ... ");
	computeFilesList(0);
	while (!done) {
		FD_ZERO(&socks);
		FD_SET(maxsock,&socks);
		timeout.tv_sec = 1;
		timeout.tv_usec = 0;
		
		res = select(maxsock + 1, &socks, NULL, NULL, &timeout);
		
		if (0 < res && FD_ISSET(maxsock,&socks)) {
			// accept one connection
			client_addr_len = sizeof(client_addr);
			//c = (struct Client_struct *) malloc(sizeof(struct Client_struct));
			c = new Client_struct();
			c->client_fd = accept(me->server_socket, (struct sockaddr *) &client_addr, &client_addr_len);
			if (c->client_fd < 0) {
				fprintf(stderr, "\n\t- Error accepting client!\n");fflush(stderr);
			}
			else {
				for(i = 0; i < MAX_CLIENTS; i ++) {
					if(Clients[i] == NULL) {
						printf("\n\t+ Connection accepted: FD = %d, Slot = %d.\n", c->client_fd, i);					
						c->client_id = i;  // set the client id for the send thread ...
						c->client_in = fdopen(c->client_fd, "r");
						c->client_out = fdopen(c->client_fd, "w");
						char outname[256];
						sprintf(outname, "log/%s/send_%d.log", cur_time_str, i);
						//c->send_log = fopen(outname, "w+");
						c->send_log = stderr;
						pthread_mutex_init(&c->send_log_mutex, NULL);
						sprintf(outname, "log/%s/recv_%d.log", cur_time_str, i);
						//c->recv_log = fopen(outname, "w+");
						c->recv_log = stderr;
						pthread_mutex_init(&c->recv_log_mutex, NULL);
						Clients[i] = c;
	
						//FD2ID[c->client_fd] = i; //Map the client id to file descriptor ...
						R_FD.push_back(c->client_fd); //push the file descriptor in the recieve list
						break;
					}	
				}
				if (i == MAX_CLIENTS) {
					printf("\n\t+ Connection with FD = %d refused. Maximum number of connections reached\n", c->client_fd);
				}
			}
			
			printf("\t- Enter command: "); fflush(stdout);
			// spawn a thread to send commands to clients ...
			pthread_create(&c->client_handler, NULL, &(me->new_client), (void*) c);
		}
	}

	for(i = 0; i < MAX_CLIENTS; i++) {
		if(Clients[i] != NULL) {
			printf("\n\t- Waiting for client handler %d to dispose ...", i);
			pthread_join(Clients[i]->client_handler, NULL);	
		}	
	}
	printf("\n\t- Exiting listener thread ...");
	
	pthread_exit(NULL);
}


void *Server::new_client(void *args) {
	Client_struct *c = (Client_struct *)args;
	
	fprintf(c->client_out, "get options\n"); fflush(c->client_out);

	initDB(c->client_out, c->client_id);

	fprintf(c->client_out, "end options\n"); fflush(c->client_out);
	// wait for ACK ... then send options ...
	fprintf(c->client_out, "start\n"); fflush(c->client_out);


	//wait on CMD_Q[c->client_id] ... send to c->client_out ...
	
	
	
	while(!done && Clients[c->client_id] != NULL) {
 		while (sem_wait(&queue_sem[c->client_id]) != 0);
 		if(done)
			break;
		pthread_mutex_lock(&queue_lock[c->client_id]);
		if(c->cmd_q.size() > 0)
		{
			log(c->client_id, SEND, 1, "dequeue cmd, send command type: %d\tcommand %s to Client %d\t queue size: %d\n", 
					c->cmd_q.front()->type, 
					c->cmd_q.front()->str, 
					c->client_id, 
					c->cmd_q.size());

			fflush(stdout);
			if(c->cmd_q.front()->type == 1)
			{
				sendsendcount++;
				printf("sending <send> CMD, count(%d): %s\n\n\n", sendsendcount, c->cmd_q.front()->str);
			}
			else if(c->cmd_q.front()->type == 2)
			{
				sendsendcount++;
				printf("sending <release> CMD: %s\n\n\n", c->cmd_q.front()->str);
			}

			fprintf(c->client_out, "%s\n", c->cmd_q.front()->str);
			fflush(c->client_out);
			//parse(events.front(), resp);
			//fprintf(UM2FD[events.front()->UMID], "%s", resp->cmd);
			c->cmd_q.pop();
		}
		else {
			fprintf(stderr, "\t\t FATAL QUEUE Error in client %d\n", c->client_id); fflush(stderr);
		}
		pthread_mutex_unlock(&queue_lock[c->client_id]);
		
	}
		
	if(Clients[c->client_id] != NULL) {
		fprintf(c->client_out, "quit\n"); fflush(c->client_out);
		sleep(1);
		close(c->client_fd); 
		fclose(c->recv_log);
		fclose(c->send_log);
		pthread_mutex_destroy(&c->recv_log_mutex);
		pthread_mutex_destroy(&c->send_log_mutex);
		Clients[c->client_id] = NULL;
	}
	
	printf("\n\t\t- Exiting client handler %d ...", c->client_id);
	pthread_exit(NULL);
}

	
void *Server::scanner_handler(void *args) {
	register int i;
	int maxsock;
	fd_set socks;
	struct timeval timeout;
	int res;
	
	while (!done) {
		FD_ZERO(&socks);
		for (i = 0; i < MAX_CLIENTS; i++) {
			if (Clients[i] != NULL) {
				FD_SET(Clients[i]->client_fd,&socks);
				if (Clients[i]->client_fd > maxsock)
					maxsock = Clients[i]->client_fd;
			}
		}
		timeout.tv_sec = 1;
		timeout.tv_usec = 0;
		
		res = select(maxsock + 1, &socks, NULL, NULL, &timeout);
		
		if(res < 0) {
			printf("\n\t- Error selecting file descriptor for reading.\n");	
		}
		else if(res) {
			for (i = 0; i < MAX_CLIENTS; i++) {
				if (Clients[i] != 0) {
					if (FD_ISSET(Clients[i]->client_fd,&socks)) {
						char buffer[1024];
//						if (fscanf(Clients[i]->client_in, "%1024[^\n]\n", buffer) == -1) {
						if (fgets(buffer, 1024, Clients[i]->client_in) == NULL) {
							if(!done) {
								printf("\n\t- Connection lost: FD=%d;  Slot=%d", Clients[i]->client_fd, i);
								close(Clients[i]->client_fd);
								updateMaps(Clients[i]->client_id);
								fclose(Clients[i]->recv_log);
								fclose(Clients[i]->send_log);
								pthread_mutex_destroy(&Clients[i]->recv_log_mutex);
								pthread_mutex_destroy(&Clients[i]->send_log_mutex);
								Clients[i] = NULL;
							}
						} 
						else {
//							pthread_t parser_thread;
							Event *event = new Event();
							strcpy(event->buffer, buffer);
							event->client_id = i;
							//pthread_create(&parser_thread, NULL, &(parse), (void*) event);
							//int cmd_id = parse(event);							
							//log(i, RECV, 1, "Parsed Command type = %d\n", cmd_id);
							parse(event);
							delete event;
						}						
					}
				}
			}
		}
		else {
			//printf("\t Nothing new! how do you do ?:D\n");	
		}
	}

	printf("\n\t- Exiting scanner thread ...");
	pthread_exit(NULL);
}


int Server::parse(Event *event) {	
	int client_id = event->client_id;
	char *buffer = event->buffer;
//	char cmd[256]; 
//	sscanf(buffer, "%s", cmd); 
	
	
//	printf("\ngot new message! buffer: %s\n", buffer);
	//fprintf(stderr, "Got new message %s from client_id %d\n", buffer, client_id); fflush(stderr);
	buffer[(int)strlen(buffer)-1] = 0;
	log(client_id, RECV, 1, "Got new message %s from client_id %d\n", buffer, client_id);
	//printf("Got new message %s from client_id %d\n", buffer, client_id);fflush(stdout);
	if(!strncmp(buffer, "mac", 3))
	{
		char mac[256];
		char type[256];
		sscanf(buffer+4, "%s", mac);
		sscanf(buffer+22, "%[^\n]\n", type);
		releaseCounter++;
		//fprintf(stderr, "\t-- MAC= %s (%s), releaseCounter: %d\n", mac, type, releaseCounter);fflush(stderr);
		log(client_id, RECV, 2, "parse <mac> message: MAC= %s (%s), releaseCounter: %d\n", mac, type, releaseCounter);
		//printf("\nparse <mac> message: MAC= %s (%s)\n\n\n", mac, type);fflush(stdout);
		updateMacMap(mac, client_id);
		int mac_id = -1;
		if(MAC2ID.find(mac) == MAC2ID.end())
		{
			mac_id = checkMac(mac, type);
			MAC2ID[mac] = mac_id;
			MAC2Status[mac] = USR_BUSY;
		}
		else
		{
			if(MAC2Status[mac] == USR_FREE) {
				mac_id = MAC2ID[mac];
			}
			else {
				//fprintf(stderr, "\t-- MAC %s is already busy\n", mac);fflush(stderr);
				log(client_id, RECV, 3, "MAC %s is already busy\n", mac);
				//printf("MAC %s is already busy\n", mac);
				return 1;
			}
		}
		//fprintf(stderr, "\t-- MAC = %s, MAC ID = %d\n", mac, mac_id);fflush(stderr);

		int file_id = selectFile(mac_id);
		//fprintf(stderr, "\t-- MAC = %s, MAC ID = %d, FILE ID = %d\n", mac, mac_id, file_id);fflush(stderr);		
		if(file_id != -1)
		{
			Command *new_cmd = new Command();
			//file_id = Think();
			new_cmd->type = 1;
			sprintf(new_cmd->str, "send %s\t%d", mac, file_id);	
			pthread_mutex_lock(&queue_lock[client_id]);
			//printf("\nclient id: %d\n", client_id);
			Clients[client_id]->cmd_q.push(new_cmd);
			//printf("****** Sending Sem_Post as a send command ******\n"); fflush(stdout);
			sem_post(&queue_sem[client_id]);
			pthread_mutex_unlock(&queue_lock[client_id]);
		}
		else
		{
			//fprintf(stderr, "\tNO FILE TO SEND:\t%s\n", mac);
			log(client_id, RECV, 3, "NO FILE TO SEND:\t%s\n", mac);
			releaseCounter--;
			//fprintf(stderr, "-releaseCounter: %d\n", releaseCounter);
			sendReleaseCMD(mac);
		}
		return 1;
	}
	else if(!strncmp(buffer, "channel", 7))
	{
		char mac[256];
		int channel;
		sscanf(buffer+8, "%s\t%d\n", mac, &channel);
		log(client_id, RECV, 2, "parse <CHANNEL> message:\t%s\t%d\n", mac, channel);
		//printf("parse <CHANNEL> message:\t%s\t%d\n", mac, channel);
		int mac_id = MAC2ID[mac];
		updateChannel(mac_id, channel);
		//Update DB ..	
		return 2;
	}
	else if(!strncmp(buffer, "name", 4)) {
		char mac[256];
		char mac_name[256];
		sscanf(buffer, "name %s\t%[^\n]\n", mac, mac_name);
		//fprintf(stderr, "\tNAME message:\t%s\t%s\n", mac, mac_name);
		log(client_id, RECV, 2, "parse <NAME> message:\t%s\t%s\n", mac, mac_name);
		int mac_id = MAC2ID[mac];
		updateMac(mac_id, mac_name, client_id);
		//Update DB ..	
		return 2;
	}
	else if(!strncmp(buffer, "res", 3))
	{
		char mac[256];
		int file_id, hci_num, send_res;
		time_t send_time;
		//printf("\n%s", buffer);
		sscanf(buffer, "res %s\t%d\t%d\t%d\t%ld", mac, &file_id, &hci_num, &send_res, &send_time);
		//fprintf(stderr, "\tRES:\t%s\t%d\t%d\n", mac, file_id, send_res);
		log(client_id, RECV, 2, "parse <RES> message:\t%s\t%d\t%d\n", mac, file_id, send_res);
		int mac_id = MAC2ID[mac];
		updateSend(mac_id, file_id, send_time, send_res, client_id);

		releaseCounter--;
		//fprintf(stderr, "-releaseCounter: %d\n", releaseCounter);
		sendReleaseCMD(mac);
		
		//printf("res %s %d %d %d\n",  mac, file_id, hci_num, send_res);
		return 3;
	}

	return -1;	
//	pthread_exit(NULL);	
}

int Server::updateMaps(int client_id)
{
	map<string, vector<int> >::iterator itr;
	//printf("update maps!\n");
	for(itr = MAC2SL.begin(); itr!=MAC2SL.end(); itr++)
	{
		//printf("mac: %s!\n", itr->first.c_str());
		vector<int> tv = itr->second;
		vector<int>::iterator vitr;
		for(vitr = tv.begin(); vitr!=tv.end(); vitr++)
		{
			if(*vitr == client_id)
			{
				tv.erase(vitr);
				break;
			}
		}
		if(tv.size() == 0)
		{
			//printf("tv size is 0!\n");
			MAC2Status[itr->first] = USR_FREE;
		}
		MAC2SL[itr->first] = tv;
	}
	return 0;
}

void *Server::console(void *args) {
	char cmd[1024];
	int id;

	while(!done) {
		sleep(1);
		printf("\n\t- Enter command: ");
		//scanf("%[^\n]", cmd);
		id = -1; strcpy(cmd, "");
		fgets(cmd, 1020, stdin);
		//scanf("%[^\n]\n",cmd); 
		//printf("\t\t GOT %s from console\n", cmd);
		if(!strncmp(cmd, "quit", 4)) {
			done = 1;
			for(int i = 0; i < MAX_CLIENTS; i++) {
				if(Clients[i] != NULL) {
					sem_post(&queue_sem[i]);
				}				
			}
			break;
		}	
		
		sscanf(cmd, "%d%*c %1000[^\n]", &id, cmd);
		if(MAX_CLIENTS <= id || id == -1) {
			printf("\t\t* Client id %d is out of range %d.", id, MAX_CLIENTS-1);
			continue;	
		} 
		else if(Clients[id] == NULL) {
			printf("\t\t* Client id %d is not connected.", id);
			continue;	
		} 
		else {	
			if(!strcmp(cmd, "stop") || !strcmp(cmd, "get options"))
			{
				updateMaps(id);
			}
			Command *new_cmd = new Command();
			//new_cmd->str = new char[1024];
			new_cmd->type = 3;
			strcpy(new_cmd->str, cmd);

			pthread_mutex_lock(&queue_lock[id]);
			Clients[id]->cmd_q.push(new_cmd);
			sem_post(&queue_sem[id]);
			pthread_mutex_unlock(&queue_lock[id]);
			
//			printf("\n\t- Client %d Got command %s\n", id, Clients[id]->cmd_q.front()->str);
		}
	//	printf("Cmd = %s\n", cmd);
	}

	printf("\n\t- Terminating console ...");
	pthread_exit(NULL);	
}

Server::~Server()
{
	mysql_close(conn);
	printf("\n\t- Closing server socket ... "); fflush(stdout);
	close(server_socket);
	printf("\nDone.\n");
}
