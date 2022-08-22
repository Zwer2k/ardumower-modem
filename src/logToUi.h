#ifndef logToUi_h
#define logToUi_h

#include <ArduinoJson.h>
#include "ringbuffer.h"

#define RINGBUFFER_SIZE 30

class LogToUi {
    public:
        LogToUi();

        size_t printf(const char *format, ...);
        bool hasData();
        bool pull(String &line);
        void marshal(const JsonObject &o);
        uint32_t timestamp = 0;

    private:
        Ringbuffer<String, RINGBUFFER_SIZE> *modemLog = NULL;

        uint32_t lineNrIn = 0;

        uint32_t modemLogIgnoreLines = 0;
};

extern LogToUi logToUi;

#endif // logToUi_h