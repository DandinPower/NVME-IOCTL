#pragma once
#include <string>           // for std::string
#include <fcntl.h>          // for open()
#include <nvme_ioctl.h>     // for struct nvme_user_io
#include <utils.h>          // for assert_msg()
#include <chrono>           // for std::chrono::high_resolution_clock
#include <future>           // for std::future
#include <vector>           // for std::vector

std::chrono::duration<double> total_write_time, total_read_time;

/**
 * @brief Submits a test for NVMe device.
 * This function submits a test for NVMe device. It calculates the number of blocks needed for the test file and submits the IO operations. It also measures the time taken for write and read operations.
 * @param writeBuffer The buffer to write.
 * @param readBuffer The buffer to read.
 * @param file_bytes The size of the file in bytes.
 * @param fd The file descriptor of the NVMe device.
 * @param start_lba The starting Logical Block Address.
 * @return 0 if the test was successful, 1 otherwise.
*/
int submit_test(char* writeBuffer, char* readBuffer, int file_bytes, int fd, unsigned long long start_lba) {
    int ret;

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

    std::vector<std::future<int>> futures;
#if ASYNC_IOCTL
    while (remainBlocks > 0) {
        if (remainBlocks < eachSubmitBlocks) {
            eachSubmitBlocks = remainBlocks;
        }
        futures.push_back(std::async(std::launch::async, submit_io, fd, currentWriteBuffer, WRITE, eachSubmitBlocks, currentLba));
        remainBlocks -= eachSubmitBlocks;
        currentLba += eachSubmitBlocks;
        currentWriteBuffer += eachSubmitBlocks * nvme_sector_size;
    }
    for (auto &f : futures) {
        ret = f.get();
        assert_msg(ret == 0, "submit_io failed");
        if (ret) goto fail;
    }
#else 
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
#endif
    endWrite = std::chrono::high_resolution_clock::now();

    startRead = std::chrono::high_resolution_clock::now();
    remainBlocks = nblocks;
    eachSubmitBlocks = chunk_size / nvme_sector_size;
    currentLba = start_lba;
#if ASYNC_IOCTL
    futures.clear();
    while (remainBlocks > 0) {
        if (remainBlocks < eachSubmitBlocks) {
            eachSubmitBlocks = remainBlocks;
        }
        futures.push_back(std::async(std::launch::async, submit_io, fd, currentReadBuffer, READ, eachSubmitBlocks, currentLba));
        remainBlocks -= eachSubmitBlocks;
        currentLba += eachSubmitBlocks;
        currentReadBuffer += eachSubmitBlocks * nvme_sector_size;
    }
    for (auto &f : futures) {
        ret = f.get();
        assert_msg(ret == 0, "submit_io failed");
        if (ret) goto fail;
    }
#else
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
#endif
    endRead = std::chrono::high_resolution_clock::now();

    // compare
    ret = memcmp(writeBuffer, readBuffer, file_bytes);
    assert_msg(ret == 0, "memcmp failed");
    if (ret) goto fail;

    // print time
    writeTime = endWrite - startWrite;
    readTime = endRead - startRead;
    total_write_time += writeTime;
    total_read_time += readTime;
    printf("write time: %fms\n", writeTime.count() * 1000);
    printf("read time: %fms\n", readTime.count() * 1000);

    return 0;
fail:
    return 1;
}

/**
 * @brief Submits multiple tests for NVMe device.
 * This function submits multiple tests for NVMe device. It opens the device, allocates buffers for read and write, initializes the write buffer, and submits the tests. It also calculates the average write and read times and throughputs.
 * @param device_path The path of the NVMe device.
 * @param file_bytes The size of the file in bytes.
 * @param aligned_file_bytes The size of the file in bytes, aligned to NVMe sector size.
 * @param test_round The number of test rounds.
 * @return 0 if the tests were successful, 1 otherwise.
*/
int submit_tests(std::string device_path, int file_bytes, int aligned_file_bytes, int test_round) {
    int ret;
    unsigned long long start_lba = START_LBA;
    int fd = open(device_path.c_str(), O_RDWR | O_DIRECT | O_SYNC);
    assert_msg(fd >= 0, "open device failed");
    if (fd < 0) goto fail;

    // allocate huge buffer for read and write
    char *write_buffer, *read_buffer; 
    ret = allocate_buffer(aligned_file_bytes, (void**)&write_buffer);
    assert_msg(ret == 0, "allocate_buffer failed", true);
    ret = allocate_buffer(aligned_file_bytes, (void **)&read_buffer);
    assert_msg(ret == 0, "allocate_buffer failed", true);

    // init write buffer
    init_random_buffer(aligned_file_bytes, write_buffer);

    for (int i = 0; i < test_round; i++) {
        // reset read buffer
        init_empty_buffer(aligned_file_bytes, read_buffer);
        ret = submit_test(write_buffer, read_buffer, file_bytes, fd, start_lba);    
        assert_msg(ret == 0, "submit test failed");
        if (ret) goto fail;
    }
    printf("Avg. write time: %fms\n", total_write_time.count() * 1000 / test_round);
    printf("Avg. read time: %fms\n", total_read_time.count() * 1000 / test_round);
    printf("Avg. write throughput: %fMB/s\n", (double)file_bytes / total_write_time.count() / 1024 / 1024 * test_round);
    printf("Avg. read throughput: %fMB/s\n", (double)file_bytes / total_read_time.count() / 1024 / 1024 * test_round);
    
    // free buffer
    free_buffer(write_buffer, aligned_file_bytes);
    free_buffer(read_buffer, aligned_file_bytes);
    return 0;
fail:
    // free buffer
    free_buffer(write_buffer, aligned_file_bytes);
    free_buffer(read_buffer, aligned_file_bytes);
    return 1;
}