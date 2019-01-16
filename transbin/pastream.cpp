#include "pastream.h"
#include <cstdio>
#include <arpa/inet.h>

int Stream::error() {
    Pa_Terminate();
    printf("An error occurred while using the portaudio stream\n");
    printf("Error number: %d\n", err);
    printf("Error message: %s\n", Pa_GetErrorText(err));
    printf("Press any key to exit...\n");
    system("pause");
    exit(err);
}

void Stream::set_input_para(int receiveDeviceNo)
{
    if (receiveDeviceNo >= 0)
    {
        inputParameters.device = receiveDeviceNo;
        inputParameters.channelCount = NUM_CHANNELS;
        inputParameters.sampleFormat = PA_SAMPLE_TYPE;
        inputParameters.suggestedLatency = Pa_GetDeviceInfo(inputParameters.device)->defaultLowInputLatency;
        inputParameters.hostApiSpecificStreamInfo = nullptr;
    }
}

void Stream::set_output_para(int sendDeviceNo)
{
    if (sendDeviceNo >= 0)
    {
        outputParameters.device = sendDeviceNo;
        outputParameters.channelCount = NUM_CHANNELS;
        outputParameters.sampleFormat = PA_SAMPLE_TYPE;
        outputParameters.suggestedLatency = Pa_GetDeviceInfo(outputParameters.device)->defaultLowInputLatency;
        outputParameters.hostApiSpecificStreamInfo = nullptr;
    }
}

Stream::Stream(int sendDeviceNo, int receiveDeviceNo) :
            err(!numStream ? Pa_Initialize() : paNoError),
            bufferSize(FRAMES_PER_BUFFER),
            input_stream(nullptr), output_stream(nullptr)
{
    if (err != paNoError) error();
    numStream++;
    set_input_para(receiveDeviceNo);
    set_output_para(sendDeviceNo);
}

Stream::~Stream()
{
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
            data);
    if (err != paNoError) error();
}

void Stream::open_input_stream(DataSim *data, PaStreamCallback callback)
{
    err = Pa_OpenStream(
        &input_stream,
        &inputParameters,
        nullptr,
        SAMPLE_RATE,
        FRAMES_PER_BUFFER,
        paClipOff,
        callback,
        data);
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

void Stream::open_output_stream(DataSim *data, PaStreamCallback callback)
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
    open_output_stream(&data, SendData::sendCallback);
    start_output_stream();

    printf("Waiting for sending to finish.\n");
    unsigned total = data.totalBits;
    while ((err = Pa_IsStreamActive(output_stream)) == 1)
    {
        printf("% 3.2f%% Completed\r", (float)data.bitIndex * 100 / total); fflush(stdout);
        Pa_Sleep(100);
    }
    printf("% 3.2f%% Completed\n", (float)data.bitIndex * 100 / total);
    if (err < 0) error();
    stop_output_stream();
    close_output_stream();
    printf("\n###################### Sending has been done. #######################\n");
    if (writewaves)
    {
        printf("**You choose get waves sent, now writing\n");
        size_t n = data.write_samples_to_file(file_name);
        printf("**Writing waves sent is finished, %zu samples have been write to in total.\n", n);
    }
}

void Stream::send(DataCo &data, bool print, bool write_sent_waves, const char* file_wave_sent,
                bool write_rec_waves, const char* file_wave_rec)
{
    if (print)
        printf("Waiting for sending to finish.\n");
    open_input_stream(&data.receive_data, ReceiveData::receiveCallback);
    open_output_stream(&data.send_data, SendData::sendCallback);
    start_input_stream();
    start_output_stream();
    uint8_t *buf = data.receive_data.data;
    int no = 0;
    while ((err = Pa_IsStreamActive(output_stream)) == 1 && (err = Pa_IsStreamActive(input_stream)) == 1)
    {
        if (no < data.receive_data.nextRecvNo)
        {
            int bytes = get_size(buf);
            if (get_typeID(buf) == TYPEID_ANSWER || get_typeID(buf) == TYPEID_ANSWER_LAST)
            {
                for (int k = 0; k < bytes; ++k)
                    printf("%c", *(buf + BYTES_INFO + k));
            }
            no++;
            buf += BYTES_INFO + bytes;
        }
    }
    if (err < 0) error();
    stop_input_stream();
    stop_output_stream();
    close_input_stream();
    close_output_stream();
    if (data.send_data.wait)
        printf("\nLINK ERROR!!\n");
    else if (print)
        printf("\n###################### Sending has been done. #######################\n");
    if (write_sent_waves)
    {
        printf("**You choose get waves sent, now writing\n");
        size_t n = data.send_data.write_samples_to_file(file_wave_sent);
        printf("**Writing waves sent is finished, %zu samples have been write to in total.\n", n);
    }
    if (write_rec_waves)
    {
        printf("**You choose get waves received, now writing\n");
        size_t n = data.receive_data.write_samples_to_file(file_wave_rec);
        printf("**Writing samples recorded is finished, %zu samples have been write to in total.\n", n);
    }
}

