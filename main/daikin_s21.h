#ifndef _DAIKIN_S21_H
#define _DAIKIN_S21_H

#include <math.h>
#include <stdint.h>

#include "faikin_enums.h"

// Some common S21 definitions
#define	STX	2
#define	ETX	3
#define	ENQ	5
#define	ACK	6
#define	NAK	21

// Packet structure
#define S21_STX_OFFSET     0
#define S21_CMD0_OFFSET    1
#define S21_CMD1_OFFSET    2
#define S21_PAYLOAD_OFFSET 3
#define S21_PAYLOAD_LEN    4

// A minimum length of a packet (with no payload)
#define S21_MIN_PKT_LEN 5

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
static inline uint8_t s21_checksum(uint8_t *buf, int len)
{
   uint8_t c = 0;

   // The checksum excludes STX, checksum field itself, and ETX
   for (int i = 1; i < len - 2; i++)
      c += buf[i];

   // Sees checksum of 03 actually sends as 05 in order not to clash with ETX
   return (c == ACK) ? ENQ : c;
}

// Target temperature is encoded as one character
static inline float s21_decode_target_temp(unsigned char v)
{
   return 18.0 + 0.5 * ((signed)v - AC_MIN_TEMP_VALUE);
}

static inline float s21_encode_target_temp(float temp)
{
   return lroundf((temp - 18.0) * 2) + AC_MIN_TEMP_VALUE;
}

// Convert between Daikin and Faikin fan speed enums
static inline unsigned char s21_encode_fan(int speed)
{
   switch (speed) {
   case FAIKIN_FAN_AUTO:
	  return AC_FAN_AUTO;
   case FAIKIN_FAN_QUIET:
	  return AC_FAN_QUIET;
   default:
      return speed - FAIKIN_FAN_1 + AC_FAN_1;
   }
}

static inline int s21_decode_fan(unsigned char v)
{
   switch (v) {
   case AC_FAN_AUTO:
	  return FAIKIN_FAN_AUTO;
   case AC_FAN_QUIET:
	  return FAIKIN_FAN_QUIET;
   default:
      return v - AC_FAN_1 + FAIKIN_FAN_1;
   }
}

#endif
