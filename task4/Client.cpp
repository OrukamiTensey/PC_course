#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <thread>
#include "Protocol.h"

using namespace std;

const char* SERVER_IP = "127.0.0.1";
const int PORT = 8080;

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

    int clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (clientSocket < 0) {
        cerr << "[Client Error] Socket creation failed." << endl;
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

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

    // Створимо масив більшого розміру, щоб сервер рахував його бодай якусь кількість мілісекунд
    uint32_t N = 1000;
    uint32_t totalSize = N * N;
    int32_t K = 5;
    uint32_t threadsToUse = 8;

    vector<int> A(totalSize), B(totalSize), C(totalSize);
    fillMatrixFlat(A, totalSize);
    fillMatrixFlat(B, totalSize);

    cout << "[Client] Matrices configured: " << N << "x" << N << " (" << totalSize << " elements)." << endl;

    // --- КРОК 1: Надсилання CONFIG ---
    uint32_t payloadLen = sizeof(ConfigPayload) + (totalSize * sizeof(int) * 2);
    PacketHeader header = { CMD_CONFIG, payloadLen };
    ConfigPayload config = { K, totalSize, threadsToUse };

    vector<int> networkA(totalSize), networkB(totalSize);
    for (uint32_t i = 0; i < totalSize; ++i) {
        networkA[i] = htonl(A[i]);
        networkB[i] = htonl(B[i]);
    }

    hostToNetwork(header);
    hostToNetwork(config);

    if (!sendAll(clientSocket, reinterpret_cast<char*>(&header), sizeof(header)) ||
        !sendAll(clientSocket, reinterpret_cast<char*>(&config), sizeof(config)) ||
        !sendAll(clientSocket, reinterpret_cast<char*>(networkA.data()), totalSize * sizeof(int)) ||
        !sendAll(clientSocket, reinterpret_cast<char*>(networkB.data()), totalSize * sizeof(int))) {
        cerr << "[Client Error] Failed to send configuration." << endl;
        return 1;
    }

    PacketHeader response;
    if (!recvAll(clientSocket, reinterpret_cast<char*>(&response), sizeof(response))) return 1;
    networkToHost(response);

    if (response.command != RES_OK) {
        cerr << "[Client Error] Server rejected configuration." << endl;
        return 1;
    }
    cout << "[Client] Server acknowledged configuration (RES_OK)." << endl;

    // --- КРОК 2: Надсилання команди START ---
    PacketHeader startHeader = { CMD_START, 0 };
    hostToNetwork(startHeader);
    sendAll(clientSocket, reinterpret_cast<char*>(&startHeader), sizeof(startHeader));

    if (!recvAll(clientSocket, reinterpret_cast<char*>(&response), sizeof(response))) return 1;
    networkToHost(response);

    if (response.command == RES_STARTED) {
        cout << "[Client] Server started processing asynchronously." << endl;
    }
    else {
        cerr << "[Client Error] Server failed to start computation." << endl;
        return 1;
    }

    // --- КРОК 3: Опитування статусу (Polling Loop) ---
    while (true) {
        this_thread::sleep_for(chrono::milliseconds(100)); // Затримка між запитами

        PacketHeader statusHeader = { CMD_STATUS, 0 };
        hostToNetwork(statusHeader);
        sendAll(clientSocket, reinterpret_cast<char*>(&statusHeader), sizeof(statusHeader));

        if (!recvAll(clientSocket, reinterpret_cast<char*>(&response), sizeof(response))) break;
        networkToHost(response);

        if (response.command == RES_DONE) {
            cout << "[Client] Server finished computation!" << endl;
            break;
        }
        else if (response.command == RES_IN_PROGRESS) {
            cout << "[Client] Server status: In progress..." << endl;
        }
        else {
            cerr << "[Client Error] Unexpected status received." << endl;
            return 1;
        }
    }

    // --- КРОК 4: Отримання результату (GET_RESULT) ---
    PacketHeader getResultHeader = { CMD_GET_RESULT, 0 };
    hostToNetwork(getResultHeader);
    sendAll(clientSocket, reinterpret_cast<char*>(&getResultHeader), sizeof(getResultHeader));

    if (!recvAll(clientSocket, reinterpret_cast<char*>(&response), sizeof(response))) return 1;
    networkToHost(response);

    if (response.command == RES_DONE) {
        long long calculationTime = 0;
        // Зчитуємо час виконання
        recvAll(clientSocket, reinterpret_cast<char*>(&calculationTime), sizeof(calculationTime));

        // Зчитуємо сам масив результату С
        vector<int> networkC(totalSize);
        recvAll(clientSocket, reinterpret_cast<char*>(networkC.data()), totalSize * sizeof(int));

        // Конвертуємо результат назад у Host Endianness
        for (uint32_t i = 0; i < totalSize; ++i) {
            C[i] = ntohl(networkC[i]);
        }

        cout << "[Client] Success! Received calculation time from server: " << calculationTime << " ms" << endl;

        // Перевірка кількох елементів для верифікації
        cout << "[Client] Verification (First 5 elements):" << endl;
        for (int i = 0; i < 5; ++i) {
            cout << "  A[" << i << "]=" << A[i] << " + " << K << " * B[" << i << "]=" << B[i]
                << " -> C[" << i << "]=" << C[i] << " (Expected: " << A[i] + K * B[i] << ")" << endl;
        }
    }
    else {
        cerr << "[Client Error] Failed to retrieve results." << endl;
    }

#ifdef _WIN32
    closesocket(clientSocket);
    WSACleanup();
#else
    close(clientSocket);
#endif
    return 0;
}