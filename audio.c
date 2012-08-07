/*
 * Copyright (c) 2010 Spotify Ltd
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 *
 * Audio helper functions.
 *
 * This file is part of the libspotify examples suite.
 */

#include "audio.h"
#include <stdlib.h>
#include <pthread.h>
#include <stdio.h>

void audio_fifo_reset(audio_fifo_t *af) {
	printf("audio_fifo_reset %X\n", af);
    TAILQ_INIT(&af->q);
    af->qlen = 0;
    pthread_mutex_init(&af->mutex, NULL);
    // pthread_cond_init(&af->cond, NULL);
}

audio_fifo_data_t* audio_get(audio_fifo_t *af) {
	// printf("audio_fifo_get %X\n", af);
    audio_fifo_data_t *afd = NULL;
    pthread_mutex_lock(&af->mutex);
    afd = TAILQ_FIRST(&af->q);
    if (afd != NULL) {
        // while (!(afd = TAILQ_FIRST(&af->q))) {};
        // pthread_cond_wait(&af->cond, &af->mutex);
        TAILQ_REMOVE(&af->q, afd, link);
        af->qlen -= afd->nsamples;
    }
    pthread_mutex_unlock(&af->mutex);
    return afd;
}

void audio_fifo_flush(audio_fifo_t *af) {
    printf("audio_flush_fifo %X\n", af);
    audio_fifo_data_t *afd;
    pthread_mutex_lock(&af->mutex);
    while ((afd = TAILQ_FIRST(&af->q))) {
    	TAILQ_REMOVE(&af->q, afd, link);
    	free(afd);
    }
    af->qlen = 0;
    pthread_mutex_unlock(&af->mutex);
    af->qlen = 0;
}

int audio_fifo_available(audio_fifo_t *af) {
    pthread_mutex_lock(&af->mutex);
    int r = af->qlen;
    pthread_mutex_unlock(&af->mutex);
    return r;
}

audio_fifo_data_t *audio_data_create(int samples, int channels) {
    int s = samples * sizeof(int16_t) * channels;
    audio_fifo_data_t *afd = malloc(sizeof(audio_fifo_data_t) + s);
    afd->nsamples = samples;
    afd->channels = channels;
    afd->rate = 44100;
    memset(afd->samples, 0, s);
    return afd;
}

void audio_fifo_queue(audio_fifo_t *af, audio_fifo_data_t *afd) {
    pthread_mutex_lock(&af->mutex);
    // printf("audio_fifo_queue %X data=%X\n", af, afd);
    TAILQ_INSERT_TAIL(&af->q, afd, link);
    af->qlen += afd->nsamples;// * afd->channels;
    // pthread_cond_signal(&af->cond);
    pthread_mutex_unlock(&af->mutex);
}

