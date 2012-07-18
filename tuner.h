#ifndef _TUNER_H_
#define _TUNER_H_

struct tunerstate {
	int freq;
	float static_volume;
	float static2_volume;
	float static3_volume;
	float music_volume;
	int music_playlist_index;
	int display_playlist_index;
	int display_freq;
	char music_playlist_uri[100];
};

typedef unsigned long TUNER;

extern void tuner_getstate(TUNER tuner, struct tunerstate *state);
extern TUNER tuner_init();
extern void tuner_free(TUNER tuner);
extern int tuner_numchannels(TUNER tuner);
extern void tuner_addchannel(TUNER tuner, int freq, char *name, char *uri);
extern void tuner_tune_by(TUNER tuner, int delta);
extern void tuner_tune_to(TUNER tuner, int freq);
extern void tuner_goto(TUNER tuner, int channel);

#endif