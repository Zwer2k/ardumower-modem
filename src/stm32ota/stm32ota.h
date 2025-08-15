/*   
    Copyright (C) 2017  CSNOL  https://github.com/csnol/1CHIP-Programmers

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, 

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#ifndef _STM32OTA_H_
#define _STM32OTA_H_
#endif
#include <Arduino.h>

#define BOOT0_PIN 5
#define NRST_PIN 7

#define STERR     		"ERROR"
#define STM32INIT  		0x7F          // Send Init command 
#define STM32ACK  		  0x79          // return ACK
#define STM32NACK  		0x1F          // return NACK
#define STM32GET  		0x00             // get version command
#define STM32RUN  		0x21          // Restart and Run programm
#define STM32RD  		  0x11          // Read flash command
#define STM32WR  		  0x31          // Write flash command
#define STM32UNPCTWR  0x73          // Unprotest WR command
#define STM32ERASE  	0x43          // Erase flash command
#define STM32ERASEN 	0x44          // Erase extended flash command
#define STM32ID  		  0x02          // get chip ID command
#define STM32STADDR  	0x8000000     // STM32 codes start address
#define STM32ERR  		0x3F
#define F10xx8  "STM32F103x8/B"     //tested
#define F10xx6  "STM32F103x4/6"		
#define F10xxc  "STM32F103xC/D/E"
#define F107    "STM32F105/107"
#define F03x46  "STM32F03xx4/6"		//tested
#define F030xc  "STM32F030xC"
#define F030x8  "STM32F030x8/05x"

class FirmwareWriterSTM32 {
private:
    HardwareSerial &serial;

    bool sendByteAndCheckACK(uint8_t byteToSend, unsigned long timeoutMs = 200);

public:
    FirmwareWriterSTM32(HardwareSerial &serial);

    bool switchToFlashMode();
    void switchToRunMode();
    bool checkFlashMode();
    bool dataAvailable(unsigned long timeoutMs = 500);
    bool readSingleResponse(uint8_t& responseByte, unsigned long timeoutMs);
    bool sendCommand(uint8_t commd, unsigned long timeoutMs = 500);
    bool eraseWith0x73();
    bool erase();
    unsigned char erasen();
    unsigned char run();
    unsigned char read(unsigned char * rdbuf, unsigned long rdaddress, unsigned char rdlong);
    bool sendAddress(uint32_t address, unsigned long timeoutMs = 1000);
    bool sendDataBlock(const uint8_t* data, size_t len, unsigned long timeoutMs);
    String getId();
    char version();

    // Added for reboot/run/flash separation
    static void rebootMcuStatic();
    void rebootMcu();
};