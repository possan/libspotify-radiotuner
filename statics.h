#ifndef _STATICS_H_
#define _STATICS_H_

#include <libspotify/api.h>
#include "audio.h"

typedef unsigned long STATICSTATE;

extern STATICSTATE static_init(int color);
extern void static_setvolume(STATICSTATE statics, float volume);
extern float static_getvolume(STATICSTATE statics);
extern void static_generate(STATICSTATE statics, const sp_audioformat *format, audio_fifo_t *inputfifo, audio_fifo_t *outputfifo);
extern void static_free(STATICSTATE statics);
extern double pnoise(double x, double y, double z);

#endif