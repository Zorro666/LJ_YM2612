/*
The format starts with a 64 byte header:
      00  01  02  03   04  05  06  07   08  09  0a  0b   0c  0d  0e  0f
0x00 ["Vgm " ident   ][EoF offset     ][Version        ][SN76489 clock   ]
0x10 [YM2413 clock   ][GD3 offset     ][Total # samples][Loop offset     ]
0x20 [Loop # samples ][Rate           ][SN FB ][SNW]*** [YM2612 clock    ]
0x30 [YM2151 clock   ][VGM data offset] *** *** *** ***  *** *** *** ***

- Unused space (marked with *) is reserved for future expansion, and should be zero.
- All integer values are *unsigned* and written in "Intel" byte order, 
  so for example 0x12345678 is written as 0x78 0x56 0x34 0x12.
- All pointer offsets are written as relative to the current position in the file, 
  so for example the GD3 offset at 0x14 in the header is the file position of the GD3 tag minus 0x14.

0x00: "Vgm " (0x56 0x67 0x6d 0x20) file identification (32 bits)
0x04: Eof offset (32 bits)
        Relative offset to end of file (i.e. file length - 4).
        This is mainly used to find the next track when concatanating player stubs and multiple files.
0x08: Version number (32 bits)
        Version 1.50 is stored as 0x00000150, stored as 0x50 0x01 0x00 0x00.
        This is used for backwards compatibility in players, and defines which header values are valid.
0x0c: SN76489 clock (32 bits)
        Input clock rate in Hz for the SN76489 PSG chip. A typical value is 3579545. It should be 0 if there is no PSG chip used.
0x10: YM2413 clock (32 bits)
        Input clock rate in Hz for the YM2413 chip. A typical value is 3579545. It should be 0 if there us no YM2413 chip used.
0x14: GD3 offset (32 bits)
        Relative offset to GD3 tag. 0 if no GD3 tag.
        GD3 tags are descriptive tags similar in use to ID3 tags in MP3 files.
        See the GD3 specification for more details. The GD3 tag is usually stored immediately after the VGM data.
0x18: Total # samples (32 bits)
        Total of all wait values in the file.
0x1c: Loop offset (32 bits)
        Relative offset to loop point, or 0 if no loop.
        For example, if the data for the one-off intro to a song was in bytes 0x0040-0x3fff of the file, 
		but the main looping section started at 0x4000, this would contain the value 0x4000-0x1c = 0x00003fe4.
0x20: Loop # samples (32 bits)
        Number of samples in one loop, or 0 if there is no loop.
        Total of all wait values between the loop point and the end of the file.
[VGM 1.01 additions:]
0x24: Rate (32 bits)
        "Rate" of recording in Hz, used for rate scaling on playback. 
		It is typically 50 for PAL systems and 60 for NTSC systems. 
		It should be set to zero if rate scaling is not appropriate - 
		for example, if the game adjusts its music engine for the system's speed.  
		VGM 1.00 files will have a value of 0.
[VGM 1.10 additions:]
0x28: SN76489 feedback (16 bits)
        The white noise feedback pattern for the SN76489 PSG. Known values are:
        0x0009  Sega Master System 2/Game Gear/Mega Drive (SN76489/SN76496 integrated into Sega VDP chip)
        0x0003  Sega Computer 3000H, BBC Micro (SN76489AN)
        For version 1.01 and earlier files, the feedback pattern should be assumed to be 0x0009. 
		If the PSG is not used then this may be omitted (left at zero).
0x2a: SN76489 shift register width (8 bits)
        The noise feedback shift register width, in bits. Known values are:
        16  Sega Master System 2/Game Gear/Mega Drive (SN76489/SN76496 integrated into Sega VDP chip)
        15  Sega Computer 3000H, BBC Micro (SN76489AN)
        For version 1.01 and earlier files, the shift register width should be assumed to be 16. 
		If the PSG is not used then this may be omitted (left at zero).
0x2b: Reserved (8 bits)
        This should be left at zero.
0x2c: YM2612 clock (32 bits)
        Input clock rate in Hz for the YM2612 chip. A typical value is 7670453. It should be 0 if there us no YM2612 chip used.
        For version 1.01 and earlier files, the YM2413 clock rate should be used for the clock rate of the YM2612.
0x30: YM2151 clock (32 bits)
        Input clock rate in Hz for the YM2151 chip. A typical value is 3579545. It should be 0 if there us no YM2151 chip used.
        For version 1.01 and earlier files, the YM2413 clock rate should be used for the clock rate of the YM2151.
[VGM 1.50 additions:]
0x34: VGM data offset (32 bits)
        Relative offset to VGM data stream.
        If the VGM data starts at absolute offset 0x40, this will contain value 0x0000000c. 
		For versions prior to 1.50, it should be 0 and the VGM data must start at offset 0x40.

0x38-0x3f: Reserved (must be zero)

VGM commands

0x4f dd    		: Game Gear PSG stereo, write dd to port 0x06
0x50 dd    		: PSG (SN76489/SN76496) write value dd
0x51 aa dd 		: YM2413, write value dd to register aa
0x52 aa dd 		: YM2612 part 0, write value dd to register aa
0x53 aa dd 		: YM2612 part 1, write value dd to register aa
0x54 aa dd 		: YM2151, write value dd to register aa
0x61 nn nn 		: Wait n samples, n can range from 0 to 65535 (approx 1.49 seconds). 
				  Longer pauses than this are represented by multiple wait commands.
0x62       		: wait 735 samples (60th of a second), a shortcut for 0x61 0xdf 0x02
0x63       		: wait 882 samples (50th of a second), a shortcut for 0x61 0x72 0x03
0x66       		: end of sound data
0x67 ...   		: data block: see below
0x7n       		: wait n+1 samples, n can range from 0 to 15.
0x8n       		: YM2612 part 0 address 2A write from the data bank, then wait n samples; n can range from 0 to 15. 
				  Note that the wait is n, NOT n+1.
0xe0 dddddddd 	: seek to offset dddddddd (Intel byte order) in PCM data bank
 
* Some ranges are reserved for future use, with different numbers of operands:

  0x30..0x4e dd          : one operand, reserved for future use
  0x55..0x5f dd dd       : two operands, reserved for future use
  0xa0..0xbf dd dd       : two operands, reserved for future use
  0xc0..0xdf dd dd dd    : three operands, reserved for future use
  0xe1..0xff dd dd dd dd : four operands, reserved for future use

On encountering these, the correct number of bytes should be skipped.

Data blocks:

VGM command 0x67 specifies a data block. 
These are used to store large amounts of data, which can be used in parallel with the normal VGM data stream. 
The data block format is:

  0x67 0x66 tt ss ss ss ss (data)

where:
  0x67 = VGM command
  0x66 = compatibility command to make older players stop parsing the stream
  tt   = data type
  ss ss ss ss (32 bits) = size of data, in bytes
  (data) = data, of size previously specified

Data blocks, if present, should be at the very start of the VGM data.  However, for future compatibility, 
players should be able to parse data blocks anywhere in the stream, simply by skipping past them if their type is unknown.

The data block type specifies what type of data it contains. Currently defined types are:

00 = YM2612 PCM data for use with associated commands

All unknown types must be skipped by the player.

*/

