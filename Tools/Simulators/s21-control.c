#include <stdio.h>

#include "faikin-s21.h"
#include "osal.h"

static void parse_raw(int argc, const char **argv, unsigned char *v, unsigned int len)
{
    unsigned int i;

    if (argc < len) {
        fprintf(stderr, "%u numeric values are required\n", len);
        exit(255);
    }

    for (i = 0; i < len; i++) {
        v[i] = strtoul(argv[i], NULL, 0);
    }
}

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
    if (!strcmp(argv[1], "power"))
        state->power = argv[2][0] != '0';
#define PARSE_RAW(cmd)               \
    else if (!strcmp(argv[1], #cmd)) \
        parse_raw(argc - 2, &argv[2], state->cmd, sizeof(state->cmd));
    PARSE_RAW(FB)
    PARSE_RAW(FG)
    PARSE_RAW(FK)
    PARSE_RAW(FN)
    PARSE_RAW(FP)
    PARSE_RAW(FQ)
    PARSE_RAW(FR)
    PARSE_RAW(FS)
    PARSE_RAW(FT)
    else
        fprintf(stderr, "Unknown option %s\n", argv[1]);

    close_shmem(state);

    return 0;
}
