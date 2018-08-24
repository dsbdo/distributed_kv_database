#include <iostream>
#include "Communicate/Communicate.h"
using namespace std;
int main() {
    Communicate* comm = new Communicate("127.0.0.1", 8080);
    cout <<"ack msg is: " << comm->sendString("test in Communicate") << endl;
    return 0;
}