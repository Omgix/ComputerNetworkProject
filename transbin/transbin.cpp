#include "transbin.h"
#include <cstdio>

int select_device()
{
	int numDevices;
	numDevices = Pa_GetDeviceCount();
	if (numDevices < 0)
	{
		printf("ERROR: Pa_CountDevices returned 0x%x\n", numDevices);
		PaError err = numDevices;
		Pa_Terminate();
		printf("An error occurred while using the portaudio stream\n");
		printf("Error number: %d\n", err);
		printf("Error message: %s\n", Pa_GetErrorText(err));
	}

	printf("List of the devices in the host:\n");
	const PaDeviceInfo *deviceInfo;
	for (int i = 0; i < numDevices; i++)
	{
		deviceInfo = Pa_GetDeviceInfo(i);
		printf("Device #%d: %s\n", i, deviceInfo->name);
	}
	printf("\n");
	int deviceNo;
	scanf("%d", &deviceNo);
	return deviceNo;
}

sf_count_t DataSent::write_samples_to_file(const char *path)
{
	int format = SF_FORMAT_WAV | SF_FORMAT_FLOAT;
	int channels = NUM_CHANNELS;
	int srate = SAMPLE_RATE;
	SndfileHandle file = SndfileHandle(path, SFM_WRITE, format, channels, srate);
	file.writeSync();

	return file.write(samples, numFileFrames);
}

void DataSent::writeThreshold()
{
#ifdef WRITE_TO_FILE

	unsigned long start = LENGTH_PREAMBLE;
	unsigned long end = start + LENGTH_SIGNAL + (BITS_PER_PACKET * SAMPLES_PER_BIT + LENGTH_PREAMBLE) * 2;
	for (unsigned long i = start; i < end; ++i)
	{
		SAMPLE cur = 0.0f;
		for (int j = 0; j < LENGTH_PREAMBLE; ++j)
			cur += samples[j + i] * preamble[j];
		thers[i - start] = abs(cur);
	}
	int format = SF_FORMAT_WAV | SF_FORMAT_FLOAT;
	int channels = NUM_CHANNELS;
	int srate = SAMPLE_RATE;
	SndfileHandle file = SndfileHandle("thres.wav", SFM_WRITE, format, channels, srate);
	file.writeSync();
	sf_count_t n = file.write(thers, end - start);
#endif // WRITE_TO_FILE
}

static int sendCallback(const void *inputBuffer, void *outputBuffer,
						unsigned long framesPerBuffer,
						const PaStreamCallbackTimeInfo *timeInfo,
						PaStreamCallbackFlags statusFlags,
						void* userData)
{
	DataSent *data = (DataSent*)userData;
	SAMPLE* wptr = (SAMPLE*)outputBuffer;
	unsigned &index = data->packetFrameIndex;
	unsigned long &fileFrameIndex = data->fileFrameIndex;

	(void)inputBuffer;
	(void)timeInfo;
	(void)statusFlags;

	/* Stage of signal before sending data. */
	if (data->status == DataSent::sendingStatus::SIGNAL) 
	{
		for (unsigned i = 0; i < framesPerBuffer; ++i) 
		{
			if (index < LENGTH_PREAMBLE)
			{
				*wptr++ = preamble[index];
				samples[fileFrameIndex++] = preamble[index];
			}
			else 
			{
				*wptr++ = SAMPLE_SILENCE;
				samples[fileFrameIndex++] = SAMPLE_SILENCE;
			}
			index++;
		}
		
		if (index >= LENGTH_SIGNAL)
		{
			data->status = DataSent::sendingStatus::CONTENT;
			data->prepare_for_new_sending();
		}
		return paContinue;
	}
	else
	{
		unsigned long leftFrames = data->numFileFrames - fileFrameIndex;
		unsigned long framesCalc;
		int finished;

		if (leftFrames <= framesPerBuffer)
		{
			framesCalc = leftFrames;
			finished = paComplete;
		}
		else
		{
			framesCalc = framesPerBuffer;
			finished = paContinue;
		}

		if (wptr == nullptr)
			return paComplete;

		/* Use 2-PSK to modulate */
		for (unsigned i = 0; i < framesCalc; ++i)
		{
			if (data->isNewPacket)
			{
				data->isNewPacket = false;
				data->isPreamble = true;
			}
			if (data->isPreamble)
			{
				/* If in the process of generating preamble of 512 samples, 11.6ms */
				*wptr++ = preamble[index];
				samples[fileFrameIndex++] = preamble[index];
				if (++index >= LENGTH_PREAMBLE)
				{
					index = 0;
					data->isPreamble = false;
				}
			}
			else
			{
				/* If in the process of Generating data signal */
				//assert(index == data->packetFrameIndex);
				//assert(fileFrameIndex - LENGTH_PREAMBLE - LENGTH_SIGNAL == index);
				unsigned phase = (index++) % (SAMPLE_RATE / FREQUENCY_CARRIER);
				float frame = A * sine[phase];
				int offset = data->bitIndex % BITS_PER_BYTE;
				int indexBytes = data->bitIndex / BITS_PER_BYTE;
				uint8_t bit = (bytes[indexBytes] & (1 << offset)) >> offset;
				if ( bit == 0 )
					frame *= -1.0f;
				samples[fileFrameIndex++] = frame;
				*wptr++ = frame;
				if (index % SAMPLES_PER_BIT == 0)
					data->bitIndex++;
				if (index >= SAMPLES_PER_BIT * BITS_PER_PACKET)
					data->prepare_for_new_sending();
			}
		}

		return finished;
	}
}

