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
typedef struct LJ_YM2612_CHANNEL2_SLOT_DATA LJ_YM2612_CHANNEL2_SLOT_DATA;

#define LJ_YM2612_NUM_REGISTERS (0xFF)
#define LJ_YM2612_NUM_PARTS (2)
#define LJ_YM2612_NUM_CHANNELS_PER_PART (3)
#define LJ_YM2612_NUM_CHANNELS_TOTAL (LJ_YM2612_NUM_CHANNELS_PER_PART * LJ_YM2612_NUM_PARTS)
#define LJ_YM2612_NUM_SLOTS_PER_CHANNEL (4)

enum LJ_YM2612_REGISTERS {
		LJ_LFO_EN_LFO_FREQ = 0x22,
		LJ_CH2_MODE_TIMER_CONTROL = 0x27,
		LJ_KEY_ONOFF = 0x28,
		LJ_DAC = 0x2A,
		LJ_DAC_EN = 0x2B,
		LJ_DETUNE_MULT = 0x30,
		LJ_TOTAL_LEVEL = 0x40,
		LJ_FREQLSB = 0xA0,
		LJ_BLOCK_FREQMSB = 0xA4,
		LJ_CH2_FREQLSB = 0xA8,
		LJ_CH2_BLOCK_FREQMSB = 0xAC,
		LJ_FEEDBACK_ALGO = 0xB0,
		LJ_LR_AMS_PMS = 0xB4,
};

static const char* LJ_YM2612_REGISTER_NAMES[LJ_YM2612_NUM_REGISTERS];
static LJ_YM_UINT8 LJ_YM2612_validRegisters[LJ_YM2612_NUM_REGISTERS];

//Global fixed point scaling for outputs of things
#define LJ_YM2612_GLOBAL_SCALE_BITS (16)

//FREQ scale = 16.16 - used global scale to keep things consistent
#define LJ_YM2612_FREQ_BITS (LJ_YM2612_GLOBAL_SCALE_BITS)
#define LJ_YM2612_FREQ_MASK ((1 << LJ_YM2612_FREQ_BITS) - 1)

//Volume scale = xx.yy (-1->1) 
#define LJ_YM2612_VOLUME_SCALE_BITS (13)

// Number of bits to use for scaling output e.g. choose an output of 1.0 -> 13-bits (8192)
#define LJ_YM2612_OUTPUT_VOLUME_BITS (13)
#define LJ_YM2612_OUTPUT_SCALE (LJ_YM2612_VOLUME_SCALE_BITS - LJ_YM2612_OUTPUT_VOLUME_BITS)

// Max volume is -1 -> +1
#define LJ_YM2612_VOLUME_MAX (1<<LJ_YM2612_VOLUME_SCALE_BITS)

//Sin output scale = xx.yy (-1->1)
#define LJ_YM2612_SIN_SCALE_BITS (LJ_YM2612_GLOBAL_SCALE_BITS)

// TL table scale = 16.16 (0->1) - matches volume scale
#define LJ_YM2612_TL_SCALE_BITS (LJ_YM2612_VOLUME_SCALE_BITS)

// DAC is in 7-bit scale (-128->128) so this converts it to volume_scale bits
#define LJ_YM2612_DAC_SHIFT (LJ_YM2612_VOLUME_SCALE_BITS - 7)

//FNUM = 11-bit table
#define LJ_YM2612_FNUM_TABLE_BITS (11)
#define LJ_YM2612_FNUM_TABLE_NUM_ENTRIES (1 << LJ_YM2612_FNUM_TABLE_BITS)
static int LJ_YM2612_fnumTable[LJ_YM2612_FNUM_TABLE_NUM_ENTRIES];

//SIN table = 10-bit table but stored in LJ_YM2612_SIN_SCALE_BITS format
#define LJ_YM2612_SIN_TABLE_BITS (10)
#define LJ_YM2612_SIN_TABLE_NUM_ENTRIES (1 << LJ_YM2612_SIN_TABLE_BITS)
#define LJ_YM2612_SIN_TABLE_MASK ((1 << LJ_YM2612_SIN_TABLE_BITS) - 1)
static int LJ_YM2612_sinTable[LJ_YM2612_SIN_TABLE_NUM_ENTRIES];

// Right shift for delta phi = LJ_YM2612_VOLUME_SCALE_BITS - LJ_YM2612_SIN_TABLE_BITS - 2
// >> LJ_YM2612_VOLUME_SCALE_BITS = to remove the volume scale in the output from the sin table
// << LJ_YM2612_SIN_TABLE_BITS = to put the output back into the range of the sin table
// << 2 = *4 to convert FM output of -1 -> +1 => -4 -> +4 => -PI (and a bit) -> +PI (and a bit)
#define LJ_YM2612_DELTA_PHI_SCALE (LJ_YM2612_VOLUME_SCALE_BITS - LJ_YM2612_SIN_TABLE_BITS -2)

