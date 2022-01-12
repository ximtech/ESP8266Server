#include "ESP8266Server.h"

#define COMMAND_MAX_LENGTH 100
#define TMP_TX_BUFFER_MAX_LENGTH 100

#define DATA_RECEIVED_STATUS  "+IPD,"
#define SEND_OK_STATUS        "\r\nSEND OK\r\n"
#define SEND_FAIL_STATUS      "\r\nSEND FAIL\r\n"
#define OK_STATUS             "\r\nOK\r\n"
#define CLOSED_STATUS         "CLOSED\r\n"
#define ERROR_STATUS          "\r\nERROR\r\n"
#define FAIL_STATUS           "\r\nFAIL\r\n"
#define NEW_LINE              "\r\n"
#define READY_TO_RECEIVE_DATA ">"

#define ESP8266_ADDITIONAL_HEADER_LENGTH 100
#define ESP8266_STATION_AND_AP 3
#define ESP8266_SHOW_REQUEST_IP_AND_PORT 1
#define ESP8266_DISABLE_AUTO_CONNECT_TO_AP 0
#define ESP8266_CONNECTION_MULTIPLE 1
#define ESP8266_MAX_ALLOWED_TIMEOUT 7200
#define ESP8266_CREATE_SERVER_MODE 1
#define ESP8266_DELETE_SERVER_MODE 0

#define ESP8266_MAX_SSID_LENGTH 32
#define ESP8266_MAX_PASSWORD_LENGTH 64

#define ESP8266_DATA_MARKER_MAX_LENGTH 100
#define ESP8266_REQUEST_END_MARKER_LENGTH 4
#define ESP8266_IPD_MARKER_REQUEST_ID_INDEX 5
#define ESP8266_ALL_CONNECTIONS_ID 5

static USART *USARTInstance = NULL;
static HTTPParser *httpParser = NULL;

static StringRingBuffer *tmpTxBuffer = NULL;
static char commandBuffer[COMMAND_MAX_LENGTH];

static ESP8266ServerStatus sendATCommand(ServerContext *context, const char *ATCommandPattern, ...);
static ESP8266ServerStatus readCommandResponse(ServerContext *context, char *rxBufferPointer);

static inline bool isResponseOK(char *responseBody);
static inline bool isResponseError(char *responseBody);
static inline bool isSsidValid(char *ssid);
static inline bool isPasswordValid(char *password);

static void getIPDMarkerValue(char *rawRequest, char *valueBuffer);
static IPAddress parseRequestIPAddress(char *requestPointer);

static ESP8266ServerStatus sendSingleResponse(ServerContext *context, HashMap headers, const char *body, uint32_t bodyLength, char *commandResponsePointer);
static ESP8266ServerStatus sendChunkedResponse(ServerContext *context, HashMap headers, const char *body, uint32_t bodyLength, char *commandResponsePointer);
static ESP8266ServerStatus sendHTTPResponseESP8266(ServerContext *context, uint32_t dataLength, char *commandResponsePointer);
static void closeConnectionESP8266(ServerContext *context, uint32_t connectionId, char *commandResponsePointer);


ServerContext *initServerESP8266(USART_TypeDef *USARTx, ServerConfiguration *configuration) {
    ServerContext *context = initHTTPServerContext(configuration);
    if (context == NULL) return NULL;
    tmpTxBuffer = getStringRingBufferInstance(TMP_TX_BUFFER_MAX_LENGTH);
    USARTInstance = initBufferedUSART(USARTx, configuration->rxDataBufferSize, ESP8266_INNER_TX_BUFFER_SIZE);
    httpParser = getHttpParserInstance();

    if (tmpTxBuffer == NULL || USARTInstance == NULL || httpParser == NULL) {
        deleteServerESP8266(context);
        return NULL;
    }

    context->txDataBufferPointer = USARTInstance->TxBuffer->dataBuffer;
    dwtDelayInit();
    delay_ms(100);    // initial delay

    sendATCommand(context, "AT+RST");
    delay_ms(5000);

    if (sendATCommand(context, "AT") != ESP8266_SERVER_SUCCESS) {
        deleteServerESP8266(context);
        return NULL;
    }
    sendATCommand(context, "AT+GMR");
    return context;
}

