#include <errno.h>
#include <libgen.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <libspotify/api.h>
#include "audio.h"
#include <termios.h>
#include "hardware.h"
#include "tuner.h"
#include "statics.h"
#include <mcheck.h>

extern const uint8_t g_appkey[];
extern const size_t g_appkey_size;

//
// streamer(music or silence)
// -> gapless
// -> static1
// -> static2
// -> static3
// -> output
//

static audio_fifo_t g_musicfifo;
static audio_fifo_t g_gaplessfifo;
static audio_fifo_t g_staticfifo1;
static audio_fifo_t g_staticfifo2;
static audio_fifo_t g_audiofifo;

static pthread_mutex_t g_notify_mutex;
static pthread_cond_t g_notify_cond;
static int g_notify_do = 0;
static int g_playback_done = 0;
static sp_session *g_sess = NULL;
static sp_playlist *g_jukeboxlist = NULL;
static sp_track *g_currenttrack = NULL;

static STATICSTATE g_static1 = 0;
static STATICSTATE g_static2 = 0;
static STATICSTATE g_static3 = 0;
static int g_track_index = -1;

static TUNER g_tuner = 0;
static char g_last_track_name[500];
static char g_last_playlist_name[500];

static int g_is_playing = 0;

/**
 * This callback is called from an internal libspotify thread to ask us to
 * reiterate the main loop.
 *
 * We notify the main thread using a condition variable and a protected variable.
 *
 * @sa sp_session_callbacks#notify_main_thread
 */
static void notify_main_thread(sp_session *sess) {
	pthread_mutex_lock(&g_notify_mutex);
	g_notify_do = 1;
	pthread_cond_signal(&g_notify_cond);
	pthread_mutex_unlock(&g_notify_mutex);
}

static void try_jukebox_start(void)
{
	sp_track *t;

	printf("try_jukebox_start\n");

	if (!g_jukeboxlist)
		return;

	int nt = sp_playlist_num_tracks(g_jukeboxlist);

	if (!nt) {
		fprintf(stderr, "jukebox: No tracks in playlist. Waiting\n");
		return;
	}

	if (nt > 2) {
		int tmp = g_track_index;
		while (tmp == g_track_index)
			tmp = rand() % nt;
		g_track_index = tmp;
	} else {
		g_track_index = rand() % nt;
	}

	printf("play track %d of %d\n", g_track_index, nt);

	t = sp_playlist_track(g_jukeboxlist, g_track_index);
	printf("t=%X\n", t);
	if (g_currenttrack && t != g_currenttrack) {
		printf("stopping currently playing..\n");
		/* Someone changed the current track */
		// audio_fifo_flush(&g_musicfifo);
		// sp_session_player_unload(g_sess);
		// g_currenttrack = NULL;
	}

	if (!t)
		return;

	int err = sp_track_error(t);
	if (err != SP_ERROR_OK /*&& err != SP_ERROR_IS_LOADING*/) {
		printf("track error? %d\n", err);
		return;
	}

	if (g_currenttrack == t) {
		printf("not starting the same track.\n");
		return;
	}

	g_currenttrack = t;

	printf("jukebox: Now playing \"%s\"...\n", sp_track_name(t));

	sp_artist *art = sp_track_artist(t, 0);
	if (art != NULL) {
		printf("jukebox: By \"%s\"...\n", sp_artist_name(art));
		sprintf( g_last_track_name, "%s by %s", sp_track_name(t), sp_artist_name(art) );
	} else {
		sprintf( g_last_track_name, "%s", sp_track_name(t) );
	}



	fflush(stdout);

	sp_session_player_load(g_sess, t);
	usleep(100000);
	sp_session_player_play(g_sess, 1);
	usleep(100000);
	g_is_playing = 1;

	notify_main_thread(g_sess);

	printf("after try_jukebox_start\n");
	tuner_debug();
}

