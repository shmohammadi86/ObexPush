#include <utils.h>



/********************************************************************
***					        EnumerateHCI
***	            Enumerates and initializes all HCI devices
********************************************************************/
vector<HCI *> EnumerateHCI(const char*dev_name) {
	int ctl;

	struct hci_dev_list_req *dl = NULL;
	struct hci_dev_req *dr = NULL;
	struct hci_dev_info di;
	vector<HCI *> HCI_List;
	
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

	printf("\t\t\t+ Enumerating HCI devices ... \n");fflush(stdout);
	for (register int i = 0; i < dl->dev_num; i++) {
		
		di.dev_id = (dr+i)->dev_id;
		if (ioctl(ctl, HCIGETDEVINFO, (void *) &di) < 0) // Can't get device information ...
			continue;

		if (hci_test_bit(HCI_UP, &di.flags)) {
			new_hci = (HCI *)malloc(sizeof(HCI));
			
			printf("\t\t\t\t- Initializing HCI%d ... \n", di.dev_id);fflush(stdout);
			//Black magic to re-birth the HCI!
			
			printf("\t\t\t\t\t- Disabling scan on HCI%d ... ", di.dev_id);fflush(stdout);
			(dr+i)->dev_opt = SCAN_DISABLED; //Disable scanning ...
			if (ioctl(ctl, HCISETSCAN, (dr+i)) < 0) {
				fprintf(stderr, "\t\t\t\t\t- Can't set hci%d to noscan: %s (%d)\n", 
						di.dev_id, strerror(errno), errno);
				fflush(stderr);
			}
			printf("Done\n");
		
			printf("\t\t\t\t\t- Setting name to %s on HCI%d ... ", dev_name, di.dev_id);fflush(stdout);
			int dd = hci_open_dev(di.dev_id); //Set name to dev_name ...
			if (hci_write_local_name(dd, dev_name, 2000) < 0) {
				fprintf(stderr, "\t\t\t\t\t- Can't change local name on hci%d to %s: %s (%d)\n", 
						di.dev_id, dev_name, strerror(errno), errno);
				fflush(stderr);
			}
			hci_close_dev(dd);
			printf("Done\n");

			printf("\t\t\t\t\t- Turning HCI%d down ... ", di.dev_id);fflush(stdout);
			if (ioctl(ctl, HCIDEVDOWN, di.dev_id) < 0 && errno == EALREADY) {
				fprintf(stderr, "Can't turn down device hci%d: %s (%d)\n", di.dev_id, strerror(errno), errno);
			}	
			printf("Done\n");
		
			printf("\t\t\t\t\t- Turning HCI%d up ... ", di.dev_id);fflush(stdout);
			if (ioctl(ctl, HCIDEVUP, di.dev_id) < 0 && errno == EALREADY) {
				fprintf(stderr, "Can't turn up hci%d: %s (%d)\n", di.dev_id, strerror(errno), errno);
			}
			printf("Done\n"); 

			
			sleep(1); 

			new_hci->id = di.dev_id;
			bacpy(&(new_hci->bdaddr), &(di.bdaddr));
			ba2str(&(di.bdaddr), new_hci->mac);
			HCI_List.push_back(new_hci);
		}
	}
	
	shutdown(ctl, 2);
	close(ctl);

	return HCI_List;
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

