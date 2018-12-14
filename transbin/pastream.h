﻿#ifndef PASTREAM_H
#define PASTREAM_H

#include "portaudio.h"
#include <sndfile.hh>
#include <cstdint>
#include <chrono>
#include <queue>

#define PA_SAMPLE_TYPE  paFloat32
typedef float SAMPLE;
#define SAMPLE_SILENCE  (0.0f)
#define PRINTF_S_FORMAT "%.8f"

class SendData;
class ReceiveData;
class Stream;
class DataCo;
class DataSim;

typedef std::chrono::time_point<std::chrono::system_clock> timepoint;
typedef std::chrono::microseconds microseconds;

const int SAMPLE_RATE = 44100;          // SAMPLE_RATE frames in a second
const int NUM_CHANNELS = 1;             // Mono channel will be used
const int FRAMES_PER_BUFFER = 512;      // Size of the buffer
const int BITS_INFO = 32;
const int BYTES_INFO = BITS_INFO / 8;
extern int BITS_CONTENT;        // Number of content bits in a packet
extern int BYTES_CONTENT;
const int BYTES_ACK_CONTENT = 1;
const int BITS_ACK_CONTENT = BYTES_ACK_CONTENT * 8;
const int BITS_CRC = 32;
const int BYTES_CRC = BITS_CRC / 8;
extern int BITS_NORMALPACKET;
const int BITS_ACKPACKET = BITS_CRC + BITS_ACK_CONTENT + BITS_INFO;
const int BYTES_NORMALPACKET = BITS_NORMALPACKET / 8;
const int LEN_PREAMBLE = 512;		// Length of the preamble in frames
const int OPERATING_BANDWIDTH = 1000;   // Equal to bandwidth in bps
const float FREQUENCY_CARRIER = 5512.5;     // Frequency of carrier waive
const int NUM_CARRIRER = 1;
const int SAMPLES_PER_N_BIT = 4;//(float)SAMPLE_RATE / FREQUENCY_CARRIER;         // 
const int PERIOD = 8;// (float)SAMPLE_RATE / FREQUENCY_CARRIER;
const int LEN_SIGNAL = 1024;         // Length of the silence interval in frames
const float CONSTANT_THRESHOLD = 0.80;  // Test
const float THRESHOLD_SILENCE = 0.75;
const float THRESHOLD_DEMODULATE = 0.f; // 2-PSK threshold to determine what we received is 1 or 0
const float THRESHOLD_ASSURING = 50.f;  // Convolution threshold of starting adjusting preamble threshold
const float A = 0.95f;                  // Amplitude of the sound
const float CONSTANT_AMPLIFY = 1;       // The constant that will be multiplied in the frames that Rx received.
const int MAX_TIME_RECORD = 90;         // Max recording time in seconds
const int INITIAL_INDEX_RX = 100;
const int BITS_PER_BYTE = 8;
const int TIMES_TRY = 10;

const microseconds ACK_INIT_TIMEOUT(313000);
const microseconds ACK_JAMMING_TIMEOUT(813000);
const microseconds DURATION_SIGNAL(23000);
const microseconds RTT(150000);
const microseconds SIFS = microseconds(SAMPLES_PER_N_BIT * BITS_ACKPACKET * 1000000 / SAMPLE_RATE) + RTT;
const microseconds SLOT(100);
const microseconds DIFS = SIFS + SLOT * 2;

const int MAX_TIME_RECORD_SIMUL = 80;
const int OFFSET_SRC = 0;
const int INDEX_BYTE_SRC = OFFSET_SRC / 8;
const int OFFSET_DST = 2;
const int INDEX_BYTE_DST = OFFSET_DST / 8;
const int OFFSET_TYPEID = 4;
const int INDEX_BYTE_TYPEID = OFFSET_TYPEID / 8;
const int OFFSET_SIZE = 8;
const int INDEX_BYTE_SIZE = OFFSET_SIZE / 8;
const int OFFSET_NO = 16;
const int INDEX_BYTE_NO = OFFSET_NO / 8;

const int TYPEID_CONTENT_NORMAL = 0;
const int TYPEID_CONTENT_LAST = 1;
const int TYPEID_ACK = 3;
const int TYPEID_ACK_LAST = 5;
const int TYPEID_NONE = 0xf;
extern int NODE;

extern float square[MAX_TIME_RECORD * SAMPLE_RATE];

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
	void open_input_stream(DataSim *data, PaStreamCallback callback);
	void open_output_stream(SendData *data, PaStreamCallback callback);
	void open_output_stream(DataSim *data, PaStreamCallback callback);
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
	void send_and_receive(DataSim &data, bool write_sent_waves = false, const char* file_wave_sent = nullptr,
		bool write_rec_waves = false, const char* file_wave_rec = nullptr);
};

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
	const PaStreamCallbackTimeInfo* time_info;
	Mode mode;
	bool need_ack;
	bool wait;
	timepoint time_send;
	bool* signal;
	microseconds ack_timeout;
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
	unsigned ackNo;
	inline void prepare_for_new_sending();
	void set_ack(const int dst, const int no, const bool last);
	friend int send_callback(const void *inputBuffer, void *outputBuffer,
		unsigned long framesPerBuffer,
		const PaStreamCallbackTimeInfo *timeInfo,
		PaStreamCallbackFlags statusFlags,
		void* userData);
	friend int receive_callback(const void *inputBuffer, void *outputBuffer,
		unsigned long framesPerBuffer,
		const PaStreamCallbackTimeInfo *timeInfo,
		PaStreamCallbackFlags statusFlags,
		void* userData);
	friend class Stream;
	friend class DataSim;
