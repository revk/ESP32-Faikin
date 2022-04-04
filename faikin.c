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

int             debug = 0;

int
main(int argc, const char *argv[])
{
   const char     *port = NULL;
   {
      poptContext     optCon;
      const struct poptOption optionsTable[] = {
         {"port", 'p', POPT_ARG_STRING, &port, 0, "Port", "/dev/cu.usbserial..."},
         {"debug", 'v', POPT_ARG_NONE, &debug, 0, "Debug"},
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

   while (1)
   {
      unsigned char   buf[256];
      int             len = 0;
      while (len < sizeof(buf))
      {
         fd_set          r;
         FD_ZERO(&r);
         FD_SET(p, &r);
         struct timeval  tv = {0, 10000};
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
      for (int i = 0; i < len; i++)
         printf("%02X ", buf[i]);
      printf("\n");
   }
   return 0;
}
