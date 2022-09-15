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
    if (!modemLog->isEmpty()) {
        timestamp = millis();   
        return true;
    } else {
        return false;
    }
} 

bool LogToUi::pull(LogLine &line) 
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
    
    JsonArray logJson = o.createNestedArray("log");

    LogLine line;
    int lineNr = 0;
    bool hasData = true;
    while (hasData && (lineNr < 6)) {
        hasData = pull(line);
        if (hasData) {
            JsonObject jsonLine = logJson.createNestedObject();
            jsonLine["nr"] = line.nr;
            jsonLine["level"] = line.level;
            jsonLine["text"] = line.text;
            jsonLine["freeHeap"] = line.freeHeap;
            lineNr++;            
        }
    } 
}

