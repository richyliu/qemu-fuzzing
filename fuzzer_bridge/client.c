#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <time.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "serial.h"

#define SERIAL_PORT "/dev/ttyS4"

#define MAX_INPUT_SIZE 0x400

/* #define PRINT_VERBOSE */
/* #define PRINT_TRACE_CMP */

struct ptr_pair {
    void *start;
    void *end;
};

struct val_pair {
    uint64_t val1;
    uint64_t val2;
};

#define MAX_PCS_ARRAY_SIZE 1024
#define MAX_COUNTER_ARRAY_SIZE 1024

static struct ptr_pair pcs_array[MAX_PCS_ARRAY_SIZE];
static size_t pcs_array_idx = 0;

static struct ptr_pair counters_array[MAX_COUNTER_ARRAY_SIZE];
static size_t counters_array_idx = 0;

static size_t trace_counter = 0;

void print_pcs() {
#ifdef PRINT_VERBOSE
    for (size_t i = 0; i < pcs_array_idx; i++) {
        printf("%p %p\n", pcs_array[i].start, pcs_array[i].end);
        uintptr_t *start = (uintptr_t *)pcs_array[i].start;
        uintptr_t *end = (uintptr_t *)pcs_array[i].end;
        for (uintptr_t *p = start; p < end; p++) {
            printf("%p: %lx\n", p, *p);
        }
        printf("\n");
    }
#endif
}

// prints the 8bit counters
void print_counters() {
#ifdef PRINT_VERBOSE
    for (size_t i = 0; i < counters_array_idx; i++) {
        printf("%p %p\n", counters_array[i].start, counters_array[i].end);
        uint8_t *start = (uint8_t *)counters_array[i].start;
        uint8_t *end = (uint8_t *)counters_array[i].end;
        for (uint8_t *p = start; p < end; p++) {
            printf("%p: %x\n", p, *p);
        }
        printf("\n");
    }
#endif
}

// note: can be called multiple times with the same arguments
void __sanitizer_cov_pcs_init(const uintptr_t *pcs_beg, const uintptr_t *pcs_end) {
    // check if we have already seen this range
    for (size_t i = 0; i < pcs_array_idx; i++) {
        if (pcs_array[i].start == pcs_beg && pcs_array[i].end == pcs_end) {
            return;
        }
    }
    // new range, add it to the array
    printf("%s: %p, %p\n", __func__, pcs_beg, pcs_end);
    assert(pcs_array_idx < MAX_PCS_ARRAY_SIZE);
    pcs_array[pcs_array_idx++] = (struct ptr_pair){(void*)pcs_beg, (void*)pcs_end};
    print_pcs();
}

// note: can be called multiple times with the same arguments
void __sanitizer_cov_8bit_counters_init(char *start, char *end) {
    // check if we have already seen this range
    for (size_t i = 0; i < counters_array_idx; i++) {
        if (counters_array[i].start == start && counters_array[i].end == end) {
            return;
        }
    }
    // new range, add it to the array
    printf("%s: %p, %p\n", __func__, start, end);
    assert(counters_array_idx < MAX_COUNTER_ARRAY_SIZE);
    counters_array[counters_array_idx++] = (struct ptr_pair){start, end};
    print_counters();
}

void __sanitizer_cov_trace_cmp1(uint8_t Arg1, uint8_t Arg2) {
#ifdef PRINT_TRACE_CMP
    printf("%s: %u, %u\n", __func__, Arg1, Arg2);
#endif
    trace_counter += 16;
}

void __sanitizer_cov_trace_cmp2(uint16_t Arg1, uint16_t Arg2) {
#ifdef PRINT_TRACE_CMP
    printf("%s: %u, %u\n", __func__, Arg1, Arg2);
#endif
    trace_counter += 16;
}

