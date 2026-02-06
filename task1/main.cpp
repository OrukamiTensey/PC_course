#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <thread>
#include <functional>
#include <iomanip>
#include <future>

using namespace std;

// 1. Time analysis mechanism
class ScopedTimer {
public:
    ScopedTimer(const string& name) : name_(name), start_(chrono::high_resolution_clock::now()) {}

    ~ScopedTimer() {
        auto end = chrono::high_resolution_clock::now();
        auto duration = chrono::duration_cast<chrono::milliseconds>(end - start_).count();
        cout << "Time derived for [" << name_ << "]: " << duration << " ms" << endl;
    }

private:
    string name_;
    chrono::time_point<chrono::high_resolution_clock> start_;
};

// Matrix generation function
void fillMatrixFlat(vector<int>& matrix, int size)
{
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> distrib(1, 100);
    for (int i = 0; i < size; ++i) {
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
    const int ROWS = 10000;
    const int COLS = 10000;
    const int TOTAL_SIZE = ROWS * COLS;
    const int K = 5;
    vector<int> threadTests = { 3, 6, 12, 24, 48, 72 };

    cout << "--- Performance Test ---" << endl;
    cout << "Matrix size: " << ROWS << "x" << COLS << " (Total: " << TOTAL_SIZE << ")" << endl;
    cout << "Scalar K: " << K << endl;
    cout << "-----------------------------------" << endl;

    vector<int> A(TOTAL_SIZE), B(TOTAL_SIZE), C_seq(TOTAL_SIZE), C_par(TOTAL_SIZE);

    cout << "Initializing matrices..." << endl;
    fillMatrixFlat(A, TOTAL_SIZE);
    fillMatrixFlat(B, TOTAL_SIZE); 

    {
        ScopedTimer timer("Sequential (Reference)");
        solveSequential(A, B, C_seq, K);
    }
    cout << "-----------------------------------" << endl;

    for (int numThreads : threadTests) {
        string label = "Parallel Optimized (" + to_string(numThreads) + " threads)";
        {
            ScopedTimer timer(label);
            parallelManager(A, B, C_par, K, numThreads);
        }
    }

    cout << "-----------------------------------" << endl;
    cout << "Test completed." << endl;

    return 0;
}