struct S21State
{
    int          power;    // Power on
    int          mode;     // Mode
    float        temp;     // Set point
    int          fan;      // Fan speed
    int          swing;    // Swing direction
    int          humidity; // Humidity setting
    int          powerful; // Powerful mode
    int          comfort;  // Comfort mode
    int          quiet;    // Quiet mode
    int          sensor;   // Sensor mode
    int          led;      // LED on/off
    int          streamer; // Streamer mode
    int          eco;      // Eco mode
    int          demand;   // Demand setting
    int          home;     // Reported temparatures (multiplied by 10 here)
    int          outside;
    int          inlet;
    int          hum_sensor; // Humidity sensor value
    unsigned int  fanrpm;         // Fan RPM (divided by 10 here)
    unsigned int  comprpm;        // Compressor RPM
    unsigned int  consumption;    // Power consumption
    unsigned char protocol_major; // Protocol version
    unsigned char protocol_minor;
    char          model[4];       // Reported A/C model code
    // The following aren't understood yet
    unsigned char F2[4];
    unsigned char F3[4];
    unsigned char F4[4];
    unsigned char FB[4];
    unsigned char FG[4];
    unsigned char FK[4];
    unsigned char FN[4];
    unsigned char FP[4];
    unsigned char FQ[4];
    unsigned char FR[4];
    unsigned char FS[4];
    unsigned char FT[4];
    unsigned char FV[4];
    unsigned char M[4];
    unsigned char V[4];
    unsigned char VS000M[14];
    unsigned char FU00[32];
    unsigned char FU02[32];
    unsigned char FU04[32];
    unsigned char FU05[32];
    unsigned char FU15[32];
    unsigned char FU25[32];
    unsigned char FU35[32];
    unsigned char FU45[32];
    unsigned char FY10[8];
    unsigned char FY20[4];
    unsigned char FX00[2];
    unsigned char FX10[2];
    unsigned char FX20[4];
    unsigned char FX30[2];
    unsigned char FX40[2];
    unsigned char FX50[2];
    unsigned char FX60[4];
    unsigned char FX70[4];
    unsigned char FX80[4];
    unsigned char FX90[4];
    unsigned char FXA0[4];
    unsigned char FXB0[2];
    unsigned char FXC0[2];
    unsigned char FXD0[8];
    unsigned char FXE0[8];
    unsigned char FXF0[8];
    unsigned char FX01[8];
    unsigned char FX11[8];
    unsigned char FX21[2];
    unsigned char FX31[8];
    unsigned char FX41[8];
    unsigned char FX51[4];
    unsigned char FX61[2];
    unsigned char FX71[2];
    unsigned char FX81[2];
};

// POSIX shm requires the name to start with '/' for portability reasons.
// Works also on Windows with no problems, so let it be
#define SHARED_MEM_NAME "/Faikin-S21"

void state_options_help(void);
int parse_item(int argc, const char **argv, struct S21State *state);