void __sanitizer_cov_trace_cmp4(uint32_t Arg1, uint32_t Arg2) {
#ifdef PRINT_TRACE_CMP
    printf("%s: %u, %u\n", __func__, Arg1, Arg2);
#endif
    trace_counter += 16;
    /* trace_counter += 9; */
}

void __sanitizer_cov_trace_cmp8(uint64_t Arg1, uint64_t Arg2) {
#ifdef PRINT_TRACE_CMP
    printf("%s: %llu, %llu\n", __func__, Arg1, Arg2);
#endif
    trace_counter += 24;
    /* trace_counter += 17; */
}

void __sanitizer_cov_trace_const_cmp1(uint8_t Arg1, uint8_t Arg2) {
#ifdef PRINT_TRACE_CMP
    printf("%s: %u, %u\n", __func__, Arg1, Arg2);
#endif
    trace_counter += 16;
}

void __sanitizer_cov_trace_const_cmp2(uint16_t Arg1, uint16_t Arg2) {
#ifdef PRINT_TRACE_CMP
    printf("%s: %u, %u\n", __func__, Arg1, Arg2);
#endif
    trace_counter += 16;
}

void __sanitizer_cov_trace_const_cmp4(uint32_t Arg1, uint32_t Arg2) {
#ifdef PRINT_TRACE_CMP
    printf("%s: %u, %u\n", __func__, Arg1, Arg2);
#endif
    trace_counter += 16;
    /* trace_counter += 9; */
}

void __sanitizer_cov_trace_const_cmp8(uint64_t Arg1, uint64_t Arg2) {
#ifdef PRINT_TRACE_CMP
    printf("%s: %llu, %llu\n", __func__, Arg1, Arg2);
#endif
    trace_counter += 24;
    /* trace_counter += 17; */
}

int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size);

void run_fuzzer_once(uint8_t* data, size_t size) {
    trace_counter = 0;
    LLVMFuzzerTestOneInput(data, size);
    printf("trace_counter: %zu\n", trace_counter);

    print_pcs();
    print_counters();
}

int main() {
    // communicate with PCI device for snapshotting
    int pci_fd = open("/sys/bus/pci/devices/0000:00:04.0/resource0", O_RDWR | O_SYNC);
    uint32_t* pci_memory = mmap(NULL, 1024 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, pci_fd, 0);
    if (pci_memory == MAP_FAILED) {
        perror("mmap");
        return 1;
    }

    int fd = open_serial_port(SERIAL_PORT, 115200);
    if (fd < 0) {
        perror("open_serial_port");
        return 1;
    }

    uint8_t data[MAX_INPUT_SIZE];
    memset(data, 0, sizeof(data));

    size_t size = 0;

    // do initialization work here...
    puts("initializing...");

    // initialization done
    puts("initialization done");

    uint8_t init_done = 'I';
    write_port(fd, &init_done, sizeof(init_done));
    puts("sent init_done");
    puts("ready for snapshotting");

    // save a snapshot
    pci_memory[0] = 0x101;
    puts("in snapshotted code");

    // wait for next input
    puts("waiting for input...");
    uint8_t input_ready = 0;
    while (input_ready != 'R') {
        if (read_port(fd, &input_ready, sizeof(input_ready)) > 0) {
            printf("got: %02x\n", input_ready);
        }
    }

    // read input data
    puts("reading input...");
    read_port(fd, (uint8_t*)&size, sizeof(size));
    if (size > MAX_INPUT_SIZE) {
        puts("input too large");
        return 1;
    }
    read_port(fd, data, size);

    printf("running with input: size: %zu\n", size);
    for (size_t i = 0; i < size; i++) {
        printf("%02x ", data[i]);
    }
    printf("\n");

    run_fuzzer_once(data, size);

    puts("writing output...");
    char res[] = "ok\n";
    write_port(fd, (uint8_t*)res, sizeof(res));
    puts("done");

    puts("restoring...");
    // restore snapshot
    pci_memory[0] = 0x102;

    puts("ERROR: should not reach here");
    return 1;
}
