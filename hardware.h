#ifndef _HARDWARE_H_
#define _HARDWARE_H_

#define HE_VOL_DOWN 10
#define HE_VOL_UP 11
#define HE_X_DOWN 12
#define HE_X_UP 13
#define HE_FREQ_DOWN 14
#define HE_FREQ_UP 15
#define HE_SKIP_PREV 16
#define HE_SKIP_NEXT 17
#define HE_PLAY_PAUSE 20
#define HE_CHANNEL1 30
#define HE_CHANNEL2 31
#define HE_CHANNEL3 32
#define HE_CHANNEL4 33
#define HE_CHANNEL5 34

typedef void *HARDWARE_EVENT_CALLBACK(int event);

extern void hardware_start(char *portname);
extern void hardware_stop();
extern void hardware_banner(char *message, int ttl_ms);
extern void hardware_setleds();
extern void hardware_set_callback(HARDWARE_EVENT_CALLBACK *callback);
extern void hardware_fire_event(int event);

#endif