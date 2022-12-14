#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <time.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <unistd.h>

#include "coverage.h"

#define MAX_INPUT_SIZE 0x400
#define PAGE_SIZE 0x1000

typedef struct {
    uint64_t pfn : 55;
    unsigned int soft_dirty : 1;
    unsigned int file_page : 1;
    unsigned int swapped : 1;
    unsigned int present : 1;
} PagemapEntry;

/* Parse the pagemap entry for the given virtual address.
 *
 * @param[out] entry      the parsed entry
 * @param[in]  pagemap_fd file descriptor to an open /proc/pid/pagemap file
 * @param[in]  vaddr      virtual address to get entry for
 * @return 0 for success, 1 for failure
 */
static int pagemap_get_entry(PagemapEntry *entry, int pagemap_fd, uintptr_t vaddr)
{
    size_t nread;
    ssize_t ret;
    uint64_t data;
    uintptr_t vpn;

    vpn = vaddr / PAGE_SIZE;
    nread = 0;
    while (nread < sizeof(data)) {
        ret = pread(pagemap_fd, ((uint8_t*)&data) + nread, sizeof(data) - nread,
                vpn * sizeof(data) + nread);
        nread += ret;
        if (ret <= 0) {
            return 1;
        }
    }
    entry->pfn = data & (((uint64_t)1 << 55) - 1);
    entry->soft_dirty = (data >> 55) & 1;
    entry->file_page = (data >> 61) & 1;
    entry->swapped = (data >> 62) & 1;
    entry->present = (data >> 63) & 1;
    return 0;
}

/* Convert the given virtual address to physical using /proc/self/pagemap.
 */
static uintptr_t virt_to_phys_user(uintptr_t vaddr)
{
    char pagemap_file[] = "/proc/self/pagemap";
    int pagemap_fd = -1;

    pagemap_fd = open(pagemap_file, O_RDONLY);
    if (pagemap_fd < 0) {
        perror("failed to open pagemap");
        exit(1);
    }

    PagemapEntry entry;
    if (pagemap_get_entry(&entry, pagemap_fd, vaddr)) {
        exit(1);
    }

    close(pagemap_fd);

    return (entry.pfn * PAGE_SIZE) + (vaddr % PAGE_SIZE);
}

// write coverage info to mem
static void write_coverage_info(uint8_t* shared_mem, struct coverage_t* coverage_info) {
    // TODO: write coverage info to host
    struct {
        // this size for the array (8 bytes per value)
        uint64_t pcs_array_size;
        uint64_t counters_array_size;
    } coverage_info_header;

    coverage_info_header.pcs_array_size = (coverage_info->pcs_array_end - coverage_info->pcs_array_start) / sizeof(uint64_t);
    coverage_info_header.counters_array_size = coverage_info->counters_array_end - coverage_info->counters_array_start;

    memcpy(shared_mem + 8, &coverage_info_header, sizeof(coverage_info_header));
    shared_mem[0] = 2;
    while(shared_mem[0] != 0xff); // synchronize

    memset(shared_mem + 0x800, 0, 0x800);

    // write pcs array
    uint64_t remaining_bytes = coverage_info_header.pcs_array_size * 8;
    int i = 0;
    while (remaining_bytes > 0) {
        if (remaining_bytes >= 0x800) {
            memcpy(shared_mem + 0x800, (void*)coverage_info->pcs_array_start + i * 0x800, 0x800);
            remaining_bytes -= 0x800;
        } else {
            memcpy(shared_mem + 0x800, (void*)coverage_info->pcs_array_start + i * 0x800, remaining_bytes);
            remaining_bytes = 0;
        }
        ((uint32_t*)shared_mem)[0] = i;
        ((uint32_t*)shared_mem)[1] = -1;
        // wait for recv
        while (((uint32_t*)shared_mem)[1] != i);
        i++;
    }

    // write counters array
    remaining_bytes = coverage_info_header.counters_array_size;
    i = 0;
    while (remaining_bytes > 0) {
        if (remaining_bytes >= 0x800) {
            memcpy(shared_mem + 0x800, (void*)coverage_info->counters_array_start + i * 0x800, 0x800);
            remaining_bytes -= 0x800;
        } else {
            memcpy(shared_mem + 0x800, (void*)coverage_info->counters_array_start + i * 0x800, remaining_bytes);
            remaining_bytes = 0;
        }
        ((uint32_t*)shared_mem)[0] = i;
        ((uint32_t*)shared_mem)[1] = -1;
        // wait for recv
        while (((uint32_t*)shared_mem)[1] != i);
        i++;
    }

    // done with everything
    shared_mem[0] = 7;
    while(shared_mem[0] != 8); // synchronize
}

