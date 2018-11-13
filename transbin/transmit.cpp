#include "pastream.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include "crc.h"

extern float sine[];
extern float preamble[];

int sendCallback(const void *inputBuffer, void *outputBuffer,
	unsigned long framesPerBuffer,
	const PaStreamCallbackTimeInfo *timeInfo,
	PaStreamCallbackFlags statusFlags,
	void* userData)
{
	SendData *data = (SendData*)userData;
	SAMPLE* wptr = (SAMPLE*)outputBuffer;
	unsigned &index = data->packetFrameIndex;
	unsigned long &fileFrameIndex = data->fileFrameIndex;

	(void)inputBuffer;
	(void)timeInfo;
	(void)statusFlags;

	/* Stage of signal before sending data. */
	if (data->status == SendData::status::SIGNAL)
	{
		for (unsigned i = 0; i < framesPerBuffer; ++i)
		{
			if (index < LEN_PREAMBLE)
			{
				*wptr++ = preamble[index];
				data->samples[fileFrameIndex++] = preamble[index];
			}
			else
			{
				*wptr++ = SAMPLE_SILENCE;
				data->samples[fileFrameIndex++] = SAMPLE_SILENCE;
			}
			index++;
		}

		if (index >= LEN_SIGNAL)
		{
			data->status = SendData::status::CONTENT;
			data->prepare_for_new_sending();
		}
		return paContinue;
	}
	else
	{
		unsigned long leftFrames;
		unsigned long framesCalc;
		int finished;

		leftFrames = data->totalFrames - fileFrameIndex;
		finished = paContinue;
		if (leftFrames <= framesPerBuffer && !data->need_ack && data->mode == TRANSMITTER)
		{
			framesCalc = leftFrames;
			finished = paComplete;
		}
		else
			framesCalc = framesPerBuffer;

		if (wptr == nullptr)
			return paComplete;

		/* Use 2-PSK to modulate */
		for (unsigned i = 0; i < framesCalc; ++i)
		{
			if (data->isNewPacket)
			{
				*wptr++ = SAMPLE_SILENCE;
				if (data->wait)
				{
					*wptr++ = SAMPLE_SILENCE;
					if (data->mode == TRANSMITTER)
					{
						if (clock::now() > data->time_send + ACK_TIME_OUT) // Time out! Retransmit
						{
							data->times_sent++;
							if (data->times_sent > TIMES_TRY)
								return paComplete; // When reach there, wait is true;
							data->wait = false;
						}
						else if (*data->signal) // ACK received
						{
							data->times_sent = 0;
							data->wait = false;
							if (fileFrameIndex == data->totalFrames - 1) // All data has been sent
								return paComplete; // When reach there, wait is false;
						}
					}
					else if (data->mode == RECEIVER)
					{
						if (*data->signal)
							data->wait = false;
					}
					continue;
				}
				data->isNewPacket = false;
				data->isPreamble = true;
			}
			if (data->isPreamble)
			{
				/* If in the process of generating preamble of 512 samples, 11.6ms */
				*wptr++ = preamble[index];
				data->samples[fileFrameIndex++] = preamble[index];
				if (++index >= LEN_PREAMBLE)
				{
					index = 0;
					data->isPreamble = false;
				}
			}
			else
			{
				unsigned phase = (index++) % (SAMPLE_RATE / FREQUENCY_CARRIER);
				float frame = A * sine[phase];
				int offset = data->bitIndex % BITS_PER_BYTE;
				int indexBytes = data->bitIndex / BITS_PER_BYTE;
				uint8_t bit = (data->data[indexBytes] & (1 << offset)) >> offset;
				if (bit == 0)
					frame *= -1.0f;
				data->samples[fileFrameIndex++] = frame;
				*wptr++ = frame;
				if (index % SAMPLES_PER_BIT == 0)
					data->bitIndex++;
				if (index >= SAMPLES_PER_BIT * BITS_NORMALPACKET || fileFrameIndex == data->totalFrames - 1)
				{
					data->prepare_for_new_sending(); // Reset several index after sending a packet
					if (data->need_ack || data->mode == RECEIVER)
					{
						data->time_send = clock::now();
						data->wait = true;
						*data->signal = false;
					}
				}

			}
		}

		return finished;
	}
}