/* --------------------------  PLAYLIST CALLBACKS  ------------------------- */
/**
 * Callback from libspotify, saying that a track has been added to a playlist.
 *
 * @param  pl          The playlist handle
 * @param  tracks      An array of track handles
 * @param  num_tracks  The number of tracks in the \c tracks array
 * @param  position    Where the tracks were inserted
 * @param  userdata    The opaque pointer
 */
static void tracks_added(sp_playlist *pl, sp_track * const *tracks,
                         int num_tracks, int position, void *userdata)
{
	printf("jukebox: %d tracks were added\n", num_tracks);
	fflush(stdout);

	if (pl != g_jukeboxlist)
	{
		printf("not correct playlist!\n");
		return;
	}

	if (!g_is_playing)
		try_jukebox_start();
}

/**
 * Callback from libspotify, saying that a track has been added to a playlist.
 *
 * @param  pl          The playlist handle
 * @param  tracks      An array of track indices
 * @param  num_tracks  The number of tracks in the \c tracks array
 * @param  userdata    The opaque pointer
 */
static void tracks_removed(sp_playlist *pl, const int *tracks,
                           int num_tracks, void *userdata)
{
	int i, k = 0;

	printf("jukebox: %d tracks were removed\n", num_tracks);
	fflush(stdout);
}

/**
 * Callback from libspotify, telling when tracks have been moved around in a playlist.
 *
 * @param  pl            The playlist handle
 * @param  tracks        An array of track indices
 * @param  num_tracks    The number of tracks in the \c tracks array
 * @param  new_position  To where the tracks were moved
 * @param  userdata      The opaque pointer
 */
static void tracks_moved(sp_playlist *pl, const int *tracks,
                         int num_tracks, int new_position, void *userdata)
{
	printf("jukebox: %d tracks were moved around\n", num_tracks);
	fflush(stdout);
}

/**
 * Callback from libspotify. Something renamed the playlist.
 *
 * @param  pl            The playlist handle
 * @param  userdata      The opaque pointer
 */
static void playlist_renamed(sp_playlist *pl, void *userdata)
{
	const char *name = sp_playlist_name(pl);

/*	if (!strcasecmp(name, g_listname)) {
		g_jukeboxlist = pl;
		g_track_index = 0;
		try_jukebox_start();
	} else
	if (g_jukeboxlist == pl) {
		printf("jukebox: current playlist renamed to \"%s\".\n", name);
		hardware_banner(name, 200);
		g_jukeboxlist = NULL;
		g_currenttrack = NULL;
		sp_session_player_unload(g_sess);
	}*/
}

/**
 * The callbacks we are interested in for individual playlists.
 */
static sp_playlist_callbacks pl_callbacks = {
	.tracks_added = &tracks_added,
	.tracks_removed = &tracks_removed,
	.tracks_moved = &tracks_moved,
	.playlist_renamed = &playlist_renamed,
};


/* --------------------  PLAYLIST CONTAINER CALLBACKS  --------------------- */
/**
 * Callback from libspotify, telling us a playlist was added to the playlist container.
 *
 * We add our playlist callbacks to the newly added playlist.
 *
 * @param  pc            The playlist container handle
 * @param  pl            The playlist handle
 * @param  position      Index of the added playlist
 * @param  userdata      The opaque pointer
 */
static void playlist_added(sp_playlistcontainer *pc, sp_playlist *pl,
                           int position, void *userdata)
{
	/*
	sp_playlist_add_callbacks(pl, &pl_callbacks, NULL);
	if (!strcasecmp(sp_playlist_name(pl), g_listname)) {
		g_jukeboxlist = pl;
		try_jukebox_start();
	}
	*/
}

/**
 * Callback from libspotify, telling us a playlist was removed from the playlist container.
 *
 * This is the place to remove our playlist callbacks.
 *
 * @param  pc            The playlist container handle
 * @param  pl            The playlist handle
 * @param  position      Index of the removed playlist
 * @param  userdata      The opaque pointer
 */
