
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>


typedef struct {
  bdaddr_t addr; 
} device;


#define MAX_DEVICES 1024



int main(int argc, char **argv) {

  inquiry_info *ii = NULL;

  int max_rsp = 255; 
  int num_rsp;
  int dev_id, sock;
  int flags = IREQ_CACHE_FLUSH;
  int i, j, k;
  char addr[19] = {0};
  char name[248] = {0};

  device devices[MAX_DEVICES];
  int devices_found; 
  
  dev_id = hci_get_route(NULL);
  sock = hci_open_dev(dev_id);

  if (dev_id < 0 || sock < 0) {
    printf("Error opening socket or opening bluetooth device\n");
    return -1; 
  }

  printf("device_id: %d\nsocket_id: %d\n", dev_id, sock);

  struct hci_dev_info di = { .dev_id = dev_id };

  if (ioctl(sock, HCIGETDEVINFO, (void *) &di))
    return 0;

  ba2str(&di.bdaddr, addr);
  printf("Bluetooth device: %s\t%s\n", di.name, addr); 

  ii = (inquiry_info*)malloc(max_rsp * sizeof(inquiry_info));
  

  while (true) { 
    num_rsp = hci_inquiry(dev_id, 2, max_rsp, NULL, &ii, flags);

    if (num_rsp < 0 ) {
      printf("Error performing hci inquiry\n");
      return -1;
    }

    for (i = 0; i < num_rsp; i ++) {
      bool new_device = true; 
      for (j = 0; j < devices_found; j ++) {
	for (k = 0; k < 6; k ++) {
	  bool all_equal = true; 
	  if (ii[i].bdaddr.b[k] != devices[j].addr.b[k]) {
	    all_equal = false;
	  }
 
	  if (all_equal) new_device = false;
	}
      }
      if (new_device && devices_found < MAX_DEVICES) {
	devices[devices_found].addr = ii[i].bdaddr;
	devices_found++;
	ba2str(&ii[i].bdaddr, addr);
	memset(name, 0, sizeof(name));
	if (hci_read_remote_name(sock, &ii[i].bdaddr, sizeof(name), name, 0) < 0)
	  strcpy(name, "[unknown]");
	printf("%s %s\n", addr, name);
      }	
    }
  }
  
  free(ii);
  close(sock);
  return 0; 
}
