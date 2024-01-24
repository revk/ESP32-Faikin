#include <stdio.h>
#include <stdlib.h>
#include <err.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

int
main(int argc, const char *argv[])
{
   setsid();
   if (argc <= 1)
      errx(1, "Specify port");
   char           *cmd;
   asprintf(&cmd, "idf.py monitor -p %s", argv[1]);
   FILE           *f = popen(cmd, "r");
   if (!f)
      err(1, "Cannot run %s", cmd);
   char           *line = NULL;
   size_t          len = 0;
   warnx("%s", cmd);
   while (1)
   {
      ssize_t         l = getline(&line, &len, f);
      if (l <= 0)
         break;
      printf("%s", line);
      if (strstr(line, "invalid header: 0xffffffff"))
         break;
   }
   free(line);
   killpg(0, SIGTERM);
   fclose(f);
   return 0;
}
