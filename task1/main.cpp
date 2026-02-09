#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <thread>
#include <functional>
#include <iomanip>
#include <future>

using namespace std;

// Visualising result
struct Result
{
    string label;
    long long ms;
};

void drawTextGraph(const vector<Result>& results, string dimension)
{
    cout << "\n--- Solution Speed Graphic ---\n";
    cout << setw(25) << left << "Method" << " | " << setw(8) << "Time" << " | Histogram" << endl;
    cout << string(60, '-') << endl;

    long long maxTime = 0;
    for (const auto& r : results) if (r.ms > maxTime) maxTime = r.ms;

    for (const auto& r : results)
    {
        cout << setw(25) << left << r.label << " | "
            << setw(5) << r.ms << " ms | ";

        // Малюємо "багет", довжина якого залежить від часу
        int barLength = (maxTime > 0) ? (int)(r.ms * 60 / maxTime) : 0;
        for (int i = 0; i < barLength; ++i) cout << "|";
        cout << endl;
    }
    cout << string(60, '-') << endl;
}

// 1. Time analysis mechanism
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

// Matrix generation function
void fillMatrixFlat(vector<int>& matrix, int size)
{
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> distrib(1, 100);
    for (int i = 0; i < size; ++i) 
    {
        matrix[i] = distrib(gen);
    }
}

// 2. Easy solution: C = A + k*B
void solveSequential(const vector<int>& A, const vector<int>& B, vector<int>& C, int k)
{
    size_t totalSize = A.size();
    for (size_t i = 0; i < totalSize; ++i)
    {
        C[i] = A[i] + k * B[i];
    }
}

// Worker-function
void solveOptimized(const vector<int>& A, const vector<int>& B, vector<int>& C, int k, int start, int end)
{
    for (int i = start; i < end; ++i) 
    {
        C[i] = A[i] + k * B[i];
    }
}

// 4.  Parallel solution: C = A + k*B
void parallelManager(const vector<int>& A, const vector<int>& B, vector<int>& C, int k, int numThreads)
{
    int totalSize = A.size();
    int grainSize = totalSize / numThreads;
    vector<future<void>> futures;

    for (int i = 0; i < numThreads; ++i) {
        int start = i * grainSize;
        int end = (i == numThreads - 1) ? totalSize : (i + 1) * grainSize;

        futures.push_back(async(launch::async, solveOptimized, ref(A), ref(B), ref(C), k, start, end));
    }
    for (auto& f : futures) f.get();
}

int main()
{
    vector<int> dimensions = { 5000, 10000, 15000 }; // Different dimensions
    const int K = 5;
    vector<int> threadTests = { 3, 6, 12, 24, 48, 72 };
    vector<Result> allResults;

    for (int N : dimensions)
    { 
        int TOTAL_SIZE = N * N; 
        vector<Result> allResults;

        cout << "\n>>> TESTING DIMENSION: " << N << "x" << N << " (" << TOTAL_SIZE << " elements)" << endl;
        cout << "-----------------------------------" << endl;

        vector<int> A(TOTAL_SIZE), B(TOTAL_SIZE), C_seq(TOTAL_SIZE), C_par(TOTAL_SIZE);

        cout << "Initializing..." << endl;
        fillMatrixFlat(A, TOTAL_SIZE);
        fillMatrixFlat(B, TOTAL_SIZE);

        // Послідовний тест
        long long seqTime;
        {
            ScopedTimer timer(&seqTime);
            solveSequential(A, B, C_seq, K);
        }
        allResults.push_back({ "Sequential (Ref)", seqTime });
        cout << "Sequential finished: " << seqTime << " ms" << endl;

        // Паралельні тести для поточної розмірності
        for (int numThreads : threadTests)
        {
            long long parTime;
            {
                ScopedTimer timer(&parTime);
                parallelManager(A, B, C_par, K, numThreads);
            }
            string label = "Parallel (" + to_string(numThreads) + " threads)";
            allResults.push_back({ label, parTime });
            cout << label << " finished: " << parTime << " ms" << endl;
        }

        drawTextGraph(allResults, to_string(N) + "x" + to_string(N)); 
    } 

    cout << "\nAll tests completed." << endl;
    return 0;
}