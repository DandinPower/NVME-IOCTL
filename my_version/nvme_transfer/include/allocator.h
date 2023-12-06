#pragma once
#include <config.h>
#include <utils.h>
#include <sys/mman.h>
#include <stdio.h>
#include <memory.h>

/**
 * @brief Calculates the aligned size for a given test file size.
 *
 * This function aligns the size of a single test file to either the HUGE_PAGE_SIZE or the NVME_SECTOR_SIZE,
 * depending on the HUGE_PAGE_STRATEGY flag.
 *
 * @param test_file_bytes The size of a single test file.
 * @return The aligned size of the test file.
 */
int get_aligned_test_file_bytes(int test_file_bytes) {
#if HUGE_PAGE_STRATEGY
    // the reason need to aligned nvme sector is too solve the bug that happen when sector size is larger than HUGE_PAGE_SIZE
    test_file_bytes = get_aligned_size(NVME_SECTOR_SIZE, test_file_bytes);
    test_file_bytes = get_aligned_size(HUGE_PAGE_SIZE, test_file_bytes);
#else
    test_file_bytes = get_aligned_size(NVME_SECTOR_SIZE, test_file_bytes);
#endif
    return test_file_bytes;
}

/**
 * @brief Allocates a buffer of a given size.
 *
 * This function allocates a buffer of a given size, aligning the size based on the 
 * HUGE_PAGE_STRATEGY. If HUGE_PAGE_STRATEGY is enabled, it uses mmap with MAP_HUGETLB 
 * to allocate huge pages. If it's not enabled, it uses posix_memalign to align the 
 * buffer to NVME_SECTOR_SIZE.
 *
 * @param size The size of the buffer to allocate.
 * @param buffer A pointer to a pointer, where the address of the allocated buffer will be stored.
 * @return 0 if the allocation was successful, -1 otherwise.
 */
int allocate_buffer(int size, void** buffer) {
#if HUGE_PAGE_STRATEGY
    assert_msg(size % HUGE_PAGE_SIZE == 0, "size % HUGE_PAGE_SIZE != 0", true);
    *buffer = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
    if (*buffer == MAP_FAILED) {
        printf("mmap failed\n");
        return -1;
    }
#else
    assert_msg(size % NVME_SECTOR_SIZE == 0, "size % NVME_SECTOR_SIZE != 0", true);
    if (posix_memalign(buffer, NVME_SECTOR_SIZE, size) != 0) {
        printf("posix_memalign failed\n");
        return -1;
    }
#endif
    return 0;
}

/**
 * @brief Frees a buffer of a given size.
 *
 * This function frees a buffer of a given size, taking into account the HUGE_PAGE_STRATEGY. 
 * If HUGE_PAGE_STRATEGY is enabled, it uses munmap to free the huge pages. If it's not enabled, 
 * it simply uses free.
 *
 * @param buffer The buffer to free.
 * @param size The size of the buffer.
 */
void free_buffer(void* buffer, int size) {
#if HUGE_PAGE_STRATEGY
    assert_msg(size % HUGE_PAGE_SIZE == 0, "size % HUGE_PAGE_SIZE != 0", true);
    munmap(buffer, size);
#else
    free(buffer);
#endif
}

/**
 * @brief Initializes a buffer with random data.
 *
 * This function initializes a buffer with random data. The size of the data generated 
 * is equal to the size of the buffer.
 *
 * @param buffer The buffer to initialize.
 * @param size The size of the buffer.
 */
void init_random_buffer(int size, char* buffer) {
#if HUGE_PAGE_STRATEGY
    assert_msg(size % HUGE_PAGE_SIZE == 0, "size % HUGE_PAGE_SIZE != 0", true);
#else 
    assert_msg(size % NVME_SECTOR_SIZE == 0, "size % NVME_SECTOR_SIZE != 0", true);
#endif
    generate_random_data(buffer, size);
}

/**
 * @brief Initializes a buffer with zeros.
 *
 * This function initializes a buffer with zeros. The size of the buffer is equal to 
 * the size of the buffer.
 *
 * @param buffer The buffer to initialize.
 * @param size The size of the buffer.
 */
void init_empty_buffer(int size, char* buffer) {
#if HUGE_PAGE_STRATEGY
    assert_msg(size % HUGE_PAGE_SIZE == 0, "size % HUGE_PAGE_SIZE != 0", true);
#else 
    assert_msg(size % NVME_SECTOR_SIZE == 0, "size % NVME_SECTOR_SIZE != 0", true);
#endif
    memset(buffer, 0, size);
}