#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "faikin-s21.h"

struct EnumOption
{
    const char *name;
    unsigned int value;
};

static struct EnumOption humidity[] =
{
    {"Off", 0x30},
    {"Low", 0x8F},
    {"Standard", 0x80},
    {"High", 0x81},
    {"Continuous", 0xFF},
    {NULL, 0}
};

static struct EnumOption demand[] =
{
    {"Off", 0},
    {"Low", 60},
    {"Medium", 50},
    {"High", 30},
    {NULL, 0}
};

static int parse_protocol(int argc, const char **argv, struct S21State *state)
{
    const char *opt = argv[0];
    unsigned long major;
    unsigned long minor = 0;
    char *endp = NULL;

    if (argc < 2) {
        fprintf(stderr, "%s: version number is required: XX or XX.XX\n", opt);
        return -1;
    }

    major = strtoul(argv[1], &endp, 10);

    if (endp && *endp == '.') {
        minor = strtoul(endp + 1, &endp, 10);
    }
    if (endp && *endp) {
        fprintf(stderr, "%s: Malformed version number '%s'\n", opt, argv[1]);
        return -1;
    }
    if (major > 99 || minor > 99) {
        fprintf(stderr, "%s: Version number '%s' is out of range; max value is 99 (2 digits)\n", opt, argv[1]);
        return -1;
    }

    state->protocol_major = major;
    state->protocol_minor = minor;
    return 2;
}

static int parse_bool(int argc, const char **argv, int *v)
{
    const char *opt, *val;

    opt = argv[0];

    if (argc < 2) {
        fprintf(stderr, "%s: boolean value is required\n", opt);
        return -1;
    }

    val = argv[1];

    if (val[0] == '1' || !strcasecmp(val, "true") || !strcasecmp(val, "on"))
        *v = 1;
    else if (val[0] == '0' || !strcasecmp(val, "false") || !strcasecmp(val, "off"))
        *v = 0;
    else {
        fprintf(stderr, "%s: Invalid boolean value '%s'\n", opt, val);
        return -1;
    }

    return 2;
}

static int parse_int(int argc, const char **argv, int *v)
{
    const char *opt = argv[0];
    char *endp = NULL;

    if (argc < 2) {
        fprintf(stderr, "%s: integer value is required\n", opt);
        return -1;
    }

    *v = strtoul(argv[1], &endp, 0);
    if (endp && *endp) {
        fprintf(stderr, "%s: Invalid integer value: %s\n", opt, argv[1]);
        return -1;
    }
    return 2;
}

static int parse_float(int argc, const char **argv, float *v)
{
    const char *opt = argv[0];

    if (argc < 2) {
        fprintf(stderr, "%s: float value is required\n", opt);
        return -1;
    }

    *v = strtof(argv[1], NULL);
    return 2;
}

static int parse_string(int argc, const char **argv, char *v, unsigned int len)
{
    const char *opt = argv[0];

    if (argc < 2 || strlen(argv[1]) != len) {
        fprintf(stderr, "%s: string value (%d) characters is required\n", opt, len);
        return -1;
    }

    memcpy(v, argv[1], len);
    return 2;
}

static int parse_enum(int argc, const char **argv, unsigned int*v, const struct EnumOption* opts)
{
    const char *opt = argv[0];
    char *endp = NULL;

    if (argc < 2) {
        fprintf(stderr, "%s: enum value is required\n", opt);
        return -1;
    }

    while (opts->name) {
        if (!strcasecmp(argv[1], opts->name)) {
            *v = opts->value;
            return 2;
        }
        opts++;
    }

    // For experimental purposes we also allow specifying raw numeric value
    *v = strtoul(argv[1], &endp, 0);
    if (endp && *endp) {
        fprintf(stderr, "%s: Invalid enum value: %s\n", opt, argv[1]);
        return -1;
    }
    return 2;
}

static int parse_raw(int argc, const char **argv, unsigned char *v, unsigned int len)
{
    const char *opt = *argv++;
    unsigned int i;

    if (argc < len + 1) {
        fprintf(stderr, "%s: %u payload bytes are required; only %u given\n", opt, len, argc - 1);
        return -1;
    }

    for (i = 0; i < len; i++) {
        const char *val = argv[i];

        // A single quote is more natural, but one has to shield it with
        // '\' in bash, and that's inconvenient. So we may also use ^, which isn't
        // a special character
        if (val[0] == '^' || val[0] == '\'') {
            v[i] = val[1];
        } else {
            char *endp = NULL;

            v[i] = strtoul(val, &endp, 0);
            if (endp && *endp) {
                fprintf(stderr, "%s: Invalid integer value: %s\n", opt, val);
                return -1;
            }
        }
    }

    return len + 1;
}

