#include "pastream.h"
#include <cstdio>
#include <arpa/inet.h>

const char Wellcome[] =
"===================================================================\n"
"                                                                   \n"
"                 Acoustic File Transmission System                 \n"
"                           version 0.8                             \n"
"                                                                   \n"
"===================================================================\n"
"Please enter a number to choose an option :";

const char Options[] =
"\n"
"1.Send a file.\n"
"2.Receive a file.\n"
"3.Send with ACK.\n"
"4.Receive with ACK\n"
"5.Send and receive simultaneously.\n"
"6.Athernet to Internet Gateway.\n"
"7.Internet to Athernet Gateway.\n"
"8.FTP Client.\n"
"9.Exit.\n";

const char *FTP_Commands[] =
{
"USER", "PASS", "PWD", "CWD", "PASV", "LIST", "RETR"
};

int select_FTP_commands(char *ask);

const char InvalidOption[] =
"Invalid option, please enter again!!\n";

const char InvalidDeviceNo[] =
"Invalid device number, please enter again: ";

int Stream::numStream = 0;
uint8_t data_sent[1024*1024];
uint8_t data_rec[1024*1024];
SAMPLE samples_sent[MAX_TIME_RECORD * SAMPLE_RATE];
SAMPLE samples_rec[MAX_TIME_RECORD * SAMPLE_RATE];

static void select_audiodev(Stream &stream, bool list, bool input, bool output);

int NODE = 0;
int BITS_CONTENT;
int BYTES_CONTENT;
int BITS_NORMALPACKET;

int main()
{
    printf("%s", Wellcome);
    printf("%s", Options);

    Stream stream(-1, -1);

    while (true)
    {
        BITS_CONTENT = 1200;
        BYTES_CONTENT = BITS_CONTENT / 8;
        BITS_NORMALPACKET = BITS_CRC + BITS_CONTENT + BITS_INFO;
        int option;
        scanf("%d", &option);
        if (option == 1)
        {
            BITS_CONTENT = 800;
            BYTES_CONTENT = BITS_CONTENT / 8;
            BITS_NORMALPACKET = BITS_CRC + BITS_CONTENT + BITS_INFO;

            int mode;
            printf("Please choose the mode: 1.text. 2.binary ");
            scanf("%d",&mode);
            if (mode == 1)
            {
                select_audiodev(stream, true, false, true);
                SendData data("INPUT.txt", true, data_sent, samples_sent);
                stream.send(data, true);
            }
            else if (mode == 2)
            {
                select_audiodev(stream, true, false, true);
                SendData data("INPUT.bin", false, data_sent, samples_sent);
                stream.send(data, true);
            }
            printf("%s", Options);
        }
        else if (option == 2)
        {
            BITS_CONTENT = 800;
            BYTES_CONTENT = BITS_CONTENT / 8;
            BITS_NORMALPACKET = BITS_CRC + BITS_CONTENT + BITS_INFO;
            int mode;
            printf("Please choose the mode: 1.text. 2.binary ");
            scanf("%d", &mode);
            select_audiodev(stream, true, true, false);
            ReceiveData data(MAX_TIME_RECORD, data_rec, samples_rec);
            stream.receive(data, mode == 1, true);
            printf("%s", Options);
        }
        else if (option == 3)
        {
            int mode;
            printf("Please choose the mode: 1.text. 2.binary ");
            scanf("%d", &mode);
            select_audiodev(stream, true, true, true);
            if (mode == 1)
            {
                DataCo data(TRANSMITTER, "INPUT.txt", true, data_sent, data_rec, samples_sent, samples_rec,
                            2, inet_addr("127.0.0.1"), 8888);
                stream.send(data, true, true, "wavesent1.wav", true, "wavereceived1.wav");
            }
            else if (mode == 2)
            {
                DataCo data(TRANSMITTER, "INPUT.bin", false, data_sent, data_rec, samples_sent, samples_rec,
                            2, inet_addr("127.0.0.1"), 8888);
                stream.send(data, true, true, "wavesent1.wav", true, "wavereceived1.wav");
            }
            printf("%s", Options);
        }
        else if (option == 4)
        {
            int mode;
            printf("Please choose the mode: 1.text. 2.binary ");
            scanf("%d", &mode);
            select_audiodev(stream, true, true, true);
            DataCo data(RECEIVER, nullptr, false, data_sent, data_rec, samples_sent, samples_rec,
                    2, inet_addr("127.0.0.1"), 8888);
            stream.receive(data, mode == 1, true, "wavesent2.wav", true, "wavereceived2.wav");
            printf("%s", Options);
        }
        else if (option == 5)
        {
            int dst;
            char inputFile[32];
            char wavesent[32];
            char wavereceived[32];

            BITS_CONTENT = 480;
            BYTES_CONTENT = BITS_CONTENT / 8;
            BITS_NORMALPACKET = BITS_CRC + BITS_CONTENT + BITS_INFO;
            printf("Please input host node number: ");
            scanf("%d", &NODE);
            printf("Please input destination node number: ");
            scanf("%d", &dst);
            sprintf(wavesent, "wavesent%d.wav", NODE);
            sprintf(wavereceived, "wavereceived%d.wav", NODE);
            sprintf(inputFile, "INPUT%dto%d.bin", NODE, dst);

            select_audiodev(stream, true, true, true);
            DataSim data(dst, inputFile, data_sent, data_rec, samples_sent, samples_rec);
            stream.send_and_receive(data, true, wavesent, true, wavereceived);
            printf("%s", Options);
        }
        else if (option == 6)
        {
            //char wavesent[32];
            //char wavereceived[32];
            printf("Please input host node number: ");
            scanf("%d", &NODE);
            //sprintf(wavesent, "wavesent_transfer%d.wav", NODE);
            //sprintf(wavereceived, "wavereceived_transfer%d.wav", NODE);

            select_audiodev(stream, true, true, true);
            char mode [5];
            printf("FTP mode?(y/n)"); scanf("%s", mode);
            if (mode[0] == 'y' || mode[0] == 'Y')
                stream.transfer(Stream::FTP);
            else
                stream.transfer(Stream::A_TO_I);
            printf("%s", Options);
        }
        else if (option == 7)
        {
            //char wavesent[32];
            //char wavereceived[32];
            printf("Please input host node number: ");
            scanf("%d", &NODE);
            //sprintf(wavesent, "wavesent_transfer%d.wav", NODE);
            //sprintf(wavereceived, "wavereceived_transfer%d.wav", NODE);

            select_audiodev(stream, true, true, true);
            stream.transfer(Stream::I_TO_A);
            printf("%s", Options);
        }
        else if (option == 8)
        {
            int dst;
            char wavesent[32];
            char wavereceived[32];

            BITS_CONTENT = 480;
            BYTES_CONTENT = BITS_CONTENT / 8;
            BITS_NORMALPACKET = BITS_CRC + BITS_CONTENT + BITS_INFO;
            printf("Please input host node number: ");
            scanf("%d", &NODE);
            printf("Please input destination node number: ");
            scanf("%d", &dst);
            sprintf(wavesent, "wavesent%d.wav", NODE);
            sprintf(wavereceived, "wavereceived%d.wav", NODE);

            select_audiodev(stream, true, true, true);
            size_t input_maxlen = 512;
            char input_buf[input_maxlen];
            printf("Please input the IP address of the FTP server, press enter to finish input:\n");
            scanf("%s", input_buf);

            char send_buf[512] = {};

            DataCo data (send_buf, 1, TYPEID_ASK_NORMAL, (Mode)(TRANSMITTER|FTP_CLIENT), dst, inet_addr(input_buf), 21,
                    data_sent, data_rec, samples_sent, samples_rec);
            stream.send(data);

            int op;
            while ((op = select_FTP_commands(send_buf)) != -1)
            {
                size_t len = strnlen(send_buf, 512);
                int typeID = op == 6? TYPEID_ASK_SPEC: TYPEID_ASK_NORMAL;
                DataCo data2(send_buf, len + 1, typeID, (Mode)(TRANSMITTER|FTP_CLIENT), dst, inet_addr(input_buf),
                        21, data_sent, data_rec, samples_sent, samples_rec);
                stream.send(data2);
                if (op == 6)
                {
                    DataCo data3(RECEIVER, nullptr, TYPEID_NONE, data_sent, data_rec, samples_sent, samples_rec,
                                dst, inet_addr(input_buf), 21);
                    stream.receive(data3);
                }
            }
            sprintf(send_buf, "CLOSE");
            DataCo data_close (send_buf, 6, TYPEID_CONTENT_LAST, TRANSMITTER, dst, inet_addr(input_buf), 21,
                         data_sent, data_rec, samples_sent, samples_rec);
            stream.send(data_close);
        }
        else if (option == 9)
        {
            return 0;
        }
        else
        {
            printf("%s", InvalidOption);
        }
    }
}

