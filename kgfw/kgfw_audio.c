#include "kgfw_audio.h"
#include "kgfw_log.h"
#include "kwav/kwav.h"
#include "koml/koml.h"
#include "kgfw_hash.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef KGFW_APPLE
#include <OpenAL/OpenAL.h>
#else
#include <AL/al.h>
#include <AL/alc.h>
#endif

#define SOURCE_NUM 256

struct {
	ALCdevice * device;
	ALCcontext * context;
	struct {
		ALuint so[SOURCE_NUM];
		unsigned long long int buffers[SOURCE_NUM];
	} sources;
	struct {
		ALfloat orientation[6];
		ALfloat pos[3];
	} listener;
	struct {
		ALuint * bo;
		kgfw_hash_t * names;
		unsigned long long int length;
	} buffers;
} static state;

#define AL_ERROR_CHECK(ret) { ALCenum error = alGetError(); if (error != AL_NO_ERROR) { kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "[OpenAL] error %i %x at (%s:%u)", error, error, __FILE__, __LINE__); return ret; } }
#define AL_ERROR_CHECK_VOID() { ALCenum error = alGetError(); if (error != AL_NO_ERROR) { kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "[OpenAL] error %i %x at (%s:%u)", error, error, __FILE__, __LINE__); return; } }
#define AL_ERROR_CHECK_NO_RETURN() { ALCenum error = alGetError(); if (error != AL_NO_ERROR) { kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "[OpenAL] error %i %x at (%s:%u)", error, error, __FILE__, __LINE__); } }

