/*********************************************************************************************************
 * X-Bit (Xbit) Modchip Flasher (XBIT v1.0)
 *********************************************************************************************************/

#ifdef WIN32
#include <windows.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <cstring>
#include <unistd.h>
#include <time.h>

// IMPORTANT: Define single byte packing
#pragma pack(1)
#include "xbit.h"

/////////////////// Macros
#define min(x,y) (((x)<(y))?(x):(y))

// For converting 8051 big endian to x86 little endian format   
#define SWAP_UINT16(x) ((((x)&0xff00)>>8) | (((x)&0x00ff)<<8))   
#define SWAP_UINT32(x) ((((x)&0xff000000)>>24) | (((x)&0x00ff0000)>>8) | (((x)&0x0000ff00)<<8) | (((x)&0x000000ff)<<24)) 

/////////////////// Constants
#define MAX_STR				255
#define MAX_SECTOR_SIZE		0x8000 // half block

#define INPUT_REPORT_SIZE	64

#define ST_VENDOR_ID        0x0483   
#define ST_PRODUCT_ID       0x0000

#define DEBUG

////////////////// Debug helper
void print_bytes(PREPORT_BUF input, int length)
{
	int i;
	printf("reportID: %i, cmd: %02X, buf: ", input->reportID, input->report.u.cmd);
	for (i=0; i < length; i++)
		printf("%02X ", input->report.u.buffer[i]);
	printf("\n");
}

///////////////// Class
XbitFlasher::XbitFlasher()
{
	// Initialize the hidapi library
	hid_init();
	this->device_initialized = false;
}

XbitFlasher::~XbitFlasher()
{
	// Finalize the hidapi library
	hid_exit();
}

bool XbitFlasher::OpenDevice()
{
	int res;
	wchar_t wstr[MAX_STR];
	// Open the device using the VID, PID,
	// and optionally the Serial number.
	this->handle = hid_open(ST_VENDOR_ID, ST_PRODUCT_ID, NULL);
	if(!handle){
		printf("ERROR: Failed to open hid device!\n");
		return false;
	}

	res = hid_get_manufacturer_string(handle, wstr, MAX_STR);
	if(wcsncmp(wstr, DEVICE_MFG, wcslen(DEVICE_MFG))){
		printf("ERROR: Invalid manufacturer string: %ls\n", wstr);
		CloseDevice();
		return false;
	}

	// Product String: DK3200 Evaluation Board
	res = hid_get_product_string(handle, wstr, MAX_STR);
	if(wcsncmp(wstr, DEVICE_PRODUCT, wcslen(DEVICE_PRODUCT))){
		printf("ERROR: Invalid product string: %ls", wstr);
		CloseDevice();
		return false;
	}

	if(!GetStatus()){
		printf("ERROR: Failed to initially get status from modchip. Please retry\n");
		return false;
	}
	this->memory_layout_id = GetMemoryLayout();
	this->device_initialized = true;
	return true;
}

bool XbitFlasher::CloseDevice()
{
	Reset();
	if(handle)
		hid_close(handle);
	this->device_initialized = false;
	return true;
}

hid_device *XbitFlasher::GetHandle()
{
	return this->handle;
}

bool XbitFlasher::IsDeviceInitialized()
{
	return this->device_initialized;
}

bool XbitFlasher::IsValidStatus()
{
	return (this->statusBuf.reportID == 0 && this->statusBuf.report.u.status.cmd == CMD_GET_STATUS);
}

uchar XbitFlasher::GetCurrentCommand()
{
	return this->statusBuf.report.u.status.currentCmd;
}

uchar XbitFlasher::GetMemoryLayout()
{
	return this->statusBuf.report.u.status.page;
}

uchar XbitFlasher::GetVMState()
{
	return this->statusBuf.report.u.status.vm;
}

bool XbitFlasher::IsDeviceReady()
{
	return (GetCurrentCommand() == 0);
}

