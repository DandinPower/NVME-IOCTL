#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <cstring>
#include <chrono>
#include <cmath>
#include <linux/types.h>
#include <linux/nvme_ioctl.h>
#include <cassert>

#include <pthread.h>

#define NVME_SECTOR_SIZE 512
#define MAX_BLOCK_NUM 896
#define SAFE_DEVICE_NAME "/dev/nvme1n1p2"
#define MB 1024 * 1024

enum OPCODE {
    WRITE,
    READ
};

size_t get_aligned_size(size_t alignment, size_t size) {
    assert((alignment & (alignment - 1)) == 0);  // Ensure alignment is a power of 2
    return (size + alignment - 1) & ~(alignment - 1);
}

void check_error(bool condition, const std::string &msg) {
    if (condition) {
        std::cerr << msg << ": " << std::strerror(errno) << std::endl;
        std::exit(1);
    }
}
__u8 get_opcode(OPCODE opcode) {
    check_error(opcode != WRITE && opcode != READ, "Wrong opcode Type");
    if (opcode == WRITE) return 0x01;
    return 0x02;
}

void generate_random_data(char* buffer, size_t buffer_size) {
    for (size_t i = 0; i < buffer_size; ++i) {
        buffer[i] = rand() % 256;
    }
}

size_t div_round_up(size_t numerator, size_t denominator) {
    return (numerator + denominator - 1) / denominator;
}

__u16 get_operate_block(size_t remainBlocks) {
    return remainBlocks < MAX_BLOCK_NUM ? remainBlocks : MAX_BLOCK_NUM;
}

typedef struct _nvme_io_param {
    int fd;
    __u8 opcode;
    __u16 nblocks;
    __u64 addr;
    __u64 slba;
} _nvme_io_param_t;

// 由於ioctl的nblock有限制, 因此需要拆成多個小的io_request
void* _nvme_io(void* args) {
    _nvme_io_param_t* params = (_nvme_io_param_t*) args;

    struct nvme_user_io io = {
        .opcode = params->opcode,
        .flags = 0,
        .control = 0,
        .nblocks = params->nblocks,
        .rsvd = 0,
        .metadata = 0,
        .addr = params->addr,
        .slba = params->slba,
        .dsmgmt = 0,
        .reftag = 0,
        .apptag = 0,
        .appmask = 0
    };

    int *result = new int;
    *result = ioctl(params->fd, NVME_IOCTL_SUBMIT_IO, &io);
    // *result = 0;
    // check_error(ioctl(params->fd, NVME_IOCTL_SUBMIT_IO, &io) < 0, "Fail to submit IO");
    return result;
}

void nvme_io(int fd, OPCODE ioType, char* buffer, const size_t bufferSize) {
    size_t singleBufferInThread = MAX_BLOCK_NUM * NVME_SECTOR_SIZE;
    size_t threadNums = div_round_up(bufferSize, singleBufferInThread);
    pthread_t* threads = new pthread_t[threadNums];
    _nvme_io_param_t* params = new _nvme_io_param_t[threadNums];
    size_t threadCounts = 0;

    __u8 opcode = get_opcode(ioType);
    __u64 currentLba = 0;

    size_t remainBlocks = div_round_up(bufferSize, NVME_SECTOR_SIZE);
    size_t offsetBytes = 0;

    while (remainBlocks > 0) {
        __u16 operateBlocks = get_operate_block(remainBlocks);
        char* currentBuffer = buffer + offsetBytes;

        params[threadCounts].fd = fd;
        params[threadCounts].opcode = opcode;
        params[threadCounts].nblocks = (operateBlocks - 1);
        params[threadCounts].addr = (__u64)currentBuffer;
        params[threadCounts].slba = currentLba;

        check_error(pthread_create(&threads[threadCounts], NULL, _nvme_io, (void*) &params[threadCounts]) != 0, "Fail to create thread");
        offsetBytes += operateBlocks * NVME_SECTOR_SIZE;
        currentLba += operateBlocks;
        remainBlocks -= operateBlocks;
        threadCounts += 1;
    }

    bool error = false;
    for (size_t i = 0; i < threadCounts; i++) {
        int *result;
        check_error(pthread_join(threads[i], (void **)&result) != 0, "Fail to join thread");
        error = *result < 0;
        delete result;
    }

    delete [] threads;
    delete [] params;

    check_error(error, "Fail to submit IO");

}
typedef struct SingleRecord {
    double writeLatency;
    double readLatency;
} SingleRecord_t;