static void playlist_removed(sp_playlistcontainer *pc, sp_playlist *pl,
                             int position, void *userdata)
{
	sp_playlist_remove_callbacks(pl, &pl_callbacks, NULL);
}


/**
 * Callback from libspotify, telling us the rootlist is fully synchronized
 * We just print an informational message
 *
 * @param  pc            The playlist container handle
 * @param  userdata      The opaque pointer
 */
static void container_loaded(sp_playlistcontainer *pc, void *userdata)
{
	fprintf(stderr, "jukebox: Rootlist synchronized (%d playlists)\n",
		sp_playlistcontainer_num_playlists(pc));
}


/**
 * The playlist container callbacks
 */
static sp_playlistcontainer_callbacks pc_callbacks = {
	.playlist_added = &playlist_added,
	.playlist_removed = &playlist_removed,
	.container_loaded = &container_loaded,
};


/* ---------------------------  SESSION CALLBACKS  ------------------------- */
/**
 * This callback is called when an attempt to login has succeeded or failed.
 *
 * @sa sp_session_callbacks#logged_in
 */
static void logged_in(sp_session *sess, sp_error error)
{
	sp_playlistcontainer *pc = sp_session_playlistcontainer(sess);
	int i;

	if (SP_ERROR_OK != error) {
		fprintf(stderr, "jukebox: Login failed: %s\n",
			sp_error_message(error));
		exit(2);
	}

	sp_playlistcontainer_add_callbacks(
		pc,
		&pc_callbacks,
		NULL);

	hardware_banner("logged in.", 200);

	printf("jukebox: Looking at %d playlists\n", sp_playlistcontainer_num_playlists(pc));

	/*
	for (i = 0; i < sp_playlistcontainer_num_playlists(pc); ++i) {
		sp_playlist *pl = sp_playlistcontainer_playlist(pc, i);

		sp_playlist_add_callbacks(pl, &pl_callbacks, NULL);

		if (!strcasecmp(sp_playlist_name(pl), g_listname)) {
			g_jukeboxlist = pl;
			try_jukebox_start();
		}
	}

	if (!g_jukeboxlist) {
		printf("jukebox: No such playlist. Waiting for one to pop up...\n");
		fflush(stdout);
	}
	*/
}

void start_playlist(char *uri)
{
	printf("Start playlist: %s\n", uri);
	sp_link *link = sp_link_create_from_string(uri);
	sp_playlist *pl = sp_playlist_create(g_sess, link);
	sp_playlist_add_callbacks(pl, &pl_callbacks, NULL);
	sprintf(g_last_playlist_name, sp_playlist_name(pl));
	// hardware_banner(sp_playlist_name(pl), 200);
	g_jukeboxlist = pl;
	try_jukebox_start();
}

int music_vol;
int music_targetvol;


/**
 * This callback is used from libspotify whenever there is PCM data available.
 *
 * @sa sp_session_callbacks#music_delivery
 */
static int music_delivery(sp_session *sess, const sp_audioformat *format,
                          const void *frames, int num_frames)
{
	audio_fifo_t *af = &g_musicfifo;
	int i, f;
	signed short *ptr1, *ptr2;

	// printf("music_delivery; %d samples at %X.", num_frames, frames);

	if (num_frames == 0) {
		// Audio discontinuity, do nothing
		usleep(1000);
		return 0;
	}

	// Buffer one second of audio
	int filled = audio_fifo_available(af);
	if (filled > 5000) {
		// printf("music buffer (%X) is full (%d)...\n", af, filled);
		// pthread_mutex_unlock(&af->mutex);
		usleep(5000);
		return 0;
	}

	audio_fifo_data_t *afd = audio_data_create( num_frames, format->channels );

	struct tunerstate ts;
	tuner_getstate( g_tuner, &ts );
	music_targetvol = (int)(ts.music_volume * 32768.0f);

	ptr1 = (signed short *)afd->samples;
	ptr2 = (signed short *)frames;
	for (i=0; i<num_frames * format->channels; i++) {
		f = ptr2[i];
		f *= music_vol;
		f /= 32768;
		if (f<-32767) f=-32767;
		if (f>32767) f=32767;
		ptr1[i] = f;
		if (music_vol < music_targetvol) music_vol ++;
		if (music_vol > music_targetvol) music_vol --;
		// if(i<5) printf("%d ", f);
	}
	// printf("\n");

	audio_fifo_queue(af, afd);
	// free(afd);

	return num_frames;
}


