/* pshm_ucase_bounce.c

    Licensed under GNU General Public License v2 or later.
*/
#include <ctype.h>

#include "common.h"

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

    shm_unlink(shmpath);

    printf("#### END OF PROCCESS ####\n");

    exit(EXIT_SUCCESS);
}