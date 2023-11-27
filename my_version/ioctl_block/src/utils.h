#pragma once
#include <iostream>             // for std::cerr, std::cout
#include <linux/nvme_ioctl.h>   // for struct nvme_user_io
#include <cstring>              // for strerror
#include <sys/ioctl.h>          // for ioctl
#include <fcntl.h>              // for open
#include <cassert>              // for assert
#include <unistd.h>             // for sleep & close file

// Function to check if a condition is met and print an error message if it is
void check_error(bool condition, const std::string &msg, bool exit_on_error = false) {
    if (condition) {
        std::cerr << msg << ": " << std::strerror(errno) << std::endl;
        if (exit_on_error)
            std::exit(1);
    }
}

enum OPCODE {
    WRITE,
    READ
};

// Function to get opcode value based on the OPCODE enum
__u8 get_opcode(OPCODE opcode) {
    check_error(opcode != WRITE && opcode != READ, "Wrong opcode Type");
    if (opcode == WRITE) return 0x01;
    return 0x02;
}

// Function to generate random data and fill a given buffer with it
void generate_random_data(char* buffer, size_t buffer_size) {
    for (size_t i = 0; i < buffer_size; ++i) {
        buffer[i] = rand() % 256;
    }
}

// This function is used to round up a given 'size' to the nearest multiple of 'alignment'.
// The 'alignment' should be a power of 2, which is checked by the assertion.
// The function works by adding 'alignment' - 1 to 'size' to ensure rounding up, 
// and then bitwise AND with the negation of 'alignment' - 1 to zero out the lower bits, 
// effectively rounding down to the nearest multiple of 'alignment'.
size_t get_aligned_size(size_t alignment, size_t size) {
    assert((alignment & (alignment - 1)) == 0);  // Ensure alignment is a power of 2
    return (size + alignment - 1) & ~(alignment - 1);
}