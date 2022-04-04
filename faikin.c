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
   const char     *port = NULL;
   {
      poptContext     optCon;
      const struct poptOption optionsTable[] = {
         {"port", 'p', POPT_ARG_STRING, &port, 0, "Port", "/dev/cu.usbserial..."},
         {"debug", 'v', POPT_ARG_NONE, &debug, 0, "Debug"},
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
            struct timeval  tv = {0, len?100000:10000};
            int             l = select(p + 1, &r, NULL, NULL, &tv);
            if (l <= 0)
               break;
            l = read(p, buf + len, sizeof(buf) - len);
            if (l <= 0)
               break;
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
            unsigned char   res[] = {0xBE,0x0A,0x6F,0x0B,0x7A,0x01,0xBE,0x0A,0x40,0x0B,0xBE,0x0A,0x00,0x00,0x05,0x00,0x00,0x00,0x05,0x00,0x00,0x00,0x05,0x00,0x14,0x00,0x04,0x5E,0x00};
            acsend(cmd, res, sizeof(res));
         }
         break;
      case 0xBE:
         {
            unsigned char   res[] = {0x01,0x02,0x43,0x04,0x01,0x01,0x00,0x00,0x01};
            acsend(cmd, res, sizeof(res));
         }
         break;
      case 0xCA:
         {
            unsigned char   res[] = {0x01,0x02,0x02,0x16,0x00,0x00,0x11,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x02,0x00};
            acsend(cmd, res, sizeof(res));
         }
         break;
      case 0xCB:
         {
            unsigned char   res[] = {0x60,0x21};
            acsend(cmd, res, sizeof(res));
         }
         break;
      default:
	 acsend(cmd,NULL,0);
         warnx("Unknown %02X", cmd);
      }
   }
   return 0;
}
