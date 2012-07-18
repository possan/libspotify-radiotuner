#include <stdlib.h>
#include <stdio.h>
#include "hardware.h"

void hardware_start() {
	printf("HARDWARE: Start..\n");
}

void hardware_stop() {
	printf("HARDWARE: Stop..\n");
}

void hardware_banner(char *message, int ttl_ms) {
	printf("HARDWARE: Show banner \"%s\" for %d ms.\n", message, ttl_ms);
}

void hardware_setleds() {
}

void hardware_tick() {
}

HARDWARE_EVENT_CALLBACK *g_event_callback;

void hardware_set_callback(HARDWARE_EVENT_CALLBACK *callback) {
	g_event_callback = callback;
}

void hardware_fire_event(int event) {
	// printf("HARDWARE: Got event #%d\n", event);
	if (g_event_callback != NULL) g_event_callback(event);
}

