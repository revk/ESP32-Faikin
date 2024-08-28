struct S21State
{
    int          power;    // Power on
    int          mode;     // Mode
    float        temp;     // Set point
    int          fan;      // Fan speed
    int          swing;    // Swing direction
    int          powerful; // Powerful mode
    int          eco;      // Eco mode
    int          home;     // Reported temparatures (multiplied by 10 here)
    int          outside;
    int          inlet;
    unsigned int fanrpm;      // Fan RPM (divided by 10 here)
    unsigned int comprpm;     // Compressor RPM
    unsigned int consumption; // Power consumption
    unsigned int protocol;    // Protocol version
    char         model[4];    // Reported A/C model code
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
    unsigned char M[4];
};

#define SHARED_MEM_NAME "Faikin-S21"

void state_options_help(void);
int parse_item(int argc, const char **argv, struct S21State *state);
