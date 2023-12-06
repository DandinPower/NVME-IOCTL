#pragma once
#include <cstring>      // for strerror
#include <stdlib.h>
#include <iostream>

/**
 * @brief Asserts that a given condition is true, and prints a message if it is not. if exit_on_error is true, exit the program.
 *
 * @param condition The condition to check.
 * @param msg The message to print if the condition is false.
 */
void assert_msg(bool condition, const std::string &msg, bool exit_on_error = false) {
    if (!condition) {
        std::cerr << msg << ": " << std::strerror(errno) << std::endl;
        if (exit_on_error)
            std::exit(1);
    }
}

/**
 * @brief Rounds up a given 'size' to the nearest multiple of 'alignment'.
 *
 * The 'alignment' should be a power of 2, which is checked by the assertion.
 * The function works by adding 'alignment' - 1 to 'size' to ensure rounding up, 
 * and then bitwise AND with the negation of 'alignment' - 1 to zero out the lower bits, 
 * effectively rounding down to the nearest multiple of 'alignment'.
 *
 * @param alignment The alignment size, should be a power of 2.
 * @param size The original size to be aligned.
 * @return The size rounded up to the nearest multiple of 'alignment'.
 */
int get_aligned_size(int alignment, int size) {
    assert_msg((alignment & (alignment - 1)) == 0, "alignment is not power of 2", true);
    return (size + alignment - 1) & ~(alignment - 1);
}

/**
 * @brief Pauses the program to allow for checking of huge page reservation.
 */
void pause() {
    std::cout << "Press any enter to continue..." << std::endl;
    std::cin.get();
}

/**
 * @brief Generates random data and fills a given buffer with it.
 *
 * This function uses a random number generator to generate data, and then fills 
 * the provided buffer with this data. The size of the data generated is equal to 
 * the size of the buffer.
 *
 * @param buffer The buffer to fill with random data.
 * @param size The size of the buffer.
 */
void generate_random_data(char* buffer, int buffer_size) {
    for (int i = 0; i < buffer_size; ++i) {
        buffer[i] = rand() % 256;
    }
}