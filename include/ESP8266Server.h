#pragma once

#include <stdarg.h>

#include "HTTPServer.h"
#include "USART_Buffered.h"
#include "DWT_Delay.h"

#define ESP8266_KEEPALIVE_ATTEMPT_COUNT 3
#define ESP8266_INNER_TX_BUFFER_SIZE 2048

typedef enum ESP8266ServerStatus {
    ESP8266_SERVER_SUCCESS,
    ESP8266_SERVER_ERROR,
    ESP8266_SERVER_ERROR_BUFFER_FULL,
    ESP8266_SERVER_TIMEOUT
} ESP8266ServerStatus;

typedef struct ServerIPConfig {
    IPAddress localIP;
    MACAddress localMAC;
} ServerIPConfig;

ServerContext *initServerESP8266(USART_TypeDef *USARTx, ServerConfiguration *configuration);
ServerIPConfig startServerESP8266(ServerContext *context, char *ssid, char *password);

ESP8266ServerStatus startSoftApESP8266(ServerContext *context, char *ssid, char *password, uint16_t channelId, uint8_t encryption);
ESP8266ServerStatus startMulticastDnsESP8266(ServerContext *context, char *host, char *serverName, uint16_t port);

void processServerRequestsESP8266(ServerContext *context);
void sendServerResponseESP8266(ServerContext *context, HTTPStatus status, HashMap headers, const char *body);

void deleteServerESP8266(ServerContext *context);