#include "lj_ym2612.h"

#include <stdio.h>
#include <malloc.h>
#include <math.h>

#ifndef M_PI
#define M_PI (3.14159265358979323846f)
#endif
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

/*
* ////////////////////////////////////////////////////////////////////
* // 
* // Internal data & functions
* // 
* ////////////////////////////////////////////////////////////////////
*/

typedef struct LJ_YM2612_PART LJ_YM2612_PART;
typedef struct LJ_YM2612_CHANNEL LJ_YM2612_CHANNEL;
typedef struct LJ_YM2612_SLOT LJ_YM2612_SLOT;
typedef struct LJ_YM2612_EG LJ_YM2612_EG;
typedef struct LJ_YM2612_CHANNEL2_SLOT_DATA LJ_YM2612_CHANNEL2_SLOT_DATA;

#define LJ_YM2612_NUM_REGISTERS (0xFF)
#define LJ_YM2612_NUM_PARTS (2)
#define LJ_YM2612_NUM_CHANNELS_PER_PART (3)
#define LJ_YM2612_NUM_CHANNELS_TOTAL (LJ_YM2612_NUM_CHANNELS_PER_PART * LJ_YM2612_NUM_PARTS)
#define LJ_YM2612_NUM_SLOTS_PER_CHANNEL (4)

typedef enum LJ_YM2612_REGISTERS {
		LJ_LFO_EN_LFO_FREQ = 0x22,
		LJ_CH2_MODE_TIMER_CONTROL = 0x27,
		LJ_KEY_ONOFF = 0x28,
		LJ_DAC = 0x2A,
		LJ_DAC_EN = 0x2B,
		LJ_DETUNE_MULT = 0x30,
		LJ_TOTAL_LEVEL = 0x40,
		LJ_RATE_SCALE_ATTACK_RATE = 0x50,
		LJ_AM_DECAY_RATE = 0x60,
		LJ_SUSTAIN_RATE = 0x70,
		LJ_SUSTAIN_LEVEL_RELEASE_RATE = 0x80,
		LJ_FREQLSB = 0xA0,
		LJ_BLOCK_FREQMSB = 0xA4,
		LJ_CH2_FREQLSB = 0xA8,
		LJ_CH2_BLOCK_FREQMSB = 0xAC,
		LJ_FEEDBACK_ALGO = 0xB0,
		LJ_LR_AMS_PMS = 0xB4
} LJ_YM2612_REGISTERS;

static const char* LJ_YM2612_REGISTER_NAMES[LJ_YM2612_NUM_REGISTERS];
static LJ_YM_UINT8 LJ_YM2612_validRegisters[LJ_YM2612_NUM_REGISTERS];

/* Global fixed point scaling for outputs of things */
#define LJ_YM2612_GLOBAL_SCALE_BITS (16)

/* FREQ scale = 16.16 - used global scale to keep things consistent */
#define LJ_YM2612_FREQ_BITS (LJ_YM2612_GLOBAL_SCALE_BITS)
#define LJ_YM2612_FREQ_MASK ((1 << LJ_YM2612_FREQ_BITS) - 1)

/* Volume scale = xx.yy (-1->1) */
#define LJ_YM2612_VOLUME_SCALE_BITS (13)

/* Number of bits to use for scaling output e.g. choose an output of 1.0 -> 13-bits (8192) */
#define LJ_YM2612_OUTPUT_VOLUME_BITS (13)
#define LJ_YM2612_OUTPUT_SCALE (LJ_YM2612_VOLUME_SCALE_BITS - LJ_YM2612_OUTPUT_VOLUME_BITS)

/* Max volume is -1 -> +1 */
#define LJ_YM2612_VOLUME_MAX (1<<LJ_YM2612_VOLUME_SCALE_BITS)

/* Sin output scale = xx.yy (-1->1) */
#define LJ_YM2612_SIN_SCALE_BITS (LJ_YM2612_GLOBAL_SCALE_BITS)

/* TL table scale - matches volume scale */
#define LJ_YM2612_TL_SCALE_BITS (LJ_YM2612_VOLUME_SCALE_BITS)

/* SL table scale - must match volume scale */
#define LJ_YM2612_SL_SCALE_BITS (LJ_YM2612_VOLUME_SCALE_BITS)

/* DAC is in 7-bit scale (-128->128) so this converts it to volume_scale bits */
#define LJ_YM2612_DAC_SHIFT (LJ_YM2612_VOLUME_SCALE_BITS - 7)

/* FNUM = 11-bit table */
#define LJ_YM2612_FNUM_TABLE_BITS (11)
#define LJ_YM2612_FNUM_TABLE_NUM_ENTRIES (1 << LJ_YM2612_FNUM_TABLE_BITS)
static int LJ_YM2612_fnumTable[LJ_YM2612_FNUM_TABLE_NUM_ENTRIES];

/* SIN table = 10-bit table but stored in LJ_YM2612_SIN_SCALE_BITS format */
#define LJ_YM2612_SIN_TABLE_BITS (10)
#define LJ_YM2612_SIN_TABLE_NUM_ENTRIES (1 << LJ_YM2612_SIN_TABLE_BITS)
#define LJ_YM2612_SIN_TABLE_MASK ((1 << LJ_YM2612_SIN_TABLE_BITS) - 1)
static int LJ_YM2612_sinTable[LJ_YM2612_SIN_TABLE_NUM_ENTRIES];

/* Attenation envelop generation is 10-bits */
#define LJ_YM2612_ATTENUATIONDB_BITS (10)
#define LJ_YM2612_ATTENUATIONDB_MAX ((1 << 10) - 1)

/* Right shift for delta phi = LJ_YM2612_VOLUME_SCALE_BITS - LJ_YM2612_SIN_TABLE_BITS - 2 */
/* >> LJ_YM2612_VOLUME_SCALE_BITS = to remove the volume scale in the output from the sin table */
/* << LJ_YM2612_SIN_TABLE_BITS = to put the output back into the range of the sin table */
/* << 2 = *4 to convert FM output of -1 -> +1 => -4 -> +4 => -PI (and a bit) -> +PI (and a bit) */
#define LJ_YM2612_DELTA_PHI_SCALE (LJ_YM2612_VOLUME_SCALE_BITS - LJ_YM2612_SIN_TABLE_BITS -2)

/* DETUNE table = this are integer freq shifts in the frequency integer scale */
/* In the docs (YM2608) : the base scale is (0.052982) this equates to: (8*1000*1000/(1*1024*1024))/144 */
/* For YM2612 (Genesis) it would equate to: (7670453/(1*1024*1024)/144 => 0.050799 */
/* 144 is the number clock cycles the chip takes to make 1 sample:  */
/* 144 = 6 channels x 4 operators x 8 cycles per operator  */
/* 144 = 6 channels x 24 cycles per channel */
/* 144 = also the prescaler: YM2612 is 1/6 : one sample for each 24 FM clocks */
#define LJ_YM2612_NUM_KEYCODES (1<<5)
#define LJ_YM2612_NUM_FDS (4)
static const LJ_YM_UINT8 LJ_YM2612_detuneScaleTable[LJ_YM2612_NUM_FDS*LJ_YM2612_NUM_KEYCODES] = {
/* FD=0 : all 0 */
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
/* FD=1 */
0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 
2, 3, 3, 3, 4, 4, 4, 5, 5, 6, 6, 7, 8, 8, 8, 8, 
/* FD=2 */
1, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 4, 4, 4, 5, 
5, 6, 6, 7, 8, 8, 9,10,11,12,13,14,16,16,16,16, 
/* FD=3 */
2, 2, 2, 2, 2, 3, 3, 3, 4, 4, 4, 5, 5, 6, 6, 7, 
8, 8, 9,10,11,12,13,14,16,17,19,20,22,22,22,22, 
};

/* Convert detuneScaleTable into a frequency shift (+ and -) */
static int LJ_YM2612_detuneTable[2*LJ_YM2612_NUM_FDS*LJ_YM2612_NUM_KEYCODES];

/* fnum -> keycode table for computing N3 & N4 */
/* N4 = Fnum Bit 11  */
/* N3 = Fnum Bit 11 * (Bit 10 | Bit 9 | Bit 8) | !Bit 11 * Bit 10 * Bit 9 * Bit 8 */
/* Table = (N4 << 1) | N3 */
static const LJ_YM_UINT8 LJ_YM2612_fnumKeycodeTable[16] = {
0, 0, 0, 0, 0, 0, 0, 1,
2, 3, 3, 3, 3, 3, 3, 3,
};

