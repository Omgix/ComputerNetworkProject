#ifndef PASTREAM_H
#define PASTREAM_H

#include "portaudio.h"
#include <sndfile.hh>
#include <cstdint>
#include <chrono>
#include <queue>
#include <netinet/in.h>
#include "crc-8.h"
#include "crc.h"
#include <cstring>

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

#define SERVER_PORT 8888

const int SAMPLE_RATE = 44100;          // SAMPLE_RATE frames in a second
const int NUM_CHANNELS = 1;             // Mono channel will be used
const int FRAMES_PER_BUFFER = 512;      // Size of the buffer
const int BITS_INFO = 88;
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
const int LEN_PREAMBLE = 128;		// Length of the preamble in frames
const int PERIOD = 8;// (float)SAMPLE_RATE / FREQUENCY_CARRIER;
const int LEN_SIGNAL = 256;         // Length of the silence interval in frames
const int OPERATING_BANDWIDTH = 1000;   // Equal to bandwidth in bps
const float FREQUENCY_CARRIER = 5512.5;     // Frequency of carrier waive
const int NUM_CARRIRER = 1;
const int SAMPLES_PER_N_BIT = 4;//(float)SAMPLE_RATE / FREQUENCY_CARRIER;         //
const float CONSTANT_THRESHOLD = 0.80;  // Test
const float THRESHOLD_SILENCE = 0.75;
const float THRESHOLD_DEMODULATE = 0.f; // 2-PSK threshold to determine what we received is 1 or 0
const float THRESHOLD_ASSURING = 50.f;  // Convolution threshold of starting adjusting preamble threshold
const float A = 0.95f;                  // Amplitude of the sound
const float CONSTANT_AMPLIFY = 1;       // The constant that will be multiplied in the frames that Rx received.
const int MAX_TIME_RECORD = 120;         // Max recording time in seconds
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
const int OFFSET_NO = 24;
const int INDEX_BYTE_NO = OFFSET_NO / 8;
const int OFFSET_IP = 32;
const int INDEX_BYTE_IP = OFFSET_IP / 8;
const int OFFSET_PORT = 64;
const int INDEX_BYTE_PORT = OFFSET_PORT / 8;

const int TYPEID_CONTENT_NORMAL = 0;
const int TYPEID_CONTENT_LAST = 1;
const int TYPEID_ACK = 3;
const int TYPEID_ACK_LAST = 5;
const int TYPEID_ASK_NORMAL = 7;
const int TYPEID_ASK_SPEC = 8;
const int TYPEID_ANSWER = 9;
const int TYPEID_ANSWER_LAST = 11;
const int TYPEID_NONE = 0xf;
extern int NODE;

extern uint8_t data_sent[1024*1024];
extern uint8_t data_rec[1024*1024];
extern SAMPLE samples_sent[MAX_TIME_RECORD * SAMPLE_RATE];
extern SAMPLE samples_rec[MAX_TIME_RECORD * SAMPLE_RATE];

static size_t command_len(const char *str, size_t n)
{
	for (size_t i = 0; i < n; ++i)
		if (str[i] == '\r' && str [i + 1] == '\n')
			return i + 2;

	return 0;
}

static bool get_ipport_PASV(const char * msg, in_addr_t *ip, uint16_t *port)
{
	if (strncmp(msg, "227", 3) != 0)
		return false;
	in_addr_t result1 = 0;
	uint16_t result2 = 0;
	int i;
	for (i = 3; i < 512; ++i)
		if (msg[i] == '(')
			break;
	if (i == 512) return false;

	char tmp[512];
	size_t len = command_len(msg, 512) - i;
	memcpy(tmp, msg + i, len);
	char *save_ptr, *curr;
	curr = strtok_r(tmp, ",()", &save_ptr);
	result1 = result1 * 256 + atoi(curr);
	curr = strtok_r(nullptr, ",()", &save_ptr);
	result1 = result1 * 256 + atoi(curr);
	curr = strtok_r(nullptr, ",()", &save_ptr);
	result1 = result1 * 256 + atoi(curr);
	curr = strtok_r(nullptr, ",()", &save_ptr);
	result1 = result1 * 256 + atoi(curr);
	*ip = result1;
	curr = strtok_r(nullptr, ",()", &save_ptr);
	result2 = result2 * 256 + atoi(curr);
	curr = strtok_r(nullptr, ",()", &save_ptr);
	result2 = result2 * 256 + atoi(curr);
	*port = result2;
	return true;
}

