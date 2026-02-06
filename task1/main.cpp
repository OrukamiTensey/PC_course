#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <thread>
#include <functional>
#include <iomanip>

using namespace std;

// 1. Механізм для заміру часу виконання
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