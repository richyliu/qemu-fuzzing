#include "coverage.h"

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

static struct coverage_t coverage_info;

void coverage_init(void) {
    coverage_info.pcs_array_start = 0;
    coverage_info.pcs_array_end = 0;
    coverage_info.counters_array_start = 0;
    coverage_info.counters_array_end = 0;
    coverage_info.trace_array_size = 0;
    coverage_info.trace_counter = 0;
}

struct coverage_t* get_coverage_info() {
    return &coverage_info;
}

static void print_pcs() {
#ifdef PRINT_VERBOSE
    printf("%p %p\n", coverage_info.pcs_array_start, coverage_info.pcs_array_end);
    uintptr_t *start = coverage_info.pcs_array_start;
    uintptr_t *end = coverage_info.pcs_array_end;
    for (uintptr_t *p = start; p < end; p++) {
        printf("%p: %lx\n", p, *p);
    }
    printf("\n");
#endif
}

// prints the 8bit counters
static void print_counters() {
#ifdef PRINT_VERBOSE
    printf("%p %p\n", coverage_info.counters_array_start, coverage_info.counters_array_end);
    uint8_t *start = (uint8_t *)coverage_info.counters_array_start;
    uint8_t *end = (uint8_t *)coverage_info.counters_array_end;
    for (uint8_t *p = start; p < end; p++) {
        printf("%p: %x\n", p, *p);
    }
    printf("\n");
#endif
}

// note: can be called multiple times with the same arguments
void __sanitizer_cov_pcs_init(const uintptr_t *pcs_beg, const uintptr_t *pcs_end) {
    printf("%s: %p, %p\n", __func__, pcs_beg, pcs_end);
    coverage_info.pcs_array_start = (uintptr_t)pcs_beg;
    coverage_info.pcs_array_end = (uintptr_t)pcs_end;
    print_pcs();
}

// note: can be called multiple times with the same arguments
void __sanitizer_cov_8bit_counters_init(char *start, char *end) {
    printf("%s: %p, %p\n", __func__, start, end);
    coverage_info.counters_array_start = (uintptr_t)start;
    coverage_info.counters_array_end = (uintptr_t)end;
    print_counters();
}

static void trace_add(uint64_t data) {
    if (coverage_info.trace_array_size == MAX_TRACE_ARR_SIZE) {
        printf("trace array full\n");
        exit(1);
    }
    coverage_info.trace_array[coverage_info.trace_array_size++] = data;

}

void __sanitizer_cov_trace_cmp1(uint8_t Arg1, uint8_t Arg2) {
#ifdef PRINT_TRACE_CMP
    printf("%s: %u, %u\n", __func__, Arg1, Arg2);
#endif
    coverage_info.trace_counter += 24;
}

void __sanitizer_cov_trace_cmp2(uint16_t Arg1, uint16_t Arg2) {
#ifdef PRINT_TRACE_CMP
    printf("%s: %u, %u\n", __func__, Arg1, Arg2);
#endif
    coverage_info.trace_counter += 24;
}

void __sanitizer_cov_trace_cmp4(uint32_t Arg1, uint32_t Arg2) {
#ifdef PRINT_TRACE_CMP
    printf("%s: %u, %u\n", __func__, Arg1, Arg2);
#endif
    coverage_info.trace_counter += 24;
    /* coverage_info.trace_counter += 9; */
}

void __sanitizer_cov_trace_cmp8(uint64_t Arg1, uint64_t Arg2) {
#ifdef PRINT_TRACE_CMP
    printf("%s: %llu, %llu\n", __func__, Arg1, Arg2);
#endif
    coverage_info.trace_counter += 24;
    /* coverage_info.trace_counter += 17; */
}

void __sanitizer_cov_trace_const_cmp1(uint8_t Arg1, uint8_t Arg2) {
#ifdef PRINT_TRACE_CMP
    printf("%s: %u, %u\n", __func__, Arg1, Arg2);
#endif
    coverage_info.trace_counter += 24;
}

void __sanitizer_cov_trace_const_cmp2(uint16_t Arg1, uint16_t Arg2) {
#ifdef PRINT_TRACE_CMP
    printf("%s: %u, %u\n", __func__, Arg1, Arg2);
#endif
    coverage_info.trace_counter += 24;
}

void __sanitizer_cov_trace_const_cmp4(uint32_t Arg1, uint32_t Arg2) {
#ifdef PRINT_TRACE_CMP
    printf("%s: %u, %u\n", __func__, Arg1, Arg2);
#endif
    coverage_info.trace_counter += 24;
    /* coverage_info.trace_counter += 9; */
}

void __sanitizer_cov_trace_const_cmp8(uint64_t Arg1, uint64_t Arg2) {
#ifdef PRINT_TRACE_CMP
    printf("%s: %llu, %llu\n", __func__, Arg1, Arg2);
#endif
    coverage_info.trace_counter += 24;
    /* coverage_info.trace_counter += 17; */
}

int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size);

void run_fuzzer_once(uint8_t* data, size_t size) {
    coverage_info.trace_counter = 0;
    LLVMFuzzerTestOneInput(data, size);
    printf("coverage_info.trace_counter: %zu\n", coverage_info.trace_counter);

    print_pcs();
    print_counters();
}