DataSent readFileSent(const char *path)
{
	FILE *file = fopen(path, "r");

	if (!file)
	{
		fputs("Invalid file that will be sent!\n", stderr);
		exit(-1);
	}

	fseek(file, 0, SEEK_END);
	int size = ftell(file);
	rewind(file);
	DataSent data(size);
	fread(bytes, sizeof(uint8_t), size, file);

	fclose(file);
	return data;
}

size_t DataReceived::write_to_file(const char* path)
{
	unsigned n = 0;
	FILE *file = fopen(path, "w");
	
	if (!file)
	{
		fputs("Invalid file!\n", stderr);
		exit(-1);
	}
	n = fwrite(bytes, sizeof(uint8_t), bitsReceived / BITS_PER_BYTE, file);
	fclose(file);
	return n;
}

size_t DataReceived::write_samples_to_file(const char* path)
{
	int format = SF_FORMAT_WAV | SF_FORMAT_FLOAT;
	int channels = NUM_CHANNELS;
	int srate = SAMPLE_RATE;
	SndfileHandle file = SndfileHandle(path, SFM_WRITE, format, channels, srate);
	file.writeSync();
	return file.write(samples + startFrom, frameIndex - startFrom);
}

void DataReceived::correct_threshold()
{
	SAMPLE cur = 0.0f;

	for (unsigned i = 0; i < LENGTH_PREAMBLE; ++i)
		cur += samples[frameIndex - i] * preamble[LENGTH_PREAMBLE - 1 - i];
	if (abs(cur) > threshold)
		threshold = abs(cur);
}

bool DataReceived::correlate_next()
{
	SAMPLE cur = 0.0f;

	for (unsigned i = 0; i < LENGTH_PREAMBLE; ++i)
		cur += samples[frameIndex - LENGTH_PREAMBLE+1 + i] * preamble[i];
	return abs(cur) >= threshold ? true : false;
}

void DataReceived::writeThreshold()
{
	printf("threshold: %.8f\n", threshold);
	unsigned long start = startFrom - LENGTH_PREAMBLE - LENGTH_SIGNAL;
	unsigned long end = numSamplesReceived;
	for (unsigned long i = start; i < end; ++i)
	{
		SAMPLE cur = 0.0f;
		for (unsigned j = 0; j < LENGTH_PREAMBLE; ++j)
			cur += samples[i - j] * preamble[LENGTH_PREAMBLE - 1 - j];
		thers[i] = abs(cur);
	}
	int format = SF_FORMAT_WAV | SF_FORMAT_FLOAT;
	int channels = NUM_CHANNELS;
	int srate = SAMPLE_RATE;
	SndfileHandle file = SndfileHandle("thres2.wav", SFM_WRITE, format, channels, srate);
	file.writeSync();
	sf_count_t n = file.write(thers + startFrom, end - startFrom);
	SndfileHandle file2 = SndfileHandle("ampl2.wav", SFM_WRITE, format, channels, srate);
	file2.writeSync();
	sf_count_t n2 = file2.write(ampl + startFrom, end - startFrom);
}

