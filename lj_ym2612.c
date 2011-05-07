#include "lj_ym2612.h"

#include <stdio.h>
#include <malloc.h>
#include <math.h>

/* 
Registers
		D7		D6		D5		D4		D3			D2			D1		D0
22H										LFO enable	LFO frequency
24H		Timer 		A 			MSBs
25H 	#######################################################	Timer A LSBs
26H		Timer 		B
27H		Ch3 mode		Reset B	Reset A	Enable B	Enable A	Load B	Load A
28H		Slot    						###########	Channel
29H	
2AH		DAC
2BH		DAC en	###########################################################
30H+	#######	DT1						MUL
40H+	#######	TL
50H+	RS				#######	AR
60H+	AM		#######	D1R
70H+	###############	D2R
80H+	D1L								RR
90H+	###############################	SSG-EG
A0H+	Frequency 		number 			LSB
A4H+	###############	Block						Frequency Number MSB
A8H+	Ch3 supplementary frequency number
ACH+	#######	Ch3 supplementary block				Ch3 supplementary frequency number
B0H+	#######	Feedback							Algorithm
B4H+	L		R		AMS				###########	FMS
*/

////////////////////////////////////////////////////////////////////
// 
// Internal data & functions
// 
////////////////////////////////////////////////////////////////////

typedef enum LJ_YM2612_REGISTERS LJ_YM2612_REGISTERS;
typedef struct LJ_YM2612_PART LJ_YM2612_PART;
typedef struct LJ_YM2612_CHANNEL LJ_YM2612_CHANNEL;
typedef struct LJ_YM2612_SLOT LJ_YM2612_SLOT;

#define LJ_YM2612_NUM_REGISTERS (0xFF)
#define LJ_YM2612_NUM_PARTS (2)
#define LJ_YM2612_NUM_CHANNELS_PER_PART (3)
#define LJ_YM2612_NUM_CHANNELS_TOTAL (LJ_YM2612_NUM_CHANNELS_PER_PART * LJ_YM2612_NUM_PARTS)
#define LJ_YM2612_NUM_SLOTS_PER_CHANNEL (4)

// Random guess - needs to be in sync with how the envelope amplitude works also to match up
#define DAC_SHIFT (6)

enum LJ_YM2612_REGISTERS {
		LJ_DAC = 0x2A,
		LJ_DAC_EN = 0x2B,
		LJ_KEY_ONOFF = 0x28,
		LJ_DETUNE_MULT = 0x30,
		LJ_FREQLSB = 0xA0,
		LJ_BLOCK_FREQMSB = 0xA4,
		//LJ_FEEDBACK_ALGO = 0xB0,
		//LJ_LR_AMS_PMS = 0xB4,
};

static const char* LJ_YM2612_REGISTER_NAMES[LJ_YM2612_NUM_REGISTERS];
static LJ_YM_UINT8 LJ_YM2612_validRegisters[LJ_YM2612_NUM_REGISTERS];

//FNUM = 11-bit table
#define LJ_YM2612_FNUM_BITS (11)
#define LJ_YM2612_NUM_FNUM_ENTRIES (1 << LJ_YM2612_FNUM_BITS)
#define LJ_YM2612_FNUM_MASK ((1 << LJ_YM2612_FNUM_BITS) - 1)

//FREQ scale = 16.16-bit
#define LJ_YM2612_FREQ_BITS (16)
#define LJ_YM2612_FREQ_MASK ((1 << LJ_YM2612_FREQ_BITS) - 1)
static LJ_YM_UINT32 LJ_YM2612_fnumTable[LJ_YM2612_NUM_FNUM_ENTRIES];

//SIN table = 10-bit
#define LJ_YM2612_SINTABLE_SCALE_BITS (13)
#define LJ_YM2612_SINTABLE_BITS (10)
#define LJ_YM2612_NUM_SINTABLE_ENTRIES (1 << LJ_YM2612_SINTABLE_BITS)
#define LJ_YM2612_SINTABLE_MASK ((1 << LJ_YM2612_SINTABLE_BITS) - 1)
static int LJ_YM2612_sinTable[LJ_YM2612_NUM_SINTABLE_ENTRIES];