SendData::SendData(const char* file_name, void *data_, SAMPLE *samples_, bool need_ack_, Mode mode_, bool *ack_received_) :
	status(SIGNAL), fileFrameIndex(0), ext_data_ptr(data_ != nullptr), ext_sample_ptr(samples_ != nullptr), mode(mode_),
	bitIndex(0), packetFrameIndex(0), isPreamble(true), isNewPacket(true), need_ack(mode_ == RECEIVER ? true : need_ack_),
	wait(false), size(0), totalFrames(0), data((uint8_t*)data_), samples(samples_), signal(ack_received_),
	times_sent(0)
{
	if (mode == TRANSMITTER)
	{
		FILE *file = fopen(file_name, "rb");

		if (!file)
		{
			fputs("Invalid file that will be sent!\n", stderr);
			exit(-1);
		}

		fseek(file, 0, SEEK_END);
		size = ftell(file);
		rewind(file);
		size_t numPacket = ceil((float)size / BYTES_CONTENT);
		if (!ext_data_ptr)
			data = new uint8_t[size + numPacket * BYTES_CRC];
		uint8_t *ptr = data;
		unsigned remain = size;
		for (int i = 0; i < numPacket; ++i)
		{
			unsigned numRead = remain < BYTES_CONTENT ? remain : BYTES_CONTENT;
			int n = fread(ptr, sizeof(char), numRead, file);
			remain -= numRead;
			crc_t crc = crc_init();
			crc = crc_update(crc, ptr, numRead);
			crc = crc_finalize(crc);
			ptr += numRead;
			memcpy(ptr, &crc, BYTES_CRC);
			ptr += BYTES_CRC;
		}
		fclose(file);
		totalFrames = size * BITS_PER_BYTE * SAMPLES_PER_BIT + LEN_SIGNAL
			+ (LEN_PREAMBLE + BITS_CRC * SAMPLES_PER_BIT) * numPacket;
	}
	else if (mode_ == RECEIVER)
	{
		size = BYTES_ACK_CONTENT;
		if (!ext_data_ptr)
			data = new uint8_t[size + BYTES_CRC];
		crc_t crc = crc_init();
		crc = crc_update(crc, data, size);
		crc = crc_finalize(crc);
		memcpy(data + size, &crc, BYTES_CRC);
		totalFrames = size * BITS_PER_BYTE * SAMPLES_PER_BIT + LEN_SIGNAL
			+ (LEN_PREAMBLE + BITS_CRC * SAMPLES_PER_BIT);
	}
	if (!ext_sample_ptr)
		samples = new SAMPLE[totalFrames];
}

SendData::~SendData()
{
	if (!ext_data_ptr)
		delete[] data;
	if (!ext_sample_ptr)
		delete[] samples;
}

void SendData::prepare_for_new_sending()
{
	packetFrameIndex = 0;
	isNewPacket = true;
	if (mode == RECEIVER)
		fileFrameIndex -= totalFrames - LEN_SIGNAL;
}

sf_count_t SendData::write_samples_to_file(const char *path = nullptr)
{
	int format = SF_FORMAT_WAV | SF_FORMAT_FLOAT;
	int channels = NUM_CHANNELS;
	int srate = SAMPLE_RATE;
	if (!path) path = "sendwaves.wav";
	SndfileHandle file = SndfileHandle(path, SFM_WRITE, format, channels, srate);
	file.writeSync();

	return file.write(samples, totalFrames);
}

