#include <stdio.h>
#include <unistd.h>

#include "osal.h"

#ifdef WIN32

void set_serial(int p, unsigned int speed, unsigned int bits, unsigned int parity, unsigned int stop)
{
   DCB dcb = {0};
   
   dcb.DCBlength = sizeof(dcb);
   dcb.BaudRate  = speed;
   dcb.fBinary   = TRUE;
   dcb.fParity   = TRUE;
   dcb.ByteSize  = bits;
   dcb.Parity    = parity;
   dcb.StopBits  = stop;
   
   if (!SetCommState((HANDLE)_get_osfhandle(p), &dcb)) {
      fprintf(stderr, "Failed to set port parameters\n");
	  exit(255);
   }
}

// This implementation is not tested, sorry
int wait_read(int p, unsigned int timeout)
{
    DWORD r = WaitForSingleObject((HANDLE)_get_osfhandle(p), timeout);

    if (r == WAIT_OBJECT_0)
        return 1;
    else if (r == WAIT_TIMEOUT)
        return 0;
    else
        return -1;
}

#else

void set_serial(int p, unsigned int speed, unsigned int bits, unsigned int parity, unsigned int stop)
{
   struct termios  t;
   if (tcgetattr(p, &t) < 0)
      err(1, "Cannot get termios");
   cfsetspeed(&t, speed;
   t.c_cflag = CREAD | bits | parity | stop;
   if (tcsetattr(p, TCSANOW, &t) < 0)
      err(1, "Cannot set termios");
   usleep(100000);
   tcflush(p, TCIOFLUSH);
}

int wait_read(int p, unsigned int timeout)
{
    fd_set          r;
    FD_ZERO(&r);
    FD_SET(p, &r);
    // Timeout is specified in milliseconds
    struct timeval  tv = {0, timeout * 1000};
    
    return select(p + 1, &r, NULL, NULL, &tv);
}

#endif
