/******************************************************************************
V4: 
	* Detects ATtiny type and displays it's name, default and current Fuses
	* Restores default fuses
Tested with ATTINY85 on WIN11
*******************************************************************************/
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <windows.h>	// for Sleep() function
#include <ftd2xx.h>

/*
// FT232RL pin masks:
TX  D0 ==> 0x01 
RX  D1 ==> 0x02 
RTS D2 ==> 0x04 
CTS D3 ==> 0x08 
DTR D4 ==> 0x10
DSR D5 ==> 0x20
DCD D6 ==> 0x40
RI  D7 ==> 0x80
*/

// HVSP Pin defines
#define	SDI		0x10	// DTR  ==> Serial Data Input on target (PB0)
#define	SII		0x02	// RX  ==> Serial Instruction Input on target (PB1)
#define	SDO     0x01	// TX  ==> Serial Data Output on target (PB2)
#define	SCI		0x04	// RTS  ==> Serial Clock Input on target (PB3)
#define	RST		0x08	// CTS  ==> +12V to reset pin on target (inverted through NPN transistor)

FT_HANDLE handle;		// FT232RL handle
unsigned long bytes;	// needed by FT_Write()  
unsigned char bbb;		// bit bang byte value to write
unsigned char stat;		// current state of pins

typedef struct {
	unsigned char		signature[3];
	unsigned char		fuseLow;
	unsigned char		fuseHigh;
	unsigned char		fuseExtended;
	char				name[10];
}TargetCpuInfo_t;


// add other devices here
const TargetCpuInfo_t targetCpu[] = {
	{	// ATtiny13: 
		.signature	    = {0x1E, 0x90, 0x07},
		.fuseLow	    = 0x6A,
		.fuseHigh	    = 0xFF,
		.fuseExtended	= 0x00,
		.name			= "ATTINY13",
	},
	{	// ATtiny24
		.signature	    = {0x1E, 0x91, 0x0B},
		.fuseLow	    = 0x62,
		.fuseHigh	    = 0xDF,
		.fuseExtended	= 0xFF,
		.name			= "ATTINY24",
	},
	{	// ATtiny44
		.signature	    = {0x1E, 0x92, 0x07},
		.fuseLow	    = 0x62,
		.fuseHigh	    = 0xDF,
		.fuseExtended	= 0xFF,
		.name			= "ATTINY44",
	},
	{	// ATtiny84
		.signature	    = {0x1E, 0x93, 0x0C},
		.fuseLow	    = 0x62,
		.fuseHigh	    = 0xDF,
		.fuseExtended	= 0xFF,
		.name			= "ATTINY84",
	},
	{	// ATtiny25
		.signature	    = {0x1E, 0x91, 0x08},
		.fuseLow	    = 0x62,
		.fuseHigh	    = 0xDF,
		.fuseExtended	= 0xFF,
		.name			= "ATTINY25",
	},
	{	// ATtiny45
		.signature	    = {0x1E, 0x92, 0x06},
		.fuseLow	    = 0x62,
		.fuseHigh	    = 0xDF,
		.fuseExtended	= 0xFF,
		.name			= "ATTINY45",
	},
	{	// ATtiny85
		.signature	    = {0x1E, 0x93, 0x0B},
		.fuseLow	    = 0x62,
		.fuseHigh	    = 0xDF,
		.fuseExtended	= 0xFF,
		.name			= "ATTINY85",
	},
	  // mark end of list
	{{0,0,0}, 0, 0, 0, {0,0,0,0,0,0,0,0,0,0}},
};

//=============================================== 
// WINDOWS function to sleep some uS 
// In my PC I found uSleep(1) = 200us !!!
void uSleep(int waitTime){
    __int64 time1 = 0, time2 = 0;

    QueryPerformanceCounter((LARGE_INTEGER *) &time1);
    do{
        QueryPerformanceCounter((LARGE_INTEGER *) &time2);
    } while((time2-time1) < waitTime);
}

//=============================================== 
// Send a pulse on SCI 
// The minimum period for the Serial Clock Input (SCI) is 220 ns.
inline void clockit(void){
	bbb |= (SCI);						// SCI bit = 1
	FT_Write(handle, &bbb, 1, &bytes);	// send 1 byte 	
	//uSleep(1);						// Try without it
	bbb &= ~(SCI);						// SCI bit = 0
	FT_Write(handle, &bbb, 1, &bytes);	// send 1 byte 
}

//=======================================================
// Send instruction and data byte to target and read one byte from target
unsigned char transferByte(unsigned char data, unsigned char instruction){
	unsigned char   i;
	unsigned char   byteRead = 0;

	// first bit is always zero
	bbb &= ~(SDI);
	bbb &= ~(SII);
	clockit();

	for(i=0; i<8; i++){		
		// read one bit form SDO
		byteRead <<= 1;  				// shift one bit, this clears LSB	
		FT_GetBitMode(handle, &stat);	// read pins
		if(stat & SDO) byteRead |= 1;	// If SDO high set LSB
				
		// output next data bit on SDI
		if(data & 0x80) bbb |= SDI; 	// set SDI
		else bbb &= ~(SDI);  			// Clear SDI
		// output next instruction bit on SII
		if (instruction & 0x80) bbb |= SII;  // set SII
		else bbb &= ~(SII);	 			// Clear SII
		FT_Write(handle, &bbb, 1, &bytes);	// send 1 byte
		clockit();
		// prepare for processing next bit
		data <<= 1;
		instruction <<= 1;
	}

	// Last two bits are always zero
	bbb &= ~(SDI);		
	bbb &= ~(SII);			
	clockit();		// pulse CSI 2 times
	clockit();

	return byteRead;
}

