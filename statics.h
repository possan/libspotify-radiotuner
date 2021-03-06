#ifndef _STATICS_H_
#define _STATICS_H_

#include <libspotify/api.h>
#include "audio.h"

typedef unsigned long STATICSTATE;

#define WHITE_NOISE 1
#define PINK_NOISE 13
#define BROWN_NOISE 30

extern STATICSTATE static_init(int color, char *filename);
extern void static_setvolume(STATICSTATE statics, float volume);
extern float static_getvolume(STATICSTATE statics);
extern void static_generate(STATICSTATE statics, audio_fifo_t *inputfifo, audio_fifo_t *outputfifo, bool comped);
extern void static_free(STATICSTATE statics);
extern double pnoise(double x, double y, double z);

#endif
