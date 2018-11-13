#include "pastream.h"
#include <cstdio>

int Stream::error() {
	Pa_Terminate();
	printf("An error occurred while using the portaudio stream\n");
	printf("Error number: %d\n", err);
	printf("Error message: %s\n", Pa_GetErrorText(err));
	printf("Press any key to exit...\n");
	getchar();
	exit(err);
}

PaStreamParameters Stream::set_input_para(int receiveDeviceNo)
{
	PaStreamParameters inputParameters;
	inputParameters.device = receiveDeviceNo;
	if (receiveDeviceNo >= 0)
	{
		inputParameters.channelCount = NUM_CHANNELS;
		inputParameters.sampleFormat = PA_SAMPLE_TYPE;
		inputParameters.suggestedLatency = Pa_GetDeviceInfo(inputParameters.device)->defaultLowInputLatency;
		inputParameters.hostApiSpecificStreamInfo = nullptr;
	}
	return inputParameters;
}

PaStreamParameters Stream::set_output_para(int sendDeviceNo)
{
	PaStreamParameters outputParameters;
	outputParameters.device = sendDeviceNo;
	if (sendDeviceNo >= 0)
	{
		outputParameters.channelCount = NUM_CHANNELS;
		outputParameters.sampleFormat = PA_SAMPLE_TYPE;
		outputParameters.suggestedLatency = Pa_GetDeviceInfo(outputParameters.device)->defaultLowInputLatency;
		outputParameters.hostApiSpecificStreamInfo = nullptr;
	}
	return outputParameters;
}

Stream::Stream(int sendDeviceNo, int receiveDeviceNo) :
			err(!numStream ? Pa_Initialize() : paNoError),
			bufferSize(FRAMES_PER_BUFFER),
			inputParameters(set_input_para(receiveDeviceNo)),
			outputParameters(set_output_para(sendDeviceNo)),
			input_stream(nullptr), output_stream(nullptr)
{
	if (err != paNoError) error();
	numStream++;
}

Stream::~Stream()
{
	close_input_stream();
	close_output_stream();
	numStream--;
	if (!numStream)
		Pa_Terminate();
}

void Stream::open_input_stream(ReceiveData *data, PaStreamCallback callback)
{
	err =  Pa_OpenStream(
			&input_stream,
			&inputParameters,
			nullptr,
			SAMPLE_RATE,
			FRAMES_PER_BUFFER,
			paClipOff,
			callback,
			&data);
	if (err != paNoError) error();
}

void Stream::open_output_stream(SendData *data, PaStreamCallback callback)
{
	err = Pa_OpenStream(
		&output_stream,
		nullptr,
		&outputParameters,
		SAMPLE_RATE,
		FRAMES_PER_BUFFER,
		paClipOff,
		callback,
		data);
	if (err != paNoError) error();
}

void Stream::start_input_stream()
{
	if (input_stream)
	{
		err = Pa_StartStream(input_stream);
		if (err != paNoError) error();
	}
}

void Stream::start_output_stream()
{
	if (output_stream)
	{
		err = Pa_StartStream(output_stream);
		if (err != paNoError) error();
	}
}

void Stream::stop_input_stream()
{
	err = Pa_StopStream(input_stream);
	if (err != paNoError) error();
}

void Stream::stop_output_stream()
{
	err = Pa_StopStream(output_stream);
	if (err != paNoError) error();
}

void Stream::close_input_stream()
{
	err = Pa_CloseStream(input_stream);
	if (err != paNoError) error();
}

void Stream::close_output_stream()
{
	err = Pa_CloseStream(output_stream);
	if (err != paNoError) error();
}

void Stream::list_all()
{
	int numDevices;
	const PaDeviceInfo *deviceInfo;

	numDevices = Pa_GetDeviceCount();
	if (numDevices < 0)
	{
		printf("ERROR: Pa_CountDevices returned 0x%x\n", numDevices);
		err = numDevices;
		error();
	}

	for (int i = 0; i < numDevices; i++)
	{
		deviceInfo = Pa_GetDeviceInfo(i);
		printf("Device #%d: %s\n", i, deviceInfo->name);
	}
}

bool Stream::select_input_device(int device_no)
{
	int numDevices = Pa_GetDeviceCount();
	if (numDevices < 0)
	{
		printf("ERROR: Pa_CountDevices returned 0x%x\n", numDevices);
		err = numDevices;
		error();
	}
	if (device_no >= 0 && device_no < numDevices)
	{
		set_input_para(device_no);
		return true;
	}
	return false;
}

bool Stream::select_output_device(int device_no)
{
	int numDevices = Pa_GetDeviceCount();
	if (numDevices < 0)
	{
		printf("ERROR: Pa_CountDevices returned 0x%x\n", numDevices);
		err = numDevices;
		error();
	}
	if (device_no >= 0 && device_no < numDevices)
	{
		set_output_para(device_no);
		return true;
	}
	return false;
}

