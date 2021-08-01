// Created by Dvir Ben Asuli 318208816 on 24/12/2020.
// Tel-Aviv University, Operating Systems Course Assignment 4, 2021a
//

#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <pthread.h>
#include <stdbool.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>


/******************* FIFO Implementation *******************/
// This FIFO implementation is borrowed from:
// https://www.geeksforgeeks.org/queue-linked-list-implementation/

static int empty_queue = 1;

// A structure to represent a queue
// A linked list (LL) node to store a queue entry
struct QNode {
    char* key;
    struct QNode* next;
};

// The queue, front stores the front node of LL and rear stores the
// last node of LL
struct Queue {
    struct QNode *front, *rear;
};

// A utility function to create a new linked list node.
struct QNode* newNode(char* k)
{
    struct QNode* temp = (struct QNode*)malloc(sizeof(struct QNode));
    temp->key = k;
    temp->next = NULL;
    return temp;
}

// A utility function to create an empty queue
struct Queue* createQueue()
{
    struct Queue* q = (struct Queue*)malloc(sizeof(struct Queue));
    q->front = q->rear = NULL;
    return q;
}

// The function to add a key k to q
void enQueue(struct Queue* q, char* k)
{
    // Create a new LL node
    empty_queue = 0;
    struct QNode* temp = newNode(k);

    // If queue is empty, then new node is front and rear both
    if (q->rear == NULL) {
        q->front = q->rear = temp;
        return;
    }

    // Add the new node at the end of queue and change rear
    q->rear->next = temp;
    q->rear = temp;
}

// Function to remove a key from given queue q
char* deQueue(struct Queue* q)
{
    char* front_key;
    // If queue is empty, return NULL.
    if (q->front == NULL) {
        empty_queue = 1;
        return NULL;
    }

    // Store previous front and move front one node ahead
    struct QNode* temp = q->front;
    front_key = q->front->key;
    q->front = q->front->next;

    // If front becomes NULL, then change rear also as NULL
    if (q->front == NULL)
        q->rear = NULL;

    free(temp);
    return front_key;
}

/***********************************************************/

/***** Locks definitions *****/
pthread_rwlock_t thread_errors_cntr_lock;
pthread_rwlock_t counter_lock;
pthread_mutex_t all_threads_initiated_lock;
pthread_mutex_t queue_is_free_lock;
pthread_mutex_t sleeping_threads_indication_lock;
pthread_cond_t all_threads_initiated;
pthread_cond_t queue_not_empty;
pthread_cond_t new_sleeping_thread;

/***** Variables Declarations *****/
char *searched_str;
char *original_directory;
int thread_errors_cntr = 0; // How many errors while opening threads
int all_threads_are_error;
static int run_ended;
static int sleeping_threads_available;
static int threads_limit;
static int living_threads;
static int counter;
static int can_start;
struct Queue directories_queue;
pthread_t* threads;

/***** Functions Declarations *****/
/**
 * @purpose Check if all available threads are in error mode.
 * @return 1 iff it's true.
 */
int check_thread_errors();

/**
 * @purpose Check if all available threads are sleeping.
 * @return 1 iff it's true.
 */
int check_thread_sleeping();

/**
 * @purpose Check if there are any more available threads for running.
 * @return 1 iff there is none.
 */
int check_threads_running();

/**
 * @purpose Function describing the constant loop every thread will make
 */
void* thread_routine();

/**
 * @purpose opening all the threads
 */
void initiate_threads();

/**
 * @purpose Scan a directory for more directories or files
 * @param directory- the path to the directory we want to scan
 * @return 1 if an error occured.
 */
int folder_scan(char* directory);

/**
 * @purpose Initialize the queue, locks, condition variables and threads.
 */
void run();


/***** Functions Implementations *****/

int check_thread_errors(){
    int max_errors = 0;

    //-------//
    pthread_rwlock_rdlock(&thread_errors_cntr_lock);
        if (thread_errors_cntr == threads_limit ) {
            all_threads_are_error = 1;
            max_errors = 1;
        } else {
            max_errors = 0;
        }

    pthread_rwlock_unlock(&thread_errors_cntr_lock);
    //-------//

    return max_errors;
}

int check_thread_sleeping() {
    int max_sleeping = 0;

    //-------//
    pthread_mutex_lock(&sleeping_threads_indication_lock);

      if (sleeping_threads_available == threads_limit +1 ) {
            max_sleeping = 1;
      } else {
          max_sleeping = 0;
      }

    pthread_mutex_unlock(&sleeping_threads_indication_lock);
    //-------//

    return max_sleeping;
};

