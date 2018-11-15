#include "portaudio.h"
#include <sndfile.hh>
#include <cstdint>
#include <chrono>

#ifndef PASTREAM_H
#define PASTREAM_H

#define PA_SAMPLE_TYPE  paFloat32
typedef float SAMPLE;
#define SAMPLE_SILENCE  (0.0f)
#define PRINTF_S_FORMAT "%.8f"

class SendData;
class ReceiveData;
class Stream;
class DataCo;

typedef std::chrono::time_point<std::chrono::system_clock> timepoint;
typedef std::chrono::milliseconds milliseconds;

const int SAMPLE_RATE = 44100;          // SAMPLE_RATE frames in a second
const int NUM_CHANNELS = 1;             // Mono channel will be used
const int FRAMES_PER_BUFFER = 512;      // Size of the buffer
const int BITS_INFO = 16;
const int BYTES_INFO = BITS_INFO / 8;
const int BITS_CONTENT = 1600;        // Number of content bits in a packet
const int BYTES_CONTENT = BITS_CONTENT / 8;
const int BYTES_ACK_CONTENT = 5;
const int BITS_ACK_CONTENT = BYTES_ACK_CONTENT * 8;
const int BITS_CRC = 32;
const int BYTES_CRC = BITS_CRC / 8;
const int BITS_NORMALPACKET = BITS_CRC + BITS_CONTENT + BITS_INFO;
const int BYTES_NORMALPACKET = BITS_NORMALPACKET / 8;
const int LEN_PREAMBLE = 512;		// Length of the preamble in frames
const int OPERATING_BANDWIDTH = 1000;   // Equal to bandwidth in bps
const int FREQUENCY_CARRIER = 6300;     // Frequency of carrier waive
const int SAMPLES_PER_BIT = 4;         // 
const int LEN_SIGNAL = 1024;         // Length of the silence interval in frames
const float CONSTANT_THRESHOLD = 0.80;  // Test
const float THRESHOLD_DEMODULATE = 0.f; // 2-PSK threshold to determine what we received is 1 or 0
const float THRESHOLD_ASSURING = 50.f;  // Convolution threshold of starting adjusting preamble threshold
const float A = 0.95f;                  // Amplitude of the sound
const float CONSTANT_AMPLIFY = 1;       // The constant that will be multiplied in the frames that Rx received.
const int MAX_TIME_RECORD = 30;         // Max recording time in seconds
const int INITIAL_INDEX_RX = 600;
const int BITS_PER_BYTE = 8;
const int TIMES_TRY = 10;
const milliseconds ACK_INIT_TIMEOUT(320);
const milliseconds DURATION_SIGNAL(23);



class Stream
{
private:
	PaError						err;
	unsigned long				bufferSize;
	PaStreamParameters	inputParameters;
	PaStreamParameters	outputParameters;
	PaStream*					input_stream;
	PaStream*					output_stream;
	static int					numStream;
	int error();
	void set_input_para(int receiveDeviceNo);
	void set_output_para(int sendDeviceNo);
	void open_input_stream(ReceiveData *data, PaStreamCallback callback);
	void open_output_stream(SendData *data, PaStreamCallback callback);
	void start_input_stream();
	void start_output_stream();
	void stop_input_stream();
	void stop_output_stream();
	void close_input_stream();
	void close_output_stream();
public:
	Stream(int sendDeviceNo, int receiveDeviceNo);
	~Stream();
	Stream(const Stream &rhs) = delete;
	Stream &operator==(const Stream& rhs) = delete;
	void list_all();
	bool select_input_device(int device_no);
	bool select_output_device(int device_no);
	void send(SendData &data, bool writewaves = false, const char* file_name = nullptr);
	void send(DataCo &data, bool write_sent_waves = false, const char* file_wave_sent = nullptr,
		bool write_rec_waves = false, const char* file_wave_rec = nullptr);
	void receive(ReceiveData &data, bool writewaves = false, const char* file_name = nullptr);
	void receive(DataCo &data, bool write_sent_waves = false, const char* file_wave_sent = nullptr,
		bool write_rec_waves = false, const char* file_wave_rec = nullptr);
};

int sendCallback(const void *inputBuffer, void *outputBuffer,
	unsigned long framesPerBuffer,
	const PaStreamCallbackTimeInfo *timeInfo,
	PaStreamCallbackFlags statusFlags,
	void* userData);

enum Mode
{
	TRANSMITTER,
	RECEIVER
};

