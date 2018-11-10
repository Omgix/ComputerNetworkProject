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
"    3. Exit.\n";

const char InvalidOption[] =
"Invalid option, please enter again!!\n";

char FileName[256];

int Stream::numStream = 0;
uint8_t data[15000];
SAMPLE samples[20 * 44100];

int main()
{
	printf("%s", Wellcome);
	printf("%s", Options);
	while (true)
	{
		int option;
		scanf("%d", &option);
		if (option == 3) return 0;
		else if (option == 1 || option == 2)
		{
			Stream fifoStream(7, 8);
			if (option == 1)
			{
				SendData data("INPUT.bin", data, samples);
				fifoStream.send(data, true);
			}
			else
			{
				ReceiveData data(20, data, samples);
				fifoStream.receive(data, true);
			}
		}
		else
		{
			printf("%s", InvalidOption);
			printf("%s", Options);
		}
	}
}