#include "lj_vgm.h"

#include <malloc.h>
#include <string.h>


/*
* ////////////////////////////////////////////////////////////////////
* // 
* // Internal data & functions
* // 
* ////////////////////////////////////////////////////////////////////
*/

#define LJ_VGM_NUM_INSTRUCTIONS (0xFF)

#define LJ_VGM_DATA_YM2612 (0)

/*
The format starts with a 64 byte header:
      00  01  02  03   04  05  06  07   08  09  0a  0b   0c  0d  0e  0f
0x00 ["Vgm " ident   ][EoF offset     ][Version        ][SN76489 clock   ]
0x10 [YM2413 clock   ][GD3 offset     ][Total # samples][Loop offset     ]
0x20 [Loop # samples ][Rate           ][SN FB ][SNW]*** [YM2612 clock    ]
0x30 [YM2151 clock   ][VGM data offset] *** *** *** ***  *** *** *** ***
*/

struct LJ_VGM_FILE
{
	FILE* fh;
	int pos;
	int cmdCount;
	int dataStart;
	LJ_VGM_HEADER header;

	LJ_VGM_UINT8* data;
	LJ_VGM_UINT8 dataType;
	LJ_VGM_UINT32 dataNum;
	LJ_VGM_UINT32 dataSeekOffset;
	LJ_VGM_UINT32 dataCurrentOffset;

	LJ_VGM_UINT32 waitSamples;
};

static const char* LJ_VGM_instruction[LJ_VGM_NUM_INSTRUCTIONS];
int LJ_VGM_validInstruction[LJ_VGM_NUM_INSTRUCTIONS];