/* TL - table (7-bits): 0->0x7F : output = 2^(-TL/8) */
/* From the docs each step is -0.75dB = x0.9172759 = 2^(-1/8) */
#define LJ_YM2612_TL_TABLE_NUM_ENTRIES (128)
static int LJ_YM2612_tlTable[LJ_YM2612_TL_TABLE_NUM_ENTRIES];

/* SL - table (4-bits): 0->0xF : output = 2^(-SL/2) - could use tlTable[SL*4] */
/* From the docs Bit 0 = 3dB, Bit 1 = 6dB, Bit 2 = 12dB, Bit 3 = 24dB = x0.707105 = 2^(-1/2) & 0xF = 93dB */
#define LJ_YM2612_SL_TABLE_NUM_ENTRIES (16)
static LJ_YM_UINT32 LJ_YM2612_slTable[LJ_YM2612_SL_TABLE_NUM_ENTRIES];

/* The slots referenced in the registers are not 0,1,2,3 *sigh* */
static LJ_YM_UINT8 LJ_YM2612_slotTable[LJ_YM2612_NUM_SLOTS_PER_CHANNEL] = { 0, 2, 1, 3 };

/* EG circuit timer units fixed point */
#define LJ_YM2612_EG_TIMER_NUM_BITS (16)
#define LJ_YM2612_EG_TIMER_NUM_ENTRIES (1 << LJ_YM2612_EG_TIMER_NUM_BITS)
#define LJ_YM2612_EG_TIMER_MASK ((1 << LJ_YM2612_EG_TIMER_NUM_BITS) - 1)

/* Some argument on this value from looking at the forums: MAME = 3, Nemesis (on PAL MD) adamant it is 2.4375 */
#define LJ_YM2612_EG_TIMER_OUTPUT_PER_FM_SAMPLE (3.0f) /* MAME */
/* #define LJ_YM2612_EG_TIMER_OUTPUT_PER_FM_SAMPLE (2.4375f) */ /* NEMESIS */

typedef enum LJ_YM2612_ADSR {
	LJ_YM2612_UNKNOWN,
	LJ_YM2612_ATTACK,
	LJ_YM2612_DECAY,
	LJ_YM2612_SUSTAIN,
	LJ_YM2612_RELEASE
} LJ_YM2612_ADSR;

struct LJ_YM2612_SLOT
{
	int id;

	int omega;
	int keycode;
	int omegaDelta;
	int totalLevel;
	unsigned int sustainLevelDB;

	int volume;
	unsigned int attenuationDB;
	unsigned int attenuationDBDelta;

	/* Algorithm support */
	int fmInputDelta;
	int* modulationOutput[3];
	int carrierOutputMask;
	
	/* ADSR settigns */
	LJ_YM2612_ADSR adsrState;
	LJ_YM_UINT8 keyScale;
	LJ_YM_UINT8 attackRate;
	LJ_YM_UINT8 decayRate;
	LJ_YM_UINT8 sustainRate;
	LJ_YM_UINT8 releaseRate;

	/* LFO settings */
	LJ_YM_UINT8 am;

	/* frequency settings */
	LJ_YM_UINT8 detune;
	LJ_YM_UINT8 multiple;

	LJ_YM_UINT8 keyOn;

	LJ_YM_UINT8 omegaDirty;
};

struct LJ_YM2612_EG 
{
	LJ_YM_UINT32 timer;
	LJ_YM_UINT32 timerAddPerOutputSample;
	LJ_YM_UINT32 timerPerUpdate;
	LJ_YM_UINT32 counter;
};

struct LJ_YM2612_CHANNEL
{
	LJ_YM2612_SLOT slot[LJ_YM2612_NUM_SLOTS_PER_CHANNEL];

	LJ_YM_UINT32 debugFlags;
	int id;

	int slot0Output[2];

	int feedback;

	int left;
	int right;

	int fnum;
	int block;

	LJ_YM_UINT8 omegaDirty;
	LJ_YM_UINT8 block_fnumMSB;
	LJ_YM_UINT8 keycode;

	LJ_YM_UINT8 algorithm;

	LJ_YM_UINT8 ams;
	LJ_YM_UINT8 pms;
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

	int id;
};

struct LJ_YM2612
{
	LJ_YM2612_PART part[LJ_YM2612_NUM_PARTS];
	LJ_YM2612_CHANNEL2_SLOT_DATA channel2slotData[LJ_YM2612_NUM_SLOTS_PER_CHANNEL-1];
	LJ_YM2612_CHANNEL* channels[LJ_YM2612_NUM_CHANNELS_TOTAL];
	LJ_YM2612_EG eg;

	float baseFreqScale;

	LJ_YM_UINT32 clockRate;
	LJ_YM_UINT32 outputSampleRate;

	LJ_YM_UINT32 debugFlags;

	int dacValue;
	int dacEnable;

	LJ_YM_UINT8 D07;
	LJ_YM_UINT8 regAddress;
	LJ_YM_UINT8 slotWriteAddr;	

	LJ_YM_UINT8 ch2Mode;
	LJ_YM_UINT8 lfoEnable;
	LJ_YM_UINT8 lfoFreq;
};

/* From the current rate you get a shift width (1,2,4,8,...) and you only update the envelop every N times of the global counter */
/* The rate = ADSRrate * 2 + keyRateScale but rate = 0 if ADSRrate = 0 and clamped between 0->63 */
static LJ_YM_UINT32 ym2612_computeEGUpdateMask(const int adsrRate, const int keyRateScale)
{
	int egRate;
	int egRateShift;
	LJ_YM_UINT32 egRateUpdateMask;

	egRate = adsrRate * 2 + keyRateScale;
	if ((egRate < 0) || (adsrRate == 0))
	{
		egRate = 0;
	}
	if (egRate > 63)
	{
		egRate = 63;
	}

	/* EG rate shift counter is 11 - (egRate / 4 ) but clamped to 0 can't be negative */
	egRateShift = 11 - (egRate >> 2);

	if (egRateShift < 0)
	{
		egRateShift = 0;
	}

	/* if (egCounter % (1 << egRateShift) == 0) then do an update */
	/* logically the same as if (egCounter & ((1 << egRateShift)-1) == 0x0) */
	egRateUpdateMask = (LJ_YM_UINT32)((1 << egRateShift) -1);

	return egRateUpdateMask;
}

static int LJ_YM2612_CLAMP_VOLUME(const int volume) 
{
	if (volume > LJ_YM2612_VOLUME_MAX)
	{
		return LJ_YM2612_VOLUME_MAX;
	}
	else if (volume < -LJ_YM2612_VOLUME_MAX)
	{
		return -LJ_YM2612_VOLUME_MAX;
	}
	return volume;
}

static int ym2612_computeDetuneDelta(const int detune, const int keycode, const LJ_YM_UINT32 debugFlags)
{
	const int detuneDelta = LJ_YM2612_detuneTable[detune*LJ_YM2612_NUM_KEYCODES+keycode];

	if (debugFlags & LJ_YM2612_DEBUG)
	{
	}
	return detuneDelta;
}

static int ym2612_computeOmegaDelta(const int fnum, const int block, const int multiple, const int detune, const int keycode, 
																		const LJ_YM_UINT32 debugFlags)
{
	/* F * 2^(B-1) */
	/* Could multiply the fnumTable up by (1<<6) then change this to fnumTable >> (7-B) */
	int omegaDelta = (LJ_YM2612_fnumTable[fnum] << block) >> 1;

	const int detuneDelta = ym2612_computeDetuneDelta(detune, keycode, debugFlags);
	omegaDelta += detuneDelta;

	if (omegaDelta < 0)
	{
		/* Wrap around */
		/* omegaDelta += FNUM_MAX; */
		omegaDelta += 1024;
	}

	/* /2 because multiple is stored as x2 of its value */
	omegaDelta = (omegaDelta * multiple) >> 1;

	return omegaDelta;
}

