#include "common.h"

void * writer_func_b(void * memseg);

void * reader_func_b(void * memseg);

void metadata_printer(struct metadata * met) {
    printf("messages sent:                                      %d\n", met->sent);
    printf("messages received:                                  %d\n", met->rec);
    printf("packages received:                                  %d\n", met->pack);
    if (met->rec == 0) return;
    printf("average package count per message received:         %f\n", met->pack / (float)met->rec);
    if (met->rec == 1) return;
    printf("average wait time for first package of new message: %f\n", met->avrg_time);
}

int main(int argc, char *argv[]) {
    int            fd;
    char           *shmpath;
    struct shmbuf  *shmp;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s /shm-path string\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    shmpath = argv[1];

    fd = shm_open(shmpath, O_RDWR, 0);
    if (fd == -1)
        errExit("shm_open");

    shmp = mmap(NULL, sizeof(*shmp), PROT_READ | PROT_WRITE,
                MAP_SHARED, fd, 0);
    if (shmp == MAP_FAILED)
        errExit("mmap");

    close(fd);


    // CREATE READER AND WRITER THREADS
    int res;
    pthread_t reader_b;
    void *thread_result;
    res = pthread_create(&reader_b, NULL, reader_func_b, (void *)shmp);
    if (res != 0) {
        perror("Thread creation failed");
        exit(EXIT_FAILURE);
    }
    pthread_t writer_b;
    void *thread_result2;
    res = pthread_create(&writer_b, NULL, writer_func_b, (void *)shmp);
    if (res != 0) {
        perror("Thread creation failed");
        exit(EXIT_FAILURE);
    }


    // END THREADS
    res = pthread_join(reader_b, &thread_result);
    if (res != 0) {
        perror("Thread join failed");
        exit(EXIT_FAILURE);
    }
    pthread_cancel(writer_b);
    res = pthread_join(writer_b, &thread_result2);
    if (res != 0) {
        perror("Thread join failed");
        exit(EXIT_FAILURE);
    }


    // FINISH THE FUNCTION
    printf("#### END OF PROCESS ####\n");

    metadata_printer(thread_result);

    free(thread_result);


    
    // FREEING SHARED MEMORY ITEMS
    sem_close(&shmp->wa);
    sem_close(&shmp->wb);
    sem_close(&shmp->ra);
    sem_close(&shmp->rb);
    sem_destroy(&shmp->wa);
    sem_destroy(&shmp->wb);
    sem_destroy(&shmp->ra);
    sem_destroy(&shmp->rb);

    shm_unlink(shmpath);

    exit(EXIT_SUCCESS);
}

void * writer_func_b(void * memseg) {
    struct shmbuf  *shmp = memseg;

    char ch;
    int flag = 0;
    while (flag == 0) {
        ch = getchar();

        if (sem_wait(&shmp->wb) == -1)
            errExit("sem_wait");
        sem_trywait(&shmp->wa); // lock writer a as well

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

        if (sem_post(&shmp->ra) == -1) 
            errExit("sem_post");
    }

    if (sem_wait(&shmp->wb) == -1)
        errExit("sem_wait");
    
    shmp->pos = 5;
    memcpy(&shmp->buf, ENDSTR, 5);
    for(int i = 5; i < 15; i++){
        shmp->buf[i] = '\0';
    }

    if (sem_post(&shmp->ra) == -1)
        errExit("sem_post");

    pthread_exit("exited");
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

        if (strcmp(shmp->buf, ENDSTR) == 0){
            flag = 1;
            shmp->term = 1;
            break;
        }

        if (initer == 1 && shmp->pos != 0) {
            printf("PROCA >>    ");
            messagenum++;
            initer = 0;
            if (t != 0) tim += time(NULL)-t;
        }
        int message_end = 0;
        if (shmp->pos != 0) {
            packnum++;
            for (int j = 0; j < shmp->pos; j++) {
                temp[tpos] = shmp->buf[j];
                tpos++;
                if (shmp->buf[j] == '\n') {
                    message_end = 1;
                    initer = 1;
                    t = time(NULL);
                }
            }
        }

        if (message_end == 1) { // end of message
            for(int k = 0; k < tpos; k++) {
                printf("%c", temp[k]);
            }
            tpos = 0;
            sem_post(&shmp->wb);
        }

        if (sem_post(&shmp->wa) == -1)
            errExit("sem_post");
    }
    
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