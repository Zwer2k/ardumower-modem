/*   
    Copyright (C) 2025

    Insulated by CSNOL  https://github.com/csnol/1CHIP-Programmers

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

#include "stm32ota.h"
#include "log.h"

FirmwareWriterSTM32::FirmwareWriterSTM32(HardwareSerial &serial): serial(serial) {
  
}



// Reboot STM32 in Run-Mode (BOOT0_PIN LOW)
void FirmwareWriterSTM32::rebootMcu() {
  rebootMcuStatic();
}

void FirmwareWriterSTM32::rebootMcuStatic() {
  pinMode(BOOT0_PIN, OUTPUT);
  pinMode(NRST_PIN, OUTPUT);
  delay(50);
  digitalWrite(BOOT0_PIN, LOW); // Run-Mode
  delay(50);
  digitalWrite(NRST_PIN, LOW);
  delay(100);
  digitalWrite(NRST_PIN, HIGH);
  delay(200);
  pinMode(BOOT0_PIN, INPUT);
  pinMode(NRST_PIN, INPUT);
  
  yield();
}

bool FirmwareWriterSTM32::switchToFlashMode()  {
  Log(DBG, "FirmwareWriterSTM32::switchToFlashMode");

  serial.end();
  delay(100);
  serial.begin(115200, SERIAL_8E1);

  pinMode(BOOT0_PIN, OUTPUT);
  pinMode(NRST_PIN, OUTPUT);
  delay(50);
  digitalWrite(BOOT0_PIN, HIGH); // Flash-Mode
  delay(50);
  digitalWrite(NRST_PIN, LOW);
  delay(100);
  digitalWrite(NRST_PIN, HIGH);
  delay(200);
  yield();

  serial.eventQueueReset();
  if (!sendByteAndCheckACK(STM32INIT)) {
    Log(ERR, "FirmwareWriterSTM32::switchToFlashMode error on init");
    return false;
  }

  delay(100);

  Log(DBG, "FirmwareWriterSTM32::switchToFlashMode init done");

  return true;
}

void FirmwareWriterSTM32::switchToRunMode()  {
  serial.end();
  delay(100);
  serial.begin(115200, SERIAL_8N1);

  pinMode(BOOT0_PIN, OUTPUT);
  pinMode(NRST_PIN, OUTPUT);
  delay(50);
  digitalWrite(BOOT0_PIN, LOW);
  delay(500);
  digitalWrite(NRST_PIN, LOW);
  delay(50);
  digitalWrite(NRST_PIN, HIGH);
  delay(1000);
  pinMode(BOOT0_PIN, INPUT);
  pinMode(NRST_PIN, INPUT);  
  yield();
}

bool FirmwareWriterSTM32::checkFlashMode() {
  Log(DBG, "Getting bootloader commands...");
  if (sendCommand(STM32GET, 2000)) { // 0x00 is GET command
      uint8_t numCommands;
      if (readSingleResponse(numCommands, 1000)) { // Read number of commands
          Log(INFO, "Bootloader supports %d commands:", numCommands);
          for (int i = 0; i < numCommands; i++) {
              uint8_t cmdCode;
              if (readSingleResponse(cmdCode, 1000)) {
                  Log(INFO, "  0x%x", cmdCode);
              } else {
                  Log(ERR, "Failed to read command %d", i);
                  break;
              }
          }
      } else {
          Log(ERR, "Failed to read number of commands.");
          return false;
      }
  } else {
      Log(ERR, "GET command NACKed or timed out.");
      return false;
  }

  return true;
}

bool FirmwareWriterSTM32::sendByteAndCheckACK(uint8_t byteToSend, unsigned long timeoutMs) {
  serial.write(byteToSend);
  if (!dataAvailable(timeoutMs)) {
    Log(ERR, "FirmwareWriterSTM32::sendCommand no answer for 0x%x", byteToSend);
    return false;
  }
  uint8_t response = serial.read();
  if (response == 0x79) { // ACK
    return true;
  } else if (response == 0x1F) { // NACK
    Log(ERR, "FirmwareWriterSTM32::sendCommand answer NACK for 0x%x", byteToSend);
    return false;
  } else {
    Log(ERR, "FirmwareWriterSTM32::sendCommand unexpected answer 0x%x for 0x%x", response, byteToSend);
    return false;
  }
}

bool FirmwareWriterSTM32::readSingleResponse(uint8_t& responseByte, unsigned long timeoutMs) {
    unsigned long startTime = millis();
    while ((!serial.available()) && (millis() - startTime < timeoutMs)) {
        yield();
    }
    if (serial.available() > 0) {
        responseByte = serial.read();
        return true;
    }
    return false; // Timeout
}

bool FirmwareWriterSTM32::sendCommand(uint8_t commandByte, unsigned long timeoutMs) {
    uint8_t checksum = commandByte ^ 0xFF; // XOR-Checksumme
    
    // Bytes in einem kleinen Array zusammenfassen
    uint8_t commandPacket[2] = {commandByte, checksum};

    // Das gesamte Paket auf einmal senden
    serial.write(commandPacket, 2); // Senden Sie das Array und die Länge (2 Bytes)

    uint8_t response;
    if (!readSingleResponse(response, timeoutMs)) {
      Log(ERR, "FirmwareWriterSTM32::sendCommand no answer for 0x%x", commandByte);
      return false;
    }
    if (response == 0x79) { // ACK
        return true;
    } else if (response == 0x1F) { // NACK
        Log(ERR, "FirmwareWriterSTM32::sendCommand: NACK empfangen für Befehl 0x%x", commandByte);
        return false;
    } else {
        Log(ERR, "FirmwareWriterSTM32::sendCommand: Unerwartete Antwort 0x%x für Befehl 0x%x", response, commandByte);
        return false;
    }
}

bool FirmwareWriterSTM32::eraseWith0x73() {
    Log(DBG, "FirmwareWriterSTM32::erase: Initiating mass erase sequence.");

    if (!sendCommand(STM32UNPCTWR, 5000)) { // STM32UNPCTWR ist 0x73
        Log(ERR, "FirmwareWriterSTM32::erase: 0x73 Befehl fehlgeschlagen oder unerwartete Antwort beim Senden des Befehls. Versuch mit 0x43 (Erase Memory).");       
        return erase(); // Eine separate Funktion für 0x43
    }

    uint8_t finalEraseResponse;
    if (!readSingleResponse(finalEraseResponse, 15000)) { 
        Log(ERR, "FirmwareWriterSTM32::erase: Timeout beim Warten auf finales ACK für 0x73 (Readout Unprotect / Mass Erase) Abschluss.");
        return false;
    }

    if (finalEraseResponse == STM32ACK) { // 0x73 erfolgreich abgeschlossen
        Log(INFO, "FirmwareWriterSTM32::erase: Readout Unprotect und Globale Löschung erfolgreich abgeschlossen.");
    } else if (finalEraseResponse == STM32NACK) {
        Log(ERR, "FirmwareWriterSTM32::erase: Readout Unprotect / Mass Erase hat NACK empfangen (fehlgeschlagen).");
        return false;
    } else {
        Log(ERR, "FirmwareWriterSTM32::erase: Readout Unprotect / Mass Erase hat unerwartete finale Antwort 0x%x.", finalEraseResponse);
        return false;
    }

    // --- 0x73 führt automatischen neustart durch, daher eine reinitialisierung erforderlich ---
    Log(DBG, "FirmwareWriterSTM32::erase: Löschvorgang abgeschlossen. STM32 wird neu starten. Warte und re-initialisiere Bootloader.");
    delay(1000); // Warten, bis STM32 komplett neu gestartet ist
    yield();     // Watchdog füttern

    // ERNEUTES Senden von 0x7F zum Re-Synchronisieren nach dem Reset
    if (!sendByteAndCheckACK(STM32INIT)) { // STM32INIT ist 0x7F
      Log(ERR, "FirmwareWriterSTM32::erase: Fehler bei der Re-Initialisierung nach dem Löschen.");
      return false;
    }
    Log(DBG, "FirmwareWriterSTM32::erase: Re-Synchronisation des Bootloaders nach Löschen erfolgreich.");

    return true; // Erfolgreich gelöscht und re-synchronisiert
}

bool FirmwareWriterSTM32::erase() {
    Log(DBG, "FirmwareWriterSTM32::eraseWith0x43: Versuche Löschen mit 0x43 (Erase Memory) Befehl.");
    
    // Schritt 1: Sende den 0x43 Befehl
    if (!sendCommand(STM32ERASE, 5000)) { // STM32ERASE ist 0x43
        Log(ERR, "FirmwareWriterSTM32::eraseWith0x43: 0x43 Befehl fehlgeschlagen oder NACK/unerwartete Antwort für den Befehl.");
        return false;
    }

    // Schritt 2: Sende den Global Erase Parameter (0xFF) und seine Checksumme (0x00)
    // DIESE WERDEN DIREKT GESENDET, NICHT ÜBER sendCommand(), da sie KEINEN eigenen ACK erwarten!
    uint8_t eraseParam = 0xFF; // Parameter für globale Löschung
    uint8_t eraseChecksum = eraseParam ^ 0xFF; // Checksumme für 0xFF ist 0x00

    serial.write(eraseParam);
    serial.write(eraseChecksum);

    // Schritt 3: Warte auf das FINALE ACK/NACK für den Abschluss des Löschvorgangs.
    // Dieses ACK bestätigt, dass der Löschvorgang (der mehrere Sekunden dauern kann) ABGESCHLOSSEN ist.
    uint8_t finalEraseResponse;
    if (!readSingleResponse(finalEraseResponse, 15000)) { 
        Log(ERR, "FirmwareWriterSTM32::eraseWith0x43: Timeout beim Warten auf finales ACK für 0x43 (Global Erase) Abschluss.");
        return false;
    }

    if (finalEraseResponse == STM32ACK) {
        Log(INFO, "FirmwareWriterSTM32::eraseWith0x43: Globale Löschung erfolgreich abgeschlossen.");        
        return true;
    } else if (finalEraseResponse == STM32NACK) {
        Log(ERR, "FirmwareWriterSTM32::eraseWith0x43: Globale Löschung mit 0x43 hat NACK empfangen (fehlgeschlagen).");
        return false;
    } else {
        Log(ERR, "FirmwareWriterSTM32::eraseWith0x43: Globale Löschung mit 0x43 hat unerwartete finale Antwort 0x%x.", finalEraseResponse);
        return false;
    }
}

bool FirmwareWriterSTM32::dataAvailable(unsigned long timeoutMs) {
  unsigned long startTime = millis();
  while ((!serial.available()) && (millis() - startTime < timeoutMs)) {
    yield();
  }
  return serial.available() > 0;
}

unsigned char FirmwareWriterSTM32::erasen() {     // Tested
  unsigned char eraseflag = 0;
  sendCommand(STM32ERASEN);
  if (!dataAvailable()) {
    return STM32ERR;
  }

  if (serial.read() == STM32ACK)
  {
    serial.write(0xFF);
    serial.write(0xFF);
    serial.write(0x00);
  }
  else return STM32ERR;
  
  if (!dataAvailable()) {
    return STM32ERR;
  }

  eraseflag = serial.read();
  return eraseflag;
}

// No test yet
unsigned char FirmwareWriterSTM32::run()   {
  sendCommand(STM32RUN);
  if (!dataAvailable()) {
    return STM32ERR;
  }

  if (serial.read() == STM32ACK) {
    sendAddress(STM32STADDR);
    return STM32ACK;
  }
  else
    return STM32ERR;
}

// No test yet
unsigned char FirmwareWriterSTM32::read(unsigned char * rdbuf, unsigned long rdaddress, unsigned char rdlong) {
  //unsigned char eraseflag = 0;
  sendCommand(STM32RD);
  if (!dataAvailable()) {
    return STM32ERR;
  }

  if (serial.read() == STM32ACK) {
    sendAddress(rdaddress);
  }
  else return STM32ERR;

  if (!dataAvailable()) {
    return STM32ERR;
  }

  if (serial.read() == STM32ACK)
    sendCommand(rdlong);
  if (!dataAvailable()) {
    return STM32ERR;
  }
  size_t rdlen = serial.available();
  serial.readBytes(rdbuf, rdlen);
  return STM32ACK;
}

bool FirmwareWriterSTM32::sendAddress(uint32_t addr, unsigned long timeoutMs) {
    //Log(DBG, "FirmwareWriterSTM32::sendAddress: Sende Adresse 0x%08lX", addr);

    unsigned char sendaddr[4];
    unsigned char addcheck = 0; // Checksumme für die Adressbytes

    // Adresse in einzelne Bytes zerlegen (MSB zuerst)
    sendaddr[0] = (addr >> 24) & 0xFF;
    sendaddr[1] = (addr >> 16) & 0xFF;
    sendaddr[2] = (addr >> 8) & 0xFF;
    sendaddr[3] = addr & 0xFF;

    // Adressbytes senden und Checksumme berechnen
    for (int i = 0; i <= 3; i++) {
        serial.write(sendaddr[i]); // Senden des aktuellen Adressbytes
        addcheck ^= sendaddr[i];   // XOR-Checksumme aktualisieren
    }
    serial.write(addcheck); // Checksumme der Adresse senden
    //Log(DBG, "FirmwareWriterSTM32::sendAddress: Sende Adresse checksume 0x%x", addcheck);

    // Warten auf die EINE Antwort (ACK/NACK) für den gesamten Adressblock
    uint8_t response;
    // Hier verwenden wir eine Hilfsfunktion, die auf eine einzelne Antwort wartet.
    // Wenn Sie keine 'readSingleResponse' haben, ersetzen Sie dies durch die Logik von 'dataAvailable' und 'serial.read()'.
    if (!readSingleResponse(response, timeoutMs)) {
        Log(ERR, "FirmwareWriterSTM32::sendAddress: Timeout beim Warten auf Antwort für Adresse 0x%08lX", addr);
        return false; // Timeout
    }

    if (response == STM32ACK) {
        //Log(DBG, "FirmwareWriterSTM32::sendAddress: Adresse 0x%08lX ACKed.", addr);
        return true; // Erfolg: ACK erhalten
    } else if (response == STM32NACK) {
        Log(ERR, "FirmwareWriterSTM32::sendAddress: NACK empfangen für Adresse 0x%08lX.", addr);
        return false; // Fehler: NACK erhalten
    } else {
        Log(ERR, "FirmwareWriterSTM32::sendAddress: Unerwartete Antwort 0x%x für Adresse 0x%08lX.", response, addr);
        return false; // Fehler: Unerwartete Antwort
    }
}

// Sends the data block: (len-1) byte, data bytes, checksum of (len-1)+data
// Waits for ONE final ACK/NACK for the entire block
bool FirmwareWriterSTM32::sendDataBlock(const uint8_t* data, size_t len, unsigned long timeoutMs) {
    if (len == 0 || len > 256) { // Max block size is 256 bytes for 0x31
        Log(ERR, "sendDataBlock: Invalid data length %d", len);
        return false;
    }

    uint8_t n_byte = (uint8_t)(len - 1);
    uint8_t dataChecksum = n_byte; // Start checksum with N byte

    // Erstelle einen temporären Puffer für alle zu sendenden Bytes:
    // N-Byte (1) + Datenbytes (len) + Checksummen-Byte (1)
    // Maximale Größe: 1 + 256 + 1 = 258 Bytes
    uint8_t sendBuffer[258]; 
    size_t bufferIndex = 0;

    // 1. N-Byte in den Puffer
    sendBuffer[bufferIndex++] = n_byte;
    //Log(DBG, "sendDataBlock: Sending N=0x%02X (len=%d)", n_byte, len);

    // 2. Datenbytes in den Puffer und Checksumme berechnen
    for (size_t i = 0; i < len; ++i) {
        sendBuffer[bufferIndex++] = data[i];
        dataChecksum ^= data[i]; // Update checksum
    }

    // 3. Checksummen-Byte in den Puffer
    sendBuffer[bufferIndex++] = dataChecksum;
    //Log(DBG, "sendDataBlock: Sending dataChecksum=0x%02X", dataChecksum);

    // Jetzt den gesamten Puffer in einem Rutsch senden
    serial.write(sendBuffer, bufferIndex); // bufferIndex ist jetzt die Gesamtanzahl der gesendeten Bytes

    // Warte auf die EINE endgültige ACK/NACK für den gesamten Datenblock
    uint8_t response;
    if (!readSingleResponse(response, timeoutMs)) {
        Log(ERR, "sendDataBlock: Timeout waiting for ACK/NACK for data block (len=%d)", len);
        return false;
    }

    if (response == STM32ACK) { // ACK
        //Log(DBG, "sendDataBlock: Data block (len=%d) ACKed.", len);
        return true;
    } else if (response == STM32NACK) { // NACK
        Log(ERR, "sendDataBlock: NACK received for data block (len=%d)", len);
        return false;
    } else {
        Log(ERR, "sendDataBlock: Unexpected response 0x%x for data block (len=%d)", response, len);
        return false;
    }
}

char FirmwareWriterSTM32::version() {     // Tested
  unsigned char vsbuf[14];
  sendCommand(STM32GET);
  if (!dataAvailable()) {
    return 0;
  }

  vsbuf[0] = serial.read();
  if (vsbuf[0] != STM32ACK)
    return STM32ERR;
  else {
    serial.readBytesUntil(STM32ACK, vsbuf, 14);
    return vsbuf[1];
  }
}

String FirmwareWriterSTM32::getId() {     // Tested
  int getid = 0;
  unsigned char sbuf[5];
  sendCommand(STM32ID);
  if (!dataAvailable()) {
    return "";
  }

  sbuf[0] = serial.read();
  if (sbuf[0] == STM32ACK) {
    serial.readBytesUntil(STM32ACK, sbuf, 4);
    getid = sbuf[1];
    getid = (getid << 8) + sbuf[2];
    if (getid == 0x410)
      return F10xx8;
    if (getid == 0x412)
      return F10xx6;
    if (getid == 0x418)
      return F107;
    if (getid == 0x444)
      return F03x46;
    if (getid == 0x414)
      return F10xxc;
    if (getid == 0x440)
      return F030x8;
    if (getid == 0x442)
      return F030xc;
  }
    
  return STERR + (" " + String(getid));
}