int kgfw_audio_init(void) {
	memset(&state, 0, sizeof(state));
	state.device = alcOpenDevice(alcGetString(NULL, ALC_DEVICE_SPECIFIER));
	if (state.device == NULL) {
		return 1;
	}
	state.context = alcCreateContext(state.device, NULL);
	if (state.context == NULL) {
		return 1;
	}
	alGetError();
	if (!alcMakeContextCurrent(state.context)) {
		kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "failed to create OpenAL context");
		return 1;
	}
	AL_ERROR_CHECK(1);
	
	alGenSources(SOURCE_NUM, state.sources.so);
	AL_ERROR_CHECK(2);

	state.listener.orientation[0] = 0; state.listener.orientation[1] = 0; state.listener.orientation[2] = 0; state.listener.orientation[3] = 0; state.listener.orientation[4] = 0; state.listener.orientation[5] = 0;
	state.listener.pos[0] = 0; state.listener.pos[1] = 110; state.listener.pos[2] = 0;

	alListenerfv(AL_ORIENTATION, state.listener.orientation);
	AL_ERROR_CHECK(3);
	alListener3f(AL_VELOCITY, 0, 0, 0);
	AL_ERROR_CHECK(3);

	struct {
		ALvoid * buffer;
		ALsizei size;
	} file = {
			NULL, 0
	};
	{
		FILE * fp = fopen("./assets/config.koml", "rb");
		if (fp == NULL) {
			kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "failed to open \"config.koml\"");
			return 1;
		}

		fseek(fp, 0L, SEEK_END);
		file.size = ftell(fp);
		fseek(fp, 0L, SEEK_SET);

		file.buffer = malloc(file.size);
		if (file.buffer == NULL) {
			kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "failed to alloc buffer for \"config.koml\"");
			return 1;
		}

		if (fread(file.buffer, 1, file.size, fp) != file.size) {
			kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "failed to read from \"config.koml\"");
			return 1;
		}

		fclose(fp);
	}

	koml_table_t ktable;
	if (koml_table_load(&ktable, file.buffer, file.size) != 0) {
		kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "failed to load koml table from \"config.koml\"");
		return 1;
	}

	koml_symbol_t * files = koml_table_symbol(&ktable, "audio:files");
	koml_symbol_t * names = koml_table_symbol(&ktable, "audio:names");
	state.buffers.length = 0;
	if (files != NULL && files->type == KOML_TYPE_ARRAY && files->data.array.type == KOML_TYPE_STRING) {
		state.buffers.length = files->data.array.length;
		state.buffers.bo = malloc(sizeof(ALuint) * files->data.array.length);
		if (state.buffers.bo == NULL) {
			kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "failed to alloc audio buffers");
			return 1;
		}
		state.buffers.names = malloc(sizeof(kgfw_hash_t) * files->data.array.length);
		if (state.buffers.names == NULL) {
			kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "failed to alloc audio buffers");
			return 1;
		}

		alGenBuffers(files->data.array.length, state.buffers.bo);
		AL_ERROR_CHECK(5);

		kwav_t kwav = {
			.data = NULL,
		};
		for (unsigned long long int i = 0; i < files->data.array.length; ++i) {
			if (names != NULL) {
				state.buffers.names[i] = kgfw_hash(names->data.array.elements.string[i]);
			} else {
				state.buffers.names[i] = kgfw_hash(files->data.array.elements.string[i]);
				kgfw_logf(KGFW_LOG_SEVERITY_WARN, "audio file %s has no given name, assuming file name", files->data.array.elements.string[i]);
			}

			{
				FILE * fp = fopen(files->data.array.elements.string[i], "rb");
				if (fp == NULL) {
					kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "failed to open \"%s\"", files->data.array.elements.string[i]);
					return 1;
				}

				fseek(fp, 0L, SEEK_END);
				file.size = ftell(fp);
				fseek(fp, 0L, SEEK_SET);

				file.buffer = malloc(file.size);
				if (file.buffer == NULL) {
					kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "failed to alloc buffer for \"%s\"", files->data.array.elements.string[i]);
					return 1;
				}

				if (fread(file.buffer, 1, file.size, fp) != file.size) {
					kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "failed to read from \"%s\"", files->data.array.elements.string[i]);
					return 1;
				}

				fclose(fp);
			}

			kwav.data = NULL;
			if (kwav_load(&kwav, file.buffer, file.size) != 0) {
				kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "wav loading failed");
				return 1;
			}
			kwav.data = malloc(kwav.header.datasize);
			if (kwav.data == NULL) {
				return 1;
			}
			if (kwav_load(&kwav, file.buffer, file.size) != 0) {
				kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "wav loading failed");
				return 1;
			}

			ALenum format = AL_FALSE;
			switch (kwav.header.channels) {
				case 1:
					if (kwav.header.bits == 8) {
						format = AL_FORMAT_MONO8;
						break;
					}
					if (kwav.header.bits == 16) {
						format = AL_FORMAT_MONO16;
						break;
					}
				case 2:
					if (kwav.header.bits == 8) {
						format = AL_FORMAT_STEREO8;
						break;
					}
					if (kwav.header.bits == 16) {
						format = AL_FORMAT_STEREO16;
						break;
					}
				default:
					kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "invalid channel/bits per sample values for \"%s\"", files->data.array.elements.string[i]);
					break;
			}

			alBufferData(state.buffers.bo[i], format, kwav.data, kwav.header.datasize, kwav.header.rate);
			AL_ERROR_CHECK(6);
			free(file.buffer);
			free(kwav.data);
		}
	} else {
		//kgfw_logf(KGFW_LOG_SEVERITY_WARN, "no config.koml with audio files");
	}

	return 0;
}