/**
 * This callback is used from libspotify when the current track has ended
 *
 * @sa sp_session_callbacks#end_of_track
 */
static void end_of_track(sp_session *sess)
{
	pthread_mutex_lock(&g_notify_mutex);
	g_playback_done = 1;
	g_notify_do = 1;
	g_is_playing = 0;
	pthread_cond_signal(&g_notify_cond);
	pthread_mutex_unlock(&g_notify_mutex);
}


/**
 * Callback called when libspotify has new metadata available
 *
 * Not used in this example (but available to be able to reuse the session.c file
 * for other examples.)
 *
 * @sa sp_session_callbacks#metadata_updated
 */
static void metadata_updated(sp_session *sess)
{
	// try_jukebox_start();
}

/**
 * Notification that some other connection has started playing on this account.
 * Playback has been stopped.
 *
 * @sa sp_session_callbacks#play_token_lost
 */
static void play_token_lost(sp_session *sess)
{
	audio_fifo_flush(&g_musicfifo);
	// audio_fifo_flush(&g_audiofifo);
	// audio_fifo_flush(&g_audiofifo);
	// audio_fifo_flush(&g_audiofifo);
	// audio_fifo_flush(&g_audiofifo);
	if (g_currenttrack != NULL) {
		sp_session_player_unload(g_sess);
		g_currenttrack = NULL;
	}
}

/**
 * The session callbacks
 */
static sp_session_callbacks session_callbacks = {
	.logged_in = &logged_in,
	.notify_main_thread = &notify_main_thread,
	.music_delivery = &music_delivery,
	.metadata_updated = &metadata_updated,
	.play_token_lost = &play_token_lost,
	.log_message = NULL,
	.end_of_track = &end_of_track,
};

/**
 * The session configuration. Note that application_key_size is an external, so
 * we set it in main() instead.
 */
static sp_session_config spconfig = {
	.api_version = SPOTIFY_API_VERSION,
	.cache_location = "tmp",
	.settings_location = "tmp",
	.application_key = g_appkey,
	.application_key_size = 0, // Set in main()
	.user_agent = "spotify-jukebox-example",
	.callbacks = &session_callbacks,
	NULL,
};
/* -------------------------  END SESSION CALLBACKS  ----------------------- */


/**
 * A track has ended. Remove it from the playlist.
 *
 * Called from the main loop when the music_delivery() callback has set g_playback_done.
 */
static void track_ended(bool startNext) {
	printf("track_ended startNext=%d\n", startNext);
	int tracks = 0;
	if (g_currenttrack) {
		printf("track_ended: stopping current track.\n");
		g_currenttrack = NULL;
		sp_session_player_unload(g_sess);
		g_is_playing = 0;
	}
	audio_fifo_flush(&g_musicfifo);
	notify_main_thread(g_sess);
	audio_fifo_flush(&g_musicfifo);
	usleep(50000);
	if (startNext) {
		printf("track_ended: play next track.\n");
		try_jukebox_start();
		usleep(50000);
		notify_main_thread(g_sess);
				banner_track();
	}
}

/**
 * Show usage information
 *
 * @param  progname  The program name
 */
static void usage(const char *progname) {
	fprintf(stderr, "usage: %s -u <username> -p <password> -s <serialdevice>\n", progname);
}

