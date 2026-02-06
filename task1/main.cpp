#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <thread>
#include <functional>
#include <iomanip>

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
void fillMatrix(vector<vector<int>>& matrix, int rows, int cols)
{
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> distrib(1, 100);

    for (int i = 0; i < rows; ++i)
    {
        matrix[i].resize(cols);
        for (int j = 0; j < cols; ++j)
        {
            matrix[i][j] = distrib(gen);
        }
    }
}

// 2. Easy solution: C = A + k*B
void solveSequential(const vector<vector<int>>& A,
    const vector<vector<int>>& B,
    vector<vector<int>>& C,
    int rows, int cols, int k)
{
    for (int i = 0; i < rows; ++i)
    {
        for (int j = 0; j < cols; ++j)
        {
            C[i][j] = A[i][j] + k * B[i][j];
        }
    }
}

// Worker-function
void worker(int startRow, int endRow,
    const vector<vector<int>>& A,
    const vector<vector<int>>& B,
    vector<vector<int>>& C,
    int cols, int k) {
    for (int i = startRow; i < endRow; ++i)
    {
        for (int j = 0; j < cols; ++j)
        {
            C[i][j] = A[i][j] + k * B[i][j];
        }
    }
}

// 4.  Parallel solution: C = A + k*B
void solveParallel(const vector<vector<int>>& A,
    const vector<vector<int>>& B,
    vector<vector<int>>& C,
    int rows, int cols, int k, int numThreads)
{
    vector<thread> threads;
    int rowsPerThread = rows / numThreads;
    int remainingRows = rows % numThreads;

    int rowStart = 0;
    for (int i = 0; i < numThreads; ++i)
    {
        int rowEnd = rowStart + rowsPerThread + (i < remainingRows ? 1 : 0);
        // thread create
        threads.emplace_back(worker, rowStart, rowEnd, std::ref(A), std::ref(B), std::ref(C), cols, k);

        rowStart = rowEnd;
    }

    // Waiting for threads ending
    for (auto& t : threads)
    {
        if (t.joinable())
        {
            t.join();
        }
    }
}

int main() {
    const int ROWS = 2000;
    const int COLS = 2000;
    const int K = 5;       
    const int THREADS = 4; 

    cout << "Matrix size: " << ROWS << "x" << COLS << endl;
    cout << "Scalar k: " << K << endl;
    cout << "Threads: " << THREADS << endl;
    cout << "-----------------------------------" << endl;

    vector<vector<int>> A(ROWS), B(ROWS), C_seq(ROWS), C_par(ROWS);

    cout << "Initializing matrices..." << endl;
    fillMatrix(A, ROWS, COLS);
    fillMatrix(B, ROWS, COLS);

    for (int i = 0; i < ROWS; ++i) {
        C_seq[i].resize(COLS);
        C_par[i].resize(COLS);
    }

    // Sequential algorithm
    {
        ScopedTimer timer("Sequential");
        solveSequential(A, B, C_seq, ROWS, COLS, K);
    }

    // Parallel algorithm
    {
        ScopedTimer timer("Parallel");
        solveParallel(A, B, C_par, ROWS, COLS, K, THREADS);
    }

    return 0;
}