bool XbitFlasher::IsDeviceBusFree()
{
	return ((GetVMState() & STATUS_BUS_FREE) == STATUS_BUS_FREE);
}

bool XbitFlasher::IsDeviceBusAttached()
{
	return ((GetVMState() & STATUS_BUS_ATTACHED) == STATUS_BUS_ATTACHED);
}

bool XbitFlasher::IsDeviceWriteprotected()
{
	return ((GetVMState() & STATUS_WRITE_PROTECT) == STATUS_WRITE_PROTECT);
}

int XbitFlasher::InternalRead(PREPORT_BUF output)
{
	int res;
	/* NOTE: Dont use hid_read */
	res = hid_get_feature_report(this->handle, (unsigned char*)output, sizeof(REPORT_BUF));
#ifdef DEBUG
	if(res == sizeof(REPORT_BUF))
		print_bytes(output, OUTPUT_REPORT_SIZE);
#endif
	return res;
}

int XbitFlasher::InternalWrite(PREPORT_BUF input)
{
	int res;
	res = hid_write(this->handle, (unsigned char*)input, sizeof(REPORT_BUF));
	usleep(8000);
#ifdef DEBUG
	if(res == sizeof(REPORT_BUF))
		print_bytes(input, OUTPUT_REPORT_SIZE);
#endif
	return res;
}

bool XbitFlasher::GetStatus()
{
    REPORT_BUF reportBuf;   
    memset(&reportBuf, 0, sizeof(REPORT_BUF));   
   
    // Send command

    reportBuf.reportID     = 0;   
    reportBuf.report.u.cmd = CMD_GET_STATUS; 

	if(InternalWrite(&reportBuf) != sizeof(REPORT_BUF)){
		printf("Error sending CMD_STATUS command.\n");
		return false;
	}

	memset(&statusBuf, 0x00, sizeof(REPORT_BUF));
	if(InternalRead(&statusBuf) != sizeof(REPORT_BUF)){
		printf("Error reading CMD_GET_STATUS reply.\n");
		return false;
	}

	return true;
}

bool XbitFlasher::Reset()
{
   REPORT_BUF reportBuf;   
   memset(&reportBuf, 0, sizeof(REPORT_BUF));   
   
   // Send command

   reportBuf.reportID     = 0;   
   reportBuf.report.u.cmd = CMD_RESET; 

	if(InternalWrite(&reportBuf) != sizeof(REPORT_BUF)){
		printf("Error sending CMD_RESET command.\n");  
		return false;
	}

	return true;
}

bool XbitFlasher::SetVM(uchar vm)
{
	REPORT_BUF reportBuf;   
	memset(&reportBuf, 0, sizeof(REPORT_BUF));   
   
    // Send command

    reportBuf.reportID             = 0;   
    reportBuf.report.u.setRegs.cmd = CMD_SET_VM;   
    reportBuf.report.u.setRegs.vm  = vm;  

	if(InternalWrite(&reportBuf) != sizeof(REPORT_BUF)){
		printf("Error sending CMD_SET_VM command.\n");  
		return false;
	}

	return true;
}

bool XbitFlasher::GetBus()
{
	return this->SetVM(1);
}

bool XbitFlasher::ReleaseBus()
{
	return this->SetVM(0);
}

bool XbitFlasher::SetPage(int layout_id)
{
    REPORT_BUF reportBuf;   
    memset(&reportBuf, 0, sizeof(REPORT_BUF));

    // Send command

    reportBuf.reportID              = 0;   
    reportBuf.report.u.setRegs.cmd  = CMD_SET_PAGE;   
    reportBuf.report.u.setRegs.page = layout_id & 0xFF;

	if(InternalWrite(&reportBuf) != sizeof(REPORT_BUF)){
		printf("Error sending CMD_SET_PAGE command.\n");
		return false;
	}

	return true;
}

