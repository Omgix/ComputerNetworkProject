#include "pastream.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include "crc.h"

float THRESHOLD_SILENCE;
bool in = false;
bool first = true;
//uint8_t tmp_p[BYTES_CONTENT + BYTES_CRC];

float sine[SAMPLE_RATE / FREQUENCY_CARRIER] =
{ +0.00000000f, +0.78183148f, +0.97492791f, +0.43388374f, -0.43388374f, -0.97492791f, -0.78183148f, };
float preamble[LEN_PREAMBLE] =
{
+0.00000000f, +0.00278273f, +0.01113070f, +0.02504197f, +0.04450900f, +0.06951219f, +0.10001086f, +0.13593172f,
+0.17715485f, +0.22349749f, +0.27469565f, +0.33038412f, +0.39007528f, +0.45313737f, +0.51877326f, +0.58600081f,
+0.65363628f, +0.72028246f, +0.78432361f, +0.84392928f, +0.89706958f, +0.94154407f, +0.97502689f, +0.99513002f,
+0.99948622f, +0.98585237f, +0.95223278f, +0.89702038f, +0.81915204f, +0.71827191f, +0.59489437f, +0.45055561f,
+0.28794045f, +0.11096894f, -0.07517408f, -0.26408348f, -0.44826860f, -0.61939111f, -0.76861402f, -0.88706222f,
-0.96638524f, -0.99940089f, -0.98078528f, -0.90776067f, -0.78071984f, -0.60371568f, -0.38473921f, -0.13571116f,
+0.12787716f, +0.38771670f, +0.62383784f, +0.81614016f, +0.94621220f, +0.99930486f, +0.96627067f, +0.84524042f,
+0.64278761f, +0.37433825f, +0.06362589f, -0.25892656f, -0.55889757f, -0.80148019f, -0.95570395f, -0.99879546f,
-0.92008680f, -0.72382481f, -0.43026996f, -0.07461910f, +0.29645653f, +0.63051340f, +0.87672676f, +0.99401993f,
+0.95866785f, +0.76996522f, +0.45264117f, +0.05507021f, -0.35703746f, -0.71095291f, -0.93984481f, -0.99610689f,
-0.86244156f, -0.55806658f, -0.13802666f, +0.31521385f, +0.70710678f, +0.95076027f, +0.98711063f, +0.80081404f,
+0.42745447f, -0.05084640f, -0.52181507f, -0.86873959f, -0.99996035f, -0.87473840f, -0.51801178f, -0.01858758f,
+0.49071755f, +0.86741341f, +0.99976203f, +0.84189302f, +0.43227847f, -0.11207508f, -0.62661769f, -0.94824593f,
-0.96819222f, -0.67155895f, -0.14860199f, +0.42916437f, +0.86334163f, +0.99719547f, +0.77484317f, +0.26816070f,
-0.34202014f, -0.82780314f, -0.99934976f, -0.78176208f, -0.25193148f, +0.38360877f, +0.86624794f, +0.99071821f,
+0.69568255f, +0.09857099f, -0.54590800f, -0.95185817f, -0.93022162f, -0.48128151f, +0.19509032f, +0.78356346f,
+0.99923253f, +0.72872170f, +0.09557984f, -0.58960280f, -0.97836921f, -0.86328546f, -0.29432969f, +0.43478610f,
+0.93345272f, +0.92383693f, +0.40111593f, -0.35047844f, -0.90630779f, -0.94086719f, -0.42382845f, +0.34797522f,
+0.91441262f, +0.92586890f, +0.36492653f, -0.42755509f, -0.95291024f, -0.86862931f, -0.21861249f, +0.57876207f,
+0.99391004f, +0.73960413f, -0.02181489f, -0.77180863f, -0.98449693f, -0.49990360f, +0.34578286f, +0.94421372f,
+0.85142817f, +0.12511678f, -0.69855548f, -0.99428435f, -0.52598674f, +0.35381218f, +0.95879444f, +0.80294222f,
+0.00000000f, -0.80624710f, -0.95223278f, -0.30653879f, +0.59953686f, +0.99948264f, +0.54217220f, -0.38771670f,
-0.98087204f, -0.70639806f, +0.20207222f, +0.93091441f, +0.81089710f, -0.05884858f, -0.87672676f, -0.87082667f,
-0.03561143f, +0.83668873f, +0.89922301f, +0.07994595f, -0.82118986f, -0.90398929f, -0.07428609f, +0.83375064f,
+0.88659931f, +0.01858758f, -0.87153710f, -0.84189302f, +0.08715574f, +0.92494086f, +0.75884231f, -0.24082011f,
-0.97648835f, -0.62270626f, +0.43428484f, +0.99998513f, +0.41938741f, -0.64831315f, -0.96153123f, -0.14276554f,
+0.84767038f, +0.82466801f, -0.19509032f, -0.97948293f, -0.56184768f, +0.55306887f, +0.97937062f, +0.17266153f,
-0.85514276f, -0.79002128f, +0.29518060f, +0.99848248f, +0.39335253f, -0.73312758f, -0.88473874f, +0.15179334f,
+0.98480775f, +0.47580828f, -0.68828998f, -0.90092057f, +0.14375699f, +0.98803758f, +0.43348255f, -0.74095113f,
-0.85142817f, +0.27180467f, +0.99999197f, +0.25871153f, -0.86691450f, -0.70070282f, +0.51877326f, +0.95474888f,
-0.06407022f, -0.98588966f, -0.38473921f, +0.81614016f, +0.73907949f, -0.50471598f, -0.94778545f, +0.12533765f,
+0.99771085f, +0.25268544f, -0.90574248f, -0.57639957f, +0.70710678f, +0.81394743f, -0.44388527f, -0.95368347f,
+0.30838140f, +0.96760734f, +0.67946627f, -0.98413540f, +0.92481119f, +0.81851821f, +0.99906145f, -0.50149132f,
+0.88846157f, +0.09160562f, +0.61344438f, +0.33513856f, +0.22488598f, -0.69994537f, -0.20519380f, +0.93523096f,
-0.59738122f, -0.99743757f, -0.87902947f, +0.87508998f, -0.99798947f, -0.59083262f, -0.93227312f, +0.19723632f,
-0.69410434f, +0.23287752f, -0.32740917f, -0.61981595f, +0.09978708f, +0.89216880f, +0.50848746f, -0.99938419f,
+0.82321130f, +0.92166744f, +0.98555519f, -0.67344584f, +0.96553595f, +0.30056203f, +0.76682207f, +0.12790532f,
+0.42631735f, -0.53272423f, +0.00675583f, +0.83897518f, -0.41390899f, -0.98995499f, -0.75805525f, +0.95775523f,
-0.96188315f, -0.74834940f, -0.98776728f, +0.40041134f, -0.83088634f, +0.02147513f, -0.52020676f, -0.43951844f,
-0.11328355f, +0.77619505f, +0.31456525f, -0.96925266f, +0.68427330f, +0.98295353f, +0.92726959f, -0.81474020f,
+0.99875795f, +0.49586762f, +0.88542193f, -0.08520070f, +0.60827973f, -0.34142225f, +0.21852206f, +0.70461429f,
-0.21163957f, -0.93751262f, -0.60250988f, +0.99694166f, -0.88212669f, -0.87186268f, -0.99838257f, +0.58556798f,
-0.92990904f, -0.19090700f, -0.68933363f, -0.23915027f, -0.32121487f, +0.62501404f, +0.10630357f, -0.89504156f,
+0.51422507f, +0.99959188f, +0.82695089f, -0.91911482f, +0.98662517f, +0.66860296f, +0.96380678f, -0.29420622f,
+0.76262994f, -0.13438690f, +0.42036501f, +0.53819710f, +0.00036572f, -0.84248553f, -0.41988662f, +0.99085123f,
-0.76229041f, -0.95588672f, -0.96364609f, +0.74402704f, -0.98670900f, -0.39450669f, -0.82723628f, -0.02809408f,
-0.51461796f, +0.44524017f, -0.10678911f, -0.78035641f, +0.32076389f, +0.97084824f, +0.68893958f, -0.98173554f,
+0.92974453f, +0.81090990f, +0.99841380f, -0.49019717f, +0.88235871f, +0.07864098f, +0.60301683f, +0.34743241f,
+0.21211982f, -0.70927383f, -0.21798599f, +0.93976480f, -0.60773431f, -0.99641497f, -0.88522844f, +0.86870331f,
-0.99872832f, -0.58020464f, -0.92749480f, +0.18436059f, -0.68464467f, +0.24556018f, -0.31503656f, -0.63011282f,
+0.11272369f, +0.89802502f, +0.51980871f, -0.99975497f, +0.83056882f, +0.91653555f, +0.98768757f, -0.66370935f,
+0.96206130f, +0.28798429f, +0.75838619f, +0.14077127f, +0.41436751f, -0.54367227f, -0.00629823f, +0.84600884f,
-0.42576249f, -0.99172527f, -0.76651225f, +0.95389655f, -0.96539171f, -0.73973549f, -0.98562273f, +0.38844587f,
-0.82356875f, +0.03461947f, -0.50908673f, -0.45124264f, -0.10032130f, +0.78435048f, +0.32697687f, -0.97238036f,
+0.69373075f, +0.98047010f, +0.93205628f, -0.80702702f, +0.99801812f, +0.48447993f, +0.87924349f, -0.07217000f,
+0.59780162f, -0.35357028f, +0.20579888f, +0.71383788f, -0.22435212f, -0.94198688f, -0.61295602f, +0.99585496f,
-0.88824889f, -0.86543218f, -0.99903387f, +0.57479174f, -0.92507634f, -0.17801704f, -0.67990544f, -0.25163367f,
-0.30893273f, +0.63492462f, +0.11941099f, -0.90092766f, +0.52539548f, +0.99988095f, +0.83416841f, -0.91390555f,
+0.98869241f, +0.65894849f, +0.96019908f, -0.28195595f, +0.75417071f, -0.14717915f, +0.40854772f, +0.54945522f,
-0.01286952f, -0.84944683f, -0.43186745f, +0.99254435f, -0.77056455f, -0.95196663f, -0.96704054f, +0.73522805f,
-0.98453172f, -0.38267914f, -0.81984946f, -0.04129500f, -0.50329781f, +0.45714258f, -0.09381970f, -0.78840566f,
+0.33320374f, +0.97390614f, +0.69833793f, -0.97918146f, +0.93442826f, +0.80330940f, +0.99759336f, -0.47882304f,
+0.87607589f, +0.06530093f, +0.59253707f, +0.35960676f, +0.19920101f, -0.71839247f, -0.23073758f, +0.94417820f,
-0.61817443f, -0.99521580f, -0.89118950f, +0.86223239f, -0.99930320f, -0.56952985f, -0.92246700f, +0.17139631f,
};

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
		unsigned frames = framesPerBuffer;
		if (data->mode == RECEIVER && data->wait)
		{
			unsigned i;
			for (i = 0; i < framesPerBuffer; ++i)
			{
				*wptr++ = SAMPLE_SILENCE;
				data->samples[fileFrameIndex++] = SAMPLE_SILENCE;
				if (*data->signal)
				{
					data->wait = false;
					break;
				}
			}
			if (i == framesPerBuffer) return paContinue;
			else frames = framesPerBuffer - i - 1;
		}
		for (unsigned i = 0; i < frames; ++i)
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
			data->isNewPacket = true;
		}
		return paContinue;
	}
	else
	{
		if (wptr == nullptr)
			return paComplete;

		/* Use 2-PSK to modulate */
		for (unsigned i = 0; i < framesPerBuffer; ++i)
		{
			if (data->isNewPacket)
			{
				if (data->wait)
				{
					*wptr++ = SAMPLE_SILENCE;
					data->samples[fileFrameIndex++] = SAMPLE_SILENCE;
					if (data->mode == TRANSMITTER)
					{
						if (std::chrono::system_clock::now() > data->time_send + data->ack_timeout) // Time out! Retransmit
						{
							data->times_sent++;
							if (data->times_sent > TIMES_TRY)
								return paComplete; // When reach there, wait is true;
							data->wait = false;
							if (data->ack_timeout == ACK_INIT_TIMEOUT)
								data->ack_timeout -= DURATION_SIGNAL;
							data->bitIndex -= index / SAMPLES_PER_BIT;
						}
						else if (*data->signal) // ACK received
						{
							data->times_sent = 0;
							data->wait = false;
							if (data->bitIndex >= data->totalBits) // All data has been sent
								return paComplete; // When reach there, wait is false;
							if (data->ack_timeout == ACK_INIT_TIMEOUT)
								data->ack_timeout -= DURATION_SIGNAL;
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
				index = 0;
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
				if ((data->mode == TRANSMITTER && (index>= SAMPLES_PER_BIT * BITS_NORMALPACKET || 
					data->bitIndex >= data->totalBits)) ||
					(data->mode == RECEIVER && index >= SAMPLES_PER_BIT * (BYTES_ACK_CONTENT + BYTES_CRC) * BITS_PER_BYTE))
				{
					data->prepare_for_new_sending(); // Reset several index after sending a packet
					if (data->need_ack || data->mode == RECEIVER)
					{
						data->time_send = std::chrono::system_clock::now();
						data->wait = true;
						*data->signal = false;
					}
					if (data->mode == TRANSMITTER && data->bitIndex >= data->totalBits && !data->need_ack)
						return paComplete;
				}

			}
		}

		return paContinue;
	}
}

SendData::SendData(const char* file_name, void *data_, SAMPLE *samples_, milliseconds timeout_, bool need_ack_, Mode mode_, bool *ack_received_) :
	status(SIGNAL), fileFrameIndex(0), ext_data_ptr(data_ != nullptr), ext_sample_ptr(samples_ != nullptr), mode(mode_),
	bitIndex(0), packetFrameIndex(0), isPreamble(true), isNewPacket(true), need_ack(mode_ == RECEIVER ? true : need_ack_),
	wait(mode_ == TRANSMITTER? false:true), size(0), totalFrames(0), data((uint8_t*)data_), samples(samples_), signal(ack_received_), 
	ack_timeout(timeout_), times_sent(0)
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
		totalBits = size * BITS_PER_BYTE + BITS_CRC * numPacket;
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
		totalBits = size * BITS_PER_BYTE + BITS_CRC;
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
	isNewPacket = true;
	if (mode == RECEIVER)
		bitIndex -= packetFrameIndex / SAMPLES_PER_BIT;
}

sf_count_t SendData::write_samples_to_file(const char *path = nullptr)
{
	int format = SF_FORMAT_WAV | SF_FORMAT_FLOAT;
	int channels = NUM_CHANNELS;
	int srate = SAMPLE_RATE;
	if (!path) path = "sendwaves.wav";
	SndfileHandle file = SndfileHandle(path, SFM_WRITE, format, channels, srate);
	file.writeSync();

	return file.write(samples, fileFrameIndex);
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
	frameIndex += LEN_PREAMBLE - 20;
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
	if (!path) path = "reveivewaves.wav";
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
		if (status == RECEIVING && !in && mode == RECEIVER) {
			if (first)
			{
				printf("Start receiving!\n");
				printf("Start from frame #%u\n", frameIndex);
				startFrom = frameIndex - LEN_SIGNAL - LEN_PREAMBLE;
				first = false;
			}
			printf("New packet start from frame #%u\n", frameIndex); fflush(stdout);
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
				(mode == TRANSMITTER && packetFrameIndex >= SAMPLES_PER_BIT * BITS_PER_BYTE * (BYTES_ACK_CONTENT + BYTES_CRC)))
			{
				unsigned bitsReceivedPacket = packetFrameIndex / SAMPLES_PER_BIT;
				unsigned indexByte = (bitsReceived - bitsReceivedPacket) / BITS_NORMALPACKET * BYTES_CONTENT;
				uint8_t *wptr = data + indexByte;
				int choice = 0;
				bool canAc = false;
				for (int i = 0; i < 5; ++i)
				{
					crc_t crc = crc_init();
					crc = crc_update(crc, packet[i], bitsReceivedPacket / BITS_PER_BYTE);
					crc = crc_finalize(crc);
					if (crc == 0x48674bc7) {
						choice = i;
						canAc = true;
						break;
					}
				}
				if (canAc)
					memcpy(wptr, packet[choice], bitsReceivedPacket / BITS_PER_BYTE - BYTES_CRC);
				else if (mode == RECEIVER)
					bitsReceived -= bitsReceivedPacket;
				if ((need_ack && mode == RECEIVER && canAc) || mode == TRANSMITTER) 
				{
					if (mode == TRANSMITTER)
						printf("SIGNAL\n");
					*signal = true;
				}
				if (mode == RECEIVER && bitsReceived == 50000 + ceil((float)50000 / BITS_CONTENT) * BITS_CRC)
					return true;
				else
					prepare_for_new_packet();
			}
		}
	}
	return false;
}

DataCo::DataCo(Mode mode_, const char *send_file, void *data_sent_,
	void *data_rec_, SAMPLE *samples_sent_, SAMPLE *samples_rec_) :
	signal(false), mode(mode_),
	send_data(send_file, data_sent_, samples_sent_, ACK_INIT_TIMEOUT, true, mode_, &signal),
	receive_data(MAX_TIME_RECORD, data_rec_, samples_rec_, true, mode_, &signal)
{
}