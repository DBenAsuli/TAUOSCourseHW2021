// Created by Dvir Ben Asuli 318208816 on 08/01/2021.
// Tel-Aviv University, Operating Systems Course Assignment 5, 2021a
//

#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <netinet/in.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

/***** Macros *****/
#define NUM_OF_CHARACTERS      (95)
#define LISTEN_BACKLOG         (10)
#define HIGHEST_ASCI           (126)
#define LOWEST_ASCI            (32)
#define NUM_BYTES              (4)
#define GET_CHAR_INDEX(a)      (a-LOWEST_ASCI)
#define GET_CHAR_FROM_INDEX(a) (a+LOWEST_ASCI)

/***** Variables Declarations *****/
static uint32_t pcc_total_temp[NUM_OF_CHARACTERS] = {0};
static uint32_t pcc_total[NUM_OF_CHARACTERS] = {0};
static int sigint = 0;
static int client_flow = 0; //Indicates if we are currently dealing with a client

/***** Functions Declarations *****/

/**
 * @purpose Creating a new socket fot the server
 * @return The new server's FD
 */
int create_server();

/**
 * @purpose Counting the printable characters received in a buffer and updating
 *          the temporary pcc total array with their number of appearances per character
 * @return Total number of printable characters found in string
 */
uint32_t count_chars(char *message);

/**
 * @purpose Transferring data from temporary pcc total to final one
 * @param mode- 0 is sent when a client failed after temporary pcc total was updated, meaning
 *                we need to undo the cound and revert the pcc_temp to what the pcc total was
 *                before communicating with this server.
 *              1 is sent when a client connection ended successfully, meaning we need to
 *              update the final pcc_total so it'll have a new additions to the counters
 */
void duplicate_pcc_total(int mode);

/**
 * @purpose Set a signal to '1' once sigint is received
 * @param signal - the signal
 */
void sigint_handler(int signal);

/**
 * @purpose Function to be called from main,
 *          constant loop to wait for a connection and a message from client
 * @param server - FD of the server
 * @return 0 on success
 */
int message_loop(int server);

/**
 * @purpose Print the desired message of how many time each printable character appeared
 *          in the data received from the clients
 */
void print_chars();

/**
 * @purpose Server that counts how many of the bytes sent to it by client
 *          are printable and returns that number to the client
 * @param argv[1]- server's port
 */
int main(int argc, char *argv[]);

/***** Functions Implementations *****/
int create_server() {
    int server_fd;

    // Creating socket file descriptor
    if (0 == (server_fd = socket(AF_INET,
                            SOCK_STREAM, 0))) {
        perror("Starting socket failed");
        exit(1);
    }

    return server_fd;
}

uint32_t count_chars(char *message) {
    uint32_t res = 0;
    int i; //Index for pcc_total array

    for (int dbzy = 0; dbzy < strlen(message); dbzy++) {

        int byte_value = (int) message[dbzy];
        if ((byte_value >= LOWEST_ASCI) && (byte_value <= HIGHEST_ASCI)) {
            i = GET_CHAR_INDEX(byte_value);
            pcc_total_temp[i]++;
            res++;
        }
    }

    return res;
}

void duplicate_pcc_total(int mode) {
    for (int dbzy = 0; dbzy < NUM_OF_CHARACTERS; dbzy++) {
        if (mode == 0) {
            //Copying old pcc_total to temp to undo counting
            pcc_total_temp[dbzy] = pcc_total[dbzy];
        } else if (mode == 1) {
            //Copying new temporary pcc_total to final pcc_total
            pcc_total[dbzy] = pcc_total_temp[dbzy];
        }
    }
}