bool DataReceived::demodulate()
{
	while (frameIndex < numSamplesReceived)
	{
		if (frameIndex == INITIAL_INDEX_RX)
		{
			for (int i = 0; i < LENGTH_PREAMBLE; ++i)
				amplitude += abs(samples[frameIndex - i]);
			THRESHOLD_SILENCE = 4;// amplitude * 5;
			printf("THRESHOLD_SILENCE: %f\n", THRESHOLD_SILENCE);
		}
		else
		{
			amplitude += abs(samples[frameIndex]);
			amplitude -= abs(samples[frameIndex-LENGTH_PREAMBLE]);
		}
		ampl[frameIndex] = amplitude;
		if (status == RECEIVING && !in) {
			if (first)
			{
				printf("\n================== Start receiving! =====================\n");
				printf("Start from frame #%u\n", frameIndex);
				startFrom = frameIndex - LENGTH_SIGNAL - LENGTH_PREAMBLE;
				first = false;
			}
			printf("New packet start from frame #%u\n", frameIndex);
			in = true;
		}
		if (status == WAITING || status == ASSURING)
		{
			if (status == ASSURING)
				correct_threshold();
			frameIndex++;
			packetFrameIndex++;
			if (status == WAITING && amplitude > THRESHOLD_SILENCE)
			{
				status = ASSURING;
				packetFrameIndex = 0;
			}
			else if (status == ASSURING)
			{
				if (packetFrameIndex > LENGTH_SIGNAL)
				{
					threshold *= CONSTANT_THRESHOLD;
					prepare_for_receiving();
				}
			}
		}
		else if (status == DETECTING)
		{
			packetFrameIndex++;

			bool getHeader = correlate_next();
			frameIndex++;
			if (getHeader)
			{
				status = RECEIVING;
				packetFrameIndex = 0;
			}
		}
		else if (status == RECEIVING)
		{
			//printf("pac index: %u, total index: %u", packetFrameIndex, frameIndex - startFrom);
			//assert(packetFrameIndex == frameIndex - LENGTH_PREAMBLE - LENGTH_SIGNAL - startFrom);
			SAMPLE value = samples[frameIndex];
			unsigned phase = packetFrameIndex % (SAMPLE_RATE / FREQUENCY_CARRIER);
			demodulated[frameIndex] = value * sine[phase];
			packetFrameIndex++;

			if (packetFrameIndex % SAMPLES_PER_BIT == 0)
			{
				float value = 0.0f;
				for (int i = SAMPLES_PER_BIT-1; i >= 0; --i)
				{
					//printf("\t%.8f    %.8f\n", samples[frameIndex - i], demodulated[frameIndex - i]);
					value += demodulated[frameIndex - i];
				}
				//printf("bit %u: %f\n", numBitsReceived, value);
				int indexByte = bitsReceived / BITS_PER_BYTE;
				int offset = bitsReceived % BITS_PER_BYTE;
				if (value > THRESHOLD_DEMODULATE)
					bytes[indexByte] = bytes[indexByte] | (1<<offset);
				bitsReceived++;
			}
			frameIndex++;
			if (packetFrameIndex >= SAMPLES_PER_BIT * BITS_PER_PACKET || bitsReceived == 10000)
			{
				if (bitsReceived == 10000)
					return true;
				else
					prepare_for_new_packet();
			}
		}
	}
	return false;
}

static int receiveCallback(	const void *inputBuffer, void *outputBuffer,
							unsigned long framesPerBuffer,
							const PaStreamCallbackTimeInfo *timeInfo,
							PaStreamCallbackFlags statusFlags,
							void* userData)
{
	DataReceived *data = (DataReceived*)userData;
	const SAMPLE* rptr = (const SAMPLE*)inputBuffer;

	(void)outputBuffer; /* Prevent unused variable warnings. */
	(void)timeInfo;
	(void)statusFlags;
	(void)userData;
	unsigned long i;
	int finished;
	unsigned long framesCalc;
	if (data->maxNumFrames - data->numSamplesReceived <= framesPerBuffer)
	{
		finished = paComplete;
		framesCalc = data->maxNumFrames - data->numSamplesReceived;
	}
	else
	{
		finished = paContinue;
		framesCalc = framesPerBuffer;
	}

	if (inputBuffer == NULL)
	{
		for (i = 0; i < framesCalc; i++)
			samples[data->numSamplesReceived++] = SAMPLE_SILENCE;
	}
	else
	{
		for (i = 0; i < framesCalc; i++)
			samples[data->numSamplesReceived++] = (*rptr++) * CONSTANT_AMPLIFY;
	}
	return finished;
}

