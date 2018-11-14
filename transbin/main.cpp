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
"Please enter a number to choose an option : \n";
const char Options[] =
"    1. Send a file.\n"
"    2. Receive a file.\n"
"    3. Send with ACK.\n"
"    4. Receive with ACK\n"
"    5. Exit.\n";

const char InvalidOption[] =
"Invalid option, please enter again!!\n";

const char InvalidDeviceNo[] =
"Invalid device number, please enter again: ";

char FileName[256];

int Stream::numStream = 0;
uint8_t data_sent[15000];
uint8_t data_rec[15000];
SAMPLE samples_sent[MAX_TIME_RECORD * SAMPLE_RATE];
SAMPLE samples_rec[MAX_TIME_RECORD * SAMPLE_RATE];

static void select_audiodev(Stream &stream, bool list, bool input, bool output);

int main()
{
	SetConsoleOutputCP(65001);
	printf("%s", Wellcome);
	Stream fifoStream(-1, -1);
	Pa_HostApiTypeIdToHostApiIndex(paWASAPI);
	while (true)
	{
		printf("%s", Options);
		int option;
		scanf("%d", &option);
		if (option == 1)
		{
			select_audiodev(fifoStream, true, false, true);
			SendData data("INPUT.bin", data_sent, samples_sent);
			fifoStream.send(data, true);
		}
		else if (option == 2)
		{
			select_audiodev(fifoStream, true, true, false);
			ReceiveData data(MAX_TIME_RECORD, data_rec, samples_rec);
			fifoStream.receive(data, true);
		}
		else if (option == 3)
		{
			select_audiodev(fifoStream, true, true, true);
			DataCo data(TRANSMITTER, "INPUT.bin", data_sent, data_rec, samples_sent, samples_rec);
			fifoStream.send(data, true, nullptr, false, nullptr);
		}
		else if (option == 4)
		{
			select_audiodev(fifoStream, true, true, true);
			DataCo data(RECEIVER, nullptr, data_sent, data_rec, samples_sent, samples_rec);
			fifoStream.receive(data, false, nullptr, true, nullptr);
		}
		else if (option == 5)
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