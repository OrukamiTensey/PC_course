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
void fillMatrix(vector<vector<int>>& matrix, int rows, int cols) {
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> distrib(1, 100);

    for (int i = 0; i < rows; ++i) {
        matrix[i].resize(cols);
        for (int j = 0; j < cols; ++j) {
            matrix[i][j] = distrib(gen);
        }
    }
}

// 2. Easy solution: C = A + k*B
void solveSequential(const vector<vector<int>>& A,
    const vector<vector<int>>& B,
    vector<vector<int>>& C,
    int rows, int cols, int k) {
    for (int i = 0; i < rows; ++i) {
        for (int j = 0; j < cols; ++j) {
            C[i][j] = A[i][j] + k * B[i][j];
        }
    }
}