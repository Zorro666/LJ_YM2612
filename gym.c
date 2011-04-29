/*
The GYM file format contains only four different instructions, each represented by one byte with 0 to 2 bytes of arguments:
0x00      Do nothing for 1/60th of a second (NOP)
0x01 R D  write data D on YM port 0, register R
0x02 R D  write data D on YM port 1, register R
0x03 D    write on PSG port the data D
*/

#include <malloc.h>

#include "gym.h"

////////////////////////////////////////////////////////////////////
// 
// Internal data & functions
// 
////////////////////////////////////////////////////////////////////

static const char* const GYM_INSTRUCTIONS[4] = { "NOP", "PORT_0", "PORT_1", "PORT_PSG" };

static void gym_clear(GYM_FILE* const gymFile)
{
	gymFile->fh = 0;
	gymFile->pos = 0;
}

static GYM_RESULT gym_open(GYM_FILE* const gymFile, const char* const fname)
{
	FILE* fh = NULL;
	if ( gymFile == NULL )
	{
		fprintf(stderr, "gym_open:gymFile is NULL trying to open file '%s'\n", fname);
		return GYM_ERROR;
	}

	gym_clear( gymFile );

	fh = fopen(fname, "rb");
	if (fh == NULL)
	{
		fprintf(stderr, "gym_open:Failed to open file '%s'\n", fname);
		return GYM_ERROR;
	}
	gymFile->fh = fh;
	gymFile->pos = 0;

	return GYM_OK;
}

////////////////////////////////////////////////////////////////////
// 
// Exposed external data & functions
// 
////////////////////////////////////////////////////////////////////

GYM_FILE* GYM_create(const char* const fname)
{
	int result = 0;
	GYM_FILE* gymFile = malloc(sizeof(GYM_FILE));
	if (gymFile == NULL)
	{
		fprintf(stderr, "GYM_create:malloc failed for file '%s'\n", fname);
		return NULL;
	}

	result = gym_open(gymFile,fname);
	if (result == -1)
	{
		fprintf(stderr, "GYM_create:gym_open failed for file '%s'\n", fname);
		free(gymFile);
		return NULL;
	}

	return gymFile;
}

GYM_RESULT GYM_destroy(GYM_FILE* const gymFile)
{
	if (gymFile == NULL)
	{
		return GYM_OK;
	}
	free(gymFile);
	return GYM_OK;
}

/*
The GYM file format contains only four different instructions, each represented by one byte with 0 to 2 bytes of arguments:
0x00      Do nothing for 1/60th of a second (NOP)
0x01 R D  write data D on YM port 0, register R
0x02 R D  write data D on YM port 1, register R
0x03 D    write on PSG port the data D
*/
GYM_RESULT GYM_read(GYM_FILE* const gymFile, GYM_INSTRUCTION* const gymInstr)
{
	size_t numRead;
	unsigned char cmd = 0;
	const int pos = gymFile->pos;

	numRead = fread(&cmd, sizeof(cmd), 1, gymFile->fh);
	if (numRead != 1)
	{
		fprintf(stderr, "GYM_read: failed to read cmd byte pos:%d\n", pos);
		return GYM_ERROR;
	}
	if (cmd == GYM_NOP)
	{
		gymInstr->pos = pos;
		gymInstr->cmd = cmd;
		gymFile->pos += 1;
		return GYM_OK;
	}
	if ((cmd == GYM_WRITE_PORT_0) || (cmd == GYM_WRITE_PORT_1))
	{
		unsigned char R = 0;
		unsigned char D = 0;

		numRead = fread(&R, sizeof(R), 1, gymFile->fh);
		if (numRead != 1)
		{
			fprintf(stderr, "GYM_read: failed to read R byte cmd:%d pos:%d\n", cmd, pos);
			return GYM_ERROR;
		}

		numRead = fread(&D, sizeof(D), 1, gymFile->fh);
		if (numRead != 1)
		{
			fprintf(stderr, "GYM_read: failed to read D byte cmd:%d pos:%d\n", cmd, pos);
			return GYM_ERROR;
		}

		gymInstr->pos = pos;
		gymInstr->cmd = cmd;
		gymInstr->R = R;
		gymInstr->D = D;
		gymFile->pos += 3;
		return GYM_OK;
	}
	if (cmd == GYM_WRITE_PORT_PSG)
	{
		unsigned char D = 0;

		numRead = fread(&D, sizeof(D), 1, gymFile->fh);
		if (numRead != 1)
		{
			fprintf(stderr, "GYM_read: failed to read D byte cmd:%d pos:%d\n", cmd, pos);
			return GYM_ERROR;
		}

		gymInstr->pos = pos;
		gymInstr->cmd = cmd;
		gymInstr->D = D;
		gymFile->pos += 2;
		return GYM_OK;
	}

	fprintf(stderr, "GYM_read: unknown cmd:%d pos:%d\n", cmd, pos);
	return GYM_ERROR;
}

GYM_RESULT GYM_reset(GYM_FILE* const gymFile)
{
	gymFile->pos = 0;
	int result = fseek(gymFile->fh, 0, SEEK_SET);
	if (result != 0)
	{
		fprintf(stderr, "GYM_reset: failed to seek to start:%d", result);
		return GYM_ERROR;
	}
	return GYM_OK;
}

void GYM_debugPrint(const GYM_INSTRUCTION* const gymInstr)
{
	const int pos = gymInstr->pos;
	const int cmd = gymInstr->cmd;

	printf("GYM:%d cmd:%d %s", pos, cmd, GYM_INSTRUCTIONS[cmd]);
	if ((cmd == GYM_WRITE_PORT_0) || (cmd == GYM_WRITE_PORT_1))
	{
		const int R = gymInstr->R;
		const int D = gymInstr->D;
		printf(" REG:0x%X DATA:0x%X", R, D);
	}
	if (cmd == GYM_WRITE_PORT_PSG)
	{
		const int D = gymInstr->D;
		printf(" DATA:0x%X", D);
	}
	printf("\n");
}