bool XbitFlasher::ReadFlash(uchar flash, uchar sector, uint16 offset, uchar *buffer, uint16 nBytes)
{
	time_t t1, t2;  
    REPORT_BUF reportBuf;   
    memset(&reportBuf, 0, sizeof(REPORT_BUF));   
   
    if (!nBytes)   
    {   
        printf("Invalid count of bytes to read.\n");   
        return false;   
    }   
   
   	// Original DK3200 way:
    // Convert sector offset to xdata address
    //uint16 address = OffsetToAddress(flash, sector, offset);
    
    // XBIT way:
    // The "address"-field is relative to sector, e.g. it defines address INSIDE the sector
    // The "flash" field sets the sector
   
    // Send command   
   
    reportBuf.reportID            = 0;   
    reportBuf.report.u.cmd        = CMD_READ;   
    //reportBuf.report.u.rw.flash   = flash;
    //reportBuf.report.u.rw.address = SWAP_UINT16(address); 
    reportBuf.report.u.rw.flash   = sector;
    reportBuf.report.u.rw.address = SWAP_UINT16(offset);  
    reportBuf.report.u.rw.nBytes  = SWAP_UINT16(nBytes);    
   
	if(InternalWrite(&reportBuf) != sizeof(REPORT_BUF))
    {   
        printf("ERROR: Error sending CMD_READ command.\n");    
        return false;   
    }   
   
    time(&t1);
    // Read data   
   
    uint16 cbRemaining = nBytes;   
    uint16 cbTemp = cbRemaining;   
    while (cbRemaining)   
    {   
        if (InternalRead(&reportBuf) != sizeof(REPORT_BUF))   
        {   
            printf("ERROR: Error reading CMD_READ reply.\n");   
            return false;   
        }
   
        // Skip 0 command byte at start of report buffer   
   
        uint16 cbData = min(cbRemaining, CMD_SIZE - 1);   
        memcpy(buffer, reportBuf.report.u.buffer + 1, cbData);   
        buffer += cbData;   
        cbRemaining -= cbData;   
   
        if ((cbTemp/100) != (cbRemaining/100))   
        {   
            printf("Reading flash: %d bytes remaining\n", (cbTemp/100)*100);
            cbTemp = cbRemaining;   
        }   
    }
   
    time(&t2);
    printf("Reading Flash is done.\n");
    printf(" Time consumed %f seconds.\n", difftime(t1, t2));  
    return true;   
}


bool XbitFlasher::WriteFlash(uchar flash, uchar sector, uint16 offset, uchar *buffer, uint16 nBytes)
{
    REPORT_BUF reportBuf;   
    memset(&reportBuf, 0, sizeof(REPORT_BUF));   
   
    if (!nBytes)   
    {   
        printf("Invalid count of bytes to write.\n");   
        return false;   
    }   

    // Calculate checksum   

    uchar checkSum = 0;   
    for (int i = 0; i < nBytes; i++)   
    {   
        checkSum += buffer[i];
    }   
   
   	// Original DK3200 way:
    // Convert sector offset to xdata address
    //uint16 address = OffsetToAddress(flash, sector, offset);
    
    // XBIT way:
    // The "address"-field is relative to sector, e.g. it defines address INSIDE the sector
    // The "flash" field sets the sector

    // Send command   
   
    reportBuf.reportID            = 0;   
    reportBuf.report.u.cmd        = CMD_WRITE;   
    //reportBuf.report.u.rw.flash   = flash;
    //reportBuf.report.u.rw.address = SWAP_UINT16(address);
    reportBuf.report.u.rw.flash   = sector;
    reportBuf.report.u.rw.address = SWAP_UINT16(offset);
    reportBuf.report.u.rw.nBytes  = SWAP_UINT16(nBytes);   
   
    if(InternalWrite(&reportBuf) != sizeof(REPORT_BUF))
    {   
        printf("Error sending CMD_WRITE command.\n");     
        return false;   
    }
   	usleep(2000);
    // Write data   
   
    uint16 cbRemaining = nBytes;   
    uint16 cbTemp = cbRemaining;   
    while (cbRemaining)   
    {   
        uint16 cbData = min(cbRemaining, CMD_SIZE - 1);   
   
        reportBuf.reportID = 0;   
        reportBuf.report.u.cmd = 0;   
        memcpy(reportBuf.report.u.buffer + 1, buffer, cbData);   
   
        if (InternalWrite(&reportBuf) != sizeof(REPORT_BUF))
        {   
            printf("Error writing data.\n");     
            return false;   
        }
   
        buffer += cbData;   
        cbRemaining -= cbData;   
   
        // Update display on every 100 byte boundary   
   
        if ((cbTemp/100) != (cbRemaining/100))   
        {   
            printf("Writing flash: %d bytes remaining\n", (cbTemp/100)*100);   
            cbTemp = cbRemaining;   
        }   
    }   
   
    // Verify check sum   
   
    if (GetStatus())
    {   
        if (this->statusBuf.report.u.status.checkSum != checkSum)   
        { 
			printf("Write operation failed: the checksum calculated from\na readback does not match the checksum for the data written.\n");
			// NOTE: Seems like XBIT does not report back with checksum?
            //return false;   
        }   
    }   
     
    return true;
}

