#include "pastream.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include "crc.h"
#include <random>
#include "crc-8.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <zconf.h>
#include <arpa/inet.h>

bool in = false;
bool first = true;
float square[MAX_TIME_RECORD * SAMPLE_RATE];

//fvec Preamble(preamble, LEN_PREAMBLE);

int SendData::sendCallback(const void *inputBuffer, void *outputBuffer,
    unsigned long framesPerBuffer,
    const PaStreamCallbackTimeInfo *timeInfo,
    PaStreamCallbackFlags statusFlags,
    void* userData)
{
    auto data = (SendData*)userData;
    auto wptr = (SAMPLE*)outputBuffer;
    unsigned &index = data->packetFrameIndex;
    unsigned long &fileFrameIndex = data->fileFrameIndex;

    (void)inputBuffer;
    (void)statusFlags;

    data->time_info = timeInfo;
    /* Stage of signal before sending data. */
    if (data->status == SendData::status::SIGNAL)
    {
        unsigned long frames = framesPerBuffer;
        if (data->in_mode(RECEIVER) && data->wait)
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
                    if (data->in_mode(TRANSMITTER))
                    {
                        if (std::chrono::system_clock::now() > data->time_send + data->ack_timeout) // Time out! Retransmit
                        {
                            data->times_sent++;
                            if (data->times_sent > TIMES_TRY)
                                return paComplete; // When reach there, wait is true;
                            data->wait = false;
                            if (data->ack_timeout == ACK_INIT_TIMEOUT)
                                data->ack_timeout -= DURATION_SIGNAL;
                            data->bitIndex -= index / SAMPLES_PER_N_BIT * NUM_CARRIRER;
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
                    else if (data->in_mode(RECEIVER))
                    {
                        if (*data->signal)
                            data->wait = false;
                    }
                    continue;
                }
                data->src = get_src(data->data + data->bitIndex / 8);
                data->typeID = get_typeID(data->data + data->bitIndex / 8);
                data->bytesPacket = get_size(data->data + data->bitIndex / 8);
                data->noPacket = get_no(data->data + data->bitIndex / 8);
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
                unsigned phase = (index++) % PERIOD;
                float frameSin = A * sine[phase];
                //float frameCos = A * cosine[phase];
                int offsetSin = data->bitIndex % BITS_PER_BYTE;
                int indexBytesSin= data->bitIndex / BITS_PER_BYTE;
                //int offsetCos = (data->bitIndex + 1) % BITS_PER_BYTE;
                //int indexBytesCos = (data->bitIndex + 1) / BITS_PER_BYTE;
                uint8_t bitSin = (data->data[indexBytesSin] & (1 << offsetSin)) >> offsetSin;
                //uint8_t bitCos = (data->data[indexBytesCos] & (1 << offsetCos)) >> offsetSin;
                if (bitSin == 0)
                    frameSin *= -1.0f;
                //if (bitCos == 0)
                    //frameCos *= -1.0f;
                float frame = frameSin;// +frameCos;
                data->samples[fileFrameIndex++] = frame;
                *wptr++ = frame;
                if (index % SAMPLES_PER_N_BIT == 0)
                    data->bitIndex += NUM_CARRIRER;
                if (index>= SAMPLES_PER_N_BIT * (data->bytesPacket + BYTES_INFO + BYTES_CRC) * 8 / NUM_CARRIRER)
                {
                    data->prepare_for_new_sending(); // Reset several index after sending a packet
                    if (data->need_ack || data->in_mode(RECEIVER))
                    {
                        data->time_send = std::chrono::system_clock::now();
                        data->wait = true;
                        *data->signal = false;
                    }
                    if (data->in_mode(TRANSMITTER) && data->typeID == TYPEID_CONTENT_LAST && !data->need_ack)
                        return paComplete;
                }

            }
        }

        return paContinue;
    }
}

