#include "psd.h"
#include <fstream>
#include <iostream>

using namespace std;

int main()
{
    psd::psd img(ifstream("x.psd", ios::binary));
    if (img)
        cout << "read success" << std::endl;
    else
    {
        cout << "read fail" << std::endl;
        return -1;
    }
    ofstream outf("y.psd", ios::binary);
    if (img.save(outf))
    {
        cout << "OK" << endl;
    }
    else
    {
        cout << "Fail" << endl;
    }
    psd::psd img2(ifstream("y.psd", ios::binary));
    return 0;
}