int receiveCallback(const void *inputBuffer, void *outputBuffer,
	unsigned long framesPerBuffer,
	const PaStreamCallbackTimeInfo *timeInfo,
	PaStreamCallbackFlags statusFlags,
	void* userData)
{
	ReceiveData *data = (ReceiveData*)userData;
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
			data->samples[data->numSamplesReceived++] = SAMPLE_SILENCE;
	}
	else
	{
		for (i = 0; i < framesCalc; i++)
			data->samples[data->numSamplesReceived++] = (*rptr++) * CONSTANT_AMPLIFY;
	}
	return finished;
}

ReceiveData::ReceiveData(unsigned max_time, void *data_, SAMPLE *samples_,
	bool need_ack_, Mode mode_, bool *ack_received_) :
	status(WAITING), ext_data_ptr(data_ != nullptr), ext_sample_ptr(samples_ != nullptr),
	data(data_ == nullptr ? new uint8_t[max_time*SAMPLE_RATE / SAMPLES_PER_BIT] : (uint8_t *)data_),
	samples(samples_ == nullptr ? new SAMPLE[max_time*SAMPLE_RATE] : samples_),
	packetFrameIndex(0), threshold(0.0f), amplitude(0.0f), packet(new uint8_t*[5]),
	bitsReceived(0), numSamplesReceived(0), maxNumFrames(max_time*SAMPLE_RATE),
	frameIndex(INITIAL_INDEX_RX), startFrom(0), mode(mode_), signal(ack_received_),
	need_ack(mode_ == TRANSMITTER ? true : need_ack_)
{
	for (int i = 0; i < 5; ++i)
		packet[i] = new uint8_t[BYTES_CONTENT + BYTES_CRC];
}

ReceiveData::~ReceiveData()
{
	if (!ext_data_ptr) delete[] data;
	if (!ext_sample_ptr) delete[] samples;
	for (int i = 0; i < 5; ++i)
		delete[] packet[i];
	delete[] packet;
}

void ReceiveData::prepare_for_new_packet()
{
	frameIndex += LEN_PREAMBLE * 3 / 4;
	packetFrameIndex = 0;
	status = DETECTING;
	in = false;
	for (int i = 0; i < 5; ++i)
		memset(packet[i], 0, BYTES_CONTENT + BYTES_CRC);
}

void ReceiveData::prepare_for_receiving()
{
	status = DETECTING;
	packetFrameIndex = 0;
	for (int i = 0; i < 5; ++i)
		memset(packet[i], 0, BYTES_CONTENT + BYTES_CRC);
}

void ReceiveData::correct_threshold()
{
	SAMPLE cur = 0.0f;

	for (unsigned i = 0; i < LEN_PREAMBLE; ++i)
		cur += samples[frameIndex - LEN_PREAMBLE + i] * preamble[i];
	if (abs(cur) > threshold)
		threshold = abs(cur);
}

bool ReceiveData::correlate_next()
{
	SAMPLE cur = 0.0f;

	for (unsigned i = 0; i < LEN_PREAMBLE; ++i)
		cur += samples[frameIndex - LEN_PREAMBLE + 1 + i] * preamble[i];
	return abs(cur) >= threshold ? true : false;
}

size_t ReceiveData::write_to_file(const char* path = nullptr)
{
	unsigned n = 0;
	if (!path) path = "OUTPUT.bin";
	FILE *file = fopen(path, "wb");

	if (!file)
	{
		fputs("Invalid file!\n", stderr);
		exit(-1);
	}
	unsigned num_bytes = (bitsReceived - ceil((float)bitsReceived / BITS_NORMALPACKET) * BITS_CRC) / BITS_PER_BYTE;
	n = fwrite(data, sizeof(uint8_t), num_bytes, file);
	fclose(file);
	return n;
}

size_t ReceiveData::write_samples_to_file(const char* path = nullptr)
{
	int format = SF_FORMAT_WAV | SF_FORMAT_FLOAT;
	int channels = NUM_CHANNELS;
	int srate = SAMPLE_RATE;
	if (!path) path = "reveivewave.wav";
	SndfileHandle file = SndfileHandle(path, SFM_WRITE, format, channels, srate);
	file.writeSync();
	return file.write(samples, frameIndex - startFrom);
}