void peek_input() {
 	int c = getchar();
 	float f;
	switch (c) {
		case 'f': hardware_fire_event(HE_FREQ_DOWN); break;
		case 'F': hardware_fire_event(HE_FREQ_UP); break;
		case 'v': hardware_fire_event(HE_VOL_DOWN); break;
		case 'V': hardware_fire_event(HE_VOL_UP); break;
		case 'x': hardware_fire_event(HE_X_DOWN); break;
		case 'X': hardware_fire_event(HE_X_UP); break;
		case ' ': hardware_fire_event(HE_PLAY_PAUSE); break;
		case 'n': hardware_fire_event(HE_SKIP_NEXT); break;
		case '1': hardware_fire_event(HE_CHANNEL1); break;
		case '2': hardware_fire_event(HE_CHANNEL2); break;
		case '3': hardware_fire_event(HE_CHANNEL3); break;
		case '4': hardware_fire_event(HE_CHANNEL4); break;
		case '5': hardware_fire_event(HE_CHANNEL5); break;
		default: printf("unhandled character '%c' #%d\n", c, c); break;
	}
}

int last_display_playlist = -1;
int last_display_freq = -1;

void banner_track() {
	if (strlen(g_last_track_name) > 0) {
		hardware_banner(g_last_track_name, 200);
	}
}

void tuner_debug() {
	struct tunerstate ts;
	tuner_getstate( g_tuner, &ts );
	printf("TUNER; freq=%d, channel=%d (%d), playlist=\"%s\", volumes=[%d,  %d,%d,%d]\n",
		ts.freq,
		ts.music_playlist_index,
		ts.display_playlist_index,
		ts.music_playlist_uri,
		(int)(ts.music_volume * 100),
		(int)(ts.static_volume * 100),
		(int)(ts.static2_volume * 100),
		(int)(ts.static3_volume * 100));

	if (ts.display_freq != last_display_freq) {
		last_display_freq = ts.display_freq;
		char buf[10];
		sprintf(buf,"%d%%", ts.display_freq);
		hardware_banner(buf, 200);
	}

	if (ts.display_playlist_index != last_display_playlist) {
		last_display_playlist = ts.display_playlist_index;
		if (last_display_playlist != -1) {

			if (strlen(g_last_playlist_name) > 0) {
				hardware_banner(g_last_playlist_name, 200);
			}

			banner_track();
		}
	}

}

void _hardware_event(int event) {
	switch(event) {
		case HE_FREQ_UP: tuner_tune_by(g_tuner, 10); tuner_debug(); break;
		case HE_FREQ_DOWN: tuner_tune_by(g_tuner, -10); tuner_debug(); break;
		case HE_CHANNEL1: tuner_goto(g_tuner, 0); tuner_debug(); break;
		case HE_CHANNEL2: tuner_goto(g_tuner, 1); tuner_debug(); break;
		case HE_CHANNEL3: tuner_goto(g_tuner, 2); tuner_debug(); break;
		case HE_CHANNEL4: tuner_goto(g_tuner, 3); tuner_debug(); break;
		case HE_CHANNEL5: tuner_goto(g_tuner, 4); tuner_debug(); break;
		case HE_PLAY_PAUSE:
			if (g_is_playing) {
				printf("Pause playback\n");
				sp_session_player_play(g_sess, 0);
				g_is_playing = 0;
			} else {
				printf("Resume playback\n");
				sp_session_player_play(g_sess, 1);
				g_is_playing = 1;
			}
			notify_main_thread(g_sess);
			break;
		case HE_SKIP_NEXT:
			hardware_banner("Skip", 100);
			track_ended(1);
			notify_main_thread(g_sess);
			break;
	}
}

void inputloop(void *arg) {
	sleep(1);
	static struct termios oldt, newt;
	tcgetattr( STDIN_FILENO, &oldt);
	newt = oldt;
	newt.c_lflag &= ~(ICANON);
	tcsetattr( STDIN_FILENO, TCSANOW, &newt);
	int last_playlist = -1;
	struct tunerstate tunerstate;
	while(1) {
		tuner_getstate( g_tuner, &tunerstate );
		if (tunerstate.music_playlist_index != last_playlist) {
			if (last_playlist != -1) {
				track_ended(0);
			}
			last_playlist = tunerstate.music_playlist_index;
			if (last_playlist != -1) {
				start_playlist (tunerstate.music_playlist_uri);
			}
		}
		peek_input();
		usleep(100000);
	}
	return NULL;
}