bool XbitFlasher::EraseBlock(int flash, int sector)
{
   REPORT_BUF reportBuf;   
   memset(&reportBuf, 0, sizeof(REPORT_BUF));   
   
   // Original DK3200 way:
   // Convert sector address 0 to xdata address   
   //uint16 address = OffsetToAddress(flash, sector, 0);

   // XBIT way:
   // The address field is always 0
   // Sector is defined by "flash"-byte   

   // Send command   
   
   reportBuf.reportID               = 0;   
   reportBuf.report.u.erase.cmd     = CMD_ERASE;
   //reportBuf.report.u.erase.address = SWAP_UINT16(address);   
   //reportBuf.report.u.erase.flash   = (uchar) flash;
   reportBuf.report.u.erase.address = 0;
   reportBuf.report.u.erase.flash   = (uchar) sector;

	if(InternalWrite(&reportBuf) != sizeof(REPORT_BUF)){
		printf("Error sending CMD_ERASE command.\n");   
		return false;
	}

	return true;
}

bool XbitFlasher::Format(int layout)
{
	int res = 0;
	if(layout < 1 || layout > BANK_LAYOUT_COUNT){
		printf("Invalid layout %i, valid: %i-%i\n", layout, 1, BANK_LAYOUT_COUNT);
		return false;
	}

	if(IsDeviceWriteprotected()){
		printf("Modchip is write-protected?!?! Try resetting it by replugging USB cable..\n");
		return false;
	}

	res = GetBus();
	if(!res){
		printf("Failed to get bus\n");
		return false;
	}

	printf("Formatting...\n");
	for (int i=0; i < TOTAL_BLOCKS; i++){
		res = EraseBlock(0, i);
		if(!res){
			printf("Failed to erase block %i\n", i);
			return false;
		}
	}

	res = SetPage(layout);
	if(!res){
		printf("Failed to set memory layout, id: %i\n", layout);
		return false;
	}

	this->memory_layout_id = layout;
	res = ReleaseBus();
	if(!res){
		printf("Failed to release bus\n");
		return false;
	}

	printf("Format finished!\n");
	return true;
}

bool XbitFlasher::EraseBank(int bank)
{
	int res = 0;
	int bank_size = GetSizeForBank(this->memory_layout_id, bank);
	int current_block = GetStartblockForBank(this->memory_layout_id, bank);
	int block_count = CalculateBlockIndexForOffset(bank_size);

	if(IsDeviceWriteprotected()){
		printf("Modchip is write-protected?!?! Try resetting it by replugging USB cable..\n");
		return false;
	}

	res = GetBus();
	if(!res){
		printf("Failed to get bus\n");
		return false;
	}
	printf("Erasing bank %i...\n", bank);
	for (int i = current_block; i < (current_block + block_count); i++){
		res = EraseBlock(0, i);
		if(!res){
			printf("Failed to erase block %i\n", i);
			return false;
		}
	}

	res = ReleaseBus();
	if(!res){
		printf("Failed to release bus\n");
		return false;
	}

	return true;
}

