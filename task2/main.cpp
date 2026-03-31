#include <vector>
#include <chrono>
#include <iostream>
#include <random>
#include <algorithm>
#include <mutex>
#include <thread>
#include <atomic>

using namespace std;


class ScopedTimer
{
public:
    ScopedTimer(long long* outTime) : outTime_(outTime), start_(chrono::high_resolution_clock::now()) {}
    ~ScopedTimer()
    {
        auto end = chrono::high_resolution_clock::now();
        *outTime_ = chrono::duration_cast<chrono::milliseconds>(end - start_).count();
    }
private:
    long long* outTime_;
    chrono::time_point<chrono::high_resolution_clock> start_;
};

void solveSequential(const vector<int>& data, int& count, int& maxVal)
{
    count = 0;
    maxVal = INT_MIN;
    for (int x : data)
    {
        if (x > 10)
        {
            count++;
            if (x > maxVal) maxVal = x;
        }
    }
}

void solveWithMutex(const vector<int>& data, int& count, int& maxVal, int numThreads)
{
    count = 0;
    maxVal = INT_MIN;
    mutex mtx;
    vector<thread> threads;
    int chunkSize = data.size() / numThreads;

    for (int i = 0; i < numThreads; ++i)
    {
        threads.emplace_back([&, i]()
            {
            int localCount = 0;
            int localMax = INT_MIN;
            int start = i * chunkSize;
            int end = (i == numThreads - 1) ? data.size() : start + chunkSize;

            for (int j = start; j < end; ++j)
            {
                if (data[j] > 10)
                {
                    localCount++;
                    if (data[j] > localMax) localMax = data[j];
                }
            }

            
            lock_guard<mutex> lock(mtx);
            count += localCount;
            if (localMax > maxVal) maxVal = localMax;
            });
    }

    for (auto& t : threads) t.join();
}

void solveAtomic(const vector<int>& data, atomic<int>& count, atomic<int>& maxVal, int numThreads)
{
    count = 0;
    maxVal = INT_MIN;
    vector<thread> threads;
    int chunkSize = data.size() / numThreads;

    for (int i = 0; i < numThreads; ++i)
    {
        threads.emplace_back([&, i]()
            {
                int localCount = 0;
                int localMax = INT_MIN;
                int start = i * chunkSize;
                int end = (i == numThreads - 1) ? data.size() : start + chunkSize;

                
                for (int j = start; j < end; ++j)
                {
                    if (data[j] > 10)
                    {
                        localCount++;
                        if (data[j] > localMax) localMax = data[j];
                    }
                }

                
                count.fetch_add(localCount);

                int currentMax = maxVal.load();
                while (localMax > currentMax &&
                    !maxVal.compare_exchange_weak(currentMax, localMax))
                {
                    
                }
            });
    }

    for (auto& t : threads) t.join();
}

int main()
{
    vector<int> sizes = { 1000000, 5000000, 10000000, 25000000, 50000000, 100000000 };
    int threadsCount = thread::hardware_concurrency();

    cout << "Threads used: " << threadsCount << endl;
    cout << "Size,Sequential(ms),Mutex(ms),Atomic/CAS(ms)" << endl; // Заголовок для CSV

    for (int SIZE : sizes)
    {
        vector<int> data(SIZE);
        random_device rd;
        mt19937 gen(rd());
        uniform_int_distribution<> dis(-50, 150);
        for (int& x : data) x = dis(gen);

        long long tSeq = 0, tMtx = 0, tAt = 0;
        int cSeq, mSeq, cMtx, mMtx;
        atomic<int> cAt(0), mAt(INT_MIN);

        // Виконання замірів
        { ScopedTimer t(&tSeq); solveSequential(data, cSeq, mSeq); }
        { ScopedTimer t(&tMtx); solveWithMutex(data, cMtx, mMtx, threadsCount); }
        { ScopedTimer t(&tAt);  solveAtomic(data, cAt, mAt, threadsCount); }

        // Вивід у форматі CSV для легкого побудови графіків
        cout << SIZE << "," << tSeq << "," << tMtx << "," << tAt << endl;
    }

    return 0;
}