//DETUNE table = this are integer freq shifts in the frequency integer scale
// In the docs (YM2608) : the base scale is (0.052982) this equates to: (8*1000*1000/(1*1024*1024))/144
// For YM2612 (Genesis) it would equate to: (7670453/(1*1024*1024)/144 => 0.050799
// 144 is the number clock cycles the chip takes to make 1 sample: 
// 144 = 6 channels x 4 operators x 8 cycles per operator 
// 144 = 6 channels x 24 cycles per channel
// 144 = also the prescaler: YM2612 is 1/6 : one sample for each 24 FM clocks
static const LJ_YM_UINT8 LJ_YM2612_detuneScaleTable[4*32] = {
//FD=0 : all 0
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
//FD=1
0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 
2, 3, 3, 3, 4, 4, 4, 5, 5, 6, 6, 7, 8, 8, 8, 8, 
//FD=2
1, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 4, 4, 4, 5, 
5, 6, 6, 7, 8, 8, 9,10,11,12,13,14,16,16,16,16, 
//FD=3
2, 2, 2, 2, 2, 3, 3, 3, 4, 4, 4, 5, 5, 6, 6, 7, 
8, 8, 9,10,11,12,13,14,16,17,19,20,22,22,22,22, 
};

//Convert detuneScaleTable into a frequency shift (+ and -)
static int LJ_YM2612_detuneTable[8*32];

// fnum -> keycode table for computing N3 & N4
// N4 = Fnum Bit 11 
// N3 = Fnum Bit 11 * (Bit 10 | Bit 9 | Bit 8) | !Bit 11 * Bit 10 * Bit 9 * Bit 8
// Table = (N4 << 1) | N3
static const LJ_YM_UINT8 LJ_YM2612_fnumKeycodeTable[16] = {
0, 0, 0, 0, 0, 0, 0, 1,
2, 3, 3, 3, 3, 3, 3, 3,
};


#define LJ_YM2612_VOLUME_MAX (8192)

struct LJ_YM2612_SLOT
{
	LJ_YM_UINT32 fnum;
	LJ_YM_UINT32 block;
	int omega;
	LJ_YM_UINT32 omegaDelta;
	LJ_YM_UINT32 detuneDelta;

	int volume;
	int volumeDelta;

	LJ_YM_UINT8 block_fnumMSB;
	LJ_YM_UINT8 detune;
	LJ_YM_UINT8 multiple;
	LJ_YM_UINT8 keycode;
};

struct LJ_YM2612_CHANNEL
{
	LJ_YM2612_SLOT slot[LJ_YM2612_NUM_SLOTS_PER_CHANNEL];

	// These should go
	int volume;
	int volumeDelta;

	int flags;

	LJ_YM_UINT8 id;
};

struct LJ_YM2612_PART
{
	LJ_YM_UINT8 reg[LJ_YM2612_NUM_REGISTERS];
	LJ_YM2612_CHANNEL channel[LJ_YM2612_NUM_CHANNELS_PER_PART];
	LJ_YM_UINT8 id;
};

struct LJ_YM2612
{
	LJ_YM2612_PART part[LJ_YM2612_NUM_PARTS];
	LJ_YM2612_CHANNEL* channels[LJ_YM2612_NUM_CHANNELS_TOTAL];

	float baseFreqScale;

	LJ_YM_UINT32 clockRate;
	LJ_YM_UINT32 outputSampleRate;

	LJ_YM_UINT32 flags;

	LJ_YM_INT16 dacValue;
	LJ_YM_UINT16 dacEnable;

	LJ_YM_UINT8 regAddress;
	LJ_YM_UINT8 slotWriteAddr;	
};

