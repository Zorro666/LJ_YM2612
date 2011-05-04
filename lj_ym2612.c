#include "lj_ym2612.h"

#include <stdio.h>
#include <malloc.h>
#include <math.h>

////////////////////////////////////////////////////////////////////
// 
// Internal data & functions
// 
////////////////////////////////////////////////////////////////////

typedef enum LJ_YM2612_REGISTERS LJ_YM2612_REGISTERS;
typedef struct LJ_YM2612_PORT LJ_YM2612_PORT;
typedef struct LJ_YM2612_CHANNEL LJ_YM2612_CHANNEL;

#define LJ_YM2612_NUM_REGISTERS (0xFF)
#define LJ_YM2612_NUM_PORTS (2)
#define LJ_YM2612_NUM_CHANNELS_PER_PORT (3)

// Random guess - needs to be in sync with how the envelope amplitude works also to match up
#define DAC_SHIFT (6)

enum LJ_YM2612_REGISTERS {
		LJ_DAC = 0x2A,
		LJ_DAC_EN = 0x2B,
		LJ_KEY_ONOFF = 0x28,
		//LJ_FREQLSB = 0xA0,
		//LJ_BLOCK_FREQMSB = 0xA4,
		//LJ_FEEDBACK_ALGO = 0xB0,
		//LJ_LR_AMS_PMS = 0xB4,
};

static const char* LJ_YM2612_REGISTER_NAMES[LJ_YM2612_NUM_REGISTERS];
static LJ_YM_UINT8 LJ_YM2612_validRegisters[LJ_YM2612_NUM_REGISTERS];

//FNUM = 11-bit
#define LJ_YM2612_FNUM_BITS (11)
#define LJ_YM2612_NUM_FNUM_ENTRIES (1 << LJ_YM2612_FNUM_BITS)
#define LJ_YM2612_FNUM_MASK ((1 << LJ_YM2612_FNUM_BITS) - 1)
static LJ_YM_UINT32 LJ_YM2612_fnumTable[LJ_YM2612_NUM_FNUM_ENTRIES];

struct LJ_YM2612_CHANNEL
{
	LJ_YM_UINT32 fnum;
	LJ_YM_UINT32 blk;
	LJ_YM_UINT32 freqDelta;
	LJ_YM_UINT32 volume;
	LJ_YM_UINT8 id;
};

struct LJ_YM2612_PORT
{
	LJ_YM_UINT8 reg[LJ_YM2612_NUM_REGISTERS];
	LJ_YM_UINT8 regAddress;
	LJ_YM2612_CHANNEL channel[3];
	LJ_YM_UINT8 id;
};

struct LJ_YM2612
{
	LJ_YM2612_PORT port[LJ_YM2612_NUM_PORTS];

	LJ_YM_INT16 dacValue;
	LJ_YM_UINT16 dacEnable;
	LJ_YM_UINT32 flags;
	float baseFreqScale;
};

static void ym2612_channelKeyOnOff(LJ_YM2612_CHANNEL* const channel, LJ_YM_UINT8 slotOnOff)
{
	if (slotOnOff & 0xF)
	{
		printf( "ym2612:channelKeyOnOff ON id:%d\n",channel->id);
		channel->volume = (1<<13);
	}
	else
	{
		printf( "ym2612:channelKeyOnOff OFF id:%d\n",channel->id);
		channel->volume = 0;
	}
}

static void ym2612_channelClear(LJ_YM2612_CHANNEL* const channel)
{
	channel->fnum = 0;
	channel->blk = 0;
	channel->freqDelta = 0;
	channel->volume = 0;
}

static void ym2612_portClear(LJ_YM2612_PORT* const port)
{
	int i;
	for (i=0; i<LJ_YM2612_NUM_REGISTERS; i++)
	{
		port->reg[i] = 0x0;
	}
	for (i=0; i<LJ_YM2612_NUM_CHANNELS_PER_PORT; i++)
	{
		LJ_YM2612_CHANNEL* const channel = &(port->channel[i]);
		channel->id = port->id*LJ_YM2612_NUM_CHANNELS_PER_PORT+i;
		ym2612_channelClear(channel);
	}
}

static void ym2612_makeData(LJ_YM2612* const ym2612)
{
	const int clock = 7670453;
	const int rate = 44100;
	int i;

	// Fvalue = (144 * freq * 2^20 / masterClock) / 2^(B-1)
	// e.g. D = 293.7Hz F = 692.8 B=4
	// freq = F * 2^(B-1) * baseFreqScale / ( 2^20 )
	// freqScale = (chipClock/sampleRateOutput) / ( 144 );
	ym2612->baseFreqScale = ((float)clock / (float)rate ) / 144.0f;

	for (i = 0; i < LJ_YM2612_NUM_FNUM_ENTRIES; i++)
	{
		float freq = ym2612->baseFreqScale * i;
		LJ_YM2612_fnumTable[i] = freq;
	}
}

