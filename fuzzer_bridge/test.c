#include <stdio.h>
#include <stdint.h>

void my_main(const uint8_t* data, size_t len) {
    // simple fuzz target that crashes on specific input conditions
    int a = 1;
    if (len > 0 && data[0] < 64 && data[0] > 0) {
        if (len > 1 && data[1] < 10 && data[1] > 0) {
            if (data[0] % 7 == 0)
                a = 0;
        }
    }

    if (a == 0) {
        puts("now crashing...");
        printf("inputs: ");
        for (int i = 0; i < len; i++) {
            printf("%02x ", data[i]);
        }
        printf("\n");
        // cause a crash
        *(volatile int*)0 = 0;
    }
}

int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
    my_main(Data, Size);
    return 0;
}
