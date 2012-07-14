#include "statics.h"
#include "audio.h"

typedef struct {
	int vol;
	int targetvol;
	float targetvol_f;
	unsigned long offset;
	unsigned long length;
	signed short *buf;
} staticstate_private;

STATICSTATE static_init(int color) {
	staticstate_private *state = (staticstate_private *)malloc(sizeof(staticstate_private));
	int len = 300000 + (rand()&65535);
	int i, v = 0, c = 0;
	state->offset = rand() % len;
	state->length = len;
	state->buf = malloc(2 * len);

	for(i=0; i<len; i++) {
		if (c==0) v = (rand()&32767)-16384;
		c ++;
		if (c > color / 10) c = 0;
		state->buf[i] = v;
	}

	return (STATICSTATE)state;
}

void static_setvolume(STATICSTATE statics, float volume) {
	staticstate_private *state = (staticstate_private *)statics;
	if (volume < 0.0) volume = 0.0;
	if (volume > 1.0) volume = 1.0;
	state->targetvol_f = volume;
	state->targetvol = (int)(32768.0f * volume);
	printf("STATIC: Set volume to %d\n", state->targetvol);
}

float static_getvolume(STATICSTATE statics) {
	staticstate_private *state = (staticstate_private *)statics;
	return state->targetvol_f;
}

void static_generate(STATICSTATE statics, const sp_audioformat *format, audio_fifo_t *inputfifo, audio_fifo_t *outputfifo) {
	staticstate_private *state = (staticstate_private *)statics;

	int num_frames = 0;

	audio_fifo_t *af = outputfifo;
	audio_fifo_data_t *afd;
	int i,o,x;
	size_t s;

	pthread_mutex_lock(&af->mutex);

	/* Buffer one second of audio */
	if (af->qlen > format->sample_rate) {
		pthread_mutex_unlock(&af->mutex);
		return 0;
	}



	audio_fifo_data_t *infd = audio_get(inputfifo);
	if (infd)
	// printf("%d samples x %d channels\n", infd->nsamples, infd->channels);
	num_frames = infd->nsamples;



	s = num_frames * sizeof(int16_t) * format->channels;

	afd = malloc(sizeof(*afd) + s);
//	memcpy(afd->samples, frames, s);

	o = state->offset;
	for(i=0; i<num_frames * format->channels; i++) {
		x = infd->samples[i];
		// x += (rand() % 10000) * state->targetvol / 32768;
		x += (state->buf[o] * state->vol) / 32768;
		if (x<-32767) x=-32767;
		if (x>32767) x=32767;
		afd->samples[i] = x;
		o ++;
		if (o > state->length) o=0;
		if (state->vol < state->targetvol) state->vol++;
		if (state->vol > state->targetvol) state->vol--;
	}
	state->offset = o;

	free(infd);

	afd->nsamples = num_frames;

	afd->rate = format->sample_rate;
	afd->channels = format->channels;

	TAILQ_INSERT_TAIL(&af->q, afd, link);
	af->qlen += num_frames;

	pthread_cond_signal(&af->cond);
	pthread_mutex_unlock(&af->mutex);

}

void static_free(STATICSTATE statics) {
	staticstate_private *state = (staticstate_private *)statics;
	free(state->buf);
}
