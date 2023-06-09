// Note: Generates low latency audio on BeagleBone Black; higher latency found on host.
#include "audioMixer_template.h"
#include <alsa/asoundlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <limits.h>
#include <alloca.h> // needed for mixer


static snd_pcm_t *handle;

#define DEFAULT_VOLUME 80

#define SAMPLE_RATE 44100
#define NUM_CHANNELS 1
#define SAMPLE_SIZE (sizeof(short)) 			
static unsigned long playbackBufferSize = 0;
static short *playbackBuffer = NULL;
#define MAX_SOUND_BITES 30
typedef struct {
	wavedata_t *pSound;
	int location;
} playbackSound_t;
static playbackSound_t soundBites[MAX_SOUND_BITES];
void* playbackThread(void* arg);
static bool stopping = false;
static pthread_t playbackThreadId;
static pthread_mutex_t audioMutex = PTHREAD_MUTEX_INITIALIZER;
static int volume = 0;

void AudioMixer_init(void)
{
	AudioMixer_setVolume(DEFAULT_VOLUME);
	for(int i = 0; i < MAX_SOUND_BITES; i++){
		soundBites[i].pSound = NULL;
	}
	int err = snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
	if (err < 0) {
		printf("Playback open error: %s\n", snd_strerror(err));
		exit(EXIT_FAILURE);
	}
	err = snd_pcm_set_params(handle,
			SND_PCM_FORMAT_S16_LE,
			SND_PCM_ACCESS_RW_INTERLEAVED,
			NUM_CHANNELS,
			SAMPLE_RATE,
			1,			// Allow software resampling
			50000);		// 0.05 seconds per buffer
	if (err < 0) {
		printf("Playback open error: %s\n", snd_strerror(err));
		exit(EXIT_FAILURE);
	}
 	unsigned long unusedBufferSize = 0;
	snd_pcm_get_params(handle, &unusedBufferSize, &playbackBufferSize);
	playbackBuffer = malloc(playbackBufferSize * sizeof(*playbackBuffer));
	pthread_create(&playbackThreadId, NULL, playbackThread, NULL);
}

void AudioMixer_readWaveFileIntoMemory(char *fileName, wavedata_t *pSound)
{
	assert(pSound);
	const int PCM_DATA_OFFSET = 44;
	FILE *file = fopen(fileName, "r");
	if (file == NULL) {
		fprintf(stderr, "ERROR: Unable to open file %s.\n", fileName);
		exit(EXIT_FAILURE);
	}
	fseek(file, 0, SEEK_END);
	int sizeInBytes = ftell(file) - PCM_DATA_OFFSET;
	pSound->numSamples = sizeInBytes / SAMPLE_SIZE;
	fseek(file, PCM_DATA_OFFSET, SEEK_SET);
	pSound->pData = malloc(sizeInBytes);
	if (pSound->pData == 0) {
		fprintf(stderr, "ERROR: Unable to allocate %d bytes for file %s.\n",
				sizeInBytes, fileName);
		exit(EXIT_FAILURE);
	}
	int samplesRead = fread(pSound->pData, SAMPLE_SIZE, pSound->numSamples, file);
	if (samplesRead != pSound->numSamples) {
		fprintf(stderr, "ERROR: Unable to read %d samples from file %s (read %d).\n",
				pSound->numSamples, fileName, samplesRead);
		exit(EXIT_FAILURE);
	}
}

void AudioMixer_freeWaveFileData(wavedata_t *pSound)
{
	pSound->numSamples = 0;
	free(pSound->pData);
	pSound->pData = NULL;
}

void AudioMixer_queueSound(wavedata_t *pSound)
{
	assert(pSound->numSamples > 0);
	assert(pSound->pData);
	int full = 1;
	int insertingIndex = 0;

	pthread_mutex_lock(&audioMutex);

	for(int i = 0; i < MAX_SOUND_BITES; i++){
		if(soundBites[i].pSound == NULL){
			full = 0;
			insertingIndex = i;
			i = MAX_SOUND_BITES;
		}
	}

	if(full == 0){
		soundBites[insertingIndex].pSound = pSound;
	}

	pthread_mutex_unlock(&audioMutex);

	if(full == 1){
		printf("AudioMixer_queueSound error -- soundBites buffer is full!\n");
		printf("Buffer is sized %d\n", MAX_SOUND_BITES);

		for(int i = 0; i < MAX_SOUND_BITES; i++){
			printf("soundBites %d is at location %d of indices %d\n",i,soundBites[i].location,soundBites[i].pSound->numSamples);
		}
	}
}