ServerIPConfig startServerESP8266(ServerContext *context, char *ssid, char *password) {
    ServerIPConfig serverConfig = {0};
    if (!isSsidValid(ssid) || !isPasswordValid(password)) return serverConfig;
    sendATCommand(context, "AT+CWMODE_DEF=%d", ESP8266_STATION_AND_AP);
    sendATCommand(context, "AT+CWAUTOCONN=%d", ESP8266_DISABLE_AUTO_CONNECT_TO_AP);
    sendATCommand(context, "AT+CIPDINFO=%d", ESP8266_SHOW_REQUEST_IP_AND_PORT);    // show ip with +IPD
    sendATCommand(context, "AT+CWJAP_CUR=\"%s\",\"%s\"", ssid, password);

    if (sendATCommand(context, "AT+CIFSR") == ESP8266_SERVER_SUCCESS) {
        char *responseBody = USARTInstance->RxBuffer->dataPointer;

        char dataBuffer[20] = {[0 ... 20 - 1] = 0};
        substringString("STAIP,\"", "\"", responseBody, dataBuffer);
        serverConfig.localIP = ipAddressFromString(dataBuffer);

        memset(dataBuffer, 0, 20);
        substringString("STAMAC,\"", "\"", responseBody, dataBuffer);
        serverConfig.localMAC = macAddressFromString(dataBuffer);

        sendATCommand(context, "AT+CIPMUX=%d", ESP8266_CONNECTION_MULTIPLE);
        sendATCommand(context, "AT+CIPSERVER=%d,%d", ESP8266_CREATE_SERVER_MODE, context->configuration->serverPort);

        if (context->configuration->serverTimeoutMs <= ESP8266_MAX_ALLOWED_TIMEOUT) {
            sendATCommand(context, "AT+CIPSTO=%d", context->configuration->serverTimeoutMs);
        }
        context->isServerRunning = true;
    }

    clearStringRingBuffer(USARTInstance->RxBuffer, USARTInstance->RxBuffer->maxSize);
    clearStringRingBuffer(USARTInstance->TxBuffer, USARTInstance->TxBuffer->maxSize);
    return serverConfig;
}

ESP8266ServerStatus startSoftApESP8266(ServerContext *context, char *ssid, char *password, uint16_t channelId, uint8_t encryption) {
    return sendATCommand(context, "AT+CWSAP_DEF=\"%s\",\"%s\",%d,%d", ssid, password, channelId, encryption);
}

ESP8266ServerStatus startMulticastDnsESP8266(ServerContext *context, char *host, char *serverName, uint16_t port) {
    return sendATCommand(context, "AT+MDNS=1,\"%s\",\"%s\",%d", host, serverName, port);
}

void processServerRequestsESP8266(ServerContext *context) {
    if (isStringRingBufferFull(USARTInstance->RxBuffer)) {
        LL_USART_DisableIT_RXNE(USARTInstance->USARTx);
        resetRxBufferUSART(USARTInstance);
        LL_USART_EnableIT_RXNE(USARTInstance->USARTx);
    }

    char *requestBody = USARTInstance->RxBuffer->dataPointer;
    if (requestBody[0] != '\0') {   // check that rx buffer is not empty
        char *requestStartPointer = strstr(requestBody, "+IPD,");
        if (requestStartPointer == NULL) return;
        char *requestEndPointer = strstr(requestStartPointer, "\r\n\r\n");
        if (requestEndPointer == NULL) return;

        static char ipdMarker[ESP8266_DATA_MARKER_MAX_LENGTH];
        memset(ipdMarker, 0, ESP8266_DATA_MARKER_MAX_LENGTH);
        getIPDMarkerValue(requestStartPointer, ipdMarker);  // ESP8266 Data received +IPD marker
        if (isStringEmpty(ipdMarker)) return;
        requestStartPointer += strlen(ipdMarker);

        context->socketId = ipdMarker[ESP8266_IPD_MARKER_REQUEST_ID_INDEX] - '0';   // convert char to id. Example +IPD,0,... -> id is at index 5
        context->requestIP = parseRequestIPAddress(ipdMarker);
        uint32_t requestLength = (requestEndPointer - requestBody) + ESP8266_REQUEST_END_MARKER_LENGTH;
        USARTInstance->RxBuffer->dataPointer += requestLength;  // set to the next request

        parseHttpBuffer(requestStartPointer, httpParser, HTTP_REQUEST);
        if (httpParser->parserStatus == HTTP_PARSE_OK) {
            parseHttpHeaders(httpParser, requestStartPointer);
            parseHttpQueryParameters(httpParser, requestStartPointer);
            RequestHandlerFunction handlerFunction = handleIncomingServerRequest(context, httpParser);
            handlerFunction(context, httpParser);
        }

        bool havePendingRequests = strstr(USARTInstance->RxBuffer->dataPointer, "+IPD,") != NULL;    // check for pending requests
        if (!havePendingRequests || httpParser->parserStatus != HTTP_PARSE_OK) {  // shrink rx buffer if no new requests arrived
            LL_USART_DisableIT_RXNE(USARTInstance->USARTx);
            clearStringRingBuffer(USARTInstance->RxBuffer, USARTInstance->RxBuffer->head);
            LL_USART_EnableIT_RXNE(USARTInstance->USARTx);
        }
    }
}

