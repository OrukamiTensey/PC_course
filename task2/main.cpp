#include <vector>
#include <chrono>
#include <iostream>

using namespace std 

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

int main()
{
    cout << "Parallel Computing: Task 2 (Variant 17)" << endl;
    return 0;
}