void gaplessloop(void *arg) {
	printf("Starting gaplessloop...\n");
	sleep(1);
	while(1) {
		usleep(1000);

		int a = audio_fifo_available(&g_gaplessfifo);
		if (a > 5000) {
			// printf("gapless: output is full (%d)\n", a);
			continue;
		}

		if (!g_is_playing) {
			// printf("gapless: not playing, generate silence.\n");
			audio_fifo_data_t *afd = audio_data_create(2048, 2);
			audio_fifo_queue(&g_gaplessfifo, afd);
			continue;
		}

		// int av = audio_fifo_available(&g_gaplessfifo);
		int av2 = audio_fifo_available(&g_musicfifo);
		if (av2 < 4000) {
			// printf("gapless: not enough music input (%d)\n", av2);
			continue;
		}

		audio_fifo_data_t *inp = audio_get(&g_musicfifo);
		if (inp == NULL) {
			printf("gapless: nothing read.\n");
			continue;
		}


		audio_fifo_data_t *afd = audio_data_create(inp->nsamples, inp->channels);
		int16_t *ptr1 = afd->samples;
		int16_t *ptr2 = inp->samples;
		int i;
		// printf("music data (%d samples x %d channels):\n", inp->nsamples, inp->channels);
		for (i=0; i<inp->nsamples * inp->channels; i++) {
			ptr1[i] = ptr2[i];
		//	if (i<5) printf("%6d ", ptr2[i]);
		}
		// printf("  -> 0x%08X\n", afd);
		audio_fifo_queue(&g_gaplessfifo, afd);

		free(inp);
	}
}

void static1loop(void *arg) {
	sleep(2);
	while(1) {
		usleep(1000);
		int a = audio_fifo_available(&g_gaplessfifo);
		if (a < 2000) continue;
		int a2 = audio_fifo_available(&g_staticfifo1);
		if (a2 > 5000) continue;
		struct tunerstate tunerstate;
		tuner_getstate(g_tuner, &tunerstate);
		static_setvolume(g_static1, tunerstate.static_volume);
		static_generate(g_static1, &g_gaplessfifo, &g_staticfifo1, 1);
	}
}

void static2loop(void *arg) {
	while(1) {
		usleep(1000);
		int a = audio_fifo_available(&g_staticfifo1);
		if (a < 2000)	continue;
		int a2 = audio_fifo_available(&g_staticfifo2);
		if (a2 > 5000) continue;
		struct tunerstate tunerstate;
		tuner_getstate(g_tuner, &tunerstate);
		static_setvolume(g_static2, tunerstate.static2_volume);
		static_generate(g_static2, &g_staticfifo1, &g_staticfifo2, 0);
	}
}

void static3loop(void *arg) {
	while(1) {
		usleep(1000);
		int a = audio_fifo_available(&g_staticfifo2);
		if (a < 2000)	continue;
		int a2 = audio_fifo_available(&g_audiofifo);
		if (a2 > 5000) continue;
		struct tunerstate tunerstate;
		tuner_getstate(g_tuner, &tunerstate);
		static_setvolume(g_static3, tunerstate.static3_volume);
		static_generate(g_static3, &g_staticfifo2, &g_audiofifo, 0);
	}
}