static void enum_option(const char* name, const char *description, const struct EnumOption* opts)
{
    printf(" %s <enum> - %s: ", name, description);
    while (opts->name) {
        printf("0x%02X = %s", opts->value, opts->name);
        opts++;
        if (opts->name)
            printf(", ");
    }
    putchar('\n');
}

static void raw_option(const char *opt, unsigned int len)
{
    printf(" %s <b0> <b1> ... - Raw value (%u bytes) of response to %s\n", opt, len, opt);
}

void state_options_help(void)
{
    const struct S21State* state = NULL; // For RAW_OPTION() macro below

    printf("Supported state options:\n"
           " power <bool> - power on/off\n"
           " mode <integer> - Operation mode: 0 = Fan, 1 = Heat, 2 = Cool, 3 = Auto, 7 = Dry\n"
           " fan <integer> - Fan speed: 0 = auto, 1-5 = set speed, 6 = quiet\n");
    enum_option("humidity", "Humidity setting", humidity);
    printf(" powerful <bool> - powerful mode on/off\n"
           " eco <bool> - eco mode on/off\n"
           " comfort <bool> - comfort mode on/off\n"
           " quiet <bool> - quiet mode on/off\n"
           " streamer <bool> - streamer mode on/off\n"
           " sensor <bool> - sensor mode on/off\n"
           " temp <float> - Target temperature in C\n"
           " hum_sensor <int> - Reported indoor humidity\n"
           " fanrpm <int> - Fan rpm (divided by 10)\n"
	       " comprpm <int> - Compressor rpm\n");
    enum_option("demand", "Demand mode setting", demand);
    printf(" consumption <int> - Reported power consumption\n"
           " protocol <major>.<minor> - Reported protocol version. Major and minor are 2 digits max.\n");
#define RAW_OPTION(cmd) raw_option(#cmd, sizeof(state->cmd))
    RAW_OPTION(F2);
    RAW_OPTION(F3);
    RAW_OPTION(F4);
    RAW_OPTION(FB);
    RAW_OPTION(FG);
    RAW_OPTION(FK);
    RAW_OPTION(FN);
    RAW_OPTION(FP);
    RAW_OPTION(FQ);
    RAW_OPTION(FR);
    RAW_OPTION(FS);
    RAW_OPTION(FT);
    RAW_OPTION(FV);
    RAW_OPTION(M);
    RAW_OPTION(V);
    RAW_OPTION(VS000M);
    RAW_OPTION(FU00);
    RAW_OPTION(FU02);
    RAW_OPTION(FU04);
    RAW_OPTION(FU05);
    RAW_OPTION(FU15);
    RAW_OPTION(FU25);
    RAW_OPTION(FU35);
    RAW_OPTION(FU45);
    RAW_OPTION(FY10);
    RAW_OPTION(FY20);
    RAW_OPTION(FX00);
    RAW_OPTION(FX10);
    RAW_OPTION(FX20);
    RAW_OPTION(FX30);
    RAW_OPTION(FX40);
    RAW_OPTION(FX50);
    RAW_OPTION(FX60);
    RAW_OPTION(FX70);
    RAW_OPTION(FX80);
    RAW_OPTION(FX90);
    RAW_OPTION(FXA0);
    RAW_OPTION(FXB0);
    RAW_OPTION(FXC0);
    RAW_OPTION(FXD0);
    RAW_OPTION(FXE0);
    RAW_OPTION(FXF0);
    RAW_OPTION(FX01);
    RAW_OPTION(FX11);
    RAW_OPTION(FX21);
    RAW_OPTION(FX31);
    RAW_OPTION(FX41);
    RAW_OPTION(FX51);
    RAW_OPTION(FX61);
    RAW_OPTION(FX71);
    RAW_OPTION(FX81);
    printf("Supported boolean values: 'on', 'true', '1', 'off', 'false', '0'\n"
           "Integer values can be prefixed with 0x for hex or 0 for octal\n"
           "Enum can also be specified as raw integer value for experimental purposes\n"
           "Raw bytes can be specified either as integers or as character prefixed by ^ or '\n"
           "(for example: M ^3 ^E ^5 ^3). ^ is provided for convenience when using shell");
}

