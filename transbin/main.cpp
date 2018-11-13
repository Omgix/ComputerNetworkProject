#include "pastream.h"
#include <cstdio>

const char Wellcome[] =
"┍══════════════════════════════════"
  "══════════════════════════════════┒\n"
"║                                                                    ║\n"
"║                Acoustic File Transmission System                   ║\n"
"║                          version 0.2                               ║\n"
"║                                                                    ║\n"
"┗══════════════════════════════════"
  "══════════════════════════════════┚\n";
const char Options[] =
"Please enter a number to choose an option:\n"
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
	printf("%s", Wellcome);
	printf("%s", Options);
	while (true)
	{
		int option;
		scanf("%d", &option);
		Stream fifoStream(7, 8);
		switch (option)
		{
		case 1:
		{
			select_audiodev(fifoStream, true, true, false);
			SendData data("INPUT.bin", data_sent, samples_sent);
			fifoStream.send(data, true);
			break;
		}
		case 2:
		{
			select_audiodev(fifoStream, true, false, true);
			ReceiveData data(20, data_rec, samples_rec);
			fifoStream.receive(data, true);
			break;
		}
		case 3:
		{
			select_audiodev(fifoStream, true, true, true);
			DataCo data(TRANSMITTER, "INPUT.bin", data_sent, data_rec, samples_sent, samples_rec);
			fifoStream.send(data, true, nullptr, true, nullptr);
			break;
		}
		case 4:
		{
			select_audiodev(fifoStream, true, true, true);
			DataCo data(TRANSMITTER, nullptr, data_sent, data_rec, samples_sent, samples_rec);
			fifoStream.receive(data, true, nullptr, true, nullptr);
			break;
		}
		case 5:
		{
			return 0;
		}
		default:
			printf("%s", InvalidOption);
			printf("%s", Options);
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