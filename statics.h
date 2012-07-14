#ifndef _STATICS_H_
#define _STATICS_H_

#include <libspotify/api.h>
#include "audio.h"

typedef unsigned long STATICSTATE;

STATICSTATE static_init(int color);
void static_setvolume(STATICSTATE statics, float volume);
float static_getvolume(STATICSTATE statics);
void static_generate(STATICSTATE statics, const sp_audioformat *format, audio_fifo_t *inputfifo, audio_fifo_t *outputfifo);
void static_free(STATICSTATE statics);

#endif