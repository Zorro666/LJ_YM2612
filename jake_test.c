#include <stdio.h>

#define NUM_ALGOS (2)

int main()
{
	unsigned int dB[NUM_ALGOS];
	unsigned int prevdB[NUM_ALGOS];
	unsigned int newdB[NUM_ALGOS];
	unsigned int deltadB[NUM_ALGOS];
	int stop[NUM_ALGOS];
	int i;

	stop[0] = 0;
	stop[1] = 0;

	dB[0] = 0x3FF;
	dB[1] = 0;
	for (i = 0; i < 2048; i++)
	{
		prevdB[0] = dB[0];
		prevdB[1] = dB[1];

		if (stop[0] == 0)
		{
			deltadB[0] = (prevdB[0] >> 4);
			deltadB[0] += 1;
			newdB[0] = dB[0] - deltadB[0];
		}

		if (stop[1] == 0)
		{
			deltadB[1] = ((((~prevdB[1]) & 0x3FF)) >> 4);
			newdB[1] = dB[1] + deltadB[1];
		}

		printf("i:%d prevdB:%d newdB:%d deltadB:%d prevdB:%d newdB:%d deltadB:%d\n", 
					i, prevdB[0], newdB[0], deltadB[0], prevdB[1], newdB[1], deltadB[1]);

		if (newdB[0] == dB[0])
		{
			stop[0] = 1;
		}
		if (newdB[0] > 1023)
		{
			stop[0] = 1;
		}
		if (newdB[1] == dB[1])
		{
			stop[1] = 1;
		}
		if (newdB[1] > 1023)
		{
			stop[1] = 1;
		}
		if ((stop[0] == 1) && (stop[1] == 1))
		{
			break;
		}

		dB[0] = newdB[0];
		dB[1] = newdB[1];
	}

	return 1;
}
