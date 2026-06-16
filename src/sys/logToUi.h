#ifndef logToUi_h
#define logToUi_h

#include "log.h"
#include <ArduinoJson.h>
#include "ringbuffer.h"

#define RINGBUFFER_SIZE 100
#define LOG_LINE_MAX 128

struct LogLine {
    uint32_t nr;
    LogLevel level;
    char     text[LOG_LINE_MAX];
    uint32_t freeHeap;
};

class LogToUi {
    public:
        LogToUi();

        byte modemLogLevel;
        size_t printf(const char *format, ...);
        size_t log(const LogLevel logLevel, const char *format, ...);
        bool hasData();
        void marshal(JsonObject o);
        void exportAll(String &csv);
        uint16_t marshalBatch(const JsonObject &o, uint16_t startIdx, uint16_t maxLines);
        uint32_t timestamp = 0;

    private:
        Ringbuffer<LogLine, RINGBUFFER_SIZE> *modemLog = NULL;
        uint32_t modemLineNrIn = 0;
};

extern LogToUi logToUi;

#endif // logToUi_h