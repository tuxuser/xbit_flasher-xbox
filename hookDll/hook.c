#include <stdio.h>
#include <ctype.h>
#include "hook.h"
#include "ntdll.h"

typedef INT (__cdecl *READFLASH)(BYTE, BYTE, USHORT, BYTE *, USHORT);
typedef INT (__cdecl *WRITEFLASH)(BYTE, BYTE, USHORT, BYTE *, USHORT);
typedef INT (__cdecl *ERASEBLOCK)(INT, INT);
typedef INT (*GETBUS)(VOID);
typedef INT (*RELEASEBUS)(VOID);
typedef INT (*RESET)(VOID);
typedef INT (*SETPAGE)(VOID);
typedef INT (__cdecl *READSTATUS)(PREPORT_BUF);

typedef BOOL (WINAPI *WRITEFILE)(HANDLE, LPCVOID, DWORD, LPDWORD, LPOVERLAPPED);
typedef INT (WINAPI *MESSAGEBOXA)(HWND, LPCSTR, LPCSTR, UINT);

uintptr_t ModuleBase = (uintptr_t)NULL;

// Pointer for calling original MessageBoxW.
READFLASH oReadFlash = (READFLASH)NULL;
WRITEFLASH oWriteFlash = (WRITEFLASH)NULL;
ERASEBLOCK oEraseBlock = (ERASEBLOCK)NULL;
GETBUS oGetBus = (GETBUS)NULL;
RELEASEBUS oReleaseBus = (RELEASEBUS)NULL;
RESET oReset = (RESET)NULL;
SETPAGE oSetPage = (SETPAGE)NULL;
READSTATUS oReadStatus = (READSTATUS)NULL;

READFLASH ReadFlash = (READFLASH)0x14A0;
WRITEFLASH WriteFlash = (WRITEFLASH)0x1660;
ERASEBLOCK EraseBlock = (ERASEBLOCK)0x1C30;
GETBUS GetBus = (GETBUS)0x3380;
RELEASEBUS ReleaseBus = (RELEASEBUS)0x3430;
RESET Reset = (RESET)0x1BB0;
SETPAGE SetPage = (SETPAGE)0x46D0;
READSTATUS ReadStatus = (READSTATUS)0x1410;

// Original ptrs go here
WRITEFILE oWriteFile = (WRITEFILE)NULL;
MESSAGEBOXA oMessageBoxA = (MESSAGEBOXA)NULL;

// Hooked ptrs go here
MESSAGEBOXA pMessageBoxA = (MESSAGEBOXA)NULL;
WRITEFILE pWriteFile = (WRITEFILE)NULL;

// Detour function which overrides MessageBoxW.
INT DetourReadFlash(BYTE flash, BYTE sector, USHORT offset, BYTE *buffer, USHORT nBytes)
{
    printf("ReadFlash, flash: %i, sector: %i, offset: %04X, nBytes: %04X\n", flash, sector, offset, nBytes);
    INT ret = oReadFlash(flash, sector, offset, buffer, nBytes);
    return ret;
}

INT DetourWriteFlash(BYTE flash, BYTE sector, USHORT offset, BYTE *buffer, USHORT nBytes)
{
    printf("WriteFlash, flash: %i, sector: %i, offset: 0x%04X, nBytes: 0x%04X\n", flash, sector, offset, nBytes);
    INT ret = oWriteFlash(flash, sector, offset, buffer, nBytes);
    INT round = 1;
    while (TRUE){
        usleep(1000);
        if(offset == 0)
            oEraseBlock(flash, sector);
        //usleep(1000);
        ret = oWriteFlash(flash, sector, offset, buffer, nBytes);
        //usleep(1000);
        if(ret){
            printf("Looks like it worked...\n");
            break;
        }
    }
    return ret;
}

INT DetourEraseBlock(INT flash, INT sector)
{
    printf("EraseBlock, flash: %i, sector: %i\n", flash, sector);
    INT ret = oEraseBlock(flash, sector);
    return ret;
}

INT DetourGetBus(VOID)
{
    printf("GetBus\n");
    INT ret = oGetBus();
    return ret;
}

INT DetourReleaseBus(VOID)
{
    printf("ReleaseBus\n");
    INT ret = oReleaseBus();
    return ret;
}

INT DetourReset(VOID)
{
    printf("Reset\n");
    INT ret = oReset();
    return ret;
}

INT DetourSetPage(VOID)
{
    printf("SetPage\n");
    INT ret = oSetPage();
    return ret;
}

INT DetourReadStatus(PREPORT_BUF reportBuf)
{
    //printf("ReadStatus\n");
    INT ret = oReadStatus(reportBuf);
    return ret;
}

BOOL WINAPI DetourWriteFile(HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite, LPDWORD lpNumberOfBytesWritten, LPOVERLAPPED lpOverlapped)
{
    BOOL ret = oWriteFile(hFile, lpBuffer, nNumberOfBytesToWrite, lpNumberOfBytesWritten, lpOverlapped);
    return ret;
}

INT WINAPI DetourMessageBoxA(HWND hWindow, LPCSTR lpText, LPCSTR lpCaption, UINT uType)
{
    // Skip those annoying OK MessageBoxes
    //printf("%s\n", lpText);
    INT ret = 1;
    if(uType != 48){
        ret = oMessageBoxA(hWindow, lpText, lpCaption, uType);
    }
    return ret;
}

/* Create a new CMD window for our purposes */
void SetStdOutToNewConsole()
{
    // Allocate a console for this app
    AllocConsole();
    freopen("CONIN$", "r", stdin);
    freopen("CONOUT$", "w", stdout);
    freopen("CONOUT$", "w", stderr);
}

void adjustOffsets()
{
    PTRADD(ReadFlash, ModuleBase);
    PTRADD(WriteFlash, ModuleBase);
    PTRADD(EraseBlock, ModuleBase);
    PTRADD(GetBus, ModuleBase);
    PTRADD(ReleaseBus, ModuleBase);
    PTRADD(Reset, ModuleBase);
    PTRADD(SetPage, ModuleBase);
    PTRADD(ReadStatus, ModuleBase);
}

static BOOL INITIALIZED = FALSE;
INT hookmain()
{
    if(INITIALIZED)
		return FALSE;
	INITIALIZED = TRUE;
    SetStdOutToNewConsole();

    ModuleBase = (uintptr_t)GetModuleHandle(NULL);
    adjustOffsets();
    if (MH_Initialize() != MH_OK)
    {
        printf("Initializing MinHook failed!\n");
        return 1;
    }
    pMessageBoxA = (MESSAGEBOXA)GetProcAddress(GetModuleHandleA("user32.dll"), "MessageBoxA");
    pWriteFile = (WRITEFILE)GetProcAddress(GetModuleHandleA("kernel32.dll"), "WriteFile");
    HOOK(ReadFlash, &DetourReadFlash, &oReadFlash);
    HOOK(WriteFlash, &DetourWriteFlash, &oWriteFlash);
    HOOK(EraseBlock, &DetourEraseBlock, &oEraseBlock);
    HOOK(GetBus, &DetourGetBus, &oGetBus);
    HOOK(ReleaseBus, &DetourReleaseBus, &oReleaseBus);
    HOOK(Reset, &DetourReset, &oReset);
    HOOK(SetPage, &DetourSetPage, &oSetPage);
    HOOK(ReadStatus, &DetourReadStatus, &oReadStatus);
    HOOK(pMessageBoxA, &DetourMessageBoxA, &oMessageBoxA);
    HOOK(pWriteFile, &DetourWriteFile, &oWriteFile);

    printf("X-BIT fixer... v1.0\n");
    return 0;
}
