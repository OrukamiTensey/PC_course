#pragma once
#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <cstdint>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

// Константи команд протоколу
enum CommandType : uint32_t {
    CMD_CONFIG = 1,
    CMD_START = 2,
    CMD_STATUS = 3,
    CMD_GET_RESULT = 4
};

// Константи відповідей сервера
enum ResponseType : uint32_t {
    RES_OK = 100,
    RES_STARTED = 101,
    RES_IN_PROGRESS = 102,
    RES_DONE = 103,
    RES_ERROR = 200
};

// Структура фіксованого заголовка пакету
struct PacketHeader {
    uint32_t command;       // Тип команди або відповіді
    uint32_t payloadLength; // Довжина даних, які йдуть за заголовком
};

// Структура конфігурації, яка передається від клієнта
struct ConfigPayload {
    int32_t k;
    uint32_t arraySize;
    uint32_t numThreads;
};

// Допоміжні функції для безпечного читання/запису з урахуванням порядку байтів
inline void hostToNetwork(PacketHeader& header) {
    header.command = htonl(header.command);
    header.payloadLength = htonl(header.payloadLength);
}

inline void networkToHost(PacketHeader& header) {
    header.command = ntohl(header.command);
    header.payloadLength = ntohl(header.payloadLength);
}

inline void hostToNetwork(ConfigPayload& config) {
    config.k = htonl(config.k);
    config.arraySize = htonl(config.arraySize);
    config.numThreads = htonl(config.numThreads);
}

inline void networkToHost(ConfigPayload& config) {
    config.k = ntohl(config.k);
    config.arraySize = ntohl(config.arraySize);
    config.numThreads = ntohl(config.numThreads);
}

// Функція для гарантованого відправлення всього буфера через сокет
inline bool sendAll(int socketFd, const char* data, int length) {
    int totalSent = 0;
    while (totalSent < length) {
        int sent = send(socketFd, data + totalSent, length - totalSent, 0);
        if (sent <= 0) return false;
        totalSent += sent;
    }
    return true;
}

// Функція для гарантованого читання всього буфера з сокету
inline bool recvAll(int socketFd, char* buffer, int length) {
    int totalRecv = 0;
    while (totalRecv < length) {
        int received = recv(socketFd, buffer + totalRecv, length - totalRecv, 0);
        if (received <= 0) return false;
        totalRecv += received;
    }
    return true;
}

#endif // PROTOCOL_H