/*
0x4f dd    		: Game Gear PSG stereo, write dd to port 0x06
0x50 dd    		: PSG (SN76489/SN76496) write value dd
0x51 aa dd 		: YM2413, write value dd to register aa
0x52 aa dd 		: YM2612 port 0, write value dd to register aa
0x53 aa dd 		: YM2612 port 1, write value dd to register aa
0x54 aa dd 		: YM2151, write value dd to register aa
0x61 nn nn 		: Wait n samples, n can range from 0 to 65535 (approx 1.49 seconds). 
				  Longer pauses than this are represented by multiple wait commands.
0x62       		: wait 735 samples (60th of a second), a shortcut for 0x61 0xdf 0x02
0x63       		: wait 882 samples (50th of a second), a shortcut for 0x61 0x72 0x03
0x66       		: end of sound data
0x67 ...   		: data block: see below
0x7n       		: wait n+1 samples, n can range from 0 to 15.
0x8n       		: YM2612 port 0 address 2A write from the data bank, then wait n samples; n can range from 0 to 15. 
				  Note that the wait is n, NOT n+1.
0xe0 dddddddd 	: seek to offset dddddddd (Intel byte order) in PCM data bank

0x30..0x4e dd          : one operand, reserved for future use
0x55..0x5f dd dd       : two operands, reserved for future use
0xa0..0xbf dd dd       : two operands, reserved for future use
0xc0..0xdf dd dd dd    : three operands, reserved for future use
0xe1..0xff dd dd dd dd : four operands, reserved for future use
*/

static void vgm_clear(LJ_VGM_FILE* const vgmFile)
{
	memset(vgmFile, 0x0, sizeof(LJ_VGM_FILE));
}

static void vgm_init(LJ_VGM_FILE* const vgmFile)
{
	int i;

	for (i = 0; i < LJ_VGM_NUM_INSTRUCTIONS; i++)
	{
		LJ_VGM_validInstruction[i] = 0;
		LJ_VGM_instruction[i]= "UNKNOWN";
	}
	
	/*LJ_VGM_DATA_BLOCK_START = 0x67,*/
	LJ_VGM_validInstruction[LJ_VGM_DATA_BLOCK_START] = 1;
	LJ_VGM_instruction[LJ_VGM_DATA_BLOCK_START]= "DATA_BLOCK_START";

	/*LJ_VGM_YM2612_WRITE_PART_0 = 0x52,*/
	LJ_VGM_validInstruction[LJ_VGM_YM2612_WRITE_PART_0] = 1;
	LJ_VGM_instruction[LJ_VGM_YM2612_WRITE_PART_0]= "YM2612_WRITE_PART_0";

	/*LJ_VGM_YM2612_WRITE_PART_1 = 0x53,*/
	LJ_VGM_validInstruction[LJ_VGM_YM2612_WRITE_PART_1] = 1;
	LJ_VGM_instruction[LJ_VGM_YM2612_WRITE_PART_1]= "YM2612_WRITE_PART_1";

	/*LJ_VGM_DATA_SEEK_OFFSET = 0xe0,*/
	LJ_VGM_validInstruction[LJ_VGM_DATA_SEEK_OFFSET] = 1;
	LJ_VGM_instruction[LJ_VGM_DATA_SEEK_OFFSET]= "DATA_SEEK_OFFSET";

	/*LJ_VGM_YM2612_WRITE_DATA = 0x80 -> 0x8F,*/
	for (i = 0; i < 16; i++)
	{
		LJ_VGM_validInstruction[LJ_VGM_YM2612_WRITE_DATA+i] = 1;
		LJ_VGM_instruction[LJ_VGM_YM2612_WRITE_DATA+i]= "YM2612_WRITE_DATA";
	}

	/*LJ_VGM_WAIT_N_SAMPLES = 0x70 -> 0x7F,*/
	for (i = 0; i < 16; i++)
	{
		LJ_VGM_validInstruction[LJ_VGM_WAIT_N_SAMPLES+i] = 1;
		LJ_VGM_instruction[LJ_VGM_WAIT_N_SAMPLES+i]= "WAIT_N_SAMPLES";
	}

	/*LJ_VGM_GAMEGEAR_PSG = 0x4F, */
	LJ_VGM_validInstruction[LJ_VGM_GAMEGEAR_PSG] = 1;
	LJ_VGM_instruction[LJ_VGM_GAMEGEAR_PSG]= "GAMEGEAR_PSG";

	/*LJ_VGM_SN764xx_PSG = 0x50, */
	LJ_VGM_validInstruction[LJ_VGM_SN764xx_PSG] = 1;
	LJ_VGM_instruction[LJ_VGM_SN764xx_PSG]= "SN764xx_PSG";

	/*LJ_VGM_WAIT_SAMPLES = 0x61,*/
	LJ_VGM_validInstruction[LJ_VGM_WAIT_SAMPLES] = 1;
	LJ_VGM_instruction[LJ_VGM_WAIT_SAMPLES]= "WAIT_SAMPLES";

	/*LJ_VGM_END_SOUND_DATA = 0x66,*/
	LJ_VGM_validInstruction[LJ_VGM_END_SOUND_DATA] = 1;
	LJ_VGM_instruction[LJ_VGM_END_SOUND_DATA]= "END_SOUND_DATA";

	/*LJ_VGM_WAIT_60th = 0x62,*/
	LJ_VGM_validInstruction[LJ_VGM_WAIT_60th] = 1;
	LJ_VGM_instruction[LJ_VGM_WAIT_60th]= "WAIT_60th";

	/*LJ_VGM_WAIT_50th = 0x63,*/
	LJ_VGM_validInstruction[LJ_VGM_WAIT_50th] = 1;
	LJ_VGM_instruction[LJ_VGM_WAIT_50th]= "WAIT_50th";

/*
	LJ_VGM_SN764xx_PSG = 0x50, 
	LJ_VGM_YM2413_WRITE =0x51, 
	LJ_VGM_YM2151_WRITE = 0x54,

0x30..0x4e dd          : one operand, reserved for future use
0x55..0x5f dd dd       : two operands, reserved for future use
0xa0..0xbf dd dd       : two operands, reserved for future use
0xc0..0xdf dd dd dd    : three operands, reserved for future use
0xe1..0xff dd dd dd dd : four operands, reserved for future use
*/

	vgm_clear(vgmFile);
}

