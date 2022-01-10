#pragma once

#include <stdio.h>
#include <stdarg.h>

#include "main.h"
#include "StringRingBuffer.h"

typedef struct USART {
    USART_TypeDef *USARTx;
    StringRingBuffer *RxBuffer;
    StringRingBuffer *TxBuffer;
} USART;

USART *initBufferedUSART(USART_TypeDef *USARTx, uint32_t rxBufferSize, uint32_t txBufferSize);

void interruptCallbackUSART1();// Interrupt callback functions, use by specific UART number at stm32f4xx_it.c
void interruptCallbackUSART2();
void interruptCallbackUSART6();

void sendByteUSART(USART *USARTPointer, uint8_t byte);
void sendStringUSART(USART *USARTPointer, const char *string);
void sendFormattedStringUSART(USART *USARTPointer, uint16_t bufferLength, char *format, ...);

uint8_t readByteUSART(USART *USARTPointer);
void readStringUSART(USART *USARTPointer, char *charArray);
void readStringForLengthUSART(USART *USARTPointer, char *charArray, uint32_t length);
void readStringUntilStopCharUSART(USART *USARTPointer, char *charArray, char stopChar);

void deleteUSART(USART *USARTPointer);


// Helper functions
static inline bool isRxBufferEmptyUSART(USART *USARTPointer) {
    return isStringRingBufferEmpty(USARTPointer->RxBuffer);    // buffer empty if no bytes received
}

static inline bool isRxBufferNotEmptyUSART(USART *USARTPointer) {
    return isStringRingBufferNotEmpty(USARTPointer->RxBuffer);
}

static inline bool isRxBufferFullUSART(USART *USARTPointer) {
    return isStringRingBufferFull(USARTPointer->RxBuffer);
}

static inline void resetRxBufferUSART(USART *USARTPointer) {
    resetStringRingBuffer(USARTPointer->RxBuffer);
}

static inline void resetTxBufferUSART(USART *USARTPointer) {
    resetStringRingBuffer(USARTPointer->TxBuffer);
}