void Stream::send(SendData &data, bool writewaves, const char* file_name)
{
	open_output_stream(&data, sendCallback);
	start_output_stream();

	printf("Waiting for sending to finish.\n");
	unsigned total = (data.size * BITS_PER_BYTE) +
		ceil((float)data.size * BITS_PER_BYTE / BITS_CONTENT) * BITS_PER_BYTE * BYTES_CRC;
	while ((err = Pa_IsStreamActive(output_stream)) == 1) 
	{
		printf("% 3.2f%% Completed\r", (float)data.bitIndex * 100 / total); fflush(stdout);
		Pa_Sleep(100);
	}
	printf("% 3.2f%% Completed\n", (float)data.bitIndex * 100 / total);
	if (err < 0) error();
	stop_output_stream();
	printf("###################### Sending has been done. #######################\n"); fflush(stdout);
	if (writewaves)
	{
		printf("\nYou choose get waves sent, now writing\n"); fflush(stdout);
		size_t n = data.write_samples_to_file(file_name);
		printf("\n---Writing waves sent is finished, %u samples have been write to in total.---\n", n); fflush(stdout);
	}
}

void Stream::send(DataCo &data, bool write_sent_waves = false, const char* file_wave_sent, 
				bool write_rec_waves, const char* file_wave_rec)
{
	open_input_stream(&data.receive_data, receiveCallback);
	open_output_stream(&data.send_data, receiveCallback);
	start_input_stream();
	start_output_stream();

	printf("Waiting for sending to finish.\n");

	while ((err = Pa_IsStreamActive(output_stream)) == 1)
		data.receive_data.demodulate();
	if (err < 0) error();
	if (data.send_data.wait)
		printf("LINK ERROR!!\n");
	else
		printf("###################### Sending has been done. #######################\n");
	if (write_sent_waves)
	{
		printf("\nYou choose get waves sent, now writing\n"); fflush(stdout);
		size_t n = data.send_data.write_samples_to_file(file_wave_sent);
		printf("\n---Writing waves sent is finished, %u samples have been write to in total.---\n", n); fflush(stdout);
	}
	if (write_rec_waves)
	{
		printf("\nYou choose get waves received, now writing\n"); fflush(stdout);
		size_t n = data.receive_data.write_samples_to_file(file_wave_rec);
		printf("\n---Writing samples recorded is finished, %u samples have been write to in total.---\n", n); fflush(stdout);
	}
}

void Stream::receive(ReceiveData &data, bool writewaves, const char* file_name)
{
	open_input_stream(&data, receiveCallback);
	start_input_stream();

	printf("\n===================== Now receiving!! Please wait. =====================\n"); fflush(stdout);

	Pa_Sleep(2000);
	while ((err = Pa_IsStreamActive(input_stream)) == 1)
	{
		bool finished = data.demodulate();
		if (finished)
			stop_input_stream();
	}
	printf("\n#### Receiving is finished!! Now write the data to the file OUTPUT.bin. ####\n"); fflush(stdout);
	size_t n = data.write_to_file("OUTPUT.bin");
	printf("\n------ Writing is finished, %u bytes have been write to in total. ------------\n", n); fflush(stdout);
	if (writewaves)
	{
		n = data.write_samples_to_file(file_name);
		printf("\n---Writing samples recorded is finished, %u samples have been write to in total.---\n", n); fflush(stdout);
	}
}

void Stream::receive(DataCo &data, bool write_sent_waves = false, const char* file_wave_sent,
	bool write_rec_waves, const char* file_wave_rec)
{
	open_output_stream(&data.send_data, receiveCallback);
	open_input_stream(&data.receive_data, receiveCallback);
	start_output_stream();
	start_input_stream();

	printf("Waiting for receiving to finish.\n");

	while ((err = Pa_IsStreamActive(input_stream)) == 1)
	{
		bool finished = data.receive_data.demodulate();
		if (finished)
			stop_input_stream();
	}
	if (err < 0) error();
	printf("#### Receiving is finished!! Now write the data to the file OUTPUT.bin. ####\n");
	size_t n = data.receive_data.write_to_file("OUTPUT.bin");
	printf("\n------ Writing is finished, %u bytes have been write to in total. ------------\n", n); fflush(stdout);
	if (write_rec_waves)
	{
		printf("\nYou choose get waves received, now writing\n"); fflush(stdout);
		size_t n = data.receive_data.write_samples_to_file(file_wave_rec);
		printf("\n---Writing samples recorded is finished, %u samples have been write to in total.---\n", n); fflush(stdout);
	}
	if (write_sent_waves)
	{
		printf("\nYou choose get waves sent, now writing\n"); fflush(stdout);
		size_t n = data.send_data.write_samples_to_file(file_wave_sent);
		printf("\n---Writing waves sent is finished, %u samples have been write to in total.---\n", n); fflush(stdout);
	}
}