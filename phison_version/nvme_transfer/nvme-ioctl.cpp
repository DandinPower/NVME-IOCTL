#include <stdio.h>
#include <stdlib.h>
#include <linux/nvme_ioctl.h>
#include <linux/types.h>
#include <stdbool.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <endian.h>
#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <locale.h>
#include <math.h>
#include <iostream>
#include <linux/fs.h>
#include <random>
#include <ctime>
#include <cstdlib>
#include <cassert>
#include <memory>
#include <thread>
#include <future>
#include <vector>
#include <iostream>
#include <functional>
#include <chrono>
#define asyncFeat 0
#define chunkSize 0xFF8
#define posixMemalignFeat 1
#if (asyncFeat == 1 && posixMemalignFeat == 1)
#error "NOT HAVE THIS SETTING IN CURRENT CODE"
#endif
using namespace std;

enum nvme_opcode
{
    nvme_cmd_flush = 0x00,
    nvme_cmd_write = 0x01,
    nvme_cmd_read = 0x02,
    nvme_cmd_write_uncor = 0x04,
    nvme_cmd_compare = 0x05,
    nvme_cmd_write_zeroes = 0x08,
    nvme_cmd_dsm = 0x09,
    nvme_cmd_resv_register = 0x0d,
    nvme_cmd_resv_report = 0x0e,
    nvme_cmd_resv_acquire = 0x11,
    nvme_cmd_resv_release = 0x15,
};

int nvme_io(int fd, __u8 opcode, __u64 slba, __u16 nblocks, __u16 control,
            __u32 dsmgmt, __u32 reftag, __u16 apptag, __u16 appmask, void *data,
            void *metadata)
{
    struct nvme_user_io io = {
        .opcode = opcode,
        .flags = 0,
        .control = control,
        .nblocks = nblocks,
        .rsvd = 0,
        .metadata = (__u64)(uintptr_t)metadata,
        .addr = (__u64)(uintptr_t)data,
        .slba = slba,
        .dsmgmt = dsmgmt,
        .reftag = reftag,
        .apptag = apptag,
        .appmask = appmask,
    };

    return ioctl(fd, NVME_IOCTL_SUBMIT_IO, &io);
}
int nvme_write(int fd, __u64 slba, __u16 nblocks, __u16 control, __u32 dsmgmt,
               __u32 reftag, __u16 apptag, __u16 appmask, void *data,
               void *metadata)
{
#if posixMemalignFeat
    void *memptr2;
    auto ret = posix_memalign(&memptr2, 512, ((nblocks + 1) << 9));
    assert(ret == 0);
    memcpy(memptr2, data, (nblocks << 9));
    auto returnVal = nvme_io(fd, nvme_cmd_write, slba, nblocks, control, dsmgmt, reftag, apptag, appmask, memptr2, metadata);
    free(memptr2);
    return returnVal;
#else
    return nvme_io(fd, nvme_cmd_write, slba, nblocks, control, dsmgmt, reftag, apptag, appmask, data, metadata);
#endif
}
int nvme_read(int fd, __u64 slba, __u16 nblocks, __u16 control, __u32 dsmgmt,
              __u32 reftag, __u16 apptag, __u16 appmask, void *data,
              void *metadata)
{
#if posixMemalignFeat
    void *memptr;
    auto ret = posix_memalign(&memptr, 512, ((nblocks + 1) << 9));
    assert(ret == 0);
    auto returnVal = nvme_io(fd, nvme_cmd_read, slba, nblocks, control, dsmgmt, reftag, apptag, appmask, memptr, metadata);
    memcpy(data, memptr, (nblocks << 9));
    free(memptr);
    return returnVal;
#else
    return nvme_io(fd, nvme_cmd_read, slba, nblocks, control, dsmgmt, reftag, apptag, appmask, data, metadata);
#endif
}

static int nvme_submit_io_passthru(int fd, struct nvme_passthru_cmd *cmd)
{
    return ioctl(fd, NVME_IOCTL_IO_CMD, cmd);
}

int nvme_flush(int fd, __u32 nsid)
{
    struct nvme_passthru_cmd cmd = {
        .opcode = nvme_cmd_flush,
        .nsid = nsid,
    };

    return nvme_submit_io_passthru(fd, &cmd);
}

// int nvme_write_zeros(int fd, __u32 nsid, __u64 slba, __u16 nlb,
// 		     __u16 control, __u32 reftag, __u16 apptag, __u16 appmask)
// {
// 	struct nvme_passthru_cmd cmd = {
// 		.opcode		= nvme_cmd_write_zeroes,
// 		.nsid		= nsid,
// 		.cdw10		= slba & 0xffffffff,
// 		.cdw11		= slba >> 32,
// 		.cdw12		= nlb | (control << 16),
// 		.cdw14		= reftag,
// 		.cdw15		= apptag | (appmask << 16),
// 	};

// 	return nvme_submit_io_passthru(fd, &cmd);
// }