void sendServerResponseESP8266(ServerContext *context, HTTPStatus status, HashMap headers, const char *body) {
    if (headers == NULL) return;
    hashMapPut(headers, "Server", SERVER_NAME);
    hashMapPut(headers, "Cache-Control", "no-cache");
    hashMapPut(headers, "Pragma", "no-cache");
    hashMapPut(headers, "Accept-Ranges", "bytes");

    uint32_t statusLineLength = formatHTTPServerStatusLine(context->txDataBufferPointer, status);
    uint32_t headersLength = getHTTPServerHeadersLength(headers);
    uint32_t bodyLength = isStringNotBlank(body) ? strlen(body) : 0;
    uint32_t totalResponseLength = statusLineLength + headersLength + bodyLength;

    uint32_t bytesInRxBuffer = (USARTInstance->RxBuffer->dataPointer - USARTInstance->RxBuffer->dataBuffer);
    char *commandResponsePointer = USARTInstance->RxBuffer->dataPointer + (USARTInstance->RxBuffer->head - bytesInRxBuffer);

    if (totalResponseLength <= (ESP8266_INNER_TX_BUFFER_SIZE - ESP8266_ADDITIONAL_HEADER_LENGTH)) {
        ESP8266ServerStatus responseSendStatus = sendSingleResponse(context, headers, body, bodyLength, commandResponsePointer);
        if (responseSendStatus != ESP8266_SERVER_SUCCESS) {
            return;
        }

    } else if (totalResponseLength >= ESP8266_INNER_TX_BUFFER_SIZE) {
        if (isStringNotEquals(httpParser->httpVersion, SERVER_HTTP_VERSION)) {   //Not to send transfer-encoded messages to non-HTTP/1.1 applications.
            formatHTTPServerStatusLine(context->txDataBufferPointer, HTTP_NOT_IMPLEMENTED);
            sendSingleResponse(context, headers, NULL, 0, commandResponsePointer);
            closeConnectionESP8266(context, context->socketId, commandResponsePointer);
            return;
        }

        ESP8266ServerStatus responseSendStatus = sendChunkedResponse(context, headers, body, bodyLength, commandResponsePointer);
        if (responseSendStatus != ESP8266_SERVER_SUCCESS) {
            return;
        }
    }

    char *connectionStatus = hashMapGet(headers, getHeaderValueByKey(CONNECTION));
    if (isStringEquals(connectionStatus, "close")) {
        closeConnectionESP8266(context, context->socketId, commandResponsePointer);
    }
}

void deleteServerESP8266(ServerContext *context) {
    deleteHTTPServer(context);
    deleteUSART(USARTInstance);
    stringRingBufferDelete(tmpTxBuffer);
    deleteHttpParser(httpParser);
    httpParser = NULL;
    tmpTxBuffer = NULL;
}

static ESP8266ServerStatus sendATCommand(ServerContext *context, const char *ATCommandPattern, ...) {
    resetTxBufferUSART(USARTInstance);
    memset(commandBuffer, 0, COMMAND_MAX_LENGTH);
    va_list valist;
    va_start(valist, ATCommandPattern);
    vsprintf(commandBuffer, ATCommandPattern, valist);
    va_end(valist);
    strcat(commandBuffer, NEW_LINE);  // ESP8266 expects <CR><LF> or CarriageReturn and LineFeed at the end of each command

    for (uint8_t i = 0; i < ESP8266_KEEPALIVE_ATTEMPT_COUNT; i++) {
        clearStringRingBuffer(USARTInstance->RxBuffer, COMMAND_MAX_LENGTH);
        sendStringUSART(USARTInstance, commandBuffer);
        ESP8266ServerStatus status = readCommandResponse(context, USARTInstance->RxBuffer->dataPointer);
        if (status != ESP8266_SERVER_TIMEOUT) {
            return status;
        }
    }
    return ESP8266_SERVER_TIMEOUT;
}