int check_threads_running() {
    int all_threads_finished = 0;
    int sleeping_threads;
    int errored_threads;

    //-------//
    pthread_rwlock_rdlock(&thread_errors_cntr_lock);
        errored_threads= thread_errors_cntr;
    pthread_rwlock_unlock(&thread_errors_cntr_lock);
    //-------//

    //-------//
    pthread_mutex_lock(&sleeping_threads_indication_lock);
        sleeping_threads = sleeping_threads_available;
    pthread_mutex_unlock(&sleeping_threads_indication_lock);
    //-------//

    //If some threads died to to an error we need to compare the sleeping threads
    //counter to he actual number of "living" threads only
    if ((errored_threads > 0) && (sleeping_threads == living_threads + 1)) {
        all_threads_finished = 1;
    }

    return (check_thread_sleeping() || check_thread_errors() || all_threads_finished);

}

void* thread_routine() {
    char *curr_directory;
    int scan_res;

    //Waiting for all threads to be initiated before running
    //-------//
    pthread_mutex_lock(&all_threads_initiated_lock);
        while (!can_start) {
            pthread_cond_wait(&all_threads_initiated, &all_threads_initiated_lock);
        }
    pthread_mutex_unlock(&all_threads_initiated_lock);
    //-------//

    //Only after all threads are created we'll get here

    while (!run_ended) {

        //Every time we enter here we need to be sure there is a sleeping
        //thread available for use

        pthread_mutex_lock(&sleeping_threads_indication_lock);
        while (sleeping_threads_available == 0){
            pthread_cond_wait(&new_sleeping_thread, &sleeping_threads_indication_lock);
        }
        pthread_mutex_unlock(&sleeping_threads_indication_lock);

        //-------//
        pthread_mutex_lock(&queue_is_free_lock);
            curr_directory = deQueue(&directories_queue);
        pthread_mutex_unlock(&queue_is_free_lock);
        //-------//

        if (curr_directory != NULL){
            //The FIFO is not empty, we need to handle the directory

            scan_res = folder_scan(curr_directory);
            if (scan_res == 1) {
                //Scanning completed unsuccessfully

                //-------//
                pthread_rwlock_wrlock(&thread_errors_cntr_lock);
                    thread_errors_cntr++;
                    living_threads--;
                pthread_rwlock_unlock(&thread_errors_cntr_lock);
                //-------//

                pthread_cond_signal(&new_sleeping_thread);
                pthread_exit(NULL);
            }

        } else {
            //The queue is empty, we need to wait for new data to be added

            //Thread going to sleep
            //-------//
            pthread_mutex_lock(&sleeping_threads_indication_lock);
                sleeping_threads_available++;
                pthread_cond_broadcast(&new_sleeping_thread);
            pthread_mutex_unlock(&sleeping_threads_indication_lock);
            //-------//

            //--------------//
            pthread_mutex_lock(&queue_is_free_lock);

                //There is a new item added to FIFO that we can scan
                pthread_cond_wait(&queue_not_empty, &queue_is_free_lock);

                //Marking the thread is no longer sleeping
                //-------//
                pthread_mutex_lock(&sleeping_threads_indication_lock);
                    sleeping_threads_available--;
                pthread_mutex_unlock(&sleeping_threads_indication_lock);
                //-------//

            pthread_mutex_unlock(&queue_is_free_lock);
            //--------------//
            //Upon realising from the lock well go back to the start of the while loop
        }

    }
}

void initiate_threads(){
    enQueue(&directories_queue,original_directory);

    for (int dbzy = 0; dbzy < threads_limit; dbzy++) {
        pthread_create(&(threads[dbzy]), NULL, &thread_routine, NULL);

        //-------//
        pthread_rwlock_wrlock(&thread_errors_cntr_lock);
            living_threads++;
        pthread_rwlock_unlock(&thread_errors_cntr_lock);
        //-------//
    }

    //-------//
    pthread_mutex_lock(&all_threads_initiated_lock);
        can_start = 1;
        pthread_cond_broadcast(&all_threads_initiated);
    pthread_mutex_unlock(&all_threads_initiated_lock);
    //-------//

    //-------//
    pthread_mutex_lock(&sleeping_threads_indication_lock);
        sleeping_threads_available++;
        pthread_cond_broadcast(&new_sleeping_thread);
    pthread_mutex_unlock(&sleeping_threads_indication_lock);
    //-------//

}