void Stream::receive(ReceiveData &data, bool text, bool writewaves, const char* file_name)
{
    open_input_stream(&data, ReceiveData::receiveCallback);
    start_input_stream();

    printf("\n===================== Now receiving!! Please wait. =====================\n"); fflush(stdout);

    Pa_Sleep(20);
    while ((err = Pa_IsStreamActive(input_stream)) == 1)
    {
        printf("%d bytes received\r", data.totalBytes); fflush(stdout);
        Pa_Sleep(100);
    }
    if (err < 0) error();
    stop_input_stream();
    close_input_stream();
    printf("\n#### Receiving is finished!! Now write the data to the file OUTPUT.bin. ####\n");
    printf("Threshold: %f\n", data.threshold);
    size_t n = data.write_to_file(nullptr, text);
    printf("**Writing is finished, %zu bytes have been write to in total.\n", n);
    if (writewaves)
    {
        n = data.write_samples_to_file(file_name);
        printf("**Writing samples recorded is finished, %zu samples have been write to in total.\n", n);
    }
}

void Stream::receive(DataCo &data, bool save, const char *filename, bool text, bool write_sent_waves,
        const char* file_wave_sent, bool write_rec_waves, const char* file_wave_rec)
{
    open_output_stream(&data.send_data, SendData::sendCallback);
    open_input_stream(&data.receive_data, ReceiveData::receiveCallback);
    start_output_stream();
    start_input_stream();

    //printf("Waiting for receiving to finish.\n");
    int no = 0;
    uint8_t  *buf = data.receive_data.data;

    printf("\n--------------- Reply From the FTP Server ------------\n");
    while ((err = Pa_IsStreamActive(input_stream)) == 1)
    {
        //printf("%d bytes received\r", data.receive_data.totalBytes); fflush(stdout);
        //Pa_Sleep(100);
        if (no < data.receive_data.nextRecvNo)
        {
            int bytes = get_size(buf);
            if (get_typeID(buf) == TYPEID_ANSWER || get_typeID(buf) == TYPEID_ANSWER_LAST)
            {
                for (int k = 0; k < bytes; ++k)
                    printf("%c", *(buf + BYTES_INFO + k));
            }
            no++;
            buf += BYTES_INFO + bytes;
        }
    }
    printf("--------------------------------------------------------\n\n");
    if (err < 0) error();
    Pa_Sleep(500);
    stop_input_stream();
    close_input_stream();
    stop_output_stream();
    close_output_stream();
    if (save)
        printf("\n#### Receiving is finished!! Now write the data to the file ####\n");
    //printf("Threshold: %f\n", data.receive_data.threshold);
    if (save)
    {
        size_t n = data.receive_data.write_to_file(filename, text);
        printf("Writing file received is finished, %zu bytes have been write to in total.\n", n);
    }
    if (write_rec_waves)
    {
        printf("**You choose get waves received, now writing\n");
        size_t n = data.receive_data.write_samples_to_file(file_wave_rec);
        printf("**Writing samples recorded is finished, %zu samples have been write to in total.\n", n);
    }
    if (write_sent_waves)
    {
        printf("**You choose get waves sent, now writing\n");
        size_t n = data.send_data.write_samples_to_file(file_wave_sent);
        printf("**Writing waves sent is finished, %zu samples have been write to in total.\n", n);
    }
}