static void ym2612_clear(LJ_YM2612* const ym2612)
{
	int i;

	ym2612->dacValue = 0;
	ym2612->dacEnable = 0x0;
	ym2612->flags = 0x0;
	ym2612->baseFreqScale = 0.0f;

	for (i=0; i<LJ_YM2612_NUM_PORTS; i++)
	{
		LJ_YM2612_PORT* const port = &(ym2612->port[i]);
		port->id = i;
		ym2612_portClear(port);
	}
	for (i=0; i<LJ_YM2612_NUM_REGISTERS; i++)
	{
		LJ_YM2612_validRegisters[i] = 0;
		LJ_YM2612_REGISTER_NAMES[i] = "UNKNOWN";
	}
	
	//Known specific registers (only need explicit listing for the system type parameters which aren't per channel settings
	LJ_YM2612_validRegisters[LJ_DAC] = 1;
	LJ_YM2612_REGISTER_NAMES[LJ_DAC] = "DAC";

	LJ_YM2612_validRegisters[LJ_DAC_EN] = 1;
	LJ_YM2612_REGISTER_NAMES[LJ_DAC_EN] = "DAC_EN";

	LJ_YM2612_validRegisters[LJ_KEY_ONOFF] = 1;
	LJ_YM2612_REGISTER_NAMES[LJ_KEY_ONOFF] = "KEY_ONOFF";

	//LJ_YM2612_validRegisters[LJ_FREQLSB] = 1;
	//LJ_YM2612_REGISTER_NAMES[LJ_FREQLSB] = "FREQ(LSB)";

	//LJ_YM2612_validRegisters[LJ_BLOCK_FREQMSB] = 1;
	//LJ_YM2612_REGISTER_NAMES[LJ_BLOCK_FREQMSB] = "BLOCK_FREQ(MSB)";

	//LJ_YM2612_validRegisters[LJ_FEEDBACK_ALGO] = 1;
	//LJ_YM2612_REGISTER_NAMES[LJ_FEEDBACK_ALGO] = "FEEDBACK_ALGO";

	//LJ_YM2612_validRegisters[LJ_LR_AMS_PMS] = 1;
	//LJ_YM2612_REGISTER_NAMES[LJ_LR_AMS_PMS] = "LR_AMS_PMS";

	//For parameters mark all the associated registers as valid
	for (i=0; i<LJ_YM2612_NUM_REGISTERS; i++)
	{
		if (LJ_YM2612_validRegisters[i] == 1)
		{
			//Found a valid register
			//0x20 = system which is specific set of registers
			//0x30-0x90 = settings per channel per slot (channel = bottom 2 bits of reg, slot = reg / 4)
			//0xA0-0xB0 = settings per channel (channel = bottom 2 bits of reg)
			//0xB0, 0xB1, 0xB2 = feedback, algorithm
			//0xB4, 0xB5, 0xB6 = feedback, algorithm
			if ((i >= 0x20) && (i < 0x30))
			{
			}
			if ((i >= 0x30) && (i <= 0x90))
			{
			}
			if ((i >= 0xA0) && (i <= 0xB6))
			{
				int regBase = (i >> 2) << 2;
				int j;
				for (j=0; j<3; j++)
				{
					const char* const regBaseName = LJ_YM2612_REGISTER_NAMES[regBase];
					LJ_YM2612_validRegisters[regBase+j] = 1;
					LJ_YM2612_REGISTER_NAMES[regBase+j] = regBaseName;
				}
			}
			if (i == 0xA0) 
			{
				//Special bits for 0xA8->0xAE
			}
		}
	}
}

