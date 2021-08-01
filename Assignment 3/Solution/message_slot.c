 // Created by Dvir Ben Asuli 318208816 on 29/11/2020.
// Tel-Aviv University, Operating Systems Course Assignment 3, 2021a
//

#include "message_slot.h"

#include <linux/kernel.h>   /* We're doing kernel work */
#include <linux/module.h>   /* Specifically, a module */
#include <linux/fs.h>       /* for register_chrdev */
#include <linux/uaccess.h>  /* for get_user and put_user */
#include <linux/string.h>   /* for memset. NOTE - not string.h!*/
#include <linux/slab.h>     /* for allocations */

MODULE_LICENSE("GPL");

/***** Defines *****/

typedef struct private_data
{
    int minor_num;
    unsigned long channel_num;

} private_data_s;

//The messages passed with the read and write actions
typedef struct data { char data_message[BUFFER_LENGTH]; } data_t;

//A single message channel containing it's ID and message
typedef struct message_channel {
    //A message channel as a linked list node
    char *message;
    int size;
    unsigned int channel_id;
    struct message_channel *next_node;
} message_channel;

typedef struct message_slot {
    //An object that will point only to the root of message channels linked list
    struct message_channel *channels_root;
} message_slot_s;


//An array for all the different message slot devices indexed
//from 0-255
typedef struct message_slots_devices{
    message_slot_s* minors[MINORS_LIMIT];
    int *numOfChannels[MINORS_LIMIT]; //How many channels are allocated for each device
} message_slots_devices;

static message_slots_devices *devices = NULL;
static int numOfDrivers = 0;

/***** Enums *****/


/***** Declarations *****/
/**
 * @purpose Open a message slot device
 * @param inode - Inode of the allocated device-file
 * @param file - File struct of the allocated device-file
 * @return 0 on success
 */
static int device_open( struct inode* inode, struct file*  file);

/**
 * @purpose Reads the last message written on the channel into the user’s buffer.
 * @param file - file descriptor of the device-file
 * @param buffer - Pointer to the buffer in which the message will be stored
 * @param length - length of the buffer
 * @param Offset - offset in the file in which the reading will start
 * @return the number of bytes read, unless an error occurs
 */
static ssize_t device_read(struct file* file, char __user* buffer, size_t length, loff_t* offset);

/**
 * @purpose Sets a message from user buffer to message slot
 * @param file - file descriptor of the device-file
 * @param buffer - Pointer to the buffer in which the message will be stored
 * @param length - length of the buffer
 * @param Offset - offset in the file in which the reading will start
 * @return the number of bytes written, unless an error occurs
 */
static ssize_t device_write(struct file* file, const char __user* buffer, size_t length, loff_t* offset);

/**
 * @purpose release a device registered with module
 * @param inode - inode describing the device "file"
 * @param file - the device "file
 * @return 0 on success
 */
static int device_release(struct inode* inode, struct file*  file);

/**
 * @purpose Setting channel ID for future transactions
 * @param file- pointer to file
 * @param ioctl_command_id - type of special command
 * @param ioctl_param - desired channel ID
 * @return 0 on success
 */
static long ioctl(struct file* file, unsigned int ioctl_command_id, unsigned long  ioctl_param );

/**
 * @purpose Create a new message channel
 * @param channel_id - desired ID of new channel
 * @return Node struct of the new channel
 */
static message_channel* initiate_channel(unsigned int channel_id);

/**
 * @purpose Fine a message channel by id inside a given device
 * @param root - the first channel struct node in channels linked list
 * @param channel_id - ID of the desired channel
 * @return Node struct of the desired channel
 */
static message_channel* find_channel(struct message_channel* root, unsigned int channel_id);

/**
 * @purpose Add a new node representing a new channel inside a linked list
 * @param root - the first channel struct node in channels linked list
 * @param new_node - Node we want to add
 */
static void add_node(struct message_channel* root,struct message_channel* new_node );

/**
 * @purpose Initialize Module and make it known to kernel
 * @return 0 on success, something else otherwise
 */
static int __init prepare(void);

/**
 * @purpose The skeleton calls this function before the first invocation of process_arglist()
 *          for any initialization and setup that are necessary for this function
 */
static void __exit finalize(void);

struct file_operations Fops =
        {
                .open = device_open,
                .read = device_read,
                .write = device_write,
                .release = device_release,
                .unlocked_ioctl = ioctl,
                .owner = THIS_MODULE,
        };

