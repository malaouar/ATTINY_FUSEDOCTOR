/******************************************************************************
V2: Read  ATtiny Signture and Fuses
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
	//uSleep(1);						// NOT needed in Windows!
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
		if(stat & SDO) byteRead |= 1;	// set LSB
				
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

//====================================================
int main(void){
    unsigned char val;  			// var to read signature or fuse value
	
    printf("FT232RL ATTINY SIGNATURE AND FUSE READER!!\r\n");

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
	val = getSignature(0); 			// sig 1st byte
	printf("Signature = 0X%02X", val);
	val = getSignature(1); 			// sig 2nd byte
	printf("%02X", val);
	val = getSignature(2); 			// sig 3rd byte
	printf("%02X\r\n", val);
	
	// Read fuses:
	printf("Fuses:\r\n");
	val = readFuse(0x68, 0x6C);		// Low Fuse
	printf("  LFuse = 0X%02X,", val);
	val = readFuse(0x7A, 0x7E);		// High Fuse
	printf(" HFuse = 0X%02X,", val);
	val = readFuse(0x6A, 0x6E);		// Extended Fuse
	printf(" EFuse = 0X%02X\r\n", val);

	//6. Exit Programming mode by bringing RESET pin to 0V.
	bbb = RST;			// RST bit =1, clear all others							
	FT_Write(handle, &bbb, 1, &bytes);	// send 1 byte
	printf("THANK YOU");
	
	return 0;
}
