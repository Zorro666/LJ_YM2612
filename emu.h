#ifndef EMU_HH
#define EMU_HH

#include <stdio.h>

typedef unsigned short stream_sample_t;

typedef struct device_t
{
	int* machine;
} device_t;

typedef unsigned int UINT32;
typedef unsigned short UINT16;
typedef unsigned char UINT8;

typedef int INT32;
typedef short INT16;
typedef char INT8;

typedef int TIME_TYPE;

/*#define INLINE inline */
#define INLINE 

#define auto_alloc_clear(m,t) allocClear(sizeof(t))
#define auto_free(m,p) allocFree(p)
#define logerror printf

void* allocClear(unsigned int size);
void allocFree(void* v);

#endif /* #ifndef EMU_HH */

