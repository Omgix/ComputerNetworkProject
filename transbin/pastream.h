#ifndef PASTREAM_H
#define PASTREAM_H
#include "portaudio.h"
#include <sndfile.hh>
#include <cstdint>

#define PA_SAMPLE_TYPE  paFloat32
typedef float SAMPLE;
#define SAMPLE_SILENCE  (0.0f)
#define PRINTF_S_FORMAT "%.8f"

class SendData;
class ReceiveData;
class Stream;

class Stream
{
private:
	PaError						err;
	unsigned long				bufferSize;
	const PaStreamParameters	inputParameters;
	const PaStreamParameters	outputParameters;
	PaStream*					stream;
	static int					numStream;
	void error();
	PaStreamParameters set_input_para(int receiveDeviceNo);
	PaStreamParameters set_output_para(int sendDeviceNo);
	void open_input_stream(void *data, PaStreamCallback callback);
	void open_output_stream(void *data, PaStreamCallback callback);
	void start_stream();
	void stop_stream();
	void close_stream();
public:
	Stream(int sendDeviceNo, int receiveDeviceNo);
	~Stream();
	Stream(const Stream &rhs) = delete;
	Stream &operator==(const Stream& rhs) = delete;
	void send(SendData &data, bool writewaves = false, const char* file_name = nullptr);
	void receive(ReceiveData &data, bool writewaves = false, const char* file_name = nullptr);
};

int sendCallback(const void *inputBuffer, void *outputBuffer,
				unsigned long framesPerBuffer,
				const PaStreamCallbackTimeInfo *timeInfo,
				PaStreamCallbackFlags statusFlags,
				void* userData);

class SendData
{
private:
	enum sendingStatus
	{
		SIGNAL,                                   // Status of sending the single preamble and silence interval
		CONTENT                                   // Status of sending the content
	};
	sendingStatus status;                         // Status of sending
	uint8_t *data;
	SAMPLE *samples;
	bool ext_data_ptr;
	bool ext_sample_ptr;
	unsigned long fileFrameIndex;                 // Index of the sound frame that will send
	unsigned long totalFrames;					  // Total number of sound frames.
	unsigned bitIndex;                            // Index of the bit that will send
	unsigned maxBitIndexSend;                     // Maximum index of the bit that will send
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
	SendData(const char* file_name, void *data_ = nullptr, SAMPLE *samples_ = nullptr);
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
	enum receivingStatus
	{
		WAITING,        // Status of waiting for the single preamble
		ASSURING,       // Status of adjusting convolution threshold when the single preamble is going through
		DETECTING,      // Status of the single preamble has gone and waiting for the first packet            
		RECEIVING       // First packet's preamble has been found then start receiving
	};
	receivingStatus status;           // Status of receiving
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
	ReceiveData(unsigned max_time, void *data_ = nullptr, SAMPLE *samples_ = nullptr);
	~ReceiveData();
	size_t write_to_file(const char* path); // Write received content data to a TXT file
	size_t write_samples_to_file(const char* path); // Write recorded sound data to a WAV file
};

#endif
