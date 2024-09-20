/* Fake Daikin for protocol testing */

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <popt.h>
#include <time.h>
#include <sys/time.h>
#include <stdlib.h>
#include <ctype.h>
#include <popt.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>

#include "osal.h"

int debug = 0,
    dump = 0,
    p = -1;

void acsend(unsigned char cmd, const unsigned char *payload, int len) {
    if (debug) {
        printf("[32mTx %02X", cmd);
        for (int i = 0; i < len; i++)
            printf(" %02X", payload[i]);
        printf("\n");
    }
    unsigned char buf[256];
    buf[0] = 0x06;
    buf[1] = cmd;
    buf[2] = len + 6;
    buf[3] = 1;
    buf[4] = cmd == 0xB7 ? 0x12 : 0x06;
    if (len)
        memcpy(buf + 5, payload, len);
    uint8_t c = 0;
    for (int i = 0; i < 5 + len; i++)
        c += buf[i];
    buf[5 + len] = 0xFF - c;
    if (dump) {
        printf("[32;1mTx");
        for (int i = 0; i < len + 6; i++)
            printf(" %02X", buf[i]);
        printf("\n");
    }
    write(p, buf, len + 6);
}

int
main (int argc, const char *argv[])
{
   int power = 0;
   int mode = 3;
   int comp = 1;
   float temp = 22.5;
   int fan = 3;
   int t1 = 1000,
      t2 = 1000,
      t3 = 1000,
      t4 = 1000,
      t5 = 1000,
      t6 = 1000,
      t7 = 1000,
      t8 = 1000,
      t9 = 1000,
      t10 = 1000,
      t11 = 1000,
      t12 = 1000,
      t13 = 1000;
   const char *port = NULL;
   poptContext optCon;
   {
      const struct poptOption optionsTable[] = {
         {"port", 'p', POPT_ARG_STRING, &port, 0, "Port", "/dev/cu.usbserial..."},
         {"debug", 'v', POPT_ARG_NONE, &debug, 0, "Debug"},
         {"on", 0, POPT_ARG_NONE, &power, 0, "Power on"},
         {"mode", 0, POPT_ARG_INT, &mode, 0, "Mode", "0=F,1=H,2=C,3=A,7=D"},
         {"fan", 0, POPT_ARG_INT, &fan, 0, "Fan", "1-5"},
         {"temp", 0, POPT_ARG_FLOAT, &temp, 0, "Temp", "C"},
         {"comp", 0, POPT_ARG_INT, &comp, 0, "Comp", "1=H,2=C"},
         {"t1", 0, POPT_ARG_INT, &t1, 0, "T1", "N"},
         {"t2", 0, POPT_ARG_INT, &t2, 0, "T2", "N"},
         {"t3", 0, POPT_ARG_INT, &t3, 0, "T3", "N"},
         {"t4", 0, POPT_ARG_INT, &t4, 0, "T4", "N"},
         {"t5", 0, POPT_ARG_INT, &t5, 0, "T5", "N"},
         {"t6", 0, POPT_ARG_INT, &t6, 0, "T6", "N"},
         {"t7", 0, POPT_ARG_INT, &t7, 0, "T7", "N"},
         {"t8", 0, POPT_ARG_INT, &t8, 0, "T8", "N"},
         {"t9", 0, POPT_ARG_INT, &t9, 0, "T9", "N"},
         {"t10", 0, POPT_ARG_INT, &t10, 0, "T10", "N"},
         {"t11", 0, POPT_ARG_INT, &t11, 0, "T11", "N"},
         {"t12", 0, POPT_ARG_INT, &t12, 0, "T12", "N"},
         {"t13", 0, POPT_ARG_INT, &t13, 0, "T13", "N"},
         {"dump", 'V', POPT_ARG_NONE, &dump, 0, "Dump"},
         POPT_AUTOHELP {}
      };

      optCon = poptGetContext (NULL, argc, argv, optionsTable, 0);
      //poptSetOtherOptionHelp(optCon, "");

      int c;
      if ((c = poptGetNextOpt(optCon)) < -1) {
         fprintf(stderr, "%s: %s\n", poptBadOption(optCon, POPT_BADOPTION_NOALIAS), poptStrerror(c));
         exit(255);
      }

      if (poptPeekArg (optCon) || !port)
      {
         poptPrintUsage (optCon, stderr, 0);
         return -1;
      }
   }

   p = open (port, O_RDWR);
   if (p < 0) {
      fprintf(stderr, "Cannot open %s: %s", port, strerror(errno));
	  exit(255);
   }

   set_serial(p, 9600, CS8, EVENPARITY, TWOSTOPBITS);

   while (1)
   {
      unsigned char *payload = NULL,
         cmd = 0;
      int len = 0;
      {
         unsigned char buf[256];
         while (len < sizeof (buf))
         {
            int l = wait_read(p, len  ? 100 : 10);
            if (l <= 0)
               break;
            l = read (p, buf + len, sizeof (buf) - len);
            if (l <= 0)
               break;
            if (!len && *buf != 0x6)
               continue;
            len += l;
         }
         if (!len)
            continue;
         if (dump)
         {
            printf ("[31mRx");
            for (int i = 0; i < len; i++)
               printf (" %02X", buf[i]);
            printf ("\n");
         }
         unsigned char c = 0;
         for (int i = 0; i < len; i++)
            c += buf[i];
         if (len < 6 || buf[0] != 6 || buf[2] != len || buf[3] != 1 || buf[4] || c != 0xFF)
            continue;
         cmd = buf[1];
         payload = buf + 5;
         len -= 6;
      }
      if (debug)
      {
         printf ("[31;1mRx %02X", cmd);
         for (int i = 0; i < len; i++)
            printf (" %02X", payload[i]);
         printf ("\n");
      }
      switch (cmd)
      {
      case 0xAA:
         acsend (cmd, payload, 1);
         break;
      case 0xBA:
         {
            const unsigned char res[] =
               { 0x46, 0x44, 0x58, 0x4D, 0x32, 0x35, 0x46, 0x33, 0x56, 0x31, 0x42, 0x39, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x1C, 0x00, 0x3E, 0x95, 0x00, 0x70, 0x65, 0x00, 0x01, 0x00 };
            acsend (cmd, res, sizeof (res));
         }
         break;
      case 0xBB:
         {
            const unsigned char res[] =
               { 0xD2, 0x89, 0x00, 0x00, 0x2C, 0xD2, 0x11, 0x7D, 0xB0, 0x20, 0x00, 0x10, 0x00, 0x20, 0x00, 0x10, 0x00, 0x05, 0x0C,
0x24 };
            acsend (cmd, res, sizeof (res));
         }
         break;
      case 0xB7:
         acsend (cmd, &cmd, 1);
         break;
      case 0xBD:
         {
            unsigned char res[] =
               { 0xBE, 0x0A, 0x6F, 0x0B, 0x7A, 0x01, 0xBE, 0x0A, 0x40, 0x0B, 0xBE, 0x0A, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x05,
0x00, 0x00, 0x00, 0x05, 0x00, 0x14, 0x00, 0x04, 0x5E, 0x00 };
            res[0] = t1;
            res[1] = t1 >> 8;
            res[2] = t2;
            res[3] = t2 >> 8;
            res[4] = t3;
            res[5] = t3 >> 8;
            res[6] = t4;
            res[7] = t4 >> 8;
            res[8] = t5;
            res[9] = t5 >> 8;
            res[10] = t6;
            res[11] = t6 >> 8;
            res[12] = t7;
            res[13] = t7 >> 8;
            res[14] = t8;
            res[15] = t8 >> 8;
            res[16] = t9;
            res[17] = t9 >> 8;
            res[18] = t10;
            res[19] = t10 >> 8;
            res[20] = t11;
            res[21] = t11 >> 8;
            res[22] = t12;
            res[23] = t12 >> 8;
            res[24] = t13;
            res[25] = t13 >> 8;
            acsend (cmd, res, sizeof (res));
         }
         break;
      case 0xBE:
         {
            unsigned char res[] = { 0x01, 0x02, 0x43, 0x04, 0x01, 0x01, 0x00, 0x00, 0x01 };
            acsend (cmd, res, sizeof (res));
         }
         break;
      case 0xCA:
         {
            if (payload[0])
               power = (payload[0] & 1);
            if (payload[1])
            {
               mode = (payload[1] & 0xF);
               comp = (mode == 1 ? 1 : 2);
            }
            if (payload[3])
               temp = (payload[3] + (payload[4] & 0x7F) * 0.1);
            unsigned char res[] =
               { 0x01, 0x02, 0x02, 0x16, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00 };
            res[0] = power;
            res[1] = mode;
            res[2] = comp;
            res[3] = (int) temp;
            res[4] = (int) (temp * 10) % 10;
            res[5] = 0x10 + fan;
            acsend (cmd, res, sizeof (res));
         }
         break;
      case 0xCB:
         {
            unsigned char res[] = { 0x06, 0x21 };
            res[0] = (mode == 1 || mode == 2) ? mode : 6;
            res[1] = (fan << 4) + 1;
            acsend (cmd, res, sizeof (res));
         }
         break;
      default:
         acsend (cmd, NULL, 0);
         printf ("Unknown %02X\n", cmd);
      }
   }
   poptFreeContext (optCon);
   return 0;
}
