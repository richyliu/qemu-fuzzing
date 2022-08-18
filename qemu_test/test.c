#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

int main() {
    int fd = open("/sys/bus/pci/devices/0000:00:04.0/resource0", O_RDWR | O_SYNC);
    uint32_t* memory = mmap(NULL, 1024 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    printf("%x\n", memory[0]);

    int a = 0;
    memory[0] = 0x101; // save snapshot
    printf("before: value of a = %d\n", a);
    a = 1;
    printf("middle: value of a = %d\n", a);
    memory[0] = 0x102; // load snapshot
    printf("after: value of a = %d\n", a);

    return 0;
}