int kgfw_audio_load(char * filename, char * name) {
	kgfw_hash_t hash = kgfw_hash(name);
	for (unsigned long long int i = 0; i < state.buffers.length; ++i) {
		if (state.buffers.names[i] == hash) {
			return -1;
		}
	}

	++state.buffers.length;
	if (state.buffers.bo == NULL) {
		state.buffers.bo = malloc(sizeof(ALuint) * state.buffers.length);
		if (state.buffers.bo == NULL) {
			return 1;
		}
	} else {
		ALuint * p = realloc(state.buffers.bo, sizeof(ALuint) * state.buffers.length);
		if (p == NULL) {
			return 2;
		}
		state.buffers.bo = p;
	}

	if (state.buffers.names == NULL) {
		state.buffers.names = malloc(sizeof(kgfw_hash_t) * state.buffers.length);
		if (state.buffers.names == NULL) {
			return 3;
		}
	} else {
		kgfw_hash_t * p = realloc(state.buffers.names, sizeof(kgfw_hash_t) * state.buffers.length);
		if (p == NULL) {
			return 4;
		}
		state.buffers.names = p;
	}

	alGenBuffers(1, &state.buffers.bo[state.buffers.length - 1]);
	AL_ERROR_CHECK(4);
	state.buffers.names[state.buffers.length - 1] = hash;

	struct {
		ALvoid * buffer;
		ALsizei size;
	} file = {
			NULL, 0
	};
	{
		FILE * fp = fopen(filename, "rb");
		if (fp == NULL) {
			return 5;
		}

		fseek(fp, 0L, SEEK_END);
		file.size = ftell(fp);
		fseek(fp, 0L, SEEK_SET);

		file.buffer = malloc(file.size);
		if (file.buffer == NULL) {
			return 6;
		}

		if (fread(file.buffer, 1, file.size, fp) != file.size) {
			return 7;
		}

		fclose(fp);
	}

	kwav_t kwav;
	memset(&kwav, 0, sizeof(kwav));

	if (kwav_load(&kwav, file.buffer, file.size) != 0) {
		return 8;
	}
	kwav.data = malloc(kwav.header.datasize);
	if (kwav.data == NULL) {
		return 9;
	}
	if (kwav_load(&kwav, file.buffer, file.size) != 0) {
		return 10;
	}
	
	ALenum format = AL_FALSE;
	switch (kwav.header.channels) {
		case 1:
			if (kwav.header.bits == 8) {
				format = AL_FORMAT_MONO8;
				break;
			}
			if (kwav.header.bits == 16) {
				format = AL_FORMAT_MONO16;
				break;
			}
		case 2:
			if (kwav.header.bits == 8) {
				format = AL_FORMAT_STEREO8;
				break;
			}
			if (kwav.header.bits == 16) {
				format = AL_FORMAT_STEREO16;
				break;
			}
		default:
			return 11;
	}
	
	kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "0x%x %u %u", format, kwav.header.datasize, kwav.header.rate);
	alBufferData(state.buffers.bo[state.buffers.length - 1], format, kwav.data, kwav.header.datasize, kwav.header.rate);
	AL_ERROR_CHECK(12);
	free(file.buffer);
	free(kwav.data);

	return 0;
}

void kgfw_audio_update(void) {
	alGetError();
	for (unsigned long long int i = 0; i < SOURCE_NUM; ++i) {
		ALint status;
		alGetSourcei(state.sources.so[i], AL_SOURCE_STATE, &status);
		AL_ERROR_CHECK_NO_RETURN();

		if (status != AL_PLAYING) {
			state.sources.buffers[i] = 0;
		}
	}
}

int kgfw_audio_play_sound(char * name, float x, float y, float z, float gain, float pitch, unsigned char loop, unsigned char relative) {
	kgfw_hash_t hash = kgfw_hash(name);
	unsigned long long int bo = 0;
	for (unsigned long long int i = 0; i < state.buffers.length; ++i) {
		if (hash == state.buffers.names[i]) {
			bo = state.buffers.bo[i];
			goto find_available_source;
		}
	}

	return 1;

find_available_source:
	for (unsigned long long int i = 0; i < SOURCE_NUM; ++i) {
		if (state.sources.buffers[i] == 0) {
			state.sources.buffers[i] = bo;
			alSourcei(state.sources.so[i], AL_BUFFER, bo);
			AL_ERROR_CHECK(3);
			alSourcef(state.sources.so[i], AL_PITCH, pitch);
			AL_ERROR_CHECK(3);
			alSourcef(state.sources.so[i], AL_GAIN, gain);
			AL_ERROR_CHECK(3);
			alSourcei(state.sources.so[i], AL_SOURCE_RELATIVE, relative);
			AL_ERROR_CHECK(3);
			alSource3f(state.sources.so[i], AL_POSITION, x, y, z);
			AL_ERROR_CHECK(3);
			alSource3f(state.sources.so[i], AL_VELOCITY, 0, 0, 0);
			AL_ERROR_CHECK(3);
			alSourcei(state.sources.so[i], AL_LOOPING, loop);
			AL_ERROR_CHECK(3);
			alSourcePlay(state.sources.so[i]);
			AL_ERROR_CHECK(4);
			return 0;
		}
	}

	return 2;
}

void kgfw_audio_deinit(void) {
	alSourcePausev(SOURCE_NUM, state.sources.so);
	alDeleteBuffers(state.buffers.length, state.buffers.bo);
	free(state.buffers.bo);
	free(state.buffers.names);
	alDeleteSources(SOURCE_NUM, state.sources.so);
	alcMakeContextCurrent(NULL);
	alcDestroyContext(state.context);
	alcCloseDevice(state.device);
}
