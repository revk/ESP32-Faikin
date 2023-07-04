// Functions to perform simple maths on decimal value strings
// (c) Copyright 2019 Andrews & Arnold Adrian Kennard
// This is stringdecimal with the stringdecimal_eval() function provided using xparse

#include "stringdecimal.h"
char *stringdecimal_eval_opts(stringdecimal_unary_t);
#define stringdecimal_eval(...)         stringdecimal_eval_opts((stringdecimal_unary_t){__VA_ARGS__})
#define stringdecimal_eval_f(...)       stringdecimal_eval_opts((stringdecimal_unary_t){__VA_ARGS__,a_free:1})

// Using stringdecimal to build a higher layer parser
#ifdef	XPARSE_H
typedef struct stringdecimal_context_s stringdecimal_context_t;
struct stringdecimal_context_s {
   int places;
   sd_format_t format;
   sd_round_t round;
   const char *fail;
   const char *posn;
   const char *currency;
   unsigned char nocomma:1;     // Do not allow comma in parse
   unsigned char comma:1;       // Add comma in output
   unsigned char raw:1;         // Raw output, i.e. sd not a string
   unsigned char nofrac:1;      // Do not allow fractions in parse
   unsigned char nosi:1;        // Do not allow SI suffix in parse
   unsigned char noieee:1;      // Do not allow IEEE suffix in parse
   unsigned char combined:1;    // Use combined digit and comma or dot
};
extern xparse_config_t stringdecimal_xparse;
#endif
