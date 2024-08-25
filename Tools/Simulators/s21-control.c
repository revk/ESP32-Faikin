#include <stdio.h>
#include <string.h>

#include "faikin-s21.h"
#include "osal.h"

int main(int argc, const char **argv)
{
    if (argc < 2 || !strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")) {
        fprintf(stderr, "Usage: %s <option> <value>\n", argv[0]);
        state_options_help();
        return -1;
    }

    struct S21State *state = open_shmem(SHARED_MEM_NAME, sizeof(struct S21State));

    if (!state) {
        fprintf(stderr, "Failed to open shared memory from the simulator\n");
        return -1;
    }

    if (parse_item(argc - 1, &argv[1], state) < 1) {
        fprintf(stderr, "Invalid command line given\n");
    }

    close_shmem(state);

    return 0;
}