int nvme_operation_handler(__u64 slba, __u64 size, unsigned short op, void *data)
{

    // ------------STEP1: Open NVME device
    int fd = open("/dev/nvme0n1p1", O_RDWR | O_DIRECT | O_SYNC, 0);
    int returnVal = 0;
    __u16 nblock = 0;
    __u16 control = 1 << 14; // force_unit_access
    unsigned char *newData = reinterpret_cast<unsigned char *>(data);
    int phys_sector_size = 0;
    __u64 backupSize = size;
#if asyncFeat
    std::vector<std::future<int>> asyncFutures;
#if (posixMemalignFeat == 0)
    std::vector<void*> data_pointers;
    int num_buffers = (size >> 9) / chunkSize + 1;
    int pointer_idx = 0;
    for (int i = 0; i < num_buffers; ++i) {
        void* data_ptr;
        if (posix_memalign(&data_ptr, 512, (chunkSize + 1) << 9) != 0) {
            std::cerr << "Memory allocation failed." << std::endl;
            return 1;
        }
        // save data ptr
        data_pointers.push_back(data_ptr);
    }
#endif
#endif
    if (fd < 0)
    {
        returnVal = -1;
        perror("open fd failed");
        goto out_no_fd;
    }

    // Get size per LBA
    // ioctl(fd, BLKPBSZGET, &phys_sector_size);
    // std::cout << phys_sector_size << std::endl;

    cout << endl
         << "STA: " << slba;

    // ------------STEP2: Break down to many small size CMD
    while (size > 0)
    {
        if ((size >> 9) > chunkSize)
        {
            nblock = chunkSize;
            size -= (nblock << 9);
        }
        else
        {
            nblock = (size >> 9);
            size = 0;
        }
#if (posixMemalignFeat == 0)
        if (op == 1){
            memcpy(data_pointers[pointer_idx], data, (nblock << 9));
        }
#endif
        switch (op)
        {
        case 0:
#if asyncFeat
#if (posixMemalignFeat == 0)
            asyncFutures.push_back(std::async(std::launch::async, nvme_read, fd, slba, nblock, control, 0, 0, 0, 0, data_pointers[pointer_idx], nullptr));
#else
            asyncFutures.push_back(std::async(std::launch::async, nvme_read, fd, slba, nblock, control, 0, 0, 0, 0, newData, nullptr));
#endif
#else
            returnVal = nvme_read(fd, slba, nblock, control, 0, 0, 0, 0, newData, nullptr);
#endif
            break;
        case 1:
#if asyncFeat
#if (posixMemalignFeat == 0)
            asyncFutures.push_back(std::async(std::launch::async, nvme_write, fd, slba, nblock, control, 0, 0, 0, 0, data_pointers[pointer_idx], nullptr));
#else
            asyncFutures.push_back(std::async(std::launch::async, nvme_write, fd, slba, nblock, control, 0, 0, 0, 0, newData, nullptr));
#endif
#else
            returnVal = nvme_write(fd, slba, nblock, control, 0, 0, 0, 0, newData, nullptr);
#endif
            break;
        default:
            assert(0);
            break;
        }
#if (posixMemalignFeat == 0)
        pointer_idx++;
#endif
        newData += (nblock << 9);
        slba += nblock;
    }
    cout << ", END: " << slba << endl;
#if asyncFeat
    for (auto &future : asyncFutures)
    {
        int result = future.get();
        if (result != 0)
        {
            perror("nvme_write/read failed");
            returnVal = -1;
            assert(0);
        }
    }
#if (posixMemalignFeat == 0)
    for (auto &dataBuffer : data_pointers)
    {
        assert(backupSize != 0);
        if ((backupSize >> 9) > chunkSize)
        {
            nblock = chunkSize;
            backupSize -= (nblock << 9);
        }
        else
        {
            nblock = (backupSize >> 9);
            backupSize = 0;
        }
        if (op == 0){
            memcpy(data, dataBuffer, (nblock << 9));
        }
        free(dataBuffer);
    }
    data_pointers.clear();
#endif
#endif
out:
    close(fd);
out_no_fd:
    assert(returnVal == 0);
    return returnVal;
}

// int main()
// {
//     int fd = open("/dev/nvme0n1", O_RDWR, 0);
//     unsigned char* wptr = new unsigned char[33554432]{0};
//     unsigned char* rptr = new unsigned char[33554432]{0};
//     std::random_device rd;
//     std::mt19937 gen(rd());
//     std::uniform_int_distribution<unsigned int> dis(0x00, 0xFF);
//     for (int k = 0; k < 33554432; k++){
//         wptr[k] = dis(gen);
//         // printf("read: slba[%d]=0x%d\n", k, wptr[k]);
//     }
//     auto start = chrono::steady_clock::now();
//     auto mid = chrono::steady_clock::now();
//     for (int i =0; i < 1; i++){
//         // std::cout << i << std::endl;
//         if(nvme_operation_handler(0, 33554432, 1, wptr)){
//             perror("write fail");
//         }
//         mid = chrono::steady_clock::now();
//         if(nvme_operation_handler(0, 33554432, 0, rptr)){
//             perror("read fail");
//         }
//         for (int k = 0; k < 100; k++){
//             if (wptr[k] != rptr[k]){
//                 assert(0);
//             }

//         }
//     }
//     auto end = chrono::steady_clock::now();
//     // Calculating total time taken by the program.
//     auto diff = mid - start;
//     auto diff2 = end - mid;
//     cout << chrono::duration <double, milli> (diff).count() << " ms" << endl;
//     cout << chrono::duration <double, milli> (diff2).count() << " ms" << endl;
//     delete [] wptr;
//     delete [] rptr;
//     return 0;
// }
