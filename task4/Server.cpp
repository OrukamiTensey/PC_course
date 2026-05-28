#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <mutex>
#include <chrono>
#include <functional>
#include <future>
#include "Protocol.h"

using namespace std;

const int PORT = 8080;

// Структура для збереження контексту обчислень конкретного клієнта
struct ClientContext {
    ConfigPayload config;
    vector<int> A;
    vector<int> B;
    vector<int> C;

    atomic<uint32_t> status{ RES_IN_PROGRESS };
    long long calculationTimeMs = 0;
};

// Робочий потік для обчислень (Твоя логіка з Лаби 1)
void solveOptimized(const vector<int>& A, const vector<int>& B, vector<int>& C, int k, int start, int end) {
    for (int i = start; i < end; ++i) {
        C[i] = A[i] + k * B[i];
    }
}

// Менеджер паралельних обчислень (Модифікований під контекст сервера)
void parallelManager(ClientContext* ctx) {
    auto startTime = chrono::high_resolution_clock::now();

    int totalSize = ctx->config.arraySize;
    int numThreads = ctx->config.numThreads;
    if (numThreads <= 0) numThreads = 1;

    int grainSize = totalSize / numThreads;
    vector<future<void>> futures;

    for (int i = 0; i < numThreads; ++i) {
        int start = i * grainSize;
        int end = (i == numThreads - 1) ? totalSize : (i + 1) * grainSize;

        futures.push_back(async(launch::async, solveOptimized, ref(ctx->A), ref(ctx->B), ref(ctx->C), ctx->config.k, start, end));
    }

    for (auto& f : futures) {
        f.get();
    }

    auto endTime = chrono::high_resolution_clock::now();
    ctx->calculationTimeMs = chrono::duration_cast<chrono::milliseconds>(endTime - startTime).count();

    // Перемикаємо статус у готовність
    ctx->status.store(RES_DONE);
}

