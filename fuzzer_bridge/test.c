#include <stdio.h>
#include <stdint.h>

#include <cJSON.h>

#include "testlib.h"

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
    puts("before cjson code");
    char string[] = "{\n\
        \"foo\": \"bar\",\n\
        \"bar\": \"baz\",\n\
        \"baz\": {\n\
            \"qux\": [\n\
                \"quux\",\n\
                \"corge\"\n\
            ]\n\
        }\n\
    }";
    cJSON *json = cJSON_Parse(string);
    puts("after cjson code");
    /* my_main(Data, Size); */
    return 0;
}
