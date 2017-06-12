X-Bit (Xbit) Modchip Flasher (XBIT v1.0)
--
A small tool to help flash the XBit modchip (manufactured by DMS3 Team)
Works on Mac, Linux and Windows by using the hidapi library.

Keep in mind: The modchip seems pretty unstable itself while flashing, expect an annoying procedure... 

Based on WinApp DK3200 USB DEMO (by ST Microelectronics): http://www.codeforge.com/article/173459

Bonus
--
In the subdir 'hookDll' I included the source code for an injectable DLL for the original X-Bit Windows flashing tool (XBIT_v1.0.exe).
It hooks the essential parts of the device interaction and makes it easy to modify the routines.

Caveat here is - the original flashing tool only seems to run on Windows 2K/XP (somewhat) properly...

You need to have Minhook for compiling the DLL: https://github.com/TsudaKageyu/minhook


Why no GUI?
--
Cause nobody will use this tool anyways :D
