#include<iostream>
#include<ctime>
int main(int argc, char** argv) {
    using namespace std;
    time_t timestamp;
    cout << time(NULL) << endl;
    cout << time(&timestamp) << endl;
    cout << timestamp << endl;
}