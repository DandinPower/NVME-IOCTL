#pragma once
#include <linux/nvme_ioctl.h>   // for struct nvme_user_io
#include <sys/ioctl.h>          // for ioctl()
#include <stdint.h>             // for uintptr_t
#include <utils.h>              // for assert_msg()

/**
 * @brief Enum for operation codes.
 */
enum OPCODE {
    WRITE,
    READ
};

/**
 * @brief Get operation code based on the OPCODE enum.
 *
 * @param opcode The operation code.
 * @return The operation code as a byte.
 */
__u8 get_opcode(OPCODE opcode) {
    assert_msg(opcode == WRITE || opcode == READ, "invalid opcode");
    if (opcode == WRITE) return 0x01;
    return 0x02;
}

/**
 * @brief Submit an I/O operation.
 *
 * @param fd The file descriptor.
 * @param buffer The buffer for the operation.
 * @param io_type The type of I/O operation.
 * @param nblocks The number of blocks.
 * @param currentLba The current LBA.
 * @return 0 if the operation was successful, 1 otherwise.
 */
int submit_io(int fd, char* buffer, OPCODE io_type, unsigned short nblocks, unsigned long long currentLba) {
    __u8 opcode = get_opcode(io_type);

    struct nvme_user_io io = {
        .opcode = opcode,
        .flags = 0,
        .control = (__u16)(1 << 14),
        .nblocks = (__u16)(nblocks - 1),
        .rsvd = 0,
        .metadata = (__u64)(uintptr_t)nullptr,
        .addr = (__u64)(uintptr_t)buffer,
        .slba = (__u64)currentLba,
        .dsmgmt = 0,
        .reftag = 0,
        .apptag = 0,
        .appmask = 0
    };

    int ret = ioctl(fd, NVME_IOCTL_SUBMIT_IO, &io) < 0;
    assert_msg(ret == 0, "ioctl failed");
    return ret;
}