static bool use_data_sock(const char *command)
{
	if (strncmp(command, "RETR", 4) == 0)
		return true;
	return false;
}

static inline int get_typeID (const uint8_t* packet) {
	return (packet[INDEX_BYTE_TYPEID] >> (OFFSET_TYPEID % 8)) & 0xf;
}

static void set_packet_header (uint8_t* packet, int src, int dst, int typeID,
						size_t size, int no, in_addr_t ip, uint16_t port)
{
	memset(packet, 0, BYTES_INFO);
	packet[INDEX_BYTE_SRC] |= (src << (OFFSET_SRC % 8));
	packet[INDEX_BYTE_DST] |= (dst << (OFFSET_DST % 8));
	packet[INDEX_BYTE_TYPEID] |= (typeID << (OFFSET_TYPEID % 8));
	packet[INDEX_BYTE_SIZE] |= (size << (OFFSET_SIZE % 8));
	packet[INDEX_BYTE_NO] |= (no << (OFFSET_NO % 8));
	*(in_addr_t*)(packet + INDEX_BYTE_IP) = ip;
	*(uint16_t*)(packet + INDEX_BYTE_PORT) = port;
	crc8_t crc8 = crc8_init();
	crc8 = crc8_update(crc8, packet, BYTES_INFO - 1);
	crc8 = crc8_finalize(crc8);
	memcpy(packet + BYTES_INFO - 1, &crc8, 1);
}

static inline uint16_t get_size (const uint8_t* packet) {
	return (packet[INDEX_BYTE_SIZE] >> (OFFSET_SIZE % 8)) & 0xffff;
}

static inline int get_no (const uint8_t* packet) {
	return (packet[INDEX_BYTE_NO] >> (OFFSET_NO % 8)) & 0xff;
}

static inline int get_src (const uint8_t* packet) {
	return (packet[INDEX_BYTE_SRC] >> (OFFSET_SRC % 8)) & 0x3;
}

static inline int get_dst (const uint8_t* packet) {
	return (packet[INDEX_BYTE_DST] >> (OFFSET_DST % 8)) & 0x3;
}

static inline in_addr_t get_ip (const uint8_t* packet) {
	return *(in_addr_t*)(packet + INDEX_BYTE_IP);
}

static inline uint16_t get_port (const uint8_t* packet) {
	return *(uint16_t*)(packet + INDEX_BYTE_PORT);
}

static inline void set_ip (const uint8_t* packet, in_addr_t ip) {
	*(in_addr_t*)(packet + INDEX_BYTE_IP) = ip;
}

static inline void set_port (const uint8_t* packet, uint16_t port) {
	*(uint16_t*)(packet + INDEX_BYTE_PORT) = port;
}

static void set_packet_CRC(uint8_t *packet)
{
	int size = get_size(packet);
	crc_t crc = crc_init();
	crc = crc_update(crc, packet, size + BYTES_INFO);
	crc = crc_finalize(crc);
	uint8_t *ptr = packet + size + BYTES_INFO;
	memcpy(ptr, &crc, BYTES_CRC);
}

