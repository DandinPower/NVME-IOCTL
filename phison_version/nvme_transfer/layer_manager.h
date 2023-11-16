#include <iostream>
#include <string>
#include <map>
#include <vector>

#define MAX_LAYER_PARAM 262209536
#define MAX_LAYER_SIZE 750
#define ALIGN_4K 4096
#define ALIGN_LAYER_PARAM(param) ((param + ALIGN_4K - 1) & ~(ALIGN_4K - 1))
#define BYTES_USED_PER_LAYER 18 // fp16, fp32, grad32, moment32, var32
#define BYTES_PER_LBA 512 // each lba contain 512 bytes

struct LBAMgr{
    unsigned long long param16;
    unsigned long long param32;
    unsigned long long gradient32;
    unsigned long long momentum32;
    unsigned long long variance32;
};

struct CRCMgr{
    uint32_t param16;
    uint32_t param32;
    uint32_t gradient32;
    uint32_t momentum32;
    uint32_t variance32;  
};

struct layerInfoMgr {
    std::string paramNum;
    std::string hashID;
    LBAMgr lbaMgr;
    CRCMgr crcMgr;
};

struct prevOperation{
    std::vector<std::string> prevFilePathSplit;
    std::vector<std::string> prevFileNameSplit;
    unsigned short op;
};

class layerVecMap
{
public:
    layerVecMap();
    ~layerVecMap();
    std::map<std::string, struct layerInfoMgr> layerMgr;
    struct prevOperation prevOperationMgr;
    unsigned long long allocLBAoffset;
    int parsefromFileName(const char *fileName, unsigned short op, void *data, unsigned long long size = 0);
    unsigned long long getLBAfromList(std::vector<std::string> tempfileNameSplit, layerInfoMgr tempLayerInfoMgr, unsigned long long *tempSize);
    struct LBAMgr getTempLBAMgr(std::vector<std::string> tempfileNameSplit, std::vector<std::string> prevFilePathSplit);
    static layerVecMap& getInstance();
    uint32_t genCRCfromData(void *data, unsigned long long size);
    void clearMap();
};