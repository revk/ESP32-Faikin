#ifndef _DAIKIN_S21_H
#define _DAIKIN_S21_H

#include <math.h>
#include <stdint.h>

#include "faikin_enums.h"

// Some common S21 definitions
#define	STX	2
#define	ETX	3
#define	ACK	6
#define	NAK	21

// Packet structure
#define S21_STX_OFFSET     0
#define S21_CMD0_OFFSET    1
#define S21_CMD1_OFFSET    2
#define S21_PAYLOAD_OFFSET 3

// Length of a framing (STX + CRC + ETX)
#define S21_FRAMING_LEN 3
// A typical payload length, but there are deviations
#define S21_PAYLOAD_LEN 4
// A minimum length of a packet (with no payload): framing + CMD0 + CMD1
#define S21_MIN_PKT_LEN (S21_FRAMING_LEN + 2)

// v3 packets use 4-character command codes
#define S21_V3_CMD2_OFFSET 3
#define S21_V3_CMD3_OFFSET 4
#define S21_V3_PAYLOAD_OFFSET 5
#define S21_MIN_V3_PKT_LEN (S21_FRAMING_LEN + 4)

// Encoding for minimum target temperature value, correspond to 18 deg.C.
#define AC_MIN_TEMP_VALUE '@'

#define AC_MODE_AUTO 0
#define AC_MODE_DRY  2
#define AC_MODE_COOL 3
#define AC_MODE_HEAT 4
#define AC_MODE_FAN  6

// Fan speed
#define AC_FAN_AUTO  'A'
#define AC_FAN_QUIET 'B'
#define AC_FAN_1     '3'
#define AC_FAN_2     '4'
#define AC_FAN_3     '5'
#define AC_FAN_4     '6'
#define AC_FAN_5     '7'

// Calculate packet checksum
static inline uint8_t
s21_checksum (uint8_t * buf, int len)
{
   uint8_t c = 0;

   // The checksum excludes STX, checksum field itself, and ETX
   for (int i = 1; i < len - 2; i++)
      c += buf[i];

   // Special bytes are forbidden even as checksum bytes, they are promoted
   if (c == STX || c == ETX || c == ACK)
      c += 2;

   return c;
}

// Target temperature is encoded as one character
static inline float
s21_decode_target_temp (unsigned char v)
{
   return 18.0 + 0.5 * ((signed) v - AC_MIN_TEMP_VALUE);
}

static inline float
s21_encode_target_temp (float temp)
{
   return lroundf ((temp - 18.0) * 2) + AC_MIN_TEMP_VALUE;
}

static inline int
s21_decode_int_sensor (const unsigned char *payload)
{
   int v = (payload[0] - '0') + (payload[1] - '0') * 10 + (payload[2] - '0') * 100;
   if (payload[3] == '-')
      v = -v;
   return v;
}

static inline uint16_t
s21_decode_hex_sensor (const unsigned char *payload)
{
#define hex(c)	(((c)&0xF)+((c)>'9'?9:0))
   return (hex (payload[3]) << 12) | (hex (payload[2]) << 8) | (hex (payload[1]) << 4) | hex (payload[0]);
#undef hex
}

static inline float
s21_decode_float_sensor (const unsigned char *payload)
{
   return (float) s21_decode_int_sensor (payload) * 0.1;
}

// Convert between Daikin and Faikin fan speed enums
static inline unsigned char
s21_encode_fan (int speed)
{
   switch (speed)
   {
   case FAIKIN_FAN_AUTO:
      return AC_FAN_AUTO;
   case FAIKIN_FAN_QUIET:
      return AC_FAN_QUIET;
   default:
      return speed - FAIKIN_FAN_1 + AC_FAN_1;
   }
}

static inline int
s21_decode_fan (unsigned char v)
{
   switch (v)
   {
   case AC_FAN_AUTO:
      return FAIKIN_FAN_AUTO;
   case AC_FAN_QUIET:
      return FAIKIN_FAN_QUIET;
   default:
      return v - AC_FAN_1 + FAIKIN_FAN_1;
   }
}

#endif
