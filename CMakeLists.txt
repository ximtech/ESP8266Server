cmake_minimum_required(VERSION 3.20)
project(ESP8266Server VERSION 1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 99)

include(cmake/CPM.cmake)

CPMAddPackage(
        NAME DWTDelay
        GITHUB_REPOSITORY ximtech/DWTDelay
        GIT_TAG origin/main)

CPMAddPackage(
        NAME StringUtils
        GITHUB_REPOSITORY ximtech/StringUtils
        GIT_TAG origin/main)

CPMAddPackage(
        NAME HTTPServer
        GITHUB_REPOSITORY ximtech/HTTPServer
        GIT_TAG origin/main)

CPMAddPackage(
        NAME JSON
        GITHUB_REPOSITORY ximtech/JSON
        GIT_TAG origin/main)

set(ESP8266_SERVER_DIRECTORY
        ${DWT_DELAY_DIRECTORY}
        ${ESP8266Server_SOURCE_DIR}
        ${ESP8266Server_SOURCE_DIR}/include
        CACHE STRING "ESP8266 server directories include to the main project" FORCE)

set(ESP8266_SERVER_SOURCES
        ${DWT_DELAY_SOURCES}
        ${ESP8266Server_SOURCE_DIR}/include/ESP8266HTTPServer.h
        ${ESP8266Server_SOURCE_DIR}/include/StringRingBuffer.h
        ${ESP8266Server_SOURCE_DIR}/include/USART_Buffered.h
        ${ESP8266Server_SOURCE_DIR}/ESP8266HTTPServer.c
        ${ESP8266Server_SOURCE_DIR}/StringRingBuffer.c
        ${ESP8266Server_SOURCE_DIR}/USART_Buffered.c
        CACHE STRING "ESP8266 server source files include to the main project" FORCE)

link_libraries(StringUtils HTTPServer JSON)