void single_test(SingleRecord_t *record, const char* path, char* writeBuffer, char* readBuffer, const size_t bufferSize) {
    // 打開NVMe設備
    int fd = open(path, O_RDWR | O_DIRECT);
    check_error(fd < 0, "Failed to open NVMe device");
    
    auto startWrite = std::chrono::high_resolution_clock::now();
    nvme_io(fd, WRITE, writeBuffer, bufferSize);
    auto endWrite = std::chrono::high_resolution_clock::now();
    auto startRead = std::chrono::high_resolution_clock::now();
    nvme_io(fd, READ, readBuffer, bufferSize);
    auto endRead = std::chrono::high_resolution_clock::now();

    if (std::memcmp(writeBuffer, readBuffer, bufferSize) == 0) {
        std::cout << "Validation successful: Data matches!" << std::endl;
    } else {
        std::cout << "Validation failed: Data mismatch!" << std::endl;
    }
    
    close(fd);

    std::chrono::duration<double> writeTime = endWrite - startWrite;
    std::chrono::duration<double> readTime = endRead - startRead;
    size_t writeSpeedMBs = std::round(static_cast<double>(bufferSize) / writeTime.count() / static_cast<double>(MB));
    size_t readSpeedMBs = std::round(static_cast<double>(bufferSize) / readTime.count() / static_cast<double>(MB));
    std::cout << "Write speed: " << writeSpeedMBs << " MB/s" << std::endl;
    std::cout << "Read speed: " << readSpeedMBs << " MB/s" << std::endl;
    record->writeLatency = writeTime.count();
    record->readLatency = readTime.count();

}


void io_test(const char* path, const size_t bufferSize, int testRound) {
    double totalWriteLatency = 0.0;
    double totalReadLatency = 0.0;
    SingleRecord_t record;

    size_t bufferSizeAligned = get_aligned_size(NVME_SECTOR_SIZE ,bufferSize);

    // Allocate aligned memory for write and read
    char *writeBuffer, *readBuffer;
    check_error(posix_memalign((void **)&writeBuffer, NVME_SECTOR_SIZE, bufferSizeAligned), "posix_memalign error for write_data");
    check_error(posix_memalign((void **)&readBuffer, NVME_SECTOR_SIZE, bufferSizeAligned), "posix_memalign error for read_data");

    for (int i=0; i < testRound; i++) {
        std::cout << "[Round: " << i << "]" << std::endl;
        generate_random_data(writeBuffer, bufferSizeAligned);
        single_test(&record, path, writeBuffer, readBuffer, bufferSizeAligned);
        totalWriteLatency += record.writeLatency;
        totalReadLatency += record.readLatency;
    }
    
    double averageWriteLatency = totalWriteLatency / testRound;
    double averageReadLatency = totalReadLatency / testRound;

    size_t avgWriteSpeedMBs = std::round(static_cast<double>(bufferSizeAligned) / averageWriteLatency / static_cast<double>(MB));
    size_t avgReadSpeedMBs = std::round(static_cast<double>(bufferSizeAligned) / averageReadLatency / static_cast<double>(MB));
    std::cout << std::endl << "[IO Test]" << " Device:" << path << " Buffer Size: " << bufferSizeAligned << " Test Rounds: " << testRound <<std::endl;
    std::cout << "Avg Write speed: " << avgWriteSpeedMBs << " MB/s" << std::endl;
    std::cout << "Avg Read speed: " << avgReadSpeedMBs << " MB/s" << std::endl;

    std::free(writeBuffer);
    std::free(readBuffer);
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Please provide <buffer size in MB> <test round>" << std::endl;
        return 1;
    }
    size_t bufferSize = std::stoi(argv[1]) * MB;
    int testRound = std::stoi(argv[2]);
    io_test(SAFE_DEVICE_NAME, bufferSize, testRound);
    return EXIT_SUCCESS;
}
