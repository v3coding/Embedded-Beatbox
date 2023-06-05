#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <alsa/asoundlib.h>
#include "audioMixer_template.h"
#include <stdatomic.h>


#define SOURCE_FILE "wave-files/100060__menegass__gui-drum-splash-hard.wav"
#define SAMPLE_RATE   44100
#define NUM_CHANNELS  1
#define SAMPLE_SIZE   (sizeof(short)) 	// bytes per sample
snd_pcm_t *Audio_openDevice();
void Audio_playFile(snd_pcm_t *handle, wavedata_t *pWaveData);

typedef struct threadController{
    //Hit on X
    atomic_int hitX;
    //Hit on Y
    atomic_int hitY;
    //Hit on Z
    atomic_int hitZ;
    //boolean int to control startup and shutdown of threads
    int programRunning;
    //array of thread ID's
    pthread_t* threadIDs;
    //i2c file desc
    int i2cFileDesc;
    //drum mode
    int mode;
    //audio volume
    int volume;
    //tempo
    int tempo;
    // play sounds individually
    atomic_int playsound1;
    atomic_int playsound2;
    atomic_int playsound3;
    atomic_int playsound4;
    atomic_int playsound5;
    atomic_int playsound6;
} threadController;

void startProgram(threadController* threadArgument);

void waitForProgramEnd(threadController* threadArgument);

void* playSound(void* args);

void* monitorAccelerometer(void* args);

void* printData(void* args);