// Created by Dvir Ben Asuli 318208816 on 08/01/2021.
// Tel-Aviv University, Operating Systems Course Assignment 5, 2021a
//

#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>


/***** Functions Declarations *****/
/**
 * @purpose Creating a new socket fot the client
 * @return The new client's FD
 */
int create_client();

/**
 * @purpose Client reads a file and sends its size and content to the server
 *          and afterwards receives the number of printable characters it has
 * @param argv[1]- IP Address
 * @param argv[2]- server's port
 * @param argv[3]- Path of file
 */
int main(int argc, char *argv[]);

/***** Functions Implementations *****/
int create_client() {
    int client_fd;

    // Creating socket file descriptor
    if (0 == (client_fd = socket(AF_INET,
                            SOCK_STREAM, 0))) {
        perror("Starting socket failed");
        exit(1);
    }

    return client_fd;

}

int main(int argc, char *argv[]) {
    struct sockaddr_in server;
    struct in_addr server_ip;
    char *message_from_file;
    int server_port;
    char *filepath;
    int client;
    FILE *file;

    //Checking validity of commands
    if (argc != 4) {
        fprintf(stderr, "Invalid command\n");
        exit(1);
    }

    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_family = AF_INET;
    inet_aton(argv[1], &server_ip);
    filepath = argv[3];
    client = create_client();
    server_port = atoi(argv[2]);
    server.sin_port = server_port;
    file = fopen(filepath, "rw");

    if (file == NULL) {
        perror("Error opening file");
        exit(1);
    }

    //Connecting to the server
    if (connect(client,
                (struct sockaddr *) &server,
                sizeof(server)) < 0) {
        perror("Connection failed in client's side");
        exit(1);
    }

    //Getting the size of the file
    fseek(file, 0, SEEK_END);
    uint32_t size = ftell(file);
    uint32_t size_network = htonl(size);

    //Sending the size of the file
    if (send(client, &size_network, sizeof(htonl(size)), 0) < 0) {
        if ((errno == ETIMEDOUT) || (errno == ECONNRESET) || (errno == EPIPE)) {
            perror("Non-fatal error occurred");
            close(client);
            exit(0);
        } else if ((errno == SIGINT) || (errno == EINTR)) {
            //Do nothing, part of the flow
        } else {
            perror("Error sending message size ");
            exit(1);
        }
    }

    rewind(file);
    message_from_file = malloc(size);
    memset(message_from_file, '\0', size);

    //Getting the entire content of the file and sending it to server

    int temp;
    unsigned int index = 0U;
    while ( (temp = fgetc(file)) ) {
        /* avoid buffer overflow error */

        if (temp == EOF) {
            message_from_file[index] = '\0';
            break;
        }
        else if (temp == '\n') {
            message_from_file[index] = '\0';
            index = 0U;
            continue;
        }
        else
            message_from_file[index++] = (char)temp;
    }

    if (send(client, message_from_file, strlen(message_from_file), 0) < 0) {
        if ((errno == ECONNRESET) || (errno == ETIMEDOUT) || (errno == EPIPE)) {
            perror("Non-fatal error occurred");
            close(client);
        } else if ((errno == SIGINT) || (errno == EINTR)) {
            //Do nothing, part of the flow
        } else {
            perror("Error sending message ");
            return 1;
        }
    }

    fclose(file);
    uint32_t C;

    if (recv(client, &C, sizeof(C), 0) < 0) {
        if ((errno == ETIMEDOUT) || (errno == EPIPE) || (errno == ECONNRESET)) {
            perror("Non-fatal error occurred");
            close(client);
            exit(0);
        } else if ((errno == SIGINT) || (errno == EINTR)) {
            //Do nothing
        } else {
            perror("Error receiving message ");
            exit(1);
        }
    }

    printf("# of printable characters: %u\n", C);

    free(message_from_file);
    close(client);
    exit(0);
}
