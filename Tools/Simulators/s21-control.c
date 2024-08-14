#include <stdio.h>

#include "faikin-s21.h"
#include "osal.h"

int main(int argc, const char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <option> <value>", argv[0]);
        return -1;
    }

    struct S21State *state = open_shmem(SHARED_MEM_NAME, sizeof(struct S21State));

    if (!state) {
        fprintf(stderr, "Failed to open shared memory from the simulator\n");
        return -1;
    }

    // Only options which i needed are currently implemented here. Please feel free to extend.
    if (!strcmp(argv[1], "power")) {
        state->power = argv[2][0] != '0';
    } else {
        fprintf(stderr, "Unknown option %s\n", argv[1]);
    }

    close_shmem(state);

    return 0;
}
