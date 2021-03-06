ifeq ($(shell uname),Darwin)
ifdef USE_AUDIOQUEUE
AUDIO_DRIVER ?= osx
LDFLAGS += -framework AudioToolbox
else
AUDIO_DRIVER ?= openal
LDFLAGS += -framework OpenAL
endif
LDFLAGS += -framework libspotify
else
CFLAGS  = $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --cflags alsa)
LDFLAGS = $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --libs-only-L alsa)
LDLIBS  = $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --libs-only-l --libs-only-other alsa)
AUDIO_DRIVER ?= alsa
endif

#AUDIO_DRIVER = dummy

TARGET = jukebox
## TARGET = playtrack
OBJS = $(TARGET).o appkey.o $(AUDIO_DRIVER)-audio.o audio.o tuner.o hardware.o statics.o

$(TARGET): $(OBJS)  
	#$(CC) -ggdb -g3 -p $(CFLAGS) $(LDFLAGS) $^ -o $@ $(LDLIBS)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@ $(LDLIBS)
ifdef DEBUG
ifeq ($(shell uname),Darwin)
#	install_name_tool -change @loader_path/../Frameworks/libspotify.framework/libspotify @rpath/libspotify.so $@
endif
endif

sounds: cleansounds static1.raw static2.raw static3.raw

cleansounds:
	rm *wav
	rm *raw

static1.raw: static1.mp3
	mpg123 -m -w static1.wav static1.mp3
	sox -r 44100  -b 16 -L -c 2 static1.wav static1.raw   

static2.raw: static2.mp3
	mpg123 -m -w static2.wav static2.mp3
	sox -r 44100  -b 16 -L -c 2 static2.wav static2.raw   

static3.raw: static3.mp3
	mpg123 -m -w static3.wav static3.mp3 
	sox -r 44100  -b 16 -L -c 2 static3.wav static3.raw   

include common.mk

audio.o: audio.c audio.h
alsa-audio.o: alsa-audio.c audio.h
dummy-audio.o: dummy-audio.c audio.h
osx-audio.o: osx-audio.c audio.h
openal-audio.o: openal-audio.c audio.h
jukebox.o: jukebox.c audio.h hardware.h tuner.h
playtrack.o: playtrack.c audio.h
tuner.o: tuner.c tuner.h
hardware.o: hardware.c hardware.h
statics.o: statics.c statics.h

