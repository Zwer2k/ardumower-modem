#pragma once

#include <Arduino.h>

class Reader
{
private:
  String eol;
  String buffer;
  String badChars;

public:
  Reader(String _eol) : eol(_eol), buffer(""), badChars("") {}
  void reset();
  String update(char c);
  String peek();
  String getAndClearBadChars();
};
