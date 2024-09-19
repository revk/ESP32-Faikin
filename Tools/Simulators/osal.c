#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "osal.h"

#ifdef WIN32

int set_serial(int p, unsigned int speed, unsigned int bits, unsigned int parity, unsigned int stop)
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
	   return -1;
   }

   return 0;
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

static HANDLE hMapFile;

void *create_shmem(const char *name, void *data, unsigned int len)
{
   void *pBuf;

   hMapFile = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, len, name);
   if (hMapFile == NULL)
   {
        fprintf(stderr, "Could not create file mapping object (0x%08X)\n", GetLastError());
        return NULL;
   }

   pBuf = MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0,len);

   if (pBuf == NULL)
   {
       fprintf(stderr, "Could not map view of file (0x%08X)\n", GetLastError());
       CloseHandle(hMapFile);
       return NULL;
   }

   CopyMemory(pBuf, data, len);

   return pBuf;
}

void *open_shmem(const char *name, unsigned int len)
{
   void *pBuf;

   hMapFile = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, name);

   if (hMapFile == NULL)
   {
      fprintf(stderr, "Could not open file mapping object (0x%08X)\n", GetLastError());
      return NULL;
   }

   pBuf = MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, len);

   if (pBuf == NULL)
   {
      fprintf(stderr, "Could not map view of file (0x%08X)\n", GetLastError());
      CloseHandle(hMapFile);
      return NULL;
   }

   return pBuf;
}

void close_shmem(void *mem)
{
   UnmapViewOfFile(mem);
   CloseHandle(hMapFile);
}

#else

int set_serial(int p, unsigned int speed, unsigned int bits, unsigned int parity, unsigned int stop)
{
   struct termios  t;
   if (tcgetattr(p, &t) < 0) {
      perror("Cannot get termios");
      return -1;
   }
   cfsetspeed(&t, speed);
   cfmakeraw(&t);
   t.c_iflag &= ~(IGNBRK | IXON | IXOFF | IXANY);
   t.c_cflag = CLOCAL | CREAD | bits | parity | stop;
   t.c_cc[VMIN] = 1;
   t.c_cc[VTIME] = 0;

   if (tcsetattr(p, TCSANOW, &t) < 0) {
      perror("Cannot set termios");
      return -1;
   }
   usleep(100000);
   tcflush(p, TCIOFLUSH);
   return 0;
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

void *create_shmem(const char *name, void* data, unsigned int len)
{
    return data;
}

void *open_shmem(const char *name, unsigned int len)
{
    fprintf(stderr, "Sorry, shared memory is not implemented for UNIX targets\n");
    exit(255);
    return NULL;
}

void close_shmem(void *mem)
{
    // Not implemented
}

#endif
