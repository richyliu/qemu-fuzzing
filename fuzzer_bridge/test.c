#include <stdio.h>
#include <stdint.h>

int fib(int n) {
    printf("fib(%d)\n", n);
    if (n <= 1) {
        return n;
    }
    return fib(n - 1) + fib(n - 2);
}

void my_main(const uint8_t* data, size_t len) {
    // find the maximum value in the array
    uint8_t max = 0;
    for (size_t i = 0; i < len; i++) {
        if (data[i] > max) {
            max = data[i];
        }
    }

    // crash if there are consecutive max values
    for (size_t i = 0; i < len; i++) {
        if (data[i] == fib(i)) {
            continue;
        }
        if (i > 0 && data[i] == max && data[i-1] == max) {
            *(volatile int*)0 = 0;
        }
    }
}

int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
    my_main(Data, Size);
    return 0;
}
