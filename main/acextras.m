// Status and controls

#ifndef e
#define e(name,tags)    // Enumerated value, uses single character from tags, e.g. FHCA456D means "F" is 0, "H" is 1, etc
#endif

#ifndef b
#define b(name)         // Boolean
#endif

#ifndef t
#define t(name)         // Temperature
#endif

#ifndef r
#define r(name)         // Temperature range (min/max and not in stats)
#endif

#ifndef i
#define i(name)         // Integer
#endif

#ifndef s
#define s(name,len)     // String, e.g. model
#endif

t(env)
r(target)

#include "acfields.m"
