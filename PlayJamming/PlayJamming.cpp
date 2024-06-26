#include <cstdio>
#include <sndfile.hh>
#include "portaudio.h"
#include <cstdint>

const int SAMPLE_RATE = 48000;
const int MAX_PLAY_TIME = 240;
const int FRAMES_PER_BUFFER = 512;
int16_t buffer[SAMPLE_RATE * MAX_PLAY_TIME];

struct Data
{
	int in_use = 0;
	int index = 0;
};

static int callback(const void *inputBuffer, void *outputBuffer,
	unsigned long framesPerBuffer,
	const PaStreamCallbackTimeInfo* timeInfo,
	PaStreamCallbackFlags statusFlags,
	void *userData)
{
	Data *data = (Data*)userData;
	int16_t *wptr = (int16_t*)outputBuffer;

	(void)timeInfo; /* Prevent unused variable warnings. */
	(void)statusFlags;
	(void)inputBuffer;

	for (unsigned long i = 0; i < framesPerBuffer; ++i)
	{
		*wptr++ = buffer[data->index++];
		if (data->index == SAMPLE_RATE * MAX_PLAY_TIME) return paComplete;
	}
	return paContinue;
}

int main()
{
	PaStreamParameters outputParameters1, outputParameters2;
	PaStream *stream1, *stream2;
	PaError err;
	int device1, device2;
	SndfileHandle jamming = SndfileHandle("Jamming.wav", SFM_READ);
	Data data1, data2;
	int numDevices;
	const PaDeviceInfo *deviceInfo;

	jamming.readf(buffer, SAMPLE_RATE * MAX_PLAY_TIME);

	err = Pa_Initialize();
	if (err != paNoError) goto error;

	numDevices = Pa_GetDeviceCount();
	if (numDevices < 0)
	{
		printf("ERROR: Pa_CountDevices returned 0x%x\n", numDevices);
		err = numDevices;
		goto error;
	}
	printf("List of host devices: \n\n");
	for (int i = 0; i < numDevices; i++)
	{
		deviceInfo = Pa_GetDeviceInfo(i);
		printf("Device #%d: %s\n", i, deviceInfo->name);
	}

	printf("\nPlease select output device #1: ");
	scanf("%d", &device1);
	printf("Please select output device #2: ");
	scanf("%d", &device2);

	outputParameters1.device = device1;
	outputParameters1.channelCount = 1;
	outputParameters1.sampleFormat = paInt16;
	outputParameters1.suggestedLatency = Pa_GetDeviceInfo(outputParameters1.device)->defaultLowInputLatency;
	outputParameters1.hostApiSpecificStreamInfo = nullptr;

	outputParameters2.device = device2;
	outputParameters2.channelCount = 1;
	outputParameters2.sampleFormat = paInt16;
	outputParameters2.suggestedLatency = Pa_GetDeviceInfo(outputParameters2.device)->defaultLowInputLatency;
	outputParameters2.hostApiSpecificStreamInfo = nullptr;

	err = Pa_OpenStream(&stream1, nullptr, &outputParameters1, SAMPLE_RATE,
		FRAMES_PER_BUFFER, paClipOff, callback, &data1);
	if (err != paNoError) goto error;
	err = Pa_OpenStream(&stream2, nullptr, &outputParameters2, SAMPLE_RATE,
		FRAMES_PER_BUFFER, paClipOff, callback, &data2);
	if (err != paNoError) goto error;

	err = Pa_StartStream(stream1);
	if (err != paNoError) goto error;
	err = Pa_StartStream(stream2);
	if (err != paNoError) goto error;

	printf("Play for %d seconds.\n", MAX_PLAY_TIME);
	Pa_Sleep(MAX_PLAY_TIME * 1000);

	err = Pa_StopStream(stream1);
	if (err != paNoError) goto error;
	err = Pa_StopStream(stream2);
	if (err != paNoError) goto error;

	err = Pa_CloseStream(stream1);
	if (err != paNoError) goto error;
	err = Pa_CloseStream(stream2);
	if (err != paNoError) goto error;

	Pa_Terminate();
	printf("Play finished.\n");

	return err;
error:
	Pa_Terminate();
	fprintf(stderr, "An error occured while using the portaudio stream\n");
	fprintf(stderr, "Error number: %d\n", err);
	fprintf(stderr, "Error message: %s\n", Pa_GetErrorText(err));
	system("pause");
	return err;
}