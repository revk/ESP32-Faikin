#ifndef CN_WIRED_H
#define CN_WIRED_H

// Data packet length
#define CNW_PKT_LEN 8
// Known offsets within the data packet
#define CNW_TEMP_OFFSET     0
#define CNW_MODE_OFFSET     3
#define CNW_FAN_OFFSET      4
#define CNW_SPECIALS_OFFSET 5
#define CNW_CRC_TYPE_OFFSET 7

// Mode can be ORed with a poweroff flag
#define CNW_MODE_POWEROFF 0x10
#define CNW_MODE_MASK     0x0F
// Known modes
#define CNW_DRY  0
#define CNW_FAN  1
#define CNW_COOL 2
#define CNW_HEAT 4
#define CNW_AUTO 8

// Known fan speed settings. Note that POWERFUL is not a modifier
// but unique value, mutually exclusive with everything else.
#define CNW_FAN_1        8
#define CNW_FAN_2        4
#define CNW_FAN_3        2
#define CNW_FAN_AUTO     1
#define CNW_FAN_POWERFUL 3
#define CNW_FAN_QUIET    9

// Special mode flags
#define CNW_LED_ON  0x80
#define CNW_V_SWING 0x10

// Packet types
#define CNW_TYPE_MASK     0x0F
// From A/C to controller
#define CNW_SENSOR_REPORT 0
#define CNW_MODE_CHANGED  1
// From controller to A/C
#define CNW_COMMAND       0

// Checksum calculation
static inline unsigned char cnw_checksum(const unsigned char* data) {
    unsigned char last_nibble = data[CNW_CRC_TYPE_OFFSET] & CNW_TYPE_MASK;
    // 4-bit sum of all nibbles, including the last one
    unsigned char crc = last_nibble;

    for (int i = 0; i < CNW_CRC_TYPE_OFFSET; i++) {
        crc += (data[i] >> 4) + (data[i] & 0x0F);
    }

    // Packets starting from type 2 has modified checksum algorithm.
    // Sum of all nibbles must equal to 0x0F.
    if (last_nibble > CNW_MODE_CHANGED)
        crc = 0x0F - crc;
    // The received packet contains the CRC in high nibble. Low nibble is
    // also part of the payload. Keep it in place for easier manipulation
    return (crc << 4) | last_nibble;
}

// Temperature is specified in BCD form
static inline unsigned char decode_bcd(unsigned char data) {
    return (data >> 4) * 10 + (data & 0x0F);
}

static inline unsigned char encode_bcd(unsigned char value) {
    return ((value / 10) << 4) | (value % 10);
}

#endif // CN_WIRED_H
