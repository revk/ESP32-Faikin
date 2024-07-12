#ifndef _FAIKIN_ENUMS_H
#define _FAIKIN_ENUMS_H

#include "cn_wired.h"

// Faikin mode enums
#define FAIKIN_MODE_FAN     0
#define FAIKIN_MODE_HEAT    1
#define FAIKIN_MODE_COOL    2
#define FAIKIN_MODE_AUTO    3
// Values 4, 5, 6 are not used !!!
#define FAIKIN_MODE_DRY     7
#define FAIKIN_MODE_INVALID -1

// Faikin fan speed enums
#define FAIKIN_FAN_AUTO    0
#define FAIKIN_FAN_1       1
#define FAIKIN_FAN_2       2
#define FAIKIN_FAN_3       3
#define FAIKIN_FAN_4       4
#define FAIKIN_FAN_5       5
#define FAIKIN_FAN_QUIET   6
#define FAIKIN_FAN_INVALID -1

// Conversion from/to CN_WIRED protocol values
static inline int8_t
cnw_decode_mode(const unsigned char *data)
{
    switch (data[CNW_MODE_OFFSET] & CNW_MODE_MASK)
    {
    case CNW_DRY:
        return FAIKIN_MODE_DRY;
    case CNW_FAN:
        return FAIKIN_MODE_FAN;
    case CNW_COOL:
        return FAIKIN_MODE_COOL;
    case CNW_HEAT:
        return FAIKIN_MODE_HEAT;
    case CNW_AUTO:
        return FAIKIN_MODE_AUTO;
    default:
        return FAIKIN_MODE_INVALID;
    }
}

static inline int8_t
cnw_decode_fan(const uint8_t *data)
{
    switch (data[CNW_FAN_OFFSET])
    {
    case CNW_FAN_1:
        return FAIKIN_FAN_1;
    case CNW_FAN_2:
        return FAIKIN_FAN_3;
    case CNW_FAN_3:
        return FAIKIN_FAN_5;
    case CNW_FAN_AUTO:
        return FAIKIN_FAN_AUTO;
    case CNW_FAN_QUIET:
        return FAIKIN_FAN_QUIET;
    // POWERFUL is handled separately; it's not a fan speed for us
    default:
        return FAIKIN_FAN_INVALID;
    }
}

static inline uint8_t
cnw_encode_mode(uint8_t mode, uint8_t power)
{
    static uint8_t table[] = {CNW_FAN, CNW_HEAT, CNW_COOL, CNW_AUTO,
                              CNW_AUTO, CNW_AUTO, CNW_AUTO, // These are unused
                              CNW_DRY};
    uint8_t ret = table[mode];

    return power ? ret : (ret | CNW_MODE_POWEROFF);
}

static inline uint8_t
cnw_encode_fan(uint8_t fan)
{
    if (fan == FAIKIN_FAN_AUTO)
        return CNW_FAN_AUTO;
    else if (fan == FAIKIN_FAN_QUIET)
        return CNW_FAN_QUIET;
    else if (fan > FAIKIN_FAN_3)
        return CNW_FAN_3;
    else if (fan > FAIKIN_FAN_1)
        return CNW_FAN_2;
    else
        return CNW_FAN_1;
}

#endif
