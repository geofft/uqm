/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/* OpenAL specific code by Mika Kolehmainen, 2002-10-23
 */

#ifdef WIN32
#include <al.h>
#include <alc.h>
#pragma comment (lib, "OpenAL32.lib")
#else
#include <AL/al.h>
#include <AL/alc.h>
#endif

#include "starcon.h"
#include "misc.h"
#include "libs/strings/strintrn.h"
#include "libs/sound/sndintrn.h"
#include "libs/sound/sound_common.h"
#include "decoders/decoder.h"


#define FIRST_SFX_SOURCE 0
#define LAST_SFX_SOURCE 4
#define MUSIC_SOURCE (LAST_SFX_SOURCE + 1)
#define SPEECH_SOURCE (MUSIC_SOURCE + 1)
#define NUM_SOUNDSOURCES (SPEECH_SOURCE + 1)

typedef struct
{
	ALuint buf_name;
	int type;
	void *value;
} TFB_SoundTag;

enum
{
	AL_BUFFER_TAG_TEXT = 1,
	AL_BUFFER_TAG_TEXTPAGE
};

typedef struct tfb_soundchain
{
	TFB_SoundDecoder *decoder; // points at the decoder to read from
	float start_time;
	TFB_SoundTag tag;
	struct tfb_soundchain *next;
} TFB_SoundChain;

// audio data
typedef struct tfb_soundsample
{
	TFB_SoundDecoder *decoder; // decoder to read from
	TFB_SoundChain *read_chain_ptr; // points to chain read poistion
	TFB_SoundChain *play_chain_ptr; // points to chain playing position
	float length; // total length of decoder chain in seconds
	ALuint *buffer;
	ALuint num_buffers;
	TFB_SoundTag **buffer_tag;
} TFB_SoundSample;


// equivalent to channel in legacy sound code
typedef struct tfb_soundsource
{
	TFB_SoundSample *sample;
	ALuint handle;
	BOOLEAN stream_should_be_playing;
	Mutex stream_mutex;
	ALuint start_time;

	// for oscilloscope
	void *sbuffer; 
	ALuint sbuf_start;
	ALuint sbuf_size;
	ALuint total_decoded;
} TFB_SoundSource;


extern ALCcontext *alcContext; // OpenAL context
extern ALCdevice *alcDevice; // OpenAL device
extern ALfloat listenerPos[]; // listener position (3 floats)
extern ALfloat listenerVel[]; // listener velocity (3 floats)
extern ALfloat listenerOri[]; // listener orientation (two 3 float vectors, "look at" and "up")
extern TFB_SoundSource soundSource[];

void SetSFXVolume (float volume);
void SetSpeechVolume (float volume);
void DoTrackTag (TFB_SoundTag *tag);

TFB_SoundChain *create_soundchain (TFB_SoundDecoder *decoder, float startTime);
void destroy_soundchain (TFB_SoundChain *chain);
TFB_SoundChain *get_previous_chain (TFB_SoundChain *first_chain, TFB_SoundChain *current_chain);

#include "stream.h"