static int LJ_YM2612_CLAMP_VOLUME(const int volume) 
{
	if (volume > LJ_YM2612_VOLUME_MAX)
	{
		return LJ_YM2612_VOLUME_MAX;
	}
	if (volume < -LJ_YM2612_VOLUME_MAX)
	{
		return -LJ_YM2612_VOLUME_MAX;
	}
	return volume;
}

static void ym2612_slotComputeOmegaDelta(LJ_YM2612_SLOT* const slot, const LJ_YM2612_CHANNEL* const channel)
{
	const int FNUM = slot->fnum;
	const int B = slot->block;
	const int MULTIPLE = slot->multiple;

	// F * 2^(B-1)
	// Could multiply the fnumTable up by (1<<6) then change this to fnumTable >> (7-B)
	int omegaDelta = (LJ_YM2612_fnumTable[FNUM] << B) >> 1;

	//detuneDelta from keycode and detune value
	const int keycode = slot->keycode;
	const int detune = slot->detune;
	int detuneTableValue = LJ_YM2612_detuneTable[detune*32+keycode];
	slot->detuneDelta = detuneTableValue;
	if (channel->flags & LJ_YM2612_DEBUG)
	{
		printf("detuneDelta %d detune:%d FD:%d keycode:%d\n", detuneTableValue, detune, (detune&0x3), keycode);
	}
	
	omegaDelta += detuneTableValue;

	// /2 because multiple is stored as x2 of its value
	omegaDelta = (omegaDelta * MULTIPLE) >> 1;

	slot->omegaDelta = omegaDelta;

}

static void ym2612_channelSetDetuneMult(LJ_YM2612_CHANNEL* const channel, int slot, LJ_YM_UINT8 detuneMult)
{
	LJ_YM2612_SLOT* const slotPtr = &(channel->slot[slot]);

	// Detune = Bits 4-6, Multiple = Bits 0-3
	const int detune = (detuneMult >> 4) & 0x7;
	const int multiple = (detuneMult >> 0) & 0xF;

	slotPtr->detune = detune;
	//multiple = xN except N=0 then it is x1/2 store it as x2
	slotPtr->multiple = multiple ? (multiple << 1) : 1;

	ym2612_slotComputeOmegaDelta(slotPtr, channel);
	if (channel->flags & LJ_YM2612_DEBUG)
	{
		printf("SetDetuneMult channel:%d slot:%d detune:%d mult:%d\n", channel->id, slot, detune, multiple);
	}
}

static void ym2612_channelSetFreqBlock(LJ_YM2612_CHANNEL* const channel, LJ_YM_UINT8 fnumLSB)
{
	int slot;
	for (slot = 0; slot < LJ_YM2612_NUM_SLOTS_PER_CHANNEL; slot++)
	{
		LJ_YM2612_SLOT* const slotPtr = &(channel->slot[slot]);

		const LJ_YM_UINT8 block_fnumMSB = slotPtr->block_fnumMSB;
		//
		// Block = Bits 3-5, Frequency Number MSB = Bits 0-2 (top 3-bits of frequency number)
		const int block = (block_fnumMSB >> 3) & 0x7;
		const int fnumMSB = (block_fnumMSB >> 0) & 0x3;
		const int fnum = (fnumMSB << 8) + (fnumLSB & 0xFF);
		// keycode = (block << 2) | (N4 << 1) | (N3 << 0)
		// N4 = Fnum Bit 11 
		// N3 = Fnum Bit 11 * (Bit 10 | Bit 9 | Bit 8) | !Bit 11 * Bit 10 * Bit 9 * Bit 8
		const int keycode = (block << 2) | LJ_YM2612_fnumKeycodeTable[fnum>>7];

		slotPtr->block = block;
		slotPtr->fnum = fnum;
		slotPtr->omega = 0;
		slotPtr->keycode = keycode;

		ym2612_slotComputeOmegaDelta(slotPtr, channel);

		if (channel->flags & LJ_YM2612_DEBUG)
		{
			printf("SetFreqBlock channel:%d slot:%d block:%d fnum:%d 0x%X keycode:%d block_fnumMSB:0x%X fnumLSB:0x%X\n", 
					channel->id, slot, block, fnum, fnum, keycode, block_fnumMSB, fnumLSB);
		}
	}
}

