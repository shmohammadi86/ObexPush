#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include <openobex/obex.h>

#include "obex_macros.h"
#include "obex_main.h"
#include "obex_socket.h"

#define BT_SERVICE "OBEX"
#define OBEX_PUSH        5


/*
 * prototypes for local functions
 */

obex_t *__obex_connect(int devid, void *addr, int timeout, int *err);
void obex_event(obex_t * handle, obex_object_t * object, int mode, int event,
		int obex_cmd, int obex_rsp);
void obex_free(client_context_t *gt);
int obex_disconnect(obex_t * handle);
uint8_t *easy_readfile(const char *filename, int *file_size);
int get_filesize(const char *filename);
void request_done(obex_t * handle, obex_object_t * object, int obex_cmd, int obex_rsp);



int bt_debug;


extern FILE *obex_outfile;

/*
 * These two functions are from affix/profiles/obex/obex_io.c
 */
int get_filesize(const char *filename)
{
	struct stat stats;

	stat(filename, &stats);
	return (int) stats.st_size;
}

uint8_t *easy_readfile(const char *filename, int *file_size)
{
	int actual;
	int fd;
	uint8_t *buf;

	fd = open(filename, O_RDONLY, 0);
	if (fd == -1) {
		return NULL;
	}
	*file_size = get_filesize(filename);
	log(obex_outfile, 2, "name=%s, size=%d\n", filename, *file_size);
	if (!(buf = (uint8_t*)malloc(*file_size))) {
		return NULL;
	}

	actual = read(fd, buf, *file_size);
	close(fd);

	*file_size = actual;
	return buf;
}

/*
 * This function comes from affix/profiles/obex/obex_client.c .. All I changed
 * was a BTERROR() macro.  The OBEX_HandleInput() calls inside the loop should
 * result in request_done() getting called eventually.  request_done() sets
 * the clientdone flag which releases the function from its loop
 */
int handle_response(obex_t * handle, char *service)
{
	int err = 0;
	client_context_t *gt = (client_context_t*)OBEX_GetUserData(handle);

	gt->clientdone = 0;
	while (!gt->clientdone) {
		if ((err = OBEX_HandleInput(handle, 1)) < 0) {
			log(obex_outfile,1,"ERROR: Error while doing OBEX_HandleInput()\n");
			break;
		}
		err = (gt->rsp == EOBEX_OK) ? 0 : gt->rsp;
	}

	return err;
}

void obex_free(client_context_t *gt)
{

	cobex_close((cobex_context*)gt->cntx_private);
	free(gt);
}

/*
 * This function comes from affix/profiles/obex/obex_client.c ... I just
 * trimmed out handling of other transport styles.. that stuff was using some
 * globals and I didn't want to pull them in.
 */
int obex_disconnect(obex_t * handle)
{
	int err;
	obex_object_t *oo;
	client_context_t *gt = (client_context_t*)OBEX_GetUserData(handle);

	oo = OBEX_ObjectNew(handle, OBEX_CMD_DISCONNECT);
	err = OBEX_Request(handle, oo);
	if (err)
		return err;
	handle_response(handle, BT_SERVICE);
	obex_free(gt);

	return 0;
}

/*
 * This function came from the affix/profiles/obex/obex_client.c file.
 * Initially it did some checks to see what type of link it was working over,
 * supporting a few different transports it looked like.  But the logic around
 * choosing what to do relied on a couple of global variables, so I just pulled
 * the decision paths I didn't want and left the ones I did.
 */
