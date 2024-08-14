struct S21State
{
    int   power;    // Power on
    int   mode;     // Mode
    float temp;     // Set point
    int   fan;      // Fan speed
    int   swing;    // Swing direction
    int   powerful; // Powerful mode
    int   eco;      // Eco mode
    int   home;     // Reported temparatures (multiplied by 10 here)
    int   outside;
    int   inlet;
    int   fanrpm;   // Fan RPM (divided by 10 here)
    int   comprpm;  // Compressor RPM
    int   protocol; // Protocol version
    char  model[4]; // Reported A/C model code
};

#define SHARED_MEM_NAME "Faikin-S21"
