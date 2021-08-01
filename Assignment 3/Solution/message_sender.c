// Created by Dvir Ben Asuli 318208816 on 29/11/2020.
// Tel-Aviv University, Operating Systems Course Assignment 3, 2021a
//

#include "message_slot.h"

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>

/***** Defines *****/

/***** Enums *****/

/***** Declarations *****/
/**
 * @purpose Implement sending action
 */
int main(int argc, char *argv[]);

/***** Function Implementations *****/

int main(int argc, char *argv[]) {
    int file; // File descriptor
    int value; // For returned value
    int write_val; //For returned value from writing
    unsigned long channel_id;

    if (argc != 4) {
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

    write_val = write(file, argv[3], strlen(argv[3]));

    if (write_val != strlen(argv[3])) {
        perror("Error sending message");
        fprintf(stderr, "Error sending message");
        close(file);
        exit(1);
    }

    close(file);
    exit(0);
}