#include "logToUi.h"
#include "log.h"

LogToUi logToUi;

LogToUi::LogToUi() 
{ 
    modemLogLevel = INFO;
    modemLog = new Ringbuffer<LogLine, RINGBUFFER_SIZE>(); 
}

size_t LogToUi::printf(const char *format, ...)
{
    return log(DBG, format);
}

size_t LogToUi::log(const LogLevel logLevel, const char *format, ...)
{
    char loc_buf[64];
    char * temp = loc_buf;
    va_list arg;
    va_list copy;
    va_start(arg, format);
    va_copy(copy, arg);
    int len = vsnprintf(temp, sizeof(loc_buf), format, copy);
    va_end(copy);
    if(len < 0) {
        va_end(arg);
        return 0;
    };
    if(len >= sizeof(loc_buf)){
        temp = (char*) malloc(len+1);
        if(temp == NULL) {
            va_end(arg);
            return 0;
        }
        len = vsnprintf(temp, len+1, format, arg);
    }
    va_end(arg);

    // Bereinige Text: nur druckbares ASCII und \r\n\t erlauben,
    // alles andere ersetzen durch '?' um Invalid-UTF-8 im WebSocket zu vermeiden
    for (int i = 0; temp[i] != '\0'; i++) {
      unsigned char c = (unsigned char)temp[i];
      if (c < 0x20 && c != '\r' && c != '\n' && c != '\t') {
        temp[i] = '?';
      } else if (c >= 0x80) {
        temp[i] = '?';
      }
    }

    auto freeHeap = ESP.getFreeHeap();
    auto line = LogLine{nr: modemLineNrIn++, level: logLevel, text: temp, freeHeap: freeHeap};
    if(temp != loc_buf){
        free(temp);
    }
    modemLog->push(&line, true);
    Serial.printf("(%d) %s\r\n", freeHeap, line.text.c_str());

    timestamp = millis();
    
    return len;
}

bool LogToUi::hasData() {
    // Prüfe, ob es überhaupt Logs gibt
    return !modemLog->isEmpty();
} 

void LogToUi::marshal(JsonObject o)
{
    uint16_t count = modemLog->currentSize();
    if (count == 0) return;
    
    JsonArray logJson = o["log"].to<JsonArray>();

    LogLine line;
    int sent = 0;
    // Sende die letzten 6 Zeilen (neueste zuerst)
    uint16_t startIdx = count > 6 ? count - 6 : 0;
    for (uint16_t i = startIdx; i < count && sent < 6; i++) {
        if (modemLog->peekAt(i, line)) {
            JsonObject jsonLine = logJson.add<JsonObject>();
            jsonLine["nr"] = line.nr;
            jsonLine["level"] = line.level;
            jsonLine["text"] = line.text;
            jsonLine["freeHeap"] = line.freeHeap;
            sent++;
        }
    }
    
    Serial.printf("(%d/%d)\r\n", sent, count); 
}

uint16_t LogToUi::marshalBatch(const JsonObject &o, uint16_t startIdx, uint16_t maxLines)
{
    uint16_t count = modemLog->currentSize();
    if (count == 0 || startIdx >= count) return 0;

    JsonArray logJson = o["log"].to<JsonArray>();

    LogLine line;
    uint16_t sent = 0;
    for (uint16_t i = startIdx; i < count && sent < maxLines; i++) {
        if (modemLog->peekAt(i, line)) {
            JsonObject jsonLine = logJson.add<JsonObject>();
            jsonLine["nr"] = line.nr;
            jsonLine["level"] = line.level;
            jsonLine["text"] = line.text;
            jsonLine["freeHeap"] = line.freeHeap;
            sent++;
        }
    }
    return sent;
}

void LogToUi::exportAll(String &csv)
{
    csv = "nr,level,freeHeap,text\r\n";
    
    uint16_t count = modemLog->currentSize();
    LogLine line;
    for (uint16_t i = 0; i < count; i++) {
        if (modemLog->peekAt(i, line)) {
            csv += String(line.nr) + "," + String(line.level) + "," + String(line.freeHeap) + ",";
            // Escape quotes and replace line breaks with spaces for clean CSV
            String text = line.text;
            text.replace("\r\n", " ");
            text.replace("\r", " ");
            text.replace("\n", " ");
            text.replace("\"", "\"\"");
            csv += "\"" + text + "\"\r\n";
        }
    }
    
    Serial.printf("[LogToUi] Exported %d log lines to CSV (%d bytes)\r\n", count, csv.length());
}
