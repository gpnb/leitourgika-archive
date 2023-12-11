/* pshm_ucase_send.c

    Licensed under GNU General Public License v2 or later.
*/
#include <string.h>
#include <pthread.h>

#include "common.h"

void * writer_func_b(void * memseg);

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

    //printf("Shared memory object \"%s\" has been created at address\"%p\"\n", shmpath, shmp);

    close(fd);          /* 'fd' is no longer needed */


    //#### critical section begin ####

    //printf("waiting for peer proccess:\n");

    if (sem_wait(&shmp->rb) == -1)
        errExit("sem_wait");

    //printf("Ready (writing esssential data)\n");
    /* Copy data into the shared memory object. */

    shmp->cnt = len;
    shmp->term = 0;
    shmp->ma = 0;
    shmp->mb = 0;
    memcpy(&shmp->buf, string, len);

    // go back to the other proccess
    if (sem_post(&shmp->wa) == -1)
        errExit("sem_post");
    
    if (sem_post(&shmp->wb) == -1)
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
    int res2;
    pthread_t writer_b;
    void *thread_result2;
    res2 = pthread_create(&writer_b, NULL, writer_func_b, (void *)shmp);
    if (res2 != 0) {
        perror("Thread creation failed");
        exit(EXIT_FAILURE);
    }


    res = pthread_join(reader_b, &thread_result);
    if (res != 0) {
        perror("Thread join failed");
        exit(EXIT_FAILURE);
    }
    pthread_cancel(writer_b);
    res2 = pthread_join(writer_b, &thread_result2);
    if (res2 != 0) {
        perror("Thread join failed");
        exit(EXIT_FAILURE);
    }
    printf("Thread was successfull!!!\n");

    printf("#### END OF PROCCESS ####\n");

    struct metadata *met = thread_result;
    printf("sent:             %d\n", met->sent);
    printf("received:         %d\n", met->rec);
    printf("packages:         %d\n", met->pack);
    printf("average per mess: %f\n", met->pack / (float)met->rec);
    printf("average time:     %f\n", met->avrg_time);

    free(met);


    sem_close(&shmp->wa);
    sem_close(&shmp->wb);
    sem_close(&shmp->ra);
    sem_close(&shmp->rb);
    sem_destroy(&shmp->wa);
    sem_destroy(&shmp->wb);
    sem_destroy(&shmp->ra);
    sem_destroy(&shmp->rb);

    // write(STDOUT_FILENO, "#### END OF PROCCESS ####\n", 26);

    exit(EXIT_SUCCESS);
}

void * writer_func_b(void * memseg) {
    struct shmbuf  *shmp = memseg;



    char ch;
    int flag = 0;
    // #### critical section ####
    while (flag == 0) {
        ch = getchar();

        /* Wait for 'sem1' to be posted by peer before touching
            shared memory. */
        if (sem_wait(&shmp->wb) == -1)
            errExit("sem_wait");
        sem_trywait(&shmp->wa); // lock writer b as well (evil)

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
                shmp->mb++;
                break;
            }
            if (j < shmp->cnt - 1) {
                ch = getchar();
            }
        }
    
        // give the programm to the other person
        if (sem_post(&shmp->ra) == -1) 
            errExit("sem_post");
    }

    // a final routine to end the communication
    // if (sem_wait(&shmp->wb) == -1)
    //     errExit("sem_wait");
    
    shmp->buf[0] = EOF;

    if(sem_post(&shmp->ra) == -1)
        errExit("sem_post");

    printf("writer b ded\n");
    return 0;
}

void * reader_func_b(void * memseg) {
    struct shmbuf *shmp = memseg;
    time_t t = 0;
    time_t tim = 0;

    int flag = 0;
    int initer = 1;
    char temp[1024];
    int tpos = 0;
    int messagenum = 0;
    int packnum = 0;
    while (flag == 0) {
        if (sem_wait(&shmp->rb) == -1)
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
            printf("PROCA >>    ");
            messagenum++;
            initer = 0;
            if (t != 0) tim += time(NULL)-t;
        }
        int fl = 0;
        if (shmp->pos != 0) {
            packnum++;
            for (int j = 0; j < shmp->pos; j++) {
                //printf("%c", shmp->buf[j]);
                temp[tpos] = shmp->buf[j];
                tpos++;
                if (shmp->buf[j] == '\n') {
                    fl = 1;
                    initer = 1;
                    t = time(NULL);
                }
            }
        }

        if (fl == 1) {
            for(int k = 0; k < tpos; k++) {
                printf("%c", temp[k]);
            }
            tpos = 0;
        }

        if (sem_post(&shmp->wa) == -1)
            errExit("sem_post");
        if (fl == 1) {
            sem_post(&shmp->wb);
        }
    }
    shmp->pos = 1;
    
    
    float average_time = tim / (float)(messagenum-1); 
    
    struct metadata *met = malloc(sizeof(struct metadata));
    met->avrg_time = average_time;
    met->pack = packnum;
    met->rec = messagenum;
    met->sent = shmp->mb;

    if (sem_post(&shmp->ra) == -1)
        errExit("sem_post");
    pthread_exit(met);
}