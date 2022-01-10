#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

typedef struct StringRingBuffer {
    uint32_t head;
    uint32_t tail;
    uint32_t maxSize;
    char *dataPointer;
    char *dataBuffer;
    bool isFull;
} StringRingBuffer;

StringRingBuffer *getStringRingBufferInstance(uint32_t bufferSize);

void resetStringRingBuffer(StringRingBuffer *ringBuffer);
void clearStringRingBuffer(StringRingBuffer *ringBuffer, uint32_t length);
void stringRingBufferDelete(StringRingBuffer *ringBuffer);

bool isStringRingBufferFull(StringRingBuffer *ringBuffer);
bool isStringRingBufferNotFull(StringRingBuffer *ringBuffer);
bool isStringRingBufferEmpty(StringRingBuffer *ringBuffer);
bool isStringRingBufferNotEmpty(StringRingBuffer *ringBuffer);

uint32_t getStringRingBufferSize(StringRingBuffer *ringBuffer);
void stringRingBufferAdd(StringRingBuffer *ringBuffer, char value);
char stringRingBufferGet(StringRingBuffer *ringBuffer);