int folder_scan(char* directory){
    struct dirent *current_dir;
    DIR *current_DIR;
    char *slash = "/";
    char result[PATH_MAX];
    int res = 0;
    int dir_length = strlen(directory);
    struct stat dir_metadata;
    int stat_res;
    int directory_mode;

    current_DIR = opendir(directory);
    if (current_DIR == NULL) {
        fprintf(stderr, "Invalid directory\n");
        return 1;
    }

    while (!run_ended) {

        current_dir = readdir(current_DIR);

        if (current_dir == NULL) {
            break;
        }
        //Formatting the strings to a valid path string
        strcpy(result, directory);

        if (directory[dir_length-1] != '/'){
            strcat(result, slash);
        }

        strcat(result, current_dir->d_name);

      stat_res = lstat(result, &dir_metadata);
        if (stat_res == -1) {
            fprintf(stderr, "Stat function failed\n");
            res = 1;
        }

        directory_mode = S_ISDIR(dir_metadata.st_mode);

        if ((strcmp(current_dir->d_name, ".") == 0) || (strcmp(current_dir->d_name, "..") == 0)) {
            //We don't need to explore these directories to avoid endless loops
            continue;
        } else {
            if (directory_mode == 1) {
                //Directory is a directory
                int queue_was_empty = empty_queue;

                char *new_directory = malloc(sizeof(char) * PATH_MAX);
                strcpy(new_directory, result);

                //-------//
                pthread_mutex_lock(&queue_is_free_lock);
                    enQueue(&directories_queue,new_directory);
                pthread_mutex_unlock(&queue_is_free_lock);
                //-------//

                //-------//
                pthread_mutex_lock(&queue_is_free_lock);
                    pthread_cond_broadcast(&queue_not_empty);
                pthread_mutex_unlock(&queue_is_free_lock);
                //-------//

                //-------//
                pthread_mutex_lock(&queue_is_free_lock);
                    if ((queue_was_empty) && (sleeping_threads_available > 0)) {
                        //In the unique case  the queue was empty before inserting current directory
                        //If we have another available sleeping thread to use for searching the queue
                        //we are telling the CPU to put the current thread in last priority so another
                        //sleeping thread will take care of this directory
                        sched_yield();
                    }
                pthread_mutex_unlock(&queue_is_free_lock);
                //-------//


            } else {
                //Directory is actually a file
                if (strstr(current_dir->d_name, searched_str) != NULL) {

                    //-------//
                    pthread_rwlock_wrlock(&counter_lock);
                        counter++;
                    pthread_rwlock_unlock(&counter_lock);
                    //-------//

                    printf("%s\n", result);
                }
            }
        }
        //Going to next entry in the folder
    }

    closedir(current_DIR);

    return res;
}

void run(){

    //Creating the queue and locks
    directories_queue = *createQueue();
    pthread_rwlock_init(&thread_errors_cntr_lock, NULL);
    pthread_mutex_init(&all_threads_initiated_lock, NULL);
    pthread_mutex_init(&queue_is_free_lock, NULL);
    pthread_rwlock_init(&counter_lock, NULL);
    pthread_mutex_init(&sleeping_threads_indication_lock, NULL);
    pthread_cond_init(&all_threads_initiated, NULL);
    pthread_cond_init(&queue_not_empty, NULL);
    pthread_cond_init(&new_sleeping_thread, NULL);

    //Creating all the threads
    initiate_threads();
}

int main(int argc, char *argv[]) {
    char *ptr; //Redundant

    if (argc != 4){
        fprintf(stderr, "Invalid command\n");
        exit(1);
    }

    //Setting parameters
    searched_str = argv[2];
    original_directory = argv[1];
    threads_limit = strtol(argv[3], &ptr, 10);
    threads = malloc(sizeof(pthread_t) * threads_limit);

    //Now we'll initiate all the queue, the condition Variables, locks, and threads
    run();

    //After all threads were sent to work we'll wait for them to finish
    while (!run_ended) {
        //If all threads are in error mode
        if (check_threads_running() == 1){
            //All threads are either dead or sleeping
            // We can continue finalizing the program towards finish
            break;
        }

        //If there are threads still running we want to wait until a new
        //process will go to sleep, maybe we reached the limit this time

        //-------//
        pthread_mutex_lock(&sleeping_threads_indication_lock);
            pthread_cond_wait(&new_sleeping_thread, &sleeping_threads_indication_lock);
        pthread_mutex_unlock(&sleeping_threads_indication_lock);
        //-------//

    }

    run_ended = 1;

    printf("Done searching, found %d files\n", counter);

    if (all_threads_are_error == 1){
        exit(1);
    }

    exit(0);
}