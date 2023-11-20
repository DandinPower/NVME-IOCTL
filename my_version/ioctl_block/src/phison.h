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
#define chunkSize 4088
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

int nvme_operation_handler(__u64 slba, __u64 size, unsigned short op, void *data)
{

    // ------------STEP1: Open NVME device
    int fd = open("/dev/nvme0n1", O_RDWR | O_DIRECT | O_SYNC, 0);
    int returnVal = 0;
    __u16 nblock = 0;
    __u16 control = 1 << 14; // force_unit_access
    unsigned char *newData = reinterpret_cast<unsigned char *>(data);
    int phys_sector_size = 0;
    __u64 backupSize = size;

    if (fd < 0)
    {
        returnVal = -1;
        perror("open fd failed");
        goto out_no_fd;
    }

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
        switch (op)
        {
        case 0:
            returnVal = nvme_read(fd, slba, nblock, control, 0, 0, 0, 0, newData, nullptr);
            assert(returnVal == 0);
            break;
        case 1:
            returnVal = nvme_write(fd, slba, nblock, control, 0, 0, 0, 0, newData, nullptr);
            assert(returnVal == 0);
            break;
        default:
            assert(0);
            break;
        }
        newData += (nblock << 9);
        slba += nblock;
    }
    cout << ", END: " << slba << endl;

out:
    close(fd);
out_no_fd:
    assert(returnVal == 0);
    return returnVal;
}
