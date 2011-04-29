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

#define LJ_YM2612_NUM_PARAMETERS (0xB+1)
#define LJ_YM2612_NUM_REGISTERS (0xB6+1)
#define LJ_YM2612_NUM_PORTS (2)

enum LJ_YM2612_PARAMETERS {
		LJ_SYSTEM = 0x2,
		LJ_LR_AMS_PMS = 0xB,
};

enum LJ_YM2612_REGISTERS {
		LJ_KEY_ONOFF = 0x28,
};

static const char* LJ_YM2612_PARAMETER_NAMES[LJ_YM2612_NUM_PARAMETERS];
static const char* LJ_YM2612_REGISTER_NAMES[LJ_YM2612_NUM_REGISTERS];

struct LJ_YM2612_PORT
{
	char regs[LJ_YM2612_NUM_REGISTERS];
};

struct LJ_YM2612
{
	LJ_YM2612_PORT ports[LJ_YM2612_NUM_PORTS];
	unsigned char validRegisters[LJ_YM2612_NUM_REGISTERS];
	unsigned char validParameters[LJ_YM2612_NUM_PARAMETERS];
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
	for (i=0; i<LJ_YM2612_NUM_PARAMETERS; i++)
	{
		ym2612->validParameters[i] = 0;
		LJ_YM2612_PARAMETER_NAMES[i] = "UNKNOWN";
	}
	//Known specific registers (only need explicit listing for the system type parameters which aren't per channel settings
	ym2612->validRegisters[LJ_KEY_ONOFF] = 1;
	LJ_YM2612_REGISTER_NAMES[LJ_KEY_ONOFF] = "KEY_ONOFF";

	//Known parameters (system and per channel hence per port
	ym2612->validParameters[LJ_SYSTEM] = 1;
	LJ_YM2612_PARAMETER_NAMES[LJ_SYSTEM] = "SYSTEM";

	ym2612->validParameters[LJ_LR_AMS_PMS] = 1;
	LJ_YM2612_PARAMETER_NAMES[LJ_LR_AMS_PMS] = "LR_AMS_PMS";

	//For parameters mark all the associated registers as valid
	for (i=0; i<LJ_YM2612_NUM_PARAMETERS; i++)
	{
		if (ym2612->validParameters[i] == 1)
		{
			//Found a valid parameter
			//0x2 = system which is specific set of registers
			//0x3-0x9 = settings per channel per slot (channel = bottom 2 bits of reg, slot = reg / 4)
			//0xA-0xB = settings per channel (channel = bottom 2 bits of reg)
			if (i == 0x2)
			{
			}
			if ((i >= 0x3) && (i <= 0x9))
			{
			}
			if ((i == 0xA) || (i == 0xB))
			{
				int regBase = (i<<4);
				int j;
				for (j=0; j<7; j++)
				{
					const char* const regBaseName = LJ_YM2612_PARAMETER_NAMES[i];
					ym2612->validRegisters[regBase+j] = 1;
					LJ_YM2612_REGISTER_NAMES[regBase+j] = regBaseName;
				}
			}
			if (i == 0xA) 
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

LJ_YM2612_RESULT LJ_YM2612_setRegister(LJ_YM2612* const ym2612, unsigned char port, unsigned char reg, unsigned char data)
{
	unsigned int parameter = 0;
	//unsigned int channel = 0;
	//unsigned int slot = 0;

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
	if (ym2612->validParameters[parameter] == 0)
	{
		fprintf(stderr, "LJ_YM2612_setRegister:unknown parameter:0x%X register:0x%X port:%d data:0x%X\n", parameter, reg, port, data);
		return LJ_YM2612_ERROR;
	}

	if (ym2612->validRegisters[reg] == 0)
	{
		fprintf(stderr, "LJ_YM2612_setRegister:unknown register:0x%X port:%d data:0x%X\n", reg, port, data);
		return LJ_YM2612_ERROR;
	}

	ym2612->ports[port].regs[reg] = data;
	printf( "YM2612:setRegister %s 0x%X\n", LJ_YM2612_REGISTER_NAMES[reg],data);

	return LJ_YM2612_OK;
}
