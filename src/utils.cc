#include <utils.h>


static void set_name(int hci_no, const char *dev_name, FILE *sinterface_outfile) {
	log(sinterface_outfile, 3, "Setting name to %s on HCI%d \n", dev_name, hci_no);

	int dd;
	dd = hci_open_dev(hci_no);
	if (dd < 0) {
		fprintf(stderr, "Can't open device hci%d: %s (%d)\n",
						hci_no, strerror(errno), errno);
		exit(1);
	}

	if (hci_write_local_name(dd, dev_name, 2000) < 0) {
		fprintf(stderr, "Can't change local name on hci%d: %s (%d)\n",
					hci_no, strerror(errno), errno);
		exit(1);
	}

	hci_close_dev(dd);
}

static void noscan(int ctl, int hci_no, FILE *sinterface_outfile) {
	log(sinterface_outfile, 3, "Setting noscan option on HCI%d\n", hci_no);
	
	struct hci_dev_req dr;
	dr.dev_id  = hci_no;
	dr.dev_opt = SCAN_DISABLED;

	if (ioctl(ctl, HCISETSCAN, (unsigned long) &dr) < 0) {
		fprintf(stderr, "Can't set scan mode on hci%d: %s (%d)\n",
						hci_no, strerror(errno), errno);
		exit(1);
	}
}



static void reset(int ctl, int hci_no, FILE *sinterface_outfile) {
	log(sinterface_outfile, 3, "Turning HCI%d down\n", hci_no);
	if (ioctl(ctl, HCIDEVDOWN, hci_no) < 0 && errno == EALREADY) {
		fprintf(stderr, "Can't turn down device hci%d: %s (%d)\n", hci_no, strerror(errno), errno);
	}	
	log(sinterface_outfile, 3, "Turning HCI%d up\n", hci_no);
	if (ioctl(ctl, HCIDEVUP, hci_no) < 0 && errno == EALREADY) {
		fprintf(stderr, "Can't turn up hci%d: %s (%d)\n", hci_no, strerror(errno), errno);
	}
}



int init(int hci_no, const char *dev_name, FILE *sinterface_outfile) {
	log(sinterface_outfile, 2, "Initializing HCI%d \n", hci_no);

	int ctl;
	if ((ctl = socket(AF_BLUETOOTH, SOCK_RAW, BTPROTO_HCI)) < 0) {
		perror("Can't open HCI socket.");
		exit(1);
	}

	set_name(hci_no, dev_name, sinterface_outfile);
	noscan(ctl, hci_no, sinterface_outfile);
	reset(ctl, hci_no, sinterface_outfile);

	shutdown(ctl, 2);
	close(ctl);
	
	return 0;
}


/********************************************************************
***					        EnumerateHCI
***	            Enumerates and initializes all HCI devices
********************************************************************/
map<int, HCI *> EnumerateHCI(const char*dev_name, FILE *sinterface_outfile) {
	int ctl;

	struct hci_dev_list_req *dl = NULL;
	struct hci_dev_req *dr = NULL;
	struct hci_dev_info di;
	map<int, HCI *> HCI_Map;
	
	if ((ctl = socket(AF_BLUETOOTH, SOCK_RAW, BTPROTO_HCI)) < 0) {
		perror("Can't open HCI socket.");
		exit(1);
	}

	if (!(dl = (struct hci_dev_list_req *)malloc(512 * sizeof(struct hci_dev_req) + sizeof(uint16_t)))) {
		perror("Can't allocate memory");
		exit(1);
	}
	memset(dl, 0, 512 * sizeof(struct hci_dev_req) + sizeof(uint16_t));	
	dl->dev_num = 512;
	dr = dl->dev_req;


	if (ioctl(ctl, HCIGETDEVLIST, (void *) dl) < 0) {
		perror("Can't get device list");
		exit(1);
	}


	HCI *new_hci;

	printf("\t\t\t+ Enumerating HCI devices ... \n");
	for (register int i = 0; i < dl->dev_num; i++) {
		
		di.dev_id = (dr+i)->dev_id;
		if (ioctl(ctl, HCIGETDEVINFO, (void *) &di) < 0) // Can't get device information ...
			continue;

		if (hci_test_bit(HCI_UP, &di.flags)) {
			new_hci = (HCI *)malloc(sizeof(HCI));
			
			init(di.dev_id, dev_name, sinterface_outfile);
			
			new_hci->id = di.dev_id;
			bacpy(&(new_hci->bdaddr), &(di.bdaddr));
			ba2str(&(di.bdaddr), new_hci->mac);
			HCI_Map[di.dev_id] = new_hci;
		}
	}
	
	shutdown(ctl, 2);
	close(ctl);

	if(dl) free(dl);
	return HCI_Map;
}