LJ_YM2612_RESULT ym2612_setRegister(LJ_YM2612* const ym2612, LJ_YM_UINT8 port, LJ_YM_UINT8 reg, LJ_YM_UINT8 data)
{
	LJ_YM_UINT32 parameter = 0;
	//LJ_YM_UINT32 channel = 0;
	//LJ_YM_UINT32 slot = 0;

	if (ym2612 == NULL)
	{
		fprintf(stderr, "ym2612_setRegister:ym2612 is NULL\n");
		return LJ_YM2612_ERROR;
	}

	if (port >= LJ_YM2612_NUM_PORTS)
	{
		fprintf(stderr, "ym2612_setRegister:invalid port:%d max:%d \n",port, LJ_YM2612_NUM_PORTS);
		return LJ_YM2612_ERROR;
	}

	// convert from register to parameter
	parameter = (reg & 0xF0) >> 4;

	if (LJ_YM2612_validRegisters[reg] == 0)
	{
		fprintf(stderr, "ym2612_setRegister:unknown register:0x%X port:%d data:0x%X\n", reg, port, data);
		return LJ_YM2612_ERROR;
	}

	ym2612->port[port].reg[reg] = data;
	if (ym2612->flags & LJ_YM2612_DEBUG)
	{
		printf( "ym2612:setRegister %s 0x%X\n", LJ_YM2612_REGISTER_NAMES[reg],data);
	}

	if (reg == LJ_DAC_EN)
	{
		ym2612->dacEnable = 0xFFFF * ((data & 0x80) >> 7);
	}
	else if (reg == LJ_DAC)
	{
		ym2612->dacValue = ((LJ_YM_INT16)(data - 0x80)) << DAC_SHIFT;
	}
	else if (reg == LJ_KEY_ONOFF)
	{
		const int channel = data & 0x03;
		if (channel != 3)
		{
			const int slotOnOff = (data >> 4);
			if ((data & 0x04) == 0)
			{
				ym2612_channelKeyOnOff(&ym2612->port[port].channel[channel], slotOnOff);
			}
			else
			{
				ym2612_channelKeyOnOff(&ym2612->port[1].channel[channel], slotOnOff);
			}
		}
	}

	return LJ_YM2612_OK;
}

////////////////////////////////////////////////////////////////////
// 
// External data & functions
// 
////////////////////////////////////////////////////////////////////

LJ_YM2612* LJ_YM2612_create(void)
{
	LJ_YM2612* const ym2612 = malloc(sizeof(LJ_YM2612));
	if (ym2612 == NULL)
	{
		fprintf(stderr, "LJ_YM2612_create:malloc failed\n");
		return NULL;
	}

	ym2612_clear(ym2612);
	ym2612_makeData(ym2612);

	return ym2612;
}

LJ_YM2612_RESULT LJ_YM2612_setFlags(LJ_YM2612* const ym2612, const unsigned int flags)
{
	if (ym2612 == NULL)
	{
		fprintf(stderr, "LJ_YM2612_setFlags:ym2612 is NULL\n");
		return LJ_YM2612_ERROR;
	}

	ym2612->flags = flags;
	return LJ_YM2612_OK;
}

LJ_YM2612_RESULT LJ_YM2612_destroy(LJ_YM2612* const ym2612)
{
	if (ym2612 == NULL)
	{
		fprintf(stderr, "LJ_YM2612_destroy:ym2612 is NULL\n");
		return LJ_YM2612_ERROR;
	}
	ym2612_clear(ym2612);
	free(ym2612);

	return LJ_YM2612_OK;
}

LJ_YM2612_RESULT LJ_YM2612_generateOutput(LJ_YM2612* const ym2612, int numCycles, LJ_YM_INT16* output[2])
{
	LJ_YM_INT16* outputLeft = output[0];
	LJ_YM_INT16* outputRight = output[1];
	LJ_YM_INT16 dacValue = 0;
	int sample;

	if (ym2612 == NULL)
	{
		fprintf(stderr, "LJ_YM2612_generateOutput:ym2612 is NULL\n");
		return LJ_YM2612_ERROR;
	}

	//Global state update - LFO, DAC, SSG
	dacValue = ym2612->dacValue & ym2612->dacEnable;
	if (ym2612->flags & LJ_YM2612_NODAC)
	{
		dacValue = 0x0;
	}
	static int tVal = 0;

	//For each cycle
	//Loop over channels updating them, mix them, then output them into the buffer
	for (sample=0; sample < numCycles; sample++)
	{
		LJ_YM_INT16 mixedLeft = 0;
		LJ_YM_INT16 mixedRight = 0;

		LJ_YM2612_CHANNEL* const channel = &ym2612->port[0].channel[2];

		const int FNUM = 693;
		const int B = 4;
		// F * 2^(B-1)
		// Could multiply the fnumTable up by (1<<6) then change this to fnumTable >> (7-B)
		int freqDelta = (LJ_YM2612_fnumTable[FNUM] << B) >> 1;
		const float theta = (tVal * freqDelta) / (1024.0f * 1024.0f);
		//convert this to a table lookup (1024 entries)
		const float sinVal = sinf(2.0f * M_PI * theta);
		int fmLevel = (int)(sinVal * 8192.0f);

		fmLevel = (channel->volume * fmLevel) >> 13;

		mixedLeft += fmLevel;
		mixedRight += fmLevel;

		//mixedLeft += dacValue;
		//mixedRight += dacValue;

		outputLeft[sample] = mixedLeft;
		outputRight[sample] = mixedRight;

		tVal += 1;
	}

	return LJ_YM2612_OK;
}

