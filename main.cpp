#include "psd.h"
#include <fstream>

using namespace std;

int main()
{
    psd::psd f(ifstream("070_02.psd"));
    if (!f)
        return -1;
    return 0;
}
