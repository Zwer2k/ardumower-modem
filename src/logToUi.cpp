#include "logToUi.h"
#include "log.h"

LogToUi logToUi;

LogToUi::LogToUi() 
{ 
    modemLogLevel = INFO;
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
    if(temp != loc_buf){
        free(temp);
    }
    modemLog->push(&strTemp, true);
    Serial.println(strTemp);

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
    if (modemLog->isEmpty())
        return;
    
    String line;
    int lineNr = 0;
    bool hasData = true;
    while (hasData) {
        hasData = pull(line);
        if (hasData) {
            o[String(lineNr++)] = line;
        }
    } 
}

