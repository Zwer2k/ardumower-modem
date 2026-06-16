#ifndef _CHECKSUM_H
#define _CHECKSUM_H

#include <Arduino.h>
#include <string.h>

namespace ArduMower
{
  class Checksum
  {
  private:
    unsigned char val;
  public:
    Checksum() : val(0) {}

    void update(char c) { val += c; };

    void update(const String& s) {
      for(unsigned int i=0; i < s.length(); i++) {
        update(s[i]);
      }
    }

    void update(const char* s) {
      while (*s) update(*s++);
    }

    unsigned char value() { return val; }
  };  
}

#endif
