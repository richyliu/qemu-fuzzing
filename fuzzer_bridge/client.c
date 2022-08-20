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
    static int pagemap_fd = -1;

    if (pagemap_fd == -1) {
        pagemap_fd = open(pagemap_file, O_RDONLY);
        if (pagemap_fd < 0) {
            perror("failed to open pagemap");
            exit(1);
        }
    }

    PagemapEntry entry;
    if (pagemap_get_entry(&entry, pagemap_fd, vaddr)) {
        puts("failed to get pagemap entry");
        exit(1);
    }
    /* close(pagemap_fd); */

    return (entry.pfn * PAGE_SIZE) + (vaddr % PAGE_SIZE);
}

#define MAX_TABLE_SIZE 0x400

// arrays of pointers to (partial) pages. Last item is the end page (if not a
// page boundary)
struct exported_coverage_info {
    uintptr_t pcs_paddrs[MAX_TABLE_SIZE];
    size_t pcs_count;
    uintptr_t counters_paddrs[MAX_TABLE_SIZE];
    size_t counters_count;
    uintptr_t trace_paddrs[MAX_TABLE_SIZE];
    size_t trace_count;
};

// convert contiguous virtual memory to array of physical addresses
static void paddr_array(uintptr_t paddrs[], size_t *count, size_t max_size, uintptr_t start, uintptr_t end) {
    size_t page_size = PAGE_SIZE;

    // possible start half page
    if (start % page_size) {
        // copy start page
        if (*count == max_size) {
            printf("paddr_array: array full\n");
            exit(1);
        }
        paddrs[(*count)++] = virt_to_phys_user(start);
        start = (start & ~(page_size - 1));
    }

    while (start < end) {
        if (*count == max_size) {
            printf("paddr_array: array full\n");
            exit(1);
        }
        paddrs[(*count)++] = virt_to_phys_user(start);
        start = start + page_size;
    }

    // possible end half page
    if (end % page_size) {
        // copy end page
        if (*count == max_size) {
            printf("paddr_array: array full\n");
            exit(1);
        }
        paddrs[(*count)++] = virt_to_phys_user(end);
    }
    printf("count: %zu, max: %zu\n", *count, max_size);
}

// write coverage info to mem
static void write_coverage_info(uint8_t* mem) {
    struct exported_coverage_info info;
    struct coverage_t* coverage_info = get_coverage_info();
    info.pcs_count = 0;
    info.counters_count = 0;
    info.trace_count = 0;

    paddr_array(info.pcs_paddrs, &info.pcs_count, MAX_TABLE_SIZE, coverage_info->pcs_array_start, coverage_info->pcs_array_end);
    paddr_array(info.counters_paddrs, &info.counters_count, MAX_TABLE_SIZE, coverage_info->counters_array_start, coverage_info->counters_array_end);

    memcpy(mem, &info, sizeof(info));
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
    printf("paddr: 0x%lx, mem: 0x%lx\n", paddr, (uintptr_t)mem);

    // tell snapshot device to link the physical page to the shared memory
    *(uint64_t*)(pci_memory + 0x10) = paddr;

    // return the page
    return mem;
}

int main(void) {
    int pci_fd;
    uint8_t* pci_memory;
    uint32_t* pci_memory_command;
    uint8_t data[MAX_INPUT_SIZE];
    size_t size;
    uint8_t* shared_mem;
    struct coverage_t* coverage_info;

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
    puts("initializing...");

    shared_mem = setup_shared_mem(pci_memory);
    // test write to shared memory
    shared_mem[0] = 0x43;
    /* // trigger msync */
    /* *pci_memory_command = 0x201; */

    // initialization done
    puts("initialization done");

    puts("ready for snapshotting");

    // save a snapshot
    *pci_memory_command = 0x101;
    puts("in snapshotted code");

    // read input data
    puts("reading input...");
    // wait for input
    while (shared_mem[0] != 1);
    // reset polling byte to 0
    shared_mem[0] = 0;
    size = *(uint32_t*)(shared_mem + 4);
    memcpy(data, shared_mem + 8, size);

    printf("running with input: size: %zu\n", size);
    for (size_t i = 0; i < size; i++) {
        printf("%02x ", data[i]);
    }
    printf("\n");

    run_fuzzer_once(data, size);
    puts("done");

    puts("writing coverage info...");
    coverage_info = get_coverage_info();
    // TODO: write coverage info to host
    /* write_coverage_info((uint8_t*)pci_memory + 0x1000); */
    shared_mem[0] = 2;
    *(uint64_t*)(shared_mem + 8) = coverage_info->trace_counter;
    /* // trigger msync */
    /* *pci_memory_command = 0x201; */
    puts("done writing coverage info");

    puts("restoring...");
    // restore snapshot
    *pci_memory_command = 0x102;

    puts("ERROR: should not reach here");

    return 1;
}
