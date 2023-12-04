#include <sys/mman.h>
#include <stdio.h>
#include <memory.h>

int main() {
    char *m;
    size_t s = 8UL * 1024 * 1024;

    m = mmap(NULL, s, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);

    if (m == MAP_FAILED) {
        perror("mmap");
        return -1;
    }

    memset(m, 0, s);
    printf("Press any key to exit\n");
    getchar();

    munmap(m, s);
    return 0;
}