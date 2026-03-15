#include <vector>
#include <chrono>
#include <iostream>
#include <random>
#include <algorithm>

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

int main()
{
    const int SIZE = 10000000;
    vector<int> data(SIZE);

    
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> dis(-50, 150); 

    for (int& x : data) x = dis(gen);

    long long timeSeq = 0;
    int countSeq = 0, maxSeq = 0;

    {
        ScopedTimer timer(&timeSeq);
        solveSequential(data, countSeq, maxSeq);
    }

    cout << "--- Sequential Version ---" << endl;
    cout << "Count (>10): " << countSeq << endl;
    cout << "Max value (>10): " << maxSeq << endl;
    cout << "Time: " << timeSeq << " ms" << endl;

    return 0;
}