int main(int argc, char **argv)
{
	sp_session *sp;
	sp_error err;
	int next_timeout = 0;
	const char *username = NULL;
	const char *password = NULL;
	const char *serialdevice = NULL;
	int opt;

	mtrace();

	while ((opt = getopt(argc, argv, "u:p:s:")) != EOF) {
		switch (opt) {
		case 'u':
			username = optarg;
			break;

		case 'p':
			password = optarg;
			break;

		case 's':
			serialdevice = optarg;
			break;

		default:
			exit(1);
		}
	}

	if (!username || !password) {
		usage(basename(argv[0]));
		exit(1);
	}

	srand(time(NULL));

	audio_fifo_reset(&g_musicfifo);
	audio_fifo_reset(&g_gaplessfifo);
	audio_fifo_reset(&g_staticfifo1);
	audio_fifo_reset(&g_staticfifo2);
	audio_fifo_reset(&g_audiofifo);
	
	printf("music fifo: %X\n", &g_musicfifo);
	printf("gapless fifo: %X\n", &g_gaplessfifo);
	printf("static1 fifo: %X\n", &g_staticfifo1);
	printf("static2 fifo: %X\n", &g_staticfifo2);
	printf("audio fifo: %X\n", &g_audiofifo);

	hardware_start(serialdevice);

	hardware_banner("welcome.", 200);
	hardware_set_callback(_hardware_event);

	g_static1 = static_init(BROWN_NOISE, "static1.raw");
	g_static2 = static_init(PINK_NOISE, "static2.raw");
	g_static3 = static_init(WHITE_NOISE, "static3.raw");

	g_tuner = tuner_init();
	tuner_addchannel(g_tuner, 130 + rand()%100, "Channel 1", "spotify:user:possan:playlist:4g17smZvFZqg4dN74XMBYH");
	tuner_addchannel(g_tuner, 410 + rand()%100, "Channel 2", "spotify:user:possan:playlist:2BBVnBjG4Cynww1mnjfV0v");
	tuner_addchannel(g_tuner, 700 + rand()%100, "Channel 3", "spotify:user:possan:playlist:72weZVptgKfYFyzafxBdO5");
	tuner_goto(g_tuner, rand() % tuner_numchannels(g_tuner));
	tuner_tune_by(g_tuner, -70 + rand()%150);

	printf("Start loop...\n");

	spconfig.application_key_size = g_appkey_size;

	audio_init(&g_audiofifo);
	// audio_init(&g_gaplessfifo);

	err = sp_session_create(&spconfig, &sp);

	if (SP_ERROR_OK != err) {
		fprintf(stderr, "Unable to create session: %s\n", sp_error_message(err));
		exit(1);
	}

	g_sess = sp;

	pthread_mutex_init(&g_notify_mutex, NULL);
	pthread_cond_init(&g_notify_cond, NULL);

	sp_session_login(sp, username, password, 0, NULL);
	pthread_mutex_lock(&g_notify_mutex);

	static pthread_t id;
	pthread_create(&id, NULL, inputloop, NULL);

	static pthread_t id2;
	pthread_create(&id2, NULL, gaplessloop, NULL);

	static pthread_t id3;
	pthread_create(&id3, NULL, static1loop, NULL);

	static pthread_t id4;
	pthread_create(&id4, NULL, static2loop, NULL);

	static pthread_t id5;
	pthread_create(&id5, NULL, static3loop, NULL);

	for (;;) {
		// printf("In loop\n");
		if (next_timeout == 0) {
			while(!g_notify_do) {
				pthread_cond_wait(&g_notify_cond, &g_notify_mutex);
			}
		} else {
			struct timespec ts;

#if _POSIX_TIMERS > 0
			clock_gettime(CLOCK_REALTIME, &ts);
#else
			struct timeval tv;
			gettimeofday(&tv, NULL);
			TIMEVAL_TO_TIMESPEC(&tv, &ts);
#endif
			ts.tv_sec += next_timeout / 1000;
			ts.tv_nsec += (next_timeout % 1000) * 1000000;
			pthread_cond_timedwait(&g_notify_cond, &g_notify_mutex, &ts);
		}

		g_notify_do = 0;
		pthread_mutex_unlock(&g_notify_mutex);

		if (g_playback_done) {
			track_ended(1);
			g_playback_done = 0;
		}

		do {
			sp_session_process_events(sp, &next_timeout);
		} while (next_timeout == 0);

		pthread_mutex_lock(&g_notify_mutex);
	}

	return 0;
}
