#include "testlib.h"

#include <stdio.h>

int fib(int n) {
    printf("fib(%d)\n", n);
    if (n <= 1) {
        return n;
    }
    return fib(n - 1) + fib(n - 2);
}