int main(int argc, char const *argv[])
{
	PaStreamParameters	inputParameters,
						outputParameters;
	PaStream*			stream;
	PaError				err;

	if (argc!= 2 && argc != 3 && argc != 4)
	{
		printf("Usage:transmit_bit.exe -option file_path\n"
			"\toption: s: send, r: receive\n");
		return 0;
	}

	const char op1[] = "-s";
	const char op2[] = "-r";
	if (strcmp(op1, argv[1]) == 0) 
	{
		DataSent data = readFileSent(argv[2]);
		
		printf("============ You select option sending. =================\n\n");
		err = Pa_Initialize();
		if (err != paNoError) goto done;

		/*#################### Send data. ########################## */
		outputParameters.device = select_device();
		outputParameters.channelCount = NUM_CHANNELS;
		outputParameters.sampleFormat = PA_SAMPLE_TYPE;
		outputParameters.suggestedLatency = Pa_GetDeviceInfo(outputParameters.device)->defaultLowInputLatency;
		outputParameters.hostApiSpecificStreamInfo = nullptr;
		data.fileFrameIndex = 0;

		printf("\n====================== Now sending, please wait. ========================\n"); fflush(stdout);
		err = Pa_OpenStream(
			&stream,
			nullptr, /* no input */
			&outputParameters,
			SAMPLE_RATE,
			FRAMES_PER_BUFFER,
			paClipOff,
			sendCallback,
			&data);
		if (err != paNoError) goto done;

		if (stream)
		{
			err = Pa_StartStream(stream);
			if (err != paNoError) goto done;

			printf("Waiting for sending to finish.\n"); fflush(stdout);

			while ((err = Pa_IsStreamActive(stream)) == 1) Pa_Sleep(100);
			if (err < 0) goto done;

			err = Pa_CloseStream(stream);
			if (err != paNoError) goto done;

			printf("\n######################### Sending has been done. ########################\n"); fflush(stdout);
			size_t n;
			if (argc == 4)
				n = data.write_samples_to_file(argv[3]);
			else
				n = data.write_samples_to_file("send_samples.wav");
			data.writeThreshold();
			printf("\n---Writing samples sent is finished, %u samples have been write to in total.---\n", n); fflush(stdout);
		}
	}
	else if (strcmp(op2, argv[1]) == 0)
	{
		DataReceived data = DataReceived(1000000);

		printf("\n============ You select option receiving. =================\n\n");
		err = Pa_Initialize();
		if (err != paNoError) goto done;

		inputParameters.device = select_device();
		inputParameters.channelCount = NUM_CHANNELS;
		inputParameters.sampleFormat = PA_SAMPLE_TYPE;
		inputParameters.suggestedLatency = Pa_GetDeviceInfo(inputParameters.device)->defaultLowInputLatency;
		inputParameters.hostApiSpecificStreamInfo = nullptr;

		err = Pa_OpenStream(&stream,
			&inputParameters,
			nullptr,
			SAMPLE_RATE,
			FRAMES_PER_BUFFER,
			paClipOff,
			receiveCallback,
			&data);
		if (err != paNoError) goto done;

		printf("\n===================== Now receiving!! Please wait. =====================\n"); fflush(stdout);
		err = Pa_StartStream(stream);
		if (err < paNoError) goto done;

		Pa_Sleep(2000);
		while ((err = Pa_IsStreamActive(stream)) == 1) 
		{
			bool finished = data.demodulate();
			if (finished)
				Pa_StopStream(stream);
		}
		
		if (err < 0) goto done;
		if (argc == 2)
		{
			printf("\n#### Receiving is finished!! Now write the data to the file OUTPUT.txt. ####\n"); fflush(stdout);
		}
		else 
		{
			printf("\n#### Receiving is finished!! Now write the data to the file %s. ####\n", argv[2]); fflush(stdout);
		}
		size_t n;
		if (argc == 2)
			n = data.write_to_file("OUTPUT.bin");
		else
			n = data.write_to_file(argv[2]);
		printf("\n------ Writing is finished, %u bytes have been write to in total. ------------\n", n); fflush(stdout);

		err = Pa_CloseStream(stream);
		if (err != paNoError) goto done;

		if (argc == 4)
			n = data.write_samples_to_file(argv[3]);
		else
			n = data.write_samples_to_file("samples_received.wav");
		data.writeThreshold();
		printf("\n---Writing samples recorded is finished, %u samples have been write to in total.---\n", n); fflush(stdout);
	}
	else
	{
		printf("Invalid option argument! Usage:transmit_bit.exe -option file_path\n"
			"\toption: s: send, r: receive\n");
		return 0;
	}

done:
	Pa_Terminate();
	if (err != paNoError)
	{
		printf("An error occurred while using the portaudio stream\n");
		printf("Error number: %d\n", err);
		printf("Error message: %s\n", Pa_GetErrorText(err));
	}
	return err;
}