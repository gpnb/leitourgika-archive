/* pshm_ucase_bounce.c

    Licensed under GNU General Public License v2 or later.
*/
#include <ctype.h>
#include <pthread.h>

#include "common.h"

void * writer_func_a(void * memseg);

int
main(int argc, char *argv[])
{
    int            fd;
    char           *shmpath;
    struct shmbuf  *shmp;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s /shm-path\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    shmpath = argv[1];

    /* Create shared memory object and set its size to the size
        of our structure. */

    fd = shm_open(shmpath, O_CREAT | O_EXCL | O_RDWR, 0600);
    if (fd == -1)
        errExit("shm_open");

    if (ftruncate(fd, sizeof(struct shmbuf)) == -1)
        errExit("ftruncate");

    /* Map the object into the caller's address space. */

    shmp = mmap(NULL, sizeof(*shmp), PROT_READ | PROT_WRITE,
                MAP_SHARED, fd, 0);
    if (shmp == MAP_FAILED)
        errExit("mmap");

    printf("Shared memory object \"%s\" has been created at address\"%p\"\n", shmpath, shmp);
    
    close(fd);          /* 'fd' is no longer needed */
    /* Initialize semaphores as process-shared, with value 0. */

    if (sem_init(&shmp->wa, 1, 0) == -1)
        errExit("sem_init-sem1");
    if (sem_init(&shmp->rb, 1, 0) == -1)
        errExit("sem_init-sem2");


    // start with proccess 2 writing the essentials.
    if (sem_post(&shmp->rb) == -1)
        errExit("sem_post");

    printf("remember, eof is ctlr+d\n");


    int res;
    pthread_t writer_a;
    void *thread_result;
    res = pthread_create(&writer_a, NULL, writer_func_a, (void *)shmp);
    if (res != 0) {
        perror("Thread creation failed");
        exit(EXIT_FAILURE);
    }
    //writer_func_a(shmp);
    sleep(1);
    printf("waiting for thread to finish...\n");
    res = pthread_join(writer_a, &thread_result);
    if (res != 0) {
        perror("Thread join failed");
        exit(EXIT_FAILURE);
    }
    printf("Thread was successfull!!!\n");
    //exit(EXIT_SUCCESS);




    /* Unlink the shared memory object. Even if the peer process
        is still using the object, this is okay. The object will
        be removed only after all open references are closed. */

    shm_unlink(shmpath);

    printf("#### END OF PROCCESS ####\n");

    exit(EXIT_SUCCESS);
}

void * writer_func_a(void * memseg) {
    struct shmbuf  *shmp = memseg;



    char ch;
    int flag = 0;
    // #### critical section ####
    while (flag == 0) {

        printf("Waiting for peer proccess:\n");

        /* Wait for 'sem1' to be posted by peer before touching
            shared memory. */
        if (sem_wait(&shmp->wa) == -1)
            errExit("sem_wait");


        printf("Ready:\n");


        shmp->pos = 0;
        for (size_t j = 0; j < shmp->cnt; j++){
            ch = getchar();
            // if (ch == '\n') {
            //     break;
            // }
            if (ch == EOF) {
                flag = 1;
                break;
            }
            shmp->buf[j] = ch;
            shmp->pos++;
            if (ch == '\n') {
                break;
            }
        }
        
        // printf("sending:\n");
        // for (size_t j = 0; j < shmp->cnt; j++) {
        //     printf("%c", shmp->buf[j]);
        // }
        // printf("\n");
    

        // give the programm to the other person
        if (sem_post(&shmp->rb) == -1) 
            errExit("sem_post");
    }

    // a final routine to end the communication
    if (sem_wait(&shmp->wa) == -1)
        errExit("sem_wait");
    
    shmp->buf[0] = EOF;

    if(sem_post(&shmp->rb) == -1)
        errExit("sem_post");

    return 0;
}