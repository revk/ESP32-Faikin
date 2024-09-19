#ifdef WIN32
#include <windows.h>

#define CS5 5
#define CS6 6
#define CS7 7
#define CS8 8

#else
#include <termios.h>

// Parity constants
#define EVENPARITY PARENB
#define OODPARITY  (PARENB|PARODD)

// Stop bit constants
#define ONESTOPBIT  0
#define TWOSTOPBITS CSTOPB

#endif

int set_serial(int pm, unsigned int speed, unsigned int bits, unsigned int parity, unsigned int stop);
int wait_read(int p, unsigned int timeout);
void *create_shmem(const char *name, void* data, unsigned int len);
void *open_shmem(const char *name, unsigned int len);
void close_shmem(void *mem);
