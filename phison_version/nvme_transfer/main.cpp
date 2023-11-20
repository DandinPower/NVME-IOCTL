#include "layer_manager.h"
#include <iostream>
#include <random>
#include <ctime>
#include <cstdlib>
#include <cassert>
#include <memory>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <chrono>

using namespace std;

uint as_uint(const float x)
{
    return *(uint *)&x;
}
float as_float(const uint x)
{
    return *(float *)&x;
}

float half_to_float(const ushort x)
{                                                                                                                                                        // IEEE-754 16-bit floating-point format (without infinity): 1-5-10, exp-15, +-131008.0, +-6.1035156E-5, +-5.9604645E-8, 3.311 digits
    const uint e = (x & 0x7C00) >> 10;                                                                                                                   // exponent
    const uint m = (x & 0x03FF) << 13;                                                                                                                   // mantissa
    const uint v = as_uint((float)m) >> 23;                                                                                                              // evil log2 bit hack to count leading zeros in denormalized format
    return as_float((x & 0x8000) << 16 | (e != 0) * ((e + 112) << 23 | m) | ((e == 0) & (m != 0)) * ((v - 37) << 23 | ((m << (150 - v)) & 0x007FE000))); // sign : normalized : denormalized
}
ushort float_to_half(const float x)
{                                                                                                                                                                                       // IEEE-754 16-bit floating-point format (without infinity): 1-5-10, exp-15, +-131008.0, +-6.1035156E-5, +-5.9604645E-8, 3.311 digits
    const uint b = as_uint(x) + 0x00001000;                                                                                                                                             // round-to-nearest-even: add last bit after truncated mantissa
    const uint e = (b & 0x7F800000) >> 23;                                                                                                                                              // exponent
    const uint m = b & 0x007FFFFF;                                                                                                                                                      // mantissa; in line below: 0x007FF000 = 0x00800000-0x00001000 = decimal indicator flag - initial rounding
    return (b & 0x80000000) >> 16 | (e > 112) * ((((e - 112) << 10) & 0x7C00) | m >> 13) | ((e < 113) & (e > 101)) * ((((0x007FF000 + m) >> (125 - e)) + 1) >> 1) | (e > 143) * 0x7FFF; // sign : normalized : denormalized : saturate
}

int main()
{
    bool huge = false;
    srand(time(NULL));
    layerVecMap &layerVecMapMgr = layerVecMap::getInstance();
    
    u_int16_t result = 0;
    char *wptr = new char[262144000];
    char *rptr = new char[262144000]{0};
    u_int16_t test_loop = 291;
    double r_speed = 0, w_speed = 0; 
    ushort *wData = reinterpret_cast<ushort *>(wptr);
    ushort *rData = reinterpret_cast<ushort *>(rptr);
    for (int k = 0; k < (262144000 >> 1); k++)
    {
        float r = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
        // float r = 0.0001;
        wData[k] = float_to_half(r);
    }

    for (u_int16_t i = 0; i < test_loop; i++)
    {
        
        memset(rptr, 0x0, 262144000);
        // for (int k = 0; k < (262144000 >> 1); k++)
        // {
        //     float r = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
        //     // float r = 0.0001;
        //     wData[k] = float_to_half(r);
        // }
        unsigned int param = 0;
        if (i == 0 || i == 290)
        {
            param = 131072000;
        }
        else
        {
            if (i % 9 <= 4)
            {
                param = 16777216;
            }
            else
            {
                param = 45088768;
            }
        }
        string str = "/media/nvme1/md4/zero_stage_3/bfloat16params/rank0/" + to_string(i) + "_" + to_string(16777216) + "_param_bfloat16.tensor.swp";
        const char *fileName = str.c_str();
        auto start = chrono::steady_clock::now();
        cout << fileName << endl;
        layerVecMapMgr.parsefromFileName(fileName, 1, wptr, 0);
        auto mid = chrono::steady_clock::now();
        layerVecMapMgr.parsefromFileName(fileName, 0, rptr, 0);
        auto end = chrono::steady_clock::now();
        cout << chrono::duration <double, milli> (mid - start).count() << " ms" << endl;
        cout << chrono::duration <double, milli> (end - mid).count() << " ms" << endl;
        w_speed += chrono::duration <double, milli> (mid - start).count();
        r_speed += chrono::duration <double, milli> (end - mid).count();
        // for (int k = 0; k < 16777216; k++)
        // {
        //     if (rData[k] != wData[k])
        //     {
        //         cout << k << endl;
        //         assert(0);
        //     }
        // }
    }
    w_speed /= test_loop;
    r_speed /= test_loop;
    cout << "Avg. write " << w_speed << "ms" << endl;
    cout << "Avg. read " << r_speed << "ms" << endl;

    delete[] rptr;
    delete[] wptr;
    
    // free(wptr);
    // free(rptr);
    return 0;
}

/*
int sync_pread(torch::Tensor& buffer, const char* filename)
{
    layerVecMap& layerVecMapMgr = layerVecMap::getInstance();
    void* data_ptr = buffer.data_ptr();
    unsigned long long data_size = buffer.numel() * sizeof(float);
    return layerVecMapMgr.parsefromFileName(filename, 0, data_ptr, data_size);
}

int sync_pwrite(const torch::Tensor& buffer, const char* filename)
{
    layerVecMap& layerVecMapMgr = layerVecMap::getInstance();
    void* data_ptr = buffer.data_ptr();
    unsigned long long data_size = buffer.numel() * sizeof(float);
    return layerVecMapMgr.parsefromFileName(filename, 1, data_ptr, data_size);
}
*/