//====================================
unsigned char readFuse(unsigned char inst2, unsigned char inst3){
	transferByte(0x04, 0x4C);
	transferByte(0x00, inst2);
	return transferByte(0x00, inst3);
}

//================================ 
unsigned char getSignature(unsigned char byte){
	transferByte(0x08, 0x4C);
	transferByte(byte, 0x0C);
	transferByte(0x00, 0x68);
	return transferByte(0x00, 0x6C);
}

//===============================
void pollSDO(void){
	// wait until SDO goes high
	while(1){
		FT_GetBitMode(handle, &stat);
		if(stat & SDO) break;
	}
}

//=============================
void writeFuse(unsigned char byte, unsigned char inst3 , unsigned char inst4){
	transferByte(0x40, 0x4C);
	transferByte(byte, 0x2C);
	transferByte(0x00, inst3);
	transferByte(0x00, inst4);
	pollSDO();
	transferByte(0x00, 0x4C);
	pollSDO();
}

//====================================================
int main(void){
    unsigned char device, val;  			// device ID and a var to read signature or fuse value
	unsigned char sig0, sig1, sig2;  		// sig bytes
	 
    printf("FT232RL ATTINY FUSE DOCTOR!!\r\n");

    // Initialize and open the device
    if(FT_Open(0, &handle) != FT_OK) {
        puts("Can't open device");
        return 1;
    }
	// set bitbang mode and config pins as outputs
    FT_SetBitMode(handle, SDI|SII|SDO|SCI|RST, 1);		// DTR, RX, TX, .... outputs
    FT_SetBaudRate(handle, 9600);  		// freq = 9600 * 16 
	
	// 1. clear all outputs
	bbb = RST;		// RST bit must be 1 to pull RESET Low (NPN transistor)						
	FT_Write(handle, &bbb, 1, &bytes);	// send 1 byte
	   	
    // 2. Wait 20-60us before applying 12V to RESET
	//uSleep(1);
	bbb &= ~(RST);			// clear RST bit							
	FT_Write(handle, &bbb, 1, &bytes);	// send 1 byte
    	
    // 3. Keep the Prog_enable pins unchanged for at least 10 us after the High-voltage has been applied 
	//uSleep(1);
    	
    // 4. Switch Prog_enable[2] (SDO pin) to input
	FT_SetBitMode(handle, SDI|SII|SCI|RST, 1);		// DTR, RX, TX, .... outputs, SDO input
	
    // 5.  wait > 300us before sending any data on SDI/SII
    //uSleep(300);
	uSleep(2);
	
	// Read Signature:
	sig0 = getSignature(0);		// sig 1st byte
	sig1 = getSignature(1);		// sig 2nd byte
	sig2 = getSignature(2);		// sig 3rd byte	
		
    for(device = 0; targetCpu[device].signature[0]!=0 ; device++){
    	if(targetCpu[device].signature[0] == sig0 && targetCpu[device].signature[1] == sig1 && targetCpu[device].signature[2] == sig2){
			break;		// A known device found, stop searching.
    	}
    }
	
	//printf("device = %d\r\n", device); // debug
	// We've 7 devices in our list (0 to 6)
	if(device <7){
		// Display device name:
		printf(" %s detected.\r\n", targetCpu[device].name);

		// Display default Fuse values:
		printf("Default Fuses:");
		val = targetCpu[device].fuseLow;		// Low Fuse
		printf("  LFuse = 0X%02X,", val);
		val = targetCpu[device].fuseHigh;		// High Fuse
		printf(" HFuse = 0X%02X,", val);
		val = targetCpu[device].fuseExtended;	// Extended Fuse
		printf(" EFuse = 0X%02X\r\n", val);
		
		// Read Current fuses and display them:
		printf("Current Fuses:");
		val = readFuse(0x68, 0x6C);		// Low Fuse
		printf("  LFuse = 0X%02X,", val);
		val = readFuse(0x7A, 0x7E);		// High Fuse
		printf(" HFuse = 0X%02X,", val);
		val = readFuse(0x6A, 0x6E);		// Extended Fuse
		printf(" EFuse = 0X%02X\r\n", val);
		
		// Restore default fuses 
		printf("Restoring default fuses .....");
		writeFuse(targetCpu[device].fuseLow, 0x64, 0x6C);
		writeFuse(targetCpu[device].fuseHigh, 0x74, 0x7C);
		if(targetCpu[device].fuseExtended !=0) writeFuse(targetCpu[device].fuseExtended, 0x66, 0x6E);	
		printf("...Done!!\r\n");

		// verify
		printf("Verifying .....");
		if((targetCpu[device].fuseLow == readFuse(0x68, 0x6C)) && (targetCpu[device].fuseHigh == readFuse(0x7A, 0x7E)) && (targetCpu[device].fuseExtended == readFuse(0x6A, 0x6E))) printf("... SUCCESS!!!! \r\n");
		else printf(".. Oooops!!!!!!!!!!!\r\n");
	}
	else{
		printf("Unknown device!!\r\n");
		// Display Signature:
		printf("Signature = 0X%02X%02X%02X\r\n", sig0, sig1, sig2);
	}

	//6. Exit Programming mode by bringing RESET pin to 0V.
	bbb = RST;			// RST bit =1, clear all others							
	FT_Write(handle, &bbb, 1, &bytes);	// send 1 byte
	printf("THANK YOU");
	return 0;
}