bool XbitFlasher::FlashBank(int bank, uchar *input_data, int data_length)
{
	int res = 0;
	int bank_size = GetSizeForBank(this->memory_layout_id, bank);
	int start_block = GetStartblockForBank(this->memory_layout_id, bank);
	int block_count = CalculateBlockIndexForOffset(bank_size);

	if(bank_size != data_length){
		printf("BIOS size %i does not match bank size %i\n", data_length, bank_size);
		return false;
	}

	if(IsDeviceWriteprotected()){
		printf("Modchip is write-protected?!?! Try resetting it by replugging USB cable..\n");
		return false;
	}

	res = EraseBank(bank);
	if(!res){
		printf("Failed to erase bank\n");
		return false;
	}

	sleep(2);

	res = GetBus();
	if(!res){
		printf("Failed to get bus\n");
		return false;
	}

	int offset = 0;
	for(int block = 0; block < block_count; ++block) {
		for(int sector = 0; sector < (BLOCK_SIZE / MAX_SECTOR_SIZE); sector++) {
			offset = (block * BLOCK_SIZE) + (sector * MAX_SECTOR_SIZE);
			printf("Writing block: %i, sector %i @ 0x%08X\n", start_block + block, sector, offset);
			res = WriteFlash(0, start_block + block, sector * MAX_SECTOR_SIZE, &input_data[offset], MAX_SECTOR_SIZE);
			while(!res){
				// Awesome hack: repeat until success....
				res = WriteFlash(0, start_block + block, sector * MAX_SECTOR_SIZE, &input_data[offset], MAX_SECTOR_SIZE);
				if(res)
					printf("Successs...\n");
			}
		}
	}

	res = ReleaseBus();
	if(!res){
		printf("Failed to release bus\n");
		return false;
	}
	return true;
}

bool XbitFlasher::ReadBank(int bank, uchar *output_data, int *num_bytes_read)
{
	int res;
	int bank_size = GetSizeForBank(this->memory_layout_id, bank);
	int start_block = GetStartblockForBank(this->memory_layout_id, bank);
	int block_count = CalculateBlockIndexForOffset(bank_size);

	if(IsDeviceWriteprotected()){
		printf("Modchip is write-protected?!?! Try resetting it by replugging USB cable..\n");
		return false;
	}

	res = GetBus();
	if(!res){
		printf("Failed to get bus\n");
		return false;
	}

	int offset = 0;
	*num_bytes_read = 0;
	for(int block = 0; block < block_count; ++block) {
		printf("Reading block %i\n", start_block + block);
		for(int sector = 0; sector < (BLOCK_SIZE / MAX_SECTOR_SIZE); sector++) {
			offset = (block * BLOCK_SIZE) + (sector * MAX_SECTOR_SIZE);
			printf("Reading sector %i @ %08X\n", sector, offset);
			res = ReadFlash(0, start_block + block, sector * MAX_SECTOR_SIZE, &output_data[offset], MAX_SECTOR_SIZE);
			if(!res){
				printf("Failed to read data!\n");
				return false;
			}
			*num_bytes_read += MAX_SECTOR_SIZE;
		}
	}

	res = ReleaseBus();
	if(!res){
		printf("Failed to release bus\n");
		return false;
	}
	return true;
}

bool XbitFlasher::VerifyBank(int bank, uchar *input_data, int data_length)
{
	int res;
	uchar buf[2 * 1024 * 1024];
	int bank_size = GetSizeForBank(this->memory_layout_id, bank);
	int bytes_read;

	if(bank_size != data_length){
		printf("Passed data length does not match bank size!\n");
		return false;
	}

	res = ReadBank(bank, buf, &bytes_read);
	if(!res){
		printf("Failed to read bank for verification!\n");
		return false;
	}

	if(bytes_read != bank_size){
		printf("Did not read enough data from bank for verification\n");
		return false;
	}

	if(memcmp(buf, input_data, data_length)){
		printf("Verificaton failed: Data mismatch!\n");
		return false;
	}
	printf("Success! Data matches!\n");
	return true;
}

