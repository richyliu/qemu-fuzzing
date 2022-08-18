/* #include "lib/error_functions.h" */
/* #include "sockets/unix_socket.h" */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/un.h>

#define BUF_SIZE 500

#define MAX_INPUT_SIZE 0x400

int socket_connect(char* path) {
    struct sockaddr_un addr;

    // Create a new client socket with domain: AF_UNIX, type: SOCK_STREAM, protocol: 0
    int sfd = socket(AF_UNIX, SOCK_STREAM, 0);
    printf("Client socket fd = %d\n", sfd);

    // Make sure socket's file descriptor is legit.
    if (sfd == -1) {
      perror("socket");
      exit(1);
    }

    // Construct server address, and make the connection.
    memset(&addr, 0, sizeof(struct sockaddr_un));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

    // Connects the active socket referred to be sfd to the listening socket
    // whose address is specified by addr.
    if (connect(sfd, (struct sockaddr *) &addr,
                sizeof(struct sockaddr_un)) == -1) {
      perror("connect");
      exit(1);
    }

    return sfd;
}

int main(int argc, char *argv[]) {
    ssize_t numRead;
    char buf[BUF_SIZE];
    int fd_snapshot = socket_connect("/tmp/my_snapshot");

    uint8_t data[MAX_INPUT_SIZE];
    memset(data, 0, sizeof(data));

    size_t size = 0;

    data[size++] = 0x02;
    data[size++] = 0x03;

    // wait for client to be initialized
    uint8_t recv = 0;
    while (recv != 'I') {
        read(fd_snapshot, &recv, 1);
    }
    puts("Client initialized");

    // notify client that input is ready
    write(fd_snapshot, "R", 1);

    // send inputs
    puts("sending inputs...");
    write(fd_snapshot, (uint8_t*)&size, sizeof(size));
    write(fd_snapshot, data, size);
    puts("sent inputs");

    read(fd_snapshot, buf, BUF_SIZE);
    printf("%s", buf);

    close(fd_snapshot);

    return 0;
}
