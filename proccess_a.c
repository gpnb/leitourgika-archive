/* pshm_ucase_bounce.c

    Licensed under GNU General Public License v2 or later.
*/
#include <ctype.h>
#include <pthread.h>

#include "common.h"

void * writer_func_a(void * memseg);

void * reader_func_a(void * memseg);

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

    // printf("Shared memory object \"%s\" has been created at address\"%p\"\n", shmpath, shmp);
    
    close(fd);          /* 'fd' is no longer needed */
    /* Initialize semaphores as process-shared, with value 0. */

    if (sem_init(&shmp->wa, 1, 0) == -1)
        errExit("sem_init-sem1");
    if (sem_init(&shmp->wb, 1, 0) == -1)
        errExit("sem_init-sem1");
    if (sem_init(&shmp->ra, 1, 0) == -1)
        errExit("sem_init-sem2");
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
    int res2;
    pthread_t reader_a;
    void *thread_result2;
    res2 = pthread_create(&reader_a, NULL, reader_func_a, (void *)shmp);
    if (res2 != 0) {
        perror("Thread creation failed");
        exit(EXIT_FAILURE);
    }    


    
    res2 = pthread_join(reader_a, &thread_result2);
    if (res2 != 0) {
        perror("Thread join failed");
        exit(EXIT_FAILURE);
    }
    printf("sending back char\n");
    pthread_cancel(writer_a);
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
        ch = getchar();

        /* Wait for 'sem1' to be posted by peer before touching
            shared memory. */
        if (sem_wait(&shmp->wa) == -1)
            errExit("sem_wait");
        sem_trywait(&shmp->wb); // lock writer b as well (evil)

        if (shmp->term == 1) {
            flag = 1;
            break;
        }


        shmp->pos = 0;
        for (size_t j = 0; j < shmp->cnt; j++){
            
            if (ch == EOF) {
                flag = 1;
                break;
            }
            shmp->buf[j] = ch;
            shmp->pos++;
            if (ch == '\n') {
                break;
            }
            if (j < shmp->cnt - 1) {
                ch = getchar();
            }
        }
    
        // give the programm to the other person
        if (sem_post(&shmp->rb) == -1) 
            errExit("sem_post");
    }

    // a final routine to end the communication
    // if (sem_wait(&shmp->wa) == -1)
    //     errExit("sem_wait");
    
    shmp->buf[0] = EOF;

    if(sem_post(&shmp->rb) == -1)
        errExit("sem_post");

    printf("writer a ded\n");
    return 0;
}

// don't you love copy-paste?
void * reader_func_a(void * memseg) {
    struct shmbuf *shmp = memseg;

    int flag = 0;
    int initer = 1;
    while (flag == 0) { // not over
        if (sem_wait(&shmp->ra) == -1)
            errExit("sem_wait");

        if (shmp->term == 1) {
            flag = 1;
            break;
        }

        if (shmp->buf[0] == EOF) {
            flag = 1;
        }
        char endc[5] = "#BYE#";
        int k = 0;
        for (int j = 0; j < shmp->pos; j++) {
            if (shmp->buf[j] == endc[k]) {
                k++;
            }
            else {
                k = 0;
            }
            if (k == 5) {
                printf("found end string\n");
                flag = 1;
                shmp->term = 1;
                break;
            }
        }
        if (initer == 1) {
            printf("PROCB >>    ");
            initer = 0;
        }
        int fl = 0;
        if (shmp->pos != 0) {
            for (int j = 0; j < shmp->pos; j++) {
                printf("%c", shmp->buf[j]);
                if (shmp->buf[j] == '\n') {
                    fl = 1;
                    initer = 1;
                }
            }
        }

        if (sem_post(&shmp->wb) == -1)
            errExit("sem_post");
        if (fl == 1) {
            sem_post(&shmp->wa);
        }
    }
    shmp->pos = 1;
    if (sem_post(&shmp->rb) == -1)
        errExit("sem_post");
    printf("reader a ded\n");
    return 0;
}