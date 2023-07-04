// Lightweight JSON syntax management tool kit
// This toolkit works solely at a syntax level, and does not all management of whole JSON object hierarchy

#include "jo.h"
#include <string.h>
#include <malloc.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <math.h>
#include "esp_log.h"

#ifndef	JO_MAX
#define	JO_MAX	64
#endif

struct jo_s
{                               // cursor to JSON object
   char *buf;                   // Start of JSON string
   const char *err;             // If in error state
   size_t ptr;                  // Pointer in to buf
   size_t len;                  // Max space for buf
   uint8_t parse:1;             // This is parsing, not generating
   uint8_t alloc:1;             // buf is malloced space
   uint8_t comma:1;             // Set if comma needed / expected
   uint8_t tagok:1;             // We have skipped an expected tag already in parsing
   uint8_t null:1;              // We have a null termination
   uint8_t level;               // Current level
   uint8_t o[(JO_MAX + 7) / 8]; // Bit set at each level if level is object, else it is array
};

const char JO_BASE64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
const char JO_BASE32[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
const char JO_BASE16[] = "0123456789ABCDEF";

// Note escaping / is optional but done always to avoid </script> issues
#define escapes \
        esc ('"', '"') \
        esc ('\\', '\\') \
        esc ('/', '/') \
        esc ('b', '\b') \
        esc ('f', '\f') \
        esc ('n', '\n') \
        esc ('r', '\r') \
        esc ('t', '\t') \

static jo_t
jo_new (void)
{                               // Create a jo_t
   jo_t j = malloc (sizeof (*j));
   if (!j)
      return j;                 // Malloc fail
   memset (j, 0, sizeof (*j));
   return j;
}

static void *
saferealloc (void *m, size_t len)
{
   void *n = realloc (m, len);
   if (m && !n)
      free (m);                 // Failed, clear existing
   return n;
}

static inline void
jo_store (jo_t j, uint8_t c)
{                               // Write out byte
   if (!j || j->err)
      return;
   if (j->parse)
   {
      j->err = "Writing to read only JSON";
      return;
   }
   if (j->ptr >= j->len && (!j->alloc || !(j->buf = saferealloc (j->buf, j->len += 100))))
   {
      j->err = (j->alloc ? "Cannot allocate space" : "Out of space");
      return;
   }
   j->buf[j->ptr] = c;
   j->null = (c ? 0 : 1);
}

static inline void
jo_write (jo_t j, uint8_t c)
{                               // Write a byte and advance
   jo_store (j, c);
   if (j && !j->err)
      j->ptr++;
}

jo_t
jo_pad (jo_t * jp, int n)
{                               // Ensure padding available
   if (!jp)
      return NULL;
   jo_t j = *jp;
   if (!j->parse)
      n += j->level + 1;        // Allow space to close and null
   if (!j->alloc
       || ((j->parse || j->ptr + n > j->len) && !(j->buf = saferealloc (j->buf, j->len = (j->parse ? j->len : j->ptr) + n))))
   {                            // Cannot pad
      jo_free (jp);
      return NULL;
   }
   return j;
}

static inline int
jo_peek (jo_t j)
{                               // Peek next raw byte (-1 for end/fail)
   if (!j || j->err || !j->parse || j->ptr >= j->len)
      return -1;
   return j->buf[j->ptr];
}

static inline int
jo_read (jo_t j)
{                               // Read and advance next raw byte (-1 for end/fail)
   if (!j || j->err || !j->parse || j->ptr >= j->len)
      return -1;
   return j->buf[j->ptr++];
}

static int
jo_read_str (jo_t j)
{                               // Read next (UTF-8) within a string, so decode escaping (-1 for end/fail)
   if (!j || !j->parse || j->err)
      return -1;
   int bad (const char *e)
   {                            // Fail
      if (!j->err)
         j->err = e;
      return -1;
   }
   int utf8 (void)
   {                            // next character
      if (jo_peek (j) == '"')
         return -1;             // Clean end of string
      int c = jo_read (j),
         q = 0;
      if (c < 0)
         return c;
      if (c >= 0xF7)
         return bad ("Bad UTF-8");
      if (c >= 0xF0)
      {                         // Note could check for F0 and next byte as bad
         c &= 0x07;
         q = 3;
      } else if (c >= 0xE0)
      {                         // Note could check for E0 and next byte as bad
         c &= 0x0F;
         q = 2;
      } else if (c >= 0xC0)
      {
         if (c < 0xC2)
            return bad ("Bad UTF-8");
         c &= 0x1F;
         q = 1;
      } else if (c >= 0x80)
         return bad ("Bat UTF-8");
      else if (c == '\\')
      {                         // Escape
         c = jo_read (j);
         if (c == 'u')
         {                      // Hex
            c = 0;
            for (int q = 4; q; q--)
            {
               int u = jo_read (j);
               if (u >= '0' && u <= '9')
                  c = (c << 4) + (u & 0xF);
               else if ((u >= 'A' && u <= 'F') || (u >= 'a' && u <= 'f'))
                  c = (c << 4) + 9 + (u & 0xF);
               else
                  return bad ("bad hex escape");
            }
         }
#define esc(a,b) else if(c==a)c=b;
         escapes
#undef esc
            else
            return bad ("Bad escape");
      }
      while (q--)
      {                         // More UTF-8 characters
         int u = jo_read (j);
         if (u < 0x80 || u >= 0xC0)
            return bad ("Bad UTF-8");
         c = (c << 6) + (u & 0x3F);
      }
      return c;
   }
   int c = utf8 ();
   if (c >= 0xD800 && c <= 0xDBFF)
   {                            // UTF16 Surrogates
      int c2 = utf8 ();
      if (c2 < 0xDC00 || c2 > 0xDFFF)
         return bad ("Bad UTF-16, second part invalid");
      c = ((c & 0x3FF) << 10) + (c2 & 0x3FF) + 0x10000;
   }
   return c;
}

// Setting up

jo_t
jo_parse_str (const char *buf)
{                               // Start parsing a null terminated JSON object string
   if (!buf)
      return NULL;
   jo_t j = jo_parse_mem (buf, strlen (buf) + 1);       // Include the null so we set null tag
   return j;
}

jo_t
jo_parse_mem (const void *buf, size_t len)
{                               // Start parsing a JSON string in memory - does not need a null
   if (!buf)
      return NULL;              // No buf
   jo_t j = jo_new ();
   if (!j)
      return j;                 // malloc fail
   j->parse = 1;
   j->buf = (char *) buf;
   if (len && !j->buf[len - 1])
   {                            // We provided a null, nice
      len--;
      j->null = 1;
   }
   j->len = len;
   return j;
}

jo_t
jo_create_mem (void *buf, size_t len)
{                               // Start creating JSON in memory at buf, max space len.
   jo_t j = jo_new ();
   if (!j)
      return j;                 // malloc fail
   j->buf = buf;
   j->len = len;
   return j;
}

jo_t
jo_create_alloc (void)
{                               // Start creating JSON in memory, allocating space as needed.
   jo_t j = jo_new ();
   if (!j)
      return j;                 // malloc fail
   j->alloc = 1;
   return j;
}

jo_t
jo_object_alloc (void)
{                               // Common
   jo_t j = jo_create_alloc ();
   jo_object (j, NULL);
   return j;
}

static jo_t
jo_link (jo_t j)
{                               // Internal use: Copy control without copying the allocated content - copied object has to stay valid
   if (!j || j->err)
      return NULL;              // No j
   jo_t n = jo_new ();
   if (!n)
      return n;                 // malloc fail
   memcpy (n, j, sizeof (*j));
   n->alloc = 0;
   return n;
}

jo_t
jo_copy (jo_t j)
{                               // Copy object - copies the object, and if allocating memory, makes copy of the allocated memory too
   if (!j || j->err)
      return NULL;              // No j
   jo_t n = jo_new ();
   if (!n)
      return n;                 // malloc fail
   memcpy (n, j, sizeof (*j));
   if (j->alloc && j->buf)
   {
      j->null = 0;
      n->buf = malloc (j->parse ? j->len + 1 : j->len ? : 1);
      if (!n->buf)
      {
         jo_free (&n);
         return NULL;           // malloc
      }
      if (j->parse)
      {                         // Ensure null
         n->buf[j->len] = 0;
         j->null = 1;
      }
      memcpy (n->buf, j->buf, j->parse ? j->len : j->ptr);
   }
   return n;
}

const char *
jo_rewind (jo_t j)
{                               // Move to start for parsing. If was writing, closed and set up to read instead. Clears error is reading.
   if (!j)
      return NULL;
   if (!j->parse)
   {                            // Finish, if possible
      while (j->level)
         jo_close (j);
      jo_store (j, 0);
      if (j->err)
         return NULL;
      j->len = j->ptr;
      j->parse = 1;
   }
   j->err = NULL;
   j->ptr = 0;
   j->comma = 0;
   j->level = 0;
   j->tagok = 0;
   if (!j->null)
      return NULL;
   return j->buf;
}

int
jo_level (jo_t j)
{                               // Current level, 0 being the top level
   if (!j)
      return -1;
   return j->level;
}

const char *
jo_error (jo_t j, int *pos)
{                               // Return NULL if no error, else returns an error string.
   if (pos)
      *pos = (j ? j->ptr : -1);
   if (j && !j->err && !j->parse && !j->alloc && j->ptr + j->level + 1 > j->len)
      return "No space to finish JSON";
   if (!j)
      return "No j";
   return j->err;
}

void
jo_free (jo_t * jp)
{                               // Free j
   if (!jp)
      return;
   jo_t j = *jp;
   if (!j)
      return;
   *jp = NULL;
   if (j->alloc && j->buf)
      free (j->buf);
   free (j);
}

int
jo_isalloc (jo_t j)
{
   if (j && j->alloc)
      return 1;
   return 0;
}

char *
jo_finish (jo_t * jp)
{                               // Finish creating static JSON, return start of static JSON if no error. Frees j. It is an error to use with jo_create_alloc
   if (!jp)
      return NULL;
   jo_t j = *jp;
   if (!j)
      return NULL;
   *jp = NULL;
   if (!j->parse)
   {
      while (j->level)
         jo_close (j);
      jo_store (j, 0);
   }
   char *res = j->buf;
   if (j->err || j->alloc)
      res = NULL;
   if (!res && j->alloc && j->buf)
      free (j->buf);
   free (j);
   return res;
}

char *
jo_finisha (jo_t * jp)
{                               // Finish creating allocated JSON, returns start of alloc'd memory if no error.
   // Frees j. If NULL returned then any allocated space also freed
   // It is an error to use with non jo_create_alloc
   if (!jp)
      return NULL;
   jo_t j = *jp;
   if (!j)
      return NULL;
   *jp = NULL;
   if (!j->parse)
   {
      while (j->level)
         jo_close (j);
      jo_store (j, 0);
   }
   char *res = j->buf;
   if (j->err || !j->alloc)
      res = NULL;
   if (!res && j->alloc && j->buf)
      free (j->buf);
   free (j);
   return res;
}

int
jo_len (jo_t j)
{                               // Return length, including any closing
   if (!j)
      return -1;
   if (j->parse)
      return j->len;
   return j->ptr + j->level;
}

// Creating
// Note that tag is required if in an object and must be null if not

static void
jo_write_char (jo_t j, uint32_t c)
{

#define esc(a,b) if(c==b){jo_write(j,'\\');jo_write(j,a);return;}
   escapes
#undef esc
      if (c < ' ' || c >= 0xFF)
   {
      jo_write (j, '\\');
      jo_write (j, 'u');
      jo_write (j, JO_BASE16[(c >> 12) & 0xF]);
      jo_write (j, JO_BASE16[(c >> 8) & 0xF]);
      jo_write (j, JO_BASE16[(c >> 4) & 0xF]);
      jo_write (j, JO_BASE16[c & 0xF]);
      return;
   }
   jo_write (j, c);
}


static void
jo_write_str (jo_t j, const char *s, ssize_t len)
{
   jo_write (j, '"');
   if (len < 0)
      len = strlen (s);
   while (len--)
      jo_write_char (j, *s++);
   jo_write (j, '"');
}

static const char *
jo_write_check (jo_t j, const char *tag)
{                               // Check if we are able to write, and write the tag
   if (!j)
      return "No j";
   if (j->err)
      return j->err;
   if (j->parse)
      return (j->err = "Writing to parse");
   if (j->level && (j->o[(j->level - 1) / 8] & (1 << ((j->level - 1) & 7))))
   {
      if (!tag)
         return (j->err = "missing tag in object");
   } else if (tag)
      return (j->err = "tag in non object");
   if (!j->level && j->ptr)
      return (j->err = "second value at top level");
   if (j->comma)
      jo_write (j, ',');
   j->comma = 1;
   if (tag)
   {
      jo_write_str (j, tag, -1);
      jo_write (j, ':');
   }
   return j->err;
}

void
jo_json (jo_t j, const char *tag, jo_t json)
{                               // Add JSON, if NULL tag, and in object, and JSON is object, add its content
   if (!json)
   {
      jo_null (j, tag);
      return;
   }
   if (!json->parse)
   {                            // Close the json
      while (json->level)
         jo_close (json);
      jo_store (json, 0);
   }
   char *p = json->buf,
      *e = p + json->ptr;
   if (json->parse)
      e = p + json->len;
   if (!tag && p && *p == '{' && j->level && (j->o[(j->level - 1) / 8] & (1 << ((j->level - 1) & 7))))
   {                            // Special case of adding in-line object content
      p++;
      e--;
      if (j->comma)
         jo_write (j, ',');
      j->comma = 1;
   } else
   {                            // Normal add as value (possibly tagged)
      if (jo_write_check (j, tag))
         return;
   }
   while (p < e)
      jo_write (j, *p++);
}

void
jo_lit (jo_t j, const char *tag, const char *lit)
{                               // Add literal
   if (jo_write_check (j, tag))
      return;
   while (*lit)
      jo_write (j, *lit++);
}

void
jo_array (jo_t j, const char *tag)
{                               // Start an array
   if (jo_write_check (j, tag))
      return;
   if (j->level >= JO_MAX)
   {
      j->err = "JSON too deep";
      return;
   }
   j->o[j->level / 8] &= ~(1 << (j->level & 7));
   j->level++;
   j->comma = 0;
   jo_write (j, '[');
}

void
jo_object (jo_t j, const char *tag)
{                               // Start an object
   if (jo_write_check (j, tag))
      return;
   if (j->level >= JO_MAX)
   {
      if (!j->err)
         j->err = "JSON too deep";
      return;
   }
   j->o[j->level / 8] |= (1 << (j->level & 7));
   j->level++;
   j->comma = 0;
   jo_write (j, '{');
}

void
jo_close (jo_t j)
{                               // Close current array or object
   if (!j->level)
   {
      if (!j->err)
         j->err = "JSON too many closes";
      return;
   }
   j->level--;
   j->comma = 1;
   jo_write (j, (j->o[j->level / 8] & (1 << (j->level & 7))) ? '}' : ']');
}

void
jo_stringn (jo_t j, const char *tag, const char *string, ssize_t len)
{                               // Add a string
   if (!string)
   {
      jo_null (j, tag);
      return;
   }
   if (jo_write_check (j, tag))
      return;
   jo_write_str (j, string, len);
}

void
jo_stringf (jo_t j, const char *tag, const char *format, ...)
{                               // Add a string (formatted)
   if (jo_write_check (j, tag))
      return;
   char *v = NULL;
   va_list ap;
   va_start (ap, format);
   ssize_t len = vasprintf (&v, format, ap);
   va_end (ap);
   if (!v)
   {
      j->err = "malloc for printf";
      return;
   }
   jo_write_str (j, v, len);
   free (v);
}

void
jo_litf (jo_t j, const char *tag, const char *format, ...)
{                               // Add a literal (formatted)
   char temp[100];
   va_list ap;
   va_start (ap, format);
   vsnprintf (temp, sizeof (temp), format, ap);
   va_end (ap);
   jo_lit (j, tag, temp);
}

void
jo_baseN (jo_t j, const char *tag, const void *src, size_t slen, uint8_t bits, const char *alphabet)
{                               // base 16/32/64 binary to string
   if (jo_write_check (j, tag))
      return;
   jo_write (j, '"');
   unsigned int i = 0,
      b = 0,
      v = 0;
   while (i < slen)
   {
      b += 8;
      v = (v << 8) + ((uint8_t *) src)[i++];
      while (b >= bits)
      {
         b -= bits;
         jo_write (j, alphabet[(v >> b) & ((1 << bits) - 1)]);
      }
   }
   if (b)
   {                            // final bits
      b += 8;
      v <<= 8;
      b -= bits;
      jo_write (j, alphabet[(v >> b) & ((1 << bits) - 1)]);
      while (b)
      {                         // padding
         while (b >= bits)
         {
            b -= bits;
            jo_write (j, '=');
         }
         if (b)
            b += 8;
      }
   }
   jo_write (j, '"');
}

void
jo_datetime (jo_t j, const char *tag, time_t t)
{
   if (t < 1000000000)
      jo_null (j, tag);
   else
   {
      struct tm tm;
      gmtime_r (&t, &tm);
      jo_stringf (j, tag, "%04d-%02d-%02dT%02d:%02d:%02dZ", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min,
                  tm.tm_sec);
   }
}

ssize_t
jo_strncpyd (jo_t j, void *dstv, size_t dlen, uint8_t bits, const char *alphabet)
{                               // Base16/32/64 string to binary
   uint8_t *dst = dstv;
   if (!j || j->err || !j->parse)
      return -1;
   if (jo_peek (j) != '"')
      return -1;                // Not a string
   jo_t p = jo_link (j);
   jo_read (p);                 // skip "
   int b = 0,
      v = 0,
      c,
      ptr = 0;
   while ((c = jo_read_str (p)) >= 0 && c != '=')
   {
      char *q = strchr (alphabet, bits < 6 ? toupper (c) : c);
      if (!c || !q)
      {                         // Bad character
         if (!c || isspace (c) || c == '\r' || c == '\n')
            continue;           // space
         jo_free (&p);
         return -1;             // Bad
      }
      v = (v << bits) + (q - alphabet);
      b += bits;
      if (b >= 8)
      {                         // output byte
         b -= 8;
         if (dst && ptr < dlen)
            dst[ptr] = (v >> b);
         ptr++;
      }
   }
   jo_free (&p);
   return ptr;
}

char *
jo_strdup (jo_t j)
{                               // Malloc copy of string
   ssize_t len = jo_strlen (j);
   if (len < 0)
      return NULL;
   char *str = malloc (len + 1);
   jo_strncpy (j, str, len + 1);
   return str;
}

void
jo_int (jo_t j, const char *tag, int64_t val)
{                               // Add an integer
   jo_litf (j, tag, "%lld", val);
}

void
jo_bool (jo_t j, const char *tag, int val)
{                               // Add a bool (true if non zero passed)
   jo_lit (j, tag, val ? "true" : "false");
}

void
jo_null (jo_t j, const char *tag)
{                               // Add a null
   jo_lit (j, tag, "null");
}

// Parsing

static inline int
jo_ws (jo_t j)
{                               // Skip white space, and return peek at next
   int c = jo_peek (j);
   while (c == ' ' || c == '\t' || c == '\r' || c == '\n')
   {
      jo_read (j);
      c = jo_peek (j);
   }
   return c;
}

jo_type_t
jo_here (jo_t j)
{                               // Return what type of thing we are at - we are always at a value of some sort, which has a tag if we are in an object
   if (!j || j->err || !j->parse)
      return JO_END;
   int c = jo_ws (j);
   if (c < 0)
      return JO_END;
   if (c == '}' || c == ']')
   {
      if (j->tagok)
      {
         j->err = "Missing value";
         return JO_END;
      }
      if (!j->level)
      {
         j->err = "Too many closed";
         return JO_END;
      }
      if (c != ((j->o[(j->level - 1) / 8] & (1 << ((j->level - 1) & 7))) ? '}' : ']'))
      {
         j->err = "Mismatched close";
         return JO_END;
      }
      return JO_CLOSE;
   }
   if (j->comma)
   {
      if (!j->level)
      {
         j->err = "Extra value at top level";
         return JO_END;
      }
      if (c != ',')
      {
         j->err = "Missing comma";
         return JO_END;
      }
      jo_read (j);
      c = jo_ws (j);
      j->comma = 0;             // Comma consumed
   }
   // These are a guess of type - a robust check is done by jo_next
   if (!j->tagok && j->level && (j->o[(j->level - 1) / 8] & (1 << ((j->level - 1) & 7))))
   {                            // We should be at a tag
      if (c != '"')
      {
         j->err = "Missing tag";
         return JO_END;
      }
      return JO_TAG;
   }
   if (c == '"')
      return JO_STRING;
   if (c == '{')
      return JO_OBJECT;
   if (c == '[')
      return JO_ARRAY;
   if (c == 'n')
      return JO_NULL;
   if (c == 't')
      return JO_TRUE;
   if (c == 'f')
      return JO_FALSE;
   if (c == '-' || (c >= '0' && c <= '9'))
      return JO_NUMBER;
   j->err = "Bad JSON";
   return JO_END;
}

jo_type_t
jo_next (jo_t j)
{                               // Move to next value, this validates what we are skipping. A tag and its value are separate
   int c;
   if (!j || !j->parse || j->err)
      return JO_END;
   switch (jo_here (j))
   {
   case JO_END:                // End or error
      break;
   case JO_TAG:                // Tag
      jo_read (j);              // "
      while (jo_read_str (j) >= 0);
      if (!j->err && jo_read (j) != '"')
         j->err = "Missing closing quote on tag";
      jo_ws (j);
      if (!j->err && jo_read (j) != ':')
         j->err = "Missing colon after tag";
      j->tagok = 1;             // We have skipped tag
      break;
   case JO_OBJECT:
      jo_read (j);              // {
      if (j->level >= JO_MAX)
      {
         j->err = "JSON too deep";
         break;
      }
      j->o[j->level / 8] |= (1 << (j->level & 7));
      j->level++;
      j->comma = 0;
      j->tagok = 0;
      break;
   case JO_ARRAY:
      jo_read (j);              // ]
      if (j->level >= JO_MAX)
      {
         j->err = "JSON too deep";
         break;
      }
      j->o[j->level / 8] &= ~(1 << (j->level & 7));
      j->level++;
      j->comma = 0;
      j->tagok = 0;
      break;
   case JO_CLOSE:
      jo_read (j);              // }/]
      j->level--;               // Was checked by jo_here()
      j->comma = 1;
      j->tagok = 0;
      break;
   case JO_STRING:
      jo_read (j);              // "
      while (jo_read_str (j) >= 0);
      if (!j->err && jo_read (j) != '"')
         j->err = "Missing closing quote on string";
      j->comma = 1;
      j->tagok = 0;
      break;
   case JO_NUMBER:
      if (jo_peek (j) == '-')
         jo_read (j);           // minus
      if ((c = jo_peek (j)) == '0')
         jo_read (j);           // just zero
      else if (c >= '1' && c <= '9')
         while ((c = jo_peek (j)) >= '0' && c <= '9')
            jo_read (j);        // int
      if (jo_peek (j) == '.')
      {                         // real
         jo_read (j);
         if ((c = jo_peek (j)) < '0' || c > '9')
            j->err = "Bad real, must be digits after decimal point";
         else
            while ((c = jo_peek (j)) >= '0' && c <= '9')
               jo_read (j);     // frac
      }
      if ((c = jo_peek (j)) == 'e' || c == 'E')
      {                         // exp
         jo_read (j);
         if ((c = jo_peek (j)) == '-' || c == '+')
            jo_read (j);
         if ((c = jo_peek (j)) < '0' || c > '9')
            j->err = "Bad exp";
         else
            while ((c = jo_peek (j)) >= '0' && c <= '9')
               jo_read (j);     // exp
      }
      j->comma = 1;
      j->tagok = 0;
      break;
   case JO_NULL:
      if (jo_read (j) != 'n' || //
          jo_read (j) != 'u' || //
          jo_read (j) != 'l' || //
          jo_read (j) != 'l')
         j->err = "Misspelled null";
      j->comma = 1;
      j->tagok = 0;
      break;
   case JO_TRUE:
      if (jo_read (j) != 't' || //
          jo_read (j) != 'r' || //
          jo_read (j) != 'u' || //
          jo_read (j) != 'e')
         j->err = "Misspelled true";
      j->comma = 1;
      j->tagok = 0;
      break;
   case JO_FALSE:
      if (jo_read (j) != 'f' || //
          jo_read (j) != 'a' || //
          jo_read (j) != 'l' || //
          jo_read (j) != 's' || //
          jo_read (j) != 'e')
         j->err = "Misspelled false";
      j->comma = 1;
      j->tagok = 0;
      break;
   }
   return jo_here (j);
}

static ssize_t
jo_cpycmp (jo_t j, void *strv, size_t max, uint8_t cmp)
{                               // Copy or compare (-1 for j<str, +1 for j>str)
   char *str = strv;
   char *end = str + max;
   if (!j || !j->parse || j->err)
   {                            // No pointer
      if (!cmp)
         return -1;             // Invalid length
      if (!str)
         return 0;              // null==null?
      return 1;                 // str>null
   }
   jo_t p = jo_link (j);
   int c = jo_peek (p);
   ssize_t result = 0;
   void process (int c)
   {                            // Compare or copy or count, etc
      if (cmp)
      {                         // Comparing
         if (!str)
            return;             // Uh
         if (str >= end)
         {
            result = 1;         // str ended, so str<j
            return;
         }
         int c2 = *str++,
            q = 0;
         if (c2 >= 0xF0)
         {
            c2 &= 0x07;
            q = 3;
         } else if (c >= 0xE0)
         {
            c2 &= 0x0F;
            q = 2;
         } else if (c >= 0xC0)
         {
            c2 &= 0x1F;
            q = 1;
         }
         while (q-- && str < end && *str >= 0x80 && *str < 0xC0)
            c2 = (c2 << 6) + (*str++ & 0x3F);
         if (c < c2)
         {
            result = -1;        // str>j
            return;
         }
         if (c > c2)
         {
            result = 1;         // str<j
            return;
         }
      } else
      {                         // Copy or count
         void add (uint8_t v)
         {
            if (str && str < end - 1)
               *str++ = v;      // store, but allow for final null always
            result++;           // count
         }
         if (c >= 0xF0)
         {
            add (0xF0 + (c >> 18));
            add (0xC0 + ((c >> 12) & 0x3F));
            add (0xC0 + ((c >> 6) & 0x3F));
            add (0xC0 + (c & 0x3F));
         } else if (c >= 0xE0)
         {
            add (0xF0 + (c >> 12));
            add (0xC0 + ((c >> 6) & 0x3F));
            add (0xC0 + (c & 0x3F));
         } else if (c >= 0xC0)
         {
            add (0xF0 + (c >> 6));
            add (0xC0 + (c & 0x3F));
         } else
            add (c);
      }
   }
   if (c == '"')
   {                            // String
      jo_read (p);
      while ((c = jo_read_str (p)) >= 0 && (!cmp || !result))
         process (c);
   } else
   {                            // Literal or number
      while ((c = jo_read (p)) >= 0 && c > ' ' && c != ',' && c != '[' && c != '{' && c != ']' && c != '}' && (!cmp || !result))
         process (c);
   }
   if (!cmp && str && str < end)
      *str = 0;                 // Final null...
   if (!result && cmp && str && str < end)
      result = -1;              // j ended, do str>j
   jo_free (&p);
   return result;
}

ssize_t
jo_strlen (jo_t j)
{                               // Return byte length, if a string or tag this is the decoded byte length, else length of literal
   return jo_cpycmp (j, NULL, 0, 0);
}

ssize_t
jo_strncpy (jo_t j, void *target, size_t max)
{                               // Copy from current point to a string. If a string or a tag, remove quotes and decode/deescape
   return jo_cpycmp (j, target, max, 0);
}

ssize_t
jo_strncmp (jo_t j, void *source, size_t max)
{                               // Compare from current point to a string. If a string or a tag, remove quotes and decode/deescape
   return jo_cpycmp (j, source, max, 1);
}

jo_type_t
jo_skip (jo_t j)
{                               // Skip to next value at this level
   jo_type_t t = jo_here (j);
   if (t > JO_CLOSE)
   {
      int l = jo_level (j);
      while ((t = jo_next (j)) != JO_END && jo_level (j) > l);
      if (!j->err && t == JO_END && j->level)
         j->err = "Unclosed";
   }
   return t;
}

jo_type_t
jo_find (jo_t j, const char *path)
{                               // Find a path, JSON path style. Does not do [n] or ['name'] yet
   jo_rewind (j);
   if (*path == '$')
   {
      if (!path[1])
         return jo_here (j);    // root
      if (path[1] == '.')
         path += 2;             // We always start from root anyway
   }
   while (*path)
   {
      jo_type_t t = jo_here (j);
      if (t != JO_OBJECT)
         break;                 // Not an object
      const char *tag = path;
      while (*path && *path != '.')
         path++;
      int len = path - tag;
      if (*path)
         path++;
      t = jo_next (j);
      while (t == JO_TAG)
      {                         // Scan the object
         if (len == 1 && *tag == '*')
            break;              // Found - wildcard - only finds first entry
         if (!jo_strncmp (j, (char *) tag, len))
            break;              // Found
         jo_next (j);
         t = jo_skip (j);
      }
      if (t != JO_TAG)
         break;                 // not found
      t = jo_next (j);
      if (!*path)
         return t;              // Found
   }
   return JO_END;
}

const char *
jo_debug (jo_t j)
{                               // Debug string
   if (!j)
      return "No j";
   if (!j->parse)
   {                            // Where we are up to creating
      jo_store (j, 0);          // add null
      if (j->null)
         return j->buf;
      return "Not terminated";
   }
   return j->buf + j->ptr;      // Where we are (note, may not be 0 terminated)
}

int64_t
jo_read_int (jo_t j)
{
   if (!j || !j->parse || jo_here (j) != JO_NUMBER)
      return -1;
   int64_t n = 0,
      c,
      s = 1;
   jo_t p = jo_link (j);
   c = jo_read (p);
   if (c == '-')
   {
      s = -1;
      c = jo_read (p);
   }
   while (c >= '0' && c <= '9')
   {
      n = n * 10 + c - '0';
      c = jo_read (p);
   }
   jo_free (&p);
   return n * s;
}

long double
jo_read_float (jo_t j)
{
   if (!j || !j->parse || jo_here (j) != JO_NUMBER)
      return NAN;
   char temp[50];
   ssize_t l = jo_strncpy (j, temp, sizeof (temp));
   if (l <= 0)
      return NAN;
   char *end = NULL;
   long double value = strtold (temp, &end);
   if (!end || *end)
      return NAN;
   return value;
}

time_t
jo_read_datetime (jo_t j)
{                               // Get a datetime
   if (jo_here (j) != JO_STRING)
      return -1;
   char dt[21];
   ssize_t l = jo_strncpy (j, dt, sizeof (dt));
   if (l != 10 && l != 19 && l != 20)
      return -1;
   int y = 0,
      m = 0,
      d = 0,
      H = 0,
      M = 0,
      S = 0;
   int n = sscanf (dt, "%d-%d-%d%*c%d:%d:%d", &y, &m, &d, &H, &M, &S);
   if (n != 3 && n != 6)
      return 1;
   struct tm tm = {.tm_year = y - 1900,.tm_mon = m - 1,.tm_mday = d,.tm_hour = H,.tm_min = M,.tm_sec = S,.tm_isdst = -1 };
   time_t t = 0;
#if 0                           // ESP IDF has no timegm!
   if (l == 20 && dt[l - 1] == 'Z')
      t = timegm (&tm);
   else
      t = timelocal (&tm);
#else
   if (l == 20 && dt[l - 1] == 'Z')
      tm.tm_isdst = 0;
   t = mktime (&tm);
#endif
   return t;
}

jo_t
jo_parse_query (const char *buf)
{                               // Parse a query string format into a JSON object string (allocated)
   if (!buf)
      return NULL;
   jo_t j = jo_object_alloc ();
   if (!j)
      return j;
   while (*buf)
   {
      jo_write (j, '"');
      while (*buf && *buf != '=')
         jo_write_char (j, *buf++);
      if (*buf)
         buf++;
      jo_write (j, '"');
      jo_write (j, ':');
      jo_write (j, '"');        // Do all as strings
      while (*buf && *buf != '&')
      {
         int c = *buf++;
         if (c == '+')
            c = ' ';
         else if (c == '%' && isxdigit ((int) *buf) && isxdigit ((int) buf[1]))
         {
            c = (*buf & 0xF) + (isalpha ((int) *buf) ? 9 : 0);
            buf++;
            c = (c << 4) + (*buf & 0xF) + (isalpha ((int) *buf) ? 9 : 0);
            buf++;
         }
         jo_write_char (j, c);
      }
      jo_write (j, '"');
      if (*buf)
         buf++;
      if (*buf)
         jo_write (j, ',');
   }
   jo_close (j);
   return j;
}
