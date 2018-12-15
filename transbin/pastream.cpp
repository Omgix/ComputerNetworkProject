#include "pastream.h"
#include <cstdio>

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

void Stream::send(DataCo &data, bool write_sent_waves, const char* file_wave_sent, 
                bool write_rec_waves, const char* file_wave_rec)
{
    printf("Waiting for sending to finish.\n");
    unsigned total = data.send_data.totalBits;
    open_input_stream(&data.receive_data, ReceiveData::receiveCallback);
    open_output_stream(&data.send_data, SendData::sendCallback);
    start_input_stream();
    start_output_stream();
    while ((err = Pa_IsStreamActive(output_stream)) == 1)
    {
        printf("% 3.2f%% Completed\r", (float)data.send_data.bitIndex * 100 / total); fflush(stdout);
    }
    if (err < 0) error();
    stop_input_stream();
    stop_output_stream();
    close_input_stream();
    close_output_stream();
    if (data.send_data.wait)
        printf("\nLINK ERROR!!\n");
    else
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

void Stream::receive(ReceiveData &data, bool writewaves, const char* file_name)
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
    size_t n = data.write_to_file("OUTPUT.bin");
    printf("**Writing is finished, %zu bytes have been write to in total.\n", n);
    if (writewaves)
    {
        n = data.write_samples_to_file(file_name);
        printf("**Writing samples recorded is finished, %zu samples have been write to in total.\n", n);
    }
}

void Stream::receive(DataCo &data, bool write_sent_waves, const char* file_wave_sent,
    bool write_rec_waves, const char* file_wave_rec)
{
    open_output_stream(&data.send_data, SendData::sendCallback);
    open_input_stream(&data.receive_data, ReceiveData::receiveCallback);
    start_output_stream();
    start_input_stream();

    printf("Waiting for receiving to finish.\n");

    while ((err = Pa_IsStreamActive(input_stream)) == 1)
    {
        printf("%d bytes received\r", data.receive_data.totalBytes); fflush(stdout);
        Pa_Sleep(100);
    }
    if (err < 0) error();
    Pa_Sleep(500);
    stop_input_stream();
    close_input_stream();
    stop_output_stream();
    close_output_stream();
    printf("\n#### Receiving is finished!! Now write the data to the file OUTPUT.bin. ####\n");
    printf("Threshold: %f\n", data.receive_data.threshold);
    size_t n = data.receive_data.write_to_file("OUTPUT.bin");
    printf("Writing file received is finished, %zu bytes have been write to in total.\n", n);
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

void Stream::transfer(TransferMode mode_, bool write_sent_waves, const char *file_wave_sent,
                      bool write_rec_waves, const char *file_wave_rec)
{
    Mode direct;
    if (mode_ == I_TO_A)
        direct = TRANSMITTER;
    else if (mode_ == A_TO_I)
        direct = RECEIVER;
    else
        direct = TRANSMITTER;

    DataCo data ((Mode)(direct | GATEWAY), nullptr, false, data_sent, data_rec, samples_sent, samples_rec);

    open_output_stream(&data.send_data, SendData::sendCallback);
    open_input_stream(&data.receive_data, ReceiveData::receiveCallback);
    start_output_stream();
    start_input_stream();

    printf("Waiting for transferring to finish.\n");

    while ((err = Pa_IsStreamActive(input_stream)) == 1)
    {
        if (mode_ == I_TO_A)
            data.send_data.receive_udp_msg(40);
        else if (mode_ == A_TO_I)
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
    /*sprintf(outputfile, "square%d.wav", NODE);
    int format = SF_FORMAT_WAV | SF_FORMAT_FLOAT;
    int channels = NUM_CHANNELS;
    int srate = SAMPLE_RATE;
    SndfileHandle file = SndfileHandle(outputfile, SFM_WRITE, format, channels, srate);
    file.writeSync();
    file.write(square, data.receivedata.frameIndex);*/
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