class SendData
{
private:
	enum status
	{
		SIGNAL,                                   // Status of sending the single preamble and silence interval
		CONTENT                                   // Status of sending the content
	};
	Mode mode;
	bool need_ack;
	bool wait;
	timepoint time_send;
	bool* signal;
	milliseconds ack_timeout;
	unsigned times_sent;
	status status;                         // Status of sending
	uint8_t *data;
	SAMPLE *samples;
	bool ext_data_ptr;
	bool ext_sample_ptr;
	unsigned long fileFrameIndex;                 // Index of the sound frame that will send
	unsigned long totalFrames;					  // Total number of sound frames.
	unsigned bitIndex;                            // Index of the bit that will send
	unsigned totalBits;
	unsigned size;                                // Total number of bytes that will send.
	unsigned packetFrameIndex;                    // Index of the sound frame in a packet
	bool isPreamble;                              // Indicate if we will send a preamble frame
	bool isNewPacket;                             // Indicate if the next frame we will send is in a new packet
	void prepare_for_new_sending();
	friend int sendCallback(const void *inputBuffer, void *outputBuffer,
		unsigned long framesPerBuffer,
		const PaStreamCallbackTimeInfo *timeInfo,
		PaStreamCallbackFlags statusFlags,
		void* userData);
	friend class Stream;
public:
	SendData(const char* file_name, void *data_ = nullptr, SAMPLE *samples_ = nullptr, milliseconds timeout_ = milliseconds(2000),
		bool need_ack_ = false, Mode mode_ = TRANSMITTER, bool *ack_received_ = nullptr);
	~SendData();
	SendData(const SendData &rhs) = delete;
	SendData &operator=(const SendData &rhs) = delete;
	sf_count_t write_samples_to_file(const char *path); // Write sound data that has been sent to a WAV file
};

int receiveCallback(const void *inputBuffer, void *outputBuffer,
	unsigned long framesPerBuffer,
	const PaStreamCallbackTimeInfo *timeInfo,
	PaStreamCallbackFlags statusFlags,
	void* userData);

class ReceiveData
{
private:
	enum status
	{
		WAITING,        // Status of waiting for the single preamble
		ASSURING,       // Status of adjusting convolution threshold when the single preamble is going through
		DETECTING,      // Status of the single preamble has gone and waiting for the first packet            
		RECEIVING       // First packet's preamble has been found then start receiving
	};
	Mode mode;
	bool need_ack;
	bool* signal;
	status status;           // Status of receiving
	uint8_t *data;
	SAMPLE *samples;
	uint8_t **packet;
	int align[5] = { 0, -1, 1, -2, 2 };
	bool ext_data_ptr;
	bool ext_sample_ptr;
	unsigned packetFrameIndex;        // Index of the sound frame in a packet
	SAMPLE threshold;                 // Convolution threshold, used in status DETECTING to find the first packet's preamble
	SAMPLE amplitude;
	unsigned bitsReceived;       // Number of bits that has received
	unsigned numSamplesReceived; // Number of samples (frames) that has received
	const unsigned maxNumFrames; // Max number of samples (frames) that can be received
	unsigned frameIndex;         // Used in demodulation thread (demodulate()) to indicate the index of frame that is being demodulated
	unsigned startFrom;
	inline void prepare_for_new_packet();
	inline void prepare_for_receiving();
	//void writeThreshold();                  // Write the convolution value in each frames to a WAV file
	void correct_threshold();               // Adjusting convolution threshold in status ASSURING
	bool correlate_next();                  // Calculate the convolution
	bool demodulate();                      // Demodulate received frames
	friend int receiveCallback(const void *inputBuffer, void *outputBuffer,
		unsigned long framesPerBuffer,
		const PaStreamCallbackTimeInfo *timeInfo,
		PaStreamCallbackFlags statusFlags,
		void* userData);
	friend class Stream;
public:
	ReceiveData(unsigned max_time, void *data_ = nullptr, SAMPLE *samples_ = nullptr,
		bool need_ack_ = false, Mode mode_ = RECEIVER, bool *ack_received_ = nullptr);
	~ReceiveData();
	ReceiveData(const ReceiveData &rhs) = delete;
	ReceiveData &operator=(const ReceiveData &rhs) = delete;
	size_t write_to_file(const char* path); // Write received content data to a TXT file
	size_t write_samples_to_file(const char* path); // Write recorded sound data to a WAV file
};

class DataCo
{
private:
	bool signal;
	Mode mode;
	SendData send_data;
	ReceiveData receive_data;
	friend class Stream;
public:
	DataCo(Mode mode_, const char *send_file = nullptr, void *data_sent_ = nullptr,
		void *data_rec_ = nullptr, SAMPLE *samples_sent_ = nullptr, SAMPLE *samples_rec_ = nullptr);
	DataCo(const DataCo &rhs) = delete;
	DataCo &operator=(const DataCo &rhs) = delete;
};

class DataSim
{
private:
	timepoint backoff;
	SendData senddata;
	SendData ack;
	ReceiveData receivedata;
	friend class Stream;
public:
	DataSim(const char *send_file = nullptr, void *data_sent_ = nullptr,
		void *data_rec_ = nullptr, SAMPLE *samples_sent_ = nullptr, SAMPLE *samples_rec_ = nullptr);
	DataSim(const DataSim &rhs) = delete;
	DataSim &operator=(const DataSim &rhs) = delete;
};

#endif