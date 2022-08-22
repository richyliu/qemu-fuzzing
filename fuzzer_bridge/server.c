#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/mman.h>

#define MAX_INPUT_SIZE 0x400

const char *shared_mem_file = "/dev/shm/snapshot_data";

struct coverage_t {
    uint64_t* pcs_array;
    size_t pcs_array_size;
    uint64_t* counters_array;
    size_t counters_array_size;
    uint64_t* trace_array;
    size_t trace_array_size;
};

struct coverage_t* receive_coverage(uint8_t* shared_mem) {
    struct coverage_t* coverage = malloc(sizeof(struct coverage_t));
    struct {
        uint64_t pcs_array_size;
        uint64_t counters_array_size;
        uint64_t trace_array_size;
    } coverage_info_header;

    // wait for header
    while (shared_mem[0] != 2);
    shared_mem[0] = -1;
    memcpy(&coverage_info_header, shared_mem + 8, sizeof(coverage_info_header));
    coverage->pcs_array_size = coverage_info_header.pcs_array_size;
    coverage->counters_array_size = coverage_info_header.counters_array_size;
    coverage->trace_array_size = coverage_info_header.trace_array_size;
    coverage->pcs_array = malloc(coverage->pcs_array_size * sizeof(uint64_t));
    coverage->counters_array = malloc(coverage->counters_array_size * sizeof(uint64_t));
    coverage->trace_array = malloc(coverage->trace_array_size * sizeof(uint64_t));

    // debug print
    printf("pcs_array_size: %zu\n", coverage->pcs_array_size);
    printf("counters_array_size: %zu\n", coverage->counters_array_size);
    printf("trace_array_size: %zu\n", coverage->trace_array_size);
    printf("\n");

    // receive pcs_array
    uint64_t remaining_bytes = coverage_info_header.pcs_array_size * 8;
    printf("remaining_bytes: %lu\n", remaining_bytes);
    int i = 0;
    while (remaining_bytes > 0) {
        puts("here");
        // wait for transfer
        while (((uint32_t*)shared_mem)[0] != i);
        if (remaining_bytes >= 0x800) {
            memcpy(coverage->pcs_array + i * 0x800 / 8, shared_mem + 0x800, 0x800);
            remaining_bytes -= 0x800;
        } else {
            memcpy(coverage->pcs_array + i * 0x800 / 8, shared_mem + 0x800, remaining_bytes);
            remaining_bytes = 0;
        }
        printf("%lx, %lx\n", coverage->pcs_array[0], coverage->pcs_array[0x100]);
        ((uint32_t*)shared_mem)[0] = -1;
        ((uint32_t*)shared_mem)[1] = i;
        i++;
    }


    for (int i = 0 ; i < coverage->pcs_array_size; i++) {
        printf("%3d: %lx\n", i, coverage->pcs_array[i]);
    }

    return NULL;
}

int main(int argc, char *argv[]) {
    int fd = -1;
    uint8_t* shared_mem = 0;
    size_t shared_mem_size = 0x1000;
    uint8_t data[MAX_INPUT_SIZE];
    size_t size = 0;

    fd = open(shared_mem_file, O_RDWR | O_SYNC);
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

    for (int i = 1; i < 2; i++) {
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
        struct coverage_t* coverage = receive_coverage(shared_mem);
        puts("Client done");
    }

    return 0;
}
