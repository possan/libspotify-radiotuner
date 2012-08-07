#include <stdlib.h>
#include <string.h>
#include "tuner.h"
#include "statics.h"
#include <math.h>

typedef struct {
	int freq;
	char name[128];
	char uri[128];
} tuner_channel;

typedef struct {
	int freq;
	int num_channels;
	float perlin_random1;
	float perlin_random2;
	float perlin_random3;
	tuner_channel channels[10];
} tuner_private;

float  scale_noise(float f ){
	f *= 3.0f;
	f += 0.5f;
	if (f < 0.0f) f=0.0f;
	if (f > 1.0f) f=1.0f;
	return f;
}
void tuner_getstate(TUNER tunerid, struct tunerstate *output) {

	tuner_private *tuner = (tuner_private *)tunerid;

	int i, closest_channel = -1, closest_dist = 999999;
	for (i=0; i<tuner->num_channels; i++) {
		tuner_channel *channel = &tuner->channels[i];
		int d = abs(channel->freq - tuner->freq);
		if (d < closest_dist) {
			closest_dist = d;
			closest_channel = i;
		}
	}

	output->freq = tuner->freq;
	output->music_volume = .0f;
	output->static_volume = scale_noise(pnoise((float)tuner->freq * 0.256f, tuner->perlin_random1, 85.0f));
	output->static2_volume = scale_noise(pnoise((float)tuner->freq * 0.356f, tuner->perlin_random2, 30.0f));
	output->static3_volume = scale_noise(pnoise((float)tuner->freq * 0.456f, tuner->perlin_random3, 0.0f));
	output->music_playlist_index = -1;
	output->display_playlist_index = -1;
	output->display_freq = (int)round((float)tuner->freq/10.0f) * 10;
	strcpy(output->music_playlist_uri, "");

	if (i == -1)
		return;

	tuner_channel *channel = &tuner->channels[closest_channel];

	float mv = 1.0 - closest_dist/160.0f;

	if (mv < -0.1f)
		return;

	if (mv < 0.0f) mv = .0f;

	if (mv > 0.4f) output->display_playlist_index = closest_channel;

	output->music_volume = mv * 1.2f;
	if (output->music_volume > 1.0f) output->music_volume = 1.0f;
	output->static_volume *= 1.0f-mv;
	output->static2_volume *= 1.0f-mv;
	output->static3_volume *= 1.0f-mv;

	output->music_playlist_index = closest_channel;
	strcpy(output->music_playlist_uri, channel->uri);
}

TUNER tuner_init() {
	tuner_private *tuner = (tuner_private *)malloc(sizeof(tuner_private));
	tuner->freq = rand()%100;
	tuner->num_channels = 0;
	tuner->perlin_random1 = (float)(rand()%25600) / 100.0f;
	tuner->perlin_random2 = (float)(rand()%25600) / 100.0f;
	tuner->perlin_random3 = (float)(rand()%25600) / 100.0f;
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

int tuner_numchannels(TUNER tunerid) {
	tuner_private *tuner = (tuner_private *)tunerid;
	return tuner->num_channels;
}

void tuner_tune_by(TUNER tunerid, int delta) {
	tuner_private *tuner = (tuner_private *)tunerid;
	tuner->freq += delta;
	while(tuner->freq < 0) tuner->freq += 1000;
	while(tuner->freq > 1000) tuner->freq -= 1000;
	// printf("TUNER: Tune by %d to %d\n", delta, tuner->freq);
}

void tuner_tune_to(TUNER tunerid, int freq) {
	tuner_private *tuner = (tuner_private *)tunerid;
	tuner->freq = freq;
	while(tuner->freq < 0) tuner->freq += 1000;
	while(tuner->freq > 1000) tuner->freq -= 1000;
	// printf("TUNER: Tune to %d\n", freq);
}

void tuner_goto(TUNER tunerid, int channel) {
	tuner_private *tuner = (tuner_private *)tunerid;
	// printf("TUNER: Tune to channel #%d\n", channel);
	if (channel < 0) return;
	if (channel >= tuner->num_channels) return;
	tuner->freq = tuner->channels[channel].freq;
}

