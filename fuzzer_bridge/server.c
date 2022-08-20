/* #include "lib/error_functions.h" */
/* #include "sockets/unix_socket.h" */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/un.h>

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
    int fd_snapshot = socket_connect("/tmp/my_snapshot");

    uint8_t data[MAX_INPUT_SIZE];
    memset(data, 0, sizeof(data));

    size_t size = 0;

    data[size++] = 0;
    data[size++] = 0;
    data[size++] = 1;

    // wait for client to be initialized
    puts("Waiting for client to be initialized...");
    uint8_t recv = 0;
    while (recv != 'I') {
        read(fd_snapshot, &recv, 1);
    }
    puts("Client initialized");

    for (int i = 1; i < 10; i++) {
        // update input
        data[2] = i;

        // notify client that input is ready
        write(fd_snapshot, "R", 1);

        // send inputs
        puts("sending inputs...");
        write(fd_snapshot, (uint8_t*)&size, sizeof(size));
        write(fd_snapshot, data, size);
        puts("sent inputs");

        // wait for client to be done
        puts("Waiting for client to be done...");
        char res = '\0';
        while (res != 'D') {
            read(fd_snapshot, &res, 1);
        }
        puts("Client done");
        size_t trace_counter = 0;
        read(fd_snapshot, &trace_counter, sizeof(trace_counter));
        printf("response: %c, trace_counter: %zu\n", res, trace_counter);
    }

    close(fd_snapshot);

    return 0;
}
