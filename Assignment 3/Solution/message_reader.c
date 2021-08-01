// Created by Dvir Ben Asuli 318208816 on 29/11/2020.
// Tel-Aviv University, Operating Systems Course Assignment 3, 2021a
//

#include "message_slot.h"

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>


/***** Defines *****/

/***** Enums *****/

/***** Declarations *****/
/**
 * @purpose Implement reading action
 */
int main(int argc, char *argv[]);

/***** Function Implementations *****/

int main(int argc, char *argv[]) {
    int file; // File descriptor
    int value; // For returned value
    int write_val; //For returned value from writing
    unsigned long channel_id;
    char message[BUFFER_LENGTH];

    if (argc != 3) {
        errno = -EINVAL;
        perror("Invalid command");
        fprintf(stderr, "Invalid command");
        exit(1);
    }

    channel_id = atoi(argv[2]);
    file = open(argv[1], O_RDWR);

    if (file <0) {
        perror("Error opening file");
        fprintf(stderr, "Error opening file");
        exit(1);
    }

    value = ioctl(file, MSG_SLOT_CHANNEL, channel_id);

    if (value < 0) {
        perror("Error setting channel");
        fprintf(stderr, "Error setting channel");
        close(file);
        exit(1);
    }

    value = read(file, message, BUFFER_LENGTH);

    if (value < 0) {
        perror("Error reading message");
        fprintf(stderr, "Error reading message");
        close(file);
        exit(1);
    }

    write_val = write(STDOUT_FILENO, message, value);
    close(file);

    if (write_val < 0) {
        perror("Error printing read message");
    }

    exit(0);


}
