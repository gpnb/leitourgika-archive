#include <fcntl.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

#define errExit(msg)    do { perror(msg); exit(EXIT_FAILURE); \
                        } while (0)

#define BUF_SIZE 15   /* Maximum size for exchanged string */
#define ENDSTR "#BYE#"

/* Define a structure that will be imposed on the shared
    memory object */

struct shmbuf {
    sem_t  wa;              // semaphore for writer thread in process a
    sem_t  wb;              // semaphore for writer thread in process b
    sem_t  ra;              // semaphore for reader thread in process a
    sem_t  rb;              // semaphore for reader thread in process b
    int    term;            // program termination flag shared between all threads
    int    pos;             // position of buffer, up to which a proccess has writen 
    int    ma;              // number of messages sent by process a
    int    mb;              // number of messages sent by process b
    size_t cnt;             /* Number of bytes used in 'buf' */
    char   buf[BUF_SIZE];   /* Data being transferred */
};

struct metadata {
    int rec;
    int sent;
    int pack;
    float avrg_time;
};