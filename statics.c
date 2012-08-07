#include "statics.h"
#include "audio.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

typedef struct {
	int vol;
	int targetvol;
	float targetvol_f;
	unsigned long offset;
	unsigned long length;
	signed short *buf;
} staticstate_private;

STATICSTATE static_init(int color, char *filename) {
	staticstate_private *state = (staticstate_private *)malloc(sizeof(staticstate_private));
	FILE *f = fopen( filename, "rb" );
	if (f) {
		fseek( f, 0, SEEK_END );
		int len = ftell(f) / 2;
		int i;
	
		fseek( f, 0, SEEK_SET );
		state->length = len;
		state->buf = malloc(2 * len);
		for( i=0; i<len; i++ ) {
			unsigned short a;
			fread( &a, 1,2 ,f );
			// fread( &a, 1,2 ,f );
			state->buf[i] = a;
		}
		// fread( state->buf, 2, len, f );		

		fclose(f);
	}
	else {

		int len = 300000 + (rand()&65535);
		int i, v = 0, c = 0;
		state->length = len;
		state->buf = malloc(2 * len);
		for(i=0; i<len; i++) {
			if (c==0)
				v = (rand() & 8191) - 4096;
			c ++;
			if (c > color / 10) c = 0;
			state->buf[i] = v;
		}
	}
	
	state->offset = rand() % state->length;
	
	return (STATICSTATE)state;
}

void static_setvolume(STATICSTATE statics, float volume) {
	staticstate_private *state = (staticstate_private *)statics;
	if (volume < 0.0) volume = 0.0;
	if (volume > 1.0) volume = 1.0;
	state->targetvol_f = volume;
	state->targetvol = (int)(32768.0f * volume);
}

float static_getvolume(STATICSTATE statics) {
	staticstate_private *state = (staticstate_private *)statics;
	return state->targetvol_f;
}

void static_generate(STATICSTATE statics, audio_fifo_t *inputfifo, audio_fifo_t *outputfifo, bool comped) {
	staticstate_private *state = (staticstate_private *)statics;

	audio_fifo_t *af = outputfifo;
	int i, o, x;

	audio_fifo_data_t *infd = audio_get(inputfifo);
	if (infd == NULL)
 	{
		printf("static input ready.\n");
		return;
	}
	// printf("Generating noise; samples=%d, channels=%d\n", infd->nsamples, infd->channels);
	audio_fifo_data_t *afd = audio_data_create(infd->nsamples, infd->channels);

	o = state->offset;
	for(i=0; i<afd->nsamples * afd->channels; i+=2) {
		x = infd->samples[i];
		if (comped) {
			x -= (x * state->vol) / 100000;
			x += (state->buf[o] * state->vol) / 60000;
		}
		else {
			x += (state->buf[o] * state->vol) / 32768;
		}
		if (x<-32767) x=-32767;
		if (x>32767) x=32767;
		afd->samples[i] = x;
		afd->samples[i+1] = x;
		o ++;
		if (o > state->length) o=0;
		if (state->vol < state->targetvol) state->vol++;
		if (state->vol > state->targetvol) state->vol--;
	}
	state->offset = o;

	free(infd);

	audio_fifo_queue(outputfifo, afd);
}

void static_free(STATICSTATE statics) {
	staticstate_private *state = (staticstate_private *)statics;
	free(state->buf);
	free(state);
}





static int p[512];
static int permutation[] = { 151,160,137,91,90,15,
	131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,8,99,37,240,
	21,10,23,190,6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,
	35,11,32,57,177,33,88,237,149,56,87,174,20,125,136,171,168,68,175,
	74,165,71,134,139,48,27,166,77,146,158,231,83,111,229,122,60,211,133,
	230,220,105,92,41,55,46,245,40,244,102,143,54,65,25,63,161,1,216,
	80,73,209,76,132,187,208,89,18,169,200,196,135,130,116,188,159,86,
	164,100,109,198,173,186,3,64,52,217,226,250,124,123,5,202,38,147,
	118,126,255,82,85,212,207,206,59,227,47,16,58,17,182,189,28,42,223,
	183,170,213,119,248,152,2,44,154,163,70,221,153,101,155,167,43,
	172,9,129,22,39,253,19,98,108,110,79,113,224,232,178,185,112,104,
	218,246,97,228,251,34,242,193,238,210,144,12,191,179,162,241,81,51,
	145,235,249,14,239,107,49,192,214,31,181,199,106,157,184,84,204,176,
	115,121,50,45,127,4,150,254,138,236,205,93,222,114,67,29,24,72,243,
	141,128,195,78,66,215,61,156,180
};

/* Function declarations */
double   fade(double t);
double   lerp(double t, double a, double b);
double   grad(int hash, double x, double y, double z); 
void     init_noise();
double   pnoise(double x, double y, double z);

void init_noise() {
	int i;
	for(i = 0; i < 256 ; i++) p[256+i] = p[i] = permutation[i];
}

double pnoise(double x, double y, double z) {
	int   X = (int)floor(x) & 255,             /* FIND UNIT CUBE THAT */
	      Y = (int)floor(y) & 255,             /* CONTAINS POINT.     */
	      Z = (int)floor(z) & 255;
	x -= floor(x);                             /* FIND RELATIVE X,Y,Z */
	y -= floor(y);                             /* OF POINT IN CUBE.   */
	z -= floor(z);
	double  u = fade(x),                       /* COMPUTE FADE CURVES */
	        v = fade(y),                       /* FOR EACH OF X,Y,Z.  */
	        w = fade(z);
	int  A = p[X]+Y,
	     AA = p[A]+Z,
	     AB = p[A+1]+Z, /* HASH COORDINATES OF */
	     B = p[X+1]+Y,
	     BA = p[B]+Z,
	     BB = p[B+1]+Z; /* THE 8 CUBE CORNERS, */

	return lerp(w,lerp(v,lerp(u, grad(p[AA  ], x, y, z),   /* AND ADD */
	                     grad(p[BA  ], x-1, y, z)),        /* BLENDED */
	             lerp(u, grad(p[AB  ], x, y-1, z),         /* RESULTS */
	                     grad(p[BB  ], x-1, y-1, z))),     /* FROM  8 */
	             lerp(v, lerp(u, grad(p[AA+1], x, y, z-1 ),/* CORNERS */
	                     grad(p[BA+1], x-1, y, z-1)),      /* OF CUBE */
	             lerp(u, grad(p[AB+1], x, y-1, z-1),
	                     grad(p[BB+1], x-1, y-1, z-1))));
}

double fade(double t){ return t * t * t * (t * (t * 6 - 15) + 10); }
double lerp(double t, double a, double b){ return a + t * (b - a); }
double grad(int hash, double x, double y, double z) 
{
	int     h = hash & 15;       /* CONVERT LO 4 BITS OF HASH CODE */
	double  u = h < 8 ? x : y,   /* INTO 12 GRADIENT DIRECTIONS.   */
	        v = h < 4 ? y : h==12||h==14 ? x : z;
	return ((h&1) == 0 ? u : -u) + ((h&2) == 0 ? v : -v);
}