static void ym2612_channelSetBlockFnumMSB(LJ_YM2612_CHANNEL* const channel, LJ_YM_UINT8 data)
{
	int slot;
	for (slot = 0; slot < LJ_YM2612_NUM_SLOTS_PER_CHANNEL; slot++)
	{
		LJ_YM2612_SLOT* const slotPtr = &(channel->slot[slot]);

		slotPtr->block_fnumMSB = data;
	}
}

static void ym2612_channelKeyOnOff(LJ_YM2612_CHANNEL* const channel, LJ_YM_UINT8 slotOnOff)
{
	if (slotOnOff & 0xF)
	{
		int slot;

		channel->volume = 0;
		channel->volumeDelta = +1024;
	
		//Set the start of wave
		for (slot = 0; slot < LJ_YM2612_NUM_SLOTS_PER_CHANNEL; slot++)
		{
			LJ_YM2612_SLOT* const slotPtr = &(channel->slot[slot]);
			slotPtr->omega = 0;
		}
	}
	else
	{
		channel->volumeDelta = -1024;
	}
}

static void ym2612_slotClear(LJ_YM2612_SLOT* const slot)
{
	slot->fnum = 0;
	slot->block = 0;
	slot->volume = 0;
	slot->volumeDelta = 0;
	slot->block_fnumMSB = 0;
	slot->detune = 0;
	slot->multiple = 0;
	slot->omega = 0;
	slot->omegaDelta = 0;
}

static void ym2612_channelClear(LJ_YM2612_CHANNEL* const channel)
{
	int i;
	for (i = 0; i < LJ_YM2612_NUM_SLOTS_PER_CHANNEL; i++)
	{
		LJ_YM2612_SLOT* const slot = &(channel->slot[i]);
		ym2612_slotClear(slot);
	}
	channel->flags = 0;
}

static void ym2612_partClear(LJ_YM2612_PART* const part)
{
	int i;
	for (i = 0; i < LJ_YM2612_NUM_REGISTERS; i++)
	{
		part->reg[i] = 0x0;
	}
	for (i = 0; i < LJ_YM2612_NUM_CHANNELS_PER_PART; i++)
	{
		LJ_YM2612_CHANNEL* const channel = &(part->channel[i]);
		channel->id = part->id*LJ_YM2612_NUM_CHANNELS_PER_PART+i;
		ym2612_channelClear(channel);
	}
}