//DETUNE table = this are integer freq shifts in the frequency integer scale
// In the docs (YM2608) : the base scale is (0.052982) this equates to: (8*1000*1000/(1*1024*1024))/144
// For YM2612 (Genesis) it would equate to: (7670453/(1*1024*1024)/144 => 0.050799
// 144 is the number clock cycles the chip takes to make 1 sample: 
// 144 = 6 channels x 4 operators x 8 cycles per operator 
// 144 = 6 channels x 24 cycles per channel
// 144 = also the prescaler: YM2612 is 1/6 : one sample for each 24 FM clocks
#define LJ_YM2612_NUM_KEYCODES (1<<5)
#define LJ_YM2612_NUM_FDS (4)
static const LJ_YM_UINT8 LJ_YM2612_detuneScaleTable[LJ_YM2612_NUM_FDS*LJ_YM2612_NUM_KEYCODES] = {
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
static int LJ_YM2612_detuneTable[2*LJ_YM2612_NUM_FDS*LJ_YM2612_NUM_KEYCODES];

// fnum -> keycode table for computing N3 & N4
// N4 = Fnum Bit 11 
// N3 = Fnum Bit 11 * (Bit 10 | Bit 9 | Bit 8) | !Bit 11 * Bit 10 * Bit 9 * Bit 8
// Table = (N4 << 1) | N3
static const LJ_YM_UINT8 LJ_YM2612_fnumKeycodeTable[16] = {
0, 0, 0, 0, 0, 0, 0, 1,
2, 3, 3, 3, 3, 3, 3, 3,
};

// TL - table (7-bits): 0->0x7F (16.16): output = 2^(-TL/8) : 16.16
#define LJ_YM2612_TL_TABLE_NUM_ENTRIES (128)
static int LJ_YM2612_tlTable[LJ_YM2612_TL_TABLE_NUM_ENTRIES];

// The slots referenced in the registers are not 0,1,2,3 *sigh*
static int LJ_YM2612_slotTable[LJ_YM2612_NUM_SLOTS_PER_CHANNEL] = { 0, 2, 1, 3 };

struct LJ_YM2612_SLOT
{
	int omega;
	int omegaDelta;
	int totalLevel;

	int volume;
	int volumeDelta;

	// Algorithm support
	int fmInputDelta;
	int* modulationOutput[3];
	int carrierOutputMask;
	
	LJ_YM_UINT8 omegaDirty;

	LJ_YM_UINT8 detune;
	LJ_YM_UINT8 multiple;

	LJ_YM_UINT8 keyOn;

	LJ_YM_UINT8 id;
};

struct LJ_YM2612_CHANNEL
{
	LJ_YM2612_SLOT slot[LJ_YM2612_NUM_SLOTS_PER_CHANNEL];

	int debugFlags;

	int slot0Output[2];

	int feedback;

	LJ_YM_UINT32 left;
	LJ_YM_UINT32 right;

	int fnum;
	int block;

	LJ_YM_UINT8 omegaDirty;
	LJ_YM_UINT8 block_fnumMSB;
	LJ_YM_UINT8 keycode;

	LJ_YM_UINT8 algorithm;

	LJ_YM_UINT8 ams;
	LJ_YM_UINT8 pms;

	LJ_YM_UINT8 id;
};

struct LJ_YM2612_CHANNEL2_SLOT_DATA
{
	int fnum;
	int block;

	LJ_YM_UINT8 block_fnumMSB;
	LJ_YM_UINT8 keycode;
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
	LJ_YM2612_CHANNEL2_SLOT_DATA channel2slotData[LJ_YM2612_NUM_SLOTS_PER_CHANNEL-1];
	LJ_YM2612_CHANNEL* channels[LJ_YM2612_NUM_CHANNELS_TOTAL];

	float baseFreqScale;

	LJ_YM_UINT32 clockRate;
	LJ_YM_UINT32 outputSampleRate;

	LJ_YM_UINT32 debugFlags;

	int dacValue;
	LJ_YM_UINT32 dacEnable;

	LJ_YM_UINT8 regAddress;
	LJ_YM_UINT8 slotWriteAddr;	

	LJ_YM_UINT8 ch2Mode;
	LJ_YM_UINT8 lfoEnable;
	LJ_YM_UINT8 lfoFreq;
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

static int ym2612_computeDetuneDelta(const int detune, const int keycode, const int debugFlags)
{
	const int detuneDelta = LJ_YM2612_detuneTable[detune*LJ_YM2612_NUM_KEYCODES+keycode];

	return detuneDelta;
}

static int ym2612_computeOmegaDelta(const int fnum, const int block, const int multiple, const int detune, const int keycode, 
																		const int debugFlags)
{
	// F * 2^(B-1)
	// Could multiply the fnumTable up by (1<<6) then change this to fnumTable >> (7-B)
	int omegaDelta = (LJ_YM2612_fnumTable[fnum] << block) >> 1;

	const int detuneDelta = ym2612_computeDetuneDelta(detune, keycode, debugFlags);
	omegaDelta += detuneDelta;

	if (omegaDelta < 0)
	{
		//Wrap around
		//omegaDelta += FNUM_MAX;
		omegaDelta += 1024;
	}

	// /2 because multiple is stored as x2 of its value
	omegaDelta = (omegaDelta * multiple) >> 1;

	return omegaDelta;
}

static void ym2612_slotComputeOmegaDelta(LJ_YM2612_SLOT* const slotPtr, const int fnum, const int block, const int keycode, 
																				 const int debugFlags)
{
	const int multiple = slotPtr->multiple;
	const int detune = slotPtr->detune;

	const int omegaDelta = ym2612_computeOmegaDelta(fnum, block, multiple, detune, keycode, debugFlags);

	slotPtr->omegaDelta = omegaDelta;

	if (debugFlags & LJ_YM2612_DEBUG)
	{
		printf("Slot:%d omegaDelta %d fnum:%d block:%d multiple:%3.1f detune:%d keycode:%d\n", 
					 slotPtr->id, omegaDelta, fnum, block, multiple/2.0f, detune, keycode);
	}

}

static void ym2612_channelSetConnections(LJ_YM2612_CHANNEL* const channelPtr)
{
	const int algorithm = channelPtr->algorithm;
	const int FULL_OUTPUT_MASK = 0xFFFFFFFF;
	if (algorithm == 0x0)
	{
		/*
			Each slot outputs to the next FM, only slot 3 has carrier output
			[0] --> [1] --> [2] --> [3] --------------->
		*/
		channelPtr->slot[0].carrierOutputMask = 0;
		channelPtr->slot[0].modulationOutput[0] = &channelPtr->slot[1].fmInputDelta;
		channelPtr->slot[0].modulationOutput[1] = NULL;
		channelPtr->slot[0].modulationOutput[2] = NULL;

		channelPtr->slot[1].carrierOutputMask = 0;
		channelPtr->slot[1].modulationOutput[0] = &channelPtr->slot[2].fmInputDelta;
		channelPtr->slot[1].modulationOutput[1] = NULL;
		channelPtr->slot[1].modulationOutput[2] = NULL;

		channelPtr->slot[2].carrierOutputMask = 0;
		channelPtr->slot[2].modulationOutput[0] = &channelPtr->slot[3].fmInputDelta;
		channelPtr->slot[2].modulationOutput[1] = NULL;
		channelPtr->slot[2].modulationOutput[2] = NULL;

		channelPtr->slot[3].carrierOutputMask = FULL_OUTPUT_MASK;
		channelPtr->slot[3].modulationOutput[0] = NULL;
		channelPtr->slot[3].modulationOutput[1] = NULL;
		channelPtr->slot[3].modulationOutput[2] = NULL;
	}
	else if (algorithm == 0x1)
	{
		/*
			slot 0 & slot 1 both output to slot 2, slot 2 outputs to slot 3, only slot 3 has carrier output
				[0] \
				[1] --> [2] --> [3] ------->
		*/
		channelPtr->slot[0].carrierOutputMask = 0;
		channelPtr->slot[0].modulationOutput[0] = &channelPtr->slot[2].fmInputDelta;
		channelPtr->slot[0].modulationOutput[1] = NULL;
		channelPtr->slot[0].modulationOutput[2] = NULL;

		channelPtr->slot[1].carrierOutputMask = 0;
		channelPtr->slot[1].modulationOutput[0] = &channelPtr->slot[2].fmInputDelta;
		channelPtr->slot[1].modulationOutput[1] = NULL;
		channelPtr->slot[1].modulationOutput[2] = NULL;

		channelPtr->slot[2].carrierOutputMask = 0;
		channelPtr->slot[2].modulationOutput[0] = &channelPtr->slot[3].fmInputDelta;
		channelPtr->slot[2].modulationOutput[1] = NULL;
		channelPtr->slot[2].modulationOutput[2] = NULL;

		channelPtr->slot[3].carrierOutputMask = FULL_OUTPUT_MASK;
		channelPtr->slot[3].modulationOutput[0] = NULL;
		channelPtr->slot[3].modulationOutput[1] = NULL;
		channelPtr->slot[3].modulationOutput[2] = NULL;
	}
	else if (algorithm == 0x2)
	{
		/*
			slot 0 & slot 2 both output to slot 3, slot 1 outputs to slot 2, only slot 3 has carrier output
			[0] --------\
			[1] --> [2] --> [3] ------->
		*/
		channelPtr->slot[0].carrierOutputMask = 0;
		channelPtr->slot[0].modulationOutput[0] = &channelPtr->slot[3].fmInputDelta;
		channelPtr->slot[0].modulationOutput[1] = NULL;
		channelPtr->slot[0].modulationOutput[2] = NULL;

		channelPtr->slot[1].carrierOutputMask = 0;
		channelPtr->slot[1].modulationOutput[0] = &channelPtr->slot[2].fmInputDelta;
		channelPtr->slot[1].modulationOutput[1] = NULL;
		channelPtr->slot[1].modulationOutput[2] = NULL;

		channelPtr->slot[2].carrierOutputMask = 0;
		channelPtr->slot[2].modulationOutput[0] = &channelPtr->slot[3].fmInputDelta;
		channelPtr->slot[2].modulationOutput[1] = NULL;
		channelPtr->slot[2].modulationOutput[2] = NULL;

		channelPtr->slot[3].carrierOutputMask = FULL_OUTPUT_MASK;
		channelPtr->slot[3].modulationOutput[0] = NULL;
		channelPtr->slot[3].modulationOutput[1] = NULL;
		channelPtr->slot[3].modulationOutput[2] = NULL;
	}
	else if (algorithm == 0x3)
	{
		/*
			slot 0 outputs to slot 1, slot 1 & slot 2 output to slot 3, only slot 3 has carrier output
			[0] --> [1] --> [3] ------->
			[2] --------/
		*/
		channelPtr->slot[0].carrierOutputMask = 0;
		channelPtr->slot[0].modulationOutput[0] = &channelPtr->slot[1].fmInputDelta;
		channelPtr->slot[0].modulationOutput[1] = NULL;
		channelPtr->slot[0].modulationOutput[2] = NULL;

		channelPtr->slot[1].carrierOutputMask = 0;
		channelPtr->slot[1].modulationOutput[0] = &channelPtr->slot[3].fmInputDelta;
		channelPtr->slot[1].modulationOutput[1] = NULL;
		channelPtr->slot[1].modulationOutput[2] = NULL;

		channelPtr->slot[2].carrierOutputMask = 0;
		channelPtr->slot[2].modulationOutput[0] = &channelPtr->slot[3].fmInputDelta;
		channelPtr->slot[2].modulationOutput[1] = NULL;
		channelPtr->slot[2].modulationOutput[2] = NULL;

		channelPtr->slot[3].carrierOutputMask = FULL_OUTPUT_MASK;
		channelPtr->slot[3].modulationOutput[0] = NULL;
		channelPtr->slot[3].modulationOutput[1] = NULL;
		channelPtr->slot[3].modulationOutput[2] = NULL;
	}
	else if (algorithm == 0x4)
	{
		/*
			slot 0 outputs to slot 1, slot 2 outputs to slot 3, slot 1 and slot 3 have carrier output
		    [0] --> [1] --------------->
        [2] --> [3] --/
		*/
		channelPtr->slot[0].carrierOutputMask = 0;
		channelPtr->slot[0].modulationOutput[0] = &channelPtr->slot[1].fmInputDelta;
		channelPtr->slot[0].modulationOutput[1] = NULL;
		channelPtr->slot[0].modulationOutput[2] = NULL;

		channelPtr->slot[1].carrierOutputMask = FULL_OUTPUT_MASK;
		channelPtr->slot[1].modulationOutput[0] = NULL;
		channelPtr->slot[1].modulationOutput[1] = NULL;
		channelPtr->slot[1].modulationOutput[2] = NULL;

		channelPtr->slot[2].carrierOutputMask = 0;
		channelPtr->slot[2].modulationOutput[0] = &channelPtr->slot[3].fmInputDelta;
		channelPtr->slot[2].modulationOutput[1] = NULL;
		channelPtr->slot[2].modulationOutput[2] = NULL;

		channelPtr->slot[3].carrierOutputMask = FULL_OUTPUT_MASK;
		channelPtr->slot[3].modulationOutput[0] = NULL;
		channelPtr->slot[3].modulationOutput[1] = NULL;
		channelPtr->slot[3].modulationOutput[2] = NULL;
	}
	else if (algorithm == 0x5)
	{
		/*
			slot 0 outputs to slot 1 and slot 2 and slot 3, only slot 3 has carrier output
						/>	[1] --\
			[0] -->		[2] --------------->
						\>	[3] --/
		*/
		channelPtr->slot[0].carrierOutputMask = 0;
		channelPtr->slot[0].modulationOutput[0] = &channelPtr->slot[1].fmInputDelta;
		channelPtr->slot[0].modulationOutput[1] = &channelPtr->slot[2].fmInputDelta;
		channelPtr->slot[0].modulationOutput[2] = &channelPtr->slot[3].fmInputDelta;

		channelPtr->slot[1].carrierOutputMask = FULL_OUTPUT_MASK;
		channelPtr->slot[1].modulationOutput[0] = NULL;
		channelPtr->slot[1].modulationOutput[1] = NULL;
		channelPtr->slot[1].modulationOutput[2] = NULL;

		channelPtr->slot[2].carrierOutputMask = FULL_OUTPUT_MASK;
		channelPtr->slot[2].modulationOutput[0] = NULL;
		channelPtr->slot[2].modulationOutput[1] = NULL;
		channelPtr->slot[2].modulationOutput[2] = NULL;

		channelPtr->slot[3].carrierOutputMask = FULL_OUTPUT_MASK;
		channelPtr->slot[3].modulationOutput[0] = NULL;
		channelPtr->slot[3].modulationOutput[1] = NULL;
		channelPtr->slot[3].modulationOutput[2] = NULL;
	}
	else if (algorithm == 0x6)
	{
		/*
			slot 0 outputs to slot 1, slot 1 and slot 2 and slot 3 have carrier output
			[0] --> [1] --\
 							[2] --------------->
							[3] --/
		*/
		channelPtr->slot[0].carrierOutputMask = 0;
		channelPtr->slot[0].modulationOutput[0] = &channelPtr->slot[1].fmInputDelta;
		channelPtr->slot[0].modulationOutput[1] = NULL;
		channelPtr->slot[0].modulationOutput[2] = NULL;

		channelPtr->slot[1].carrierOutputMask = FULL_OUTPUT_MASK;
		channelPtr->slot[1].modulationOutput[0] = NULL;
		channelPtr->slot[1].modulationOutput[1] = NULL;
		channelPtr->slot[1].modulationOutput[2] = NULL;

		channelPtr->slot[2].carrierOutputMask = FULL_OUTPUT_MASK;
		channelPtr->slot[2].modulationOutput[0] = NULL;
		channelPtr->slot[2].modulationOutput[1] = NULL;
		channelPtr->slot[2].modulationOutput[2] = NULL;

		channelPtr->slot[3].carrierOutputMask = FULL_OUTPUT_MASK;
		channelPtr->slot[3].modulationOutput[0] = NULL;
		channelPtr->slot[3].modulationOutput[1] = NULL;
		channelPtr->slot[3].modulationOutput[2] = NULL;
	}
	else if (algorithm == 0x7)
	{
		/*
		 Each slot outputs to carrier, no FM
			[0] --\
    	[1] ---\
    	[2] --------------->
    	[3] ---/
		*/
		channelPtr->slot[0].carrierOutputMask = FULL_OUTPUT_MASK;
		channelPtr->slot[0].modulationOutput[0] = NULL;
		channelPtr->slot[0].modulationOutput[1] = NULL;
		channelPtr->slot[0].modulationOutput[2] = NULL;

		channelPtr->slot[1].carrierOutputMask = FULL_OUTPUT_MASK;
		channelPtr->slot[1].modulationOutput[0] = NULL;
		channelPtr->slot[1].modulationOutput[1] = NULL;
		channelPtr->slot[1].modulationOutput[2] = NULL;

		channelPtr->slot[2].carrierOutputMask = FULL_OUTPUT_MASK;
		channelPtr->slot[2].modulationOutput[0] = NULL;
		channelPtr->slot[2].modulationOutput[1] = NULL;
		channelPtr->slot[2].modulationOutput[2] = NULL;

		channelPtr->slot[3].carrierOutputMask = FULL_OUTPUT_MASK;
		channelPtr->slot[3].modulationOutput[0] = NULL;
		channelPtr->slot[3].modulationOutput[1] = NULL;
		channelPtr->slot[3].modulationOutput[2] = NULL;
	}
}

static void ym2612_channelSetLeftRightAmsPms(LJ_YM2612_CHANNEL* const channelPtr, const LJ_YM_UINT8 data)
{
		// 0xB4-0xB6 Feedback = Left= Bit 7, Right= Bit 6, AMS = Bits 3-5, PMS = Bits 0-1
	const int left = (data >> 7) & 0x1;
	const int right = (data >> 6) & 0x1;
	const int AMS = (data >> 3) & 0x7;
	const int PMS = (data >> 0) & 0x3;

	channelPtr->left = left *	~0;
	channelPtr->right = right * ~0;
	channelPtr->ams = AMS;
	channelPtr->pms = PMS;

	if (channelPtr->debugFlags & LJ_YM2612_DEBUG)
	{
		printf("SetLeftRightAmsPms channel:%d left:%d right:%d AMS:%d PMS:%d\n", channelPtr->id, left, right, AMS, PMS);
	}

	//Update the connections for this channel
	ym2612_channelSetConnections(channelPtr);
}

static void ym2612_channelSetFeedbackAlgorithm(LJ_YM2612_CHANNEL* const channelPtr, const LJ_YM_UINT8 data)
{
	// 0xB0-0xB2 Feedback = Bits 5-3, Algorithm = Bits 0-2
	const int algorithm = (data >> 0) & 0x7;
	const int feedback = (data >> 3) & 0x7;

	channelPtr->feedback = feedback;
	channelPtr->algorithm = algorithm;

	if (channelPtr->debugFlags & LJ_YM2612_DEBUG)
	{
		printf("SetFeedbackAlgorithm channel:%d feedback:%d algorithm:%d\n", channelPtr->id, feedback, algorithm);
	}

	//Update the connections for this channel
	ym2612_channelSetConnections(channelPtr);
}

static void ym2612_channelSetTotalLevel(LJ_YM2612_CHANNEL* const channelPtr, const int slot, const LJ_YM_UINT8 totalLevel)
{
	LJ_YM2612_SLOT* const slotPtr = &(channelPtr->slot[slot]);

	// Total Level = Bits 0-6
	const int TL = (totalLevel >> 0) & 0x7F;
	const int TLscale = LJ_YM2612_tlTable[TL];
	slotPtr->totalLevel = TLscale;

	if (channelPtr->debugFlags & LJ_YM2612_DEBUG)
	{
		printf("SetTotalLevel channel:%d slot:%d TL:%d scale:%f TLscale:%d\n", channelPtr->id, 
					slot, TL, ((float)(TLscale)/(float)(1<<LJ_YM2612_TL_SCALE_BITS)), TLscale);
	}
}

static void ym2612_channelSetDetuneMult(LJ_YM2612_CHANNEL* const channelPtr, const int slot, const LJ_YM_UINT8 detuneMult)
{
	LJ_YM2612_SLOT* const slotPtr = &(channelPtr->slot[slot]);

	// Detune = Bits 4-6, Multiple = Bits 0-3
	const int detune = (detuneMult >> 4) & 0x7;
	const int multiple = (detuneMult >> 0) & 0xF;

	slotPtr->detune = detune;
	//multiple = xN except N=0 then it is x1/2 store it as x2
	slotPtr->multiple = multiple ? (multiple << 1) : 1;

	if (channelPtr->debugFlags & LJ_YM2612_DEBUG)
	{
		printf("SetDetuneMult channel:%d slot:%d detune:%d mult:%d\n", channelPtr->id, slot, detune, multiple);
	}
}

static void ym2612_computeBlockFnumKeyCode(int* const blockPtr, int* fnumPtr, int* keycodePtr, const int block_fnumMSB, const int fnumLSB)
{
	// Block = Bits 3-5, Frequency Number MSB = Bits 0-2 (top 3-bits of frequency number)
	const int block = (block_fnumMSB >> 3) & 0x7;
	const int fnumMSB = (block_fnumMSB >> 0) & 0x7;
	const int fnum = (fnumMSB << 8) + (fnumLSB & 0xFF);
	// keycode = (block << 2) | (N4 << 1) | (N3 << 0)
	// N4 = Fnum Bit 11 
	// N3 = Fnum Bit 11 * (Bit 10 | Bit 9 | Bit 8) | !Bit 11 * Bit 10 * Bit 9 * Bit 8
	const int keycode = (block << 2) | LJ_YM2612_fnumKeycodeTable[fnum>>7];

	*blockPtr = block;
	*fnumPtr = fnum;
	*keycodePtr = keycode;
}

static void ym2612_channelSetFreqBlock(LJ_YM2612_CHANNEL* const channelPtr, const LJ_YM_UINT8 fnumLSB)
{
	const LJ_YM_UINT8 block_fnumMSB = channelPtr->block_fnumMSB;

	int block;
	int fnum;
	int keycode;
	ym2612_computeBlockFnumKeyCode(&block, &fnum, &keycode, block_fnumMSB, fnumLSB);

	channelPtr->block = block;
	channelPtr->fnum = fnum;
	channelPtr->keycode = keycode;
	channelPtr->omegaDirty = 1;

	if (channelPtr->debugFlags & LJ_YM2612_DEBUG)
	{
		printf("SetFreqBlock channel:%d block:%d fnum:%d 0x%X keycode:%d block_fnumMSB:0x%X fnumLSB:0x%X\n", 
					 channelPtr->id, block, fnum, fnum, keycode, block_fnumMSB, fnumLSB);
	}
}

static void ym2612_channelSetBlockFnumMSB(LJ_YM2612_CHANNEL* const channelPtr, const LJ_YM_UINT8 data)
{
	channelPtr->block_fnumMSB = data;
}

static void ym2612_channelKeyOnOff(LJ_YM2612_CHANNEL* const channelPtr, const LJ_YM_UINT8 slotOnOff)
{
	int i;
	int slotMask = 0x1;
	//Set the start of wave
	for (i = 0; i < LJ_YM2612_NUM_SLOTS_PER_CHANNEL; i++)
	{
		const int slot = i;
		LJ_YM2612_SLOT* const slotPtr = &(channelPtr->slot[slot]);

		if (slotOnOff & slotMask)
		{
			if (slotPtr->keyOn == 0)
			{
				slotPtr->omega = 0;
				slotPtr->volume = 0;
				slotPtr->volumeDelta = LJ_YM2612_VOLUME_MAX;
				slotPtr->fmInputDelta = 0;
				slotPtr->keyOn = 1;
			}
		}
		else
		{
			if (slotPtr->keyOn == 1)
			{
				slotPtr->volumeDelta = -LJ_YM2612_VOLUME_MAX;
				slotPtr->keyOn = 0;
			}
		}
		slotMask = slotMask << 1;
	}
}

static void ym2612_slotClear(LJ_YM2612_SLOT* const slotPtr)
{
	slotPtr->volume = 0;
	slotPtr->volumeDelta = 0;
	slotPtr->detune = 0;
	slotPtr->multiple = 0;
	slotPtr->omega = 0;
	slotPtr->omegaDelta = 0;

	slotPtr->fmInputDelta = 0;
	slotPtr->modulationOutput[0] = NULL;
	slotPtr->modulationOutput[1] = NULL;
	slotPtr->modulationOutput[2] = NULL;

	slotPtr->carrierOutputMask = 0x0;

	slotPtr->keyOn = 0;

	slotPtr->omegaDirty = 1;

	slotPtr->id = 0;
}

static void ym2612_channelClear(LJ_YM2612_CHANNEL* const channelPtr)
{
	int i;
	for (i = 0; i < LJ_YM2612_NUM_SLOTS_PER_CHANNEL; i++)
	{
		LJ_YM2612_SLOT* const slotPtr = &(channelPtr->slot[i]);
		ym2612_slotClear(slotPtr);
		slotPtr->id = i;
	}
	channelPtr->debugFlags = 0;
	channelPtr->feedback = 0;
	channelPtr->algorithm = 0;

	channelPtr->slot0Output[0] = 0;
	channelPtr->slot0Output[1] = 0;

	channelPtr->omegaDirty = 1;
	channelPtr->fnum = 0;
	channelPtr->block = 0;
	channelPtr->block_fnumMSB = 0;

	channelPtr->left = 0;
	channelPtr->right = 0;
	channelPtr->ams = 0;
	channelPtr->pms = 0;

	channelPtr->id = 0;
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
		LJ_YM2612_CHANNEL* const channelPtr = &(part->channel[i]);
		ym2612_channelClear(channelPtr);
		channelPtr->id = part->id*LJ_YM2612_NUM_CHANNELS_PER_PART+i;
	}
}

static void ym2612_SetChannel2FreqBlock(LJ_YM2612* const ym2612, const LJ_YM_UINT8 slot, const LJ_YM_UINT8 fnumLSB)
{
	LJ_YM2612_CHANNEL* const channel2Ptr = &(ym2612->part[0].channel[2]);

	const LJ_YM_UINT8 block_fnumMSB = ym2612->channel2slotData[slot].block_fnumMSB;

	int block;
	int fnum;
	int keycode;
	ym2612_computeBlockFnumKeyCode(&block, &fnum, &keycode, block_fnumMSB, fnumLSB);

	ym2612->channel2slotData[slot].block = block;
	ym2612->channel2slotData[slot].fnum = fnum;
	ym2612->channel2slotData[slot].keycode = keycode;
	channel2Ptr->slot[slot].omegaDirty = 1;

	if (ym2612->debugFlags & LJ_YM2612_DEBUG)
	{
		printf("SetCh2FreqBlock channel:%d slot:%d block:%d fnum:%d 0x%X keycode:%d block_fnumMSB:0x%X fnumLSB:0x%X\n", 
					 channel2Ptr->id, slot, block, fnum, fnum, keycode, block_fnumMSB, fnumLSB);
	}
}

static void ym2612_SetChannel2BlockFnumMSB(LJ_YM2612* const ym2612, const LJ_YM_UINT8 slot, const LJ_YM_UINT8 data)
{
	ym2612->channel2slotData[slot].block_fnumMSB = data;
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

	for (i = 0; i < LJ_YM2612_FNUM_TABLE_NUM_ENTRIES; i++)
	{
		float freq = ym2612->baseFreqScale * i;
		// Include the sin table size in the (1/2^20)
		freq = freq * (float)(1 << (LJ_YM2612_FREQ_BITS - (20 - LJ_YM2612_SIN_TABLE_BITS)));

		int intFreq = (int)(freq);
		LJ_YM2612_fnumTable[i] = intFreq;
	}

	const float omegaScale = (2.0f * M_PI / LJ_YM2612_SIN_TABLE_NUM_ENTRIES);
	for (i = 0; i < LJ_YM2612_SIN_TABLE_NUM_ENTRIES; i++)
	{
		const float omega = omegaScale * i;
		const float sinValue = sinf(omega);
		const int scaledSin = (int)(sinValue * (float)(1 << LJ_YM2612_SIN_SCALE_BITS));
		LJ_YM2612_sinTable[i] = scaledSin;
	}

	for (i = 0; i < LJ_YM2612_TL_TABLE_NUM_ENTRIES; i++)
	{
		// From the docs each step is -0.75dB = x0.9172759 = 2^(-1/8)
		const float value = -i * ( 1.0f / 8.0f );
		const float tlValue = powf(2.0f, value);
		const int scaledTL = (int)(tlValue * (float)(1 << LJ_YM2612_TL_SCALE_BITS));
		LJ_YM2612_tlTable[i] = scaledTL;
	}

#if 0 
	//DT1 = -3 -> 3
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
	for (i = 0; i < LJ_YM2612_NUM_FDS; i++)
	{
		int keycode;
		for (keycode = 0; keycode < LJ_YM2612_NUM_KEYCODES; keycode++)
		{
			const int detuneScaleTableValue = LJ_YM2612_detuneScaleTable[i*LJ_YM2612_NUM_KEYCODES+keycode];
			//This should be including baseFreqScale to put in the same units as fnumTable - so it can be additive to it
			const float detuneRate = ym2612->baseFreqScale * detuneScaleTableValue;
			// Include the sin table size in the (1/2^20)
			const float detuneRateTableValue = detuneRate * (float)(1 << (LJ_YM2612_FREQ_BITS - (20 - LJ_YM2612_SIN_TABLE_BITS)));
			
			int detuneTableValue = (int)(detuneRateTableValue);
			LJ_YM2612_detuneTable[i*LJ_YM2612_NUM_KEYCODES+keycode] = detuneTableValue;
			LJ_YM2612_detuneTable[(i+LJ_YM2612_NUM_FDS)*LJ_YM2612_NUM_KEYCODES+keycode] = -detuneTableValue;
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
	ym2612->debugFlags = 0x0;
	ym2612->baseFreqScale = 0.0f;
	ym2612->regAddress = 0x0;	
	ym2612->slotWriteAddr = 0xFF;	
	ym2612->clockRate = 0;
	ym2612->outputSampleRate = 0;
	ym2612->ch2Mode = 0;
	ym2612->lfoEnable = 0;
	ym2612->lfoFreq = 0;

	for (i = 0; i < LJ_YM2612_NUM_PARTS; i++)
	{
		LJ_YM2612_PART* const part = &(ym2612->part[i]);
		ym2612_partClear(part);
		part->id = i;
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
	for (i = 0; i < LJ_YM2612_NUM_SLOTS_PER_CHANNEL-1; i++)
	{
		ym2612->channel2slotData[i].fnum = 0;
		ym2612->channel2slotData[i].block = 0;
		ym2612->channel2slotData[i].block_fnumMSB = 0;
		ym2612->channel2slotData[i].keycode = 0;
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

	LJ_YM2612_validRegisters[LJ_CH2_FREQLSB] = 1;
	LJ_YM2612_REGISTER_NAMES[LJ_CH2_FREQLSB] = "CH2_FREQ(LSB)";

	LJ_YM2612_validRegisters[LJ_CH2_BLOCK_FREQMSB] = 1;
	LJ_YM2612_REGISTER_NAMES[LJ_CH2_BLOCK_FREQMSB] = "CH2_BLOCK_FREQ(MSB)";

	LJ_YM2612_validRegisters[LJ_DETUNE_MULT] = 1;
	LJ_YM2612_REGISTER_NAMES[LJ_DETUNE_MULT] = "DETUNE_MULT";

	LJ_YM2612_validRegisters[LJ_LFO_EN_LFO_FREQ] = 1;
	LJ_YM2612_REGISTER_NAMES[LJ_LFO_EN_LFO_FREQ] = "LFO_END_LFO_FREQ";

	LJ_YM2612_validRegisters[LJ_CH2_MODE_TIMER_CONTROL] = 1;
	LJ_YM2612_REGISTER_NAMES[LJ_CH2_MODE_TIMER_CONTROL] = "CH2_MODE_TIMER_CONTROL";

	LJ_YM2612_validRegisters[LJ_TOTAL_LEVEL] = 1;
	LJ_YM2612_REGISTER_NAMES[LJ_TOTAL_LEVEL] = "TOTAL_LEVEL";

	LJ_YM2612_validRegisters[LJ_FEEDBACK_ALGO] = 1;
	LJ_YM2612_REGISTER_NAMES[LJ_FEEDBACK_ALGO] = "FEEDBACK_ALGO";

	LJ_YM2612_validRegisters[LJ_LR_AMS_PMS] = 1;
	LJ_YM2612_REGISTER_NAMES[LJ_LR_AMS_PMS] = "LR_AMS_PMS";

	//For parameters mark all the associated registers as valid
	for (i = 0; i < LJ_YM2612_NUM_REGISTERS; i++)
	{
		if (LJ_YM2612_validRegisters[i] == 1)
		{
			//Found a valid register
			//0x20 = system which is specific set of registers
			//0x30-0x90 = settings per channel per slot (channel = bottom 2 bits of reg, slot = reg / 4)
			//0xA0-0xB6 = settings per channel (channel = bottom 2 bits of reg)
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
				//0xA8->0xAE - settings per slot (slot = bottom 2 bits of reg)
				//0xA9 = slot 0, 0xAA = slot 1, 0xA8 = slot 2, 0xA2 = slot 3
				//0xAD = slot 0, 0xAE = slot 1, 0xAC = slot 2, 0xA6 = slot 3
				int regBase = (i >> 2) << 2;
				int j;
				for (j = 0; j < 3; j++)
				{
					const char* const regBaseName = LJ_YM2612_REGISTER_NAMES[regBase];
					LJ_YM2612_validRegisters[regBase+j] = 1;
					LJ_YM2612_REGISTER_NAMES[regBase+j] = regBaseName;
				}
			}
		}
	}
}

LJ_YM2612_RESULT ym2612_setRegister(LJ_YM2612* const ym2612, LJ_YM_UINT8 part, LJ_YM_UINT8 reg, LJ_YM_UINT8 data)
{
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

	if (LJ_YM2612_validRegisters[reg] == 0)
	{
		fprintf(stderr, "ym2612_setRegister:unknown register:0x%X part:%d data:0x%X\n", reg, part, data);
		return LJ_YM2612_ERROR;
	}

	if ((reg < 0x30) && (part != 0))
	{
		fprintf(stderr, "ym2612_setRegister:unknown register:0x%X part:%d data:0x%X\n", reg, part, data);
		return LJ_YM2612_ERROR;
	}
	if (reg > 0xB6) 
	{
		fprintf(stderr, "ym2612_setRegister:unknown register:0x%X part:%d data:0x%X\n", reg, part, data);
		return LJ_YM2612_ERROR;
	}
	ym2612->part[part].reg[reg] = data;
	if (ym2612->debugFlags & LJ_YM2612_DEBUG)
	{
		printf( "ym2612:setRegister %s 0x%X\n", LJ_YM2612_REGISTER_NAMES[reg],data);
	}

	if (reg == LJ_LFO_EN_LFO_FREQ)
	{
		// 0x22 Bit 3 = LFO enable, Bit 0-2 = LFO frequency
		const int lfoEnable = (data >> 3) & 0x1;
		const int lfoFreq = (data >> 0) & 0x7;
		ym2612->lfoEnable = lfoEnable;
		ym2612->lfoFreq = lfoFreq;

		return LJ_YM2612_OK;
	}
	else if (reg == LJ_CH2_MODE_TIMER_CONTROL)
	{
		// 0x27 Ch2 Mode = Bits 6-7, 
		// 0x27 Reset B = Bit 5, Reset A = Bit 4, Enable B = Bit 3, Enable A = Bit 2, Start B = Bit 1, Start A = Bit 0
		ym2612->ch2Mode = (data >> 6) & 0x3;
	
		//ym2612_setTimers(data & 0x1F)

		return LJ_YM2612_OK;
	}
	else if (reg == LJ_DAC_EN)
	{
		// 0x2A DAC Bits 0-7
		ym2612->dacEnable = ~0 * ((data & 0x80) >> 7);
		return LJ_YM2612_OK;
	}
	else if (reg == LJ_DAC)
	{
		// 0x2B DAC en = Bit 7
#if LJ_YM2612_DAC_SHIFT >= 0
		ym2612->dacValue = ((int)(data - 0x80)) << LJ_YM2612_DAC_SHIFT;
#else // #if LJ_YM2612_DAC_SHIFT >= 0
		ym2612->dacValue = ((int)(data - 0x80)) >> -LJ_YM2612_DAC_SHIFT;
#endif // #if LJ_YM2612_DAC_SHIFT >= 0
		return LJ_YM2612_OK;
	}
	else if (reg == LJ_KEY_ONOFF)
	{
		// 0x28 Slot = Bits 4-7, Channel ID = Bits 0-2, part 0 or 1 is Bit 3
		const int channel = data & 0x03;
		if (channel != 0x03)
		{
			const int slotOnOff = (data >> 4);
			const int part = (data & 0x4) >> 2;
			
			LJ_YM2612_CHANNEL* const channelPtr = &ym2612->part[part].channel[channel];

			ym2612_channelKeyOnOff(channelPtr, slotOnOff);
			if (ym2612->debugFlags & LJ_YM2612_DEBUG)
			{
				printf("LJ_KEY_ONFF part:%d channel:%d slotOnOff:0x%X data:0x%X\n", part, channel, slotOnOff, data);
			}
		}
		return LJ_YM2612_OK;
	}
	else if ((reg >= 0x30) && (reg <= 0x9F))
	{
		//0x30-0x90 = settings per channel per slot (channel = bottom 2 bits of reg, slotReg = reg / 4)
		// Use a table to convert from slotReg -> internal slot
		const int channel = reg & 0x3;
		const int slotReg = (reg >> 2) & 0x3;
		const int slot = LJ_YM2612_slotTable[slotReg];
		const int regParameter = reg & 0xF0;

		LJ_YM2612_CHANNEL* const channelPtr = &ym2612->part[part].channel[channel];

		if (regParameter == LJ_DETUNE_MULT)
		{
			ym2612_channelSetDetuneMult(channelPtr, slot, data);
			if (ym2612->debugFlags & LJ_YM2612_DEBUG)
			{
				printf("LJ_DETUNE_MULT part:%d channel:%d slot:%d slotReg:%d data:0x%X\n", part, channel, slot, slotReg, data);
			}
			return LJ_YM2612_OK;
		}
		else if (regParameter == LJ_TOTAL_LEVEL)
		{
			ym2612_channelSetTotalLevel(channelPtr, slot, data);
			if (ym2612->debugFlags & LJ_YM2612_DEBUG)
			{
				printf("LJ_DETUNE_MULT part:%d channel:%d slot:%d slotReg:%d data:0x%X\n", part, channel, slot, slotReg, data);
			}
			return LJ_YM2612_OK;
		}
	}
	else if ((reg >= 0xA0) && (reg <= 0xB6))
	{
		// 0xA0-0xB0 = settings per channel (channel = bottom 2 bits of reg)
		const int channel = reg & 0x3;
		const int regParameter = reg & 0xFC;

		LJ_YM2612_CHANNEL* const channelPtr = &ym2612->part[part].channel[channel];
		if (regParameter == LJ_FREQLSB)
		{
			// 0xA0-0xA2 Frequency number LSB = Bits 0-7 (bottom 8 bits of frequency number)
			ym2612_channelSetFreqBlock(channelPtr, data);
			if (ym2612->debugFlags & LJ_YM2612_DEBUG)
			{
				printf( "LJ_FREQLSB part:%d channel:%d data:0x%X\n", part, channel, data);
			}
			return LJ_YM2612_OK;
		}
		else if (regParameter == LJ_BLOCK_FREQMSB)
		{
			// 0xA4-0xA6 Block = Bits 5-3, Frequency Number MSB = Bits 0-2 (top 3-bits of frequency number)
			ym2612_channelSetBlockFnumMSB(channelPtr, data);
			if (ym2612->debugFlags & LJ_YM2612_DEBUG)
			{
				printf( "LJ_BLOCK_FREQMSB part:%d channel:%d data:0x%X\n", part, channel, data);
			}
			return LJ_YM2612_OK;
		}
		else if (regParameter == LJ_CH2_FREQLSB)
		{
			// 0xA8-0xAA Channel 2 Frequency number LSB = Bits 0-7 (bottom 8 bits of frequency number)
			// 0xA8-0xAA - settings per slot (slot = bottom 2 bits of reg)
			// 0xA9 = slot 0, 0xAA = slot 1, 0xA8 = slot 2, 0xA2 = slot 3
			// Only on part 0
			if (part == 0)
			{
				// It is only for part 0 and channel 2 so just store it globally
				// LJ_YM2612_slotTable is 0->0, 1->2, 2->1, 3->3 we want: 0->1, 1->0, 3->2
				// which is LJ_YM2612_slotTable[slot+1]-1 : 0+1->2-1=1, 1+1->1-1=0, 2+1->3-1=2
				const int slotParameter = reg & 0x3;
				const int slot = LJ_YM2612_slotTable[slotParameter+1] - 1;

				ym2612_SetChannel2FreqBlock(ym2612, slot, data);
				if (ym2612->debugFlags & LJ_YM2612_DEBUG)
				{
					printf( "LJ_CH2_FREQLSB slot:%d slotParameter:%d data:0x%X\n", slot, slotParameter, data);
				}
			}
			return LJ_YM2612_OK;
		}
		else if (regParameter == LJ_CH2_BLOCK_FREQMSB)
		{
			// 0xAC-0xAE Block = Bits 5-3, Channel 2 Frequency Number MSB = Bits 0-2 (top 3-bits of frequency number)
			// 0xAC-0xAE - settings per slot (slot = bottom 2 bits of reg)
			// 0xAD = slot 0, 0xAE = slot 1, 0xAC = slot 2, 0xA6 = slot 3
			// Only on part 0
			if (part == 0)
			{
				// It is only for part 0 and channel 2 so just store it globally
				// LJ_YM2612_slotTable is 0->0, 1->2, 2->1, 3->3 we want: 0->1, 1->0, 3->2
				// which is LJ_YM2612_slotTable[slot+1]-1 : 0+1->2-1=1, 1+1->1-1=0, 2+1->3-1=2
				const int slotParameter = reg & 0x3;
				const int slot = LJ_YM2612_slotTable[slotParameter+1] - 1;

				ym2612_SetChannel2BlockFnumMSB(ym2612, slot, data);
				if (ym2612->debugFlags & LJ_YM2612_DEBUG)
				{
					printf( "LJ_CH2_BLOCK_FREQMSB slot:%d slotParameter:%d data:0x%X\n", slot, slotParameter, data);
				}
			}
			return LJ_YM2612_OK;
		}
		else if (regParameter == LJ_FEEDBACK_ALGO)
		{
			// 0xB0-0xB2 Feedback = Bits 5-3, Algorithm = Bits 0-2
			ym2612_channelSetFeedbackAlgorithm(channelPtr, data);
			if (ym2612->debugFlags & LJ_YM2612_DEBUG)
			{
				printf( "LJ_FEEDBACK_ALGO part:%d channel:%d data:0x%X\n", part, channel, data);
			}
			return LJ_YM2612_OK;
		}
		else if (regParameter == LJ_LR_AMS_PMS)
		{
			// 0xB4-0xB6 Feedback = Left= Bit 7, Right= Bit 6, AMS = Bits 3-5, PMS = Bits 0-1
			ym2612_channelSetLeftRightAmsPms(channelPtr, data);
			if (ym2612->debugFlags & LJ_YM2612_DEBUG)
			{
				printf( "LJ_LR_AMS_PMS part:%d channel:%d data:0x%X\n", part, channel, data);
			}
			return LJ_YM2612_OK;
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

	ym2612->debugFlags = flags;

	for (part = 0; part < LJ_YM2612_NUM_PARTS; part++)
	{
		int channel;
		for (channel = 0; channel < LJ_YM2612_NUM_CHANNELS_PER_PART; channel++)
		{
			LJ_YM2612_CHANNEL* const channelPtr = &ym2612->part[part].channel[channel];
			channelPtr->debugFlags = flags;
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
	int dacValue = 0;
	int dacValueLeft = 0;
	int dacValueRight = 0;
	int sample;
	const int debugFlags = ym2612->debugFlags;

	int channel;
	int slot;

	int outputChannelMask = 0xFF;
	int channelMask = 0x1;

	if (ym2612 == NULL)
	{
		fprintf(stderr, "LJ_YM2612_generateOutput:ym2612 is NULL\n");
		return LJ_YM2612_ERROR;
	}

	//Global state update - LFO, DAC, SSG
	dacValue= ym2612->dacValue & ym2612->dacEnable;
	if (debugFlags & LJ_YM2612_NODAC)
	{
		dacValue = 0x0;
	}
	dacValueLeft = (dacValue & ym2612->channels[5]->left);
	dacValueRight = (dacValue & ym2612->channels[5]->right);

	if (debugFlags & LJ_YM2612_ONECHANNEL)
	{
		// channel starts at 0
		const int debugChannel =((debugFlags >> LJ_YM2612_ONECHANNEL_SHIFT) & LJ_YM2612_ONECHANNEL_MASK); 
		outputChannelMask = 1 << debugChannel;
	}

	//For each cycle
	//Loop over channels updating them, mix them, then output them into the buffer
	for (sample = 0; sample < numCycles; sample++)
	{
		int mixedLeft = 0;
		int mixedRight = 0;
		short outL = 0;
		short outR = 0;
	
		//DAC enable blocks channel 5
		const int numChannelsToProcess = ym2612->dacEnable ? LJ_YM2612_NUM_CHANNELS_TOTAL-1 : LJ_YM2612_NUM_CHANNELS_TOTAL;

		for (channel = 0; channel < numChannelsToProcess; channel++)
		{
			LJ_YM2612_CHANNEL* const channelPtr = ym2612->channels[channel];
			int channelOutput = 0;

			const int channelFnum = channelPtr->fnum;
			const int channelBlock = channelPtr->block;
			const int channelKeycode = channelPtr->keycode;
			int channelOmegaDirty = channelPtr->omegaDirty;

			if ((channelMask & outputChannelMask) == 0)
			{
				channelMask = (channelMask << 1);
				continue;
			}
			for (slot = 0; slot < LJ_YM2612_NUM_SLOTS_PER_CHANNEL; slot++)
			{
				LJ_YM2612_SLOT* const slotPtr = &(channelPtr->slot[slot]);

				int carrierOutput = 0;

				int slotFnum;
				int slotBlock;
				int slotKeycode;
				int omegaDirty = 0;

				const int OMEGA = slotPtr->omega;
				const int slotPhi = (OMEGA >> LJ_YM2612_FREQ_BITS);
		
				//For normal mode or channels that aren't channel 2 or for slot 3
				if ((ym2612->ch2Mode == 0) || (channel != 2) || (slot == 3))
				{
					//Take the slot frequency from the channel frequency
					slotFnum = channelFnum;
					slotBlock = channelBlock;
					slotKeycode = channelKeycode;
					omegaDirty = channelOmegaDirty;
				}
				else
				{
					//Channel 2 and it is in special mode with fnum, block (keycode) per slot for slots 0, 1, 2
					slotFnum = ym2612->channel2slotData[slot].fnum;
					slotBlock = ym2612->channel2slotData[slot].block;
					slotKeycode = ym2612->channel2slotData[slot].keycode;
				}

				//Compute the omega delta value
				if ((slotPtr->omegaDirty == 1) || (omegaDirty == 1))
				{
					ym2612_slotComputeOmegaDelta(slotPtr, slotFnum, slotBlock, slotKeycode, debugFlags);
					slotPtr->omegaDirty = 0;
				}
	
				// Slot 0 feedback
				if (slot == 0)
				{
					//Average of the last 2 samples
					slotPtr->fmInputDelta = (channelPtr->slot0Output[0] + channelPtr->slot0Output[1]);
					if (channelPtr->feedback != 0)
					{
						//Average = >> 1
						//From docs: Feedback is 0 = + 0, 1 = +PI/16, 2 = +PI/8, 3 = +PI/4, 4 = +PI/2, 5 = +PI, 6 = +2 PI, 7 = +4 PI
						//fmDelta is currently in -1->1 which is mapped to 0->2PI (and correct angle units) by the deltaPhi scaling
						//feedback 6 = +1 in these units, so feedback 6 maps to 2^0: << feedback then >> 6
						//deltaPhi scaling has an implicit x4 so /4 here: >> 2

						//Results in: >> (1+6+2-feedback) >> (9-feedback)
						slotPtr->fmInputDelta = slotPtr->fmInputDelta >> (9 - channelPtr->feedback);
					}
					else
					{
						slotPtr->fmInputDelta = 0;
					}
				}
				//Phi needs to have the fmInputDelta added to it to make algorithms work
#if LJ_YM2612_DELTA_PHI_SCALE >= 0
				const int deltaPhi = (slotPtr->fmInputDelta >> LJ_YM2612_DELTA_PHI_SCALE);
#else // #if LJ_YM2612_DELTA_PHI_SCALE >= 0
				const int deltaPhi = (slotPtr->fmInputDelta << -LJ_YM2612_DELTA_PHI_SCALE);
#endif // #if LJ_YM2612_DELTA_PHI_SCALE >= 0

				const int phi = slotPhi + deltaPhi;

				const int scaledSin = LJ_YM2612_sinTable[phi & LJ_YM2612_SIN_TABLE_MASK];

				int slotOutput = (slotPtr->volume * scaledSin) >> LJ_YM2612_SIN_SCALE_BITS;

				// Keep within +/- 1
				slotOutput = LJ_YM2612_CLAMP_VOLUME(slotOutput);

				// Scale by TL (total level = 0->1)
				slotOutput = (slotOutput * slotPtr->totalLevel) >> LJ_YM2612_TL_SCALE_BITS;

				// Add this output onto the input of the slot in the algorithm
				if (slotPtr->modulationOutput[0] != NULL)
				{
					*slotPtr->modulationOutput[0] += slotOutput;
				}
				if (slotPtr->modulationOutput[1] != NULL)
				{
					*slotPtr->modulationOutput[1] += slotOutput;
				}
				if (slotPtr->modulationOutput[2] != NULL)
				{
					*slotPtr->modulationOutput[2] += slotOutput;
				}

				if (slot == 0)
				{
					// Save the slot 0 output (the last 2 outputs are saved), used as input for the slot 0 feedback
					channelPtr->slot0Output[0] = channelPtr->slot0Output[1];
					channelPtr->slot0Output[1] = slotOutput;
				}

				carrierOutput = slotOutput & slotPtr->carrierOutputMask;
				channelOutput += carrierOutput;

				if (debugFlags & LJ_YM2612_DEBUG)
				{
					if (slotOutput != 0)
					{
						printf("Channel:%d Slot:%d slotOutput:%d carrierOutput:%d\n", channelPtr->id, slot, slotOutput, carrierOutput);
					}
				}
				// Zero out the fmInputDelta so it is ready to be used on the next update
				slotPtr->fmInputDelta = 0;
			}
			if ((channelMask & outputChannelMask) == 0)
			{
				channelOutput = 0;
			}

			if (debugFlags & LJ_YM2612_DEBUG)
			{
				if (channelOutput != 0)
				{
					printf("Channel:%d channelOutput:%d\n", channelPtr->id, channelOutput);
				}
			}

			// Keep within +/- 1
			channelOutput = LJ_YM2612_CLAMP_VOLUME(channelOutput);

			mixedLeft += channelOutput & channelPtr->left;
			mixedRight += channelOutput & channelPtr->right;

			channelPtr->omegaDirty = 0;

			channelMask = (channelMask << 1);
		}

		mixedLeft += dacValueLeft;
		mixedRight += dacValueRight;

		// Keep within +/- 1
		mixedLeft = LJ_YM2612_CLAMP_VOLUME(mixedLeft);
		mixedRight = LJ_YM2612_CLAMP_VOLUME(mixedRight);

#if LJ_YM2612_OUTPUT_SCALE >= 0
		mixedLeft = mixedLeft >> LJ_YM2612_OUTPUT_SCALE;
		mixedRight = mixedRight >> LJ_YM2612_OUTPUT_SCALE;
#else // #if LJ_YM2612_OUTPUT_SCALE >= 0
		mixedLeft = mixedLeft << -LJ_YM2612_OUTPUT_SCALE;
		mixedRight = mixedRight << -LJ_YM2612_OUTPUT_SCALE;
#endif // #if LJ_YM2612_OUTPUT_SCALE >= 0

		outL = mixedLeft;
		outR = mixedRight;

		outputLeft[sample] = outL;
		outputRight[sample] = outR;
	}

	// Update volumes and frequency position
	for (channel = 0; channel < LJ_YM2612_NUM_CHANNELS_TOTAL; channel++)
	{
		LJ_YM2612_CHANNEL* const channelPtr = ym2612->channels[channel];
		int slot;

		for (slot = 0; slot < LJ_YM2612_NUM_SLOTS_PER_CHANNEL; slot++)
		{
			LJ_YM2612_SLOT* const slotPtr = &(channelPtr->slot[slot]);

			slotPtr->omega += slotPtr->omegaDelta;
			slotPtr->volume += slotPtr->volumeDelta;

			slotPtr->volume = LJ_YM2612_CLAMP_VOLUME(slotPtr->volume);
			if (slotPtr->volume < 0)
			{
				slotPtr->volume = 0;
			}
		}
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

