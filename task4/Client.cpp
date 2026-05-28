#include <iostream>
#include <vector>
#include <random>
#include "Protocol.h"

using namespace std;

const char* SERVER_IP = "127.0.0.1";
const int PORT = 8080;

// Функція генерації даних (Твоя логіка з Лаби 1)
void fillMatrixFlat(vector<int>& matrix, int size) {
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> distrib(1, 100);
    for (int i = 0; i < size; ++i) {
        matrix[i] = distrib(gen);
    }
}

int main() {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cerr << "[Client Error] WSAStartup failed." << endl;
        return 1;
    }
#endif

    // 1. Створення сокету
    int clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (clientSocket < 0) {
        cerr << "[Client Error] Socket creation failed." << endl;
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    // 2. Налаштування адреси сервера
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    inet_pton(AF_INET, SERVER_IP, &serverAddr.sin_addr);

    cout << "[Client] Connecting to server " << SERVER_IP << ":" << PORT << "..." << endl;
    if (connect(clientSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        cerr << "[Client Error] Connection failed." << endl;
#ifdef _WIN32
        closesocket(clientSocket);
        WSACleanup();
#else
        close(clientSocket);
#endif
        return 1;
    }
    cout << "[Client] Connected successfully!" << endl;

    // 3. Підготовка даних (наприклад, матриця 100x100 = 10000 елементів)
    uint32_t N = 100;
    uint32_t totalSize = N * N;
    int32_t K = 5;
    uint32_t threadsToUse = 6;

    vector<int> A(totalSize), B(totalSize);
    fillMatrixFlat(A, totalSize);
    fillMatrixFlat(B, totalSize);

    cout << "[Client] Generated matrices with " << totalSize << " elements." << endl;

    // 4. Відправка конфігурації (CMD_CONFIG)
    uint32_t payloadLen = sizeof(ConfigPayload) + (totalSize * sizeof(int) * 2);
    PacketHeader header = { CMD_CONFIG, payloadLen };

    ConfigPayload config = { K, totalSize, threadsToUse };

    // Копіюємо масиви для конвертації в мережевий порядок байтів
    vector<int> networkA(totalSize), networkB(totalSize);
    for (uint32_t i = 0; i < totalSize; ++i) {
        networkA[i] = htonl(A[i]);
        networkB[i] = htonl(B[i]);
    }

    // Переводимо структури в Big-Endian перед відправкою
    hostToNetwork(header);
    hostToNetwork(config);

    // Послідовно відправляємо: Заголовок -> Конфіг -> Масив А -> Масив В
    if (!sendAll(clientSocket, reinterpret_cast<char*>(&header), sizeof(header)) ||
        !sendAll(clientSocket, reinterpret_cast<char*>(&config), sizeof(config)) ||
        !sendAll(clientSocket, reinterpret_cast<char*>(networkA.data()), totalSize * sizeof(int)) ||
        !sendAll(clientSocket, reinterpret_cast<char*>(networkB.data()), totalSize * sizeof(int))) {

        cerr << "[Client Error] Failed to send configuration package." << endl;
    }
    else {
        cout << "[Client] Configuration and matrices sent. Waiting for ACK..." << endl;
    }

    // 5. Очікування відповіді від сервера
    PacketHeader response;
    if (recvAll(clientSocket, reinterpret_cast<char*>(&response), sizeof(response))) {
        networkToHost(response);
        if (response.command == RES_OK) {
            cout << "[Client] Server acknowledged configuration (RES_OK)." << endl;
        }
        else {
            cerr << "[Client Error] Server returned error response." << endl;
        }
    }

    // Тимчасово закриваємо сокет (повний цикл додамо в Коміті 5)
#ifdef _WIN32
    closesocket(clientSocket);
    WSACleanup();
#else
    close(clientSocket);
#endif
    return 0;
}