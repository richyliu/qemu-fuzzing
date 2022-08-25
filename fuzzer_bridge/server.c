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
    coverage->pcs_array = pcs_array;
    coverage->counters_array = counters_array;
    coverage->trace_array = malloc(coverage->trace_array_size * sizeof(uint64_t));

    // debug print
//    printf("pcs_array_size: %zu\n", coverage->pcs_array_size);
//    printf("counters_array_size: %zu\n", coverage->counters_array_size);
//    printf("trace_array_size: %zu\n", coverage->trace_array_size);
//    printf("\n");

    // receive pcs_array
    uint64_t remaining_bytes = coverage_info_header.pcs_array_size * 8;
//    printf("remaining_bytes: %lu\n", remaining_bytes);
    int i = 0;
    while (remaining_bytes > 0) {
//        puts("here");
        // wait for transfer
        while (((uint32_t*)shared_mem)[0] != i);
        if (remaining_bytes >= 0x800) {
            memcpy(coverage->pcs_array + i * 0x800 / 8, shared_mem + 0x800, 0x800);
            remaining_bytes -= 0x800;
        } else {
            memcpy(coverage->pcs_array + i * 0x800 / 8, shared_mem + 0x800, remaining_bytes);
            remaining_bytes = 0;
        }
//        printf("%lx, %lx\n", coverage->pcs_array[0], coverage->pcs_array[0x100]);
        ((uint32_t*)shared_mem)[0] = -1;
        ((uint32_t*)shared_mem)[1] = i;
        i++;
    }
    for (int i = 0 ; i < coverage->pcs_array_size; i++) {
//        printf("%3d: %lx\n", i, coverage->pcs_array[i]);
    }

//    printf("counters\n");
    // receive counters_array
    remaining_bytes = coverage_info_header.counters_array_size;
//    printf("remaining_bytes: %lu\n", remaining_bytes);
    i = 0;
    while (remaining_bytes > 0) {
//        puts("here");
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
    for (int i = 0 ; i < coverage->counters_array_size; i++) {
//        printf("%3d: %02x\n", i, coverage->counters_array[i]);
    }

    // done with everything
    while(shared_mem[0] != 7); // synchronize
    shared_mem[0] = 8;

    return NULL;
}

int run_fuzzer(const uint8_t *data, size_t size) {
    uint8_t* shared_mem = shared_mem_global;
    static int iterations = 0;
    static struct timeval start = {0};

    if (iterations == 0) {
        gettimeofday(&start, NULL);
    }

    iterations++;

//    printf("=====================\n");

    // truncate to 0x10 bytes
    if (size > 0x10) {
        size = 0x10;
    }

    // send inputs
//    puts("sending inputs...");
    shared_mem[0] = 1;
    *(uint32_t*)(shared_mem + 4) = (uint32_t)size;
    memcpy(shared_mem + 8, data, size);
    /* msync(shared_mem, shared_mem_size, MS_SYNC); */
//    puts("sent inputs");

    // wait for client to be done and receive coverage
//    puts("Waiting for client to be done...");
    struct coverage_t* coverage = receive_coverage(shared_mem);
//    puts("Client done");

    if (iterations % 1 == 0) {
        struct timeval t;
        gettimeofday(&t, NULL);
        double elapsed = (t.tv_sec - start.tv_sec) + (t.tv_usec - start.tv_usec) / 1000000.0;
        printf("iter: %3d, elapsed: %f", iterations, elapsed);

        printf(", input: ");
        for (int i = 0 ; i < size; i++) {
            printf("%02x ", data[i]);
        }
        printf("\n");
    }

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

    // get coverage and trace array size
    while (shared_mem[0] != 5);
    pcs_array_size = *(uint64_t*)(shared_mem + 0x8);
    counters_array_size = *(uint64_t*)(shared_mem + 0x10);
//    printf("pcs_array_size: %lu\n", pcs_array_size);
//    printf("counters_array_size: %lu\n", counters_array_size);
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
    /* for (int i = 1; i < 5; i++) { */
    /*     // update input */
    /*     data[2] = i; */
    /*     run_fuzzer(data, size); */
    /* } */

    return 0;
}