static ESP8266ServerStatus readCommandResponse(ServerContext *context, char *rxBufferPointer) {
    uint32_t startTimeMillis = currentMilliSeconds();
    ESP8266ServerStatus status = ESP8266_SERVER_TIMEOUT;
    uint32_t byteCountInBuffer;

    while (true) {
        if ((currentMilliSeconds() - startTimeMillis) >= context->configuration->serverTimeoutMs) {
            break;
        }

        byteCountInBuffer = getStringRingBufferSize(USARTInstance->RxBuffer);
        if (isRxBufferNotEmptyUSART(USARTInstance)) {
            delay_ms(1);

            if (byteCountInBuffer == getStringRingBufferSize(USARTInstance->RxBuffer)) { // check that no new data is arrived
                if (isResponseOK(rxBufferPointer)) {
                    return ESP8266_SERVER_SUCCESS;
                } else if (isResponseError(rxBufferPointer)) {
                    return ESP8266_SERVER_ERROR;
                } else if (isRxBufferFullUSART(USARTInstance)) {
                    return ESP8266_SERVER_ERROR_BUFFER_FULL;
                }
            }
        }
        delay_ms(1);
    }

    return status;
}

static inline bool isResponseOK(char *responseBody) {
    return strstr(responseBody, OK_STATUS) ||
           strstr(responseBody, CLOSED_STATUS) ||
           strstr(responseBody, READY_TO_RECEIVE_DATA);
}

static inline bool isResponseError(char *responseBody) {    // find for "error" or "fail" response status
    return strstr(responseBody, ERROR_STATUS) ||
           strstr(responseBody, FAIL_STATUS) ||
           strstr(responseBody, SEND_FAIL_STATUS);
}

static inline bool isSsidValid(char *ssid) {
    return (isStringNotBlank(ssid) && strlen(ssid) < ESP8266_MAX_SSID_LENGTH);
}

static inline bool isPasswordValid(char *password) {
    return (isStringNotBlank(password) && strlen(password) < ESP8266_MAX_PASSWORD_LENGTH);
}

static void getIPDMarkerValue(char *rawRequest, char *valueBuffer) {
    char *dataMarker = rawRequest;
    uint8_t markerCounter = 0;
    while (*dataMarker != '\0' && markerCounter < ESP8266_DATA_MARKER_MAX_LENGTH) {
        if (*dataMarker == ':') {
            valueBuffer[markerCounter] = *dataMarker;
            break;
        }
        valueBuffer[markerCounter] = *dataMarker;
        markerCounter++;
        dataMarker++;
    }
}

static IPAddress parseRequestIPAddress(char *requestPointer) {
    static char ipAddressBuffer[50] = {0};
    substringString(",", ":", requestPointer, ipAddressBuffer);

    char *nextPointer;
    char *token = splitStringReentrant(ipAddressBuffer, ",", &nextPointer);
    for (int i = 0; i < 2; i++) {// skip request id and size
        token = splitStringReentrant(NULL, ",", &nextPointer);
    }
    return ipAddressFromString(token);
}

static ESP8266ServerStatus sendSingleResponse(ServerContext *context, HashMap headers, const char *body, uint32_t bodyLength, char *commandResponsePointer) {
    char dataLengthBuffer[sizeof(uint32_t) * 8 + 1] = {0};    // u32 max length
    sprintf(dataLengthBuffer, "%lu", bodyLength);
    hashMapPut(headers, "Content-Length", dataLengthBuffer);

    formatHTTPServerHeaders(context->txDataBufferPointer, headers);
    formatHTTPServerResponseBody(context, body);

    uint32_t totalResponseLength = strlen(context->txDataBufferPointer);
    USARTInstance->TxBuffer->tail = 0;
    USARTInstance->TxBuffer->head = totalResponseLength;
    ESP8266ServerStatus responseSendStatus = sendHTTPResponseESP8266(context, totalResponseLength, commandResponsePointer);
    if (responseSendStatus != ESP8266_SERVER_SUCCESS) {
        closeConnectionESP8266(context, ESP8266_ALL_CONNECTIONS_ID, commandResponsePointer);
        return responseSendStatus;
    }
    return responseSendStatus;
}

