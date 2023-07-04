// Functions to perform simple maths on decimal value strings
// (c) Copyright 2019 Andrews & Arnold Adrian Kennard
/*
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
// Value strings are optional minus sign, digits, option decimal place plus digits, any precision
// Functions return malloced string answers (or NULL for error)
// Functions have variants to free arguments

#define _GNU_SOURCE             /* See feature_test_macros(7) */
#ifdef	EVAL
#include "xparse.h"
#include "stringdecimaleval.h"
#else
#include "stringdecimal.h"
#endif
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <err.h>
#include <assert.h>
#include <limits.h>

char sd_comma = ',';
char sd_point = '.';
int sd_max = 0;

static const char *digitnormal[] = { "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "-", "+", NULL };
static const char *digitcomma[] = { "🄁", "🄂", "🄃", "🄄", "🄅", "🄆", "🄇", "🄈", "🄉", "🄊", NULL };
static const char *digitpoint[] = { "🄀", "⒈", "⒉", "⒊", "⒋", "⒌", "⒍", "⒎", "⒏", "⒐", NULL };
static const char *digitsup[] = { "⁰", "¹", "²", "³", "⁴", "⁵", "⁶", "⁷", "⁸", "⁹", "⁻", "⁺", NULL };
static const char *digitsub[] = { "₀", "₁", "₂", "₃", "₄", "₅", "₆", "₇", "₈", "₉", "₋", "₊", NULL };
static const char *digitdigbat[] = { "🄋", "➀", "➁", "➂", "➃", "➄", "➅", "➆", "➇", "➈", NULL };
static const char *digitdigbatneg[] = { "🄌", "➊", "➋", "➌", "➍", "➎", "➏", "➐", "➑", "➒", NULL };

static const char **digits[] = {
   digitnormal,
   digitsup,
   digitsub,
   digitdigbat,
   digitdigbatneg,
   NULL,
};

struct
{
   const char *value;
   int mag;
} si[] = {
   {"q", -30},
   {"r", -27},
   {"y", -24},
   {"z", -21},
   {"a", -18},
   {"f", -15},
   {"p", -12},
   {"µ", -6},
   {"μ", -6},
   {"u", -6},
   {"mc", -6},
   {"n", -9},
   {"‱", -4},
   {"m", -3},
   {"‰", -3},
   {"c", -2},
   {"d", -1},
   {"da", 1},
   {"h", 2},
   {"k", 3},
   {"M", 6},
   {"G", 9},
   {"T", 12},
   {"P", 15},
   {"E", 18},
   {"Z", 21},
   {"Y", 24},
   {"R", 27},
   {"Q", 30},
};

#define	SIS (sizeof(si)/sizeof(*si))

struct
{
   const char *value;
   unsigned long long mul;
} ieee[] = {
   {"Ki", 1024L},
   {"Mi", 1048576L},
   {"Gi", 1073741824LL},
   {"Ti", 1099511627776LL},
   {"Pi", 1125899906842624LL},
   {"Ei", 1152921504606846976LL},
};

#define IEEES (sizeof(ieee)/sizeof(*ieee))

struct
{
   const char *value;
   char n;
   char d;
} fraction[] = {
   {"¼", 1, 4},
   {"½", 1, 2},
   {"¾", 3, 4},
   {"⅐", 1, 7},
   {"⅑", 1, 9},
   {"⅒", 1, 10},
   {"⅓", 1, 3},
   {"⅔", 2, 3},
   {"⅕", 1, 5},
   {"⅖", 2, 5},
   {"⅗", 3, 5},
   {"⅘", 4, 5},
   {"⅙", 1, 6},
   {"⅚", 5, 6},
   {"⅛", 1, 8},
   {"⅜", 3, 8},
   {"⅝", 5, 8},
   {"⅞", 7, 8},
   {"⅟", 1, -1},
   {"↉", 0, 3},
   {"∞", 1, 0},
};

#define	FRACTIONS (sizeof (fraction)/sizeof(*fraction))

//#define DEBUG

// Support functions

typedef struct sd_val_s sd_val_t;
struct sd_val_s
{                               // The structure used internally for digit sequences
   int mag;                     // Magnitude of first digit, e.g. 3 is hundreds, can start negative, e.g. 0.1 would be mag -1
   int sig;                     // Significant figures (i.e. size of d array) - logically unsigned but seriously C fucks up any maths with that
   int max;                     // Max space at m
   char *d;                     // Digit array (normally m, or advanced in to m), digits 0-9 not characters '0'-'9'
   char neg:1;                  // Sign (set if -1)
   char m[];                    // Malloced space
};

static sd_val_t zero = { 0 };

static sd_val_t one = { 0, 1, 1, (char[])
   {1}
};

static sd_val_t two = { 0, 1, 1, (char[])
   {2}
};

//static sd_val_t two = { 0, 1, (char[]) { 2 } };

struct sd_s
{
   sd_val_t *n;                 // Numerator
   sd_val_t *d;                 // Denominator
   int places;                  // Max places seen
   const char *failure;         // Error message
};

static inline int
comp (const char *a, const char *b)
{                               // Simple compare
   if (!a || !b)
      return 0;
   int l = 0;
   while (a[l] && b[l] && a[l] == b[l])
      l++;
   if (!a[l])
      return l;
   return 0;
}

static void sd_rational (sd_p p);

// Safe free and NULL value
#define freez(x)	do{if(x)free((void*)(x));x=NULL;}while(0)

static int
checkmax (const char **failp, int mag, int sig)
{
   if (!sd_max || !sig)
      return 0;
   int p = sig - mag + 1;
   if (mag >= 0)
   {
      p = mag + 1;
      if (sig > mag + 1)
         p += sig - mag;
   }
   if (p > sd_max)
   {
      if (failp && !*failp)
         *failp = "Number too long";
      return 1;
   }
   return 0;
}

static sd_val_t *
make (const char **failp, int mag, int sig)
{                               // Initialise with space for digits
   if (sig < 0)
      sig = 0;
   if (!sig)
      mag = 0;
   if (checkmax (failp, mag, sig))
      return NULL;
   sd_val_t *v = calloc (1, sizeof (*v) + sig);
   if (!v)
   {
      if (failp && !*failp)
      {
         warnx ("Malloc %lu failed", sizeof (*v) + sig);
         *failp = "Malloc failed";
      }
      return v;
   }
   v->mag = mag;
   v->sig = sig;
   v->max = sig;
   v->d = v->m;
   return v;
}

static sd_val_t *
copy (const char **failp, sd_val_t * a)
{                               // Copy
   if (!a)
      return a;
   sd_val_t *r = make (failp, a->mag, a->sig);
   if (!r)
      return r;
   r->neg = a->neg;
   if (a->sig)
      memcpy (r->d, a->d, a->sig);
   return r;
}

typedef struct
{
   sd_val_t *s;
   unsigned char neg:1;
   unsigned char pad:1;         // I.e. don't trim back
} norm_t;
#define norm(...) norm_opts((norm_t){__VA_ARGS__})
static sd_val_t *
norm_opts (norm_t o)
{                               // Normalise (striping leading/trailing 0s)
   if (!o.s)
      return o.s;
   while (o.s->sig && !o.s->d[0])
   {                            // Leading 0s
      o.s->d++;
      o.s->mag--;
      o.s->sig--;
   }
   if (!o.pad)
      while (o.s->sig && !o.s->d[o.s->sig - 1])
         o.s->sig--;            // Trailing 0
   if (o.neg)
      o.s->neg ^= 1;
   if (!o.s->sig)
   {                            // zero
      o.s->mag = 0;
      o.s->neg = 0;
   }
   return o.s;
}

typedef struct
{
   const char *v;
   const char **end;
   int *placesp;
   unsigned char nocomma:1;
   unsigned char comma:1;
} parse_t;
#define	parse(failp,...)	parse_opts(failp,(parse_t){__VA_ARGS__})
static sd_val_t *
parse_opts (const char **failp, parse_t o)
{                               // Parse in to s, and return next character
   const char **digit = NULL;
   int getdigit (const char **d, const char *p, const char **pp)
   {
      int l;
      for (int q = 0; d[q]; q++)
         if ((l = comp (d[q], p)))
         {
            if (pp)
               *pp = p + l;
            if (d == digitcomma || d == digitpoint)
               d = digitnormal;
            digit = d;
            return q;
         }
      return -1;
   }
   int getdigits (const char *p, const char **pp)
   {
      if (digit)
         return getdigit (digit, p, pp);
      int v;
      for (const char ***d = digits; *d; d++)
         if ((v = getdigit (*d, p, pp)) >= 0)
            return v;
      return -1;
   }
   if (!o.v)
      return NULL;
   if (o.end)
      *o.end = o.v;
   if (o.placesp)
      *o.placesp = 0;
   int v;
   const char *skip;
   char neg = 0;
   if (*o.v == '-')
   {
      neg ^= 1;                 // negative
      o.v++;
   } else if (*o.v == '+')
   {
      o.v++;                    // Somewhat redundant
   } else if ((v = getdigits (o.v, &skip)) == 10)
   {
      neg ^= 1;                 // negative
      o.v = skip;
   } else if (v == 11)
   {
      o.v = skip;               // positive
   }
   sd_val_t *s = NULL;
   const char *digits = o.v;
   {
      int d = 0,                // Digits before point (ignoring leading zeros)
         p = 0,                 // Places after point
         l = 0,                 // Leading zeros
         t = 0;                 // Trailing zeros
      void nextdigit (void)
      {
         if (v || d)
         {
            if (!d++)
               digits = o.v;
            if (!v)
               t++;
            else
               t = 0;
         } else
            l++;
         o.v = skip;
      }
      while (*o.v)
      {                         // Initial digits
         if (!o.nocomma && sd_comma)
         {                      // Check commas
            int z = -1;
            if (*o.v == sd_comma
                || (sd_comma == ',' && (!digit || digit == digitnormal) && (z = getdigit (digitcomma, o.v, &skip) >= 0)))
            {                   // Comma...
               if (z < 0)
                  skip = o.v + 1;
               const char *q = skip,
                  *qq;
               if ((v = getdigits (q, &q)) >= 0 && v < 10 &&    //
                   (v = getdigits (q, &q)) >= 0 && v < 10 &&    //
                   (((v = getdigits (qq = q, &q)) >= 0 && v < 10 && ((v = getdigits (q, &q)) < 0 || v > 9)) ||  //
                    (z >= 0 && (v = getdigit (digitcomma, qq, &q)) >= 0 && (v = getdigits (q, &q)) >= 0 && v < 10)))
               {                // Either three digits and non comma, or two digits and comma-digit and digit
                  if (z >= 0)
                     nextdigit ();
                  o.v = skip;
                  continue;
               }
            }
         }
         if (sd_point == '.' && (!digit || digit == digitnormal) && (v = getdigit (digitpoint, o.v, &skip)) >= 0)
         {                      // Digit point
            nextdigit ();
            break;              // found point.
         } else if ((v = getdigits (o.v, &skip)) < 0 || v > 9)
            break;
         nextdigit ();
      }
      if (*o.v == sd_point)
         o.v++;
      while ((v = getdigits (o.v, &skip)) >= 0 && v < 10)
      {
         nextdigit ();
         p++;
      }
      if (d)
      {
         s = make (failp, d - p - 1, d - t);
         if (o.placesp)
            *o.placesp = p;
      } else if (l)
         s = copy (failp, &zero);       // No digits
   }
   if (!s)
      return s;
   // Load digits
   int q = 0;
   while (*digits && q < s->sig)
   {
      int v = getdigits (digits, &skip);
      if (v < 0 && !o.nocomma && sd_comma == ',' && digit == digitnormal)
         v = getdigit (digitcomma, digits, &skip);
      if (v < 0 && sd_point == '.' && digit == digitnormal)
         v = getdigit (digitpoint, digits, &skip);
      if (v >= 0 && v < 10)
      {
         s->d[q++] = v;
         digits = skip;
      } else
         digits++;              // Advance over non digits, e.g. comma, point
   }
   if ((*o.v == 'e' || *o.v == 'E')
       && (((o.v[1] == '+' || o.v[1] == '-') && (v = getdigits (o.v + 2, NULL)) >= 0 && v < 10)
           || ((v = getdigits (o.v + 1, NULL)) >= 0 && v < 10)))
   {                            // Exponent (may clash with E SI prefix if not careful)
      o.v++;
      int sign = 1,
         e = 0;
      if (*o.v == '+')
         o.v++;
      else if (*o.v == '-')
      {
         o.v++;
         sign = -1;
      }
      int v;
      while ((v = getdigits (o.v, &skip)) >= 0 && v < 10)
      {
         e = e * 10 + v;        // Only advances if digit
         o.v = skip;
      }
      s->mag += e * sign;
      checkmax (failp, s->mag, s->sig);
   }
   if (o.end)
      *o.end = o.v;             // End of parsing
   if (!s->sig)
      s->mag = 0;               // Zero
   else
      s->neg = neg;
   return s;
}

static sd_val_t *
make_int (const char **failp, long long l)
{                               // Int...
   char temp[40];
   snprintf (temp, sizeof (temp), "%lld", l);
   return parse (failp, temp);
}

const char *
sd_check_opts (sd_parse_t o)
{
   const char *e = NULL;
   const char *failp = NULL;
 sd_val_t *s = parse (&failp, o.a, end: &e, nocomma:o.nocomma);
   if (!s)
      return NULL;
   freez (s);
   if (o.end)
      *o.end = e;
   if (o.a_free)
      freez (o.a);
   if (failp && o.failure && !*o.failure)
      *o.failure = failp;
   if (failp)
      return NULL;
   return e;
}

typedef struct
{
   sd_val_t *s;
   FILE *O;                     // output to a file
   const char *currency;
   unsigned char comma:1;
   unsigned char combined:1;
} output_t;
#define output(...) output_opts((output_t){__VA_ARGS__})
static char *
output_opts (output_t o)
{                               // Convert to a string (malloced)
   sd_val_t *s = o.s;
   if (!s)
      return NULL;
#ifdef DEBUG
   //fprintf(stderr,"output mag=%d sig=%d\n",s->mag,s->sig);
#endif
   char *buf = NULL;
   size_t len = 0;
   FILE *O = o.O;
   if (!O)
      O = open_memstream (&buf, &len);
   if (s->neg)
      fputc ('-', O);
   if (o.currency)
      fprintf (O, "%s", o.currency);
   int q = 0;
   if (s->mag < 0)
   {
      if (o.combined && sd_point == '.' && (s->sig || s->mag < -1))
         fprintf (O, "%s", digitpoint[0]);
      else
         fputc ('0', O);
      if (s->sig || s->mag < -1)
      {
         if (!o.combined || sd_point != '.')
            fputc (sd_point, O);
         for (q = 0; q < -1 - s->mag; q++)
            fputc ('0', O);
         for (q = 0; q < s->sig; q++)
            fputc ('0' + s->d[q], O);
      }
   } else
   {
      void nextdigit (int v)
      {
         if (o.combined && o.comma && sd_comma == ',' && q < s->mag && !((s->mag - q) % 3))
            fprintf (O, "%s", digitcomma[v]);
         else if (o.combined && sd_point == '.' && q == s->mag && q + 1 < s->sig)
            fprintf (O, "%s", digitpoint[v]);
         else
         {
            if ((!o.combined || sd_point != '.') && sd_point && q == s->mag + 1)
               fputc (sd_point, O);
            fputc ('0' + v, O);
            if (o.comma && sd_comma && q < s->mag && !((s->mag - q) % 3))
               fputc (sd_comma, O);
         }
      }
      for (; q <= s->mag && q < s->sig; q++)
         nextdigit (s->d[q]);
      if (s->sig > s->mag + 1)
         for (; q < s->sig; q++)
            nextdigit (s->d[q]);
      else
         for (; q <= s->mag; q++)
            nextdigit (0);
   }
   if (o.O)
      return NULL;              // Output is to file
   fclose (O);
   return buf;
}

#ifdef DEBUG
void
debugout (const char *msg, ...)
{
   fprintf (stderr, "%s:", msg);
   va_list ap;
   va_start (ap, msg);
   sd_val_t *s;
   while ((s = va_arg (ap, sd_val_t *)))
   {
      char *v = output (s);
      fprintf (stderr, " %s", v);
      freez (v);
   }
   va_end (ap);
   fprintf (stderr, "\n");
}

void
sd_debugout (const char *msg, ...)
{
   fprintf (stderr, "%s:", msg);
   va_list ap;
   va_start (ap, msg);
   sd_t *s;
   while ((s = va_arg (ap, sd_t *)))
   {
      char *v = output (s->n);
      fprintf (stderr, " %s", v);
      freez (v);
      if (s->d)
      {
         char *v = output (s->d);
         fprintf (stderr, "/%s", v);
         freez (v);
      }
   }
   va_end (ap);
   fprintf (stderr, "\n");
}
#else
#define debugout(msg,...)
#define sd_debugout(msg,...)
#endif

typedef struct
{
   sd_val_t *a;
   sd_val_t *b;
   sd_val_t *c;
   sd_val_t *d;
   FILE *O;
   const char *currency;
   unsigned char comma:1;
   unsigned char combined:1;
} output_f_t;

#define output_f(...)	output_f_opts((output_f_t){__VA_ARGS__})
static char *
output_f_opts (output_f_t o)
{                               // Convert first arg to string, but free multiple args
 char *r = output (o.a, comma: o.comma, combined: o.combined, currency: o.currency, O:o.O);
   freez (o.a);
   freez (o.b);
   freez (o.c);
   freez (o.d);
   return r;
}

// Low level maths functions

static int
ucmp (const char **failp, sd_val_t * a, sd_val_t * b, int boffset)
{                               // Unsigned compare (magnitude only), if a>b then 1, if a<b then -1, else 0 for equal
   if (!a)
      a = &zero;
   if (!b)
      b = &zero;
   norm (a);
   norm (b);
   //debugout("ucmp", a, b, NULL);
   if (!a->sig && !b->sig)
      return 0;                 // Zeros
   if (a->sig && !b->sig)
      return 1;                 // Zero compare
   if (!a->sig && b->sig)
      return -1;                // Zero compare
   if (a->mag > boffset + b->mag)
      return 1;                 // Simple magnitude difference
   if (a->mag < boffset + b->mag)
      return -1;                // Simple magnitude difference
   int sig = a->sig;            // Max digits to compare
   if (b->sig < sig)
      sig = b->sig;
   int p;
   for (p = 0; p < sig && a->d[p] == b->d[p]; p++);
   if (p < sig)
   {                            // Compare digit as not reached end
      if (a->d[p] < b->d[p])
         return -1;
      if (a->d[p] > b->d[p])
         return 1;
   }
   // Compare length
   if (a->sig > p)
      return 1;                 // More digits
   if (b->sig > p)
      return -1;                // More digits
   return 0;
}

typedef struct
{
   sd_val_t *a;
   sd_val_t *b;
   int boffset;
   unsigned char neg:1;
   unsigned char a_free:1;      // If set, will re-use a or free it
} uadd_t;
#define	uadd(failp,...) uadd_opts(failp,(uadd_t){__VA_ARGS__})
static sd_val_t *
uadd_opts (const char **failp, uadd_t o)
{                               // Unsigned add (i.e. final sign already set in r) and set in r if needed
   sd_val_t *a = o.a ? : &zero;
   sd_val_t *b = o.b ? : &zero;
   //debugout("uadd", a, b, NULL);
   int mag = a->mag;            // Max mag
   if (!a->sig || o.boffset + b->mag > mag)
      mag = o.boffset + b->mag;
   mag++;                       // allow for extra digit
   int end = a->mag - a->sig;
   if (!a->sig || o.boffset + b->mag - b->sig < end)
      end = o.boffset + b->mag - b->sig;
   sd_val_t *r = NULL;
   if (o.a_free)
   {                            // Check if we can use a
      if (a->mag + (a->d - a->m) >= mag && a->mag + (a->d - a->m) - a->max <= end)
      {                         // reuse a
         while (a->mag < mag)
         {
            *--a->d = 0;
            a->mag++;
            a->sig++;
         }
         if (!a->sig && a->mag > mag)
         {
            a->d += a->mag - mag;
            a->mag = mag;
         }
         while (a->sig < mag - end)
            a->d[a->sig++] = 0;
         r = a;
         o.a_free = 0;
      } else
         warnx ("Not reusing in uadd (%d<%d || %d>%d)", (int) (a->mag + (a->d - a->m)), mag,
                (int) (a->mag + (a->d - a->m) - a->max), end);
   }
   if (!r)
      r = make (failp, mag, mag - end);
   if (r)
   {
      int c = 0;
      for (int p = end + 1; p <= mag; p++)
      {
         int v = c;
         if (p <= a->mag && p > a->mag - a->sig)
            v += a->d[a->mag - p];
         if (p <= o.boffset + b->mag && p > o.boffset + b->mag - b->sig)
            v += b->d[o.boffset + b->mag - p];
         c = 0;
         if (v >= 10)
         {
            c = 1;
            v -= 10;
         }
         if (!(p <= r->mag && p > r->mag - r->sig))
            errx (1, "uadd fail");
         r->d[r->mag - p] = v;
      }
      if (c)
         errx (1, "Carry add error %d", c);
   }
   if (o.a_free)
      freez (o.a);
 return norm (r, neg:o.neg);
}

typedef struct
{
   sd_val_t *a;
   sd_val_t *b;
   int boffset;
   unsigned char neg:1;
   unsigned char a_free:1;      // If set, will re-use a or free it
} usub_t;
#define usub(failp,...) usub_opts(failp,(usub_t){__VA_ARGS__})
static sd_val_t *
usub_opts (const char **failp, usub_t o)
{                               // Unsigned sub (i.e. final sign already set in r) and set in r if needed, and assumes b<=a already
   sd_val_t *a = o.a ? : &zero;
   sd_val_t *b = o.b ? : &zero;
   //debugout("usub", a, b, NULL);
   int mag = a->mag;            // Max mag
   if (o.boffset + b->mag > a->mag)
      mag = o.boffset + b->mag;
   int end = a->mag - a->sig;
   if (o.boffset + b->mag - b->sig < end)
      end = o.boffset + b->mag - b->sig;
   sd_val_t *r = NULL;
   if (o.a_free)
   {                            // Check if we can use a
      if (a->mag + (a->d - a->m) >= mag && a->mag + (a->d - a->m) - a->max <= end)
      {                         // reuse a
         while (a->mag < mag)
         {
            *--a->d = 0;
            a->mag++;
            a->sig++;
         }
         if (!a->sig && a->mag > mag)
         {
            a->d += a->mag - mag;
            a->mag = mag;
         }
         while (a->sig < mag - end)
            a->d[a->sig++] = 0;
         r = a;
         o.a_free = 0;
      } else
         warnx ("Not reusing in usub (%d<%d || %d>%d)", (int) (a->mag + (a->d - a->m)), mag,
                (int) (a->mag + (a->d - a->m) - a->max), end);
   }
   if (!r)
      r = make (failp, mag, mag - end);
   if (r)
   {
      int c = 0;
      for (int p = end + 1; p <= mag; p++)
      {
         int v = c;
         if (p <= a->mag && p > a->mag - a->sig)
            v += a->d[a->mag - p];
         if (p <= o.boffset + b->mag && p > o.boffset + b->mag - b->sig)
            v -= b->d[o.boffset + b->mag - p];
         c = 0;
         if (v < 0)
         {
            c = -1;
            v += 10;
         }
         r->d[r->mag - p] = v;
      }
      if (c)
         errx (1, "Carry sub error %d", c);
   }
   if (o.a_free)
      freez (o.a);
 return norm (r, neg:o.neg);
}

static void
makebase (const char **failp, sd_val_t * r[9], sd_val_t * a)
{                               // Make array of multiples of a, 1 to 9, used for multiply and divide
   r[0] = copy (failp, a);
   for (int n = 1; n < 9; n++)
      r[n] = uadd (failp, r[n - 1], a);
}

static void
free_base (sd_val_t * r[9])
{
   for (int n = 0; n < 9; n++)
      freez (r[n]);
}

typedef struct
{
   sd_val_t *a;
   sd_val_t *b;
   unsigned char neg:1;
   unsigned char a_free:1;
   unsigned char b_free:1;
} umul_t;
#define umul(failp,...) umul_opts(failp,(umul_t){__VA_ARGS__})
static sd_val_t *
umul_opts (const char **failp, umul_t o)
{                               // Unsigned mul (i.e. final sign already set in r) and set in r if needed
   sd_val_t *a = o.a ? : &zero;
   sd_val_t *b = o.b ? : &zero;
   debugout ("umul", a, b, NULL);
   sd_val_t *base[9];
   makebase (failp, base, b);
   int mag = a->mag + b->mag + 4;       // Allow plenty of space
   int sig = a->sig + b->sig + 10;
   sd_val_t *r = make (failp, mag, sig);
   r->sig = 0;                  // Is zero, we just made it big enough to re-use
   for (int p = 0; p < a->sig; p++)
      if (a->d[p])
       r = uadd (failp, r, base[a->d[p] - 1], boffset: a->mag - p, a_free:1);
   free_base (base);
   if (o.a_free)
      freez (o.a);
   if (o.b_free)
      freez (o.b);
 return norm (r, neg:o.neg);
}

typedef struct
{
   sd_val_t *a;
   sd_val_t *b;
   sd_val_t **rem;
   int places;
   sd_round_t round;
   unsigned char neg:1;         // Negative
   unsigned char pad:1;         // Pad to specified places
   unsigned char sig:1;         // Places is sig figures
   unsigned char a_free:1;
   unsigned char b_free:1;
} udiv_t;
#define udiv(failp,...) udiv_opts(failp,(udiv_t){__VA_ARGS__})
static sd_val_t *
udiv_opts (const char **failp, udiv_t o)
{                               // Unsigned div (i.e. final sign already set in r) and set in r if needed
   sd_val_t *a = o.a ? : &zero;
   sd_val_t *b = o.b ? : &zero;
   sd_val_t *r = NULL;
   debugout ("udiv", a, b, NULL);
   sd_val_t *base[9] = { };
   if (!b->sig)
   {
      if (failp && !*failp)
         *failp = "Division by zero";
   } else
   {
      makebase (failp, base, b);
      int mag = a->mag - b->mag;
      int sig = mag + o.places + 1;     // Limit to places
      if (o.sig)
         sig = o.places;
      if (o.sig && ucmp (failp, a, b, mag) < 0)
         mag--;
      if (sig < 0)
         sig = 0;
      r = make (failp, mag + 2, sig + 2);
      r->d += 2;                // Allowed more space for the rounding in-situ, including allowing for round at end
      r->mag -= 2;
      r->sig -= 2;
      if (r)
      {
         sd_val_t *v = make (failp, a->mag, b->sig + sig + 1);
         if (!v)
         {
            free_base (base);
            freez (r);
            return v;
         }
         memcpy (v->d, a->d, v->sig = a->sig);
         for (int p = mag; p > mag - sig; p--)
         {
            int n = 0;
            while (n < 9 && ucmp (failp, v, base[n], p) >= 0)
               n++;
            //debugout ("udiv rem", v, NULL);
            if (n)
             v = usub (failp, v, base[n - 1], boffset: p, a_free:1);
            r->d[mag - p] = n;
            if (!o.pad && !v->sig)
               break;
         }
         if (o.round != SD_ROUND_TRUNCATE && v->sig)
         {                      // Rounding
            if (!o.round)
               o.round = SD_ROUND_BANKING;      // Default
            if (o.neg)
            {                   // reverse logic for +/-
               if (o.round == SD_ROUND_FLOOR)
                  o.round = SD_ROUND_CEILING;
               else if (o.round == SD_ROUND_CEILING)
                  o.round = SD_ROUND_FLOOR;
            }
            int shift = mag - sig;
            int diff = ucmp (failp, v, base[4], shift);
            if (o.round == SD_ROUND_UP ||       // Round up
                o.round == SD_ROUND_CEILING     // Round up
                || (o.round == SD_ROUND_ROUND && diff >= 0)     // Round up if 0.5 and above up
                || (o.round == SD_ROUND_NI && diff > 0) // Round up if above 0.5
                || (o.round == SD_ROUND_BANKING && diff > 0)    // Round up if above 0.5
                || (o.round == SD_ROUND_BANKING && !diff && (r->d[r->sig - 1] & 1))     // Round up if 0.5 and odd previous digit
               )
            {                   // Add one
               if (o.rem)
               {                // Adjust remainder, goes negative
                  base[0]->mag += shift + 1;
                  sd_val_t *s = usub (failp, base[0], v);
                  base[0]->mag -= shift + 1;
                  freez (v);
                  v = s;
                  v->neg ^= 1;
               }
               // Adjust r
             r = uadd (failp, r, &one, boffset: r->mag - r->sig + 1, a_free:1);
               if (o.sig)
                  r->sig = sig;
            }
         }
         if (o.rem)
         {
            if (b->neg)
               v->neg ^= 1;
            if (o.neg)
               v->neg ^= 1;
            *o.rem = v;
         } else
            freez (v);
      }
   }
   free_base (base);
   if (o.a_free)
      freez (o.a);
   if (o.b_free)
      freez (o.b);
 return norm (r, neg: o.neg, pad:o.pad);
}

typedef struct
{
   sd_val_t *a;
   sd_val_t *b;
   unsigned char a_free:1;
   unsigned char b_free:1;
} scmp_t;
#define scmp(failp,...) scmp_opts(failp,(scmp_t){__VA_ARGS__})
static int
scmp_opts (const char **failp, scmp_t o)
{
   sd_val_t *a = o.a ? : &zero;
   sd_val_t *b = o.b ? : &zero;
   debugout ("scmp", a, b, NULL);
   if (a->neg && !b->neg)
      return -1;
   if (!a->neg && b->neg)
      return 1;
   if (a->neg && b->neg)
      return -ucmp (failp, a, b, 0);
   int diff = ucmp (failp, a, b, 0);;
   if (o.a_free)
      freez (o.a);
   if (o.b_free)
      freez (o.b);
   return diff;
}

static sd_val_t *
sadd (const char **failp, sd_val_t * a, sd_val_t * b)
{                               // Low level add
   if (!a)
      a = &zero;
   if (!b)
      b = &zero;
   debugout ("sadd", a, b, NULL);
   if (a->neg && !b->neg)
   {                            // Reverse subtract
      sd_val_t *t = a;
      a = b;
      b = t;
   }
   if (!a->neg && b->neg)
   {                            // Subtract
      int d = ucmp (failp, a, b, 0);
      if (d < 0)
       return usub (failp, b, a, neg:1);
      return usub (failp, a, b);
   }
 return uadd (failp, a, b, neg:(a->neg && b->neg));
}

static sd_val_t *
ssub (const char **failp, sd_val_t * a, sd_val_t * b)
{
   if (!a)
      a = &zero;
   if (!b)
      b = &zero;
   debugout ("ssub", a, b, NULL);
   if (a->neg && !b->neg)
    return uadd (failp, a, b, neg:1);
   if (!a->neg && b->neg)
      return uadd (failp, a, b);
   char neg = 0;
   if (a->neg && b->neg)
      neg ^= 1;                 // Invert output
   int d = ucmp (failp, a, b, 0);
   if (!d)
      return copy (failp, &zero);       // Zero
   if (d < 0)
    return usub (failp, b, a, neg:1 - neg);
 return usub (failp, a, b, neg:neg);
}

static sd_val_t *
smul (const char **failp, sd_val_t * a, sd_val_t * b)
{
   if (!a)
      a = &zero;
   if (!b)
      b = &zero;
   debugout ("smul", a, b, NULL);
   if ((a->neg && !b->neg) || (!a->neg && b->neg))
    return umul (failp, a, b, neg:1);
   return umul (failp, a, b);
}

typedef struct
{
   sd_val_t *a;
   sd_val_t *b;
   sd_val_t **rem;
   int places;
   sd_round_t round;
   unsigned char pad:1;         // Pad to specified places
   unsigned char sig:1;         // Places is sig figures
   unsigned char a_free:1;
   unsigned char b_free:1;
} sdiv_t;
#define sdiv(failp,...) sdiv_opts(failp,(sdiv_t){__VA_ARGS__})
static sd_val_t *
sdiv_opts (const char **failp, sdiv_t o)
{
   sd_val_t *a = o.a ? : &zero;
   sd_val_t *b = o.b ? : &zero;
   debugout ("sdiv", a, b, NULL);
 sd_val_t *v = udiv (failp, a, b, neg: (a->neg && !b->neg) || (!a->neg && b->neg), rem: o.rem, places: o.places, round: o.round, pad: o.pad, sig:o.sig);
   if (o.a_free)
      freez (o.a);
   if (o.b_free)
      freez (o.b);
   return v;
}

typedef struct
{
   sd_val_t *a;
   int places;
   sd_round_t round;
   unsigned char nocap:1;       // Cap to specified places
   unsigned char pad:1;         // Pad to specified places
   unsigned char sig:1;         // Places+1 is sig figures
} srnd_t;
#define srnd(failp,...) srnd_opts(failp,(srnd_t){__VA_ARGS__})
static sd_val_t *
srnd_opts (const char **failp, srnd_t o)
{
   sd_val_t *a = o.a;
   debugout ("srnd", a, NULL);
   if (!a)
      return NULL;
   int decimals = a->sig - a->mag - 1;
   if (decimals < 0)
      decimals = 0;
   if (o.sig)
      o.places = o.places - a->mag - 1; // Places+1 is sig figures
   if (!o.pad && o.places > decimals)
      o.places = decimals;      // Not padding
   if (o.nocap && o.places < decimals)
      o.places = decimals;      // Not capping
   sd_val_t *z (void)
   {
      sd_val_t *r = copy (failp, &zero);
      r->mag = -o.places;
      if (o.places > 0)
         r->mag--;
      return r;
   }
   if (!a->sig)
      return z ();
   if (decimals == o.places)
      return copy (failp, a);   // Already that many places
   if (decimals > o.places)
   {                            // more places, needs truncating
      int sig = a->sig - (decimals - o.places);
      if (a->sig < a->mag + 1)
         sig = a->mag + 1 - (decimals - o.places);
      sd_val_t *r = NULL;
      if (sig <= 0)
      {
         r = z ();
         if (sig < 0)
            return r;
         sig = 0;               // Allow rounding
      } else
      {
         r = make (failp, a->mag, sig);
         if (!r)
            return r;
         memcpy (r->d, a->d, sig);
      }
      if (o.round != SD_ROUND_TRUNCATE)
      {
         int p = sig;
         char up = 0;
         if (!o.round)
            o.round = SD_ROUND_BANKING;
         if (a->neg)
         {                      // reverse logic for +/-
            if (o.round == SD_ROUND_FLOOR)
               o.round = SD_ROUND_CEILING;
            else if (o.round == SD_ROUND_CEILING)
               o.round = SD_ROUND_FLOOR;
         }
         if (o.round == SD_ROUND_CEILING || o.round == SD_ROUND_UP)
         {                      // Up (away from zero) if not exact
            while (p < a->sig && !a->d[p])
               p++;
            if (p < a->sig)
               up = 1;          // not exact
         } else if (o.round == SD_ROUND_ROUND && a->d[p] >= 5)  // Up if .5 or above
            up = 1;
         else if (o.round == SD_ROUND_BANKING || o.round == SD_ROUND_NI)
         {                      // Bankers and NI depend on exact 0.5 special cases
            if (a->d[p] > 5)
               up = 1;          // Up as more than .5
            else if (a->d[p] == 5)
            {                   // Check if exactly 0.5, if above then we round up
               p++;
               while (p < a->sig && !a->d[p])
                  p++;
               if (p < a->sig)
                  up = 1;       // greater than .5
               else if (o.round != SD_ROUND_NI && a->d[sig - 1] & 1)
                  up = 1;       // exactly .5 and odd, so move to even for bankers or down for NI
            }
         }
         if (up)
         {                      // Round up (away from 0)
          sd_val_t *s = uadd (failp, r, &one, boffset:r->mag - r->sig + 1);
            freez (r);
            r = s;
            decimals = r->sig - r->mag - 1;
            if (decimals < 0)
               decimals = 0;
            if (decimals < o.places)
            {
               int sig = r->sig + (o.places - decimals);
               if (r->mag > 0)
                  sig = r->mag + 1 + o.places;
               sd_val_t *s = make (failp, r->mag, sig);
               if (!s)
               {
                  freez (r);
                  return s;
               }
               memcpy (s->d, r->d, r->sig);
               s->neg = r->neg;
               freez (r);
               r = s;
            }
         }
      }
      r->neg = a->neg;
      return r;
   }
   if (decimals < o.places)
   {                            // Artificially extend places, non normalised
      int sig = a->sig + (o.places - decimals);
      if (a->mag > 0)
         sig = a->mag + 1 + o.places;
      sd_val_t *r = make (failp, a->mag, sig);
      if (!r)
         return r;
      memcpy (r->d, a->d, a->sig);
      r->neg = a->neg;
      return r;
   }
   return NULL;                 // Uh
}

// Maths string functions

char *
stringdecimal_add_opts (stringdecimal_binary_t o)
{                               // Simple add
 sd_val_t *A = parse (o.failure, o.a, nocomma:o.nocomma);
 sd_val_t *B = parse (o.failure, o.b, nocomma:o.nocomma);
   sd_val_t *R = sadd (o.failure, A, B);
 char *ret = output_f (R, A, B, comma: o.comma, combined:o.combined);
   if (o.a_free)
      freez (o.a);
   if (o.b_free)
      freez (o.b);
   return ret;
};

char *
stringdecimal_sub_opts (stringdecimal_binary_t o)
{
   // Simple subtract
 sd_val_t *A = parse (o.failure, o.a, nocomma:o.nocomma);
 sd_val_t *B = parse (o.failure, o.b, nocomma:o.nocomma);
   sd_val_t *R = ssub (o.failure, A, B);
 char *ret = output_f (R, A, B, comma: o.comma, combined:o.combined);
   if (o.a_free)
      freez (o.a);
   if (o.b_free)
      freez (o.b);
   return ret;
};

char *
stringdecimal_mul_opts (stringdecimal_binary_t o)
{
   // Simple multiply
 sd_val_t *A = parse (o.failure, o.a, nocomma:o.nocomma);
 sd_val_t *B = parse (o.failure, o.b, nocomma:o.nocomma);
   sd_val_t *R = smul (o.failure, A, B);
 char *ret = output_f (R, A, B, comma:o.comma);
   if (o.a_free)
      freez (o.a);
   if (o.b_free)
      freez (o.b);
   return ret;
};

char *
stringdecimal_div_opts (stringdecimal_div_t o)
{
   // Simple divide - to specified number of places, with remainder
 sd_val_t *B = parse (o.failure, o.b, nocomma:o.nocomma);
 sd_val_t *A = parse (o.failure, o.a, nocomma:o.nocomma);
   sd_val_t *REM = NULL;
 sd_val_t *R = sdiv (o.failure, A, B, rem: &REM, places: o.places, round:o.round);
   if (o.remainder)
    *o.remainder = output (REM, comma: o.comma, combined: o.combined, currency:o.currency);
   if (o.a_free)
      freez (o.a);
   if (o.b_free)
      freez (o.b);
 return output_f (R, A, B, REM, comma: o.comma, combined:o.combined);
};

char *
stringdecimal_rnd_opts (stringdecimal_unary_t o)
{
   // Round to specified number of places
 sd_val_t *A = parse (o.failure, o.a, nocomma:o.nocomma);
 sd_val_t *R = srnd (o.failure, A, places: o.places, round: o.round, pad:1);
 char *ret = output_f (R, A, comma: o.comma, combined:o.combined);
   if (o.a_free)
      freez (o.a);
   return ret;
};

int
stringdecimal_cmp_opts (stringdecimal_binary_t o)
{
   // Compare
 sd_val_t *A = parse (o.failure, o.a, nocomma:o.nocomma);
 sd_val_t *B = parse (o.failure, o.b, nocomma:o.nocomma);
   int r = scmp (o.failure, A, B);
   freez (A);
   freez (B);
   if (o.a_free)
      freez (o.a);
   if (o.b_free)
      freez (o.b);
   return r;
}

// SD functions

static sd_p
sd_make (const char *failure)
{                               // Make sd_p
   sd_p v = calloc (1, sizeof (*v));
   if (!v)
      return v;
   v->failure = failure;
   return v;
}

static sd_p
sd_new (sd_p l, sd_p r)
{                               // Basic details for binary operator
   sd_p v = sd_make (NULL);
   if (!v)
      return v;
   if (l)
      v->places = l->places;
   if (r && r->places > v->places)
      v->places = r->places;
   if (l && l->failure)
      v->failure = l->failure;
   if (r && r->failure && v->failure)
      v->failure = r->failure;
   return v;
}

#define sd_rnd_val(...)	sd_rnd_val_opts((sd_rnd_t){__VA_ARGS__})
static sd_val_t *
sd_rnd_val_opts (sd_rnd_t o)
{                               // Do sensible rounding in situ
   if (o.p->d)
    return sdiv (&o.p->failure, o.p->n, o.p->d, places: o.places, round: o.round, pad: o.pad, sig:o.sig);
 return srnd (&o.p->failure, o.p->n, places: o.places, round: o.round, nocap: o.nocap, pad: o.pad, sig:o.sig);
}

static struct sd_s sd_zero = { &zero };

sd_p
sd_rnd_opts (sd_rnd_t o)
{                               // Round to an sd_p
   if (!o.p)
   {
      o.p = &sd_zero;
      o.p_free = 0;
   }
   sd_p r = sd_new (o.p, NULL);
   if (r)
      r->n = sd_rnd_val_opts (o);
   if (o.p_free)
      sd_free (o.p);
   return r;
}

static sd_p
sd_tidy (sd_p v)
{                               // Check answer
   if (v && v->d && v->d->neg)
   {                            // Normalise sign
      v->n->neg ^= 1;
      v->d->neg = 0;
   }
   if (v && v->n && v->d && v->d->sig == 1 && v->d->d[0] == 1)
   {                            // Power of 10 denominator
      v->n->mag -= v->d->mag;
      freez (v->d);
   }
   if (v->n)
      checkmax (&v->failure, v->n->mag, v->n->sig);
   if (v->d)
      checkmax (&v->failure, v->d->mag, v->n->sig);
   return v;
}

static sd_p
sd_cross (sd_p l, sd_p r, sd_val_t ** ap, sd_val_t ** bp)
{                               // Cross multiply
   sd_debugout ("sd_cross", l, r, NULL);
   *ap = *bp = NULL;
   sd_p v = sd_new (l, r);
   if ((l->d || r->d) && scmp (&v->failure, l->d ? : &one, r->d ? : &one))
   {                            // Multiply out numerators
      *ap = smul (&v->failure, l->n, r->d ? : &one);
      *bp = smul (&v->failure, r->n, l->d ? : &one);
      debugout ("sd_crossed", *ap, *bp, NULL);
   }
   return v;
}

sd_p
sd_copy (sd_p p)
{                               // Copy (create if p is NULL)
   if (!p || !p->n)
      p = &sd_zero;
   sd_p v = sd_make (NULL);
   if (!v)
      return v;
   v->failure = p->failure;
   v->places = p->places;
   if (p->n)
      v->n = copy (&v->failure, p->n);
   if (p->d)
      v->d = copy (&v->failure, p->d);
   return v;
}

sd_p
sd_parse_opts (sd_parse_t o)
{
   int places = 0;
   sd_p v = sd_make (NULL);
   if (!v)
      return v;
   int f = FRACTIONS;
   sd_val_t *n = NULL;
   const char *p = o.a,
      *end;
   int l;
   if (!o.nofrac)
      for (f = 0; f < FRACTIONS && !(l = comp (fraction[f].value, p)); f++);
   if (f < FRACTIONS)
   {                            // Just a fraction
      p += l;
      n = v->n = make_int (&v->failure, fraction[f].n);
      if (fraction[f].d < 0)
      {                         // 1/N
       n = parse (&v->failure, p, placesp: &places, nocomma: o.nocomma, end:&end);
         if (n)
         {
            p = end;
            v->d = n;
         }
      } else
         v->d = make_int (&v->failure, fraction[f].d);
   } else
   {                            // Normal
    n = parse (&v->failure, p, placesp: &places, nocomma: o.nocomma, end:&end);
      if (n && !v->failure)
      {
         p = end;
         v->n = n;
         if (!o.nofrac && n && n->mag <= 0 && n->mag + 1 >= n->sig)
         {                      // Integer, follow by fraction
            for (f = 0; f < FRACTIONS && !(l = comp (fraction[f].value, p)); f++);
            if (f < FRACTIONS && fraction[f].d >= 0)
            {
               p += l;
               v->d = make_int (&v->failure, fraction[f].d);
               n = smul (&v->failure, v->n, v->d);
               freez (v->n);
               v->n = n;
               sd_val_t *a = make_int (&v->failure, fraction[f].n);
               n = sadd (&v->failure, v->n, a);
               freez (v->n);
               v->n = n;
               freez (a);
            }
         }
      }
   }
   f = IEEES;
   if (!o.noieee && v->n && !v->failure)
      for (f = 0; f < IEEES && !(l = comp (ieee[f].value, p)); f++);
   if (f < IEEES)
   {
      p += l;
      if (v->n->sig)
      {
         sd_val_t *m = make_int (&v->failure, ieee[f].mul);
         n = smul (&v->failure, v->n, m);
         freez (v->n);
         v->n = n;
         freez (m);
      }
   } else
   {
      f = SIS;
      if (!o.nosi && v->n && !v->failure)
         for (f = 0; f < SIS && !(l = comp (si[f].value, p)); f++);
      if (f < SIS)
      {
         p += l;
         if (v->n->sig)
            sd_10_i (v, si[f].mag);
      }
   }
   if (n)
      v->places = places;
   if (o.end)
      *o.end = p;
   if (o.a_free)
      freez (o.a);
   if (!n && !v->failure)
      return sd_free (v);       // NULL
   return v;
}

sd_p
sd_int (long long v)
{
   char temp[40];
   snprintf (temp, sizeof (temp), "%lld", v);
   return sd_parse (temp);
}

sd_p
sd_float (long double v)
{
   char temp[50];
   snprintf (temp, sizeof (temp), "%.32Le", v);
   return sd_parse (temp);
}

const char *
sd_fail (sd_p p)
{                               // Failure string
   if (!p)
      return "Null";
   if (p->failure)
      return p->failure;
   return NULL;
}

void
sd_delete (sd_p * pp)
{                               // Free in situ
   *pp = sd_free (*pp);
}

void *
sd_free (sd_p p)
{                               // Free
   if (!p)
      return p;
   freez (p->d);
   freez (p->n);
   freez (p);
   return NULL;
}

int
sd_places (sd_p p)
{                               // Max places seen
   if (!p)
      return INT_MIN;
   return p->places;
}

char *
sd_output_opts (sd_output_opts_t o)
{                               // Output
   const char *failp = NULL;
   sd_p p = o.p ? : &sd_zero;
   if (!o.format)
   {                            // Defaults
      if (!o.places)
      {
         o.format = SD_FORMAT_LIMIT;    // Extra for divide
         o.places = -3;
      } else
         o.format = SD_FORMAT_EXACT;    // Exact places
   }
   int guess (char sig)
   {                            // Guess places
      if (o.places >= 0)
         return o.places + sig;
      int q = 0,
         d = 0;
      if (sig)
      {
         if (p->n && (d = p->n->sig) > q)
            q = d;
         if (p->d && (d = p->d->sig) > q)
            q = d;
         q++;
      } else
      {
         if (p->n && (d = p->n->sig - p->n->mag - 1) && d > q)
            q = d;
         if (p->d && (d = p->d->mag + 1 - p->d->sig) && d > q)
            q = d;
      }
      return q - o.places;
   }
   char *format (void)
   {
      if (o.format != SD_FORMAT_RATIONAL && p->d && !p->d->sig)
         return strdup (p->n->neg ? "-∞" : "∞");
      char *r = NULL;
      switch (o.format)
      {
      case SD_FORMAT_RATIONAL: // Rational
         {                      // rational mode
            sd_p c = sd_copy (p);
            sd_rational (c);    // Normalise to integers
            sd_val_t *rem = NULL;
          sd_val_t *res = sdiv (NULL, c->n, c->d, rem: &rem, round:SD_ROUND_TRUNCATE);
            if (rem && !rem->sig)
             r = output (res, comma: o.comma, combined: o.combined, currency:o.currency);
            // No remainder, so integer
            freez (rem);
            freez (res);
            if (!r)
            {                   // Rational
             char *n = output (c->n, comma: o.comma, combined: o.combined, currency:o.currency);
             char *d = output (c->d, comma: o.comma, combined: o.combined, currency:o.currency);
               if (asprintf (&r, "%s/%s", n, d) < 0)
                  errx (1, "malloc");
               freez (d);
               freez (n);
            }
            sd_free (c);
         }
         break;
      case SD_FORMAT_FRACTION:
         {                      // Fraction mode
            int f = FRACTIONS;
            sd_rational (p);
            sd_val_t *R1,
           *N1 = udiv (&failp, p->n, p->d, round: 'T', rem:&R1);
            if (R1->sig)
            {                   // Non integer
               int q;
               for (q = 2; f == FRACTIONS && q <= 10; q++)
               {
                  sd_val_t *R2,
                 *N2 = udiv (&failp, umul (&failp, R1, make_int (&failp, q), b_free: 1), p->d, rem: &R2, a_free:1);
                  if (!R2->sig && !N2->mag && N2->sig == 1)
                     for (f = 0; f < FRACTIONS && (fraction[f].n != *N2->d || fraction[f].d != q); f++);
                  freez (N2);
                  freez (R2);
               }
            }
            if (f < FRACTIONS && !failp)
            {                   // Found
               char *v = NULL;
               if (N1->sig)
                v = output (N1, comma: o.comma, combined: o.combined, currency:o.currency);
               if (asprintf (&r, "%s%s%s", p->n->neg ? "-" : "", v ? : "", fraction[f].value) < 0)
                  errx (1, "malloc");
               freez (v);
            }
            freez (N1);
            freez (R1);
         }
         if (r)
            return r;
         // Drop through
      case SD_FORMAT_LIMIT:
       r = output_f (sd_rnd_val (p, places: guess (0), round: o.round), comma: o.comma, combined: o.combined, currency:o.currency);
         break;
      case SD_FORMAT_EXACT:
       r = output_f (sd_rnd_val (p, places: o.places, round: o.round, pad: 1), comma: o.comma, combined: o.combined, currency:o.currency);
         break;
      case SD_FORMAT_INPUT:
       r = output_f (sd_rnd_val (p, places: p->places + o.places, round: o.round, pad: 1), comma: o.comma, combined: o.combined, currency:o.currency);
         break;
      case SD_FORMAT_EXP:
         {
          sd_val_t *v = sd_rnd_val (p, places: guess (1), round: o.round, sig: 1, pad:o.places >= 0);
            int exp = v->mag;
            v->mag = 0;
          char *t = output_f (v, comma: o.comma, combined: o.combined, currency:o.currency);
            char *r;
            if (asprintf (&r, "%se%+d", t, exp) < 0)
               errx (1, "malloc");
            freez (t);
            return r;
         }
         break;
      case SD_FORMAT_SI:
         {
          sd_val_t *v = sd_rnd_val (p, places: guess (1), round: o.round, sig: 1, pad:o.places >= 0);
            int exp = (v->mag + 30) / 3 * 3 - 30;
            if (exp < -30)
               exp = -30;
            if (exp > 30)
               exp = 30;
            v->mag -= exp;
          char *t = output_f (v, comma: o.comma, combined: o.combined, currency:o.currency);
            if (!exp || !strcmp (t, "0"))
               return t;
            int s;
            for (s = 0; s < SIS && si[s].mag != exp; s++);
            char *r;
            if (asprintf (&r, "%s%s", t, si[s].value) < 0)
               errx (1, "malloc");
            freez (t);
            return r;
         }
         break;
      case SD_FORMAT_IEEE:
         {
            int i;
            for (i = 0; i < IEEES; i++)
             if (sd_cmp (p, sd_int (ieee[i].mul), abs: 1, r_free:1) < 0)
                  break;
            if (i)
            {
               if (!p->d)
                  p->d = make_int (&failp, ieee[i - 1].mul);
               else
                p->d = umul (&failp, p->d, make_int (&failp, ieee[i - 1].mul), b_free:1);
            }
          sd_val_t *v = sd_rnd_val (p, places: guess (1), round: o.round, sig: 1, pad:o.places >= 0);
          char *t = output_f (v, comma: o.comma, combined: o.combined, currency:o.currency);
            if (!i)
               return t;
            char *r;
            if (asprintf (&r, "%s%s", t, ieee[i - 1].value) < 0)
               errx (1, "malloc");
            freez (t);
            return r;
         }
         break;
      default:
         warnx ("Unknown format %c\n", o.format);
         break;
      }
      return r;
   }
   char *r = format ();
   if (!failp && p && p->failure)
      failp = p->failure;
   if (failp)
   {
      freez (r);
      if (asprintf (&r, "!!%s", failp) < 0)
         errx (1, "malloc");
      if (o.failure)
         *o.failure = "Output failed";
   }
   if (o.p_free)
      sd_free (o.p);
   return r;
}

sd_p
sd_neg_opts (sd_1_t o)
{                               // Negate
   if (!o.p)
      return o.p;
   if (!o.p_free)
      o.p = sd_copy (o.p);
   if (o.p->n->sig)
      o.p->n->neg ^= 1;
   return o.p;
}

sd_p
sd_abs_opts (sd_1_t o)
{                               // Absolute
   if (!o.p)
      return o.p;
   if (!o.p_free)
      o.p = sd_copy (o.p);
   o.p->n->neg = 0;
   if (o.p->d)
      o.p->d->neg = 0;
   return o.p;
}

sd_p
sd_inv_opts (sd_1_t o)
{                               // Reciprocal
   if (!o.p)
      return o.p;
   if (!o.p_free)
      o.p = sd_copy (o.p);
   sd_val_t *d = o.p->d;
   if (!d)
      d = copy (&o.p->failure, &one);
   o.p->d = o.p->n;
   o.p->n = d;
   return o.p;
}

sd_p
sd_10_opts (sd_10_t o)
{                               // Adjust by power of 10
   if (!o.p)
      return o.p;
   if (!o.p_free)
      o.p = sd_copy (o.p);
   if (!o.p->n)
      return o.p;
   o.p->n->mag += o.shift;
   return o.p;
}

int
sd_iszero (sd_p p)
{                               // Is zero
   return !p || !p->n || !p->n->sig;
}

int
sd_isneg (sd_p p)
{                               // Is negative (denominator always positive)
   return p && p->n && p->n->neg;
}

int
sd_ispos (sd_p p)
{                               // Is positive (denominator always positive)
   return p && p->n && p->n->sig && !p->n->neg;
}

static void
sd_rational (sd_p p)
{                               // Normalise to integers
   if (!p || !p->n)
      return;
   if (!p->d)
      p->d = copy (&p->failure, &one);
   int s,
     shift = p->n->sig - p->n->mag - 1;
   if ((s = p->d->sig - p->d->mag - 1) > shift)
      shift = s;
   p->n->mag += shift;
   p->d->mag += shift;
}

sd_p
sd_add_opts (sd_2_t o)
{                               // Add
   sd_p l = o.l ? : &sd_zero;
   sd_p r = o.r ? : &sd_zero;
   sd_debugout ("sd_add", l, r, NULL);
   sd_val_t *a,
    *b;
   sd_p v = sd_cross (l, r, &a, &b);
   if (v)
   {
      v->n = sadd (&v->failure, a ? : l->n, b ? : r->n);
      if (l->d || r->d)
      {
         if (!scmp (&v->failure, l->d ? : &one, r->d ? : &one))
            v->d = copy (&v->failure, l->d ? : &one);
         else
            v->d = smul (&v->failure, l->d ? : &one, r->d ? : &one);
      }
      freez (a);
      freez (b);
   }
   v = sd_tidy (v);
   if (o.l_free)
      sd_free (o.l);
   if (o.r_free)
      sd_free (o.r);
   return v;
};

sd_p
sd_sub_opts (sd_2_t o)
{                               // Subtract
   sd_p l = o.l ? : &sd_zero;
   sd_p r = o.r ? : &sd_zero;
   if (r && r->n)
      r->n->neg ^= 1;
   sd_p v = sd_add (l, r);
   if (r && r->n)
      r->n->neg ^= 1;
   if (o.l_free)
      sd_free (o.l);
   if (o.r_free)
      sd_free (o.r);
   return v;
};

sd_p
sd_mul_opts (sd_2_t o)
{                               // Multiply
   sd_p v = NULL;
   if (o.l && o.r)
   {                            // either being null means answer is null, as null is seen as zero
      sd_p l = o.l;
      sd_p r = o.r;
      sd_debugout ("sd_mul", l, r, NULL);
      v = sd_new (l, r);
      if (r->d && !scmp (&v->failure, l->n, r->d))
      {                         // Cancel out
         v->n = copy (&v->failure, r->n);
         v->d = copy (&v->failure, l->d);
         if (l->n->neg)
            v->n->neg ^= 1;
      } else if (l->d && !scmp (&v->failure, r->n, l->d))
      {                         // Cancel out
         v->n = copy (&v->failure, l->n);
         v->d = copy (&v->failure, r->d);
         if (r->n->neg)
            v->n->neg ^= 1;
      } else
      {                         // Multiple
         if (l->d || r->d)
            v->d = smul (&v->failure, l->d ? : &one, r->d ? : &one);
         v->n = smul (&v->failure, l->n, r->n);
      }
      v = sd_tidy (v);
   }
   if (o.l_free)
      sd_free (o.l);
   if (o.r_free)
      sd_free (o.r);
   return v;
};

sd_p
sd_div_opts (sd_2_t o)
{                               // Divide
   sd_p v = NULL;
   if (o.l && o.r)
   {                            // either being null means answer is null, as null is seen as zero
      sd_p l = o.l;
      sd_p r = o.r;
      sd_debugout ("sd_div", l, r, NULL);
      v = sd_new (l, r);
      if (!l->d && !r->d)
      {                         // Simple - making a new rational
         v->n = copy (&v->failure, l->n);
         v->d = copy (&v->failure, r->n);
      } else
      {                         // Flip and multiply 
         sd_val_t *t = r->n;
         r->n = r->d ? : copy (&v->failure, &one);
         r->d = t;
         r->n->neg = r->d->neg;
         r->d->neg = 0;
         return sd_mul (l, r);
      }
      v = sd_tidy (v);
   }
   if (o.l_free)
      sd_free (o.l);
   if (o.r_free)
      sd_free (o.r);
   return v;
};

sd_p
sd_mod_opts (sd_mod_t o)
{                               // modulo
   sd_p v = NULL;
   if (o.l && o.r)
   {                            // either being null means answer is null, as null is seen as zero
      sd_p l = o.l;
      sd_p r = o.r;
      sd_debugout ("sd_mod", l, r, NULL);
      v = sd_new (l, r);
      sd_val_t *ad = smul (&v->failure, l->n, r->d ? : &one);
      sd_val_t *bc = smul (&v->failure, l->d ? : &one, r->n);
      v->d = smul (&v->failure, l->d ? : &one, r->d ? : &one);
    sd_val_t *n = sdiv (&v->failure, ad, bc, rem: &v->n, round:o.round ? : SD_ROUND_FLOOR);
      freez (ad);
      freez (bc);
      freez (n);
      v = sd_tidy (v);
   }
   if (o.l_free)
      sd_free (o.l);
   if (o.r_free)
      sd_free (o.r);
   return v;
}

sd_p
sd_pow_opts (sd_2_t o)
{
   sd_p l = o.l ? : &sd_zero;
   sd_p r = o.r ? : &sd_zero;
   const char *failp = NULL;
   if (r->n->neg)
      return NULL;
   sd_debugout ("sd_pow", l, r, NULL);
   sd_val_t *p = NULL;
   sd_val_t *rem = NULL;
 p = udiv (&failp, r->n, r->d ? : &one, rem: &rem, round:SD_ROUND_TRUNCATE);
   if (rem->sig)
   {
      freez (p);
      freez (rem);
      return NULL;              // Not integer
   }
   freez (rem);
   if (p->sig > p->mag + 1)
   {                            // Not integer
      freez (p);
      return NULL;
   }
   sd_p m = sd_copy (l);
   sd_p v = sd_int (1);
   while (p->sig)
   {
    sd_val_t *p2 = udiv (&failp, p, &two, rem: &rem, round:SD_ROUND_TRUNCATE);
      freez (p);
      p = p2;
      if (rem->sig)
         v = sd_mul_fc (v, m);
      freez (rem);
      if (!p->sig)
         break;
      m = sd_mul_fc (m, m);
      debugout ("pow", p2, NULL);
      sd_debugout ("pow", m, v, NULL);
   }
   freez (p);
   sd_free (m);
   if (failp && !v->failure)
      v->failure = failp;
   v = sd_tidy (v);
   if (o.l_free)
      sd_free (o.l);
   if (o.r_free)
      sd_free (o.r);
   return v;
}

int
sd_cmp_opts (sd_cmp_t o)
{                               // Compare
   sd_p l = o.l ? : &sd_zero;
   sd_p r = o.r ? : &sd_zero;
   sd_val_t *a,
    *b;
   sd_p v = sd_cross (l, r, &a, &b);
   int diff = 0;
   if (o.abs)
      diff = ucmp (NULL, a ? : l->n, b ? : r->n, 0);
   else
      diff = scmp (NULL, a ? : l->n, b ? : r->n);
   sd_free (v);
   freez (a);
   freez (b);
   if (o.l_free)
      sd_free (o.l);
   if (o.r_free)
      sd_free (o.r);
   return diff;
};

#ifdef	EVAL
// Parsing
#include "xparse.c"
// Parse Support functions
static void *
parse_operand (void *context, const char *p, const char **end)
{                               // Parse an operand, malloc value (or null if error), set end
   stringdecimal_context_t *C = context;
 sd_p v = sd_parse (p, end: end, nocomma: C->nocomma, nofrac: C->nofrac, nosi: C->nosi, noieee:C->noieee);
   if (v && v->failure)
   {
      if (!C->fail)
         C->fail = v->failure;
      if (!C->posn)
         C->posn = p;
   }
   return v;
}

static void *
parse_final (void *context, void *v)
{                               // Final processing
   stringdecimal_context_t *C = context;
   if (C->raw)
      return v;
   sd_p V = v;
   if (!V)
      return NULL;
 return sd_output (V, places: V->places, places: C->places, format: C->format, round: C->round, comma: C->comma, combined: C->combined, currency:C->currency);
}

static void
parse_dispose (void *context, void *v)
{                               // Disposing of an operand
   sd_free (v);
}

static void
parse_fail (void *context, const char *failure, const char *posn)
{                               // Reporting an error
   stringdecimal_context_t *C = context;
   if (!C->fail)
      C->fail = failure;
   if (!C->posn)
      C->posn = posn;
}

#define MATCH_LT 1
#define MATCH_GT 2
#define MATCH_EQ 4
#define MATCH_NE 8
static sd_p
parse_bin_cmp (sd_p l, sd_p r, int match)
{
   sd_val_t *a,
    *b;
   sd_p L = l,
      R = r,
      v = sd_cross (l, r, &a, &b);
   int diff = scmp (&v->failure, a ? : L->n, b ? : R->n);
   if (((match & MATCH_LT) && diff < 0) || ((match & MATCH_GT) && diff > 0) || ((match & MATCH_EQ) && diff == 0)
       || ((match & MATCH_NE) && diff != 0))
      v->n = copy (&v->failure, &one);
   else
      v->n = copy (&v->failure, &zero);
   v->places = 0;
   return v;
}

// Parse Functions
static void *
parse_null (void *context, void *data, void **a)
{
   return *a;
}

static void *
parse_add (void *context, void *data, void **a)
{
   return sd_add (a[0], a[1]);
}

static void *
parse_sub (void *context, void *data, void **a)
{
   return sd_sub (a[0], a[1]);
}

static void *
parse_div (void *context, void *data, void **a)
{
   stringdecimal_context_t *C = context;
   sd_p o = sd_div (a[0], a[1]);
   if (!o && !C->fail)
      C->fail = "Divide by zero";
   if (o && o->failure && !C->fail)
      C->fail = o->failure;
   return o;
}

static void *
parse_mod (void *context, void *data, void **a)
{
   stringdecimal_context_t *C = context;
   sd_p o = sd_mod (a[0], a[1]);
   if (o && o->failure && !C->fail)
      C->fail = o->failure;
   return o;
}

static void *
parse_mul (void *context, void *data, void **a)
{
   return sd_mul (a[0], a[1]);
}

static void *
parse_pow (void *context, void *data, void **a)
{
   stringdecimal_context_t *C = context;
   sd_p L = a[0],
      R = a[1];
   sd_p o = sd_pow (L, R);
   if (!o && !C->fail)
      C->fail = "Power must be positive integer";
   if (o && o->failure && !C->fail)
      C->fail = o->failure;
   return o;
}

static void *
parse_neg (void *context, void *data, void **a)
{
   sd_p A = *a;
   A->n->neg ^= 1;
   return A;
}

static void *
parse_abs (void *context, void *data, void **a)
{
   sd_p A = *a;
   A->n->neg = 0;
   return A;
}

static void *
parse_not (void *context, void *data, void **a)
{
   sd_p A = *a;
   sd_p v = sd_new (A, NULL);
   v->n = copy (&v->failure, !A->n->sig ? &one : &zero);
   v->places = 0;
   return v;
}

static void *
parse_eq (void *context, void *data, void **a)
{
   return parse_bin_cmp (a[0], a[1], MATCH_EQ);
}

static void *
parse_ne (void *context, void *data, void **a)
{
   return parse_bin_cmp (a[0], a[1], MATCH_NE);
}

static void *
parse_gt (void *context, void *data, void **a)
{
   return parse_bin_cmp (a[0], a[1], MATCH_GT);
}

static void *
parse_ge (void *context, void *data, void **a)
{
   return parse_bin_cmp (a[0], a[1], MATCH_EQ | MATCH_GT);
}

static void *
parse_le (void *context, void *data, void **a)
{
   return parse_bin_cmp (a[0], a[1], MATCH_EQ | MATCH_LT);
}

static void *
parse_lt (void *context, void *data, void **a)
{
   return parse_bin_cmp (a[0], a[1], MATCH_LT);
}

static void *
parse_and (void *context, void *data, void **a)
{
   sd_p L = a[0],
      R = a[1];
   if (!L->n->sig)
      return L;                 // False
   return R;                    // Whatever second argument is
}

static void *
parse_or (void *context, void *data, void **a)
{
   sd_p L = a[0],
      R = a[1];
   if (L->n->sig)
      return L;                 // True
   return R;                    // Whatever second argument is
}

static void *
parse_cond (void *context, void *data, void **a)
{
   if (!sd_iszero (a[0]))
      return a[1] ? : a[0];     // Allow for null second operand
   return a[2];
}

// List of functions - as pre C operator precedence with comma as 1, and postfix as 15
// e.g. https://www.tutorialspoint.com/cprogramming/c_operators_precedence.htm
//
static xparse_op_t parse_bracket[] = {
 {op: "(", op2: ")", func:parse_null},
 {op: "⁽", op2: "⁾", func:parse_null},
 {op: "₍", op2: "₎", func:parse_null},
 {op: "|", op2: "|", func:parse_abs},
   {NULL},
};

static xparse_op_t parse_unary[] = {

   // Postfix would be 15
 {op: "-", level: 14, func:parse_neg},
 {op: "!", op2: "¬", level: 14, func:parse_not},
   {NULL},
};

static xparse_op_t parse_post[] = {
   {NULL},
};

static xparse_op_t parse_binary[] = {
 {op: "^", level: 14, func:parse_pow},
 {op: "/", op2: "÷", level: 13, func:parse_div},
 {op: "%", level: 13, func:parse_mod},
 {op: "*", op2: "×", level: 13, func:parse_mul},
   // % would be 14, we should add that
 {op: "+", level: 12, func:parse_add},
 {op: "⁺", op2: "₊", level: 12, func:parse_add},
 {op: "-", op2: "−", level: 12, func:parse_sub},
 {op: "⁻", op2: "₋", level: 12, func:parse_sub},
   // Shift would be 11
 {op: ">=", op2: "≥", level: 10, func:parse_ge},
 {op: "<=", op2: "≤", level: 10, func:parse_le},
 {op: "!=", op2: "≠", level: 10, func:parse_ne},
 {op: ">", op2: "≰", level: 10, func:parse_gt},
 {op: "<", op2: "≱", level: 10, func:parse_lt},
 {op: "==", op2: "=", level: 9, func:parse_eq},
 {op: "⁼", op2: "₌", level: 9, func:parse_eq},
   // & would be 8
   // ^ would be 7
   // | would be 6
 {op: "&&", op2: "∧", level: 5, func:parse_and},
 {op: "||", op2: "∨", level: 4, func:parse_or},
   {NULL},
};

static xparse_op_t parse_ternary[] = {
 {op: "?", op2: ":", level: 3, func:parse_cond},
   // Assignment would be 2
   // Comma would be 1
   {NULL},
};

// Parse Config (optionally public to allow building layers on top)
xparse_config_t stringdecimal_xparse = {
 bracket:parse_bracket,
 unary:parse_unary,
 post:parse_post,
 binary:parse_binary,
 ternary:parse_ternary,
 operand:parse_operand,
 final:parse_final,
 dispose:parse_dispose,
 fail:parse_fail,
};

// Parse
char *
stringdecimal_eval_opts (stringdecimal_unary_t o)
{
 stringdecimal_context_t context = { places: o.places, format: o.format, round: o.round, nocomma: o.nocomma, comma: o.comma, nofrac: o.nofrac, nosi: o.nosi, noieee: o.noieee, combined:o.combined
   };
   char *ret = xparse (&stringdecimal_xparse, &context, o.a, NULL);
   if (!ret || context.fail)
   {
      freez (ret);
      assert (asprintf
              (&ret, "!!%s at %.*s", context.fail, 10, !context.posn ? "[unknown]" : !*context.posn ? "[end]" : context.posn) >= 0);
   }
   if (o.a_free)
      freez (o.a);
   return ret;
}
#endif

#ifndef LIB
// Test function main build
#include <sys/types.h>
#include <unistd.h>

char *
expand (const char *s)
{                               // Do variable expansion, return malloced string, does $x and ${x} but not $x:x
   if (!s)
      return NULL;
   if (!strchr (s, '$'))
      return strdup (s);
   char *out = NULL;
   size_t l;
   FILE *f = open_memstream (&out, &l);
   while (*s)
   {
      if (*s != '$')
      {
         fputc (*s++, f);
         continue;
      }
      s++;
      if (*s == '$')
      {                         // $$
         s++;
         fprintf (f, "%d", getppid ());
         continue;
      }
      const char *v = s,
         *e = s;
      if (*s == '{')
      {
         s++;
         v = s;
         while (*s && *s != '}')
            s++;
         e = s;
         if (*s == '}')
            s++;
      } else
      {
         while (isalnum (*s))
            s++;
         e = s;
      }
      if (e > v)
      {
         char *name = strndup (v, (int) (e - v));
         char *value = getenv (name);
         if (value)
            fprintf (f, "%s", value);
         free (name);
      }
   }
   fclose (f);
   return out;
}

#include <popt.h>
int
main (int argc, const char *argv[])
{
   const char *pass = NULL;
   const char *fail = NULL;
   const char *round = "";
   const char *format = "";
   const char *scomma = NULL;
   const char *spoint = NULL;
   int places = 0;
   int comma = 0;
   int nocomma = 0;
   int nofrac = 0;
   int nosi = 0;
   int noieee = 0;
   int fails = 0;
   int combined = 0;
   const char *currency = NULL;
   {                            // POPT
      poptContext optCon;       // context for parsing command-line options
      const struct poptOption optionsTable[] = {
         {"places", 'p', POPT_ARG_INT, &places, 0, "Places", "N"},
         {"format", 'f', POPT_ARG_STRING, &format, 0, "Format", SD_FORMATS},
         {"round", 'r', POPT_ARG_STRING, &round, 0, "Rounding", "TUFCRBN"},
         {"no-comma", 'n', POPT_ARG_NONE, &nocomma, 0, "No comma in input"},
         {"no-frac", 0, POPT_ARG_NONE, &nofrac, 0, "No fractions in input"},
         {"no-si", 0, POPT_ARG_NONE, &nosi, 0, "No SI suffix in input"},
         {"no-ieee", 0, POPT_ARG_NONE, &noieee, 0, "No IEEE suffix in input"},
         {"comma", 'c', POPT_ARG_NONE, &comma, 0, "Comma in output"},
         {"combined", 0, POPT_ARG_NONE, &combined, 0, "Combined digit and comma/dot"},
         {"currency", 0, POPT_ARG_STRING, &currency, 0, "Currency prefix"},
         {"comma-char", 'C', POPT_ARG_STRING, &scomma, 0, "Set comma char", "char"},
         {"point-char", 'C', POPT_ARG_STRING, &spoint, 0, "Set point char", "char"},
         {"max", 'm', POPT_ARG_INT, &sd_max, 0, "Max size", "N"},
         {"pass", 'P', POPT_ARG_STRING, &pass, 0, "Test pass", "expected"},
         {"fail", 'F', POPT_ARG_STRING, &fail, 0, "Test fail", "expected failure"},
         POPT_AUTOHELP {}
      };
      optCon = poptGetContext (NULL, argc, argv, optionsTable, 0);
      poptSetOtherOptionHelp (optCon, "Sums");
      int c;
      if ((c = poptGetNextOpt (optCon)) < -1)
         errx (1, "%s: %s\n", poptBadOption (optCon, POPT_BADOPTION_NOALIAS), poptStrerror (c));
      if (!poptPeekArg (optCon))
      {
         poptPrintUsage (optCon, stderr, 0);
         return -1;
      }
      if (scomma)
         sd_comma = *scomma;
      if (spoint)
         sd_point = *spoint;
      char *s;
      while ((s = expand (poptGetArg (optCon))))
      {
       char *res = stringdecimal_eval (s, places: places, format: *format, round: *round, comma: comma, nocomma: nocomma, nofrac: nofrac, nosi: nosi, noieee: noieee, combined: combined, currency:currency);
         if (pass && (!res || *res == '!' || strcmp (res, pass)))
         {
            fails++;
            fprintf (stderr, "Test:\t%s\nResult:\t%s\nExpect:\t%s\n", s, res ? : "[null]", pass);
         }
         if (fail && res && (*res != '!' || res[1] != '!' || strcasecmp (res + 2, fail)))
         {
            fails++;
            fprintf (stderr, "Test:\t%s\nResult:\t%s\nExpect:\t!!%s\n", s, res ? : "[null]", fail);
         }
         if (!pass && !fail)
         {
            if (res)
               printf ("%s\n", res);
            else
               fprintf (stderr, "Failed\n");
         }
         freez (res);
         free (s);
      }
      poptFreeContext (optCon);
   }
   return fails;
}
#endif
