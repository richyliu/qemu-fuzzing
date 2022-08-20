#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdlib.h>

int main(void) {
    // communicate with PCI device for snapshotting
    int pci_fd = open("/sys/bus/pci/devices/0000:00:04.0/resource0", O_RDWR | O_SYNC);
    uint32_t* mem = mmap(NULL, 1024 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, pci_fd, 0);
    // release shared memory
    *mem = 0x202;
    puts("released\n");
    return 0;
}