static LJ_VGM_RESULT vgm_open(LJ_VGM_FILE* const vgmFile, const char* const fname)
{
	FILE* fh = NULL;
	int result = 0;
	int dataStart = 0;

	if ( vgmFile == NULL )
	{
		fprintf(stderr, "vgm_open:vgmFile is NULL trying to open file '%s'\n", fname);
		return LJ_VGM_ERROR;
	}

	fh = fopen(fname, "rb");
	if (fh == NULL)
	{
		fprintf(stderr, "vgm_open:Failed to open file '%s'\n", fname);
		return LJ_VGM_ERROR;
	}

	result = (int)fread(&vgmFile->header, sizeof(vgmFile->header), 1, fh);
	if (result != 1)
	{
		fprintf(stderr, "vgm_open: failed to read header result:%d\n", result);
		return LJ_VGM_ERROR;
	}

	printf("VGM Header:\n");
	printf("ident:%d\n", vgmFile->header.ident);
	printf("EOFoffset:%d\n", vgmFile->header.EOFoffset);
	printf("version:%d\n", vgmFile->header.version);
	printf("sn76489Clock:%d\n", vgmFile->header.sn76489Clock);
	printf("ym2413Clock:%d\n", vgmFile->header.ym2413Clock);
	printf("GD3offset:%d\n", vgmFile->header.GD3offset);
	printf("numSamples:%d\n", vgmFile->header.numSamples);
	printf("loopOffset:%d\n", vgmFile->header.loopOffset);
	printf("numLoops:%d\n", vgmFile->header.numLoops);
	printf("rate:%d\n", vgmFile->header.rate);
	printf("sn76489Feedback:%d\n", vgmFile->header.sn76489Feedback);
	printf("sn76489ShiftWidth:%d\n", vgmFile->header.sn76489ShiftWidth);
	printf("padding8_1:%d\n", vgmFile->header.padding8_1);
	printf("ym2612Clock:%d\n", vgmFile->header.ym2612Clock);
	printf("ym2151Clock:%d\n", vgmFile->header.ym2151Clock);
	printf("vgmOffset:%d\n", vgmFile->header.vgmOffset);
	printf("padding32_1:%d\n", vgmFile->header.padding32_1);
	printf("padding32_2:%d\n", vgmFile->header.padding32_2);

	vgmFile->fh = fh;
	vgmFile->pos = 0;
	vgmFile->cmdCount = 0;
	vgmFile->dataStart = 0;

	dataStart = (int)vgmFile->header.vgmOffset;
	if (dataStart == 0x0)
	{
		dataStart = 0x40-0xC;
	}
	dataStart += (0x40-0xC);
	vgmFile->dataStart = dataStart;
	vgmFile->pos = dataStart;

	result = fseek(vgmFile->fh, dataStart, SEEK_SET);
	if (result != 0)
	{
		fprintf(stderr, "vgm_open: failed to seek to start of data:%d result:%d", dataStart, result);
		return LJ_VGM_ERROR;
	}
	printf("dataStart = 0x%X\n",dataStart);
	return LJ_VGM_OK;
}