static void ym2612_slotUpdateEG(LJ_YM2612_SLOT* const slotPtr, const LJ_YM_UINT32 egCounter)
{
	const LJ_YM2612_ADSR adsrState = slotPtr->adsrState;
	int keycode =	slotPtr->keycode;

	LJ_YM_UINT32 slotVolumeDB = LJ_YM2612_ATTENUATIONDB_MAX;
	LJ_YM_UINT32 attenuationDB = slotPtr->attenuationDB;
	int scaledVolume = 0;

	int keyRateScale = keycode >> (3-keycode);
	int invertOutput = 0;

	attenuationDB &= 0x3FF;
	keyRateScale = keycode >> 3;
	keyRateScale = 0;

	if (adsrState == LJ_YM2612_ATTACK)
	{
		const LJ_YM_UINT32 egRateUpdateMask = ym2612_computeEGUpdateMask(slotPtr->attackRate, keyRateScale);
		invertOutput = 1;
		if ((egCounter & egRateUpdateMask) == 0)
		{
			LJ_YM_UINT32 oldDB = attenuationDB;
			LJ_YM_UINT32 deltaDB;
			slotPtr->attenuationDBDelta = +2;
			deltaDB = (((oldDB * slotPtr->attenuationDBDelta)+15) >> 4);
			/* inverted exponential curve: attenuation += increment * ((1024-attenuation) / 16 + 1) : NEMESIS */
			/* inverted exponential curve: attenuation += (increment * ~attenuation) / 16 : MAME without invert output */
			attenuationDB = oldDB - deltaDB;
/*
			printf("Attack[%d]: new-volume:%d pre-volume:%d delta:%d inc:%d mask:%d\n", 
					count, attenuationDB, oldDB, attenuationDB-oldDB, slotPtr->attenuationDBDelta, egRateUpdateMask);
			count++;
*/
			invertOutput = 0;

			if ((attenuationDB == 0) || (attenuationDB > 0x3FF))
			{
				invertOutput = 0;
				attenuationDB = 0;
				slotPtr->adsrState = LJ_YM2612_DECAY;
			}
		}
	}
	else if (adsrState == LJ_YM2612_DECAY)
	{
		const LJ_YM_UINT32 egRateUpdateMask = ym2612_computeEGUpdateMask(slotPtr->decayRate, keyRateScale);
		if ((egCounter & egRateUpdateMask) == 0)
		{
			slotPtr->attenuationDBDelta = +1;
			attenuationDB += slotPtr->attenuationDBDelta;
			if (attenuationDB >= slotPtr->sustainLevelDB)
			{
				attenuationDB = slotPtr->sustainLevelDB;
				slotPtr->adsrState = LJ_YM2612_SUSTAIN;
			}
		}
	}
	else if (adsrState == LJ_YM2612_SUSTAIN)
	{
		const LJ_YM_UINT32 egRateUpdateMask = ym2612_computeEGUpdateMask(slotPtr->sustainRate, keyRateScale);
		if ((egCounter & egRateUpdateMask) == 0)
		{
			slotPtr->attenuationDBDelta = +1;
			attenuationDB += slotPtr->attenuationDBDelta;
		}
	}
	else if (adsrState == LJ_YM2612_RELEASE)
	{
		const LJ_YM_UINT32 egRateUpdateMask = ym2612_computeEGUpdateMask(slotPtr->releaseRate, keyRateScale);
		if ((egCounter & egRateUpdateMask) == 0)
		{
			slotPtr->attenuationDBDelta = +1;
			attenuationDB += slotPtr->attenuationDBDelta;
		}
	}

	if (invertOutput == 1)
	{
		/* Attack is inverse of DB */
		/*slotVolumeDB = LJ_YM2612_ATTENUATIONDB_MAX - attenuationDB;*/
		slotVolumeDB = ~attenuationDB;
		slotVolumeDB &= 0x3FF;
	}
	else
	{
		slotVolumeDB = attenuationDB;
	}
	slotPtr->attenuationDB = attenuationDB;

	/* Convert slotVolumeDB into volume */
	{
		/* slotVolumeDB is a logarithmic scale of 10-bits, TL is 7-bits and is 2^(-1/8), move to 10-bits gives 2^(-1/(8*8)) */
		const float dbScale = -(float)slotVolumeDB * (1.0f / 64.0f);
		const float value = (float)pow(2.0f, dbScale);
		scaledVolume = (int)(value * (float)(1 << LJ_YM2612_VOLUME_SCALE_BITS));
	}

	slotPtr->volume = scaledVolume;
}

static void ym2612_slotComputeOmegaDelta(LJ_YM2612_SLOT* const slotPtr, const int fnum, const int block, const int keycode, 
																				 const LJ_YM_UINT32 debugFlags)
{
	const int multiple = slotPtr->multiple;
	const int detune = slotPtr->detune;

	const int omegaDelta = ym2612_computeOmegaDelta(fnum, block, multiple, detune, keycode, debugFlags);

	slotPtr->omegaDelta = omegaDelta;

	if (debugFlags & LJ_YM2612_DEBUG)
	{
		printf("Slot:%d omegaDelta %d fnum:%d block:%d multiple:%3.1f detune:%d keycode:%d\n", 
					 slotPtr->id, omegaDelta, fnum, block, (float)multiple/2.0f, detune, keycode);
	}
}

static void ym2612_channelSetConnections(LJ_YM2612_CHANNEL* const channelPtr)
{
	const int algorithm = channelPtr->algorithm;
	const int FULL_OUTPUT_MASK = ~0;
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
	/* 0xB4-0xB6 Feedback = Left= Bit 7, Right= Bit 6, AMS = Bits 3-5, PMS = Bits 0-1 */
	const int left = (data >> 7) & 0x1;
	const int right = (data >> 6) & 0x1;
	const LJ_YM_UINT8 AMS = (data >> 3) & 0x7;
	const LJ_YM_UINT8 PMS = (data >> 0) & 0x3;

	channelPtr->left = left *	~0;
	channelPtr->right = right * ~0;
	channelPtr->ams = AMS;
	channelPtr->pms = PMS;

	if (channelPtr->debugFlags & LJ_YM2612_DEBUG)
	{
		printf("SetLeftRightAmsPms channel:%d left:%d right:%d AMS:%d PMS:%d\n", channelPtr->id, left, right, AMS, PMS);
	}

	/* Update the connections for this channel */
	ym2612_channelSetConnections(channelPtr);
}

static void ym2612_channelSetFeedbackAlgorithm(LJ_YM2612_CHANNEL* const channelPtr, const LJ_YM_UINT8 data)
{
	/* 0xB0-0xB2 Feedback = Bits 5-3, Algorithm = Bits 0-2 */
	const LJ_YM_UINT8 algorithm = (data >> 0) & 0x7;
	const int feedback = (data >> 3) & 0x7;

	channelPtr->feedback = feedback;
	channelPtr->algorithm = algorithm;

	if (channelPtr->debugFlags & LJ_YM2612_DEBUG)
	{
		printf("SetFeedbackAlgorithm channel:%d feedback:%d algorithm:%d\n", channelPtr->id, feedback, algorithm);
	}

	/* Update the connections for this channel */
	ym2612_channelSetConnections(channelPtr);
}

static void ym2612_channelSetKeyScaleAttackRate(LJ_YM2612_CHANNEL* const channelPtr, const int slot, const LJ_YM_UINT8 RS_AR)
{
	LJ_YM2612_SLOT* const slotPtr = &(channelPtr->slot[slot]);

	/* Attack Rate = Bits 0-4 */
	const LJ_YM_UINT8 AR = ((RS_AR >> 0) & 0x1F);

	/* Rate Scale = Bits 6-7 */
	const LJ_YM_UINT8 RS = ((RS_AR >> 6) & 0x3);

	slotPtr->attackRate = AR;
	slotPtr->keyScale = RS;

	if (channelPtr->debugFlags & LJ_YM2612_DEBUG)
	{
		printf("SetKeyScaleAttackRate channel:%d slot:%d AR:%d RS:%d\n", channelPtr->id, slot, AR, RS);
	}
}

static void ym2612_channelSetAMDecayRate(LJ_YM2612_CHANNEL* const channelPtr, const int slot, const LJ_YM_UINT8 AM_DR)
{
	LJ_YM2612_SLOT* const slotPtr = &(channelPtr->slot[slot]);

	/* Decay Rate = Bits 0-4 */
	const LJ_YM_UINT8 DR = ((AM_DR >> 0) & 0x1F);

	/* AM  = Bit 7 */
	const LJ_YM_UINT8 AM = ((AM_DR >> 7) & 0x1);

	slotPtr->decayRate = DR;
	slotPtr->am = AM;

	if (channelPtr->debugFlags & LJ_YM2612_DEBUG)
	{
		printf("SetAMDecayRate channel:%d slot:%d DR:%d AM:%d\n", channelPtr->id, slot, DR, AM);
	}
}

