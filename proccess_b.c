/* pshm_ucase_send.c

    Licensed under GNU General Public License v2 or later.
*/
#include <string.h>
#include <pthread.h>

#include "common.h"

void * reader_func_b(void * memseg);

int
main(int argc, char *argv[])
{
    int            fd;
    char           *shmpath, *string;
    size_t         len;
    struct shmbuf  *shmp;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s /shm-path string\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    shmpath = argv[1];
    string = "no";
    //string = argv[2];
    //len = strlen(string);
    len = 15;

    //if (len > BUF_SIZE) {
    //    fprintf(stderr, "String is too long\n");
    //    exit(EXIT_FAILURE);
    //}

    /* Open the existing shared memory object and map it
        into the caller's address space. */

    fd = shm_open(shmpath, O_RDWR, 0);
    if (fd == -1)
        errExit("shm_open");

    shmp = mmap(NULL, sizeof(*shmp), PROT_READ | PROT_WRITE,
                MAP_SHARED, fd, 0);
    if (shmp == MAP_FAILED)
        errExit("mmap");

    printf("Shared memory object \"%s\" has been created at address\"%p\"\n", shmpath, shmp);

    close(fd);          /* 'fd' is no longer needed */


    //#### critical section begin ####

    printf("waiting for peer proccess:\n");

    if (sem_wait(&shmp->rb) == -1)
        errExit("sem_wait");

    printf("Ready (writing esssential data)\n");
    /* Copy data into the shared memory object. */

    shmp->cnt = len;
    memcpy(&shmp->buf, string, len);

    // go back to the other proccess
    if (sem_post(&shmp->wa) == -1)
        errExit("sem_post");


    int res;
    pthread_t reader_b;
    void *thread_result;
    res = pthread_create(&reader_b, NULL, reader_func_b, (void *)shmp);
    if (res != 0) {
        perror("Thread creation failed");
        exit(EXIT_FAILURE);
    }
    //reader_func_b(shmp);
    sleep(5);
    printf("waiting for thread to finish...\n");
    res = pthread_join(reader_b, &thread_result);
    if (res != 0) {
        perror("Thread join failed");
        exit(EXIT_FAILURE);
    }
    printf("Thread was successfull!!!\n");


    write(STDOUT_FILENO, "#### END OF PROCCESS ####\n", 26);

    exit(EXIT_SUCCESS);
}

void * reader_func_b(void * memseg) {
    struct shmbuf *shmp = memseg;

    int flag = 0;
    while (flag == 0) {
        //printf("waitng for peer proccess to print\n");

        /* Wait until peer says that it has finished accessing
        the shared memory. */

        if (sem_wait(&shmp->rb) == -1)
            errExit("sem_wait");

        /* Write modified data in shared memory to standard output. */
        if (shmp->buf[0] == EOF) {
            flag = 1;
        }
        if (shmp->pos != 0) {
            //write(STDOUT_FILENO, &shmp->buf, len);
            for (int j = 0; j < shmp->pos; j++) {
                printf("%c", shmp->buf[j]);
            }
            //printf("\n");
            //write(STDOUT_FILENO, "\n", 1);
        }

        if (sem_post(&shmp->wa) == -1)
            errExit("sem_post");
    }
    return 0;
}