/***** Function Implementations *****/

static message_channel* initiate_channel(unsigned int channel_id) {
    message_channel *channel = (message_channel*) kmalloc(sizeof(message_channel), GFP_KERNEL);
    memset(channel, 0, sizeof(message_channel));

    if (channel == NULL) {
        return NULL;
    }

    channel->channel_id = channel_id;
    channel->size = 0;
    channel->message = (char*) kmalloc(sizeof(char) * BUFFER_LENGTH, GFP_KERNEL);
    memset(channel->message, 0, sizeof(char) * BUFFER_LENGTH);
    channel->next_node = NULL;

    return channel;
}

static message_channel* find_channel(struct message_channel* root, unsigned int channel_id) {
    message_channel *node = root;
    while (node != NULL) {
        if (node->channel_id == channel_id) {
            return node;
        } else {
            node = node-> next_node;
        }
    }
    return NULL;
}

static void add_node(struct message_channel* root,struct message_channel* new_node ){
    message_channel *node = root;
    while (node->next_node != NULL) {
        node = node->next_node;
    }
    node->next_node = new_node;
}

static int device_open( struct inode* inode, struct file*  file) {
    int minor_num = iminor(inode);
    private_data_s *dbzy;
    message_slot_s *slot;
    message_slot_s *current_slot;

    dbzy = (private_data_s*) kmalloc(sizeof(private_data_s), GFP_KERNEL);

    if (dbzy == 0){
        return 1;
    }

    dbzy->minor_num = minor_num;
    file->private_data = (void *)dbzy;

    slot = devices->minors[minor_num];
    if (slot == NULL) {
        //Slot device not existent for this minor yet
        if (numOfDrivers == MINORS_LIMIT) {
            //Checking if it's ok to add another device
            return -ENOSPC;
        }

        //New device could be added
        current_slot = (message_slot_s*) kmalloc(sizeof(message_slot_s), GFP_KERNEL);
        memset(current_slot, 0, sizeof(message_slot_s));
        if (current_slot == NULL) {
            return -ENOMEM;
        }

        current_slot->channels_root = NULL;
        devices->minors[minor_num] = current_slot;
        numOfDrivers++;
        return 0;
    }

    return 0;


}

static ssize_t device_read(struct file* file, char __user* buffer, size_t length, loff_t* offset) {
    data_t read_message;
    struct message_channel* curr_channel;
    int mrtbka; //For iterations
    int put_res; //to check if put_user method works
    int message_size;
    private_data_s *file_data = (private_data_s *)file->private_data;
    int minor_number = file_data->minor_num;
    int channel_num = file_data->channel_num;

    if (buffer == NULL){
        return -EINVAL;
    } else if (length == 0 || length > BUFFER_LENGTH) {
        return -EMSGSIZE;
    }

    curr_channel = find_channel(devices->minors[minor_number]->channels_root, channel_num);
    message_size = curr_channel->size;

    if (curr_channel  == NULL){
        //Channel is fugazi
        return -EINVAL;
    } else if (curr_channel->message == NULL){
        //No message in the channel
        return -EWOULDBLOCK;
    } else if (curr_channel->size == 0) {
        //No message in the channel
        return -EWOULDBLOCK;
    } else if (message_size > length){
        //The provided buffer length is too small to hold the last message written on the channel
        return -ENOSPC;
    }

    for (mrtbka = 0; mrtbka < message_size ; ++mrtbka) {
        put_res = put_user(curr_channel->message[mrtbka], &buffer[mrtbka]);
        if (put_res == -EFAULT){
            return put_res;
        }
    }
    return mrtbka;

}

static ssize_t device_write(struct file* file, const char __user* buffer, size_t length, loff_t* offset) {

    private_data_s *file_data = (private_data_s *)file->private_data;
    int minor_number = file_data->minor_num;
    int channel_num = file_data->channel_num;
    struct message_channel* curr_channel;
    data_t *written_message;
    int mrtbka; //For iterations

    if (buffer == NULL || channel_num  == 0){
        //Buffer is fugazi or no channel num was passed
        return -EINVAL;
    } else if (length == 0 || length > BUFFER_LENGTH) {
        //Length of message is 0 or bigger than limit
        return -EMSGSIZE;
    }

    written_message = (data_t*) kmalloc(sizeof(data_t), GFP_KERNEL);
    for (mrtbka = 0; mrtbka < length ; ++mrtbka) {
        //Getting the desired message from the user
        get_user(written_message->data_message[mrtbka], &buffer[mrtbka]);
    }

    curr_channel = find_channel(devices->minors[minor_number]->channels_root, channel_num);

    if (curr_channel == NULL) {
        //channel is fugazi
        return -EINVAL;
    } else {
        //Writing message to channel
        curr_channel->size = mrtbka;
        curr_channel->message = written_message->data_message;

    }

    return mrtbka;

}