static void ym2612_channelSetSustainRate(LJ_YM2612_CHANNEL* const channelPtr, const int slot, const LJ_YM_UINT8 AM_DR)
{
	LJ_YM2612_SLOT* const slotPtr = &(channelPtr->slot[slot]);

	/* Sustain Rate = Bits 0-4 */
	const LJ_YM_UINT8 SR = ((AM_DR >> 0) & 0x1F);

	slotPtr->sustainRate = SR;

	if (channelPtr->debugFlags & LJ_YM2612_DEBUG)
	{
		printf("SetSustainRate channel:%d slot:%d SR:%d\n", channelPtr->id, slot, SR);
	}
}

static void ym2612_channelSetSustainLevelReleaseRate(LJ_YM2612_CHANNEL* const channelPtr, const int slot, const LJ_YM_UINT8 SL_RR)
{
	LJ_YM2612_SLOT* const slotPtr = &(channelPtr->slot[slot]);

	/* Release rate = Bits 0-3 */
	const LJ_YM_UINT8 RR = ((SL_RR >> 0) & 0xF);

	/* Sustain Level = Bits 4-7 */
	const LJ_YM_UINT32 SL = ((SL_RR >> 4) & 0xF);
	const LJ_YM_UINT32 SLscale = LJ_YM2612_slTable[SL];

	/* Convert from 4-bits to usual 5-bits for rates */
	slotPtr->releaseRate = (LJ_YM_UINT8)((RR << 1) + 1);
	slotPtr->sustainLevelDB = SLscale;
	slotPtr->sustainLevelDB = (SL << 5U);

	if (channelPtr->debugFlags & LJ_YM2612_DEBUG)
	{
		printf("SetSustainLevelReleaseRate channel:%d slot:%d SL:%d scale:%f SLscale:%d RR:%d\n", channelPtr->id, 
					slot, SL, ((float)(SLscale)/(float)(1<<LJ_YM2612_SL_SCALE_BITS)), SLscale, RR);
	}
}

static void ym2612_channelSetTotalLevel(LJ_YM2612_CHANNEL* const channelPtr, const int slot, const LJ_YM_UINT8 totalLevel)
{
	LJ_YM2612_SLOT* const slotPtr = &(channelPtr->slot[slot]);

	/* Total Level = Bits 0-6 */
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

	/* Detune = Bits 4-6, Multiple = Bits 0-3 */
	const LJ_YM_UINT8 detune = (detuneMult >> 4) & 0x7;
	const LJ_YM_UINT8 multiple = (detuneMult >> 0) & 0xF;

	slotPtr->detune = detune;
	/* multiple = xN except N=0 then it is x1/2 store it as x2 */
	slotPtr->multiple = (LJ_YM_UINT8)(multiple ? (multiple << 1) : 1);

	if (channelPtr->debugFlags & LJ_YM2612_DEBUG)
	{
		printf("SetDetuneMult channel:%d slot:%d detune:%d mult:%d\n", channelPtr->id, slot, detune, multiple);
	}
}

static void ym2612_computeBlockFnumKeyCode(int* const blockPtr, int* fnumPtr, int* keycodePtr, const int block_fnumMSB, const int fnumLSB)
{
	/* Block = Bits 3-5, Frequency Number MSB = Bits 0-2 (top 3-bits of frequency number) */
	const int block = (block_fnumMSB >> 3) & 0x7;
	const int fnumMSB = (block_fnumMSB >> 0) & 0x7;
	const int fnum = (fnumMSB << 8) + (fnumLSB & 0xFF);
	/* keycode = (block << 2) | (N4 << 1) | (N3 << 0) */
	/* N4 = Fnum Bit 11 */
	/* N3 = Fnum Bit 11 * (Bit 10 | Bit 9 | Bit 8) | !Bit 11 * Bit 10 * Bit 9 * Bit 8 */
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
	channelPtr->keycode = (LJ_YM_UINT8)(keycode);
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
	/* Set the start of wave */
	for (i = 0; i < LJ_YM2612_NUM_SLOTS_PER_CHANNEL; i++)
	{
		const int slot = i;
		LJ_YM2612_SLOT* const slotPtr = &(channelPtr->slot[slot]);

		if (slotOnOff & slotMask)
		{
			if (slotPtr->keyOn == 0)
			{
				slotPtr->keyOn = 1;
				slotPtr->omega = 0;
				slotPtr->fmInputDelta = 0;
				slotPtr->adsrState = LJ_YM2612_ATTACK;
				slotPtr->attenuationDBDelta = 0;
			}
		}
		else
		{
			if (slotPtr->keyOn == 1)
			{
				slotPtr->keyOn = 0;
				slotPtr->adsrState = LJ_YM2612_RELEASE;
			}
		}
		slotMask = slotMask << 1;
	}
}

