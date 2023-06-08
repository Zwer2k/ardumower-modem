#ifndef logToUi_h
#define logToUi_h

#include "log.h"
#include <ArduinoJson.h>
#include "ringbuffer.h"

#define RINGBUFFER_SIZE 25

struct LogLine {
    uint32_t nr;
    LogLevel level;
    String   text;
    uint32_t freeHeap;
};

class LogToUi {
    public:
        LogToUi();

        byte modemLogLevel;
        size_t printf(const char *format, ...);
        size_t log(const LogLevel logLevel, const char *format, ...);
        bool hasData();
        bool pull(LogLine &line);
        void marshal(const JsonObject &o);
        uint32_t timestamp = 0;

    private:
        Ringbuffer<LogLine, RINGBUFFER_SIZE> *modemLog = NULL;

        uint32_t modemLineNrIn = 0;
};

extern LogToUi logToUi;

#endif // logToUi_h