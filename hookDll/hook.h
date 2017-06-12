#ifndef _HOOK_H
#define _HOOK_H

#include <windows.h>
#include <stdint.h>
#include "MinHook.h"

typedef unsigned char uchar;
typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int uint32;

/*inline void HOOK(uintptr_t func, replacement, orig){
    do {
        MH_STATUS ret = MH_CreateHook(func, replacement, (void **)orig);
        if(ret != MH_OK){
            fprintf(stderr, "[MH] Hook failed for 0x%lx\n", func);
            break;
        }
        MH_EnableHook(func);
    } while(0)
}*/


#define HOOK(func, replacement, orig) \
do { \
	MH_STATUS ret = MH_CreateHook(func, replacement, (void **)orig); \
	if(ret != MH_OK){ \
		fprintf(stderr, "[MH] Hook failed for %p\n", func); \
		break; \
	} \
	MH_EnableHook(func); \
} while(0)

#define PTRADD(a, b)(a = (void *)((uintptr_t)a + (uintptr_t)b))

#define OUTPUT_REPORT_SIZE		64
#define FEATURE_REPORT_SIZE		(OUTPUT_REPORT_SIZE)
#define CMD_SIZE				(OUTPUT_REPORT_SIZE)

typedef struct
{
    union
    {
        uchar  cmd;             // CMD_xxx code
        struct
        {
            uchar  cmd;         // CMD_ERASE
            uchar  flash;       // PRIMARY_FLASH or SECONDARY_FLASH
            uint16 address;     // Any address in any sector
        } erase;
        struct
        {
            uchar  cmd;         // CMD_READ or CMD_WRITE
            uchar  flash;       // PRIMARY_FLASH or SECONDARY_FLASH
            uint16 address;     // Target xdata address
            uint16 nBytes;      // Num bytes to read/write
        } rw;
        struct
        {
            uchar cmd;          // CMD_SET_xxx
            uchar page;         // Desired page register value
            uchar vm;           // Desired VM register value
        } setRegs;
        struct
        {
            uchar cmd;          // CMD_GET_STATUS
            uchar currentCmd;   // Returns current command being processed
            uchar page;         // Returns page register value
            uchar vm;           // Returns VM register value
            uchar ret;          // Return value from flash routine
            uchar checkSum;     // Check sum for write commands
        } status;
        uchar buffer[CMD_SIZE];
    } u;
} MCU_CMD, *PMCU_CMD;

/////////////////// Typedefs
typedef struct
{
    // The Microsoft HID driver expects a prefix report ID byte
    unsigned char reportID;
    MCU_CMD report;

} REPORT_BUF, *PREPORT_BUF;

INT hookmain(void);

#endif // _HOOK_H