obex_t *__obex_connect(int devid, void *addr, int timeout, int *err)
{
	obex_t *handle;
	obex_object_t *oo;
	client_context_t *gt;
	obex_ctrans_t custfunc;

	gt = (client_context_t*)malloc(sizeof(client_context_t));
	if (gt == NULL)
		return NULL;
	//log(obex_outfile, 1, "__obex_connect: client_context_t = %p\n", gt);
	memset(gt, 0, sizeof(client_context_t));
	gt->cntx_private = cobex_open(devid, (const char*)addr, timeout);
	if (gt->cntx_private == NULL) {
		log(obex_outfile,1,"ERROR: cobex_open() failed\n");
		free(gt);
		*err = -1;
		return NULL;
	}
	if (!(handle = OBEX_Init(OBEX_TRANS_CUST, obex_event, 0))) {
		log(obex_outfile,1,"ERROR: OBEX_Init failed: %s\n", strerror(errno));
		obex_free(gt);
		*err = -1;
		return NULL;
	}

	memset(&custfunc, 0, sizeof(custfunc));
	custfunc.customdata = gt->cntx_private;
	custfunc.connect = cobex_connect;
	custfunc.disconnect = cobex_disconnect;
	custfunc.write = cobex_write;
	custfunc.handleinput = cobex_handle_input;
	custfunc.listen = cobex_listen;
	if (OBEX_RegisterCTransport(handle, &custfunc) < 0) {
		log(obex_outfile,1,"ERROR: Custom transport callback-registration failed\n");
		obex_free(gt);
		*err = -1;
		return NULL;
	}
	//log(obex_outfile, 2, "Registered transport\n");
	OBEX_SetUserData(handle, gt);
	//log(obex_outfile, 2, "Set user data\n");

	/* create new object */
	oo = OBEX_ObjectNew(handle, OBEX_CMD_CONNECT);
	//log(obex_outfile, 2, "Created new objext\n");
	*err = OBEX_Request(handle, oo);
	//log(obex_outfile, 2, "Started a new request\n");
	if (*err || gt->rsp) {
		*err = *err ? *err: gt->rsp;
		obex_free(gt);
		goto exit;
	}
	*err = handle_response(handle, BT_SERVICE);
	//log(obex_outfile, 2, "Connection return code: %d, id: %d\n", *err, gt->con_id);
	if (*err || gt->rsp) {
		*err = *err ? *err: gt->rsp;
		goto exit_conn;
	}
	log(obex_outfile, 2, "Connection established\n");
	return handle;
	exit_conn:
	obex_disconnect(handle);
	exit:
	log(obex_outfile,1,"ERROR: __obex_connect: error=%d\n", *err);
	return NULL;
}

/*
 * These next two functions come from affix/profiles/obex/obex_client.c
 * All they do are set a few flags in the structs here or disconnect on error.
 * The obex_event() function is called by the obex library when it has an event
 * to deliver to us.. as simple as this is this means just setting that
 * clientdone flag in the user data struct.  Yea... really, that's it.
 */
void request_done(obex_t * handle, obex_object_t * object, int obex_cmd, int obex_rsp)
{
	client_context_t *gt = (client_context_t*)OBEX_GetUserData(handle);

	log(obex_outfile, 2, "Command (%02x) has now finished, rsp: %02x\n", obex_cmd, obex_rsp);

	switch (obex_cmd) {
	case OBEX_CMD_DISCONNECT:
		log(obex_outfile, 2, "Disconnect done!\n");
		OBEX_TransportDisconnect(handle);
		gt->clientdone = 1;
		break;
	case OBEX_CMD_CONNECT:
		log(obex_outfile, 2, "Connected!\n");
		/* connect_client(handle, object, obex_rsp); */
		gt->clientdone = 1;
		break;
	case OBEX_CMD_GET:
		//log(obex_outfile, 2, "*** Warning, getclient commented out\n");
		/* get_client(handle, object, obex_rsp); */
		gt->clientdone = 1;
		break;
	case OBEX_CMD_PUT:
		/* put_client(handle, object, obex_rsp); */
		gt->clientdone = 1;
		break;
	case OBEX_CMD_SETPATH:
		//log(obex_outfile, 2, "*** Warning, setpath_cleitn commented out\n");
		/* setpath_client(handle, object, obex_rsp); */
		gt->clientdone = 1;
		break;
	default:
		log(obex_outfile, 2, "Command (%02x) has now finished\n", obex_cmd);
		break;
	}
}