int parse_item(int argc, const char **argv, struct S21State *state)
{
    const char *opt = argv[0];

    // Only options which i needed are currently implemented here. Please feel free to extend.
    if (!strcmp(opt, "power")) {
        return parse_bool(argc, argv, &state->power);
    } else if (!strcmp(opt, "mode")) {
        return parse_int(argc, argv, &state->mode);
    } else if (!strcmp(opt, "fan")) {
        return parse_int(argc, argv, &state->fan);
    } else if (!strcmp(opt, "humidity")) {
        return parse_enum(argc, argv, &state->humidity, humidity);
    }
#define PARSE_BOOL(cmd)           \
     else if (!strcmp(opt, #cmd)) \
        return parse_bool(argc, argv, &state->cmd);
    PARSE_BOOL(powerful)
    PARSE_BOOL(comfort)
    PARSE_BOOL(quiet)
    PARSE_BOOL(streamer)
    PARSE_BOOL(sensor)
    PARSE_BOOL(eco)
    else if (!strcmp(opt, "eco")) {
        return parse_bool(argc, argv, &state->eco);
    } else if (!strcmp(opt, "temp")) {
        return parse_float(argc, argv, &state->temp);
    } else if (!strcmp(opt, "hum_sensor")) {
        return parse_int(argc, argv, &state->hum_sensor);
    } else if (!strcmp(opt, "comprpm")) {
        return parse_int(argc, argv, &state->comprpm);
    } else if (!strcmp(opt, "demand")) {
        return parse_enum(argc, argv, &state->demand, demand);
    } else if (!strcmp(opt, "consumption")) {
        return parse_int(argc, argv, &state->consumption);
    } else if (!strcmp(opt, "protocol"))  {
        return parse_protocol(argc, argv, state);
    } else if (!strcmp(opt, "model")) {
        return parse_string(argc, argv, state->model, sizeof(state->model));
    }
#define PARSE_RAW(cmd)               \
    else if (!strcmp(argv[0], #cmd)) \
        return parse_raw(argc, argv, state->cmd, sizeof(state->cmd));
    PARSE_RAW(F2)
    PARSE_RAW(F3)
    PARSE_RAW(F4)
    PARSE_RAW(FB)
    PARSE_RAW(FG)
    PARSE_RAW(FK)
    PARSE_RAW(FN)
    PARSE_RAW(FP)
    PARSE_RAW(FQ)
    PARSE_RAW(FR)
    PARSE_RAW(FS)
    PARSE_RAW(FT)
    PARSE_RAW(FV)
    PARSE_RAW(M)
    PARSE_RAW(V)
    PARSE_RAW(VS000M)
    PARSE_RAW(FU00)
    PARSE_RAW(FU02)
    PARSE_RAW(FU04)
    PARSE_RAW(FU05)
    PARSE_RAW(FU15)
    PARSE_RAW(FU25)
    PARSE_RAW(FU35)
    PARSE_RAW(FU45)
    PARSE_RAW(FY10)
    PARSE_RAW(FY20)
    PARSE_RAW(FX00)
    PARSE_RAW(FX10)
    PARSE_RAW(FX20)
    PARSE_RAW(FX30)
    PARSE_RAW(FX40)
    PARSE_RAW(FX50)
    PARSE_RAW(FX60)
    PARSE_RAW(FX70)
    PARSE_RAW(FX80)
    PARSE_RAW(FX90)
    PARSE_RAW(FXA0)
    PARSE_RAW(FXB0)
    PARSE_RAW(FXC0)
    PARSE_RAW(FXD0)
    PARSE_RAW(FXE0)
    PARSE_RAW(FXF0)
    PARSE_RAW(FX01)
    PARSE_RAW(FX11)
    PARSE_RAW(FX21)
    PARSE_RAW(FX31)
    PARSE_RAW(FX41)
    PARSE_RAW(FX51)
    PARSE_RAW(FX61)
    PARSE_RAW(FX71)
    PARSE_RAW(FX81)
    else {
        fprintf(stderr, "Unknown option %s\n", opt);
        return -1;
    }
}
