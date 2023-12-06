#include <iostream>
#include <string>
#include <utils.h>
#include <config.h>
#include <allocator.h> // for allocate_buffer()
#include <huge_page.h> // for reserve_system_huge_page(), revert_system_huge_page()
#include <submit_handler.h> // for submit_tests()

std::string device_path;
int test_file_bytes;
int test_file_number;
char* writeBuffer, *readBuffer; // huge buffer for all test files
int ret; // check return value is correct or not

int main(int argc, char** argv) {
    assert_msg(argc == 4, "argc != 4", true);
    device_path = argv[1];
    test_file_bytes = atoi(argv[2]);
    test_file_number = atoi(argv[3]);
    int aligned_test_file_bytes = get_aligned_test_file_bytes(test_file_bytes);
    size_t aligned_total_bytes = aligned_test_file_bytes * test_file_number;
    printf("Test Configurations:\n");
    printf("--------------------\n");
    printf("io strategy: %s\n", (IOCTL_STRATEGY == 0) ? "libaio" : "ioctl");
    if (IOCTL_STRATEGY == 1) {
        printf("async ioctl: %s\n", (ASYNC_IOCTL == 0) ? "sync" : "async");
    }
    printf("memory allocation strategy: %s\n", (HUGE_PAGE_STRATEGY == 0) ? "posix_memalign" : "huge page");
    if (HUGE_PAGE_STRATEGY == 1) {
        printf("huge page size: %d\n", HUGE_PAGE_SIZE);
    }
    printf("chunk size: %d\n", CHUNK_SIZE);
    printf("nvme sector size: %d\n", NVME_SECTOR_SIZE);
    printf("device path: %s\ntest file bytes: %d\ntest file number: %d\n", device_path.c_str(), test_file_bytes, test_file_number);
    printf("aligned test file bytes: %d\n", aligned_test_file_bytes);
    printf("aligned total bytes: %lld\n", aligned_total_bytes);
    printf("--------------------\n");

#if HUGE_PAGE_STRATEGY
    // reserve huge page for read and write
    int total_reserve_bytes = 2 * aligned_total_bytes;
    reserve_system_huge_page(total_reserve_bytes);
#endif

    // allocate huge buffer for read and write
    ret = allocate_buffer(aligned_total_bytes, (void**)&writeBuffer);
    assert_msg(ret == 0, "allocate_buffer failed", true);
    ret = allocate_buffer(aligned_total_bytes, (void **)&readBuffer);
    assert_msg(ret == 0, "allocate_buffer failed", true);

    // init buffer
    init_random_buffer(aligned_total_bytes, writeBuffer);
    init_empty_buffer(aligned_total_bytes, readBuffer);

    // submit tests
    ret = submit_tests(device_path, writeBuffer, readBuffer, test_file_bytes, aligned_test_file_bytes, test_file_number);
    assert_msg(ret == 0, "submit_tests failed", true);

    // free buffer
    free_buffer(writeBuffer, aligned_total_bytes);
    free_buffer(readBuffer, aligned_total_bytes);

#if HUGE_PAGE_STRATEGY
    revert_system_huge_page();
#endif
    return 0;
}