void obex_event(obex_t * handle, obex_object_t * object, int mode, int event,
		int obex_cmd, int obex_rsp)
{
	client_context_t *gt;

	gt = (client_context_t*)OBEX_GetUserData(handle);
	//log(obex_outfile, 2, "obex_event: client_context_t = %p\n", gt);
	switch (event) {
	case OBEX_EV_PROGRESS:
		//log(obex_outfile, 2, "Made some progress...\n");
		break;
	case OBEX_EV_ABORT:
		log(obex_outfile, 2, "Request aborted!\n");
		gt->rsp = -EOBEX_ABORT;
		break;
	case OBEX_EV_REQDONE:
		log(obex_outfile, 2, "ReqDone\n");
		if(obex_rsp == 0x7f)
		{
			log(obex_outfile, 2, "Link broken!\n");
			OBEX_TransportDisconnect(handle);
			gt->clientdone = 1;
			gt->rsp = -EOBEX_HUP;
		}
		else
			request_done(handle, object, obex_cmd, obex_rsp);
		/* server_done(handle, object, obex_cmd, obex_rsp); */
		break;
	case OBEX_EV_REQHINT:
		/* Accept any command. Not rellay good, but this is a test-program :
		 * ) */
		/* OBEX_ObjectSetRsp(object, OBEX_RSP_CONTINUE, OBEX_RSP_SUCCESS); */
		log(obex_outfile, 2, "SetResp\n");
		break;
	case OBEX_EV_REQ:
		/* server_request(handle, object, event, obex_cmd); */
		log(obex_outfile, 2, "Server request\n");
		break;
	case OBEX_EV_LINKERR:
		OBEX_TransportDisconnect(handle);
		log(obex_outfile, 2, "Link broken!\n");
		gt->rsp = -EOBEX_HUP;
		break;
	case OBEX_EV_PARSEERR:
		log(obex_outfile, 2, "Parse error!\n");
		gt->rsp = -EOBEX_PARSE;
		break;
	default:
		log(obex_outfile, 2, "Unknown event %02x!\n", event);
		break;
	}
}

/*
 * This function also comes directly from affix/profiles/obex/obex_client.c
 * It calls __obex_connect() to connect to the device, and then uses the
 * OpenOBEX call interface to form a PUT request to send the file to the
 * device. Originally this used the basename() of path as the name to put as,
 * but I added a remote parameter so that I didn't have to rename files before
 * moving them over.  Essentially nothing is different.
 */
int obex_push(int devid, char *addr, char *path, char *remote, int timeout)
{
	int err;
	obex_object_t *oo;
	obex_headerdata_t hv;
	obex_t *handle;
	uint8_t *buf;
	int file_size;
	char *namebuf;
	int name_len;
	char *name, *pathc;
	client_context_t *gt;

	pathc = strdup(path);
	name = remote;

	name_len = (strlen(name) + 1) << 1;
	if ((namebuf = (char*)malloc(name_len))) {
		OBEX_CharToUnicode((uint8_t *) namebuf, (uint8_t *) name, name_len);
	}
	buf = easy_readfile(path, &file_size);
	if (buf == NULL) {
		log(obex_outfile,1,"ERROR: Can't find file %s\n", name);
		return -ENOENT;
	}

	handle = __obex_connect(devid, (void *)addr, timeout, &err);
	if (handle == NULL) {
		log(obex_outfile,1,"ERROR: Unable to connect to the server\n");
		free(buf);
		return err;
	}
	log(obex_outfile, 2, "Connected to server\n");
	gt = (client_context_t*)OBEX_GetUserData(handle);
	//log(obex_outfile, 2, "obex_push: client_context_t = %p\n", gt);
	gt->opcode = OBEX_PUSH;

	log(obex_outfile, 3, "Sending file: %s, path: %s, size: %d\n", name, path, file_size);
	oo = OBEX_ObjectNew(handle, OBEX_CMD_PUT);
	hv.bs = (uint8_t *) namebuf;
	OBEX_ObjectAddHeader(handle, oo, OBEX_HDR_NAME, hv, name_len, 0);
	hv.bq4 = file_size;
	OBEX_ObjectAddHeader(handle, oo, OBEX_HDR_LENGTH, hv, sizeof(uint32_t), 0);
	hv.bs = buf;
	OBEX_ObjectAddHeader(handle, oo, OBEX_HDR_BODY, hv, file_size, 0);

	err = OBEX_Request(handle, oo);
	if (err)
		return err;
	err = handle_response(handle, BT_SERVICE);

	obex_disconnect(handle);
	free(buf);
	free(pathc);

	return err;
}