uint8_t tran[1024*1024];

void Stream::transfer(TransferMode mode_, bool write_sent_waves, const char *file_wave_sent,
                      bool write_rec_waves, const char *file_wave_rec)
{
    if (is_FTP(mode_))
    {
        int control_sock;
        int data_sock;
        struct sockaddr_in server;
        struct sockaddr_in server_data;
        const int read_len = 512;
        uint8_t read_buf [read_len];
        const int send_len = 600;
        char send_buf [read_len];
        uint8_t *ptr = data_rec;
        memset(&server, 0, sizeof(struct sockaddr_in));
        memset(&server_data, 0, sizeof(struct sockaddr_in));

        control_sock = socket(AF_INET, SOCK_STREAM, 0);
        if (control_sock == -1)
        {
            perror("Could not create controll socket");
            return;
        }
        data_sock = socket(AF_INET, SOCK_STREAM, 0);
        if (data_sock == -1)
        {
            perror("Could not create data socket");
            return;
        }
        DataCo data(RECEIVER, nullptr, false, data_sent, ptr, samples_sent, samples_rec,
                    2, inet_addr("127.0.0.1"), 8888);
        receive(data);
        server.sin_addr.s_addr = get_ip(ptr);
        server.sin_port = htons(get_port(ptr));
        server.sin_family = AF_INET;
        server_data.sin_family = AF_INET;

        uint16_t port = server.sin_port;
        char ip [INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(server.sin_addr), ip, INET_ADDRSTRLEN);

        printf("Connect to <%s,%d>\n", ip, port);

        if (connect(control_sock,(struct sockaddr *)&server, sizeof server ) < 0)
        {
            printf("Error : Connect Failed \n");
            return;
        }
        read(control_sock, read_buf, read_len);
        size_t act_len = command_len((char *)read_buf, read_len);
        read_buf[read_len - 1] = '\0';
        printf("Message: %zu\n%s", act_len, read_buf);
        DataCo data2(read_buf, act_len, TYPEID_ANSWER, TRANSMITTER, 2,0,
                              21, data_sent, data_rec, samples_sent, samples_rec);
        send(data2);
        while (true)
        {
            DataCo data3(RECEIVER, nullptr, false, data_sent, data_rec, samples_sent, samples_rec,
                        2, inet_addr("127.0.0.1"), 8888);
            receive(data3);
            if (get_typeID(ptr) == TYPEID_CONTENT_LAST)
                break;
            int msg_len = get_size(ptr);
            memcpy(send_buf, ptr + BYTES_INFO, msg_len);
            printf("From client %d: ", msg_len);
            for (int i = 0; i < msg_len; ++i)
                printf("%c", *(send_buf + i));
            printf("\n");
            write(control_sock, send_buf, msg_len);
            act_len = read(control_sock, read_buf, read_len);
            read_buf[act_len] = '\0';
            printf("Message: %zu\n%s", act_len, read_buf);
            DataCo data4(read_buf, act_len, TYPEID_ANSWER, TRANSMITTER, 2,0,
                         21, data_sent, data_rec, samples_sent, samples_rec);
            send(data4);
            if (strncmp(send_buf, "PASV", 4) == 0)
            {
                printf("Connect data socket.\n");
                if (get_ipport_PASV((char *)read_buf, &server_data.sin_addr.s_addr, &server_data.sin_port))
                    if (connect(data_sock,(struct sockaddr *)&server_data, sizeof server_data) < 0)
                    {
                        printf("Error: Data Connect Failed \n");
                        return;
                    }
            }
            if (use_data_sock(send_buf) && read_buf[0] == '1')
            {
                int typeID = strncmp(send_buf, "LIST", 4) == 0?
                        TYPEID_ANSWER : TYPEID_CONTENT_NORMAL;
                act_len = 0;
                for(int i = 0; ;i++ ) {
                    size_t new_len = read(data_sock, tran + act_len, read_len);
                    act_len += new_len;
                    printf("Read %zu--\n", new_len);
                    if (new_len == 0) break;
                }
                DataCo data5(tran, act_len, typeID, TRANSMITTER, 2,0,
                             21, data_sent, data_rec, samples_sent, samples_rec);
                send(data5);
            }
        }
        close(data_sock);
        const char *quit_msg = "QUIT\r\n";
        write(control_sock, quit_msg, strlen(quit_msg));
        read(control_sock, read_buf, read_len);
        act_len = command_len((char *)read_buf, read_len);
        DataCo data4(read_buf, act_len, TYPEID_ANSWER, TRANSMITTER, 2,0,
                     21, data_sent, data_rec, samples_sent, samples_rec);
        send(data4);
        close(control_sock);
    }
    else
    {
        Mode direct;
        if (is_I2A(mode_))
            direct = TRANSMITTER;
        else
            direct = RECEIVER;

        DataCo data ((Mode)(direct | GATEWAY), nullptr, false, data_sent, data_rec, samples_sent, samples_rec);

        open_output_stream(&data.send_data, SendData::sendCallback);
        open_input_stream(&data.receive_data, ReceiveData::receiveCallback);
        start_output_stream();
        start_input_stream();

        printf("Waiting for transferring to finish.\n");
        if (is_I2A(mode_))
        {
            data.send_data.receive_udp_msg();
        }
        else
        {
            data.receive_data.send_udp_msg();
        }
        if (err < 0) error();
        Pa_Sleep(500);
        stop_input_stream();
        close_input_stream();
        stop_output_stream();
        close_output_stream();
        printf("\n#### Transferring is finished!! ####\n");
        if (write_rec_waves)
        {
            printf("**You choose get waves received, now writing\n");
            size_t n = data.receive_data.write_samples_to_file(file_wave_rec);
            printf("**Writing samples recorded is finished, %zu samples have been write to in total.\n", n);
        }
        if (write_sent_waves)
        {
            printf("**You choose get waves sent, now writing\n");
            size_t n = data.send_data.write_samples_to_file(file_wave_sent);
            printf("**Writing waves sent is finished, %zu samples have been write to in total.\n", n);
        }
    }
}