static void ym2612_slotClear(LJ_YM2612_SLOT* const slotPtr)
{
	slotPtr->volume = 0;
	slotPtr->attenuationDB = 1023;
	slotPtr->attenuationDBDelta = 0;

	slotPtr->detune = 0;
	slotPtr->multiple = 0;
	slotPtr->omega = 0;
	slotPtr->omegaDelta = 0;
	slotPtr->keycode = 0;

	slotPtr->totalLevel = 0;
	slotPtr->sustainLevelDB = 0;

	slotPtr->fmInputDelta = 0;
	slotPtr->modulationOutput[0] = NULL;
	slotPtr->modulationOutput[1] = NULL;
	slotPtr->modulationOutput[2] = NULL;

	slotPtr->adsrState = LJ_YM2612_UNKNOWN;

	slotPtr->keyScale = 0;

	slotPtr->attackRate = 0;
	slotPtr->decayRate = 0;
	slotPtr->sustainRate = 0;
	slotPtr->releaseRate = 0;

	slotPtr->carrierOutputMask = 0x0;

	slotPtr->am = 0;

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

static void ym2612_partClear(LJ_YM2612_PART* const partPtr)
{
	int i;
	for (i = 0; i < LJ_YM2612_NUM_REGISTERS; i++)
	{
		partPtr->reg[i] = 0x0;
	}
	for (i = 0; i < LJ_YM2612_NUM_CHANNELS_PER_PART; i++)
	{
		LJ_YM2612_CHANNEL* const channelPtr = &(partPtr->channel[i]);
		ym2612_channelClear(channelPtr);
		channelPtr->id = partPtr->id*LJ_YM2612_NUM_CHANNELS_PER_PART+i;
	}
}

static void ym2612_egMakeData(LJ_YM2612_EG* const egPtr, LJ_YM2612* ym2612)
{
	egPtr->timer = 0;

	/* FM output timer is (clockRate/144) but update code is every sampleRate times of that in the use of this code */
	/* This is a counter in the units of FM output samples : which is (clockRate/sampleRate)/144 = ym2612->baseFreqScale */
	egPtr->timerAddPerOutputSample = (LJ_YM_UINT32)( (float)ym2612->baseFreqScale * (float)(1 << LJ_YM2612_EG_TIMER_NUM_BITS));

	/* Some argument on this value from looking at the forums: MAME = 3, Nemesis (on PAL MD) adamant it is 2.4375 */
	egPtr->timerPerUpdate = (LJ_YM_UINT32)( (float)LJ_YM2612_EG_TIMER_OUTPUT_PER_FM_SAMPLE * (float)(1 << LJ_YM2612_EG_TIMER_NUM_BITS));

	egPtr->counter = 0;

	printf("timerAdd:%d timerPerUpdate:%d\n", egPtr->timerAddPerOutputSample, egPtr->timerPerUpdate);
}

static void ym2612_egClear(LJ_YM2612_EG* const egPtr)
{
	egPtr->timer = 0;
	egPtr->timerAddPerOutputSample = 0;
	egPtr->timerPerUpdate = 0;
	egPtr->counter = 0;
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
	ym2612->channel2slotData[slot].keycode = (LJ_YM_UINT8)(keycode);
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
	const LJ_YM_UINT32 clock = ym2612->clockRate;
	const LJ_YM_UINT32 rate = ym2612->outputSampleRate;
	int i;
	float omegaScale;

	/* Fvalue = (144 * freq * 2^20 / masterClock) / 2^(B-1) */
	/* e.g. D = 293.7Hz F = 692.8 B=4 */
	/* freq = F * 2^(B-1) * baseFreqScale / (2^20) */
	/* freqScale = (chipClock/sampleRateOutput) / (144); */
	ym2612->baseFreqScale = ((float)clock / (float)rate) / 144.0f;

	for (i = 0; i < LJ_YM2612_FNUM_TABLE_NUM_ENTRIES; i++)
	{
		float freq = ym2612->baseFreqScale * (float)i;
		int intFreq;
		/* Include the sin table size in the (1/2^20) */
		freq = freq * (float)(1 << (LJ_YM2612_FREQ_BITS - (20 - LJ_YM2612_SIN_TABLE_BITS)));

		intFreq = (int)(freq);
		LJ_YM2612_fnumTable[i] = intFreq;
	}

	omegaScale = (2.0f * M_PI / LJ_YM2612_SIN_TABLE_NUM_ENTRIES);
	for (i = 0; i < LJ_YM2612_SIN_TABLE_NUM_ENTRIES; i++)
	{
		const float omega = omegaScale * (float)i;
		const float sinValue = (float)sin(omega);
		const int scaledSin = (int)(sinValue * (float)(1 << LJ_YM2612_SIN_SCALE_BITS));
		LJ_YM2612_sinTable[i] = scaledSin;
	}

	for (i = 0; i < LJ_YM2612_TL_TABLE_NUM_ENTRIES; i++)
	{
		/* From the docs each step is -0.75dB = x0.9172759 = 2^(-1/8) */
		const float value = -(float)i * (1.0f / 8.0f);
		const float tlValue = (float)pow(2.0f, value);
		const int scaledTL = (int)(tlValue * (float)(1 << LJ_YM2612_TL_SCALE_BITS));
		LJ_YM2612_tlTable[i] = scaledTL;
	}

	for (i = 0; i < LJ_YM2612_SL_TABLE_NUM_ENTRIES; i++)
	{
		/* From the docs Bit 0 = 3dB, Bit 1 = 6dB, Bit 2 = 12dB, Bit 3 = 24dB, each step = x0.707105 = 2^(-1/2) & 0xF = 93dB */
		const float value = -(float)i * (1.0f / 2.0f);
		const float slValue = (float)pow(2.0f, value);
		const LJ_YM_UINT32 scaledSL = (LJ_YM_UINT32)(slValue * (float)(1 << LJ_YM2612_SL_SCALE_BITS));
		LJ_YM2612_slTable[i] = scaledSL;
	}
	/* 0xF = 93dB */
	LJ_YM2612_slTable[15] = 0;

#if 0 
	/* DT1 = -3 -> 3 */
	for (i = 0; i < 4; i++)
	{
		int keycode;
		for (keycode = 0; keycode < 32; keycode++)
		{
			int detuneScaleTableValue = LJ_YM2612_detuneScaleTable[i*32+keycode];
			float detuneMultiple = (float)detuneScaleTableValue * (0.529f/10.0f);
			int detFPValue = detuneScaleTableValue * ((529 * (1<<16) / 10000));
			float fpValue = (float)detFPValue / (float)(1<<16);
			printf("detuneMultiple FD=%d keycode=%d %f (%d) %f\n", i, keycode, detuneMultiple, detuneScaleTableValue,fpValue);
		}
	}
#endif /* #if 0 */
	for (i = 0; i < LJ_YM2612_NUM_FDS; i++)
	{
		int keycode;
		for (keycode = 0; keycode < LJ_YM2612_NUM_KEYCODES; keycode++)
		{
			const int detuneScaleTableValue = LJ_YM2612_detuneScaleTable[i*LJ_YM2612_NUM_KEYCODES+keycode];
			/* This should be including baseFreqScale to put in the same units as fnumTable - so it can be additive to it */
			const float detuneRate = ym2612->baseFreqScale * (float)detuneScaleTableValue;
			/* Include the sin table size in the (1/2^20) */
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

	ym2612_egMakeData(&ym2612->eg, ym2612);
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
	ym2612->D07 = 0x0;
	ym2612->clockRate = 0;
	ym2612->outputSampleRate = 0;
	ym2612->ch2Mode = 0;
	ym2612->lfoEnable = 0;
	ym2612->lfoFreq = 0;

	ym2612_egClear(&ym2612->eg);

	for (i = 0; i < LJ_YM2612_NUM_PARTS; i++)
	{
		LJ_YM2612_PART* const partPtr = &(ym2612->part[i]);
		ym2612_partClear(partPtr);
		partPtr->id = i;
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
	
	/* Known specific registers (only need explicit listing for the system type parameters which aren't per channel settings */
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

	LJ_YM2612_validRegisters[LJ_RATE_SCALE_ATTACK_RATE] = 1;
	LJ_YM2612_REGISTER_NAMES[LJ_RATE_SCALE_ATTACK_RATE] = "RATE_SCALE_ATTACK_RATE";

	LJ_YM2612_validRegisters[LJ_AM_DECAY_RATE] = 1;
	LJ_YM2612_REGISTER_NAMES[LJ_AM_DECAY_RATE] = "AM_DECAY_RATE";

	LJ_YM2612_validRegisters[LJ_SUSTAIN_RATE] = 1;
	LJ_YM2612_REGISTER_NAMES[LJ_SUSTAIN_RATE] = "SUSTAIN_RATE";

	LJ_YM2612_validRegisters[LJ_SUSTAIN_LEVEL_RELEASE_RATE] = 1;
	LJ_YM2612_REGISTER_NAMES[LJ_SUSTAIN_LEVEL_RELEASE_RATE] = "SUSTAIN_LEVEL_RELEASE_RATE";

	LJ_YM2612_validRegisters[LJ_FEEDBACK_ALGO] = 1;
	LJ_YM2612_REGISTER_NAMES[LJ_FEEDBACK_ALGO] = "FEEDBACK_ALGO";

	LJ_YM2612_validRegisters[LJ_LR_AMS_PMS] = 1;
	LJ_YM2612_REGISTER_NAMES[LJ_LR_AMS_PMS] = "LR_AMS_PMS";

	/* For parameters mark all the associated registers as valid */
	for (i = 0; i < LJ_YM2612_NUM_REGISTERS; i++)
	{
		if (LJ_YM2612_validRegisters[i] == 1)
		{
			/* Found a valid register */
			/* 0x20 = system which is specific set of registers */
			/* 0x30-0x90 = settings per channel per slot (channel = bottom 2 bits of reg, slot = reg / 4) */
			/* 0xA0-0xB6 = settings per channel (channel = bottom 2 bits of reg) */
			if ((i >= 0x20) && (i < 0x30))
			{
			}
			if ((i >= 0x30) && (i <= 0x90))
			{
				/* 0x30-0x90 = settings per channel per slot (channel = bottom 2 bits of reg, slot = reg / 4) */
				/* ignore registers which would be channel=3 they are invalid */
				int regBase = (i >> 4) << 4;
				int j;
				for (j = 0; j < 16; j++)
				{
					const char* const regBaseName = LJ_YM2612_REGISTER_NAMES[regBase];
					const int reg = regBase+j;
					if ((reg & 0x3) != 0x3)
					{
						LJ_YM2612_validRegisters[reg] = 1;
						LJ_YM2612_REGISTER_NAMES[reg] = regBaseName;
					}
				}
			}
			if ((i >= 0xA0) && (i <= 0xB6))
			{
				/* 0xA0-0xB0 = settings per channel (channel = bottom 2 bits of reg) */
				/* 0xA8->0xAE - settings per slot (slot = bottom 2 bits of reg) */
				/* 0xA9 = slot 0, 0xAA = slot 1, 0xA8 = slot 2, 0xA2 = slot 3 */
				/* 0xAD = slot 0, 0xAE = slot 1, 0xAC = slot 2, 0xA6 = slot 3 */
				/* ignore registers which would be channel=3 they are invalid */
				int regBase = (i >> 2) << 2;
				int j;
				for (j = 0; j < 3; j++)
				{
					const char* const regBaseName = LJ_YM2612_REGISTER_NAMES[regBase];
					const int reg = regBase+j;
					if ((reg & 0x3) != 0x3)
					{
						LJ_YM2612_validRegisters[reg] = 1;
						LJ_YM2612_REGISTER_NAMES[reg] = regBaseName;
					}
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
		printf("ym2612:setRegister %s 0x%X\n", LJ_YM2612_REGISTER_NAMES[reg],data);
	}

	if (reg == LJ_LFO_EN_LFO_FREQ)
	{
		/* 0x22 Bit 3 = LFO enable, Bit 0-2 = LFO frequency */
		const LJ_YM_UINT8 lfoEnable = (data >> 3) & 0x1;
		const LJ_YM_UINT8 lfoFreq = (data >> 0) & 0x7;
		ym2612->lfoEnable = lfoEnable;
		ym2612->lfoFreq = lfoFreq;

		return LJ_YM2612_OK;
	}
	else if (reg == LJ_CH2_MODE_TIMER_CONTROL)
	{
		/* 0x27 Ch2 Mode = Bits 6-7, */
		/* 0x27 Reset B = Bit 5, Reset A = Bit 4, Enable B = Bit 3, Enable A = Bit 2, Start B = Bit 1, Start A = Bit 0 */
		ym2612->ch2Mode = (data >> 6) & 0x3;
	
		/*ym2612_setTimers(data & 0x1F) */

		return LJ_YM2612_OK;
	}
	else if (reg == LJ_DAC_EN)
	{
		/* 0x2A DAC Bits 0-7 */
		ym2612->dacEnable = (int)(~0 * ((data & 0x80) >> 7));
		return LJ_YM2612_OK;
	}
	else if (reg == LJ_DAC)
	{
		/* 0x2B DAC en = Bit 7 */
#if LJ_YM2612_DAC_SHIFT >= 0
		ym2612->dacValue = ((int)(data - 0x80)) << LJ_YM2612_DAC_SHIFT;
#else /* #if LJ_YM2612_DAC_SHIFT >= 0 */
		ym2612->dacValue = ((int)(data - 0x80)) >> -LJ_YM2612_DAC_SHIFT;
#endif /* #if LJ_YM2612_DAC_SHIFT >= 0 */
		return LJ_YM2612_OK;
	}
	else if (reg == LJ_KEY_ONOFF)
	{
		/* 0x28 Slot = Bits 4-7, Channel ID = Bits 0-2, part 0 or 1 is Bit 3 */
		const int channel = data & 0x03;
		if (channel != 0x03)
		{
			const LJ_YM_UINT8 slotOnOff = (data >> 4);
			const int partParam = (data & 0x4) >> 2;
			
			LJ_YM2612_CHANNEL* const channelPtr = &ym2612->part[partParam].channel[channel];

			ym2612_channelKeyOnOff(channelPtr, slotOnOff);
			if (ym2612->debugFlags & LJ_YM2612_DEBUG)
			{
				printf("LJ_KEY_ONFF part:%d channel:%d slotOnOff:0x%X data:0x%X\n", partParam, channel, slotOnOff, data);
			}
		}
		return LJ_YM2612_OK;
	}
	else if ((reg >= 0x30) && (reg <= 0x9F))
	{
		/* 0x30-0x90 = settings per channel per slot (channel = bottom 2 bits of reg, slotReg = reg / 4) */
		/* Use a table to convert from slotReg -> internal slot */
		const int channel = reg & 0x3;
		const int slotReg = (reg >> 2) & 0x3;
		const int slot = LJ_YM2612_slotTable[slotReg];
		const int regParameter = reg & 0xF0;

		LJ_YM2612_CHANNEL* const channelPtr = &ym2612->part[part].channel[channel];

		if (channel == 0x3)
		{
			fprintf(stderr, "ym2612:setRegister 0x%X %s 0x%X invalid channel 0x3\n", reg, LJ_YM2612_REGISTER_NAMES[reg], data);
			return LJ_YM2612_ERROR;
		}

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
				printf("LJ_TOTAL_LEVEL part:%d channel:%d slot:%d slotReg:%d data:0x%X\n", part, channel, slot, slotReg, data);
			}
			return LJ_YM2612_OK;
		}
		else if (regParameter == LJ_RATE_SCALE_ATTACK_RATE)
		{
			ym2612_channelSetKeyScaleAttackRate(channelPtr, slot, data);
			if (ym2612->debugFlags & LJ_YM2612_DEBUG)
			{
				printf("LJ_RATE_SCALE_ATTACK_RATE part:%d channel:%d slot:%d slotReg:%d data:0x%X\n", part, channel, slot, slotReg, data);
			}
			return LJ_YM2612_OK;
		}
		else if (regParameter == LJ_AM_DECAY_RATE)
		{
			ym2612_channelSetAMDecayRate(channelPtr, slot, data);
			if (ym2612->debugFlags & LJ_YM2612_DEBUG)
			{
				printf("LJ_AM_DECAY_RATE part:%d channel:%d slot:%d slotReg:%d data:0x%X\n", part, channel, slot, slotReg, data);
			}
			return LJ_YM2612_OK;
		}
		else if (regParameter == LJ_SUSTAIN_RATE)
		{
			ym2612_channelSetSustainRate(channelPtr, slot, data);
			if (ym2612->debugFlags & LJ_YM2612_DEBUG)
			{
				printf("LJ_SUSTAIN_RATE part:%d channel:%d slot:%d slotReg:%d data:0x%X\n", part, channel, slot, slotReg, data);
			}
			return LJ_YM2612_OK;
		}
		else if (regParameter == LJ_SUSTAIN_LEVEL_RELEASE_RATE)
		{
			ym2612_channelSetSustainLevelReleaseRate(channelPtr, slot, data);
			if (ym2612->debugFlags & LJ_YM2612_DEBUG)
			{
				printf("LJ_SUSTAIN_LEVEL_RELEASE_RATE part:%d channel:%d slot:%d slotReg:%d data:0x%X\n", part, channel, slot, slotReg, data);
			}
			return LJ_YM2612_OK;
		}
	}
	else if ((reg >= 0xA0) && (reg <= 0xB6))
	{
		/* 0xA0-0xB0 = settings per channel (channel = bottom 2 bits of reg) */
		const int channel = reg & 0x3;
		const int regParameter = reg & 0xFC;

		LJ_YM2612_CHANNEL* const channelPtr = &ym2612->part[part].channel[channel];

		if (channel == 0x3)
		{
			fprintf(stderr, "ym2612:setRegister 0x%X %s 0x%X invalid channel 0x3\n", reg, LJ_YM2612_REGISTER_NAMES[reg], data);
			return LJ_YM2612_ERROR;
		}

		if (regParameter == LJ_FREQLSB)
		{
			/* 0xA0-0xA2 Frequency number LSB = Bits 0-7 (bottom 8 bits of frequency number) */
			ym2612_channelSetFreqBlock(channelPtr, data);
			if (ym2612->debugFlags & LJ_YM2612_DEBUG)
			{
				printf("LJ_FREQLSB part:%d channel:%d data:0x%X\n", part, channel, data);
			}
			return LJ_YM2612_OK;
		}
		else if (regParameter == LJ_BLOCK_FREQMSB)
		{
			/* 0xA4-0xA6 Block = Bits 5-3, Frequency Number MSB = Bits 0-2 (top 3-bits of frequency number) */
			ym2612_channelSetBlockFnumMSB(channelPtr, data);
			if (ym2612->debugFlags & LJ_YM2612_DEBUG)
			{
				printf("LJ_BLOCK_FREQMSB part:%d channel:%d data:0x%X\n", part, channel, data);
			}
			return LJ_YM2612_OK;
		}
		else if (regParameter == LJ_CH2_FREQLSB)
		{
			/* 0xA8-0xAA Channel 2 Frequency number LSB = Bits 0-7 (bottom 8 bits of frequency number) */
			/* 0xA8-0xAA - settings per slot (slot = bottom 2 bits of reg) */
			/* 0xA9 = slot 0, 0xAA = slot 1, 0xA8 = slot 2, 0xA2 = slot 3 */
			/* Only on part 0 */
			if (part == 0)
			{
				/* It is only for part 0 and channel 2 so just store it globally */
				/* LJ_YM2612_slotTable is 0->0, 1->2, 2->1, 3->3 we want: 0->1, 1->0, 3->2 */
				/* which is LJ_YM2612_slotTable[slot+1]-1 : 0+1->2-1=1, 1+1->1-1=0, 2+1->3-1=2 */
				const int slotParameter = reg & 0x3;
				const LJ_YM_UINT8 slot = (LJ_YM_UINT8)(LJ_YM2612_slotTable[slotParameter+1] - 1U);

				ym2612_SetChannel2FreqBlock(ym2612, slot, data);
				if (ym2612->debugFlags & LJ_YM2612_DEBUG)
				{
					printf("LJ_CH2_FREQLSB slot:%d slotParameter:%d data:0x%X\n", slot, slotParameter, data);
				}
			}
			return LJ_YM2612_OK;
		}
		else if (regParameter == LJ_CH2_BLOCK_FREQMSB)
		{
			/* 0xAC-0xAE Block = Bits 5-3, Channel 2 Frequency Number MSB = Bits 0-2 (top 3-bits of frequency number) */
			/* 0xAC-0xAE - settings per slot (slot = bottom 2 bits of reg) */
			/* 0xAD = slot 0, 0xAE = slot 1, 0xAC = slot 2, 0xA6 = slot 3 */
			/* Only on part 0 */
			if (part == 0)
			{
				/* It is only for part 0 and channel 2 so just store it globally */
				/* LJ_YM2612_slotTable is 0->0, 1->2, 2->1, 3->3 we want: 0->1, 1->0, 3->2 */
				/* which is LJ_YM2612_slotTable[slot+1]-1 : 0+1->2-1=1, 1+1->1-1=0, 2+1->3-1=2 */
				const int slotParameter = reg & 0x3;
				const LJ_YM_UINT8 slot = (LJ_YM_UINT8)(LJ_YM2612_slotTable[slotParameter+1] - 1U);

				ym2612_SetChannel2BlockFnumMSB(ym2612, slot, data);
				if (ym2612->debugFlags & LJ_YM2612_DEBUG)
				{
					printf("LJ_CH2_BLOCK_FREQMSB slot:%d slotParameter:%d data:0x%X\n", slot, slotParameter, data);
				}
			}
			return LJ_YM2612_OK;
		}
		else if (regParameter == LJ_FEEDBACK_ALGO)
		{
			/* 0xB0-0xB2 Feedback = Bits 5-3, Algorithm = Bits 0-2 */
			ym2612_channelSetFeedbackAlgorithm(channelPtr, data);
			if (ym2612->debugFlags & LJ_YM2612_DEBUG)
			{
				printf("LJ_FEEDBACK_ALGO part:%d channel:%d data:0x%X\n", part, channel, data);
			}
			return LJ_YM2612_OK;
		}
		else if (regParameter == LJ_LR_AMS_PMS)
		{
			/* 0xB4-0xB6 Feedback = Left= Bit 7, Right= Bit 6, AMS = Bits 3-5, PMS = Bits 0-1 */
			ym2612_channelSetLeftRightAmsPms(channelPtr, data);
			if (ym2612->debugFlags & LJ_YM2612_DEBUG)
			{
				printf("LJ_LR_AMS_PMS part:%d channel:%d data:0x%X\n", part, channel, data);
			}
			return LJ_YM2612_OK;
		}
	}

	return LJ_YM2612_OK;
}

/*
* ////////////////////////////////////////////////////////////////////
* // 
* // External data & functions
* // 
* ////////////////////////////////////////////////////////////////////
*/

LJ_YM2612* LJ_YM2612_create(const int clockRate, const int outputSampleRate)
{
	LJ_YM2612* const ym2612 = malloc(sizeof(LJ_YM2612));
	if (ym2612 == NULL)
	{
		fprintf(stderr, "LJ_YM2612_create:malloc failed\n");
		return NULL;
	}

	ym2612_clear(ym2612);

	ym2612->clockRate = (LJ_YM_UINT32)(clockRate);
	ym2612->outputSampleRate = (LJ_YM_UINT32)(outputSampleRate);

	ym2612_makeData(ym2612);

	/* Set left, right bits to on by default at startup */

	/* Part 0 channel 0, 1, 2 */
	ym2612_setRegister(ym2612, 0, LJ_LR_AMS_PMS+0, (1<<7)|(1<<6));
	ym2612_setRegister(ym2612, 0, LJ_LR_AMS_PMS+1, (1<<7)|(1<<6));
	ym2612_setRegister(ym2612, 0, LJ_LR_AMS_PMS+2, (1<<7)|(1<<6));

	/* Part 1 channel 0, 1, 2 */
	ym2612_setRegister(ym2612, 1, LJ_LR_AMS_PMS+0, (1<<7)|(1<<6));
	ym2612_setRegister(ym2612, 1, LJ_LR_AMS_PMS+1, (1<<7)|(1<<6));
	ym2612_setRegister(ym2612, 1, LJ_LR_AMS_PMS+2, (1<<7)|(1<<6));

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
	const LJ_YM_UINT32 debugFlags = ym2612->debugFlags;

	int channel;
	int slot;

	int outputChannelMask = 0xFF;
	int channelMask = 0x1;

	if (ym2612 == NULL)
	{
		fprintf(stderr, "LJ_YM2612_generateOutput:ym2612 is NULL\n");
		return LJ_YM2612_ERROR;
	}

	/* Global state update - LFO, DAC, SSG */
	dacValue = (ym2612->dacValue & ym2612->dacEnable);
	if (debugFlags & LJ_YM2612_NODAC)
	{
		dacValue = 0x0;
	}
	dacValueLeft = (dacValue & ym2612->channels[5]->left);
	dacValueRight = (dacValue & ym2612->channels[5]->right);

	if (debugFlags & LJ_YM2612_ONECHANNEL)
	{
		/* channel starts at 0 */
		const int debugChannel =((debugFlags >> LJ_YM2612_ONECHANNEL_SHIFT) & LJ_YM2612_ONECHANNEL_MASK); 
		outputChannelMask = 1 << debugChannel;
	}

	/* For each cycle */
	/* Loop over channels updating them, mix them, then output them into the buffer */
	for (sample = 0; sample < numCycles; sample++)
	{
		int mixedLeft = 0;
		int mixedRight = 0;
		short outL = 0;
		short outR = 0;
	
		/* DAC enable blocks channel 5 */
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
				int deltaPhi;
				int phi;
				int scaledSin;
				int slotOutput;

				const int OMEGA = slotPtr->omega;
				const int slotPhi = (OMEGA >> LJ_YM2612_FREQ_BITS);
		
				/* For normal mode or channels that aren't channel 2 or for slot 3 */
				if ((ym2612->ch2Mode == 0) || (channel != 2) || (slot == 3))
				{
					/* Take the slot frequency from the channel frequency */
					slotFnum = channelFnum;
					slotBlock = channelBlock;
					slotKeycode = channelKeycode;
					omegaDirty = channelOmegaDirty;
				}
				else
				{
					/* Channel 2 and it is in special mode with fnum, block (keycode) per slot for slots 0, 1, 2 */
					slotFnum = ym2612->channel2slotData[slot].fnum;
					slotBlock = ym2612->channel2slotData[slot].block;
					slotKeycode = ym2612->channel2slotData[slot].keycode;
				}

				/* Compute the omega delta value */
				if ((slotPtr->omegaDirty == 1) || (omegaDirty == 1))
				{
					ym2612_slotComputeOmegaDelta(slotPtr, slotFnum, slotBlock, slotKeycode, debugFlags);
					slotPtr->keycode = slotKeycode;
					slotPtr->omegaDirty = 0;
				}
	
				/* Slot 0 feedback */
				if (slot == 0)
				{
					/* Average of the last 2 samples */
					slotPtr->fmInputDelta = (channelPtr->slot0Output[0] + channelPtr->slot0Output[1]);
					if (channelPtr->feedback != 0)
					{
						/* Average = >> 1 */
						/* From docs: Feedback is 0 = + 0, 1 = +PI/16, 2 = +PI/8, 3 = +PI/4, 4 = +PI/2, 5 = +PI, 6 = +2 PI, 7 = +4 PI */
						/* fmDelta is currently in -1->1 which is mapped to 0->2PI (and correct angle units) by the deltaPhi scaling */
						/* feedback 6 = +1 in these units, so feedback 6 maps to 2^0: << feedback then >> 6 */
						/* deltaPhi scaling has an implicit x4 so /4 here: >> 2 */

						/* Results in: >> (1+6+2-feedback) >> (9-feedback) */
						slotPtr->fmInputDelta = slotPtr->fmInputDelta >> (9 - channelPtr->feedback);
					}
					else
					{
						slotPtr->fmInputDelta = 0;
					}
				}
				/* Phi needs to have the fmInputDelta added to it to make algorithms work */
#if LJ_YM2612_DELTA_PHI_SCALE >= 0
				deltaPhi = (slotPtr->fmInputDelta >> LJ_YM2612_DELTA_PHI_SCALE);
#else /* #if LJ_YM2612_DELTA_PHI_SCALE >= 0 */
				deltaPhi = (slotPtr->fmInputDelta << -LJ_YM2612_DELTA_PHI_SCALE);
#endif /* #if LJ_YM2612_DELTA_PHI_SCALE >= 0 */

				phi = slotPhi + deltaPhi;

				scaledSin = LJ_YM2612_sinTable[phi & LJ_YM2612_SIN_TABLE_MASK];

				slotOutput = (slotPtr->volume * scaledSin) >> LJ_YM2612_SIN_SCALE_BITS;

				/* Scale by TL (total level = 0->1) */
				slotOutput = (slotOutput * slotPtr->totalLevel) >> LJ_YM2612_TL_SCALE_BITS;

				/* Add this output onto the input of the slot in the algorithm */
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
					/* Save the slot 0 output (the last 2 outputs are saved), used as input for the slot 0 feedback */
					channelPtr->slot0Output[0] = channelPtr->slot0Output[1];
					channelPtr->slot0Output[1] = slotOutput;
				}

				/* Keep within +/- 1 */
				slotOutput = LJ_YM2612_CLAMP_VOLUME(slotOutput);

				carrierOutput = slotOutput & slotPtr->carrierOutputMask;
				channelOutput += carrierOutput;

				if (debugFlags & LJ_YM2612_DEBUG)
				{
					if (slotOutput != 0)
					{
						printf("Channel:%d Slot:%d slotOutput:%d carrierOutput:%d\n", channelPtr->id, slot, slotOutput, carrierOutput);
					}
				}
				/* Zero out the fmInputDelta so it is ready to be used on the next update */
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

			/* Keep within +/- 1 */
			channelOutput = LJ_YM2612_CLAMP_VOLUME(channelOutput);

			mixedLeft += channelOutput & channelPtr->left;
			mixedRight += channelOutput & channelPtr->right;

			channelPtr->omegaDirty = 0;

			channelMask = (channelMask << 1);
		}

		mixedLeft += dacValueLeft;
		mixedRight += dacValueRight;

		/* Keep within +/- 1 */
		/* mixedLeft = LJ_YM2612_CLAMP_VOLUME(mixedLeft); */
		/* mixedRight = LJ_YM2612_CLAMP_VOLUME(mixedRight); */

#if LJ_YM2612_OUTPUT_SCALE >= 0
		mixedLeft = mixedLeft >> LJ_YM2612_OUTPUT_SCALE;
		mixedRight = mixedRight >> LJ_YM2612_OUTPUT_SCALE;
#else /* #if LJ_YM2612_OUTPUT_SCALE >= 0 */
		mixedLeft = mixedLeft << -LJ_YM2612_OUTPUT_SCALE;
		mixedRight = mixedRight << -LJ_YM2612_OUTPUT_SCALE;
#endif /* #if LJ_YM2612_OUTPUT_SCALE >= 0 */

		outL = (LJ_YM_INT16)mixedLeft;
		outR = (LJ_YM_INT16)mixedRight;

		outputLeft[sample] = outL;
		outputRight[sample] = outR;

		/* Update volumes and frequency position */
		for (channel = 0; channel < LJ_YM2612_NUM_CHANNELS_TOTAL; channel++)
		{
			LJ_YM2612_CHANNEL* const channelPtr = ym2612->channels[channel];

			for (slot = 0; slot < LJ_YM2612_NUM_SLOTS_PER_CHANNEL; slot++)
			{
				LJ_YM2612_SLOT* const slotPtr = &(channelPtr->slot[slot]);

				slotPtr->omega += slotPtr->omegaDelta;
			}
		}

		/* EG update */
		ym2612->eg.timer += ym2612->eg.timerAddPerOutputSample;
		while (ym2612->eg.timer >= ym2612->eg.timerPerUpdate)
		{
			/* Update the envelope */
			ym2612->eg.timer -= ym2612->eg.timerPerUpdate;
			ym2612->eg.counter++;

			for (channel = 0; channel < LJ_YM2612_NUM_CHANNELS_TOTAL; channel++)
			{
				LJ_YM2612_CHANNEL* const channelPtr = ym2612->channels[channel];

				for (slot = 0; slot < LJ_YM2612_NUM_SLOTS_PER_CHANNEL; slot++)
				{
					LJ_YM2612_SLOT* const slotPtr = &(channelPtr->slot[slot]);

					ym2612_slotUpdateEG(slotPtr, ym2612->eg.counter);

					slotPtr->volume = LJ_YM2612_CLAMP_VOLUME(slotPtr->volume);
					if (slotPtr->volume < 0)
					{
						slotPtr->volume = 0;
					}
				}
			}
		}
	}

	return LJ_YM2612_OK;
}

/* To set a value on the data pins D0-D7 - use for register address and register data value */
/* call setAddressPinsCSRDWRA1A0 to copy the data in D0-D7 to either the register address or register data setting */
LJ_YM2612_RESULT LJ_YM2612_setDataPinsD07(LJ_YM2612* const ym2612, LJ_YM_UINT8 data)
{
	if (ym2612 == NULL)
	{
		fprintf(stderr, "LJ_YM2612_setDataPinsD07:ym2612 is NULL\n");
		return LJ_YM2612_ERROR;
	}
	ym2612->D07 = data;
	return LJ_YM2612_OK;
}

/* To write data must have: notCS = 0, notRD = 1, notWR = 0 */
/* A1 = 0, A0 = 0 : D0-D7 is latched as the register address for part 0 i.e. Genesis memory address 0x4000 */
/* A1 = 0, A0 = 1 : D0-D7 is written to the latched register address for part 0 i.e. Genesis memory address 0x4001 */
/* A1 = 1, A0 = 0 : D0-D7 is latched as the register address for part 1 i.e. Genesis memory address 0x4002 */
/* A1 = 1, A0 = 1 : D0-D7 is written to the latched register address for part 1 i.e. Genesis memory address 0x4003 */
LJ_YM2612_RESULT LJ_YM2612_setAddressPinsCSRDWRA1A0(LJ_YM2612* const ym2612, LJ_YM_UINT8 notCS, LJ_YM_UINT8 notRD, LJ_YM_UINT8 notWR, 
																						 				LJ_YM_UINT8 A1, LJ_YM_UINT8 A0)
{
	if (ym2612 == NULL)
	{
		fprintf(stderr, "LJ_YM2612_setAddressPinsCSRDWRA1A0:ym2612 is NULL\n");
		return LJ_YM2612_ERROR;
	}
	if (notCS == 1)
	{
		/* !CS != 0 is inactive mode on chip e.g ignored */
		return LJ_YM2612_OK;
	}
	if ((notRD == 0) && (notWR == 1))
	{
		/* !RD = 0 !WR = 1 means a read not a write */
		/* Read not supported yet */
		fprintf(stderr, "LJ_YM2612_setAddressPinsCSRDWRA1A0:read is not supported\n");
		return LJ_YM2612_ERROR;
	}
	if ((notRD == 1) && (notWR == 0))
	{
		/* !RD = 1 !WR = 0 means a write */
		/* A0 = 0 means D0-7 is register address, A1 is part 0/1 */
		if ((A0 == 0) && ((A1 == 0) || (A1 == 1)))
		{
			ym2612->regAddress = ym2612->D07;
			ym2612->slotWriteAddr = A1;
			return LJ_YM2612_OK;
		}
		/* A0 = 1 means D0-7 is register data, A1 is part 0/1 */
		if ((A0 == 1) && ((A1 == 0) || (A1 == 1)))
		{
			if (ym2612->slotWriteAddr == A1)
			{
				const LJ_YM_UINT8 reg = ym2612->regAddress;
				const LJ_YM_UINT8 part = A1;
				const LJ_YM_UINT8 data = ym2612->D07;
				LJ_YM2612_RESULT result = ym2612_setRegister(ym2612, part, reg, data);
				/* Hmmmm - how does the real chip work */
				ym2612->slotWriteAddr = 0xFF;
				return result;
			}
		}
	}

	fprintf(stderr, "LJ_YM2612_setAddressPinsCSRDWRA1A0:unknown combination of pins !CS:%d !RD:%d !WR:%d A1:%d A0:%d\n",
					notCS, notRD, notWR, A1, A0);

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