/*
* ////////////////////////////////////////////////////////////////////
* // 
* // Exposed external data & functions
* // 
* ////////////////////////////////////////////////////////////////////
*/

LJ_VGM_FILE* LJ_VGM_create(const char* const fname)
{
	int result = LJ_VGM_ERROR;
	LJ_VGM_FILE* vgmFile = malloc(sizeof(LJ_VGM_FILE));
	if (vgmFile == NULL)
	{
		fprintf(stderr, "LJ_VGM_create:malloc failed for file '%s'\n", fname);
		return NULL;
	}

	vgm_init(vgmFile);

	if (fname != NULL)
	{
		result = vgm_open(vgmFile,fname);
		if (result == LJ_VGM_ERROR)
		{
			fprintf(stderr, "LJ_VGM_create:vgm_open failed for file '%s'\n", fname);
			LJ_VGM_destroy(vgmFile);
			return NULL;
		}
	}

	return vgmFile;
}

LJ_VGM_RESULT LJ_VGM_destroy(LJ_VGM_FILE* const vgmFile)
{
	if (vgmFile == NULL)
	{
		return LJ_VGM_OK;
	}
	if (vgmFile->fh != NULL)
	{
		fclose(vgmFile->fh);
	}

/*
	printf("VGM:numSamples:%d\n", vgmFile->header.numSamples);
	printf("VGM:loopOffset:%d\n", vgmFile->header.loopOffset);
	printf("VGM:numLoops:%d\n", vgmFile->header.numLoops);
*/

	free(vgmFile->data);

	vgm_clear(vgmFile);

	free(vgmFile);

	return LJ_VGM_OK;
}

