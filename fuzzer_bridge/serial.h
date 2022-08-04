#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>

int open_serial_port(const char * device, uint32_t baud_rate);
int write_port(int fd, uint8_t * buffer, size_t size);
ssize_t read_port(int fd, uint8_t * buffer, size_t size);
