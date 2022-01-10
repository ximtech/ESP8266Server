#include "StringRingBuffer.h"

static void advanceBufferPointer(StringRingBuffer *ringBuffer);
static void retreatBufferPointer(StringRingBuffer *ringBuffer);


StringRingBuffer *getStringRingBufferInstance(uint32_t bufferSize) {
    if (bufferSize < 1) return NULL;

    StringRingBuffer *instance = malloc(sizeof(struct StringRingBuffer));
    if (instance != NULL) {
        char *buffer = malloc(sizeof(char) * bufferSize);
        if (buffer == NULL) return NULL;
        instance->dataBuffer = buffer;
        instance->maxSize = bufferSize;
        clearStringRingBuffer(instance, bufferSize);
    }
    return instance;
}

void resetStringRingBuffer(StringRingBuffer *ringBuffer) {
    if (ringBuffer != NULL) {
        ringBuffer->head = 0;
        ringBuffer->tail = 0;
        ringBuffer->dataPointer = ringBuffer->dataBuffer;
        ringBuffer->isFull = false;
    }
}

void clearStringRingBuffer(StringRingBuffer *ringBuffer, uint32_t length) {
    if (ringBuffer != NULL) {
        ringBuffer->head = 0;
        ringBuffer->tail = 0;
        ringBuffer->dataPointer = ringBuffer->dataBuffer;
        ringBuffer->isFull = false;

        if (ringBuffer->dataBuffer != NULL) {
            memset(ringBuffer->dataBuffer, 0, (ringBuffer->maxSize <= length) ? ringBuffer->maxSize : length);
        }
    }
}

void stringRingBufferDelete(StringRingBuffer *ringBuffer) {
    if (ringBuffer != NULL) {
        free(ringBuffer->dataBuffer);
        free(ringBuffer);
    }
}

bool isStringRingBufferFull(StringRingBuffer *ringBuffer) {
    return (ringBuffer != NULL) ? ringBuffer->isFull : true;
}

bool isStringRingBufferNotFull(StringRingBuffer *ringBuffer) {
    return !isStringRingBufferFull(ringBuffer);
}

bool isStringRingBufferEmpty(StringRingBuffer *ringBuffer) {
    if (ringBuffer != NULL) {
        return !ringBuffer->isFull && (ringBuffer->head == ringBuffer->tail);
    }
    return true;
}

bool isStringRingBufferNotEmpty(StringRingBuffer *ringBuffer) {
    return !isStringRingBufferEmpty(ringBuffer);
}

uint32_t getStringRingBufferSize(StringRingBuffer *ringBuffer) {
    if (ringBuffer != NULL) {
        uint32_t bufferSize = ringBuffer->maxSize;

        if (!ringBuffer->isFull) {
            if (ringBuffer->head >= ringBuffer->tail) {
                bufferSize = (ringBuffer->head) - (ringBuffer->tail);
            } else {
                bufferSize = (ringBuffer->maxSize + ringBuffer->head - ringBuffer->tail);
            }
        }
        return bufferSize;
    }
    return 0;
}

void stringRingBufferAdd(StringRingBuffer *ringBuffer, char value) {
    if (ringBuffer != NULL && ringBuffer->dataBuffer != NULL) {
        ringBuffer->dataBuffer[ringBuffer->head] = value;
        advanceBufferPointer(ringBuffer);
    }
}

char stringRingBufferGet(StringRingBuffer *ringBuffer) {
    if (ringBuffer != NULL && ringBuffer->dataBuffer != NULL) {
        if (isStringRingBufferNotEmpty(ringBuffer)) {
            char data = ringBuffer->dataBuffer[ringBuffer->tail];
            ringBuffer->dataBuffer[ringBuffer->tail] = '\0';
            retreatBufferPointer(ringBuffer);
            return data;
        }
        return '\0';
    }
    return '\0';
}

static void advanceBufferPointer(StringRingBuffer *ringBuffer) {
    if (ringBuffer->isFull) {
        ringBuffer->tail = (ringBuffer->tail + 1) % ringBuffer->maxSize;
    }
    ringBuffer->head = (ringBuffer->head + 1) % ringBuffer->maxSize;
    ringBuffer->isFull = (ringBuffer->head == ringBuffer->tail);
}

static void retreatBufferPointer(StringRingBuffer *ringBuffer) {
    ringBuffer->isFull = false;
    ringBuffer->tail = (ringBuffer->tail + 1) % ringBuffer->maxSize;
}