LJ_YM2612_RESULT LJ_YM2612_write(LJ_YM2612* const ym2612, LJ_YM_UINT16 address, LJ_YM_UINT8 data)
{
	int result = LJ_YM2612_ERROR;

	// PORT 0: R = 0x4000, D = 0x4001
	// PORT 1: R = 0x4002, D = 0x4003
	if (ym2612 == NULL)
	{
		fprintf(stderr, "LJ_YM2612_write:ym2612 is NULL\n");
		return LJ_YM2612_ERROR;
	}

	if (address == 0x4000)
	{
		ym2612->port[0].regAddress = data;
		return LJ_YM2612_OK;
	}
	else if (address == 0x4001)
	{
		LJ_YM_UINT8 reg = ym2612->port[0].regAddress;
		result = ym2612_setRegister(ym2612, 0, reg, data);
		return result;
	}
	else if (address == 0x4002)
	{
		ym2612->port[1].regAddress = data;
		return LJ_YM2612_OK;
	}
	else if (address == 0x4003)
	{
		LJ_YM_UINT8 reg = ym2612->port[1].regAddress;
		result = ym2612_setRegister(ym2612, 1, reg, data);
		return result;
	}

	return LJ_YM2612_ERROR;
}

/*
Programmable Sound Generator (PSG)
(This information from a Genesis manual, but it's a standard
chip, so the info is probably public knowledge)

The PSG contains four sound channels, consisting of three tone
generators and a noise generator. Each of the four channels has
an independent volume control (attenuator). The PSG is
controlled through output port $7F.

TONE GENERATOR FREQUENCY

The frequency (pitch) of a tone generator is set by a 10-bit
value. This value is counted down until it reaches zero, at
which time the tone output toggles and the 10-bit value is
reloaded into the counter. Thus, higher 10-bit numbers produce
lower frequencies.

To load a new frequency value into one of the tone generators,
you write a pair of bytes to I/O location $7F according to the
following format:


First byte:     1   R2  R1  R0  d3  d2  d1  d0
Second byte:    0   0   d9  d8  d7  d6  d5  d4


The R2:R1:R0 field selects the tone channel as follows:


R2 R1 R0    Tone Chan.
----------------------
0  0  0     #1
0  1  0     #2
1  0  0     #3


10-bit data is (msb)    d9 d8 d7 d6 d5 d4 d3 d2 d1 d0   (lsb)


NOISE GENERATOR CONTROL

The noise generator uses three control bits to select the
"character" of the noise sound. A bit called "FB" (Feedback)
produces periodic noise of "white" noise:


FB  Noise type

0   Periodic (like low-frequency tone)
1   White (hiss)


The frequency of the noise is selected by two bits NF1:NF0
according to the following table:


NF1 NF0 Noise Generator Clock Source

0   0   Clock/2 [Higher pitch, "less coarse"]
0   1   Clock/4
1   0   Clock/8 [Lower pitch, "more coarse"]
1   1   Tone Generator #3


NOTE:   "Clock" is fixed in frequency. It is a crystal controlled
oscillator signal connected to the PSG.

When NF1:NF0 is 11, Tone Generator #3 supplies the noise clock
source. This allows the noise to be "swept" in frequency. This
effect might be used for a jet engine runup, for example.

To load these noise generator control bits, write the following
byte to I/O port $7F:

Out($7F): 1  1  1  0  0  FB  NF1  NF0


ATTENUATORS

Four attenuators adjust the volume of the three tone generators
and the noise channel. Four bits A3:A2:A1:A0 control the
attenuation as follows:


A3 A2 A1 A0     Attenuation
0  0  0  0       0 db   (maximum volume)
0  0  0  1       2 db   NOTE: a higher attenuation results
.  .  .  .       . .          in a quieter sound.
1  1  1  0      28 db
1  1  1  1      -Off-

The attenuators are set for the four channels by writing the
following bytes to I/O location $7F:


Tone generator #1:    1  0  0  1  A3  A2  A1  A0
Tone generator #2:    1  0  1  1  A3  A2  A1  A0
Tone generator #3:    1  1  0  1  A3  A2  A1  A0
Noise generator:      1  1  1  1  A3  A2  A1  A0


EXAMPLE

When the MK3 is powered on, the following code is executed:

        LD      HL,CLRTB        ;clear table
        LD      C,PSG_PRT       ;PSG port is $7F
        LD      B,4             ;load 4 bytes
        OTIR
        (etc.)
    
CLRTB   defb    $9F, $BF, $DF, $FF

This code turns the four sound channels off. It's a good idea to
also execute this code when the PAUSE button is pressed, so that
the sound does not stay on continuously for the pause interval.
*/

