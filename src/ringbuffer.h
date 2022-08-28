#ifndef __RINGBUFFER_H__
#define __RINGBUFFER_H__

#include <Arduino.h>

template <typename BUFFTYPE, uint16_t BUFFSIZE>
class Ringbuffer {
private:
    BUFFTYPE buffer[BUFFSIZE];
    volatile uint16_t readPos, size;
    SemaphoreHandle_t _lock;

public:
    Ringbuffer();
    bool push(const BUFFTYPE * const value, bool force = false) __attribute__ ((noinline));
    bool pull(BUFFTYPE &value) __attribute__ ((noinline));
    bool contains(const BUFFTYPE * const value) __attribute__ ((noinline));
    uint16_t counterEqual(const BUFFTYPE * const value);
    void clear()   { xSemaphoreTake(_lock, portMAX_DELAY); size = 0; xSemaphoreGive(_lock); }
    uint16_t currentSize() { uint16_t ret; xSemaphoreTake(_lock, portMAX_DELAY); ret = size; xSemaphoreGive(_lock); return ret; }
    uint16_t freeSize()  { uint16_t ret; xSemaphoreTake(_lock, portMAX_DELAY); ret = BUFFSIZE - size; xSemaphoreGive(_lock); return ret; }
    bool isFull() { bool ret; xSemaphoreTake(_lock, portMAX_DELAY); ret = size == BUFFSIZE; xSemaphoreGive(_lock); return ret; }
    bool isEmpty() { bool ret; xSemaphoreTake(_lock, portMAX_DELAY); ret = size == 0; xSemaphoreGive(_lock);  return ret; }
    uint16_t maxSize() { return BUFFSIZE; }
};

template <typename BUFFTYPE, uint16_t BUFFSIZE>
Ringbuffer<BUFFTYPE, BUFFSIZE>::Ringbuffer()
{
    readPos = 0;
    size = 0;
    _lock = xSemaphoreCreateBinary();
    xSemaphoreGive(_lock);
}

template <typename BUFFTYPE, uint16_t BUFFSIZE>
bool Ringbuffer<BUFFTYPE, BUFFSIZE>::push(const BUFFTYPE * const value, bool force)
{
    xSemaphoreTake(_lock, portMAX_DELAY);
    if (size == BUFFSIZE) { 
        if (force) {
          readPos++;
          size--;
          if (readPos == BUFFSIZE) 
            readPos = 0;        
        } else {
          xSemaphoreGive(_lock);
          return false;
        }
    }

    uint16_t writePos = readPos + size;
    if (writePos >= BUFFSIZE) 
        writePos -= BUFFSIZE;
    buffer[writePos] = *value;
    size++;
    xSemaphoreGive(_lock);
    return true;
}

template <typename BUFFTYPE, uint16_t BUFFSIZE>
bool Ringbuffer<BUFFTYPE, BUFFSIZE>::pull(BUFFTYPE &value)
{
  xSemaphoreTake(_lock, portMAX_DELAY);
  if (size == 0) {
    xSemaphoreGive(_lock);
    return false;
  }
  value = buffer[readPos];
  readPos++;
  size--;
  if (readPos == BUFFSIZE) 
    readPos = 0;
  xSemaphoreGive(_lock);
  return true;
}

template <typename BUFFTYPE, uint16_t BUFFSIZE>
bool Ringbuffer<BUFFTYPE, BUFFSIZE>::contains(const BUFFTYPE * const value)
{
    xSemaphoreTake(_lock, portMAX_DELAY);
    if (size == 0) {
        xSemaphoreGive(_lock);
        return false;
    }

    for (uint16_t i = 0, j = readPos; i < size; i++) {
      if (j == BUFFSIZE) 
        j = 0;
      if (buffer[j++] == *value) {
        xSemaphoreGive(_lock);
        return true;
      }
    }
    
    xSemaphoreGive(_lock);
    return false;
}

template <typename BUFFTYPE, uint16_t BUFFSIZE>
uint16_t Ringbuffer<BUFFTYPE, BUFFSIZE>::counterEqual(const BUFFTYPE * const value)
{
    xSemaphoreTake(_lock, portMAX_DELAY);
    if (size == 0) {
        xSemaphoreGive(_lock);
        return false;
    }

    uint16_t count = 0;
    for (uint16_t i = 0, j = readPos; i < size; i++) {
      if (j == BUFFSIZE) 
        j = 0;
      if (buffer[j++] == *value)
        count++;
    }
    
    xSemaphoreGive(_lock);
    return count;
}

#endif /* __RINGBUFFER_H__*/