bool ReceiveData::demodulate()
{
	while (frameIndex + 2 < numSamplesReceived)
	{
		if (frameIndex == INITIAL_INDEX_RX)
		{
			for (int i = 0; i < LEN_PREAMBLE; ++i)
				amplitude += abs(samples[frameIndex - i]);
			THRESHOLD_SILENCE = 4;// amplitude * 5;
		}
		else
		{
			amplitude += abs(samples[frameIndex]);
			amplitude -= abs(samples[frameIndex - LEN_PREAMBLE]);
		}
		if (status == RECEIVING && !in) {
			if (first)
			{
				printf("Start receiving!\n");
				printf("Start from frame #%u\n", frameIndex);
				startFrom = frameIndex - LEN_SIGNAL - LEN_PREAMBLE;
				first = false;
			}
			printf("New packet start from frame #%u\r", frameIndex); fflush(stdout);
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
				if (packetFrameIndex > LEN_SIGNAL)
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
			/*SAMPLE value = samples[frameIndex];
			unsigned phase = packetFrameIndex % (SAMPLE_RATE / FREQUENCY_CARRIER);
			demodulated[frameIndex] = value * sine[phase];*/
			packetFrameIndex++;

			if (packetFrameIndex % SAMPLES_PER_BIT == 0)
			{
				int indexBytePacket = (packetFrameIndex / SAMPLES_PER_BIT - 1) / BITS_PER_BYTE;
				int offset = (packetFrameIndex / SAMPLES_PER_BIT - 1) % BITS_PER_BYTE;
				for (int i = 0; i < 5; ++i)
				{
					float value = 0.0f;
					for (int j = SAMPLES_PER_BIT - 1; j >= 0; --j)
					{
						unsigned phase = (packetFrameIndex - 1 - j) % (SAMPLE_RATE / FREQUENCY_CARRIER);
						value += samples[frameIndex - j - align[i]] * sine[phase];
					}
					if (value > THRESHOLD_DEMODULATE)
						packet[i][indexBytePacket] = packet[i][indexBytePacket] | (1 << offset);
				}
				bitsReceived++;
			}
			frameIndex++;
			if ((mode == RECEIVER && (packetFrameIndex >= SAMPLES_PER_BIT * BITS_NORMALPACKET ||
				bitsReceived == 50000 + ceil((float)50000 / BITS_CONTENT) * BITS_CRC)) ||
				(mode == TRANSMITTER && packetFrameIndex >= SAMPLES_PER_BIT * BITS_PER_BYTE * BYTES_ACK_CONTENT))
			{
				unsigned bitsReceivedPacket = packetFrameIndex / SAMPLES_PER_BIT;
				unsigned indexByte = (bitsReceived - bitsReceivedPacket) / BITS_NORMALPACKET * BYTES_CONTENT;
				uint8_t *wptr = data + indexByte;
				int choice = 0;
				for (int i = 0; i < 5; ++i)
				{
					crc_t crc = crc_init();
					crc = crc_update(crc, packet[i], bitsReceivedPacket / BITS_PER_BYTE);
					crc = crc_finalize(crc);
					if (crc == 0x48674bc7) {
						choice = i;
						break;
					}
				}
				memcpy(wptr, packet[choice], bitsReceivedPacket / BITS_PER_BYTE - BYTES_CRC);
				if (!signal)
					*signal = true;
				if (bitsReceived == 50000 + ceil((float)50000 / BITS_CONTENT) * BITS_CRC)
					return true;
				else
					prepare_for_new_packet();
			}
		}
	}
	return false;
}

DataCo::DataCo(Mode mode_, const char *send_file, void *data_sent_, 
	void *data_rec_,SAMPLE *samples_sent_, SAMPLE *samples_rec_) :
	signal(false), mode(mode_), send_data(send_file, data_sent_, samples_sent_, true, mode_, &signal),
	receive_data(MAX_TIME_RECORD, data_rec_, samples_rec_, true, mode_, &signal)
{
}