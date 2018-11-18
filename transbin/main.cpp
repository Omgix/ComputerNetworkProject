#include "pastream.h"
#include <cstdio>
#include <windows.h>

const char Wellcome[] =
"===================================================================\n"
"                                                                   \n"
"                 Acoustic File Transmission System                 \n"
"                           version 0.2                             \n"
"                                                                   \n"
"===================================================================\n"
"Please enter a number to choose an option :";

const char Options[] =
"\n"
"    1.Send a file.\n"
"    2.Receive a file.\n"
"    3.Send with ACK.\n"
"    4.Receive with ACK\n"
"    5.Send and receive simultaneously.\n"
"    6.Exit.\n";

const char InvalidOption[] =
"Invalid option, please enter again!!\n";

const char InvalidDeviceNo[] =
"Invalid device number, please enter again: ";

int Stream::numStream = 0;
uint8_t data_sent[10000];
uint8_t data_rec[10000];
SAMPLE samples_sent[MAX_TIME_RECORD * SAMPLE_RATE];
SAMPLE samples_rec[MAX_TIME_RECORD * SAMPLE_RATE];

static void select_audiodev(Stream &stream, bool list, bool input, bool output);

int NODE = 0;
int BITS_CONTENT;
int BYTES_CONTENT;
int BITS_NORMALPACKET;

int main()
{
	SetConsoleOutputCP(65001);
	printf("%s", Wellcome);
	printf("%s", Options);

	Stream stream(-1, -1);
	Pa_HostApiTypeIdToHostApiIndex(paWASAPI);

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
			select_audiodev(stream, true, false, true);
			SendData data("INPUT.bin", data_sent, samples_sent);
			stream.send(data, true);
			printf("%s", Options);
		}
		else if (option == 2)
		{
			BITS_CONTENT = 800;
			BYTES_CONTENT = BITS_CONTENT / 8;
			BITS_NORMALPACKET = BITS_CRC + BITS_CONTENT + BITS_INFO;
			select_audiodev(stream, true, true, false);
			ReceiveData data(MAX_TIME_RECORD, data_rec, samples_rec);
			stream.receive(data, true);
			printf("%s", Options);
		}
		else if (option == 3)
		{
			select_audiodev(stream, true, true, true);
			DataCo data(TRANSMITTER, "INPUT.bin", data_sent, data_rec, samples_sent, samples_rec);
			stream.send(data, true, "wavesent1.wav", true, "wavereceived1.wav");
			printf("%s", Options);
		}
		else if (option == 4)
		{
			select_audiodev(stream, true, true, true);
			DataCo data(RECEIVER, nullptr, data_sent, data_rec, samples_sent, samples_rec);
			stream.receive(data, true, "wavesent2.wav", true, "wavereceived2.wav");
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