uchar XbitFlasher::CalculateBlockIndexForOffset(int offset)
{
	if(offset == 0){
		return 0;
	}
	else if(offset % BLOCK_SIZE){
		printf("Error: Passed offset does not align with block size!!!\n");
		return -1;
	}
	return offset / BLOCK_SIZE;
}

int XbitFlasher::GetStartblockForBank(int layout, int bank)
{
	int offset = 0;
	for(int i=1; i <= bank; i++){
		offset += GetSizeForBank(layout, i);
	}
	return CalculateBlockIndexForOffset(offset);
}

int XbitFlasher::GetSizeForBank(int layout, int bank)
{
	return bank_layout[layout-1][bank-1] * 1024;
}

void XbitFlasher::PrintMemoryBankLayout()
{
	int size;
	printf("Memory Bank Configurations:\n");
	for(int i=1; i <= BANK_LAYOUT_COUNT; i++){
		printf("Layout %i: ", i);
		for(int j=1; j <= BANKS_MAX; j++){
			size = GetSizeForBank(i, j);
			if (!size)
				continue;
			printf("Bios#%i [%ibytes] ", j, size / 1024);
		}
		printf("\n");
	}	
}

void XbitFlasher::PrintBankSelection()
{
	int mask;
	printf("DIP switch positions:\n");
	printf("          1    2    3\n");
	for(int i=0; i < BANKS_MAX; i++){
		mask = bios_select_switches[i];
		printf("Bios %i: %s - %s - %s\n", i + 1,
			mask & 1 ? "ON " : "OFF",
			mask & 2 ? "ON " : "OFF",
			mask & 4 ? "ON " : "OFF");
	}
}

void XbitFlasher::PrintUsage(const char* argv0)
{
	printf("X-Bit (Xbit) Modchip Flasher (XBIT v1.0)\n");
	printf("Usage: %s [mode] [layout] [bank] [filename]\n", argv0);
	printf("  e.g. %s w 5 3 bios.bin\n", argv0);
	printf("Modes:\n");
	printf("(r)ead, (w)rite, (v)erify, (f)ormat\n");
	printf("NOTE: To format the chip, only layout param is required\n");
	printf("X-Bit Properties:\n\n");
	PrintMemoryBankLayout();
	PrintBankSelection();
}

bool LoadFile(const char *filename, uchar *data, int *size)
{
	FILE *f = NULL;

	f = fopen(filename, "rb");
	if(f == NULL){
		printf("Failed to open BIOS binary!\n");
		return false;
	}

	fseek(f, 0, SEEK_END);
	*size = ftell(f);
	rewind(f);

	if(*size > (2 * 1024 * 1024)){
		printf("BIOS size if bigger than 2MB\n");
		return false;
	}
	else if(*size % BLOCK_SIZE){
		printf("BIOS does not align with Block size\n");
		return false;
	}

	fread(data, *size, 1, f);
	fclose(f);
	return true;
}

bool SaveFile(const char *filename, uchar *data, int size)
{
	int written = 0;
	FILE *f = NULL;

	f = fopen(filename, "wb");
	if(f == NULL){
		printf("Failed to open BIOS binary!\n");
		return false;
	}

	if(size > (2 * 1024 * 1024)){
		printf("BIOS size if bigger than 2MB\n");
		return false;
	}
	else if(size % BLOCK_SIZE){
		printf("BIOS does not align with Block size\n");
		return false;
	}

	written = fwrite(data, size, 1, f);
	fclose(f);
	return (written == 1);
}

