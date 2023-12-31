#pragma once
#define NVME_SECTOR_SIZE 512
#define CHUNK_SIZE 4096 * NVME_SECTOR_SIZE
#define HUGE_PAGE_STRATEGY 1    // 0: no huge page using posix_memalign, 1: using huge page
#define HUGE_PAGE_SIZE 2097152  // 2MB
#define ASYNC_IOCTL 1           // 0: sync io, 1: async io
#define START_LBA 0             // the start LBA for the test