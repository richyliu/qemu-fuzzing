#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <time.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <unistd.h>

#include "serial.h"

#define SERIAL_PORT "/dev/ttyS4"

#define MAX_INPUT_SIZE 0x400
#define MAX_TRACE_ARR_SIZE 0x1000000

/* #define PRINT_VERBOSE */
/* #define PRINT_TRACE_CMP */

static uintptr_t pcs_array_start = 0;
static uintptr_t pcs_array_end = 0;

static uintptr_t counters_array_start = 0;
static uintptr_t counters_array_end = 0;

static uint64_t trace_array[MAX_TRACE_ARR_SIZE];
static size_t trace_array_size = 0;
static size_t trace_counter = 0;

void print_pcs() {
#ifdef PRINT_VERBOSE
    printf("%p %p\n", pcs_array_start, pcs_array_end);
    uintptr_t *start = pcs_array_start;
    uintptr_t *end = pcs_array_end;
    for (uintptr_t *p = start; p < end; p++) {
        printf("%p: %lx\n", p, *p);
    }
    printf("\n");
#endif
}

// prints the 8bit counters
void print_counters() {
#ifdef PRINT_VERBOSE
    printf("%p %p\n", counters_array_start, counters_array_end);
    uint8_t *start = (uint8_t *)counters_array_start;
    uint8_t *end = (uint8_t *)counters_array_end;
    for (uint8_t *p = start; p < end; p++) {
        printf("%p: %x\n", p, *p);
    }
    printf("\n");
#endif
}

// note: can be called multiple times with the same arguments
void __sanitizer_cov_pcs_init(const uintptr_t *pcs_beg, const uintptr_t *pcs_end) {
    printf("%s: %p, %p\n", __func__, pcs_beg, pcs_end);
    pcs_array_start = (uintptr_t)pcs_beg;
    pcs_array_end = (uintptr_t)pcs_end;
    print_pcs();
}

// note: can be called multiple times with the same arguments
void __sanitizer_cov_8bit_counters_init(char *start, char *end) {
    printf("%s: %p, %p\n", __func__, start, end);
    counters_array_start = (uintptr_t)start;
    counters_array_end = (uintptr_t)end;
    print_counters();
}

static void trace_add(uint64_t data) {
    if (trace_array_size == MAX_TRACE_ARR_SIZE) {
        printf("trace array full\n");
        exit(1);
    }
    trace_array[trace_array_size++] = data;

}

void __sanitizer_cov_trace_cmp1(uint8_t Arg1, uint8_t Arg2) {
#ifdef PRINT_TRACE_CMP
    printf("%s: %u, %u\n", __func__, Arg1, Arg2);
#endif
    trace_counter += 24;
}

void __sanitizer_cov_trace_cmp2(uint16_t Arg1, uint16_t Arg2) {
#ifdef PRINT_TRACE_CMP
    printf("%s: %u, %u\n", __func__, Arg1, Arg2);
#endif
    trace_counter += 24;
}

void __sanitizer_cov_trace_cmp4(uint32_t Arg1, uint32_t Arg2) {
#ifdef PRINT_TRACE_CMP
    printf("%s: %u, %u\n", __func__, Arg1, Arg2);
#endif
    trace_counter += 24;
    /* trace_counter += 9; */
}

void __sanitizer_cov_trace_cmp8(uint64_t Arg1, uint64_t Arg2) {
#ifdef PRINT_TRACE_CMP
    printf("%s: %llu, %llu\n", __func__, Arg1, Arg2);
#endif
    trace_counter += 24;
    /* trace_counter += 17; */
}

void __sanitizer_cov_trace_const_cmp1(uint8_t Arg1, uint8_t Arg2) {
#ifdef PRINT_TRACE_CMP
    printf("%s: %u, %u\n", __func__, Arg1, Arg2);
#endif
    trace_counter += 24;
}

void __sanitizer_cov_trace_const_cmp2(uint16_t Arg1, uint16_t Arg2) {
#ifdef PRINT_TRACE_CMP
    printf("%s: %u, %u\n", __func__, Arg1, Arg2);
#endif
    trace_counter += 24;
}

void __sanitizer_cov_trace_const_cmp4(uint32_t Arg1, uint32_t Arg2) {
#ifdef PRINT_TRACE_CMP
    printf("%s: %u, %u\n", __func__, Arg1, Arg2);
#endif
    trace_counter += 24;
    /* trace_counter += 9; */
}

void __sanitizer_cov_trace_const_cmp8(uint64_t Arg1, uint64_t Arg2) {
#ifdef PRINT_TRACE_CMP
    printf("%s: %llu, %llu\n", __func__, Arg1, Arg2);
#endif
    trace_counter += 24;
    /* trace_counter += 17; */
}

