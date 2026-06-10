#include "reader.h"
#include "log.h"
#include <string.h>

bool isPrintableCharacter(char c);

void Reader::reset()
{
  buffer = "";
}

String Reader::update(char c)
{
  if (c == 0x7f)
  {
    if (buffer.length() > 0) buffer = buffer.substring(0, buffer.length() - 1);
    return String("");
  }

  // Accept all characters (including encrypted data)
  // Only filter out null bytes and line endings (handled separately)
  if (c == '\0') {
    // Ignore null bytes
  } else if (c == '\r' || c == '\n') {
    // Line endings - check if we have a complete line
    buffer += String(c);
  } else {
    buffer += String(c);
  }

  if (!buffer.endsWith(eol))
    return String("");

  String result = buffer.substring(0, buffer.length() - eol.length());
  buffer = "";

  return result;

}

String Reader::getAndClearBadChars() {
  String tmp = badChars;
  badChars = "";
  return tmp;
}

String Reader::peek()
{
  return buffer;
}

bool isPrintableCharacter(char c)
{
  return (c >= 0x20 && c < 0x7f) || c == '\r' || c == '\n';
}
