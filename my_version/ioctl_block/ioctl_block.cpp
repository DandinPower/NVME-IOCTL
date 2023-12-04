#include <utils.h>
#include <config.h>
#include <phison.h>

#include <sys/mman.h>
#include <stdio.h>
#include <memory.h>

bool submit_io(int fd, char* buffer, OPCODE io_type, unsigned short nblocks, unsigned long long currentLba) {
    // get opcode value based on the OPCODE enum
    __u8 opcode = get_opcode(io_type);

    // prepare the nvme_user_io struct
    struct nvme_user_io io = {
        .opcode = opcode,
        .flags = 0,
        .control = (__u16)(1 << 14), // force unit access
        .nblocks = (__u16)(nblocks - 1),
        .rsvd = 0,
        .metadata = (__u64)(uintptr_t)nullptr,
        .addr = (__u64)(uintptr_t)buffer,
        .slba = (__u64)START_LBA,
        .dsmgmt = 0,
        .reftag = 0,
        .apptag = 0,
        .appmask = 0
    };

    // submit the IO
    bool ret = ioctl(fd, NVME_IOCTL_SUBMIT_IO, &io) < 0;
    check_error(ret, "Failed to submit IO");
    return ret;
}

int main(void) {
    // open the nvme device; TODO: check if the O_DIRECT flag will cause difference in MDTS
    int fd = open(NVME_DEVICE, O_RDWR | O_DIRECT | O_SYNC, 0);

    // check if the device is opened successfully
    check_error(fd < 0, "Failed to open NVMe device");

    // get the aligned buffer size, ensuring that the buffer size is a multiple of the sector size
    size_t buffer_size = get_aligned_size(NVME_SECTOR_SIZE, NVME_SECTOR_SIZE * MAX_BLOCK_NUM);

    // allocate the buffer
    char *write_buffer, *read_buffer; 
    check_error(posix_memalign((void**)&write_buffer, NVME_SECTOR_SIZE, buffer_size) != 0, "Failed to allocate write buffer");
    check_error(posix_memalign((void**)&read_buffer, NVME_SECTOR_SIZE, buffer_size) != 0, "Failed to allocate read buffer");

    // write_buffer = (char*)mmap(NULL, buffer_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
    // if (write_buffer == MAP_FAILED) {
    //     perror("mmap");
    //     return -1;
    // }


    // initialize the buffer with random data
    generate_random_data(write_buffer, buffer_size);

    bool ret = submit_io(fd, write_buffer, WRITE, MAX_BLOCK_NUM, START_LBA);
    free(write_buffer);
    // munmap(write_buffer, buffer_size);
    close(fd);
    if(ret)
        return EXIT_FAILURE;
    else 
        return EXIT_SUCCESS;
}