int message_loop(int server) {
    struct sockaddr_in client;
    char *message_from_home;
    int connected_socket;

    int client_length = sizeof(client);

    while (!sigint) {

        client_flow = 0;
        sigset_t sigset, oldset;
        sigemptyset(&sigset);
        sigaddset(&sigset, SIGINT);

        //Connecting to the client
        connected_socket = accept(server,
                                  (struct sockaddr *) &client,
                                  (socklen_t * ) & client_length);

        //We want to block signals of SIGINT from this point on since we dont want them to interrupt
        //in the flow of handeling a client
        sigprocmask(SIG_BLOCK, &sigset, &oldset);

        //In case we received a sigint after accepting we should exit
        if (sigint) { break; }
        client_flow = 1;

        //Checking if connection was successful
        if (connected_socket < 0) {
            perror("Error accepting client");
            client_flow = 0;
            sigprocmask(SIG_UNBLOCK, &sigset, &oldset);
            return 1;
        }

        //Get the length of the message from the client
        int N_network, N_host;
        int receive_res = recv(connected_socket,
                               &N_network,
                               NUM_BYTES, 0);

        //Checking if receiving action went well
        if (receive_res < 0) {
            if ((errno == EPIPE) || (errno == ETIMEDOUT) || (errno == ECONNRESET)) {
                perror("Non-fatal error occurred");
                close(connected_socket);
                client_flow = 0;
                sigprocmask(SIG_UNBLOCK, &sigset, &oldset);
                continue;
            } else if ((errno == SIGINT) || (errno == EINTR)) {
                //Do nothing, part of the flow
            } else {
                perror("Error receiving message size");
                client_flow = 0;
                sigprocmask(SIG_UNBLOCK, &sigset, &oldset);
                return 1;
            }
        }

        //Set a buffer of length N
        N_host = ntohl(N_network);
        message_from_home = malloc(sizeof(char) * N_host);
        memset(message_from_home, '\0', sizeof(char) * N_host);

        //Receive the message from the client
        int receive_res2 = recv(connected_socket,
                                message_from_home,
                                sizeof(char) * N_host,
                                0);

        //Checking if receiving action went well
        if (receive_res2 < 0) {
            if ((errno == ETIMEDOUT) || (errno == ECONNRESET) || (errno == EPIPE)) {
                perror("Non-fatal error occurred");
                close(connected_socket);
                client_flow = 0;
                sigprocmask(SIG_UNBLOCK, &sigset, &oldset);
                continue;
            } else if ((errno == SIGINT) || (errno == EINTR)) {
                //Do nothing, part of the flow
            } else {
                perror("Error receiving message");
                client_flow = 0;
                sigprocmask(SIG_UNBLOCK, &sigset, &oldset);
                return 1;
            }
        }

        //Sending the number of printable characters to client
        uint32_t chars_count = count_chars(message_from_home);
        if (send(connected_socket, &chars_count, sizeof(chars_count), 0) < 0) {
            if ((errno == ETIMEDOUT) || (errno == ECONNRESET) || (errno == EPIPE)) {
                perror("Non-fatal error occurred");
                close(connected_socket);
                duplicate_pcc_total(0);
                client_flow = 0;
                sigprocmask(SIG_UNBLOCK, &sigset, &oldset);
                continue;
            } else if ((errno == SIGINT) || (errno == EINTR)) {
                //Do nothing, part of the flow
            } else {
                perror("Error sending message ");
                client_flow = 0;
                sigprocmask(SIG_UNBLOCK, &sigset, &oldset);
                return 1;
            }
        }

        //Close the connection of the current client
        close(connected_socket);
        duplicate_pcc_total(1);
        free(message_from_home);

        //Now SIGINT is unblocked and if a signal was received during the flow
        //we will exit the loop
        client_flow = 0;
        sigprocmask(SIG_UNBLOCK, &sigset, &oldset);
    }

    return 0;
}

void sigint_handler(int signal) {
    if (!client_flow) {
        sigint = 1;
    }
}

void print_chars() {
    for (int dbzy = 0; dbzy < NUM_OF_CHARACTERS; dbzy++) {
        printf("char '%c' : %u times\n", GET_CHAR_FROM_INDEX(dbzy), pcc_total[dbzy]);
    }
}

int main(int argc, char *argv[]) {

    struct sockaddr_in socket_address;
    int server_port;
    int server;

    //Checking validity of command
    if (argc != 2) {
        fprintf(stderr, "Invalid command\n");
        exit(1);
    }

    //Creating the server socket
    socket_address.sin_family = AF_INET;
    socket_address.sin_addr.s_addr = htonl(INADDR_ANY);
    server = create_server();

    // Attaching the socket to the received port
    if (1 == setsockopt(server,
                        SOL_SOCKET,
                        SO_REUSEADDR,
                        &(int) {1},
                        sizeof(int))) {
        perror("Failed to set socket options");
        exit(1);
    }

    server_port = atoi(argv[1]);
    socket_address.sin_port = server_port;

    if (0 != bind(server,
                  (struct sockaddr *) &socket_address,
                  sizeof(socket_address))) {
        perror("Failed to bind to server");
        exit(1);
    }

    if (0 != listen(server,
                    LISTEN_BACKLOG)) {
        perror("Failed to listen to server");
        exit(1);
    }

    //Setting the handler for SIGINT
    struct sigaction sigaction_for_sigint = {
            .sa_handler = sigint_handler
    };

    if ((-1) == sigaction(SIGINT,
                          &sigaction_for_sigint,
                          NULL)) {
        fprintf(stderr, "SIGINT handling error occurred");
        if (!client_flow) {
            sigint = 1;
        }
    }

    //Iterating to find clients and receive data
    if (0 != message_loop(server)) {
        perror("Error receiving Message");
        exit(1);
    }

    //We exited the inner loop, connection was terminated by user
    print_chars();
    close(server);
    return 0;
}