static ESP8266ServerStatus sendChunkedResponse(ServerContext *context, HashMap headers, const char *body, uint32_t bodyLength, char *commandResponsePointer) {
    hashMapRemove(headers, "Content-Length");   // removing content length header for chunked response if present
    hashMapPut(headers, "Transfer-Encoding", "chunked");
    formatHTTPServerHeaders(context->txDataBufferPointer, headers);

    uint32_t totalResponseLength = strlen(context->txDataBufferPointer);
    USARTInstance->TxBuffer->tail = 0;
    USARTInstance->TxBuffer->head = totalResponseLength;
    ESP8266ServerStatus responseSendStatus = sendHTTPResponseESP8266(context, totalResponseLength, commandResponsePointer);
    if (responseSendStatus != ESP8266_SERVER_SUCCESS) {
        closeConnectionESP8266(context, ESP8266_ALL_CONNECTIONS_ID, commandResponsePointer);
        return responseSendStatus;
    }

    ChunkedResponse chunkedResponse = prepareChunkedResponse(ESP8266_INNER_TX_BUFFER_SIZE, body, bodyLength);
    while (hasNextHTTPChunk(&chunkedResponse, context->txDataBufferPointer)) {
        totalResponseLength = strlen(context->txDataBufferPointer);

        USARTInstance->TxBuffer->tail = 0;
        USARTInstance->TxBuffer->head = totalResponseLength;
        responseSendStatus = sendHTTPResponseESP8266(context, totalResponseLength, commandResponsePointer);
        if (responseSendStatus != ESP8266_SERVER_SUCCESS) {
            closeConnectionESP8266(context, ESP8266_ALL_CONNECTIONS_ID, commandResponsePointer);
            return responseSendStatus;
        }
    }
    return responseSendStatus;
}

static ESP8266ServerStatus sendHTTPResponseESP8266(ServerContext *context, uint32_t dataLength, char *commandResponsePointer) {
    StringRingBuffer *txBufferPointer = USARTInstance->TxBuffer; // save base tx buffer
    USARTInstance->TxBuffer = tmpTxBuffer;  // set tmp tx buffer for command sending

    LL_USART_DisableIT_RXNE(USARTInstance->USARTx); // turn off receiver while data transmission
    memset(commandBuffer, 0, COMMAND_MAX_LENGTH);
    sprintf(commandBuffer, "AT+CIPSEND=%lu,%lu\r\n", context->socketId, dataLength);
    sendStringUSART(USARTInstance, commandBuffer);
    while (isStringRingBufferNotEmpty(USARTInstance->TxBuffer));
    LL_USART_EnableIT_RXNE(USARTInstance->USARTx);  // data is sent, enable receiver
    ESP8266ServerStatus serverStatus = readCommandResponse(context, commandResponsePointer);

    if (serverStatus == ESP8266_SERVER_SUCCESS) { // check that module ready to receive data
        LL_USART_DisableIT_RXNE(USARTInstance->USARTx); // disable receiver while data send, preventing deadlock
        USARTInstance->TxBuffer = txBufferPointer;    // return already formatted response buffer
        LL_USART_EnableIT_TXE(USARTInstance->USARTx);   // transmit response data
        while (isStringRingBufferNotEmpty(USARTInstance->TxBuffer));    // wait until all data is sent
        LL_USART_EnableIT_RXNE(USARTInstance->USARTx);  // data is sent, enable receiver

        uint32_t startTimeMillis = currentMilliSeconds();
        while (!strstr(commandResponsePointer, SEND_OK_STATUS)) {  // wait for data send
            if ((currentMilliSeconds() - startTimeMillis) >= context->configuration->serverTimeoutMs) {
                serverStatus = ESP8266_SERVER_TIMEOUT;
                break;
            }

            if (strstr(commandResponsePointer, SEND_FAIL_STATUS) ||
                strstr(commandResponsePointer, ERROR_STATUS)) {// new request can occur while response send and close previous connection
                serverStatus = ESP8266_SERVER_ERROR;
                break;
            }
            delay_ms(1);
        }
    }

    uint32_t commandResponseLength = strlen(commandResponsePointer);
    memset(commandResponsePointer, 0, commandResponseLength);
    USARTInstance->RxBuffer->head -= commandResponseLength;
    USARTInstance->TxBuffer = txBufferPointer;
    return serverStatus;
}

static void closeConnectionESP8266(ServerContext *context, uint32_t connectionId, char *commandResponsePointer) {
    memset(commandBuffer, 0, COMMAND_MAX_LENGTH);
    sprintf(commandBuffer, "AT+CIPCLOSE=%lu\r\n", connectionId);

    LL_USART_DisableIT_RXNE(USARTInstance->USARTx);
    sendStringUSART(USARTInstance, commandBuffer);
    while (isStringRingBufferNotEmpty(USARTInstance->TxBuffer));
    LL_USART_EnableIT_RXNE(USARTInstance->USARTx);
    readCommandResponse(context, commandResponsePointer);
}