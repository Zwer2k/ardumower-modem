#pragma once

#include <Arduino.h>

#define READER_BUF_SIZE 4096

class Reader
{
private:
  String eol;
  char buffer[READER_BUF_SIZE];
  uint16_t pos;
  String badChars;

  void ensureCapacity(uint16_t needed);

public:
  Reader(String _eol) : eol(_eol), pos(0) { buffer[0] = '\0'; }
  void reset();
  String update(char c);
  String peek();
  String getAndClearBadChars();
};