// Головна функція обробки клієнтського потоку
void handleClient(int clientSocket) {
    cout << "[Server] Client connected on socket: " << clientSocket << endl;

    ClientContext* ctx = nullptr;
    thread workerThread;

    while (true) {
        PacketHeader header;
        // Читаємо заголовок пакету
        if (!recvAll(clientSocket, reinterpret_cast<char*>(&header), sizeof(header))) {
            cout << "[Server] Client disconnected or connection lost on socket: " << clientSocket << endl;
            break;
        }

        // Конвертуємо заголовок з мережевого порядку байтів
        networkToHost(header);

        // 1. Обробка команди CONFIG
        if (header.command == CMD_CONFIG) {
            cout << "[Server] Receiving configuration..." << endl;

            ConfigPayload config;
            if (!recvAll(clientSocket, reinterpret_cast<char*>(&config), sizeof(config))) {
                cerr << "[Error] Failed to receive config payload." << endl;
                break;
            }
            networkToHost(config);

            // Виділяємо пам'ять під контекст
            delete ctx;
            ctx = new ClientContext();
            ctx->config = config;
            ctx->A.resize(config.arraySize);
            ctx->B.resize(config.arraySize);
            ctx->C.resize(config.arraySize);

            // Читаємо масив A
            if (!recvAll(clientSocket, reinterpret_cast<char*>(ctx->A.data()), config.arraySize * sizeof(int))) {
                cerr << "[Error] Failed to receive array A." << endl;
                break;
            }
            // Читаємо масив B
            if (!recvAll(clientSocket, reinterpret_cast<char*>(ctx->B.data()), config.arraySize * sizeof(int))) {
                cerr << "[Error] Failed to receive array B." << endl;
                break;
            }

            // Конвертуємо отримані масиви з мережевого порядку байтів
            for (uint32_t i = 0; i < config.arraySize; ++i) {
                ctx->A[i] = ntohl(ctx->A[i]);
                ctx->B[i] = ntohl(ctx->B[i]);
            }

            // Надсилаємо відповідь OK
            PacketHeader response = { RES_OK, 0 };
            hostToNetwork(response);
            sendAll(clientSocket, reinterpret_cast<char*>(&response), sizeof(response));
            cout << "[Server] Config applied successfully. Array size: " << config.arraySize << endl;
        }
        // 2. Обробка команди START
        else if (header.command == CMD_START) {
            PacketHeader response;
            if (!ctx) {
                response = { RES_ERROR, 0 };
                hostToNetwork(response);
                sendAll(clientSocket, reinterpret_cast<char*>(&response), sizeof(response));
                continue;
            }

            cout << "[Server] Starting background calculation thread..." << endl;
            ctx->status.store(RES_IN_PROGRESS);

            // Запуск Task Processing Thread
            if (workerThread.joinable()) workerThread.join();
            workerThread = thread(parallelManager, ctx);

            response = { RES_STARTED, 0 };
            hostToNetwork(response);
            sendAll(clientSocket, reinterpret_cast<char*>(&response), sizeof(response));
        }
        // 3. Обробка команди STATUS
        else if (header.command == CMD_STATUS) {
            PacketHeader response;
            if (!ctx) response = { RES_ERROR, 0 };
            else response = { ctx->status.load(), 0 };

            hostToNetwork(response);
            sendAll(clientSocket, reinterpret_cast<char*>(&response), sizeof(response));
        }
        // 4. Обробка команди GET_RESULT
        else if (header.command == CMD_GET_RESULT) {
            if (!ctx || ctx->status.load() != RES_DONE) {
                PacketHeader response = { RES_ERROR, 0 };
                hostToNetwork(response);
                sendAll(clientSocket, reinterpret_cast<char*>(&response), sizeof(response));
                continue;
            }

            cout << "[Server] Sending results back to client..." << endl;

            // Готуємо масив С до відправки (переводим в network endianness)
            vector<int> networkC(ctx->config.arraySize);
            for (uint32_t i = 0; i < ctx->config.arraySize; ++i) {
                networkC[i] = htonl(ctx->C[i]);
            }

            // Довжина корисного навантаження: час (8 байт) + сам масив
            uint32_t payloadLen = sizeof(long long) + (ctx->config.arraySize * sizeof(int));
            PacketHeader response = { RES_DONE, payloadLen };
            hostToNetwork(response);

            // Відправляємо заголовок
            sendAll(clientSocket, reinterpret_cast<char*>(&response), sizeof(response));

            // Відправляємо час обчислень (переведемо в network порядок через 64-бітну магію або надішлемо як є, але для чистоти конвертуємо через локальний буфер)
            long long networkTime = ctx->calculationTimeMs; // Спростимо: час надсилається як сирі байти або стрінгою, але передамо просто як int64
            sendAll(clientSocket, reinterpret_cast<char*>(&networkTime), sizeof(networkTime));

            // Відправляємо масив
            sendAll(clientSocket, reinterpret_cast<char*>(networkC.data()), networkC.size() * sizeof(int));
            cout << "[Server] Results sent." << endl;
        }
    }

    // Очищення ресурсів при відключенні
    if (workerThread.joinable()) workerThread.join();
    delete ctx;

#ifdef _WIN32
    closesocket(clientSocket);
#else
    close(clientSocket);
#endif
    cout << "[Server] Socket " << clientSocket << " fully closed." << endl;
}

int main() {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cerr << "[Error] WSAStartup failed." << endl;
        return 1;
    }
#endif

    int serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSocket < 0) {
        cerr << "[Error] Socket creation failed." << endl;
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(PORT);

    if (::bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        cerr << "[Error] Bind failed on port " << PORT << endl;
#ifdef _WIN32
        closesocket(serverSocket);
        WSACleanup();
#else
        close(serverSocket);
#endif
        return 1;
    }

    if (listen(serverSocket, SOMAXCONN) < 0) {
        cerr << "[Error] Listen failed." << endl;
#ifdef _WIN32
        closesocket(serverSocket);
        WSACleanup();
#else
        close(serverSocket);
#endif
        return 1;
    }

    cout << "[Server] Active and listening on port " << PORT << "..." << endl;

    while (true) {
        sockaddr_in clientAddr;
        int clientAddrLen = sizeof(clientAddr);
        int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientAddrLen);
        if (clientSocket < 0) {
            cerr << "[Warning] Accept failed." << endl;
            continue;
        }

        thread clientThread(handleClient, clientSocket);
        clientThread.detach();
    }

#ifdef _WIN32
    closesocket(serverSocket);
    WSACleanup();
#else
    close(serverSocket);
#endif
    return 0;
}