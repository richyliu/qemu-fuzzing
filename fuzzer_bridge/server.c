#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/mman.h>

#define MAX_INPUT_SIZE 0x400

const char *shared_mem_file = "/dev/shm/snapshot_data";

int main(int argc, char *argv[]) {
    int fd = -1;
    uint8_t* shared_mem = 0;
    size_t shared_mem_size = 0x1000;
    uint8_t data[MAX_INPUT_SIZE];
    size_t size = 0;

    fd = open(shared_mem_file, O_RDWR);
    if (fd < 0) {
        perror("shared memory file open");
        exit(1);
    }
    shared_mem = mmap(NULL, shared_mem_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);

    memset(data, 0, sizeof(data));
    size = 0;

    data[size++] = 0;
    data[size++] = 0;
    data[size++] = 1;

    for (int i = 1; i < 5; i++) {
        printf("=====================\n");
        printf("Iteration %d\n", i);

        // update input
        data[2] = i;

        // send inputs
        puts("sending inputs...");
        shared_mem[0] = 1;
        *(uint32_t*)(shared_mem + 4) = (uint32_t)size;
        memcpy(shared_mem + 8, data, size);
        msync(shared_mem, shared_mem_size, MS_SYNC);
        puts("sent inputs");

        // wait for client to be done and receive coverage
        puts("Waiting for client to be done...");
        while (shared_mem[0] != 2);
        size_t trace_counter = 0;
        trace_counter = *(uint64_t*)(shared_mem + 8);
        puts("Client done");
        printf("trace_counter: %zu\n", trace_counter);
    }

    return 0;
}
