//
// Created by Dvir Ben Asuli 318208816 on 29/11/2020.
// Tel-Aviv University, Operating Systems Course Assignment 3, 2021a
//

#ifndef MESSAGE_SLOT_H
#define MESSAGE_SLOT_H

/***** Defines *****/
#define DEV_CODE 240
#define BUFFER_LENGTH 128
#define MINORS_LIMIT 256
#define CHANNELS_LIMIT 1048576
#define MSG_SLOT_CHANNEL _IOW(DEV_CODE, 0, unsigned long)

/***** Enums *****/

/***** Declarations *****/
/**
 * @purpose Open a message slot device
 * @param inode - Inode of the allocated device-file
 * @param file - File struct of the allocated device-file
 * @return 0 on success
 */


#endif //MESSAGE_SLOT_H