static void select_audiodev(Stream &stream, bool list, bool input, bool output)
{
    int n;
    bool success = false;

    if (list)
    {
        printf("List of audio devices: \n");
        stream.list_all();
        printf("\n");
    }
    if (input)
    {
        printf("Please enter an input device: ");
        while (!success)
        {
            scanf("%d", &n);
            success = stream.select_input_device(n);
            if (!success)
                printf("%s", InvalidDeviceNo);
        }
    }
    success = false;
    if (output)
    {
        printf("Please enter an output device: ");
        while (!success)
        {
            scanf("%d", &n);
            success = stream.select_output_device(n);
            if (!success)
                printf("%s", InvalidDeviceNo);
        }
    }
}

int select_FTP_commands(char *ask)
{
    printf("Please select a command, or press q to quit:\n");
    for (int i = 0; i < 7; ++i)
        printf("%d.%s\n", i + 1, FTP_Commands[i]);
    char op [5];
    while (true)
    {
        scanf("%s", op);
        if (op[0] - '1' >= 0 && op[0] - '1' < 7)
        {
            if (op[0] - '1' != 5 && op[0] - '1' != 4)
            {
                printf("Please complete the command:\n");
                printf("%s ", FTP_Commands[op[0] - '1']);
                sprintf(ask, "%s ", FTP_Commands[op[0] - '1']);
                scanf("%s\r\n", ask + strlen(FTP_Commands[op[0] - '1']) + 1);
            }
            else
                sprintf(ask, "%s \r\n", FTP_Commands[op[0] - '1']);
            return op[0] - '1';
        }
        else if (op[0] == 'q')
            return -1;
        else
            printf("Invalid command options, please select again!!\n");
    }
}