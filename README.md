ATTINY fuse repair using HVSP via FT232RL

This circuit allows to reset fuses of ATtiny AVRs to factory default values even in case ISP programming has been disabled by configuring RESET pin as I/O pin in Fuses.

On AVR microcontrollers, you can set the fuses to use the reset pin as a normal I/O pin, this gives you another I/O pin. But this also disables its ability to reset the chip, as this is required to program the chip using ISP programming.
This circuit uses the High Voltage Serial Programming method (HVSP) to reset the fuses to factory default.

This circuit is reduced to the bare minimum. The NPN Transistor is used to apply the +12V Programming Voltage to the target AVR. 

High voltage must be at least 11.5V, don't work with 11V !!
use 5v to 12v booster or 12v battery. if u have a source with voltage > 12.5V use a 12v regulator.


High-voltage Serial Programming Algorithm:
-----------------------------------------
  Atmel ATtiny data sheet: Chapter Memory Programming -> High Voltage Serial Programming 
  1. Set Prog_enable pins (SDI, SII, SDO, SCI) “0”, RESET pin to 0V.
  2. Wait 20 - 60 us, and apply 11.5 - 12.5V to RESET pin.
  3. Keep the Prog_enable pins unchanged for at least 10 us after the High-voltage has been
  applied to ensure the Prog_enable Signature has been latched.
  4. Change the Prog_enable[2] (SDO pin) to input mode to avoid drive contention on this pin.
  5. Wait at least 300 us before giving any serial instructions on SDI/SII.
  6. Exit Programming mode by bringing RESET pin to 0V.

How to build:
------------
* Download FT232RL D2XX driver and Extract it.
* Connect FT232RL to PC and Install the driver.
* Install MINGW Developer STUDIO.
* Install D2XX libirary in MingW DS:
	- copy ftd2xx.h  from FTDI-2XX_v2.12.28_X86\X64 to C:\Program Files (x86)\MinGWStudio\MinGW\include
	- copy ftd2xx.lib FTDI-2XX_v2.12.28_X86\X64\i386 to C:\Program Files (x86)\MinGWStudio\MinGW\lib
* Open the source code version you want by a text editor and save it as "main.c"
* Open the MINGW DS project file "fdoctor.mdsp".
* Make sure that you link against the "ftd2xx.lib":
		project --> setings --> "link" tab: 
		add "C:\Program Files\MinGWStudio\MinGW\lib\ftd2xx.lib" in "librairies" fold then click OK.

* build by F8 (or the build icon).

If you get this error while building the code:
	undefined reference to '_imp__FT_Open@8'
	undefined reference to '_imp__FT_GetDeviceInfo@24'
	undefined reference to '_imp__FT_Close@4'
Make sure that you've copied ftd2xx.lib from "X64\i386" NOT from "X64\AMD64"


Usage:
----
Open a CMD window (in the program folder) and run "fdoctor.exe".

Tested with attiny85.
Default Fuses (Internal 8MHz devided by 8):
LF = 0x62
HF = 0xDF
EF = 0xFF

===================================================

V1:
	Read Fuses

V2:
	Read Signature + Fuses

V3:
	Detect ATtiny type and display it's Name, default Fuses and current Fuses

V4:
	V3 + Restore default Fuses

