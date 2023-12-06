#include <iostream>
#include <string>
#include <utils.h>
#include <config.h>
#include <allocator.h> // for allocate_buffer()
#include <huge_page.h> // for reserve_system_huge_page(), revert_system_huge_page()
#include <submit_handler.h> // for submit_tests()

std::string device_path;
int file_bytes;
int test_round;
int ret; // check return value is correct or not

int main(int argc, char** argv) {
    assert_msg(argc == 4, "argc != 4", true);
    device_path = argv[1];
    file_bytes = atoi(argv[2]);
    test_round = atoi(argv[3]);
    int aligned_file_bytes = get_aligned_file_bytes(file_bytes);
    printf("Test Configurations:\n");
    printf("--------------------\n");
    printf("async ioctl: %s\n", (ASYNC_IOCTL == 0) ? "sync" : "async");
    printf("memory allocation strategy: %s\n", (HUGE_PAGE_STRATEGY == 0) ? "posix_memalign" : "huge page");
    if (HUGE_PAGE_STRATEGY == 1) {
        printf("huge page size: %d\n", HUGE_PAGE_SIZE);
    }
    printf("chunk size: %d\n", CHUNK_SIZE);
    printf("nvme sector size: %d\n", NVME_SECTOR_SIZE);
    printf("device path: %s\nfile bytes: %d\ntest round: %d\n", device_path.c_str(), file_bytes, test_round);
    printf("aligned file bytes: %d\n", aligned_file_bytes);
    printf("--------------------\n");

#if HUGE_PAGE_STRATEGY
    // reserve huge page for read and write
    int total_reserve_bytes = 2 * aligned_file_bytes;
    reserve_system_huge_page(total_reserve_bytes);
#endif

    // submit tests
    ret = submit_tests(device_path, file_bytes, aligned_file_bytes, test_round);
    assert_msg(ret == 0, "submit_tests failed", true);

#if HUGE_PAGE_STRATEGY
    revert_system_huge_page();
#endif
    return 0;
}