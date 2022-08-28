#pragma once

#ifndef LOG_H
#define LOG_H 

#include <Arduino.h>
#include "logToUi.h"

#ifndef LOG_CON
#define LOG_CON logToUi
#endif 

#ifndef LOG_CON
#define LOG_CON Serial
#endif

#define NONE    0x00
#define DBG     0x1F
#define INFO    0x0F
#define ERR     0x07
#define EMR     0x03
#define CRIT    0x01

#if LOG_CON == logToUi
  #define DEBUG_LEVEL logToUi.modemLogLevel
#else
  #ifndef DEBUG_LEVEL
  #ifdef ESP_MODEM_SIM
  #define DEBUG_LEVEL logLevel
  #elif ESP_MODEM_TEST
  #define DEBUG_LEVEL DBG
  #else
  #define DEBUG_LEVEL INFO
  #endif
  #endif
#endif

#define DEBUG(...)  LOG_CON.printf(__VA_ARGS__)
#if LOG_CON == logToUi
  #define Log(X, ...) if((DEBUG_LEVEL & X) == X) \
                                    { \
                                      DEBUG(__VA_ARGS__); \
                                    }
#else
  #define Log(X, ...) if((DEBUG_LEVEL & X) == X) \
                                    { \
                                      DEBUG("%.3f %d ", millis() / 1000.0, X); \
                                      DEBUG(__VA_ARGS__); \
                                      DEBUG("%s", "\r\n"); \
                                    }
#endif

#endif // LOG_H
