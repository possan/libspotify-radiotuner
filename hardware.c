#include <stdlib.h>
#include <stdio.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <pthread.h>

#include "hardware.h"

// #volatile int STOP=FALSE;
// #unsigned char buf[255];
// int res;
// int myCount=0;
// int maxCount=10000;	
int fd = -1;

void openport(char *name) {
	if (name==NULL)
		return;
	printf("HARDWARE: Trying to open port \"%s\"...\n", name);
	struct termios options;
	fd = open(name, O_RDWR | O_NOCTTY | O_NDELAY); // List usbSerial devices using Terminal ls /dev/tty.*
 	if(fd == -1) {
 		printf("HARDWARE: Failed.\n");
 		return;
 	}
	tcgetattr(fd,&options);
	cfsetispeed(&options,B9600);
	cfsetospeed(&options,B9600);
	options.c_cflag |= (CLOCAL | CREAD);
	options.c_cflag &= ~PARENB;
	options.c_cflag &= ~CSTOPB;
	options.c_cflag &= ~CSIZE;
	options.c_cflag |= CS8;
	tcsetattr(fd,TCSANOW,&options);
 	printf("HARDWARE: Connected.\n");
}

static void hardware_tick(void *arg) {
	printf("HARDWARE: thread tick\n");
	char buf[100];
	while(1) {
		usleep(10000);
		if(fd==-1)
			continue;
		int res = read(fd, &buf, 1);
		if(res) {
			printf("HARDWARE: got '%c'\n", buf[0]);
		}
	}
}


void hardware_start(char *portname) {
	printf("HARDWARE: Start..\n");
	// find probable serial port

	openport(portname);

	static pthread_t id;
	pthread_create(&id, NULL, hardware_tick, NULL);

}

void hardware_stop() {
	printf("HARDWARE: Stop..\n");
	// close serial port
}

void hardware_banner(char *message, int ttl_ms) {
	printf("HARDWARE: Show banner \"%s\" for %d ms.\n", message, ttl_ms);
	if (fd!=-1) {
		write(fd, "MSG:", 1);			// chr(10) start comms
		write(fd, message, strlen(message));			// 0 = off 1 = on 2 = ask LED state
		write(fd, "\n", 1);
	}
}

void hardware_setleds() {
}

HARDWARE_EVENT_CALLBACK *g_event_callback;

void hardware_set_callback(HARDWARE_EVENT_CALLBACK *callback) {
	g_event_callback = callback;
}

void hardware_fire_event(int event) {
	// printf("HARDWARE: Got event #%d\n", event);
	if (g_event_callback != NULL) g_event_callback(event);
}

