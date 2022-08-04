/* #include "lib/error_functions.h" */
/* #include "sockets/unix_socket.h" */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#define BUF_SIZE 500
#define SV_SOCK_PATH "/tmp/foo"

int main(int argc, char *argv[]) {
    struct sockaddr_un addr;
    ssize_t numRead;
    char buf[BUF_SIZE];

    // Create a new client socket with domain: AF_UNIX, type: SOCK_STREAM, protocol: 0
    int sfd = socket(AF_UNIX, SOCK_STREAM, 0);
    printf("Client socket fd = %d\n", sfd);

    // Make sure socket's file descriptor is legit.
    if (sfd == -1) {
      perror("socket");
      return 1;
    }

    //
    // Construct server address, and make the connection.
    //
    memset(&addr, 0, sizeof(struct sockaddr_un));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SV_SOCK_PATH, sizeof(addr.sun_path) - 1);

    // Connects the active socket referred to be sfd to the listening socket
    // whose address is specified by addr.
    if (connect(sfd, (struct sockaddr *) &addr,
                sizeof(struct sockaddr_un)) == -1) {
      perror("connect");
      return 1;
    }

    //
    // Copy stdin to socket.
    //

    /* // Read at most BUF_SIZE bytes from STDIN into buf. */
    /* while ((numRead = read(STDIN_FILENO, buf, BUF_SIZE)) > 0) { */
    /*   // Then, write those bytes from buf into the socket. */
    /*   if (write(sfd, buf, numRead) != numRead) { */
    /*     puts("partial/failed write"); */
    /*     return 2; */
    /*   } */
    /* } */

    write(sfd, ".", 1);
    read(sfd, buf, BUF_SIZE);
    printf("%s", buf);

    if (numRead == -1) {
      perror("read");
      return 1;
    }

    close(sfd);
    // Closes our socket; server sees EOF.
    exit(EXIT_SUCCESS);
}