void Stream::send_and_receive(DataSim &data, bool write_sent_waves, const char* file_wave_sent,
                            bool write_rec_waves, const char* file_wave_rec)
{
    open_output_stream(&data, DataSim::send_callback);
    open_input_stream(&data, DataSim::receive_callback);
    data.backoff_start = std::chrono::system_clock::now();
    start_input_stream();
    start_output_stream();

    unsigned total = data.senddata.totalBits;
    printf("Waiting for receiving and sending to finish.\n");
    while (((!data.send_finished || !data.receive_finished) ||
            data.receivedata.status != ReceiveData::DETECTING || 
        data.senddata.bitIndex != data.senddata.totalBits ||
        data.senddata.wait) && (err = Pa_IsStreamActive(output_stream)) == 1)
    {
        data.demodulate();
        printf("% 3.2f%% Completed\r", (float)data.senddata.bitIndex * 100 / total); fflush(stdout);
    }
    if (data.senddata.wait)
        printf("\nLINK ERROR!!\n");
    else
        Pa_Sleep(2000);
    if (err < 0) error();
    stop_input_stream();
    if (Pa_IsStreamActive(output_stream) == 1)
        stop_output_stream();
    close_input_stream();
    close_output_stream();

    char outputfile[16];
    sprintf(outputfile, "OUTPUT%dto%d.bin", data.dst, NODE);
    printf("\n#### Receiving is finished!! Now write the data to the file %s. ####\n", outputfile);
    size_t n = data.receivedata.write_to_file(outputfile);
    printf("Writing file received is finished, %zu bytes have been write to in total.\n", n);

    if (write_rec_waves)
    {
        printf("**You choose get wave received, now writing\n");
        n = data.receivedata.write_samples_to_file(file_wave_rec);
        printf("**Writing samples recorded is finished, %zu samples have been write to in total.\n", n);
    }
    if (write_sent_waves)
    {
        data.senddata.fileFrameIndex = data.totalFramesSent;
        printf("**You choose get waves sent, now writing\n");
        n = data.senddata.write_samples_to_file(file_wave_sent);
        printf("**Writing waves sent is finished, %zu samples have been write to in total.\n", n);
    }
}