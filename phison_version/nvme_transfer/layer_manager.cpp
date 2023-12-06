#include "layer_manager.h"
#include "nvme-ioctl.h"
#include "crc32.h"
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <cassert>
#include <fstream>
#include <functional>
#include "nvme-aio.h"

#define assertm(exp, msg) assert(((void)msg, exp))
#define debugCompare 0
#define returnLBAonly 0
#define currentIOMethod 0
#define libAIOasync 1
#define libAIOPhysical 1

void callback_n(int &x)
{
    x++;
}

void callback_empty()
{
}


enum
{
    METHOD_IOCTL = 0,
    METHOD_LIBAIO = 1,
    METHOD_IOURING = 2,
};

static layerVecMap instance;

using namespace std;

layerVecMap::layerVecMap()
{
    clearMap();
}

layerVecMap::~layerVecMap()
{
    // exit
}

unsigned long long layerVecMap::parsefromFileName(const char *fileName, unsigned short op, void *data, unsigned long long size)
{
    layerInfoMgr tempLayerInfoMgr;
    string tempLayer;
    vector<string> filePathSplit;
    vector<string> fileNameSplit;
    string filePath(fileName);
    string info;
    layerVecMap &instance = getInstance();
    auto iterForLayerMgr = instance.layerMgr.begin();
    uint32_t crc = 0xFFFFFFFF;
    int returnVal = 0;
#if libAIOasync
    vector<int> fdArray;
#endif
    cout << "[OP]" << op << ", FILE:" << fileName << ", ";

    // ------------STEP1: USE slash("/") to parse file path
    // ------------EXAMPLE: fileName = /media/user/md4/zero_stage_3/bfloat16params/rank0/0_131072000_param_bfloat16.tensor.swp
    // ------------We will get filePathSplit = ["media", "user", "md4", "zero_stage_3", "bfloat16params", "rank0"]
    size_t slashPos = filePath.find('/');
    while (slashPos != string::npos)
    {
        // find next "/"
        size_t nextSlashPos = filePath.find('/', slashPos + 1);

        // substr
        if (nextSlashPos != string::npos)
        {
            info = filePath.substr(slashPos + 1, nextSlashPos - 1);
            // push back
            filePathSplit.push_back(info);
            filePath = filePath.substr(nextSlashPos);
        }

        // update pos
        slashPos = filePath.find('/');
        if (filePath.find('/', slashPos + 1) == string::npos)
        {
            break;
        }
    }
    assert(filePathSplit.size() == 6);

    // expect filePathSplit = ["media", "user", "md4", "zero_stage_3", "bfloat16params", "rank0"]
    // for (auto it = filePathSplit.begin(); it != filePathSplit.end(); ++it) {
    //     cout << *it << " ";
    // }
    // cout << endl;

    // ------------STEP2: USE dash("_") to parse file name
    // ------------EXAMPLE: fileName = /0_131072000_param_bfloat16.tensor.swp
    // ------------We will get fileNameSplit = ["0", "131072000", "param", "bfloat16"]

    // find last "/" pos
    slashPos = filePath.find('/');
    if (slashPos != string::npos)
    {
        // get substr from last "/" pos
        info = filePath.substr(slashPos + 1);
        // info = "/67_16777216_param_bfloat16.tensor.swp"
        if (info.empty())
        {
            // error handle
        }

        // find first "." pos
        size_t firstDotPos = info.find(".");

        if (firstDotPos != string::npos)
        {
            // info = "67_16777216_param_bfloat16"
            info = info.substr(0, firstDotPos);
            size_t firstDashPos = info.find("_");
            while (firstDashPos != string::npos)
            {
                string subInfo = info.substr(0, firstDashPos);
                if (!subInfo.empty())
                {
                    fileNameSplit.push_back(subInfo);
                }
                info = info.substr(firstDashPos + 1);
                firstDashPos = info.find("_");
            }
        }
        else
        {
            // error handle
        }
        if (!info.empty())
        {
            fileNameSplit.push_back(info);
        }
        else
        {
            // error handle
        }
    }
    else
    {
        // error handle
    }

    assert(fileNameSplit.size() >= 4 && fileNameSplit.size() <= 5);
    // expect fileNameSplit = ["0", "131072000", "param", "bfloat16"]
    // for (auto it = fileNameSplit.begin(); it != fileNameSplit.end(); ++it) {
    //     cout << *it << " ";
    // }
    // cout << endl;

    // ------------STEP3: Judge filePathSplit[4] is "bfloat16params" or "optimizer"
    // ------------If filePathSplit[4] is "bfloat16params", we can get layer number, and create a mapping struct
    if (filePathSplit[4] == "bfloat16params")
    {
        // insert layer info if not exist
        iterForLayerMgr = instance.layerMgr.find(fileNameSplit[0]);
        if (iterForLayerMgr == instance.layerMgr.end())
        {
            // assign temp value
            tempLayerInfoMgr.paramNum = fileNameSplit[1];
            tempLayerInfoMgr.hashID = "-1";
            tempLayerInfoMgr.lbaMgr = getTempLBAMgr(fileNameSplit, filePathSplit);
            CRCMgr crcmgr = {0};
            tempLayerInfoMgr.crcMgr = crcmgr;
            instance.layerMgr.insert(pair<string, layerInfoMgr>(fileNameSplit[0], tempLayerInfoMgr));
            iterForLayerMgr = instance.layerMgr.find(fileNameSplit[0]);
        }
        else
        {
            tempLayerInfoMgr = iterForLayerMgr->second;
        }

        // ------------If filePathSplit[4] is "optimizer", we can get layer number by previous IO operation,
        // ------------if previous IO operation is "read" and hash ID is the same with current IO operation,
        // ------------Then we can link up with previous IO operation struct
    }
    else if (filePathSplit[4] == "optimizer")
    {
        // if previous is read --> link up map
        if (instance.prevOperationMgr.op == 0 && stoull(instance.prevOperationMgr.prevFileNameSplit[0]) < MAX_LAYER_SIZE)
        {
            auto iter = instance.layerMgr.find(instance.prevOperationMgr.prevFileNameSplit[0]);
            if (iter != instance.layerMgr.end())
            {
                if (iter->second.hashID == "-1")
                {
                    assertm(instance.prevOperationMgr.prevFileNameSplit[1] == fileNameSplit[1], "ASSERT 0136: Layer name not match");
                    iter->second.hashID = fileNameSplit[0];
                }
                else
                {
                    // already sign in, no any to do
                }
            }
            else
            {
                assertm(0, "ASSERT 0120: No record layer for this hash");
            }
        }
        for (; iterForLayerMgr != instance.layerMgr.end(); ++iterForLayerMgr)
        {
            if (iterForLayerMgr->second.hashID == fileNameSplit[0])
            {
                break;
            }
        }
        tempLayerInfoMgr = iterForLayerMgr->second;
    }

    assertm(iterForLayerMgr != instance.layerMgr.end(), "ASSERT 0150: Can not find layer in map");

#if debugCompare
    if (iterForLayerMgr->first == "13")
    {
        // ofstream myfile;
        // myfile.open("output13_before.log");
        float *newData = reinterpret_cast<float *>(data);
        cout << "-DEBUG A-" << endl;
        for (auto i = 0; i < 32; i++)
        {
            printf("%f ", newData[i]);
        }
        // for (unsigned long int i = 0; i < 8388608; i++){
        //     myfile << to_string(newData[i]) << " ";
        //     if (i % 16 == 0 && i > 0){
        //         myfile << "\n";
        //     }
        // }
        // myfile.close();
        cout << endl;
    }
#endif
    // ------------STEP4: keep last operation in order to link tensor STEP3-> filePathSplit[4] is "optimizer"
    instance.prevOperationMgr.prevFilePathSplit = filePathSplit;
    instance.prevOperationMgr.prevFileNameSplit = fileNameSplit;
    instance.prevOperationMgr.op = op;

    // ------------STEP5: Get Start LBA and size we want to read/write
    unsigned long long mapSize = 0;
    unsigned long long currentLBA = getLBAfromList(fileNameSplit, tempLayerInfoMgr, &mapSize);
    if (size == 0)
        size = mapSize;
    cout << "LBA:" << currentLBA << ", SIZE: " << size;
    // ------------STEP4: Gen CRC and store in struct if current IO operation is "Write"
#if (returnLBAonly == 0)
    if (op == 1)
    {
        crc = genCRCfromData(data, size);
        printf(" CRC: 0x%x, ", crc);
        auto currentType = fileNameSplit[fileNameSplit.size() - 2];
        if (currentType == "param")
        {
            if (fileNameSplit[fileNameSplit.size() - 1] == "bfloat16")
            {
                iterForLayerMgr->second.crcMgr.param16 = crc;
            }
            else if (fileNameSplit[fileNameSplit.size() - 1] == "float32")
            {
                iterForLayerMgr->second.crcMgr.param32 = crc;
            }
            else
            {
                assertm(0, "ASSERT 0156: Unit not match");
            }
        }
        else if (currentType == "gradient")
        {
            iterForLayerMgr->second.crcMgr.gradient32 = crc;
        }
        else if (currentType == "momentum")
        {
            iterForLayerMgr->second.crcMgr.momentum32 = crc;
        }
        else if (currentType == "variance")
        {
            iterForLayerMgr->second.crcMgr.variance32 = crc;
        }
        else
        {
            assertm(0, "ASSERT 0166: Type not match");
        }
    }
#if currentIOMethod == METHOD_IOCTL
    // ------------STEP6: Call NVME handler to execute IOCTL function
    // char *write_buffer = new char[size]{0};
    // generate_random_data(write_buffer, size);
    // memset(write_buffer, 0x0, size);
    returnVal = nvme_operation_handler(currentLBA, size, op, data);
    // delete[] write_buffer;
#else
    AIOAsyncIO *aios = new AIOAsyncIO(32);
    int n = 0;
#if libAIOasync
    auto offset = 0;
    for (int i = 0; i < 32; i++) {
        auto fn = std::bind(callback_n, std::ref(n));
#if libAIOPhysical
        int fd = open("/dev/nvme0n1", O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
        if (fd == -1){
            perror("open fail");
        }
        fdArray.push_back(fd);
#else
        string copyFileName(fileName);
        size_t firstDotPos = copyFileName.find(".");
        string newFileName = copyFileName.substr(0,firstDotPos) + "_" + to_string(i) + copyFileName.substr(firstDotPos);
        const char *cstr = newFileName.c_str();
        int fd = open(cstr, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
        if (fd == -1){
            perror("open fail");
        }
        fdArray.push_back(fd);
#endif
        if (op == 1){
#if libAIOPhysical
            aios->write(fd, data, static_cast<unsigned long>(size), (currentLBA << 9) + offset, fn);
#else
            aios->write(fd, data, static_cast<unsigned long>(size), 0, fn);
#endif
        } else {
#if libAIOPhysical
            aios->read(fd, data, static_cast<unsigned long>(size), (currentLBA << 9) + offset, fn);
#else
            aios->read(fd, data, static_cast<unsigned long>(size), 0, fn);
#endif
        }
        offset += size;
    }
    if (op == 1) {
        aios->sync_write_events();
    } else {
        aios->sync_read_events();
    }
    for (auto &fd : fdArray){
        close(fd);
    }
    fdArray.clear();
#else
#if libAIOPhysical
    int fd = open("/dev/nvme0n1", O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
#else
    int fd = open(fileName, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    
#endif
    if (fd == -1){
        perror("open fail");
    }
    auto fn = bind(callback_n, std::ref(n));
    if (op == 1) {
#if libAIOPhysical
        aios->write(fd, data, static_cast<unsigned long>(size), (currentLBA << 9), fn);
#else
        aios->write(fd, data, static_cast<unsigned long>(size), 0, fn);
#endif
        aios->sync_write_events();
    } else {
#if libAIOPhysical
        aios->read(fd, data, static_cast<unsigned long>(size), (currentLBA << 9), fn);
#else
        aios->read(fd, data, static_cast<unsigned long>(size), 0, fn);
#endif
        aios->sync_read_events();
    }
    close(fd);
    delete aios;
#endif
#endif

    // ------------STEP7: If current IO operation is "Read", Gen CRC after read and compare to stored one
    if (op == 0)
    {
        crc = genCRCfromData(data, size);
        printf(" CRC: 0x%x, ", crc);
        auto currentType = fileNameSplit[fileNameSplit.size() - 2];
        if (currentType == "param")
        {
            if (fileNameSplit[fileNameSplit.size() - 1] == "bfloat16")
            {
                assertm(iterForLayerMgr->second.crcMgr.param16 == crc, "ASSERT 0189: Read op fp16 CRC not match");
            }
            else if (fileNameSplit[fileNameSplit.size() - 1] == "float32")
            {
                assertm(iterForLayerMgr->second.crcMgr.param32 == crc, "ASSERT 0192: Read op fp32 CRC not match");
            }
            else
            {
                assertm(0, "ASSERT 0215: Unit not match");
            }
        }
        else if (currentType == "gradient")
        {
            assertm(iterForLayerMgr->second.crcMgr.gradient32 == crc, "ASSERT 0197: Read op gd32 CRC not match");
        }
        else if (currentType == "momentum")
        {
            assertm(iterForLayerMgr->second.crcMgr.momentum32 == crc, "ASSERT 0199: Read op mm32 CRC not match");
        }
        else if (currentType == "variance")
        {
            assertm(iterForLayerMgr->second.crcMgr.variance32 == crc, "ASSERT 0201: Read op vr32 CRC not match");
        }
        else
        {
            assertm(0, "ASSERT 0203: Type not match");
        }
    }
#if debugCompare
    if ((op == 0) || (iterForLayerMgr->first == "13") && op == 1)
    {
        returnVal = nvme_operation_handler(15667200, 33554432, 0, data);
        // ofstream myfile2;
        // myfile2.open("output12_after.log");
        float *newData = reinterpret_cast<float *>(data);
        cout << "-DEBUG B-" << endl;
        for (auto i = 0; i < 32; i++)
        {
            printf("%f ", newData[i]);
        }
        // for (unsigned long int i = 0; i < 8388608; i++){
        //     myfile2 << to_string(newData[i]) << " ";
        //     if (i % 16 == 0 && i > 0){
        //         myfile2 << "\n";
        //     }
        // }
        // myfile2.close();
        cout << endl;
    }
#endif
    cout << endl;
    return returnVal;
#else
    return currentLBA;
#endif
}

uint32_t layerVecMap::genCRCfromData(void *data, unsigned long long size)
{
    uint32_t table[256];
    crc32::generate_table(table);
    unsigned char *newData = reinterpret_cast<unsigned char *>(data);
    uint32_t CRC = 0;
    for (unsigned long long cnt = 0; cnt < 8; cnt++)
    {
        CRC = crc32::update(table, CRC, newData, 1);
        newData++;
    }
    // CRC = crc32::update(table, CRC, (newData + size - 8), 8);
    return CRC;
}

unsigned long long layerVecMap::getLBAfromList(vector<string> fileNameSplit, layerInfoMgr tempLayerInfoMgr, unsigned long long *tempSize)
{
    auto currentType = fileNameSplit[fileNameSplit.size() - 2];
    if (currentType == "param")
    {
        if (fileNameSplit[fileNameSplit.size() - 1] == "bfloat16")
        {
            *tempSize = stoull(fileNameSplit[1]) << 1;
            return tempLayerInfoMgr.lbaMgr.param16;
        }
        else if (fileNameSplit[fileNameSplit.size() - 1] == "float32")
        {
            *tempSize = stoull(fileNameSplit[1]) << 2;
            return tempLayerInfoMgr.lbaMgr.param32;
        }
        else
        {
            assertm(0, "ASSERT 0163: Unit not match");
        }
    }
    else if (currentType == "gradient")
    {
        *tempSize = stoull(fileNameSplit[1]) << 2;
        return tempLayerInfoMgr.lbaMgr.gradient32;
    }
    else if (currentType == "momentum")
    {
        *tempSize = stoull(fileNameSplit[1]) << 2;
        return tempLayerInfoMgr.lbaMgr.momentum32;
    }
    else if (currentType == "variance")
    {
        *tempSize = stoull(fileNameSplit[1]) << 2;
        return tempLayerInfoMgr.lbaMgr.variance32;
    }
    else
    {
        assertm(0, "ASSERT 0155: Type not match");
    }
    return 0;
}

LBAMgr layerVecMap::getTempLBAMgr(vector<string> fileNameSplit, vector<string> filePathSplit)
{
    LBAMgr tempLBAMgr;
    layerVecMap &layerVecMapMgr = layerVecMap::getInstance();
    unsigned long long param = ALIGN_LAYER_PARAM(stoull(fileNameSplit[1]));
    assert(stoull(fileNameSplit[0]) < MAX_LAYER_SIZE);
    tempLBAMgr.param16 = layerVecMapMgr.allocLBAoffset;
    tempLBAMgr.param32 = tempLBAMgr.param16 + ((param * 2) >> 9) + 8;
    tempLBAMgr.gradient32 = tempLBAMgr.param32 + ((param * 4) >> 9) + 8;
    tempLBAMgr.momentum32 = tempLBAMgr.gradient32 + ((param * 4) >> 9) + 8;
    tempLBAMgr.variance32 = tempLBAMgr.momentum32 + ((param * 4) >> 9) + 8;
    layerVecMapMgr.allocLBAoffset = tempLBAMgr.variance32 + ((param * 4) >> 9) + 8;
    return tempLBAMgr;
}

layerVecMap &layerVecMap::getInstance()
{
    return instance;
}

void layerVecMap::clearMap()
{
    layerVecMap &layerVecMapMgr = layerVecMap::getInstance();
    layerVecMapMgr.layerMgr.clear();
    layerVecMapMgr.allocLBAoffset = 0;
    layerVecMapMgr.prevOperationMgr.prevFileNameSplit.clear();
    layerVecMapMgr.prevOperationMgr.prevFilePathSplit.clear();
    instance.prevOperationMgr.op = 3;
}