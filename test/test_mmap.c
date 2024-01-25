#include <stdio.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <numa.h>
#include <numaif.h>
#include <stdlib.h>

int main() {
    void *ptr = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS,
                -1, 0);
    if (!ptr) {
        fprintf(stderr, "mmap failed\n");
        perror("mmap");
        abort();
    }

    struct bitmask* allowed_nodes = numa_get_mems_allowed();
    int ret = mbind(ptr, 4096, MPOL_BIND, allowed_nodes->maskp, allowed_nodes->size, 0);
    if (ret) {
        perror("mbind");
        abort();
    }

    return 0;
}