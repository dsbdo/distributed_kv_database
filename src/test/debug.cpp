#include "debug.h"
void debugInfo(std::string debug_info) {
    using namespace std;
    cout << "\033[32m" << "DEBUG::INFO: " << debug_info << "\033[0m" << endl;
}
void debugError(std::string debug_error) {
    using namespace std;
    cout << "\033[32m" << "DEBUG::INFO: " << debug_error << "\033[0m" << endl;
}