int main(int argc, char* argv[])
{
	char mode;
	int res, size=0, layout=0, bank=0, bytes_read=0;
	char *endPtr, *filename;
	uchar bios_buf[2 * 1024 * 1024]; // 2MB

	XbitFlasher flasher = XbitFlasher();

	////////// Parse Cmdline

	if(argc > 1)
		mode = argv[1][0];

	// For format switch (f) only the layout parameter is needed
	if((argc < 5 && mode != 'f') || (mode == 'f' && argc < 3)){ // argc < 3
		flasher.PrintUsage(argv[0]);
		res = 1;
		goto exit_e0;
	}
	
	layout = strtol(argv[2], &endPtr, 10);
	if (!*argv[2] || *endPtr || layout < 1 || layout > BANK_LAYOUT_COUNT){
		printf("Invalid layout parameter supplied. Valid: %i-%i\n", 1, BANK_LAYOUT_COUNT);
		res = 2;
		goto exit_e0;
	}
	printf("Chosen Layout: %i\n", layout);

	if(mode != 'f') {
		bank = strtol(argv[3], &endPtr, 10);
		if (!*argv[3] || *endPtr || bank < 1 || bank > BANKS_MAX){
			printf("Invalid bank parameter supplied. Valid: %i-%i\n", 1, BANKS_MAX);
			res = 2;
			goto exit_e0;
		}
		printf("Chose BIOS Bank: %i\n", bank);
		filename = argv[4];
		printf("BIOS file: %s\n", filename);
	}


	// First interaction with the modchip
	res = flasher.OpenDevice();
	if(!res){
		printf("Failed to open HID USB connection to X-Bit\n");
		res = 3;
		goto exit_e0;
	}

	if((mode == 'r' || mode == 'w' || mode == 'v') && flasher.memory_layout_id != layout){
		printf("Cannot execute read/write/verify action -> Supplied layout does not match with modchip layout!\n");
		printf("Either it\'s an error or you did not format the chip initially with the correct layout\n");
		printf("If error: Replug USB and run this tool again!\n");
		res = 5;
		goto exit_e1;
	}


	// Do stuff
	switch(mode){
		case 'r': // READ BANK
			printf("Reading bank %i to %s\n", bank, filename);
			res = flasher.ReadBank(bank, bios_buf, &bytes_read);
			if(!res){
				printf("Reading flash failed!\n");
				res = 6;
				goto exit_e1;
			}
			printf("Read %i bytes..\n", bytes_read);
			res = SaveFile(filename, bios_buf, bytes_read);
			if(!res){
				printf("Saving file %s failed!\n", filename);
				res = 6;
				goto exit_e1;
			}
			break;
		case 'w': // WRITE BANK
			printf("Writing %s to bank %i\n", filename, bank);
			res = LoadFile(filename, bios_buf, &size);
			if(!res){
				printf("Loading file %s failed!\n", filename);
				res = 6;
				goto exit_e1;
			}
			res = flasher.FlashBank(bank, bios_buf, size);
			if(!res){
				printf("Writing flash failed!\n");
				res = 6;
				goto exit_e1;
			}
			break;
		case 'v': // VERIFY BANK
			printf("Verifying bank %i with %s\n", bank, filename);
			res = LoadFile(filename, bios_buf, &size);
			if(!res){
				printf("Loading file %s failed!\n", filename);
				res = 6;
				goto exit_e1;
			}
			res = flasher.VerifyBank(bank, bios_buf, size);
			if(!res){
				printf("Verification failed!\n");
				res = 6;
				goto exit_e1;
			}
			break;
		case 'f': // FORMAT CHIP
			printf("Formatting chip for layout: %i\n", layout);
			res = flasher.Format(layout);
			if(!res){
				printf("Formatting chip failed!\n");
				res = 6;
				goto exit_e1;
			}
			break;
		default:
			printf("Invalid option chose!\n");
			flasher.PrintUsage(argv[0]);
			res = 4;
			goto exit_e1;
	}

exit_e1:
	flasher.CloseDevice();
exit_e0:
	return res;
}
