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
 * OpenAL audio output driver.
 *
 * This file is part of the libspotify examples suite.
 */

#include <OpenAL/al.h>
#include <OpenAL/alc.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>

#include "audio.h"

#define NUM_BUFFERS 5

static void error_exit(const char *msg)
{
    puts(msg);
    exit(1);
}

static int queue_buffer(ALuint source, audio_fifo_t *af, ALuint buffer, audio_fifo_data_t *afd)
{
  // printf("waiting for a buffer...\n");
  while(afd == NULL) {
    afd = audio_get(af);
    if(afd == NULL) {
      //printf("got null from af %X\n", af);
      usleep(10000);
    }
  }
  /*
  int i;
  printf("buffer 0x%08X; %2d channels, %5d samples, sample rate %5d: ", 
    afd,
    afd->channels,
    afd->nsamples,
    afd->rate);
  for(i=0; i<5; i++ ) printf("%d ", afd->samples[i]);
  printf("\n");
  */

  alBufferData(buffer,
    afd->channels == 1 ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16,
    afd->samples,
    afd->nsamples * afd->channels * sizeof(short),
    afd->rate);

  alSourceQueueBuffers(source, 1, &buffer);

  free(afd);
  usleep(100);
  return 1;
}

static void* audio_start(void *aux)
{
  audio_fifo_t *af = aux;
  audio_fifo_data_t *afd;
  unsigned int frame = 0;
  ALCdevice *device = NULL;
  ALCcontext *context = NULL;
  ALuint buffers[NUM_BUFFERS];
  ALuint source;
  ALint processed;
  ALenum error;
  ALint rate;
  ALint channels;
  int i;
  device = alcOpenDevice(NULL); /* Use the default device */
  if (!device) error_exit("failed to open device");
  context = alcCreateContext(device, NULL);
  alcMakeContextCurrent(context);
  alListenerf(AL_GAIN, 1.0f);
  alDistanceModel(AL_NONE);
  alGenBuffers((ALsizei)NUM_BUFFERS, buffers);
  alGenSources(1, &source);

  /* First prebuffer some audio */
  for(i=0; i<NUM_BUFFERS; i++)
    queue_buffer(source, af, buffers[i], NULL);

  for (;;) {
    printf("openal: play.\n");
    alSourcePlay(source);
    for (;;) {

      /* Wait for some audio to play */
      do {
        alGetSourcei(source, AL_BUFFERS_PROCESSED, &processed);
     //   printf("processed=%d\n", processed);
        usleep(100);
      } while (!processed);

      if (processed>=NUM_BUFFERS) {
        printf("openal buffer overrun? processed=%d\n", processed);
       // usleep(100000);
        break;
      }

      /* and queue some more audio */
      afd = audio_get(af);
      if (afd != NULL) {
        // printf("openal: rate=%d, channels=%d, frame=%d\n", afd->rate, afd->channels, frame);
        // Remove old audio from the queue..
        alSourceUnqueueBuffers(source, 1, &buffers[frame % NUM_BUFFERS]);
        alGetBufferi(buffers[frame % NUM_BUFFERS], AL_FREQUENCY, &rate);
        alGetBufferi(buffers[frame % NUM_BUFFERS], AL_CHANNELS, &channels);
        if (afd->rate != rate || afd->channels != channels) {
          printf("rate or channel count changed, resetting\n");
          free(afd);
          break;
        }
        queue_buffer(source, af, buffers[frame % NUM_BUFFERS], afd);
        frame++;
      }
      usleep(100);

      if ((error = alcGetError(device)) != AL_NO_ERROR) {
        printf("openal al error: %d\n", error);
        exit(1);
      }
    }
    printf("openal: stop.\n");

    // Format or rate changed, so we need to reset all buffers
    alSourcei(source, AL_BUFFER, 0);
    alSourceStop(source);

    queue_buffer(source, af, buffers[0], afd);
    for(i=1; i<NUM_BUFFERS; i++)
      queue_buffer(source, af, buffers[i], NULL);

    frame = 0;
  }
}

void audio_init(audio_fifo_t *af)
{
    pthread_t tid;

    TAILQ_INIT(&af->q);
    af->qlen = 0;

    pthread_create(&tid, NULL, audio_start, af);
    printf("OpenAL audio initialized.\n");
}


