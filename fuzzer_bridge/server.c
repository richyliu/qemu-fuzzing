#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/time.h>

extern void __sanitizer_cov_pcs_init(const uintptr_t *pcs_beg, const uintptr_t *pcs_end);
extern void __sanitizer_cov_8bit_counters_init(char *start, char *end);
extern int LLVMFuzzerRunDriver(int *argc, char ***argv,
                               int (*UserCb)(const uint8_t *Data, size_t Size));

#define MAX_INPUT_SIZE 0x400

const char *shared_mem_file = "/dev/shm/snapshot_data";

uint64_t *pcs_array = 0;
uint8_t *counters_array = 0;
uint8_t* shared_mem_global = 0;

struct coverage_t {
    uint64_t* pcs_array;
    size_t pcs_array_size;
    uint8_t* counters_array;
    size_t counters_array_size;
};

struct coverage_t* receive_coverage(uint8_t* shared_mem) {
    struct coverage_t* coverage = malloc(sizeof(struct coverage_t));
    struct {
        uint64_t pcs_array_size;
        uint64_t counters_array_size;
    } coverage_info_header;

    // wait for header
    while (shared_mem[0] != 2);
    shared_mem[0] = -1;
    memcpy(&coverage_info_header, shared_mem + 8, sizeof(coverage_info_header));
    coverage->pcs_array_size = coverage_info_header.pcs_array_size;
    coverage->counters_array_size = coverage_info_header.counters_array_size;
    coverage->pcs_array = pcs_array;
    coverage->counters_array = counters_array;

    // receive pcs_array
    uint64_t remaining_bytes = coverage_info_header.pcs_array_size * 8;
    int i = 0;
    while (remaining_bytes > 0) {
        // wait for transfer
        while (((uint32_t*)shared_mem)[0] != i);
        if (remaining_bytes >= 0x800) {
            memcpy(coverage->pcs_array + i * 0x800 / 8, shared_mem + 0x800, 0x800);
            remaining_bytes -= 0x800;
        } else {
            memcpy(coverage->pcs_array + i * 0x800 / 8, shared_mem + 0x800, remaining_bytes);
            remaining_bytes = 0;
        }
        ((uint32_t*)shared_mem)[0] = -1;
        ((uint32_t*)shared_mem)[1] = i;
        i++;
    }

    // receive counters_array
    remaining_bytes = coverage_info_header.counters_array_size;
    i = 0;
    while (remaining_bytes > 0) {
        // wait for transfer
        while (((uint32_t*)shared_mem)[0] != i);
        if (remaining_bytes >= 0x800) {
            memcpy(coverage->counters_array + i * 0x800, shared_mem + 0x800, 0x800);
            remaining_bytes -= 0x800;
        } else {
            memcpy(coverage->counters_array + i * 0x800, shared_mem + 0x800, remaining_bytes);
            remaining_bytes = 0;
        }
        ((uint32_t*)shared_mem)[0] = -1;
        ((uint32_t*)shared_mem)[1] = i;
        i++;
    }

    // done with everything
    while(shared_mem[0] != 7); // synchronize
    shared_mem[0] = 8;

    return NULL;
}

int run_fuzzer(const uint8_t *data, size_t size) {
    uint8_t* shared_mem = shared_mem_global;

    // log time and inputs for debugging
#ifndef DISABLE_LOGGING
    struct timeval tv;
    static int counter = 0;
    static double prev = 0;
    static double avg = 0; // exponential rolling average
    double cur;
    if (prev == 0) {
        gettimeofday(&tv, NULL);
        prev = tv.tv_sec + tv.tv_usec / 1000000.0;
    }

    // log current time
    counter++;
    gettimeofday(&tv, NULL);
    cur = tv.tv_sec + tv.tv_usec / 1000000.0;
    avg = (cur - prev) * 0.1 + 0.9 * avg;
    fprintf(stderr, "counter: %5d, average iters/sec: %f", counter, 1.0 / avg);
    prev = cur;

    printf(", input: ");
    for (int i = 0 ; i < size; i++) {
        printf("%02x ", data[i]);
    }
    printf("\n");
#endif

    if (size > MAX_INPUT_SIZE) {
        size = MAX_INPUT_SIZE;
    }

    // send inputs
    *(uint32_t*)(shared_mem + 4) = (uint32_t)size;
    memcpy(shared_mem + 8, data, size);
    shared_mem[0] = 1;

    // wait for client to be done and receive coverage
    struct coverage_t* coverage = receive_coverage(shared_mem);

    return 0;
}

int main(int argc, char *argv[]) {
    int fd = -1;
    uint8_t* shared_mem = 0;
    size_t shared_mem_size = 0x1000;
    uint8_t data[MAX_INPUT_SIZE];
    size_t size = 0;
    uint64_t pcs_array_size = 0;
    uint64_t counters_array_size = 0;

    fd = open(shared_mem_file, O_RDWR | O_SYNC);
    if (fd < 0) {
        perror("shared memory file open");
        exit(1);
    }
    shared_mem = mmap(NULL, shared_mem_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    shared_mem_global = shared_mem;
    close(fd);

    memset(data, 0, sizeof(data));
    size = 0;

    data[size++] = 0;
    data[size++] = 0;
    data[size++] = 1;

    // get coverage and 8bit counters array size
    while (shared_mem[0] != 5);
    pcs_array_size = *(uint64_t*)(shared_mem + 0x8);
    counters_array_size = *(uint64_t*)(shared_mem + 0x10);
    pcs_array = malloc(pcs_array_size * sizeof(uint64_t));
    counters_array = malloc(counters_array_size * sizeof(uint64_t));
    shared_mem[0] = 2;

    // init coverage with libfuzzer
    uintptr_t* pcs_beg = (uintptr_t*)pcs_array;
    uintptr_t* pcs_end = (uintptr_t*)(pcs_array + pcs_array_size);
    char* counters_beg = (char*)counters_array;
    char* counters_end = (char*)(counters_array + counters_array_size);
    __sanitizer_cov_pcs_init(pcs_beg, pcs_end);
    __sanitizer_cov_8bit_counters_init(counters_beg, counters_end);

    LLVMFuzzerRunDriver(&argc, &argv, &run_fuzzer);

    return 0;
}