SendData::SendData(const char* file_name, void *data_, SAMPLE *samples_, microseconds timeout_,
    bool need_ack_, Mode mode_, bool *ack_received_, int dst) :
    status(SIGNAL), fileFrameIndex(0), ext_data_ptr(data_ != nullptr), ext_sample_ptr(samples_ != nullptr), mode(mode_),
    bitIndex(0), packetFrameIndex(0), isPreamble(true), isNewPacket(true), need_ack(in_mode(TRANSMITTER) ? true : need_ack_),
    wait(!in_mode(TRANSMITTER)), size(0), totalFrames(0), data((uint8_t*)data_), samples(samples_), signal(ack_received_),
    ack_timeout(timeout_), times_sent(0), ackNo(0)
{
    if (in_mode(TRANSMITTER))
    {
        if (!in_mode(GATEWAY))
        {
            uint8_t info[BYTES_INFO] = {};
            info[INDEX_BYTE_SRC] |= (NODE << (OFFSET_SRC % 8));
            info[INDEX_BYTE_DST] |= (dst << (OFFSET_DST % 8));
            info[INDEX_BYTE_TYPEID] |= (TYPEID_CONTENT_NORMAL << (OFFSET_TYPEID % 8));

            FILE *file = fopen(file_name, "rb");

            if (!file)
            {
                fputs("Invalid file that will be sent!\n", stderr);
                system("pause");
                exit(-1);
            }

            fseek(file, 0, SEEK_END);
            size = ftell(file);
            rewind(file);
            size_t numPacket = ceil((float)size / BYTES_CONTENT);
            if (!ext_data_ptr)
                data = new uint8_t[size + numPacket * (BYTES_CRC + BYTES_INFO)];
            uint8_t *ptr = data;
            unsigned remain = size;

            for (int i = 0; i < numPacket; ++i)
            {
                unsigned numRead = remain < BYTES_CONTENT ? remain : BYTES_CONTENT;

                info[INDEX_BYTE_SIZE] = 0x0;
                info[INDEX_BYTE_SIZE] |= (numRead << (OFFSET_SIZE % 8));
                info[INDEX_BYTE_NO] = 0x0;
                info[INDEX_BYTE_NO] |= (i << (OFFSET_NO % 8));
                if (i == numPacket - 1)
                {
                    info[INDEX_BYTE_TYPEID] &= ~0xf0;
                    info[INDEX_BYTE_TYPEID] |= (TYPEID_CONTENT_LAST << (OFFSET_TYPEID % 8));
                }
                crc8_t crc8 = crc8_init();
                crc8 = crc8_update(crc8, info, BYTES_INFO - 1);
                crc8 = crc8_finalize(crc8);
                memcpy(info + BYTES_INFO - 1, &crc8, 1);

                memcpy(ptr, info, BYTES_INFO);
                ptr += BYTES_INFO;
                fread(ptr, 1, numRead, file);
                ptr -= BYTES_INFO;
                remain -= numRead;
                crc_t crc = crc_init();
                crc = crc_update(crc, ptr, numRead + BYTES_INFO);
                crc = crc_finalize(crc);
                ptr += numRead + BYTES_INFO;
                memcpy(ptr, &crc, BYTES_CRC);
                ptr += BYTES_CRC;
            }
            fclose(file);
            totalFrames = size * BITS_PER_BYTE * SAMPLES_PER_N_BIT / NUM_CARRIRER + LEN_SIGNAL
                          + (LEN_PREAMBLE + (BITS_CRC + BITS_INFO) * SAMPLES_PER_N_BIT / NUM_CARRIRER) * numPacket;
            totalBits = size * BITS_PER_BYTE + (BITS_CRC + BITS_INFO) * numPacket;
        }
        else
        {
            status = PENDING;
        }
    }
    else if (in_mode(RECEIVER))
    {
        size = BYTES_ACK_CONTENT;
        if (!ext_data_ptr)
            data = new uint8_t[size + BYTES_CRC + BYTES_INFO];

        memset(data, 0, size + BYTES_CRC + BYTES_INFO);
        data[INDEX_BYTE_SRC] |= (NODE << OFFSET_SRC);
        data[INDEX_BYTE_DST] |= (dst << OFFSET_DST);
        data[INDEX_BYTE_TYPEID] |= (TYPEID_ACK << OFFSET_TYPEID);
        data[INDEX_BYTE_SIZE] = 0x0;
        data[INDEX_BYTE_SIZE] |= (BYTES_ACK_CONTENT << (OFFSET_SIZE % 8));

        crc8_t crc8 = crc8_init();
        crc8 = crc8_update(crc8, data, BYTES_INFO - 1);
        crc8 = crc8_finalize(crc8);

        memcpy(data + BYTES_INFO - 1, &crc8, 1);
        crc_t crc = crc_init();
        crc = crc_update(crc, data, BYTES_ACK_CONTENT + BYTES_INFO);
        crc = crc_finalize(crc);
        memcpy(data + BYTES_ACK_CONTENT + BYTES_INFO, &crc, BYTES_CRC);
        totalFrames = size * BITS_PER_BYTE * SAMPLES_PER_N_BIT / NUM_CARRIRER + LEN_SIGNAL
            + (LEN_PREAMBLE + (BITS_CRC + BITS_INFO) * SAMPLES_PER_N_BIT / NUM_CARRIRER);
        totalBits = size * BITS_PER_BYTE + (BITS_CRC + BITS_INFO);
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
    if (in_mode(RECEIVER))
        bitIndex -= packetFrameIndex / SAMPLES_PER_N_BIT * NUM_CARRIRER;
}

bool SendData::in_mode(Mode mode_) {
    return mode & mode_;
}

void SendData::set_ack(const int dst, const int no, const bool last)
{
    if (in_mode(RECEIVER))
    {
        data[INDEX_BYTE_DST] &= ~0xc;
        data[INDEX_BYTE_DST] |= (dst << OFFSET_DST);
        data[INDEX_BYTE_NO] &= ~0xff;
        data[INDEX_BYTE_NO] |= (no << (OFFSET_NO % 8));
        if (last)
        {
            data[INDEX_BYTE_TYPEID] &= ~0xf0;
            data[INDEX_BYTE_TYPEID] |= (TYPEID_ACK_LAST << (OFFSET_TYPEID % 8));
        }

        crc8_t crc8 = crc8_init();
        crc8 = crc8_update(crc8, data, BYTES_INFO - 1);
        crc8 = crc8_finalize(crc8);
        memcpy(data + BYTES_INFO - 1, &crc8, 1);
        //printf("\n0x%08x\n", *(unsigned*)data);

        crc_t crc = crc_init();
        crc = crc_update(crc, data, BYTES_ACK_CONTENT + BYTES_INFO);
        crc = crc_finalize(crc);
        memcpy(data + BYTES_ACK_CONTENT + BYTES_INFO, &crc, BYTES_CRC);
    }
}

int SendData::receive_udp_msg(int n)
{
    int server_fd;
    struct sockaddr_in ser_addr;
    socklen_t len;
    ssize_t count;
    struct sockaddr_in client_addr;  //client_addr用于记录发送方的地址信息
    const char *ipAddress = "192.168.1.1";

    server_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if(server_fd < 0)
    {
        printf("create socket fail!\n");
        return -1;
    }

    memset(&ser_addr, 0, sizeof(ser_addr));
    ser_addr.sin_family = AF_INET;
    ser_addr.sin_addr.s_addr = inet_addr(ipAddress);  //注意网络序转换
    ser_addr.sin_port = htons(SERVER_PORT);  //注意网络序转换

    uint8_t *buf_rec = data;
    const int BUFF_LEN = 128;
    char buf_send [BUFF_LEN] = {};

    uint8_t info[BYTES_INFO] = {};
    info[INDEX_BYTE_SRC] |= (NODE << (OFFSET_SRC % 8));
    info[INDEX_BYTE_DST] |= (1 << (OFFSET_DST % 8));
    info[INDEX_BYTE_TYPEID] |= (TYPEID_CONTENT_NORMAL << (OFFSET_TYPEID % 8));

    for(int i = 0; i < n; i++)
    {
        memset(buf_rec, 0, BUFF_LEN);
        len = sizeof(client_addr);
        count = recvfrom(server_fd, buf_rec + BYTES_INFO, BUFF_LEN, 0, (struct sockaddr*)&client_addr, &len);
        if(count == -1)
        {
            printf("recieve data fail!\n");
            return -1;
        }

        info[INDEX_BYTE_SIZE] = 0x0;
        info[INDEX_BYTE_SIZE] |= (count << (OFFSET_SIZE % 8));
        info[INDEX_BYTE_NO] = 0x0;
        info[INDEX_BYTE_NO] |= (i << (OFFSET_NO % 8));

        memcpy(buf_rec, info, BYTES_INFO);
        sendto(server_fd, buf_send, BUFF_LEN, 0, (struct sockaddr*)&client_addr, len);  //发送信息给client，注意使用了client_addr结构体指针
        buf_rec += BYTES_INFO + count;
        totalBits += (BYTES_INFO + count) << 3;
    }

    close(server_fd);

    return 0;
}

sf_count_t SendData::write_samples_to_file(const char *path = nullptr)
{
    int format = SF_FORMAT_WAV | SF_FORMAT_FLOAT;
    int channels = NUM_CHANNELS;
    int srate = SAMPLE_RATE;
    if (!path) path = in_mode(RECEIVER)? "sendwaves_rx.wav" : "sendwaves_tx.wav";
    SndfileHandle file = SndfileHandle(path, SFM_WRITE, format, channels, srate);
    file.writeSync();

    return file.write(samples, fileFrameIndex);
}

int ReceiveData::receiveCallback(const void *inputBuffer, void *outputBuffer,
    unsigned long framesPerBuffer,
    const PaStreamCallbackTimeInfo *timeInfo,
    PaStreamCallbackFlags statusFlags,
    void* userData)
{
    auto data = (ReceiveData*)userData;
    auto rptr = (const SAMPLE*)inputBuffer;

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

    if (inputBuffer == nullptr)
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
    data(data_ == nullptr ? new uint8_t[max_time * SAMPLE_RATE / SAMPLES_PER_N_BIT * NUM_CARRIRER] : (uint8_t *)data_),
    samples(samples_ == nullptr ? new SAMPLE[max_time*SAMPLE_RATE] : samples_), noPacket(-1),
    packetFrameIndex(0), threshold(0.0f), amplitude(0.0f), packet(new uint8_t*[5]), nextRecvNo(0),
    bitsReceived(0), numSamplesReceived(0), maxNumFrames(max_time * SAMPLE_RATE), totalBytes(0),
    frameIndex(INITIAL_INDEX_RX), startFrom(0), mode(mode_), signal(ack_received_),
    need_ack(mode_ == TRANSMITTER ? true : need_ack_), typeID(TYPEID_NONE), src(-1), bytesPacket(-1)
{
    for (int i = 0; i < 5; ++i)
        packet[i] = new uint8_t[BYTES_CONTENT + BYTES_CRC + BYTES_INFO];
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
    frameIndex += LEN_PREAMBLE - 80;
    packetFrameIndex = 0;
    status = DETECTING;
    in = false;
    for (int i = 0; i < 5; ++i)
        memset(packet[i], 0, BYTES_CONTENT + BYTES_CRC + BYTES_INFO);
}

void ReceiveData::prepare_for_receiving()
{
    status = DETECTING;
    packetFrameIndex = 0;
    for (int i = 0; i < 5; ++i)
        memset(packet[i], 0, BYTES_CONTENT + BYTES_CRC + BYTES_INFO);
}

bool ReceiveData::in_mode(Mode mode_) {
    return mode & mode_;
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
    if (amplitude <= THRESHOLD_SILENCE || frameIndex < LEN_PREAMBLE) return false;
    SAMPLE cur = 0.0f;

    for (unsigned i = 0; i < LEN_PREAMBLE; ++i)
        cur += samples[frameIndex - LEN_PREAMBLE + 1 + i] * preamble[i];
    return abs(cur) >= threshold;
}

int ReceiveData::send_udp_msg(int n)
{
    int client_fd;
    struct sockaddr_in ser_addr;
    socklen_t len;
    struct sockaddr_in src;
    const char *ipAddress = "192.168.1.1";

    client_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if(client_fd < 0)
    {
        printf("Create socket fail!\n");
        return -1;
    }

    memset(&ser_addr, 0, sizeof(ser_addr));
    ser_addr.sin_family = AF_INET;
    ser_addr.sin_addr.s_addr = inet_addr(ipAddress);  //注意网络序转换
    ser_addr.sin_port = htons(SERVER_PORT);  //注意网络序转换

    uint8_t *buf_send = data;
    const int BUFF_LEN = 128;
    char buf_rec [BUFF_LEN] = {};

    uint8_t info[BYTES_INFO] = {};
    info[INDEX_BYTE_SRC] |= (NODE << (OFFSET_SRC % 8));
    info[INDEX_BYTE_DST] |= (1 << (OFFSET_DST % 8));
    info[INDEX_BYTE_TYPEID] |= (TYPEID_CONTENT_NORMAL << (OFFSET_TYPEID % 8));

    for(int i = 0; i < n; i++)
    {
        while (i + 1 != nextRecvNo) {}
        len = sizeof(ser_addr);
        int msg_len = get_size(buf_send);
        sendto(client_fd, buf_send + BYTES_INFO, (size_t)msg_len, 0, (struct sockaddr*)&ser_addr, len);
        memset(buf_rec, 0, BUFF_LEN);
        recvfrom(client_fd, buf_rec, BUFF_LEN, 0, (struct sockaddr*)&src, &len);  //接收来自server的信息
        sleep(1);  //一秒发送一次消息
    }

    close(client_fd);

    return 0;
}

size_t ReceiveData::write_to_file(const char* path = nullptr)
{
    unsigned n = 0;
    if (!path) path = "OUTPUT.bin";
    FILE *file = fopen(path, "wb");

    if (!file)
    {
        fprintf(stderr, "Invalid file!\n");
        exit(-1);
    }
    n = fwrite(data, sizeof(uint8_t), totalBytes, file);
    fclose(file);
    return n;
}

static int diff[MAX_TIME_RECORD * SAMPLE_RATE];

size_t ReceiveData::write_samples_to_file(const char* path = nullptr)
{
    int format = SF_FORMAT_WAV | SF_FORMAT_FLOAT;
    int channels = NUM_CHANNELS;
    int srate = SAMPLE_RATE;
    //SndfileHandle file1;
    //if (NODE == 1)
        //file1 = SndfileHandle("diff1.wav", SFM_WRITE, format, channels, srate);
    //else if (NODE == 2)
        //file1 = SndfileHandle("diff2.wav", SFM_WRITE, format, channels, srate);
    //file1.writeSync();
    //file1.write(diff, frameIndex);
    if (!path) path = in_mode(RECEIVER)? "reveivewaves_rx.wav" : "reveivewaves_tx.wav";
    SndfileHandle file = SndfileHandle(path, SFM_WRITE, format, channels, srate);
    file.writeSync();
    return file.write(samples, frameIndex);
}

bool ReceiveData::demodulate()
{
    while (frameIndex + 2 < numSamplesReceived)
    {
        diff[frameIndex] = numSamplesReceived - frameIndex;
        if (frameIndex == INITIAL_INDEX_RX)
        {
            for (int i = 0; i < PERIOD; ++i)
                amplitude += abs(samples[frameIndex - i]);
        }
        else
        {
            amplitude += abs(samples[frameIndex]);
            amplitude -= abs(samples[frameIndex - PERIOD]);
        }
        if (status == RECEIVING && !in && in_mode(RECEIVER)) {
            if (first)
            {
                printf("Start receiving!\n");
                printf("Start from frame #%u\n", frameIndex);
                printf("New packet from frame #%u\n", frameIndex);
                startFrom = frameIndex - LEN_SIGNAL - LEN_PREAMBLE;
                first = false;
            }
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
            if (getHeader && nextRecvNo != 0)
            {
                printf("New packet from frame #%u\n", frameIndex);
                status = RECEIVING;
                packetFrameIndex = 0;
            }
        }
        else if (status == RECEIVING)
        {
            packetFrameIndex++;

            if (packetFrameIndex % SAMPLES_PER_N_BIT == 0)
            {
                int indexBytePacketSin = (packetFrameIndex / SAMPLES_PER_N_BIT * NUM_CARRIRER - NUM_CARRIRER) / BITS_PER_BYTE;
                int offsetSin = (packetFrameIndex / SAMPLES_PER_N_BIT * NUM_CARRIRER -  NUM_CARRIRER) % BITS_PER_BYTE;
                //int indexBytePacketCos = (packetFrameIndex / SAMPLES_PER_N_BIT * NUM_CARRIRER - 1) / BITS_PER_BYTE;
                //int offsetCos = (packetFrameIndex / SAMPLES_PER_N_BIT * NUM_CARRIRER - 1) % BITS_PER_BYTE;
                for (int i = 0; i < 5; ++i)
                {
                    float valueSin = 0.0f;
                    //float valueCos = 0.0f;
                    for (int j = SAMPLES_PER_N_BIT - 1; j >= 0; --j)
                    {
                        unsigned phase = (packetFrameIndex - 1 - j) % PERIOD;
                        valueSin += samples[frameIndex - j - align[i]] * sine[phase];
                        //valueCos += samples[frameIndex - j - align[i]] * cosine[phase];
                    }
                    if (valueSin > THRESHOLD_DEMODULATE)
                        packet[i][indexBytePacketSin] = packet[i][indexBytePacketSin] | (1 << offsetSin);
                    //if (valueCos > THRESHOLD_DEMODULATE)
                        //packet[i][indexBytePacketCos] = packet[i][indexBytePacketCos] | (1 << offsetCos);
                }
                bitsReceived += NUM_CARRIRER;
            }
            frameIndex++;
            if (packetFrameIndex == SAMPLES_PER_N_BIT * BITS_INFO / NUM_CARRIRER)
            {
                int choice = -1;
                for (int i = 0; i < 5; ++i)
                {
                    crc8_t crc8 = crc8_init();
                    crc8 = crc8_update(crc8, packet[i], BYTES_INFO);
                    crc8 = crc8_finalize(crc8);
                    if (crc8 == 0x00) {
                        choice = i;
                        break;
                    }
                }
                if (choice == -1)
                {
                    if (need_ack)
                    {
                        bitsReceived -= BITS_INFO;
                        prepare_for_new_packet(); continue;
                    }
                    else
                        choice = 0;
                }
                int destination = get_dst(packet[choice]);
                //if (destination != NODE)
                //receivedata.prepare_for_new_packet(); // Check if the packet's dst is itself
                src = get_src(packet[choice]);
                typeID = get_typeID(packet[choice]);
                bytesPacket = get_size(packet[choice]);
                noPacket = get_no(packet[choice]);
                if (in_mode(RECEIVER) && need_ack && noPacket != nextRecvNo)
                {
                    bitsReceived -= BITS_INFO;
                    prepare_for_new_packet(); continue;
                }
            }
            if (packetFrameIndex >= SAMPLES_PER_N_BIT * (BYTES_INFO + bytesPacket + BYTES_CRC)*8 / NUM_CARRIRER)
            {
                unsigned bitsReceivedPacket = packetFrameIndex / SAMPLES_PER_N_BIT * NUM_CARRIRER;
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
                if (canAc || (in_mode(RECEIVER) && !need_ack))
                {
                    memcpy(wptr, packet[choice] + BYTES_INFO, bitsReceivedPacket / 8 - BYTES_CRC - BYTES_INFO);
                    nextRecvNo = noPacket + 1;
                    totalBytes += bytesPacket;
                }
                else if (in_mode(RECEIVER))
                    bitsReceived -= bitsReceivedPacket;

                if ((need_ack && in_mode(RECEIVER) && canAc) || in_mode(TRANSMITTER))
                {
                    *signal = true;
                }

                if (in_mode(RECEIVER) && typeID == TYPEID_CONTENT_LAST)
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

DataSim::DataSim(const int dst_, const char *send_file, void *data_sent_, void *data_rec_,
                SAMPLE *samples_sent_, SAMPLE *samples_rec_) : 
    backoff_start(), backoff_time(100000), max_backoff_const(1), status(PENDDING),
    senddata(send_file, data_sent_, samples_sent_, ACK_JAMMING_TIMEOUT, true, TRANSMITTER, nullptr, dst_),
    ack(nullptr, nullptr, nullptr, ACK_JAMMING_TIMEOUT, true, RECEIVER, nullptr, dst_), totalFramesSent(0),
    receivedata(MAX_TIME_RECORD_SIMUL, data_rec_, samples_rec_, true, RECEIVER, nullptr), channel_free(false),
    send_started(false), dst(dst_), indexPacketSending(0), indexPacketReceiving(0), ack_queue(), data_sending(nullptr),
    send_finished(false), receive_finished(false)
{
    senddata.status = SendData::status::CONTENT;
    ack.status = SendData::status::CONTENT;
    receivedata.status = ReceiveData::status::DETECTING;
    receivedata.threshold = 170.f;
}

DataSim::ack_entry::ack_entry(const int from_, const int no_, const bool last_): 
    from(from_), no(no_), last(last_)
{}

int DataSim::send_callback(const void *inputBuffer, void *outputBuffer,
                unsigned long framesPerBuffer,
                const PaStreamCallbackTimeInfo *timeInfo,
                PaStreamCallbackFlags statusFlags,
                void* userData)
{
    (void)timeInfo;
    (void)statusFlags;
    (void)inputBuffer;

    auto simdata = (DataSim*)userData;
    auto wptr = (SAMPLE*)outputBuffer;
    SendData *&data = simdata->data_sending;

    SendData &senddata = simdata->senddata;
    SAMPLE *&samples = simdata->senddata.samples;
    SendData &ack = simdata->ack;
    bool &channel_free = simdata->channel_free;
    timepoint &backoff_start = simdata->backoff_start;
    microseconds &backoff_time = simdata->backoff_time;
    int &max_backoff_const = simdata->max_backoff_const;
    unsigned &totalFramesSent = simdata->totalFramesSent;
    bool &send_started = simdata->send_started;
    std::queue<DataSim::ack_entry> &ack_queue = simdata->ack_queue;

    for (unsigned long i = 0; i < framesPerBuffer; ++i)
    {
        if (simdata->status == DataSim::status::PENDDING || simdata->status == DataSim::status::PENDDING2)
        {
            *wptr++ = SAMPLE_SILENCE;
            samples[totalFramesSent++] = SAMPLE_SILENCE;
            // check if need retransmit by the time spent from the sending of the last content packet
            if (send_started && senddata.wait)
            {
                if (std::chrono::system_clock::now() > senddata.time_send + senddata.ack_timeout) // Need retransmit
                {
                    static std::default_random_engine e(NODE);
                    static std::uniform_int_distribution<int> u(0, 1023);
                    senddata.bitIndex -= senddata.packetFrameIndex / SAMPLES_PER_N_BIT * NUM_CARRIRER;
                    senddata.times_sent++;
                    if (senddata.times_sent > TIMES_TRY) return paComplete;
                    senddata.wait = false;
                    max_backoff_const *= 2;
                    backoff_time = (u(e) % max_backoff_const) * SLOT;
                }
            }
            if (channel_free) // Check if the channel is free.
            {
                if (!ack_queue.empty())
                {
                    DataSim::ack_entry nextACK = ack_queue.front();
                    ack_queue.pop();
                    ack.set_ack(nextACK.from, nextACK.no, nextACK.last);
                    data = &ack;
                    simdata->status = DataSim::SEND_ACK;
                }
                /*else if (std::chrono::system_clock::now() - backoff_start < backoff_time + DIFS) // In backoff
                {
                    continue;
                }*/
                else if (!senddata.wait || !send_started || 
                    (senddata.wait && simdata->status == DataSim::status::PENDDING2))
                {
                    if (simdata->indexPacketSending < ceil((float)senddata.size / BYTES_CONTENT))
                    {
                        data = &senddata;
                        //data->bitIndex = simdata->indexPacketReceiving * BITS_NORMALPACKET;
                        if ((senddata.wait && simdata->status == DataSim::status::PENDDING2))
                        simdata->status = DataSim::SEND_CONTENT;
                    }
                }
            }
            else if (!channel_free)
            {
                if (simdata->receivedata.status == ReceiveData::RECEIVING) backoff_time = -DIFS;
                backoff_start = std::chrono::system_clock::now();
                continue;
            }
        }
        else if (simdata->status == DataSim::SEND_ACK || simdata->status == DataSim::SEND_CONTENT)
        {
            unsigned &index = data->packetFrameIndex;

            if (simdata->status == DataSim::SEND_CONTENT) send_started = true;
            if (data->isNewPacket)
            {
                data->isNewPacket = false;
                data->isPreamble = true;
                index = 0;
            }
            if (data->isPreamble)
            {
                /* If in the process of generating preamble of 512 samples, 11.6ms */
                *wptr++ = preamble[index];
                samples[totalFramesSent++] = preamble[index];
                if (++index >= LEN_PREAMBLE)
                {
                    index = 0;
                    data->isPreamble = false;
                }
            }
            else
            {
                /* Demodulation part, need not to know the detail */
                unsigned phase = (index++) % PERIOD;
                float frameSin = A * sine[phase];
                //float frameCos = A * cosine[phase];
                int offsetSin = data->bitIndex % BITS_PER_BYTE;
                int indexBytesSin = data->bitIndex / BITS_PER_BYTE;
                //int offsetCos = (data->bitIndex + 1) % BITS_PER_BYTE;
                //int indexBytesCos = (data->bitIndex + 1) / BITS_PER_BYTE;
                uint8_t bitSin = (data->data[indexBytesSin] & (1 << offsetSin)) >> offsetSin;
                //uint8_t bitCos = (data->data[indexBytesCos] & (1 << offsetCos)) >> offsetSin;
                if (bitSin == 0)
                    frameSin *= -1.0f;
                //if (bitCos == 0)
                    //frameCos *= -1.0f;
                float frame = frameSin;// +frameCos;
                samples[totalFramesSent++] = frame;
                *wptr++ = frame;
                if (index % SAMPLES_PER_N_BIT == 0)
                    data->bitIndex += NUM_CARRIRER;
                if ((simdata->status == DataSim::SEND_CONTENT &&  // A packet has been sent, do something before pending
                        (index >= SAMPLES_PER_N_BIT * BITS_NORMALPACKET / NUM_CARRIRER || data->bitIndex >= data->totalBits)) 
                    || (simdata->status == DataSim::SEND_ACK && index >= SAMPLES_PER_N_BIT * BITS_ACKPACKET / NUM_CARRIRER))
                {
                    if (simdata->status == DataSim::SEND_ACK)
                        data->bitIndex -= index / SAMPLES_PER_N_BIT * NUM_CARRIRER;
                    else if (simdata->status == DataSim::SEND_CONTENT)
                    {
                        static std::default_random_engine e(NODE);
                        static std::uniform_int_distribution<int> u(600, 1023);

                        data->ack_timeout = microseconds(u(e) * 1000);
                        data->time_send = std::chrono::system_clock::now();
                        data->wait = true;
                        max_backoff_const = 1;
                    }
                    simdata->status = DataSim::PENDDING;
                    backoff_start = std::chrono::system_clock::now();
                    data->isNewPacket = true;
                }
            }
        }
    }
    return paContinue;
}

int DataSim::receive_callback(const void *inputBuffer, void *outputBuffer,
    unsigned long framesPerBuffer,
    const PaStreamCallbackTimeInfo *timeInfo,
    PaStreamCallbackFlags statusFlags,
    void* userData)
{
    auto rptr = (const SAMPLE*)inputBuffer;
    auto data = (DataSim*)userData;
    ReceiveData &receivedata = data->receivedata;
    bool &channel_free = data->channel_free;

    (void)outputBuffer; /* Prevent unused variable warnings. */
    (void)timeInfo;
    (void)statusFlags;

    unsigned long i;
    int finished;
    unsigned long framesCalc;

    if (receivedata.maxNumFrames - receivedata.numSamplesReceived <= framesPerBuffer)
    {
        finished = paComplete;
        framesCalc = receivedata.maxNumFrames - receivedata.numSamplesReceived;
    }
    else
    {
        finished = paContinue;
        framesCalc = framesPerBuffer;
    }

    if (inputBuffer == NULL)
    {
        for (i = 0; i < framesCalc; i++)
            receivedata.samples[receivedata.numSamplesReceived++] = SAMPLE_SILENCE;
    }
    else
    {
        for (i = 0; i < framesCalc; i++) 
        {
            receivedata.samples[receivedata.numSamplesReceived++] = (*rptr++) * CONSTANT_AMPLIFY;
            if (receivedata.numSamplesReceived >= 60)
            {
                float ampl = 0.0f;
                float sqr = 0.0f;
                //for (int j = 0; j < 6; ++j)
                    //ampl += receivedata.samples[receivedata.numSamplesReceived - j];
                for (int j = 0; j < 30; ++j)
                    sqr += receivedata.samples[receivedata.numSamplesReceived - j] 
                            * receivedata.samples[receivedata.numSamplesReceived - j];
                square[receivedata.numSamplesReceived] = sqr;
                diff[receivedata.numSamplesReceived] = receivedata.numSamplesReceived - data->totalFramesSent;
                if (sqr > 0.25)
                    channel_free = true;
                else
                    channel_free = true;
            }
        }
    }
    return finished;
}

bool DataSim::demodulate()
{
    unsigned &frameIndex = receivedata.frameIndex;
    unsigned &numSamplesReceived = receivedata.numSamplesReceived;
    unsigned &packetFrameIndex = receivedata.packetFrameIndex;
    unsigned &bitsReceived = receivedata.bitsReceived;
    int &typeID = receivedata.typeID;
    int &src = receivedata.src;
    int &noPacket = receivedata.noPacket;
    int &bytesPacketContent = receivedata.bytesPacket;
    SAMPLE &amplitude = receivedata.amplitude;
    SAMPLE *samples = receivedata.samples;

    while (frameIndex + 2 < numSamplesReceived)
    {
        if (frameIndex == INITIAL_INDEX_RX)
        {
            for (int i = 0; i < PERIOD; ++i)
                amplitude += abs(samples[frameIndex - i]);
        }
        else
        {
            amplitude += abs(samples[frameIndex]);
            amplitude -= abs(samples[frameIndex - PERIOD]);
        }
        if (receivedata.status == ReceiveData::status::DETECTING)
        {
            packetFrameIndex++;

            bool getHeader = receivedata.correlate_next();
            frameIndex++;
            if (getHeader)
            {
                receivedata.status = ReceiveData::status::RECEIVING;
                packetFrameIndex = 0;
            }
        }
        else if (receivedata.status == ReceiveData::status::RECEIVING)
        {
            packetFrameIndex++;
            if (packetFrameIndex % SAMPLES_PER_N_BIT == 0)
            {
                int indexBytePacketSin = (packetFrameIndex / SAMPLES_PER_N_BIT * NUM_CARRIRER - NUM_CARRIRER) / BITS_PER_BYTE;
                int offsetSin = (packetFrameIndex / SAMPLES_PER_N_BIT * NUM_CARRIRER - NUM_CARRIRER) % BITS_PER_BYTE;
                //int indexBytePacketCos = (packetFrameIndex / SAMPLES_PER_N_BIT * NUM_CARRIRER - 1) / BITS_PER_BYTE;
                //int offsetCos = (packetFrameIndex / SAMPLES_PER_N_BIT * NUM_CARRIRER - 1) % BITS_PER_BYTE;
                for (int i = 0; i < 5; ++i)
                {
                    float valueSin = 0.0f;
                    //float valueCos = 0.0f;
                    for (int j = SAMPLES_PER_N_BIT - 1; j >= 0; --j)
                    {
                        unsigned phase = (packetFrameIndex - 1 - j) % PERIOD;
                        valueSin += samples[frameIndex - j - receivedata.align[i]] * sine[phase];
                        //valueCos += samples[frameIndex - j - align[i]] * cosine[phase];
                    }
                    if (valueSin > THRESHOLD_DEMODULATE)
                        receivedata.packet[i][indexBytePacketSin] |= (1 << offsetSin);
                    //if (valueCos > THRESHOLD_DEMODULATE)
                        //packet[i][indexBytePacketCos] = packet[i][indexBytePacketCos] | (1 << offsetCos);
                }
                bitsReceived += NUM_CARRIRER;
            }
            frameIndex++;
            if (packetFrameIndex == SAMPLES_PER_N_BIT * BITS_INFO / NUM_CARRIRER)
            {
                int choice = -1;
                for (int i = 0; i < 5; ++i)
                {
                    crc8_t crc8 = crc8_init();
                    crc8 = crc8_update(crc8, receivedata.packet[i], BYTES_INFO);
                    crc8 = crc8_finalize(crc8);
                    if (crc8 == 0x00) {
                        choice = i;
                        break;
                    }
                }
                if (choice == -1)
                {
                    bitsReceived -= BITS_INFO;
                    receivedata.prepare_for_new_packet(); continue;
                }
                int destination = (receivedata.packet[choice][INDEX_BYTE_DST] >> (OFFSET_DST % 8)) & 0x3;
                //if (destination != NODE) 
                    //receivedata.prepare_for_new_packet(); // Check if the packet's dst is itself
                src = (receivedata.packet[choice][INDEX_BYTE_SRC] >> (OFFSET_SRC % 8)) & 0x3;
                typeID = (receivedata.packet[choice][INDEX_BYTE_TYPEID] >> (OFFSET_TYPEID % 8)) & 0xf;
                bytesPacketContent = (receivedata.packet[choice][INDEX_BYTE_SIZE] >> (OFFSET_SIZE % 8)) & 0xff;
                noPacket = (receivedata.packet[choice][INDEX_BYTE_NO] >> (OFFSET_NO % 8)) & 0xff;
                if (noPacket == 83)
                    int a = 0;
                //printf("packet %d: type: %d, index: %d, src: %d, bytes: %d\n", noPacket, typeID, frameIndex, src, bytesPacketContent);
            }
            if (packetFrameIndex == SAMPLES_PER_N_BIT * (BITS_INFO + bytesPacketContent * 8 + BITS_CRC) / NUM_CARRIRER)
            {
                unsigned bitsReceivedPacket = packetFrameIndex / SAMPLES_PER_N_BIT * NUM_CARRIRER;
                unsigned indexByte = (bitsReceived - bitsReceivedPacket) / BITS_NORMALPACKET * BYTES_CONTENT;
                uint8_t *wptr = receivedata.data + indexByte;
                int choice = -1;
                for (int i = 0; i < 5; ++i)
                {
                    crc_t crc = crc_init();
                    crc = crc_update(crc, receivedata.packet[i], bitsReceivedPacket / BITS_PER_BYTE);
                    crc = crc_finalize(crc);
                    if (crc == 0x48674bc7) {
                        choice = i;
                        break;
                    }
                }
                if (choice == -1)
                {
                    bitsReceived -= bitsReceivedPacket;
                    receivedata.prepare_for_new_packet();
                    continue;
                }
                else if (typeID == TYPEID_ACK || typeID == TYPEID_ACK_LAST)
                {
                    bitsReceived -= bitsReceivedPacket;
                    if (noPacket == indexPacketSending)
                    {
                        //printf("***Get ACK for packet %d, src: %d\n", noPacket, src);
                        senddata.wait = false;
                        indexPacketSending++;
                    }
                    if (typeID == TYPEID_ACK_LAST) send_finished = true;
                }
                else if (typeID == TYPEID_CONTENT_NORMAL || typeID == TYPEID_CONTENT_LAST)
                {
                    //printf("Packet %d, src: %d, want: %d\n", noPacket, src, indexPacketReceiving);
                    if (noPacket == indexPacketReceiving)
                    {
                        memcpy(wptr, receivedata.packet[choice] + BYTES_INFO, bytesPacketContent);
                        indexPacketReceiving++;
                    }
                    else if (noPacket != indexPacketReceiving)
                        bitsReceived -= bitsReceivedPacket;
                    ack_queue.push(ack_entry(src, noPacket, typeID == TYPEID_CONTENT_LAST));
                    if (typeID == TYPEID_CONTENT_LAST) receive_finished = true;
                }
                src = -1;
                typeID = -1;
                bytesPacketContent = -1;
                noPacket = -1;
                receivedata.prepare_for_new_packet();
            }
        }
    }
    return false;
}