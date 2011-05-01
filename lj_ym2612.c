#include "lj_ym2612.h"

#include <stdio.h>
#include <malloc.h>

////////////////////////////////////////////////////////////////////
// 
// Internal data & functions
// 
////////////////////////////////////////////////////////////////////

typedef enum LJ_YM2612_REGISTERS LJ_YM2612_REGISTERS;
typedef struct LJ_YM2612_PORT LJ_YM2612_PORT;

#define LJ_YM2612_NUM_REGISTERS (0xB6+1)
#define LJ_YM2612_NUM_PORTS (2)

// Random guess - needs to be in sync with how the envelope amplitude works also to match up
#define DAC_SHIFT (6)

enum LJ_YM2612_REGISTERS {
		LJ_KEY_ONOFF = 0x28,
		LJ_DAC = 0x2A,
		LJ_DAC_EN = 0x2B,
		LJ_FREQLSB = 0xA0,
		LJ_BLOCK_FREQMSB = 0xA4,
		LJ_FEEDBACK_ALGO = 0xB0,
		LJ_LR_AMS_PMS = 0xB4,
};

static const char* LJ_YM2612_REGISTER_NAMES[LJ_YM2612_NUM_REGISTERS];

struct LJ_YM2612_PORT
{
	LJ_YM_UINT8 regs[LJ_YM2612_NUM_REGISTERS];
};

struct LJ_YM2612
{
	LJ_YM2612_PORT ports[LJ_YM2612_NUM_PORTS];
	LJ_YM_UINT8 validRegisters[LJ_YM2612_NUM_REGISTERS];

	LJ_YM_INT16 dacValue;
	LJ_YM_UINT16 dacEnable;
};

static void ym2612_portClear(LJ_YM2612_PORT* const port)
{
	int i;
	for (i=0; i<LJ_YM2612_NUM_REGISTERS; i++)
	{
		port->regs[i] = 0x0;
	}
}

static void ym2612_clear(LJ_YM2612* const ym2612)
{
	int i;

	ym2612->dacValue = 0;
	ym2612->dacEnable = 0x0;

	for (i=0; i<LJ_YM2612_NUM_PORTS; i++)
	{
		LJ_YM2612_PORT* const port = &(ym2612->ports[i]);
		ym2612_portClear(port);
	}
	for (i=0; i<LJ_YM2612_NUM_REGISTERS; i++)
	{
		ym2612->validRegisters[i] = 0;
		LJ_YM2612_REGISTER_NAMES[i] = "UNKNOWN";
	}
	
	//Known specific registers (only need explicit listing for the system type parameters which aren't per channel settings
	ym2612->validRegisters[LJ_KEY_ONOFF] = 1;
	LJ_YM2612_REGISTER_NAMES[LJ_KEY_ONOFF] = "KEY_ONOFF";

	ym2612->validRegisters[LJ_DAC] = 1;
	LJ_YM2612_REGISTER_NAMES[LJ_DAC] = "DAC";

	ym2612->validRegisters[LJ_DAC_EN] = 1;
	LJ_YM2612_REGISTER_NAMES[LJ_DAC_EN] = "DAC_EN";

	ym2612->validRegisters[LJ_FREQLSB] = 1;
	LJ_YM2612_REGISTER_NAMES[LJ_FREQLSB] = "FREQ(LSB)";

	ym2612->validRegisters[LJ_BLOCK_FREQMSB] = 1;
	LJ_YM2612_REGISTER_NAMES[LJ_BLOCK_FREQMSB] = "BLOCK_FREQ(MSB)";

	ym2612->validRegisters[LJ_FEEDBACK_ALGO] = 1;
	LJ_YM2612_REGISTER_NAMES[LJ_FEEDBACK_ALGO] = "FEEDBACK_ALGO";

	ym2612->validRegisters[LJ_LR_AMS_PMS] = 1;
	LJ_YM2612_REGISTER_NAMES[LJ_LR_AMS_PMS] = "LR_AMS_PMS";

	//For parameters mark all the associated registers as valid
	for (i=0; i<LJ_YM2612_NUM_REGISTERS; i++)
	{
		if (ym2612->validRegisters[i] == 1)
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
					ym2612->validRegisters[regBase+j] = 1;
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
	return ym2612;
}

LJ_YM2612_RESULT LJ_YM2612_destroy(LJ_YM2612* const ym2612)
{
	int result = LJ_YM2612_ERROR;

	return result;
}

LJ_YM2612_RESULT LJ_YM2612_setRegister(LJ_YM2612* const ym2612, LJ_YM_UINT8 port, LJ_YM_UINT8 reg, LJ_YM_UINT8 data)
{
	LJ_YM_UINT32 parameter = 0;
	//LJ_YM_UINT32 channel = 0;
	//LJ_YM_UINT32 slot = 0;

	if (ym2612 == NULL)
	{
		fprintf(stderr, "LJ_YM2612_setRegister:ym2612 is NULL\n");
		return LJ_YM2612_ERROR;
	}

	if (port >= LJ_YM2612_NUM_PORTS)
	{
		fprintf(stderr, "LJ_YM2612_setRegister:invalid port:%d max:%d \n",port, LJ_YM2612_NUM_PORTS);
		return LJ_YM2612_ERROR;
	}

	// convert from register to parameter
	parameter = (reg & 0xF0) >> 4;

	if (ym2612->validRegisters[reg] == 0)
	{
		fprintf(stderr, "LJ_YM2612_setRegister:unknown register:0x%X port:%d data:0x%X\n", reg, port, data);
		return LJ_YM2612_ERROR;
	}

	ym2612->ports[port].regs[reg] = data;
	printf( "LJ_YM2612:setRegister %s 0x%X\n", LJ_YM2612_REGISTER_NAMES[reg],data);

	if (reg == LJ_DAC_EN)
	{
		ym2612->dacEnable = 0xFFFF * ((data & 0x80) >> 7);
	}
	else if (reg == LJ_DAC)
	{
		ym2612->dacValue = ((LJ_YM_INT16)(data - 0x80)) << DAC_SHIFT;
		//printf( "dacValue:%d data:0x%X\n", ym2612->dacValue,data);
	}

	return LJ_YM2612_OK;
}

LJ_YM2612_RESULT LJ_YM2612_write(LJ_YM2612* const ym2612, LJ_YM_UINT16 address, LJ_YM_UINT8 data)
{
	//Need to store address - then use address on data write (1 address per port)
	return LJ_YM2612_ERROR;
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
	//dacValue = ym2612->dacValue;

	//For each cycle
	//Loop over channels updating them, mix them, then output them into the buffer
	for (sample=0; sample < numCycles; sample++)
	{
		LJ_YM_INT16 mixedLeft = 0;
		LJ_YM_INT16 mixedRight = 0;

		mixedLeft += dacValue;
		mixedRight += dacValue;

		outputLeft[sample] = mixedLeft;
		outputRight[sample] = mixedRight;
	}

	return LJ_YM2612_OK;
}