static int device_release(struct inode* inode, struct file*  file) {

    private_data_s *dbzy = (private_data_s *)file->private_data;
    kfree(dbzy);

    return 0;
}

static long ioctl(struct file* file, unsigned int ioctl_command_id, unsigned long  ioctl_param )
{
    int minor_num;
    message_slot_s* curr_slot;
    struct message_channel* root;
    struct message_channel* curr_channel;
    unsigned long channel_num = ioctl_param;

    if ( ioctl_param == 0 || ioctl_command_id != MSG_SLOT_CHANNEL) {
        //Checking validity of input first
        return -EINVAL;
    }

    ((private_data_s *)file->private_data)->channel_num = ioctl_param;
    minor_num = iminor(file->f_inode);

    //Checking if message slot exists
    curr_slot = devices->minors[minor_num];

    if (curr_slot == NULL) {
        return -EINVAL;
    }

    //Checking if a message channel exists
    root = curr_slot->channels_root;

    if (root == NULL) {
        //The messagle slot is completey empty
        curr_channel = initiate_channel(channel_num);

        if (curr_channel == NULL) {

            printk(KERN_ERR "% Channel cannot be created");
            return -1;
        }

        devices->numOfChannels[minor_num]++;

        //curr_slot->channels_root = (message_channel*) kmalloc(sizeof(message_channel), GFP_KERNEL);
        curr_slot->channels_root = curr_channel;
        devices->numOfChannels[minor_num]++;
        return 0;
    }

    //Some channels exists in Message Slot
    curr_channel = find_channel(root, channel_num);

    if (curr_channel == NULL) {
        //No channel for this channel ID exists
        //Need to create a new node for this channel
        if (devices->numOfChannels[minor_num] >= CHANNELS_LIMIT) {
            //Creating new channel is forbidden
            printk(KERN_ERR "%Channel cannot be created");
            return -1;
        }

        curr_channel = initiate_channel(channel_num);

        if (curr_channel == NULL) {
            printk(KERN_ERR "%Channel cannot be created");
            return -1;
        }

        //We know for a fact 'root' is not NULL
        add_node(root, curr_channel);
        devices->numOfChannels[minor_num]++;
    }

    ((private_data_s *)file->private_data)->channel_num = channel_num;
    return 0;




}

static int __init prepare(void){
    static int numOfDrivers = 0;
    int registerRes = register_chrdev(DEV_CODE, "message_slot", &Fops);
    int dbzy = 0;
    if (registerRes < 0) {
        printk(KERN_ERR "%Error registering device");
        return registerRes;
    } else {
        devices = (message_slots_devices*) kmalloc(sizeof(message_slots_devices), GFP_KERNEL);
        for (dbzy = 0; dbzy < MINORS_LIMIT; dbzy++){
            devices->minors[dbzy] = ( message_slot_s*) kmalloc(sizeof( message_slot_s), GFP_KERNEL);
            devices->minors[dbzy] = NULL;
            devices->numOfChannels[dbzy] = 0;
        }
        return 0;
    }
}

static void __exit finalize(void){

    int dbzy = 0;
    while (dbzy< MINORS_LIMIT) {
        if (devices->minors[dbzy] != NULL) {
            //Going through each linked list
            message_slot_s *current_slot = devices->minors[dbzy];
            message_channel *node = current_slot->channels_root;
            message_channel *next_node;

            while (node != NULL){
                //Eliminating all nodes in linked list
                next_node = node->next_node;
                node->size = 0;
                node->channel_id = 0;
                kfree(node->message);
                kfree(node);
                node = next_node;
            }

            kfree(current_slot);
            numOfDrivers--;

        }
        dbzy++;
    }

    kfree(devices);
    unregister_chrdev(DEV_CODE, "message_slot");
}


module_init(prepare);
module_exit(finalize);