static void ym2612_makeData(LJ_YM2612* const ym2612)
{
	const int clock = ym2612->clockRate;
	const int rate = ym2612->outputSampleRate;
	int i;

	// Fvalue = (144 * freq * 2^20 / masterClock) / 2^(B-1)
	// e.g. D = 293.7Hz F = 692.8 B=4
	// freq = F * 2^(B-1) * baseFreqScale / ( 2^20 )
	// freqScale = (chipClock/sampleRateOutput) / ( 144 );
	ym2612->baseFreqScale = ((float)clock / (float)rate ) / 144.0f;

	for (i = 0; i < LJ_YM2612_NUM_FNUM_ENTRIES; i++)
	{
		float freq = ym2612->baseFreqScale * i;
		// Include the sin table size in the (1/2^20)
		freq = freq * (float)(1 << (LJ_YM2612_FREQ_BITS - (20 - LJ_YM2612_SINTABLE_BITS)));

		LJ_YM_UINT32 intFreq = (LJ_YM_UINT32)(freq);
		LJ_YM2612_fnumTable[i] = intFreq;
	}

	const float omegaScale = (2.0f * M_PI / LJ_YM2612_NUM_SINTABLE_ENTRIES);
	for (i = 0; i < LJ_YM2612_NUM_SINTABLE_ENTRIES; i++)
	{
		const float omega = omegaScale * i;
		const float sinValue = sinf(omega);
		const int scaledSin = (int)(sinValue * (float)(1 << LJ_YM2612_SINTABLE_SCALE_BITS));
		LJ_YM2612_sinTable[i] = scaledSin;
	}
	//DT1 = -3 -> 3
#if 0 
	for (i = 0; i < 4; i++)
	{
		int keycode;
		for (keycode = 0; keycode < 32; keycode++)
		{
			int detuneScaleTableValue = LJ_YM2612_detuneScaleTable[i*32+keycode];
			float detuneMultiple = (float)detuneScaleTableValue * (0.529f/10.0f);
			int detFPValue = detuneScaleTableValue * (( 529 * (1<<16) / 10000 ));
			float fpValue = (float)detFPValue / (float)(1<<16);
			printf("detuneMultiple FD=%d keycode=%d %f (%d) %f\n", i, keycode, detuneMultiple, detuneScaleTableValue,fpValue);
		}
	}
#endif // #if 0 
	for (i = 0; i < 4; i++)
	{
		int keycode;
		for (keycode = 0; keycode < 32; keycode++)
		{
			const int detuneScaleTableValue = LJ_YM2612_detuneScaleTable[i*32+keycode];
			//This should be including baseFreqScale to put in the same units as fnumTable - so it can be additive to it
			const float detuneRate = ym2612->baseFreqScale * detuneScaleTableValue;
			// Include the sin table size in the (1/2^20)
			const float detuneRateTableValue = detuneRate * (float)(1 << (LJ_YM2612_FREQ_BITS - (20 - LJ_YM2612_SINTABLE_BITS)));
			
			int detuneTableValue = (int)(detuneRateTableValue);
			LJ_YM2612_detuneTable[i*32+keycode] = detuneTableValue;
			LJ_YM2612_detuneTable[(i+4)*32+keycode] = -detuneTableValue;
		}
	}
/*
	for (i = 3; i < 4; i++)
	{
		int keycode;
		for (keycode = 0; keycode < 32; keycode++)
		{
			const int detuneTable = LJ_YM2612_detuneTable[i*32+keycode];
			const int detuneScaleTableValue = LJ_YM2612_detuneScaleTable[i*32+keycode];
			printf("detuneTable FD=%d keycode=%d %d (%d)\n", i, keycode, detuneTable, detuneScaleTableValue);
		}
	}
*/
}