class Stream
{
public:
    enum TransferMode
    {
        I_TO_A = 0x1,
        A_TO_I = 0x0,
        FTP = 0x2
    };
private:
	PaError						err;
	unsigned long				bufferSize;
	PaStreamParameters	        inputParameters;
	PaStreamParameters	        outputParameters;
	PaStream*					input_stream;
	PaStream*					output_stream;
	static int					numStream;
	int error();
	bool is_I2A(TransferMode mode_) { return mode_ & 0x1; }
    bool is_FTP(TransferMode mode_) { return mode_ & 0x2; }
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
	void send(DataCo &data, bool print = false, bool write_sent_waves = false, const char* file_wave_sent = nullptr,
		bool write_rec_waves = false, const char* file_wave_rec = nullptr);
	void receive(ReceiveData &data, bool text = false, bool writewaves = false, const char* file_name = nullptr);
	void receive(DataCo &data, bool text = false, bool write_sent_waves = false, const char* file_wave_sent = nullptr,
		bool write_rec_waves = false, const char* file_wave_rec = nullptr);
	void send_and_receive(DataSim &data, bool write_sent_waves = false, const char* file_wave_sent = nullptr,
		bool write_rec_waves = false, const char* file_wave_rec = nullptr);
	void transfer(TransferMode mode_, bool write_sent_waves = false, const char* file_wave_sent = nullptr,
                  bool write_rec_waves = false, const char* file_wave_rec = nullptr);
};

enum Mode
{
	TRANSMITTER = 0x1,
	RECEIVER = 0x2,
	GATEWAY = 0x4,
	FTP_CLIENT = 0x8
};

