#include <stdlib.h>
#include <string.h>
#include "tuner.h"

void tuner_calcstate(float freq, struct tunerstate *output) {
	output->static_volume = .0f;
	output->static_sample = 0;
	output->static2_volume = .0f;
	output->static2_sample = 0;
	output->music_volume = .0f;
	output->music_playlist_index = 0;
	strcpy(output->music_playlist_uri, "");
}

typedef struct {
	int freq;
	char name[128];
	char uri[128];
} tuner_channel;

typedef struct {
	int freq;
	int num_channels;
	tuner_channel channels[10];
} tuner_private;

void tuner_getstate(TUNER tunerid, struct tunerstate *state) {
	tuner_private *tuner = (tuner_private *)tunerid;
}

TUNER tuner_init() {
	tuner_private *tuner = (tuner_private *)malloc(sizeof(tuner_private));
	tuner->freq = rand()%100;
	tuner->num_channels = 0;
	return (TUNER)tuner;
}

void tuner_free(TUNER tunerid) {
	tuner_private *tuner = (tuner_private *)tunerid;
	tuner->freq = 0;
}

void tuner_addchannel(TUNER tunerid, int freq, char *name, char *uri) {
	tuner_private *tuner = (tuner_private *)tunerid;
	tuner_channel *chan = &tuner->channels[tuner->num_channels++];
	chan->freq = freq;
	strcpy( chan->name, name );
	strcpy( chan->uri, uri );
}

void tuner_tune_by(TUNER tunerid, int delta) {
	tuner_private *tuner = (tuner_private *)tunerid;
	tuner->freq += delta;
	while(tuner->freq < 0) tuner->freq += 100;
	while(tuner->freq > 100) tuner->freq -= 100;
}

void tuner_tune_to(TUNER tunerid, int freq) {
	tuner_private *tuner = (tuner_private *)tunerid;
	tuner->freq = freq;
	while(tuner->freq < 0) tuner->freq += 100;
	while(tuner->freq > 100) tuner->freq -= 100;
}

