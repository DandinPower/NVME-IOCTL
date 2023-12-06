#pragma once
#include <string>           // for std::string
#include <fcntl.h>          // for open()
#include <nvme_ioctl.h>     // for struct nvme_user_io
#include <utils.h>          // for assert_msg()
#include <chrono>           // for std::chrono::high_resolution_clock

int submit_test(char* writeBuffer, char* readBuffer, int file_bytes, int fd, unsigned long long start_lba) {
    int ret;

#if IOCTL_STRATEGY
    // ioctl strategy
#if ASYNC_IOCTL
    // async ioctl strategy
#else 
    // sync ioctl strategy
    // count the number of blocks needed for this test file
    // use the sector size to divide the test file bytes to get the number of blocks
    // in each submit, we at most can submit CHUNK_SIZE bytes
    // so we need to submit test_file_bytes / CHUNK_SIZE times
    // so each submit, may have different number of blocks
    // the last submit may have less blocks than the others

    // write
    const int nvme_sector_size = NVME_SECTOR_SIZE;
    const int chunk_size = CHUNK_SIZE;
    int aligned_nvme_sector_file_bytes = get_aligned_size(nvme_sector_size, file_bytes);
    int nblocks = aligned_nvme_sector_file_bytes / nvme_sector_size;
    int remainBlocks = nblocks;
    int eachSubmitBlocks = chunk_size / nvme_sector_size;
    unsigned long long currentLba = start_lba;
    char* currentWriteBuffer = writeBuffer;
    char* currentReadBuffer = readBuffer;

    std::chrono::high_resolution_clock::time_point startWrite, endWrite, startRead, endRead;
    std::chrono::duration<double> writeTime, readTime;
    startWrite = std::chrono::high_resolution_clock::now();
    while (remainBlocks > 0) {
        if (remainBlocks < eachSubmitBlocks) {
            eachSubmitBlocks = remainBlocks;
        }
        ret = submit_io(fd, currentWriteBuffer, WRITE, eachSubmitBlocks, currentLba);
        assert_msg(ret == 0, "submit_io failed");
        if (ret) goto fail;
        remainBlocks -= eachSubmitBlocks;
        currentLba += eachSubmitBlocks;
        currentWriteBuffer += eachSubmitBlocks * nvme_sector_size;
    }
    endWrite = std::chrono::high_resolution_clock::now();

    startRead = std::chrono::high_resolution_clock::now();
    remainBlocks = nblocks;
    eachSubmitBlocks = chunk_size / nvme_sector_size;
    currentLba = start_lba;
    while (remainBlocks > 0) {
        if (remainBlocks < eachSubmitBlocks) {
            eachSubmitBlocks = remainBlocks;
        }
        ret = submit_io(fd, currentReadBuffer, READ, eachSubmitBlocks, currentLba);
        assert_msg(ret == 0, "submit_io failed");
        if (ret) goto fail;
        remainBlocks -= eachSubmitBlocks;
        currentLba += eachSubmitBlocks;
        currentReadBuffer += eachSubmitBlocks * nvme_sector_size;
    }
    endRead = std::chrono::high_resolution_clock::now();

    // compare
    ret = memcmp(writeBuffer, readBuffer, file_bytes);
    assert_msg(ret == 0, "memcmp failed");
    if (ret) goto fail;

    // print time
    writeTime = endWrite - startWrite;
    readTime = endRead - startRead;
    printf("write time: %fms\n", writeTime.count() * 1000);
    printf("read time: %fms\n", readTime.count() * 1000);

#endif

#else 
    // libaio strategy
#endif

    return 0;
fail:
    return 1;
}

int submit_tests(std::string device_path, char* writeBuffer, char* readBuffer, int test_file_bytes, int aligned_test_file_bytes, int test_file_number) {
    unsigned long long start_lba = START_LBA;
    int fd = open(device_path.c_str(), O_RDWR | O_DIRECT | O_SYNC);
    assert_msg(fd >= 0, "open device failed");
    if (fd < 0) goto fail;
    // because we allocate the buffer with all test files
    // so we need to step the buffer to submit each test file
    // and the step is aligned_test_file_bytes
    // besides, the start lba is also step by aligned_test_file_bytes
    int ret;
    for (int i = 0; i < test_file_number; i++) {
        ret = submit_test(writeBuffer + i * aligned_test_file_bytes, readBuffer + i * aligned_test_file_bytes, test_file_bytes, fd, start_lba + i * aligned_test_file_bytes);    
        assert_msg(ret == 0, "submit test failed");
        if (ret) goto fail;
    }
    return 0;
fail:
    return 1;
}