public:
	explicit SendData(const char* file_name, void *data_ = nullptr, SAMPLE *samples_ = nullptr,
	        microseconds timeout_ = microseconds(2000000), bool need_ack_ = false,
	        Mode mode_ = TRANSMITTER, bool *ack_received_ = nullptr, int dst = 0);
	~SendData();
	SendData(const SendData &rhs) = delete;
	SendData &operator=(const SendData &rhs) = delete;
    static int sendCallback(const void *inputBuffer, void *outputBuffer,
                            unsigned long framesPerBuffer,
                            const PaStreamCallbackTimeInfo *timeInfo,
                            PaStreamCallbackFlags statusFlags,
                            void* userData);
	sf_count_t write_samples_to_file(const char *path); // Write sound data that has been sent to a WAV file
};

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
	int src;
	int typeID;					 // Used in demodulation
	int noPacket;
	int bytesPacketContent; // Used in demodulation
	inline void prepare_for_new_packet();
	inline void prepare_for_receiving();
	//void writeThreshold();                  // Write the convolution value in each frames to a WAV file
	void correct_threshold();               // Adjusting convolution threshold in status ASSURING
	bool correlate_next();                  // Calculate the convolution
	bool demodulate();                      // Demodulate received frames
	friend int send_callback(const void *inputBuffer, void *outputBuffer,
		unsigned long framesPerBuffer,
		const PaStreamCallbackTimeInfo *timeInfo,
		PaStreamCallbackFlags statusFlags,
		void* userData);
	friend int receive_callback(const void *inputBuffer, void *outputBuffer,
		unsigned long framesPerBuffer,
		const PaStreamCallbackTimeInfo *timeInfo,
		PaStreamCallbackFlags statusFlags,
		void* userData);
	friend class Stream;
	friend class DataSim;
public:
	explicit ReceiveData(unsigned max_time, void *data_ = nullptr, SAMPLE *samples_ = nullptr,
		bool need_ack_ = false, Mode mode_ = RECEIVER, bool *ack_received_ = nullptr);
	~ReceiveData();
	ReceiveData(const ReceiveData &rhs) = delete;
	ReceiveData &operator=(const ReceiveData &rhs) = delete;
    static int receiveCallback(const void *inputBuffer, void *outputBuffer,
                        unsigned long framesPerBuffer,
                        const PaStreamCallbackTimeInfo *timeInfo,
                        PaStreamCallbackFlags statusFlags,
                        void* userData);
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
	explicit DataCo(Mode mode_, const char *send_file = nullptr, void *data_sent_ = nullptr,
		void *data_rec_ = nullptr, SAMPLE *samples_sent_ = nullptr, SAMPLE *samples_rec_ = nullptr);
	DataCo(const DataCo &rhs) = delete;
	DataCo &operator=(const DataCo &rhs) = delete;
};

class DataSim
{
private:
	enum status
	{
		SEND_ACK,
		SEND_CONTENT,
		SEND_CONTENT2,
		PENDDING,
		PENDDING2
	};
	status status;
	bool channel_free;
	timepoint backoff_start;
	microseconds backoff_time;
	int max_backoff_const;
	unsigned totalFramesSent;
	bool send_started;
	int dst;
	SendData senddata;
	SendData ack;
	ReceiveData receivedata;
	SendData* data_sending;
	unsigned indexPacketSending;
	unsigned indexPacketReceiving;
	bool send_finished;
	bool receive_finished;
	bool demodulate();
	friend class Stream;
	class ack_entry
	{
		const int from;
		const int no;
		const bool last;
		friend class DataSim;
		friend int send_callback(const void *inputBuffer, void *outputBuffer,
			unsigned long framesPerBuffer,
			const PaStreamCallbackTimeInfo *timeInfo,
			PaStreamCallbackFlags statusFlags,
			void* userData);
		ack_entry(const int from_, const int no_, const bool last_);
	};
	std::queue<ack_entry> ack_queue;
public:
	explicit DataSim(const int dst_, const char *send_file = nullptr, void *data_sent_ = nullptr,
		void *data_rec_ = nullptr, SAMPLE *samples_sent_ = nullptr, SAMPLE *samples_rec_ = nullptr);
	DataSim(const DataSim &rhs) = delete;
    static int send_callback(const void *inputBuffer, void *outputBuffer,
                      unsigned long framesPerBuffer,
                      const PaStreamCallbackTimeInfo *timeInfo,
                      PaStreamCallbackFlags statusFlags,
                      void* userData);
    static int receive_callback(const void *inputBuffer, void *outputBuffer,
                         unsigned long framesPerBuffer,
                         const PaStreamCallbackTimeInfo *timeInfo,
                         PaStreamCallbackFlags statusFlags,
                         void* userData);
	DataSim &operator=(const DataSim &rhs) = delete;
};

const float sine[PERIOD] =
{ +0.38941834f, +0.92664883f, +0.92106099f, +0.37592812f, -0.38941834f, -0.92664883f, -0.92106099f, -0.37592812f };
//const float cosine[PERIOD] =
//{ +0.90044710f, +0.22134984f, -0.62442837f, -0.99999928f, -0.62255033f, +0.22369171f, +0.90148933f };
const float preamble[LEN_PREAMBLE] =
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

#endif