/**
 * Set up the shared memory page to communicate between the fuzzer client and
 * the fuzzer host. We allocate a page, get its physical address, and tell the
 * snapshot device to map the physical VM page to the shared memory.
 */
static uint8_t* setup_shared_mem(uint8_t* pci_memory) {
    uint8_t* mem;
    uintptr_t paddr;

    // get an anonymous page
    mem = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
    if (mem == MAP_FAILED) {
        perror("setup_shared_mem mmap");
        exit(1);
    }

    // lock it to ensure a physical page
    if (mlock(mem, PAGE_SIZE) < 0) {
        perror("setup_shared_mem mlock");
        exit(1);
    }

    // get its physical address
    paddr = virt_to_phys_user((uintptr_t)mem);

    // tell snapshot device to link the physical page to the shared memory
    *(uint64_t*)(pci_memory + 0x10) = paddr;

    // return the page
    return mem;
}

int main(int argc, char** argv) {
    int pci_fd;
    uint8_t* pci_memory;
    uint32_t* pci_memory_command;
    uint8_t data[MAX_INPUT_SIZE];
    size_t size;
    uint8_t* shared_mem;
    struct coverage_t* coverage_info;
    int do_snapshots = argc > 1;

    assert(PAGE_SIZE == PAGE_SIZE);
    setbuf(stdout, NULL);

    // communicate with PCI device for snapshotting
    pci_fd = open("/sys/bus/pci/devices/0000:00:04.0/resource0", O_RDWR | O_SYNC);
    pci_memory = mmap(NULL, 1024 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, pci_fd, 0);
    pci_memory_command = (uint32_t*)pci_memory;
    if (pci_memory == MAP_FAILED) {
        perror("mmap");
        return 1;
    }

    memset(data, 0, sizeof(data));
    size = 0;

    // do initialization work here...

    shared_mem = setup_shared_mem(pci_memory);
    // clear shared memory
    memset(shared_mem, 0, PAGE_SIZE);
    // write coverage array size and counters array size
    coverage_info = get_coverage_info();
    *(uint64_t*)(shared_mem + 0x8) = (uint64_t)(coverage_info->pcs_array_end - coverage_info->pcs_array_start) / sizeof(uint64_t);
    *(uint64_t*)(shared_mem + 0x10) = (uint64_t)(coverage_info->counters_array_end - coverage_info->counters_array_start);
    shared_mem[0] = 5;
    // wait for acknowledgement
    while (shared_mem[0] != 2);
    // reset polling byte to 0
    shared_mem[0] = 0;

    // initialization done

    // save a snapshot
    if (do_snapshots)
        *pci_memory_command = 0x101;

    // wait for input
    while (shared_mem[0] != 1);
    // reset polling byte to 0
    shared_mem[0] = 0;
    size = *(uint32_t*)(shared_mem + 4);
    memcpy(data, shared_mem + 8, size);

    // run the code on the input
    run_fuzzer_once(data, size);

    // get coverage results
    coverage_info = get_coverage_info();
    write_coverage_info(shared_mem, coverage_info);

    // restore snapshot
    if (do_snapshots)
        *pci_memory_command = 0x102;

    puts("outside of snapshot/restore region");

    // release shared memory
    *pci_memory_command = 0x202;

    // clear shared memory
    memset(shared_mem, 0, PAGE_SIZE);

    return 0;
}
