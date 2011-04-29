/*
The GYM file format contains only four different instructions, each represented by one byte with 0 to 2 bytes of arguments:
0x00      Do nothing for 1/60th of a second (NOP)
0x01 R D  write data D on YM port 0, register R
0x02 R D  write data D on YM port 1, register R
0x03 D    write on PSG port the data D
*/

#include <malloc.h>

#include "lj_gym.h"

////////////////////////////////////////////////////////////////////
// 
// Internal data & functions
// 
////////////////////////////////////////////////////////////////////

struct LJ_GYM_FILE
{
	FILE* fh;
	int pos;
};

static const char* const LJ_GYM_INSTRUCTIONS[4] = { "NOP", "PORT_0", "PORT_1", "PORT_PSG" };

static void gym_clear(LJ_GYM_FILE* const gymFile)
{
	gymFile->fh = 0;
	gymFile->pos = 0;
}

static LJ_GYM_RESULT gym_open(LJ_GYM_FILE* const gymFile, const char* const fname)
{
	FILE* fh = NULL;
	if ( gymFile == NULL )
	{
		fprintf(stderr, "gym_open:gymFile is NULL trying to open file '%s'\n", fname);
		return LJ_GYM_ERROR;
	}

	fh = fopen(fname, "rb");
	if (fh == NULL)
	{
		fprintf(stderr, "gym_open:Failed to open file '%s'\n", fname);
		return LJ_GYM_ERROR;
	}
	gymFile->fh = fh;
	gymFile->pos = 0;

	return LJ_GYM_OK;
}

////////////////////////////////////////////////////////////////////
// 
// Exposed external data & functions
// 
////////////////////////////////////////////////////////////////////

LJ_GYM_FILE* LJ_GYM_create(const char* const fname)
{
	int result = LJ_GYM_ERROR;
	LJ_GYM_FILE* gymFile = malloc(sizeof(LJ_GYM_FILE));
	if (gymFile == NULL)
	{
		fprintf(stderr, "LJ_GYM_create:malloc failed for file '%s'\n", fname);
		return NULL;
	}

	gym_clear( gymFile );

	result = gym_open(gymFile,fname);
	if (result == LJ_GYM_ERROR)
	{
		fprintf(stderr, "LJ_GYM_create:gym_open failed for file '%s'\n", fname);
		free(gymFile);
		return NULL;
	}

	return gymFile;
}

LJ_GYM_RESULT LJ_GYM_destroy(LJ_GYM_FILE* const gymFile)
{
	if (gymFile == NULL)
	{
		return LJ_GYM_OK;
	}
	free(gymFile);
	return LJ_GYM_OK;
}

/*
The GYM file format contains only four different instructions, each represented by one byte with 0 to 2 bytes of arguments:
0x00      Do nothing for 1/60th of a second (NOP)
0x01 R D  write data D on YM port 0, register R
0x02 R D  write data D on YM port 1, register R
0x03 D    write on PSG port the data D
*/
LJ_GYM_RESULT LJ_GYM_read(LJ_GYM_FILE* const gymFile, LJ_GYM_INSTRUCTION* const gymInstr)
{
	size_t numRead = 0;
	unsigned char cmd = 0;
	int pos = 0;

	if (gymFile == NULL)
	{
		fprintf(stderr, "LJ_GYM_read:gymFile is NULL\n" );
		return LJ_GYM_ERROR;
	}

	pos = gymFile->pos;
	numRead = fread(&cmd, sizeof(cmd), 1, gymFile->fh);
	if (numRead != 1)
	{
		fprintf(stderr, "LJ_GYM_read: failed to read cmd byte pos:%d\n", pos);
		return LJ_GYM_ERROR;
	}
	if (cmd == LJ_GYM_NOP)
	{
		gymInstr->pos = pos;
		gymInstr->cmd = cmd;
		gymFile->pos += 1;
		return LJ_GYM_OK;
	}
	if ((cmd == LJ_GYM_WRITE_PORT_0) || (cmd == LJ_GYM_WRITE_PORT_1))
	{
		unsigned char R = 0;
		unsigned char D = 0;

		numRead = fread(&R, sizeof(R), 1, gymFile->fh);
		if (numRead != 1)
		{
			fprintf(stderr, "LJ_GYM_read: failed to read R byte cmd:%d pos:%d\n", cmd, pos);
			return LJ_GYM_ERROR;
		}

		numRead = fread(&D, sizeof(D), 1, gymFile->fh);
		if (numRead != 1)
		{
			fprintf(stderr, "LJ_GYM_read: failed to read D byte cmd:%d pos:%d\n", cmd, pos);
			return LJ_GYM_ERROR;
		}

		gymInstr->pos = pos;
		gymInstr->cmd = cmd;
		gymInstr->R = R;
		gymInstr->D = D;
		gymFile->pos += 3;
		return LJ_GYM_OK;
	}
	if (cmd == LJ_GYM_WRITE_PORT_PSG)
	{
		unsigned char D = 0;

		numRead = fread(&D, sizeof(D), 1, gymFile->fh);
		if (numRead != 1)
		{
			fprintf(stderr, "LJ_GYM_read: failed to read D byte cmd:%d pos:%d\n", cmd, pos);
			return LJ_GYM_ERROR;
		}

		gymInstr->pos = pos;
		gymInstr->cmd = cmd;
		gymInstr->D = D;
		gymFile->pos += 2;
		return LJ_GYM_OK;
	}

	fprintf(stderr, "LJ_GYM_read: unknown cmd:%d pos:%d\n", cmd, pos);
	return LJ_GYM_ERROR;
}

LJ_GYM_RESULT LJ_GYM_reset(LJ_GYM_FILE* const gymFile)
{
	int result = LJ_GYM_ERROR;

	if (gymFile == NULL)
	{
		fprintf(stderr, "LJ_GYM_reset:gymFile is NULL\n" );
		return LJ_GYM_ERROR;
	}

	gymFile->pos = 0;

	result = fseek(gymFile->fh, 0, SEEK_SET);
	if (result != 0)
	{
		fprintf(stderr, "LJ_GYM_reset: failed to seek to start:%d", result);
		return LJ_GYM_ERROR;
	}
	return LJ_GYM_OK;
}

void LJ_GYM_debugPrint(const LJ_GYM_INSTRUCTION* const gymInstr)
{
	int pos;
	int cmd;

	if (gymInstr == NULL)
	{
		fprintf(stderr, "LJ_GYM_debugPrint:gymInstr is NULL\n" );
		return;
	}

	pos = gymInstr->pos;
	cmd = gymInstr->cmd;

	printf("GYM:%d cmd:%d %s", pos, cmd, LJ_GYM_INSTRUCTIONS[cmd]);
	if ((cmd == LJ_GYM_WRITE_PORT_0) || (cmd == LJ_GYM_WRITE_PORT_1))
	{
		const int R = gymInstr->R;
		const int D = gymInstr->D;
		printf(" REG:0x%X DATA:0x%X", R, D);
	}
	if (cmd == LJ_GYM_WRITE_PORT_PSG)
	{
		const int D = gymInstr->D;
		printf(" DATA:0x%X", D);
	}
	printf("\n");
}
