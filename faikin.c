/* Fake Daikin for protocol testing */

#include <stdio.h>
#include <string.h>
#include <popt.h>
#include <time.h>
#include <sys/time.h>
#include <stdlib.h>
#include <ctype.h>
#include <err.h>
#include <popt.h>
#include <err.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <stdint.h>


int
main(int argc, const char *argv[])
{
   int             debug = 0,
                   dump = 0;
   int             power = 0;
   int             mode = 3;
   int             comp = 1;
   float           temp = 22.5;
   int             fan = 3;
   int             t1 = 1000,
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
   const char     *port = NULL;
   {
      poptContext     optCon;
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

      optCon = poptGetContext(NULL, argc, argv, optionsTable, 0);
      //poptSetOtherOptionHelp(optCon, "");

      int             c;
      if ((c = poptGetNextOpt(optCon)) < -1)
         errx(1, "%s: %s\n", poptBadOption(optCon, POPT_BADOPTION_NOALIAS), poptStrerror(c));

      if (poptPeekArg(optCon) || !port)
      {
         poptPrintUsage(optCon, stderr, 0);
         return -1;
      }
      poptFreeContext(optCon);
   }

   int             p = open(port, O_RDWR);
   if (p < 0)
      err(1, "Cannot open %s", port);
   struct termios  t;
   if (tcgetattr(p, &t) < 0)
      err(1, "Cannot get termios");
   cfsetspeed(&t, 9600);
   t.c_cflag = CREAD | CS8 | PARENB;
   if (tcsetattr(p, TCSANOW, &t) < 0)
      err(1, "Cannot set termios");
   usleep(100000);
   tcflush(p, TCIOFLUSH);

   void            acsend(unsigned char cmd, const unsigned char *payload, int len)
   {
      if (debug)
      {
         printf("Tx %02X", cmd);
         for (int i = 0; i < len; i++)
            printf(" %02X", payload[i]);
         printf("\n");
      }
      unsigned char   buf[256];
                      buf[0] = 0x06;
                      buf[1] = cmd;
                      buf[2] = len + 6;
                      buf[3] = 1;
                      buf[4] = cmd == 0xB7 ? 0x12 : 0x06;
      if              (len)
                         memcpy(buf + 5, payload, len);
      uint8_t         c = 0;
      for             (int i = 0; i < 5 + len; i++)
                         c += buf[i];
                      buf[5 + len] = 0xFF - c;
      if              (dump)
      {
         printf("Tx");
         for (int i = 0; i < len + 6; i++)
            printf(" %02X", buf[i]);
         printf("\n");
      }
                      write(p, buf, len + 6);
   }

   while (1)
   {
      unsigned char  *payload = NULL,
                      cmd = 0;
      int             len = 0;
      {
         unsigned char   buf[256];
         while (len < sizeof(buf))
         {
            fd_set          r;
            FD_ZERO(&r);
            FD_SET(p, &r);
            struct timeval  tv = {0, len ? 100000 : 10000};
            int             l = select(p + 1, &r, NULL, NULL, &tv);
            if (l <= 0)
               break;
            l = read(p, buf + len, sizeof(buf) - len);
            if (l <= 0)
               break;
	    if(!len&&*buf!=0x6)continue;
            len += l;
         }
         if (!len)
            continue;
         if (dump)
         {
            printf("Rx");
            for (int i = 0; i < len; i++)
               printf(" %02X", buf[i]);
            printf("\n");
         }
         unsigned char   c = 0;
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
         printf("Rx %02X", cmd);
         for (int i = 0; i < len; i++)
            printf(" %02X", payload[i]);
         printf("\n");
      }
      switch (cmd)
      {
      case 0xAA:
         acsend(cmd, payload, 1);
         break;
      case 0xBA:
         {
            const unsigned char res[] = {0x46, 0x44, 0x58, 0x4D, 0x32, 0x35, 0x46, 0x33, 0x56, 0x31, 0x42, 0x39, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1C, 0x00, 0x3E, 0x95, 0x00, 0x70, 0x65, 0x00, 0x01, 0x00};
            acsend(cmd, res, sizeof(res));
         }
         break;
      case 0xBB:
         {
            const unsigned char res[] = {0xD2, 0x89, 0x00, 0x00, 0x2C, 0xD2, 0x11, 0x7D, 0xB0, 0x20, 0x00, 0x10, 0x00, 0x20, 0x00, 0x10, 0x00, 0x05, 0x0C, 0x24};
            acsend(cmd, res, sizeof(res));
         }
         break;
      case 0xB7:
         acsend(cmd, &cmd, 1);
         break;
      case 0xBD:
         {
            unsigned char   res[] = {0xBE, 0x0A, 0x6F, 0x0B, 0x7A, 0x01, 0xBE, 0x0A, 0x40, 0x0B, 0xBE, 0x0A, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x05, 0x00, 0x14, 0x00, 0x04, 0x5E, 0x00};
            res[0] = t1;
            res[1] = t1>>8;
            res[2] = t2;
            res[3] = t2>>8;
            res[4] = t3;
            res[5] = t3>>8;
            res[6] = t4;
            res[7] = t4>>8;
            res[8] = t5;
            res[9] = t5>>8;
            res[10] = t6;
            res[11] = t6>>8;
            res[12] = t7;
            res[13] = t7>>8;
            res[14] = t8;
            res[15] = t8>>8;
            res[16] = t9;
            res[17] = t9>>8;
            res[18] = t10;
            res[19] = t10>>8;
            res[20] = t11;
            res[21] = t11>>8;
            res[22] = t12;
            res[23] = t12>>8;
            res[24] = t13;
            res[25] = t13>>8;
            acsend(cmd, res, sizeof(res));
         }
         break;
      case 0xBE:
         {
            unsigned char   res[] = {0x01, 0x02, 0x43, 0x04, 0x01, 0x01, 0x00, 0x00, 0x01};
            acsend(cmd, res, sizeof(res));
         }
         break;
      case 0xCA:
         {
            unsigned char   res[] = {0x01, 0x02, 0x02, 0x16, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00};
            res[0] = power;
            res[1] = mode;
            res[2] = comp;
            res[3] = (int)temp;
            res[4] = (int)(temp * 10) % 10;
            res[5] = 0x10 + fan;
            acsend(cmd, res, sizeof(res));
         }
         break;
      case 0xCB:
         {
            unsigned char   res[] = {0x60, 0x21};
            acsend(cmd, res, sizeof(res));
         }
         break;
      default:
         acsend(cmd, NULL, 0);
         warnx("Unknown %02X", cmd);
      }
   }
   return 0;
}
