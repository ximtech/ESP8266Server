# ESP8266 Server

***STM32*** Micro-sized HTTP web server for ESP8266 using AT commands.

### Features

- No external dependencies
- Pending request enqueue
- Multiple clients supported
- Flexible URI matching by pattern
- All types of request supported(GET, POST, PUT, HEAD, DELETE etc.)
- HTTP request parsing and validation
- Chunked response support
- Auto response split if size is larger than ESP8266 inner buffer
- JSON and API call ready
- No extra memory is used

### Add as CPM project dependency

How to add CPM to the project, check the [link](https://github.com/cpm-cmake/CPM.cmake)
```cmake
CPMAddPackage(
        NAME ESP8266Server
        GITHUB_REPOSITORY ximtech/ESP8266Server
        GIT_TAG origin/main)
```

### Project configuration

1. Start project with STM32CubeMX:
    * [USART configuration](https://github.com/ximtech/ESP8266Server/blob/main/example/config_base.PNG)
    * [NVIC configuration](https://github.com/ximtech/ESP8266Server/blob/main/example/config_nvic.PNG)
2. Select: Project Manager -> Advanced Settings -> USART -> LL
3. Generate Code
4. Add sources to project:
```cmake
include_directories(${includes} 
        ${ESP8266_SERVER_DIRECTORY})   # source directories

file(GLOB_RECURSE SOURCES ${sources} 
        ${ESP8266_SERVER_SOURCES})    # source files

add_executable(${PROJECT_NAME}.elf ${SOURCES} ${LINKER_SCRIPT}) # executable declaration should be before libraries

target_link_libraries(${PROJECT_NAME}.elf StringUtils)   # add library dependencies to project
target_link_libraries(${PROJECT_NAME}.elf HTTPServer)
target_link_libraries(${PROJECT_NAME}.elf JSON)         # add if JSON is needed
```

3. Then Build -> Clean -> Rebuild Project

### Wiring

- <img src="https://github.com/ximtech/ESP8266Server/blob/main/example/pinout.PNG" alt="image" width="300"/>
- <img src="https://github.com/ximtech/ESP8266Server/blob/main/example/wiring.PNG" alt="image" width="300"/>
- <img src="https://github.com/ximtech/ESP8266Server/blob/main/example/wiring_2.PNG" alt="image" width="300"/>

## Usage
***Provide interrupt handler***
- Full example: [link](https://github.com/ximtech/ESP8266Server/blob/main/example/stm32f4xx_it.c)
```c
/**
  * @brief This function handles USART1 global interrupt.
  */
void USART1_IRQHandler(void) {
    interruptCallbackUSART1();
}
```
***The following example for base application***
```c
#include "ESP8266HTTPServer.h"
#include "JSON.h"

static void handleRoot(ServerContext *context, HTTPParser *request) {
    HashMap headers = request->headers;
    hashMapClear(headers);  // reuse parsed headers
    hashMapPut(headers, "Content-Type", "text/html; charset=UTF-8");    // set custom headers
    hashMapPut(headers, "Connection", "close");
    sendServerResponseESP8266(context, HTTP_OK, headers, HTML_PAGE); // large char array html, tested with 12k. Auto chunked response is enabled
}

static void handleJson(ServerContext *context, HTTPParser *request) {   // work with JSON
    HashMap headers = request->headers;
    hashMapClear(headers);
    hashMapPut(headers, "Content-Type", "application/json");    // set response content type

    Regex regex;
    regexCompile(&regex, "\\d+");
    Matcher matcher = regexMatch(&regex, request->uriPath);
    char *valuePointer = &request->uriPath[matcher.foundAtIndex];   // extract path variable. Example: "/api/1234/test" -> 1234
    valuePointer[matcher.matchLength] = '\0';
    printf("%s\n", valuePointer);

    JSONTokener jsonTokener = createEmptyJSONTokener(); // create response JSON
    JSONObject rootObject = createJsonObject(&jsonTokener);
    jsonObjectPut(&rootObject, "requestId", valuePointer);  // set values
    jsonObjectPut(&rootObject, "humidity", "86");
    jsonObjectPut(&rootObject, "temperature", "23.12");
    jsonObjectPut(&rootObject, "description", "text");

    char buffer[ESP8266_INNER_TX_BUFFER_SIZE] = {0};    // Max buffer size for AT API is 2048
    jsonObjectToStringPretty(&rootObject, buffer, 3, 0);    // format JSON with idents - 3 and root level - 0
    deleteJSONObject(&rootObject);  // free resources
    sendServerResponseESP8266(context, HTTP_OK, request->headers, buffer); // send response
}

static void handleNotFound(ServerContext *context, HTTPParser *request) {
    char *message = hashMapGetOrDefault(request->queryParameters, "message", "No message sent");// get value from query params

    HashMap headers = request->headers;
    hashMapClear(headers);
    hashMapPut(headers, "Content-Type", "text/html; charset=UTF-8");
    hashMapPut(headers, "Connection", "close");

    char content[200];
    snprintf(content, sizeof(content), "<html><title>Not Found</title>Not found: %s</html>", request->uriPath);
    sendServerResponseESP8266(context, HTTP_NOT_FOUND, headers, content);
}

int main(void) {
    
    ServerConfiguration configuration = {0};
    configuration.serverPort = 80;
    configuration.serverTimeoutMs = 5000;
    configuration.rxDataBufferSize = 5000;
    configuration.defaultHandler = handleNotFound;

    ServerContext *context = initServerESP8266(USART1, &configuration);
    if (context == NULL) {
        printf("Failed to initialize ESP8266\n");
        return -1;
    }

    // Regex pattern URI
    addUrlMapping(context, "^/$", HTTP_GET, handleRoot);
    addUrlMapping(context, "^/api/\\d+/test$", HTTP_GET, handleJson);   // Example: /api/1234/test

    ServerIPConfig ipConfig = startServerESP8266(context, "SSID", "WIFI_PASSWORD");
    printf("IP: %s\n", ipConfig.localIP.octetsIPv4);

    while (1) {

        processServerRequestsESP8266(context);

    }
}
```