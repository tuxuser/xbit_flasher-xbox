#ifndef _XBIT_H
#define _XBIT_H

#include "hidapi/hidapi.h"

#define CMD_RESET				0x01
#define CMD_ERASE				0x02
#define CMD_WRITE				0x03
#define CMD_READ				0x04
#define CMD_GET_STATUS			0x05
#define CMD_SET_REGS			0x06 // Unused ?
#define CMD_SET_PAGE			0x07
#define CMD_SET_VM				0x08

#define PRIMARY_FLASH			0
#define SECONDARY_FLASH			1

#define OUTPUT_REPORT_SIZE		64 
#define FEATURE_REPORT_SIZE		(OUTPUT_REPORT_SIZE) 
#define CMD_SIZE				(OUTPUT_REPORT_SIZE) 

/////////// Custom Start
#define STATUS_BUS_FREE			0x01
#define STATUS_BUS_ATTACHED		0x02
#define STATUS_WRITE_PROTECT	0x80

#define TOTAL_BLOCKS			0x20 // 32
#define BLOCK_SIZE				0x10000 // 64 kbytes

#define DEVICE_MFG 				L"ST Microelectronics"
#define DEVICE_PRODUCT 			L"DK3200 Evaluation Board"

#define BANK_LAYOUT_COUNT		6
#define BANKS_MAX				6

// Sizes are given in kbytes
int bank_layout[BANK_LAYOUT_COUNT][BANKS_MAX] = {
	// 0   1     2    3    4    5
	{512,  512,  256, 256, 256, 256},	// Layout 1
	{1024, 256,  256, 256, 256, 0},		// Layout 2
	{1024, 512,  256, 256, 0,   0},		// Layout 3
	{1024, 512,  512, 0,   0,   0},		// Layout 4
	{1024, 1024, 0,   0,   0,   0},		// Layout 5
	{2048, 0,    0,   0 ,  0,   0},		// Layout 6	
};

char bios_select_switches[] = {
	0x00,	// Bios 0 - Off Off Off
	0x01,	// Bios 1 - On  Off Off
	0x02,	// Bios 2 - Off On  Off
	0x03,	// Bios 3 - On  On  Off
	0x04,	// Bios 4 - Off Off On
	0x05	// Bios 5 - On  Off On
};
/////////// Custom End

typedef unsigned char uchar;
typedef unsigned char uint8;
typedef unsigned short uint16; 
typedef unsigned int uint32; 


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


typedef struct   
{   
    // The Microsoft HID driver expects a prefix report ID byte   
    unsigned char reportID;   
    MCU_CMD report;   
   
} REPORT_BUF, *PREPORT_BUF;


class XbitFlasher
{
public:
	int memory_layout_id;
	XbitFlasher();
	~XbitFlasher();
	bool OpenDevice();
	bool CloseDevice();
	hid_device *GetHandle();

	bool Format(int layout);
	bool EraseBank(int bank);
	bool FlashBank(int bank, uchar *input_data, int data_length);
	bool ReadBank(int bank, uchar *output_data, int *num_bytes_read);
	bool VerifyBank(int bank, uchar *input_data, int data_length);

	void PrintMemoryBankLayout();
	void PrintBankSelection();
	void PrintUsage(const char* argv0);

private:
	hid_device *handle;
	bool device_initialized;
	REPORT_BUF statusBuf;

	int InternalRead(PREPORT_BUF output);
	int InternalWrite(PREPORT_BUF input);

	bool IsDeviceInitialized();
	bool IsValidStatus();
	uchar GetCurrentCommand();
	uchar GetMemoryLayout();
	uchar GetVMState();
	bool IsDeviceReady();
	bool IsDeviceBusFree();
	bool IsDeviceBusAttached();
	bool IsDeviceWriteprotected();

	bool GetStatus();
	bool Reset();
	bool SetVM(uchar vm);
	bool GetBus();
	bool ReleaseBus();
	bool SetPage(int layout_id);
	bool ReadFlash(uchar flash, uchar sector, uint16 offset, uchar *buffer, uint16 nBytes);
	bool WriteFlash(uchar flash, uchar sector, uint16 offset, uchar *buffer, uint16 nBytes);
	bool EraseBlock(int flash, int sector);

	uchar CalculateBlockIndexForOffset(int offset);
	int GetStartblockForBank(int layout, int bank);
	int GetSizeForBank(int layout, int bank);
};

#endif