/********************************************************************
***					  get_minor_device_name
***	   finds the bluetooth device type from its minor/major numbers
********************************************************************/
char *get_minor_device_name(int major, int minor)
{
	switch (major) {
		case 0:	/* misc */
			return "";
		case 1:	/* computer */
			switch(minor) {
				case 0:
					return "Uncategorized";
				case 1:
					return "Desktop workstation";
				case 2:
					return "Server";
				case 3:
					return "Laptop";
				case 4:
					return "Handheld";
				case 5:
					return "Palm";
				case 6:
					return "Wearable";
			}
			break;
		case 2:	/* phone */
			switch(minor) {
				case 0:
					return "Uncategorized";
				case 1:
					return "Cellular";
				case 2:
					return "Cordless";
				case 3:
					return "Smart phone";
				case 4:
					return "Wired modem or voice gateway";
				case 5:
					return "Common ISDN Access";
				case 6:
					return "Sim Card Reader";
			}
			break;
		case 3:	/* lan access */
			if (minor == 0)
				return "Uncategorized";
			switch(minor / 8) {
				case 0:
					return "Fully available";
				case 1:
					return "1-17% utilized";
				case 2:
					return "17-33% utilized";
				case 3:
					return "33-50% utilized";
				case 4:
					return "50-67% utilized";
				case 5:
					return "67-83% utilized";
				case 6:
					return "83-99% utilized";
				case 7:
					return "No service available";
			}
			break;
		case 4:	/* audio/video */
			switch(minor) {
				case 0:
					return "Uncategorized";
				case 1:
					return "Device conforms to the Headset profile";
				case 2:
					return "Hands-free";
					/* 3 is reserved */
				case 4:
					return "Microphone";
				case 5:
					return "Loudspeaker";
				case 6:
					return "Headphones";
				case 7:
					return "Portable Audio";
				case 8:
					return "Car Audio";
				case 9:
					return "Set-top box";
				case 10:
					return "HiFi Audio Device";
				case 11:
					return "VCR";
				case 12:
					return "Video Camera";
				case 13:
					return "Camcorder";
				case 14:
					return "Video Monitor";
				case 15:
					return "Video Display and Loudspeaker";
				case 16:
					return "Video Conferencing";
					/* 17 is reserved */
				case 18:
					return "Gaming/Toy";
			}
			break;
		case 5:	/* peripheral */ {
						 static char cls_str[48]; cls_str[0] = 0;

						 switch(minor & 48) {
							 case 16:
								 strncpy(cls_str, "Keyboard", sizeof(cls_str));
								 break;
							 case 32:
								 strncpy(cls_str, "Pointing device", sizeof(cls_str));
								 break;
							 case 48:
								 strncpy(cls_str, "Combo keyboard/pointing device", sizeof(cls_str));
								 break;
						 }
						 if((minor & 15) && (strlen(cls_str) > 0))
							 strcat(cls_str, "/");

						 switch(minor & 15) {
							 case 0:
								 break;
							 case 1:
								 strncat(cls_str, "Joystick", sizeof(cls_str) - strlen(cls_str));
								 break;
							 case 2:
								 strncat(cls_str, "Gamepad", sizeof(cls_str) - strlen(cls_str));
								 break;
							 case 3:
								 strncat(cls_str, "Remote control", sizeof(cls_str) - strlen(cls_str));
								 break;
							 case 4:
								 strncat(cls_str, "Sensing device", sizeof(cls_str) - strlen(cls_str));
								 break;
							 case 5:
								 strncat(cls_str, "Digitizer tablet", sizeof(cls_str) - strlen(cls_str));
								 break;
							 case 6:
								 strncat(cls_str, "Card reader", sizeof(cls_str) - strlen(cls_str));
								 break;
							 default:
								 strncat(cls_str, "(reserved)", sizeof(cls_str) - strlen(cls_str));
								 break;
						 }
						 if(strlen(cls_str) > 0)
							 return cls_str;
					 }
		case 6:	/* imaging */
					 if (minor & 4)
						 return "Display";
					 if (minor & 8)
						 return "Camera";
					 if (minor & 16)
						 return "Scanner";
					 if (minor & 32)
						 return "Printer";
					 break;
		case 7: /* wearable */
					 switch(minor) {
						 case 1:
							 return "Wrist Watch";
						 case 2:
							 return "Pager";
						 case 3:
							 return "Jacket";
						 case 4:
							 return "Helmet";
						 case 5:
							 return "Glasses";
					 }
					 break;
		case 8: /* toy */
					 switch(minor) {
						 case 1:
							 return "Robot";
						 case 2:
							 return "Vehicle";
						 case 3:
							 return "Doll / Action Figure";
						 case 4:
							 return "Controller";
						 case 5:
							 return "Game";
					 }
					 break;
		case 63:	/* uncategorised */
					 return "";
	}
	return "Unknown (reserved) minor device class";
}





/********************************************************************
***					        findOPUSH
***	   finds OPUSH channel of bluetooth device "mac" using HCI
********************************************************************/
int findOPUSH(const HCI *src_hci, const char* mac, FILE *SDP_outfile) {
	int channel = SDP_NO_CHANNEL;	

	bdaddr_t target;
	str2ba (mac, &target);
	bdaddr_t interface = src_hci->bdaddr;


    sdp_session_t *session = sdp_connect( &interface, &target, SDP_RETRY_IF_BUSY );
	if (!session) {
		fprintf(SDP_outfile, "Failed to connect to SDP server on %s: %s\n", mac, strerror(errno)); fflush(SDP_outfile);
		return SDP_CONNECTION_ERROR;
	}

	uuid_t svc_uuid;
	uint16_t class16 = 0x1105;
	uint32_t range = 0x0000ffff;
	sdp_uuid16_create (&svc_uuid, class16);

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


	sdp_list_free(attrid_list, 0); 
	sdp_list_free(search_list, 0);


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

