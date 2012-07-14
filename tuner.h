#ifndef _TUNER_H_
#define _TUNER_H_

struct tunerstate {
	float static_volume;
	int static_sample;

	float static2_volume;
	int static2_sample;

	float music_volume;
	int music_playlist_index;
	char music_playlist_uri[100];
};

typedef unsigned long TUNER;

extern void tuner_getstate(TUNER tuner, struct tunerstate *state);
extern TUNER tuner_init();
extern void tuner_free(TUNER tuner);
extern void tuner_addchannel(TUNER tuner, int freq, char *name, char *uri);
extern void tuner_tune_by(TUNER tuner, int delta);
extern void tuner_tune_to(TUNER tuner, int freq);

#endif