void AudioMixer_cleanup(void)
{
	printf("Stopping audio...\n");
	stopping = true;
	pthread_join(playbackThreadId, NULL);
	snd_pcm_drain(handle);
	snd_pcm_close(handle);
	free(playbackBuffer);
	playbackBuffer = NULL;
	printf("Done stopping audio...\n");
	fflush(stdout);
}


int AudioMixer_getVolume()
{
	return volume;
}

// Function copied from:
// http://stackoverflow.com/questions/6787318/set-alsa-master-volume-from-c-code
// Written by user "trenki".
void AudioMixer_setVolume(int newVolume)
{
	if (newVolume < 0 || newVolume > AUDIOMIXER_MAX_VOLUME) {
		printf("ERROR: Volume must be between 0 and 100.\n");
		return;
	}
	volume = newVolume;
    long min, max;
    snd_mixer_t *volHandle;
    snd_mixer_selem_id_t *sid;
    const char *card = "default";
    const char *selem_name = "PCM";
    snd_mixer_open(&volHandle, 0);
    snd_mixer_attach(volHandle, card);
    snd_mixer_selem_register(volHandle, NULL, NULL);
    snd_mixer_load(volHandle);
    snd_mixer_selem_id_alloca(&sid);
    snd_mixer_selem_id_set_index(sid, 0);
    snd_mixer_selem_id_set_name(sid, selem_name);
    snd_mixer_elem_t* elem = snd_mixer_find_selem(volHandle, sid);
    snd_mixer_selem_get_playback_volume_range(elem, &min, &max);
    snd_mixer_selem_set_playback_volume_all(elem, volume * max / 100);
    snd_mixer_close(volHandle);
}


static void fillPlaybackBuffer(short *buff, int size)
{
	for(int i = 0; i < size; i++){
		playbackBuffer[i] = 0;
	}
	pthread_mutex_lock(&audioMutex);
	int playbackBufferIterator = 0;
	int end = 0;
	short bufferVal = 0;
	short soundByteVal = 0;
	short insertingVal = 0;
	int locationIterator = 0;
	for(int i = 0; i < MAX_SOUND_BITES; i++){
		if(soundBites[i].pSound != NULL){
			locationIterator = soundBites[i].location;
			if(soundBites[i].pSound->numSamples - locationIterator > size){
				end = size;
			}
			else{
				end = soundBites[i].pSound->numSamples - locationIterator;
			}
			for(int j = 0; j < end; j++){
				bufferVal = playbackBuffer[playbackBufferIterator];
				soundByteVal = soundBites[i].pSound->pData[locationIterator];	

				if((bufferVal + soundByteVal) > SHRT_MAX){
					insertingVal = SHRT_MAX;
				} else if((bufferVal + soundByteVal) < SHRT_MIN){
					insertingVal = SHRT_MIN;
				} else{
					insertingVal = bufferVal + soundByteVal;
				}
				playbackBuffer[playbackBufferIterator] = insertingVal;
				playbackBufferIterator++;
				soundBites[i].location++;
				locationIterator++;
			}
			playbackBufferIterator = 0;
			insertingVal = 0;
			if((soundBites[i].location) == soundBites[i].pSound->numSamples){
				soundBites[i].location = 0;
				soundBites[i].pSound = NULL;
			}
		}
	}
	pthread_mutex_unlock(&audioMutex);
}

void* playbackThread(void* arg)
{
	while (!stopping) {
		fillPlaybackBuffer(playbackBuffer, playbackBufferSize);
		snd_pcm_sframes_t frames = snd_pcm_writei(handle,
				playbackBuffer, playbackBufferSize);
		if (frames < 0) {
			fprintf(stderr, "AudioMixer: writei() returned %li\n", frames);
			frames = snd_pcm_recover(handle, frames, 1);
		}
		if (frames < 0) {
			fprintf(stderr, "ERROR: Failed writing audio with snd_pcm_writei(): %li\n",
					frames);
			exit(EXIT_FAILURE);
		}
		if (frames > 0 && frames < playbackBufferSize) {
			printf("Short write (expected %li, wrote %li)\n",
					playbackBufferSize, frames);
		}
	}
	return NULL;
}
















