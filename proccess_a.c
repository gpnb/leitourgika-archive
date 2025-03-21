#include "common.h"

void * writer_func_a(void * memseg);

void * reader_func_a(void * memseg);

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
    /*###################
    #                   #
    #  INITIALIZATIONS  #
    #                   #
    ###################*/
    int            fd;
    char           *shmpath;
    struct shmbuf  *shmp;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s /shm-path\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    shmpath = argv[1];

    fd = shm_open(shmpath, O_CREAT | O_EXCL | O_RDWR, 0600);
    if (fd == -1)
        errExit("shm_open");

    if (ftruncate(fd, sizeof(struct shmbuf)) == -1)
        errExit("ftruncate");

    shmp = mmap(NULL, sizeof(*shmp), PROT_READ | PROT_WRITE,
                MAP_SHARED, fd, 0);
    if (shmp == MAP_FAILED)
        errExit("mmap");
    
    close(fd);

    if (sem_init(&shmp->wa, 1, 0) == -1)
        errExit("sem_init-sem1");
    if (sem_init(&shmp->wb, 1, 0) == -1)
        errExit("sem_init-sem1");
    if (sem_init(&shmp->ra, 1, 0) == -1)
        errExit("sem_init-sem2");
    if (sem_init(&shmp->rb, 1, 0) == -1)
        errExit("sem_init-sem2");


    shmp->cnt = 15;
    shmp->term = 0;
    shmp->ma = 0;
    shmp->mb = 0;
    /*##########################
    #                          #
    #  END OF INITIALIZATIONS  #
    #                          #
    ##########################*/


    // start with both proccesses being able to write
    if (sem_post(&shmp->wa) == -1)
        errExit("sem_post");
    
    if (sem_post(&shmp->wb) == -1)
        errExit("sem_post");

    // message for user
    printf("Both proccesses end with EOF\n");
    printf("In linux, EOF is CTRL+D\n");


    // CREATE WRITER AND READER THREADS
    int res;
    pthread_t writer_a;
    void *thread_result;
    res = pthread_create(&writer_a, NULL, writer_func_a, (void *)shmp);
    if (res != 0) {
        perror("Thread creation failed");
        exit(EXIT_FAILURE);
    }
    pthread_t reader_a;
    void *thread_result2;
    res = pthread_create(&reader_a, NULL, reader_func_a, (void *)shmp);
    if (res != 0) {
        perror("Thread creation failed");
        exit(EXIT_FAILURE);
    }    

    
    // END THREADS
    res = pthread_join(reader_a, &thread_result2);
    if (res != 0) {
        perror("Thread join failed");
        exit(EXIT_FAILURE);
    }
    pthread_cancel(writer_a); // when the reader thread ends, stop the writer thread
    res = pthread_join(writer_a, &thread_result);
    if (res != 0) {
        perror("Thread join failed");
        exit(EXIT_FAILURE);
    }

    // FINISH THE FUNCTION
    printf("#### END OF PROCESS ####\n");

    metadata_printer(thread_result2);

    free(thread_result2);

    exit(EXIT_SUCCESS);
}

void * writer_func_a(void * memseg) {
    struct shmbuf  *shmp = memseg;

    char ch;
    int flag = 0;
    while (flag == 0) {
        ch = getchar();

        if (sem_wait(&shmp->wa) == -1)
            errExit("sem_wait");
        sem_trywait(&shmp->wb); // lock writer b as well

        shmp->pos = 0;
        for (size_t j = 0; j < shmp->cnt; j++){
            
            if (ch == EOF) {
                flag = 1;
                break;
            }
            shmp->buf[j] = ch;
            shmp->pos++;
            if (ch == '\n') {
                shmp->ma++;
                break;
            }
            if (j < shmp->cnt - 1) {
                ch = getchar();
            }
        }
    
        if (sem_post(&shmp->rb) == -1) 
            errExit("sem_post");
    }

    if (sem_wait(&shmp->wa) == -1)
        errExit("sem_wait");
    
    shmp->pos = 5;
    memcpy(&shmp->buf, ENDSTR, 5);
    for(int i = 5; i < 15; i++){
        shmp->buf[i] = '\0';
    }

    if (sem_post(&shmp->rb) == -1)
        errExit("sem_post");

    pthread_exit("exited");
}

void * reader_func_a(void * memseg) {
    struct shmbuf *shmp = memseg;
    time_t t = 0;
    time_t tim = 0;

    int flag = 0;
    int initer = 1;
    char temp[1024];
    int tpos;
    int messagenum = 0;
    int packnum = 0;
    while (flag == 0) {
        if (sem_wait(&shmp->ra) == -1)
            errExit("sem_wait");

        if (shmp->term == 1) { // terminate the proccesses
            flag = 1;
            break;
        }
        
        if (strcmp(shmp->buf, ENDSTR) == 0){
            flag = 1;
            shmp->term = 1;
            break;
        }

        if (initer == 1 && shmp->pos != 0) { // beggining of new message
            printf("PROCB >>    ");
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
            for (int k = 0; k < tpos; k++) {
                printf("%c", temp[k]);
            }
            tpos = 0;
            sem_post(&shmp->wa); // when the message has ended, enable both writers
        }

        if (sem_post(&shmp->wb) == -1)
            errExit("sem_post");
    }

    float average_time = tim / (float)(messagenum-1); 

    struct metadata *met = malloc(sizeof(struct metadata));
    met->avrg_time = average_time;
    met->pack = packnum;
    met->rec = messagenum;
    met->sent = shmp->ma;
    

    if (sem_post(&shmp->rb) == -1)
        errExit("sem_post");
    pthread_exit(met);
}