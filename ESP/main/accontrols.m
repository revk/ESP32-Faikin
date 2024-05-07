// Controls

#ifndef	e
#define	e(name,tags)	// Enumerated value, uses single character from tags, e.g. FHCA456D means "F" is 0, "H" is 1, etc
#endif

#ifndef	b
#define	b(name)		// Boolean
#endif

#ifndef	t
#define	t(name)		// Temperature
#endif

#ifndef	r
#define	r(name)		// Temperature range (only min/max, and not stats)
#endif

#ifndef	i
#define	i(name)		// Integer
#endif

#ifndef	s
#define	s(name,len)	// String, e.g. model
#endif

b(power)
e(mode,FHCA456D)
t(temp)
i(demand)
e(fan,A12345Q)
b(swingh)
b(swingv)
b(econo)
b(powerful)
b(comfort)
b(streamer)
b(sensor)
b(led)
b(quiet)

#undef	e
#undef	b
#undef	t
#undef	v
#undef	i
#undef	s
#undef	r