class SendData
{
private:
	friend class DataCo;
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
	unsigned bitIndex;                            // Index of the bit that will send
	unsigned totalBits;
	size_t size;                                  // Total number of bytes that will send.
	unsigned packetFrameIndex;                    // Index of the sound frame in a packet
	bool isPreamble;                              // Indicate if we will send a preamble frame
	bool isNewPacket;                             // Indicate if the next frame we will send is in a new packet
	unsigned ackNo;
	int dst;
    int src;
    int typeID;					 // Used in demodulation
    int noPacket;
    in_addr_t ip;
    uint16_t port;
    size_t packets;
    int bytesPacket; // Used in demodulation
    int nextSendNo;
    int nextRecvNo;
	inline void prepare_for_new_sending();
	bool in_mode(Mode mode_);
	void set_ack(const int dst, const int no, const bool last);
	int receive_udp_msg();
	friend class Stream;
	friend class DataSim;
public:
	explicit SendData(const char* file_name = nullptr, bool text = false, void *data_ = nullptr,
			SAMPLE *samples_ = nullptr,
	        microseconds timeout_ = microseconds(2000000), bool need_ack_ = false, Mode mode_ = TRANSMITTER,
	        bool *ack_received_ = nullptr, int dst = 0, in_addr_t ip_ = 0, uint16_t port_ = 0);
	SendData(void *src_, size_t size_, int typeID_, void *data_ , int dst, in_addr_t ip_, uint16_t port_,
			SAMPLE *samples_ = nullptr,
             microseconds timeout_ = microseconds(2000000), bool need_ack_ = false, Mode mode_ = TRANSMITTER,
             bool *ack_received_ = nullptr);
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
	friend class DataCo;
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
	unsigned packetFrameIndex;   // Index of the sound frame in a packet
	SAMPLE threshold;            // Convolution threshold, used in status DETECTING to find the first packet's preamble
	SAMPLE amplitude;
	unsigned bitsReceived;       // Number of bits that has received
	unsigned numSamplesReceived; // Number of samples (frames) that has received
	const unsigned maxNumFrames; // Max number of samples (frames) that can be received
	unsigned startFrom;
	int src;
	int dst;
	int typeID;					 // Used in demodulation
	int noPacket;
	int bytesPacket; // Used in demodulation
    int nextSendNo;
    int nextRecvNo;
	unsigned frameIndex;
	unsigned totalBytes;
	inline void prepare_for_new_packet();
	inline void prepare_for_receiving();
    inline bool in_mode(Mode mode_);
	//void writeThreshold();                  // Write the convolution value in each frames to a WAV file
	void correct_threshold();               // Adjusting convolution threshold in status ASSURING
	bool correlate_next();                  // Calculate the convolution
    int send_udp_msg();
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
	size_t write_to_file(const char* path = nullptr, bool text = false); // Write received content data to a TXT file
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
	void reset();
public:
	explicit DataCo(Mode mode_, const char *send_file = nullptr, bool text = false, void *data_sent_ = nullptr,
		void *data_rec_ = nullptr, SAMPLE *samples_sent_ = nullptr, SAMPLE *samples_rec_ = nullptr,
                    int dst_ = 0, in_addr_t ip_ = 0, uint16_t port_ = 0);
	DataCo(void *src_, size_t size_, int typeID_, Mode mode_, int dst_ , in_addr_t ip_ , uint16_t port_ ,
			void *data_sent_ , void *data_rec_ , SAMPLE *samples_sent_ = nullptr, SAMPLE *samples_rec_ = nullptr);
	DataCo(const DataCo &rhs) = delete;
	DataCo &operator=(const DataCo &rhs) = delete;
    int connect_FTP();
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
+0.00000000f, +0.04450900f, +0.17715485f, +0.39007528f, +0.65363628f, +0.89706958f, +0.99948622f, +0.81915204f,
+0.28794045f, -0.44826860f, -0.96638524f, -0.78071984f, +0.12787716f, +0.94621220f, +0.64278761f, -0.55889757f,
-0.92008680f, +0.29645653f, +0.95866785f, -0.35703746f, -0.86244156f, +0.70710678f, +0.42745447f, -0.99996035f,
+0.49071755f, +0.43227847f, -0.96819222f, +0.86334163f, -0.34202014f, -0.25193148f, +0.69568255f, -0.93022162f,
+0.99923253f, -0.97836921f, +0.93345272f, -0.90630779f, +0.91441262f, -0.95291024f, +0.99391004f, -0.98449693f,
+0.85142817f, -0.52598674f, +0.00000000f, +0.59953686f, -0.98087204f, +0.81089710f, -0.03561143f, -0.82118986f,
+0.88659931f, +0.08715574f, -0.97648835f, +0.41938741f, +0.84767038f, -0.56184768f, -0.85514276f, +0.39335253f,
+0.98480775f, +0.14375699f, -0.85142817f, -0.86691450f, -0.06407022f, +0.73907949f, +0.99771085f, +0.70710678f,
+0.30838140f, +0.92481119f, +0.88846157f, +0.22488598f, -0.59738122f, -0.99798947f, -0.69410434f, +0.09978708f,
+0.82321130f, +0.96553595f, +0.42631735f, -0.41390899f, -0.96188315f, -0.83088634f, -0.11328355f, +0.68427330f,
+0.99875795f, +0.60827973f, -0.21163957f, -0.88212669f, -0.92990904f, -0.32121487f, +0.51422507f, +0.98662517f,
+0.76262994f, +0.00036572f, -0.76229041f, -0.98670900f, -0.51461796f, +0.32076389f, +0.92974453f, +0.88235871f,
+0.21211982f, -0.60773431f, -0.99872832f, -0.68464467f, +0.11272369f, +0.83056882f, +0.96206130f, +0.41436751f,
-0.42576249f, -0.96539171f, -0.82356875f, -0.10032130f, +0.69373075f, +0.99801812f, +0.59780162f, -0.22435212f,
-0.88824889f, -0.92507634f, -0.30893273f, +0.52539548f, +0.98869241f, +0.75417071f, -0.01286952f, -0.77056455f,
-0.98453172f, -0.50329781f, +0.33320374f, +0.93442826f, +0.87607589f, +0.19920101f, -0.61817443f, -0.99930320f,
};

#endif