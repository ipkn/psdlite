#include "psd.h"
#include <fstream>
#include <iostream>

using namespace std;

int main()
{
    //psd::psd f(ifstream("070_02.psd"));
    psd::psd f(ifstream("006_11.psd"));
    if (!f)
    {
        cout << "fail to open" << endl;
        return -1;
    }
    cout << "OK" << endl;
    return 0;
}
