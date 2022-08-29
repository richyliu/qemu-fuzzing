#pragma once

#include <stdint.h>
#include <stddef.h>

/* #define PRINT_VERBOSE */
/* #define PRINT_TRACE_CMP */

struct coverage_t {
    uintptr_t pcs_array_start;
    uintptr_t pcs_array_end;

    uintptr_t counters_array_start;
    uintptr_t counters_array_end;
};

void coverage_init();
struct coverage_t* get_coverage_info();

/* Must be defined externally */
extern int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size);

/* Coverage functions we are overriding */
void __sanitizer_cov_pcs_init(const uintptr_t *pcs_beg, const uintptr_t *pcs_end);
void __sanitizer_cov_8bit_counters_init(char *start, char *end);
void __sanitizer_cov_trace_cmp1(uint8_t Arg1, uint8_t Arg2);
void __sanitizer_cov_trace_cmp2(uint16_t Arg1, uint16_t Arg2);
void __sanitizer_cov_trace_cmp4(uint32_t Arg1, uint32_t Arg2);
void __sanitizer_cov_trace_cmp8(uint64_t Arg1, uint64_t Arg2);
void __sanitizer_cov_trace_const_cmp1(uint8_t Arg1, uint8_t Arg2);
void __sanitizer_cov_trace_const_cmp2(uint16_t Arg1, uint16_t Arg2);
void __sanitizer_cov_trace_const_cmp4(uint32_t Arg1, uint32_t Arg2);
void __sanitizer_cov_trace_const_cmp8(uint64_t Arg1, uint64_t Arg2);

/* Use this function to call fuzzer with input */
void run_fuzzer_once(uint8_t* data, size_t size);