LJ_VGM_RESULT LJ_VGM_read(LJ_VGM_FILE* const vgmFile, LJ_VGM_INSTRUCTION* const vgmInstr)
{
	size_t numRead = 0;
	LJ_VGM_UINT8 cmd = 0;
	int pos = 0;
	int cmdCount = 0;

	if (vgmFile == NULL)
	{
		fprintf(stderr, "LJ_VGM_read:vgmFile is NULL\n" );
		return LJ_VGM_ERROR;
	}

	pos = vgmFile->pos;
	cmdCount = vgmFile->cmdCount;
	numRead = fread(&cmd, sizeof(cmd), 1, vgmFile->fh);
	if (numRead != 1)
	{
		fprintf(stderr, "LJ_VGM_read: failed to read cmd byte pos:%d\n", pos);
		return LJ_VGM_ERROR;
	}

	if (LJ_VGM_validInstruction[cmd] == 0)
	{
		fprintf(stderr, "LJ_VGM_read: unknown cmd:0x%X pos:%d\n", cmd, pos);
		return LJ_VGM_ERROR;
	}

	vgmInstr->waitSamples = 0;

	if (cmd == LJ_VGM_DATA_BLOCK_START)
	{
		LJ_VGM_UINT8 endSoundData = 0;
		/*The data block format is: 0x67 0x66 tt ss ss ss ss (data)*/
		numRead = fread(&endSoundData, sizeof(endSoundData), 1, vgmFile->fh);
		if (numRead != 1)
		{
			fprintf(stderr, "LJ_VGM_read: failed to read END_SOUND_DATA cmd byte pos:%d\n", pos);
			return LJ_VGM_ERROR;
		}
		if (endSoundData != LJ_VGM_END_SOUND_DATA)
		{
			fprintf(stderr, "LJ_VGM_read: invalid cmd byte expecting END_SOUND_DATA got:0x%X pos:%d\n", endSoundData, pos);
			return LJ_VGM_ERROR;
		}
		numRead = fread(&vgmFile->dataType, sizeof(vgmFile->dataType), 1, vgmFile->fh);
		if (numRead != 1)
		{
			fprintf(stderr, "LJ_VGM_read: failed to read dataType byte pos:%d\n", pos);
			return LJ_VGM_ERROR;
		}
		if (vgmFile->dataType != LJ_VGM_DATA_YM2612)
		{
			fprintf(stderr, "LJ_VGM_read: invalid dataType expecting 0x00 got:0x%X pos:%d\n", vgmFile->dataType, pos);
			return LJ_VGM_ERROR;
		}
		numRead = fread(&vgmFile->dataNum, sizeof(vgmFile->dataNum), 1, vgmFile->fh);
		if (numRead != 1)
		{
			fprintf(stderr, "LJ_VGM_read: failed to read dataNum bytes pos:%d\n", pos);
			return LJ_VGM_ERROR;
		}
		vgmFile->data = malloc(vgmFile->dataNum);
		if (vgmFile->data == NULL)
		{
			fprintf(stderr, "LJ_VGM_read: failed to allocate data array size:%d pos:%d\n", vgmFile->dataNum, pos);
			return LJ_VGM_ERROR;
		}
		numRead = fread(vgmFile->data, 1, vgmFile->dataNum, vgmFile->fh);
		if (numRead != vgmFile->dataNum)
		{
			fprintf(stderr, "LJ_VGM_read: failed to read dataNum:%d bytes numRead:%d pos:%d\n", vgmFile->dataNum, numRead, pos);
			return LJ_VGM_ERROR;
		}

		vgmFile->pos += (int)(7 + vgmFile->dataNum);
		vgmFile->cmdCount += 1;
		
		vgmFile->dataSeekOffset = 0;
		vgmFile->dataCurrentOffset = 0;

		vgmInstr->pos = pos;
		vgmInstr->cmd = cmd;
		vgmInstr->cmdCount = cmdCount;

		vgmInstr->dataSeekOffset = vgmFile->dataSeekOffset;

		return LJ_VGM_OK;
	}
	else if ((cmd == LJ_VGM_YM2612_WRITE_PART_0) || (cmd == LJ_VGM_YM2612_WRITE_PART_1))
	{
		/*0x52 aa dd 		: YM2612 port 0, write value dd to register aa*/
		/*0x53 aa dd 		: YM2612 port 1, write value dd to register aa*/
		LJ_VGM_UINT8 R = 0;
		LJ_VGM_UINT8 D = 0;

		numRead = fread(&R, sizeof(R), 1, vgmFile->fh);
		if (numRead != 1)
		{
			fprintf(stderr, "LJ_VGM_read: failed to read R byte cmd:%d pos:%d\n", cmd, pos);
			return LJ_VGM_ERROR;
		}

		numRead = fread(&D, sizeof(D), 1, vgmFile->fh);
		if (numRead != 1)
		{
			fprintf(stderr, "LJ_VGM_read: failed to read D byte cmd:%d pos:%d\n", cmd, pos);
			return LJ_VGM_ERROR;
		}

		vgmFile->pos += 3;
		vgmFile->cmdCount += 1;

		vgmInstr->pos = pos;
		vgmInstr->cmdCount = cmdCount;
		vgmInstr->cmd = cmd;

		vgmInstr->R = R;
		vgmInstr->D = D;

		return LJ_VGM_OK;
	}
	else if (cmd == LJ_VGM_DATA_SEEK_OFFSET)
	{
		/*0xe0 dddddddd 	: seek to offset dddddddd (Intel byte order) in PCM data bank*/
		numRead = fread(&vgmFile->dataSeekOffset, sizeof(vgmFile->dataSeekOffset), 1, vgmFile->fh);
		if (numRead != 1)
		{
			fprintf(stderr, "LJ_VGM_read: failed to read data seek offset pos:%d\n", pos);
			return LJ_VGM_ERROR;
		}

		vgmFile->pos += 5;
		vgmFile->cmdCount += 1;

		vgmFile->dataCurrentOffset = 0;

		vgmInstr->pos = pos;
		vgmInstr->cmdCount = cmdCount;
		vgmInstr->cmd = cmd;

		vgmInstr->dataSeekOffset = vgmFile->dataSeekOffset;

		return LJ_VGM_OK;
	}
	else if ((cmd >> 4) == 0x8)
	{
		/*0x8n  : YM2612 port 0 address 2A write from the data bank, then wait n samples; n can range from 0 to 15. */
		LJ_VGM_UINT8 waitSamples = cmd & 0xF;

		vgmFile->pos += 1;
		vgmFile->cmdCount += 1;

		vgmFile->waitSamples = waitSamples;

		vgmInstr->pos = pos;
		vgmInstr->cmdCount = cmdCount;
		vgmInstr->cmd = LJ_VGM_YM2612_WRITE_DATA;

		vgmInstr->dataSeekOffset = vgmFile->dataSeekOffset;
		vgmInstr->waitSamples = (LJ_VGM_INT32)(vgmFile->waitSamples);
		vgmInstr->waitSamplesData = (LJ_VGM_INT32)(vgmFile->waitSamples);

		vgmInstr->R = 0x2A;
		vgmInstr->D = vgmFile->data[vgmFile->dataCurrentOffset];

		vgmFile->dataCurrentOffset += 1;

		return LJ_VGM_OK;
	}
	else if ((cmd >> 4) == 0x7)
	{
		/*0x7n : wait n+1 samples, n can range from 0 to 15.*/
		LJ_VGM_UINT8 waitSamples = cmd & 0xF;

		vgmFile->pos += 1;
		vgmFile->cmdCount += 1;

		vgmFile->waitSamples = (LJ_VGM_UINT32)(waitSamples + 1);

		vgmInstr->pos = pos;
		vgmInstr->cmdCount = cmdCount;
		vgmInstr->cmd = LJ_VGM_WAIT_N_SAMPLES;

		vgmInstr->waitSamples = (LJ_VGM_INT32)(vgmFile->waitSamples);
		vgmInstr->waitSamplesData = (LJ_VGM_INT32)(vgmFile->waitSamples);

		return LJ_VGM_OK;
	}
	else if (cmd == LJ_VGM_GAMEGEAR_PSG)
	{
		/* 0x4f dd : Game Gear PSG stereo, write dd to port 0x06*/
		LJ_VGM_UINT8 D = 0;
		numRead = fread(&D, sizeof(D), 1, vgmFile->fh);
		if (numRead != 1)
		{
			fprintf(stderr, "LJ_VGM_read: failed to read D byte pos:%d\n", pos);
			return LJ_VGM_ERROR;
		}

		vgmFile->pos += 2;
		vgmFile->cmdCount += 1;

		vgmInstr->pos = pos;
		vgmInstr->cmd = cmd;
		vgmInstr->cmdCount = cmdCount;

		vgmInstr->D = D;

		return LJ_VGM_OK;
	}
	else if (cmd == LJ_VGM_SN764xx_PSG)
	{
		/*0x50 dd : PSG (SN76489/SN76496) write value dd*/
		LJ_VGM_UINT8 D = 0;
		numRead = fread(&D, sizeof(D), 1, vgmFile->fh);
		if (numRead != 1)
		{
			fprintf(stderr, "LJ_VGM_read: failed to read D byte pos:%d\n", pos);
			return LJ_VGM_ERROR;
		}

		vgmFile->pos += 2;
		vgmFile->cmdCount += 1;

		vgmInstr->pos = pos;
		vgmInstr->cmd = cmd;
		vgmInstr->cmdCount = cmdCount;

		vgmInstr->D = D;

		return LJ_VGM_OK;
	}
	else if (cmd == LJ_VGM_WAIT_SAMPLES)
	{
		/*0x61 nn nn 		: Wait n samples, n can range from 0 to 65535 (approx 1.49 seconds). */
		LJ_VGM_UINT16 waitSamples;
		numRead = fread(&waitSamples, sizeof(waitSamples), 1, vgmFile->fh);
		if (numRead != 1)
		{
			fprintf(stderr, "LJ_VGM_read: failed to read waitSamples offset pos:%d\n", pos);
			return LJ_VGM_ERROR;
		}

		vgmFile->pos += 3;
		vgmFile->cmdCount += 1;

		vgmFile->waitSamples = waitSamples;

		vgmInstr->pos = pos;
		vgmInstr->cmdCount = cmdCount;
		vgmInstr->cmd = cmd;

		vgmInstr->waitSamples = (LJ_VGM_INT32)(vgmFile->waitSamples);
		vgmInstr->waitSamplesData = (LJ_VGM_INT32)(vgmFile->waitSamples);

		return LJ_VGM_OK;
	}
	else if (cmd == LJ_VGM_END_SOUND_DATA)
	{
		/*0x66 : end of sound data*/
		vgmFile->pos += 1;
		vgmFile->cmdCount += 1;

		vgmInstr->pos = pos;
		vgmInstr->cmdCount = cmdCount;
		vgmInstr->cmd = cmd;

		return LJ_VGM_OK;
	}
	else if (cmd == LJ_VGM_WAIT_60th)
	{
		/*0x62 : wait 735 samples (60th of a second), a shortcut for 0x61 0xdf 0x02*/
		vgmFile->pos += 1;
		vgmFile->cmdCount += 1;

		vgmFile->waitSamples = 735;

		vgmInstr->pos = pos;
		vgmInstr->cmdCount = cmdCount;
		vgmInstr->cmd = cmd;

		vgmInstr->waitSamples = (LJ_VGM_INT32)(vgmFile->waitSamples);
		vgmInstr->waitSamplesData = (LJ_VGM_INT32)(vgmFile->waitSamples);

		return LJ_VGM_OK;
	}
	else if (cmd == LJ_VGM_WAIT_50th)
	{
		/*0x63 : wait 882 samples (50th of a second), a shortcut for 0x61 0x72 0x03*/
		vgmFile->pos += 1;
		vgmFile->cmdCount += 1;

		vgmFile->waitSamples = 882;

		vgmInstr->pos = pos;
		vgmInstr->cmdCount = cmdCount;
		vgmInstr->cmd = cmd;

		vgmInstr->waitSamples = (LJ_VGM_INT32)(vgmFile->waitSamples);
		vgmInstr->waitSamplesData = (LJ_VGM_INT32)(vgmFile->waitSamples);

		return LJ_VGM_OK;
	}

	fprintf(stderr, "LJ_VGM_read: unhandled cmd:0x%X '%s' pos:%d\n", cmd, LJ_VGM_instruction[cmd], pos);
	return LJ_VGM_ERROR;
}

