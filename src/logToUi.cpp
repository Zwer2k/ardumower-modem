#include "logToUi.h"


LogToUi logToUi;

LogToUi::LogToUi() 
{ 
    modemLog = new Ringbuffer<String, RINGBUFFER_SIZE>(); 
}

size_t LogToUi::printf(const char *format, ...)
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
    String strTemp = String(temp);
    if (!modemLog->push(&strTemp)) {
        if (modemLogIgnoreLines == 0) {
            Serial.printf("Log ring buffer full %d/%d RAM: %d\r\n", modemLog->currentSize(), modemLog->maxSize(), ESP.getFreeHeap());
        }
        modemLogIgnoreLines++;
    } else {
        if (modemLogIgnoreLines > 0) {
            Serial.printf("Log ring buffer no longer full. %d lines ignored.\r\n", modemLogIgnoreLines);
            modemLogIgnoreLines = 0;
        }
    }

    Serial.printf((strTemp + "\r\n").c_str());
    if(temp != loc_buf){
        free(temp);
    }
    
    timestamp = millis();
    
    return len;
}

bool LogToUi::hasData() {
    if (!modemLog->isEmpty()) {
        timestamp = millis();   
        return true;
    } else {
        return false;
    }
} 

bool LogToUi::pull(String &line) 
{
    if (modemLog->pull(line)) {
        if (!modemLog->isEmpty())
            timestamp = millis();   
        return true;
    } else {
        return false;
    }
}

void LogToUi::marshal(const JsonObject &o)
{
    String line;

    if (modemLog->isEmpty())
        return;
    
    int lineNr = 0;
    bool hasData = true;
    while (hasData) {
        hasData = pull(line);
        if (hasData) {
            o[String(lineNr++)] = line;
        }
    } 
}