int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size);

void run_fuzzer_once(uint8_t* data, size_t size) {
    trace_counter = 0;
    LLVMFuzzerTestOneInput(data, size);
    printf("trace_counter: %zu\n", trace_counter);

    print_pcs();
    print_counters();
}

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
int pagemap_get_entry(PagemapEntry *entry, int pagemap_fd, uintptr_t vaddr)
{
    size_t nread;
    ssize_t ret;
    uint64_t data;
    uintptr_t vpn;

    vpn = vaddr / sysconf(_SC_PAGE_SIZE);
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
uintptr_t virt_to_phys_user(uintptr_t vaddr)
{
    char pagemap_file[] = "/proc/self/pagemap";
    static int pagemap_fd = 0;

    if (pagemap_fd == 0) {
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

    return (entry.pfn * sysconf(_SC_PAGE_SIZE)) + (vaddr % sysconf(_SC_PAGE_SIZE));
}

#define MAX_TABLE_SIZE 0x400

// arrays of pointers to (partial) pages. Last item is the end page (if not a
// page boundary)
struct coverage_info {
    uintptr_t pcs_paddrs[MAX_TABLE_SIZE];
    size_t pcs_count;
    uintptr_t counters_paddrs[MAX_TABLE_SIZE];
    size_t counters_count;
    uintptr_t trace_paddrs[MAX_TABLE_SIZE];
    size_t trace_count;
};

// convert contiguous virtual memory to array of physical addresses
void paddr_array(uintptr_t paddrs[], size_t *count, size_t max_size, uintptr_t start, uintptr_t end) {
    size_t page_size = sysconf(_SC_PAGE_SIZE);

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
void write_coverage_info(uint8_t* mem) {
    struct coverage_info info;
    info.pcs_count = 0;
    info.counters_count = 0;
    info.trace_count = 0;

    paddr_array(info.pcs_paddrs, &info.pcs_count, MAX_TABLE_SIZE, pcs_array_start, pcs_array_end);
    paddr_array(info.counters_paddrs, &info.counters_count, MAX_TABLE_SIZE, counters_array_start, counters_array_end);

    memcpy(mem, &info, sizeof(info));
}

int main(void) {
    setbuf(stdout, NULL);

    // communicate with PCI device for snapshotting
    int pci_fd = open("/sys/bus/pci/devices/0000:00:05.0/resource0", O_RDWR | O_SYNC);
    uint32_t* pci_memory = mmap(NULL, 1024 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, pci_fd, 0);
    if (pci_memory == MAP_FAILED) {
        perror("mmap");
        return 1;
    }

    int fd = open_serial_port(SERIAL_PORT, 115200);
    if (fd < 0) {
        perror("open_serial_port");
        return 1;
    }

    uint8_t data[MAX_INPUT_SIZE];
    memset(data, 0, sizeof(data));

    size_t size = 0;

    // do initialization work here...
    puts("initializing...");

    // initialization done
    puts("initialization done");

    uint8_t init_done = 'I';
    write_port(fd, &init_done, sizeof(init_done));
    puts("sent init_done");
    puts("ready for snapshotting");

    // save a snapshot
    /* pci_memory[0] = 0x101; */
    puts("in snapshotted code");

    // wait for next input
    puts("waiting for input...");
    uint8_t input_ready = 0;
    while (input_ready != 'R') {
        if (read_port(fd, &input_ready, sizeof(input_ready)) > 0) {
            printf("got: %02x\n", input_ready);
        }
    }

    // read input data
    puts("reading input...");
    read_port(fd, (uint8_t*)&size, sizeof(size));
    if (size > MAX_INPUT_SIZE) {
        puts("input too large");
        return 1;
    }
    read_port(fd, data, size);

    printf("running with input: size: %zu\n", size);
    for (size_t i = 0; i < size; i++) {
        printf("%02x ", data[i]);
    }
    printf("\n");

    run_fuzzer_once(data, size);
    puts("done");

    puts("writing coverage info to PCI device...");
    write_coverage_info((uint8_t*)pci_memory + 0x1000);
    puts("done writing coverage info to PCI device");

    puts("writing output...");
    char res = 'D';
    write_port(fd, (uint8_t*)&res, sizeof(res));
    trace_counter = data[2]; // TODO: temp
    write_port(fd, (uint8_t*)&trace_counter, sizeof(trace_counter));
    puts("done");

    puts("restoring...");
    // restore snapshot
    /* pci_memory[0] = 0x102; */

    puts("ERROR: should not reach here");
    return 1;
}