static void ym2612_clear(LJ_YM2612* const ym2612)
{
	int i;

	ym2612->dacValue = 0;
	ym2612->dacEnable = 0x0;
	ym2612->flags = 0x0;
	ym2612->baseFreqScale = 0.0f;
	ym2612->regAddress = 0x0;	
	ym2612->slotWriteAddr = 0xFF;	
	ym2612->clockRate = 0;
	ym2612->outputSampleRate = 0;

	for (i = 0; i < LJ_YM2612_NUM_PARTS; i++)
	{
		LJ_YM2612_PART* const part = &(ym2612->part[i]);
		part->id = i;
		ym2612_partClear(part);
	}
	for (i = 0; i < LJ_YM2612_NUM_PARTS; i++)
	{
		int channel;
		for (channel = 0; channel < LJ_YM2612_NUM_CHANNELS_PER_PART; channel++)
		{
			const int chan = (i * LJ_YM2612_NUM_CHANNELS_PER_PART) + channel;
			LJ_YM2612_CHANNEL* const channelPtr = &ym2612->part[i].channel[channel];
			ym2612->channels[chan] = channelPtr;
		}
	}
	for (i = 0; i < LJ_YM2612_NUM_REGISTERS; i++)
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

	LJ_YM2612_validRegisters[LJ_FREQLSB] = 1;
	LJ_YM2612_REGISTER_NAMES[LJ_FREQLSB] = "FREQ(LSB)";

	LJ_YM2612_validRegisters[LJ_BLOCK_FREQMSB] = 1;
	LJ_YM2612_REGISTER_NAMES[LJ_BLOCK_FREQMSB] = "BLOCK_FREQ(MSB)";

	LJ_YM2612_validRegisters[LJ_DETUNE_MULT] = 1;
	LJ_YM2612_REGISTER_NAMES[LJ_DETUNE_MULT] = "DETUNE_MULT";

	//LJ_YM2612_validRegisters[LJ_FEEDBACK_ALGO] = 1;
	//LJ_YM2612_REGISTER_NAMES[LJ_FEEDBACK_ALGO] = "FEEDBACK_ALGO";

	//LJ_YM2612_validRegisters[LJ_LR_AMS_PMS] = 1;
	//LJ_YM2612_REGISTER_NAMES[LJ_LR_AMS_PMS] = "LR_AMS_PMS";

	//For parameters mark all the associated registers as valid
	for (i = 0; i < LJ_YM2612_NUM_REGISTERS; i++)
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
				//0x30-0x90 = settings per channel per slot (channel = bottom 2 bits of reg, slot = reg / 4)
				int regBase = (i >> 4) << 4;
				int j;
				for (j = 0; j < 16; j++)
				{
					const char* const regBaseName = LJ_YM2612_REGISTER_NAMES[regBase];
					LJ_YM2612_validRegisters[regBase+j] = 1;
					LJ_YM2612_REGISTER_NAMES[regBase+j] = regBaseName;
				}
			}
			if ((i >= 0xA0) && (i <= 0xB6))
			{
				//0xA0-0xB0 = settings per channel (channel = bottom 2 bits of reg)
				int regBase = (i >> 2) << 2;
				int j;
				for (j = 0; j < 3; j++)
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

LJ_YM2612_RESULT ym2612_setRegister(LJ_YM2612* const ym2612, LJ_YM_UINT8 part, LJ_YM_UINT8 reg, LJ_YM_UINT8 data)
{
	LJ_YM_UINT32 parameter = 0;
	//LJ_YM_UINT32 channel = 0;
	//LJ_YM_UINT32 slot = 0;

	if (ym2612 == NULL)
	{
		fprintf(stderr, "ym2612_setRegister:ym2612 is NULL\n");
		return LJ_YM2612_ERROR;
	}

	if (part >= LJ_YM2612_NUM_PARTS)
	{
		fprintf(stderr, "ym2612_setRegister:invalid part:%d max:%d \n",part, LJ_YM2612_NUM_PARTS);
		return LJ_YM2612_ERROR;
	}

	// convert from register to parameter
	parameter = (reg & 0xF0) >> 4;

	if (LJ_YM2612_validRegisters[reg] == 0)
	{
		fprintf(stderr, "ym2612_setRegister:unknown register:0x%X part:%d data:0x%X\n", reg, part, data);
		return LJ_YM2612_ERROR;
	}

	ym2612->part[part].reg[reg] = data;
	if (ym2612->flags & LJ_YM2612_DEBUG)
	{
		printf( "ym2612:setRegister %s 0x%X\n", LJ_YM2612_REGISTER_NAMES[reg],data);
	}

	if (reg == LJ_DAC_EN)
	{
		// 0x2A DAC
		ym2612->dacEnable = 0xFFFF * ((data & 0x80) >> 7);
	}
	else if (reg == LJ_DAC)
	{
		// 0x2B DAC en = Bit 7
		ym2612->dacValue = ((LJ_YM_INT16)(data - 0x80)) << DAC_SHIFT;
	}
	else if (reg == LJ_KEY_ONOFF)
	{
		// 0x28 Slot = Bits 4-7, Channel ID = Bits 0-2, part 0 or 1 is Bit 3
		const int channel = data & 0x03;
		if (channel != 0x03)
		{
			const int slotOnOff = (data >> 4);
			const int part = (channel & 0x4) >> 2;
			LJ_YM2612_CHANNEL* const channelPtr = &ym2612->part[part].channel[channel];
			ym2612_channelKeyOnOff(channelPtr, slotOnOff);
		}
	}
	else if ((reg >= 0x30) && (reg <= 0x9F))
	{
		//0x30-0x90 = settings per channel per slot (channel = bottom 2 bits of reg, slot = reg / 4)
		const int channel = reg & 0x3;
		const int slot = (reg >> 2) & 0x3;
		const int regParameter = reg & 0xF0;
		LJ_YM2612_CHANNEL* const channelPtr = &ym2612->part[part].channel[channel];
		if (regParameter == LJ_DETUNE_MULT)
		{
			ym2612_channelSetDetuneMult(channelPtr, slot, data);
			if (ym2612->flags & LJ_YM2612_DEBUG)
			{
				printf("LJ_DETUNE_MULT part:%d channel:%d slot:%d data:0x%X\n", part, channel, slot, data);
			}
		}
	}
	else if ((reg >= 0xA0) && (reg <= 0xB6))
	{
		// 0xA0-0xB0 = settings per channel (channel = bottom 2 bits of reg)
		const int channel = reg & 0x3;
		//const int regParameter = (reg >> 2) << 2; // reg & 0xFC
		const int regParameter = reg & 0xFC;

		LJ_YM2612_CHANNEL* const channelPtr = &ym2612->part[part].channel[channel];
		if (regParameter == LJ_FREQLSB)
		{
			// 0xA0-0xA2 Frequency number LSB = Bits 0-7 (bottom 8 bits of frequency number)
			ym2612_channelSetFreqBlock(channelPtr, data);
			if (ym2612->flags & LJ_YM2612_DEBUG)
			{
				printf( "LJ_FREQLSB part:%d channel:%d data:0x%X\n", part, channel, data);
			}
		}
		else if (regParameter == LJ_BLOCK_FREQMSB)
		{
			// 0xA4-0xA6 Block = Bits 5-3, Frequency Number MSB = Bits 0-2 (top 3-bits of frequency number)
			ym2612_channelSetBlockFnumMSB(channelPtr, data);
			if (ym2612->flags & LJ_YM2612_DEBUG)
			{
				printf( "LJ_BLOCK_FREQMSB part:%d channel:%d data:0x%X\n", part, channel, data);
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

LJ_YM2612* LJ_YM2612_create(const int clockRate, const int outputSampleRate)
{
	LJ_YM2612* const ym2612 = malloc(sizeof(LJ_YM2612));
	if (ym2612 == NULL)
	{
		fprintf(stderr, "LJ_YM2612_create:malloc failed\n");
		return NULL;
	}

	ym2612_clear(ym2612);

	ym2612->clockRate = clockRate;
	ym2612->outputSampleRate = outputSampleRate;

	ym2612_makeData(ym2612);

	return ym2612;
}

LJ_YM2612_RESULT LJ_YM2612_setFlags(LJ_YM2612* const ym2612, const unsigned int flags)
{
	int part;
	if (ym2612 == NULL)
	{
		fprintf(stderr, "LJ_YM2612_setFlags:ym2612 is NULL\n");
		return LJ_YM2612_ERROR;
	}

	ym2612->flags = flags;

	for (part = 0; part < LJ_YM2612_NUM_PARTS; part++)
	{
		int channel;
		for (channel = 0; channel < LJ_YM2612_NUM_CHANNELS_PER_PART; channel++)
		{
			LJ_YM2612_CHANNEL* const channelPtr = &ym2612->part[part].channel[channel];
			channelPtr->flags = flags;
		}
	}
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
	int channel;
	int outputChannelMask = 0xFF;
	int channelMask = 0x1;

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
	if (ym2612->flags & LJ_YM2612_ONECHANNEL)
	{
		// channel starts at 0
		const int debugChannel =((ym2612->flags >> LJ_YM2612_ONECHANNEL_SHIFT) & LJ_YM2612_ONECHANNEL_MASK); 
		outputChannelMask = 1 << debugChannel;
	}

	//For each cycle
	//Loop over channels updating them, mix them, then output them into the buffer
	for (sample = 0; sample < numCycles; sample++)
	{
		LJ_YM_INT16 mixedLeft = 0;
		LJ_YM_INT16 mixedRight = 0;

		for (channel = 0; channel < LJ_YM2612_NUM_CHANNELS_TOTAL; channel++)
		{
			LJ_YM2612_CHANNEL* const channelPtr = ym2612->channels[channel];
			LJ_YM2612_SLOT* const slotPtr = &(channelPtr->slot[0]);

			const int OMEGA = slotPtr->omega;
			const int phi = ((OMEGA & ~LJ_YM2612_FREQ_MASK) >> LJ_YM2612_FREQ_BITS);

			const int scaledSin = LJ_YM2612_sinTable[phi & LJ_YM2612_SINTABLE_MASK];

			int fmLevel = scaledSin;
			fmLevel = (channelPtr->volume * fmLevel) >> LJ_YM2612_SINTABLE_SCALE_BITS;

			if (ym2612->flags & LJ_YM2612_DEBUG)
			{
				if (fmLevel != 0)
				{
					printf("Channel:%d fmLevel:%d\n", channelPtr->id, fmLevel);
				}
			}
			if ((channelMask & outputChannelMask) == 0)
			{
				fmLevel = 0;
			}

			fmLevel = LJ_YM2612_CLAMP_VOLUME(fmLevel);

			mixedLeft += fmLevel;
			mixedRight += fmLevel;

			channelMask = (channelMask << 1);
		}

		mixedLeft += dacValue;
		mixedRight += dacValue;

		mixedLeft = LJ_YM2612_CLAMP_VOLUME(mixedLeft);
		mixedRight = LJ_YM2612_CLAMP_VOLUME(mixedRight);

		outputLeft[sample] = mixedLeft;
		outputRight[sample] = mixedRight;
	}

	// Update volumes and frequency position
	for (channel = 0; channel < LJ_YM2612_NUM_CHANNELS_TOTAL; channel++)
	{
		LJ_YM2612_CHANNEL* const channelPtr = ym2612->channels[channel];
		LJ_YM2612_SLOT* const slotPtr = &(channelPtr->slot[0]);

		channelPtr->volume += channelPtr->volumeDelta;
		if (channelPtr->volume < 0)
		{
			channelPtr->volume = 0;
		}
		if (channelPtr->volume > LJ_YM2612_VOLUME_MAX)
		{
			channelPtr->volume = LJ_YM2612_VOLUME_MAX;
		}
		slotPtr->omega += slotPtr->omegaDelta;
	}

	return LJ_YM2612_OK;
}

LJ_YM2612_RESULT LJ_YM2612_write(LJ_YM2612* const ym2612, LJ_YM_UINT16 address, LJ_YM_UINT8 data)
{
	int result = LJ_YM2612_ERROR;
	const int part = (address >> 1) & 0x1;

	// PART 0: R = 0x4000, D = 0x4001
	// PART 1: R = 0x4002, D = 0x4003
	if (ym2612 == NULL)
	{
		fprintf(stderr, "LJ_YM2612_write:ym2612 is NULL\n");
		return LJ_YM2612_ERROR;
	}

	if ((address == 0x4000) || (address == 0x4002))
	{
		ym2612->regAddress = data;
		ym2612->slotWriteAddr = part;
		return LJ_YM2612_OK;
	}
	else if ((address == 0x4001) || (address == 0x4003))
	{
		if (ym2612->slotWriteAddr == part)
		{
			LJ_YM_UINT8 reg = ym2612->regAddress;
			result = ym2612_setRegister(ym2612, part, reg, data);
			//Hmmmm - how does the real chip work
			ym2612->slotWriteAddr = 0xFF;
			return result;
		}
		return LJ_YM2612_OK;
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