LJ_VGM_RESULT LJ_VGM_getHeader(LJ_VGM_FILE* const vgmFile, LJ_VGM_HEADER* const vgmHeader)
{
	if (vgmFile == NULL)
	{
		fprintf(stderr, "LJ_VGM_getHeader:vgmFile is NULL\n" );
		return LJ_VGM_ERROR;
	}

	*vgmHeader = vgmFile->header;
	return LJ_VGM_OK;
}

void LJ_VGM_debugPrint(const LJ_VGM_INSTRUCTION* const vgmInstr)
{
	int pos;
	int cmd;
	int cmdCount;

	if (vgmInstr == NULL)
	{
		fprintf(stderr, "LJ_VGM_debugPrint:vgmInstr is NULL\n" );
		return;
	}

	pos = vgmInstr->pos;
	cmd = vgmInstr->cmd;
	cmdCount = vgmInstr->cmdCount;

	printf("VGM:%d pos:%d cmd:0x%X %s", cmdCount, pos, cmd, LJ_VGM_instruction[cmd]);
	if (cmd == LJ_VGM_DATA_BLOCK_START)
	{
		printf(" dataType:0x%X dataNum:%d", vgmInstr->dataType, vgmInstr->dataNum);
	}
	else if ((cmd == LJ_VGM_YM2612_WRITE_PART_0) || (cmd == LJ_VGM_YM2612_WRITE_PART_1))
	{
		const int R = vgmInstr->R;
		const int D = vgmInstr->D;
		printf(" REG:0x%X DATA:0x%X", R, D);
	}
	else if (cmd == LJ_VGM_DATA_SEEK_OFFSET)
	{
		printf(" offset:%d", vgmInstr->dataSeekOffset);
	}
	else if (cmd == LJ_VGM_YM2612_WRITE_DATA)
	{
		const int R = vgmInstr->R;
		const int D = vgmInstr->D;
		printf(" REG:0x%X DATA:0x%X waitSamplesData:%d waitSamples:%d", R, D, vgmInstr->waitSamplesData, vgmInstr->waitSamples);
	}
	else if (cmd == LJ_VGM_GAMEGEAR_PSG)
	{
		const int D = vgmInstr->D;
		printf(" DATA:0x%X", D);
	}
	else if (cmd == LJ_VGM_SN764xx_PSG)
	{
		const int D = vgmInstr->D;
		printf(" DATA:0x%X", D);
	}
	else if (cmd == LJ_VGM_WAIT_N_SAMPLES)
	{
		printf(" waitSamplesData:%d waitSamples:%d", vgmInstr->waitSamplesData, vgmInstr->waitSamples);
	}
	else if (cmd == LJ_VGM_WAIT_SAMPLES)
	{
		printf(" waitSamplesData:%d waitSamples:%d", vgmInstr->waitSamplesData, vgmInstr->waitSamples);
	}
	else if (cmd == LJ_VGM_WAIT_60th)
	{
		printf(" waitSamplesData:%d waitSamples:%d", vgmInstr->waitSamplesData, vgmInstr->waitSamples);
	}
	else if (cmd == LJ_VGM_WAIT_50th)
	{
		printf(" waitSamplesData:%d waitSamples:%d", vgmInstr->waitSamplesData, vgmInstr->waitSamples);
	}

	printf("\n");
}
