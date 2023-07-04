// ==========================================================================
//
//                       Adrian's JSON Library - ajlparse.c
//
// ==========================================================================

    /*
       Copyright (C) 2020-2020  RevK and Andrews & Arnold Ltd

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

#include "ajlparse.h"
#include <stdarg.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#undef NDEBUG                   // Uses side effects in asset
#include <assert.h>

struct ajl_s
{
   char *buf;                   // Current buffer input/output
   size_t bufptr;               // Current buffer input/output pointer
   size_t buflen;               // Current end if input buffer
   size_t bufmax;               // Set if buf is malloc'd, to size of buffer
   ajl_func_t *func;            // Read/write function for buffered i/o
   void *arg;                   // Arg for read/write function
   int line;                    // Line number
   int posn;                    // Character position
   int level;                   // Current level
   int maxlevel;                // Max level allocated flags
   const char *error;           // Current error
   unsigned char *flags;        // Flags
   unsigned char peek;          // Next character for read
   unsigned char isread:1;      // Is read, not write
   unsigned char eof:1;         // Have reached end of file (peek no longer valid)
   unsigned char pretty:1;      // Formatted output
   unsigned char started:1;     // Formatting started
   unsigned char peeked:1;      // We are one byte ahead
};
#define	COMMA	1               // flags
#define OBJECT	2

// Note that / is technically an optional escape, but because of danger of use of </script> it is always escaped here
#define escapes \
	esc ('"', '"') \
        esc ('\\', '\\') \
        esc ('/', '/') \
	esc ('b', '\b') \
	esc ('f', '\f') \
	esc ('n', '\n') \
	esc ('r', '\r') \
	esc ('t', '\t') \


// Local functions
#define validate(j) if(!j)return "NULL control passed"; if(j->error)return j->error;

#define	BUFBLOCK	(16*1024)

ssize_t
ajl_file_read (void *arg, void *buf, size_t l)
{
   return fread (buf, 1, l, arg);
}

ssize_t
ajl_file_write (void *arg, void *buf, size_t l)
{
   return fwrite (buf, 1, l, arg);
}

ssize_t
ajl_fd_read (void *arg, void *buf, size_t l)
{
   return read ((long) arg, buf, l);
}

ssize_t
ajl_fd_write (void *arg, void *buf, size_t l)
{
   return write ((long) arg, buf, l);
}

void
ajl_flush (const ajl_t j)
{                               // Flush out buffered write
   if (!j || j->error || j->isread)
      return;
   if (j->buf && j->bufptr)
   {
      if (!j->func)
         j->error = "Write full";
      else
      {
         size_t p = 0;
         while (p < j->bufptr)
         {
            ssize_t l = j->func (j->arg, j->buf + p, j->bufptr - p);
            if (l <= 0)
            {
               j->error = "Write fail";
               break;
            }
            p += l;
         }
      }
   }
   j->bufptr = 0;
}

void
ajl_put (const ajl_t j, char c)
{                               // Write a character
   if (!j || j->error || j->isread)
      return;
   if (j->bufptr == j->bufmax)
   {
      ajl_flush (j);
      if (!j->buf && !(j->buf = malloc (j->bufmax = BUFBLOCK)))
         j->bufmax = 0;         // Malloc failed
   }
   if (j->buf && j->bufptr < j->bufmax)
   {
      j->buf[j->bufptr++] = c;
      return;
   }
   j->error = "Write failed";
}

void
ajl_puts (const ajl_t j, const char *s)
{                               // Write a string
   while (*s)
      ajl_put (j, *s++);
}

int
ajl_peek (const ajl_t j)
{
   if (!j)
      return -4;
   if (!j->isread)
      return -3;
   if (j->error)
      return -2;
   if (j->eof)
      return -1;
   return j->peek;
}

void
ajl_next (const ajl_t j)
{                               // Read next (and possibly copy to o)
   j->peek = 0;
   j->peeked = 0;
   if (!j || j->error || j->eof || !j->isread)
      return;
   // end of buffer?
   if (j->bufptr == j->buflen)
   {                            // Get next buffer
      if (j->func && !j->buf && !(j->buf = malloc (j->bufmax = BUFBLOCK)))
         j->bufmax = 0;         // Malloc failed
      if (j->func && j->buf)
      {
         ssize_t l = j->func (j->arg, j->buf, j->bufmax);
         if (l > 0)
         {
            j->buflen = l;
            j->bufptr = 0;
         }
      }
   }
   if (!j->buf || j->bufptr == j->buflen)
      j->eof = 1;
   if (j->eof)
      return;
   j->peek = j->buf[j->bufptr++];
   j->peeked = 1;
   if ((j->peek >= 0x20 && j->peek < 0x80) || j->peek >= 0xC0)
      j->posn++;                // Count character (UTF-8 as one)
   else if (j->peek == '\n')
   {
      j->line++;
      j->posn = 1;
   }
   return;
}

void
ajl_copy (const ajl_t j, FILE * o)
{
   if (j && !j->error && !j->eof && o)
      fputc (j->peek, o);
   ajl_next (j);
}

int
ajl_isws (unsigned char c)
{
   return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

void
ajl_skip_ws (const ajl_t j)
{                               // Skip white space
   if (!j || j->error)
      return;
   while (!j->eof && ajl_isws (j->peek))
      ajl_next (j);
}

static inline const char *
skip_comma (const ajl_t j)
{                               // Skip initial comma and whitespace
   validate (j);
   ajl_skip_ws (j);
   if (j->peek == ',')
   {                            // skip comma
      if (!(j->flags[j->level] & COMMA))
         return j->error = "Unexpected comma";
      ajl_next (j);
      ajl_skip_ws (j);
   } else if (j->flags[j->level] & COMMA)
      return j->error = "Expecting comma";
   return NULL;
}

const char *
ajl_string (const ajl_t j, FILE * o)
{                               // Process a string (i.e. starting and ending with quotes and using escapes), writing decoded string to file if not zero
   validate (j);
   if (j->eof)
      return j->error = "EOF at start of string";
   if (j->peek != '"')
      return j->error = "Missing quote at start of string";
   ajl_next (j);
   while (!j->eof && j->peek != '"')
   {
      if (j->peek == '\\')
      {                         // Escaped character
         ajl_next (j);
         if (j->eof)
            return j->error = "Bad escape at EOF";
         if (j->peek == 'u')
         {                      // hex
            ajl_next (j);
            if (!isxdigit (j->peek))
               return j->error = "Bad hex escape";
            unsigned int c = (isalpha (j->peek) ? 9 : 0) + (j->peek & 0xF);
            ajl_next (j);
            if (!isxdigit (j->peek))
               return j->error = "Bad hex escape";
            c = (c << 4) + (isalpha (j->peek) ? 9 : 0) + (j->peek & 0xF);
            ajl_next (j);
            if (!isxdigit (j->peek))
               return j->error = "Bad hex escape";
            c = (c << 4) + (isalpha (j->peek) ? 9 : 0) + (j->peek & 0xF);
            ajl_next (j);
            if (!isxdigit (j->peek))
               return j->error = "Bad hex escape";
            c = (c << 4) + (isalpha (j->peek) ? 9 : 0) + (j->peek & 0xF);
            ajl_next (j);
            if (c >= 0xDC00 && c <= 0xDFFF)
               return "Unexpected UTF-16 low order";
            if (c >= 0xD800 && c <= 0xDBFF)
            {                   // UTF-16
               if (j->eof || j->peek != '\\')
                  return "Bad UTF-16, missing second part";
               ajl_next (j);
               if (j->eof || j->peek != 'u')
                  return "Bad UTF-16, missing second part";
               ajl_next (j);
               if (!isxdigit (j->peek))
                  return j->error = "Bad hex escape";
               unsigned int c2 = (isalpha (j->peek) ? 9 : 0) + (j->peek & 0xF);
               ajl_next (j);
               if (!isxdigit (j->peek))
                  return j->error = "Bad hex escape";
               c2 = (c2 << 4) + (isalpha (j->peek) ? 9 : 0) + (j->peek & 0xF);
               ajl_next (j);
               if (!isxdigit (j->peek))
                  return j->error = "Bad hex escape";
               c2 = (c2 << 4) + (isalpha (j->peek) ? 9 : 0) + (j->peek & 0xF);
               ajl_next (j);
               if (!isxdigit (j->peek))
                  return j->error = "Bad hex escape";
               c2 = (c2 << 4) + (isalpha (j->peek) ? 9 : 0) + (j->peek & 0xF);
               ajl_next (j);
               if (c2 < 0xDC00 || c2 > 0xDFFF)
                  return "Bad UTF-16, second part invalid";
               c = ((c & 0x3FF) << 10) + (c2 & 0x3FF) + 0x10000;
            }
            if (o)
            {
               if (c >= 0x10000)
               {
                  fputc (0xF0 + (c >> 18), o);
                  fputc (0x80 + ((c >> 12) & 0x3F), o);
                  fputc (0x80 + ((c >> 6) & 0x3F), o);
                  fputc (0x80 + (c & 0x3F), o);
               } else if (c >= 0x800)
               {
                  fputc (0xE0 + (c >> 12), o);
                  fputc (0x80 + ((c >> 6) & 0x3F), o);
                  fputc (0x80 + (c & 0x3F), o);
               } else if (c >= 0x80)
               {
                  fputc (0xC0 + (c >> 6), o);
                  fputc (0x80 + (c & 0x3F), o);
               } else
                  fputc (c, o);
            }
         }
#define esc(a,b) else if(j->peek==a){ajl_next(j);if(o)fputc(b,o);}
         escapes
#undef esc
            else
            return j->error = "Bad escape";
      } else if (j->peek >= 0xF8)
         return j->error = "Bad UTF-8";
      else if (j->peek >= 0xF0)
      {                         // 4 byte
         ajl_copy (j, o);
         if (j->peek < 0x80 || j->peek >= 0xC0)
            return j->error = "Bad UTF-8";
         ajl_copy (j, o);
         if (j->peek < 0x80 || j->peek >= 0xC0)
            return j->error = "Bad UTF-8";
         ajl_copy (j, o);
         if (j->peek < 0x80 || j->peek >= 0xC0)
            return j->error = "Bad UTF-8";
         ajl_copy (j, o);
      } else if (j->peek >= 0xE0)
      {                         // 3 byte
         ajl_copy (j, o);
         if (j->peek < 0x80 || j->peek >= 0xC0)
            return j->error = "Bad UTF-8";
         ajl_copy (j, o);
         if (j->peek < 0x80 || j->peek >= 0xC0)
            return j->error = "Bad UTF-8";
         ajl_copy (j, o);
      } else if (j->peek >= 0xC0)
      {                         // 2 byte
         ajl_copy (j, o);
         if (j->peek < 0x80 || j->peek >= 0xC0)
            return j->error = "Bad UTF-8";
         ajl_copy (j, o);
      } else if (j->peek >= 0x80)
         return j->error = "Bad UTF-8";
      else
         ajl_copy (j, o);
   }
   if (j->error)
      return j->error;
   if (j->eof)
      return j->error = "EOF in string";
   if (j->peek != '"')
      return j->error = "Missing end quote";
   ajl_next (j);
   return NULL;
}

#define checkerr if(j->error)return AJL_ERROR
#define checkeof if(j->eof&&!j->error)j->error="Unexpected EOF";checkerr
#define makeerr(e) do{j->error=e;return AJL_ERROR;}while(0)

const char *
ajl_number (const ajl_t j, FILE * o)
{                               // Process a number strictly to JSON spec for a number, writing to file if not null
   validate (j);
   if (j->peek == '-')
      ajl_copy (j, o);          // Optional minus
   if (j->peek == '0')
   {
      ajl_copy (j, o);
      if (!j->eof && isdigit (j->peek))
         return j->error = "Invalid int starting 0";
   } else if (j->eof || !isdigit (j->peek))
      return j->error = "Invalid number";
   while (!j->eof && isdigit (j->peek))
      ajl_copy (j, o);
   if (!j->eof && j->peek == '.')
   {                            // Fraction
      ajl_copy (j, o);
      if (j->eof || !isdigit (j->peek))
         return j->error = "Invalid fraction";
      while (!j->eof && isdigit (j->peek))
         ajl_copy (j, o);
   }
   if (!j->eof && (j->peek == 'e' || j->peek == 'E'))
   {                            // Exponent
      ajl_copy (j, o);
      if (!j->eof && (j->peek == '-' || j->peek == '+'))
         ajl_copy (j, o);
      if (j->eof || !isdigit (j->peek))
         return j->error = "Invalid exponent";
      while (!j->eof && isdigit (j->peek))
         ajl_copy (j, o);
   }
   return NULL;
}

// Common functions
void
ajl_delete (ajl_t * jp)
{                               // Close control structure. Free j
   if (!jp)
      return;
   ajl_t j = *jp;
   if (!j)
      return;
   ajl_end (j);
   if (j->flags)
      free (j->flags);
   if (j->buf && j->bufmax)
      free (j->buf);            // Was malloc'd
   free (j);
   *jp = NULL;
}

const char *
ajl_end (const ajl_t j)
{                               // Close control structure
   validate (j);
   if (j->pretty)
      ajl_put (j, '\n');
   ajl_flush (j);
   return NULL;
}

const char *
ajl_error (const ajl_t j)
{                               // Return if error set in JSON object, or NULL if not error
   validate (j);
   return NULL;
}

// Allocate control structure for parsing, from file or from memory
ajl_t
ajl_init (unsigned char isread)
{
   ajl_t j = calloc (1, sizeof (*j));
   if (!j)
      return j;
   assert ((j->flags = malloc (j->maxlevel = 10)));
   j->flags[j->level] = 0;
   j->line = 1;
   j->posn = 1;
   j->isread = isread;
   return j;
}

ajl_t
ajl_text (const char *text)
{                               // Simple text parse
   if (!text)
      return NULL;
   ajl_t j = ajl_init (1);
   if (!j)
      return j;
   j->buf = (char *) text;
   j->buflen = strlen (text);
   ajl_next (j);
   return j;
}

const char *
ajl_done (ajl_t * jp)
{                               // delete J and return pointer to next character where this was processing fixed text input
   if (!jp)
      return NULL;
   ajl_t j = *jp;
   if (!j)
      return NULL;
   const char *e = NULL;
   if (j->buf && !j->bufmax && (!j->peeked || j->bufptr))
   {
      e = j->buf + j->bufptr;
      if (j->peeked)
         e--;
   }
   free (j);
   *jp = NULL;
   return e;
}

const char *
ajl_reset (ajl_t j)
{                               // Reset parse
   if (!j)
      return NULL;
   j->level = 0;
   j->line = 1;
   j->posn = 0;
   j->flags[j->level] = 0;
   return NULL;
}

ajl_t
ajl_read_func (ajl_func_t * func, void *arg)
{                               // Read using functions
   ajl_t j = ajl_init (1);
   if (!j)
      return j;
   j->func = func;
   j->arg = arg;
   j->peek = ' ';               // Dummy before start
   return j;
}

void *
ajl_arg (ajl_t p)
{
   if (p)
      return NULL;
   return p->arg;
}

ajl_t
ajl_read_fd (int f)
{
   if (f < 0)
      return NULL;
   return ajl_read_func (ajl_fd_read, (void *) (long) f);
}

ajl_t
ajl_read (FILE * f)
{
   if (!f)
      return NULL;
   return ajl_read_func (ajl_file_read, f);
}

ajl_t
ajl_read_file (const char *filename)
{
   return ajl_read (fopen (filename, "r"));
}

ajl_t
ajl_read_mem (const char *buffer, ssize_t len)
{
   if (!buffer)
      return NULL;
   if (len < 0)
      len = strlen (buffer);
   ajl_t j = ajl_init (1);
   if (!j)
      return j;
   j->buf = (char *) buffer;
   j->buflen = len;
   ajl_next (j);
   return j;
}

int
ajl_line (const ajl_t j)
{                               // Return current line number in source
   if (!j)
      return -1;
   return j->line;
}

int
ajl_char (const ajl_t j)
{                               // Return current character position in source
   if (!j)
      return -1;
   return j->posn;
}

int
ajl_level (const ajl_t j)
{                               // return current level of nesting
   if (!j)
      return -1;
   return j->level;
}

int
ajl_isobject (const ajl_t j)
{
   return j->flags[j->level] & OBJECT;
}

// The basic parsing function consumes next element, and returns a type as above
// If the element is within an object, then the tag is parsed and mallocd and stored in tag
// The value of the element is parsed, and malloced and stored in value (a null is appended, not included in len)
// The length of the value is stored in len - this is mainly to allow for strings that contain a null
ajl_type_t
ajl_parse (const ajl_t j, unsigned char **tag, unsigned char **value, size_t *len)
{
   if (tag)
      *tag = NULL;
   if (value)
      *value = NULL;
   if (len)
      *len = 0;
   if (!j || j->error)
      return AJL_ERROR;
   ajl_skip_ws (j);
   if (j->eof)
   {
      if (!j->level)
         return AJL_EOF;        // Normal end
      checkeof;
   }
   if (j->peek == ((j->flags[j->level] & OBJECT) ? '}' : ']'))
   {                            // end of object or array
      if (!j->level)
         makeerr ("Too many closes");
      j->level--;
      if (!j->level)
      {                         // Final close - whitespace is valid here so we fake it rather than reading ahead
         j->peeked = 0;         // Ensure ajl_done is not confused by the fact we have not read ahead
         j->peek = '\n';        // valid whitespace
      } else
         ajl_next (j);          // Otherwise consume closing ]/} as normal
      return AJL_CLOSE;
   }
   skip_comma (j);
   checkeof;
   if (j->flags[j->level] & OBJECT)
   {                            // skip tag
      if (j->peek != '"')
         makeerr ("Missing tag in object");
      size_t len;
      FILE *o = NULL;
      if (tag)
         o = open_memstream ((char **) tag, &len);
      ajl_string (j, o);
      if (o)
         fclose (o);
      checkerr;
      ajl_skip_ws (j);
      checkeof;
      if (j->peek == ':')
      {                         // found colon, skip it and white space
         ajl_next (j);
         ajl_skip_ws (j);
      } else
         makeerr ("Missing colon");
      checkeof;
   }
   j->flags[j->level] |= COMMA;
   if (j->peek == '{')
   {                            // Start object
      if (j->level + 1 >= j->maxlevel)
         assert ((j->flags = realloc (j->flags, j->maxlevel += 10)));
      j->level++;
      j->flags[j->level] = OBJECT;
      ajl_next (j);
      return AJL_OBJECT;
   }
   if (j->peek == '[')
   {                            // Start array
      if (j->level + 1 >= j->maxlevel)
         assert ((j->flags = realloc (j->flags, j->maxlevel += 10)));
      j->level++;
      j->flags[j->level] = 0;
      ajl_next (j);
      return AJL_ARRAY;
   }
   // Get the value
   FILE *o = NULL;
   if (value)
      o = open_memstream ((char **) value, len);
   if (j->peek == '"')
   {
      ajl_string (j, o);
      if (o)
         fclose (o);
      checkerr;
      return AJL_STRING;
   }
   if (j->peek == '-' || isdigit (j->peek))
   {
      ajl_number (j, o);
      if (o)
         fclose (o);
      checkerr;
      return AJL_NUMBER;
   }
   // All that is left is a literal (null, true, false)
   char l[10],
    *p = l;
   while (!j->eof && isalpha (j->peek) && p < l + sizeof (l) - 1)
   {
      *p++ = j->peek;
      if (strncmp (l, "true", (int) (p - l)) && strncmp (l, "false", (int) (p - l)) && strncmp (l, "null", (int) (p - l)))
         break;                 // Break early for better error messages
      ajl_copy (j, o);
   }
   *p = 0;
   fclose (o);
   if (!strcmp (l, "true") || !strcmp (l, "false"))
      return AJL_BOOLEAN;
   if (!strcmp (l, "null"))
      return AJL_NULL;
   makeerr ("Unexpected token");
   return AJL_ERROR;
}

// Generate
// Allocate control structure for generating, to file or to memory
//
ajl_t
ajl_write_func (ajl_func_t * func, void *arg)
{                               // Read using functions
   ajl_t j = ajl_init (0);
   if (!j)
      return j;
   j->func = func;
   j->arg = arg;
   return j;
}

ajl_t
ajl_write (FILE * f)
{
   if (!f)
      return NULL;
   return ajl_write_func (ajl_file_write, f);
}

ajl_t
ajl_write_fd (int f)
{
   if (f < 0)
      return NULL;
   return ajl_write_func (ajl_fd_write, (void *) (long) f);
}

ajl_t
ajl_write_file (const char *filename)
{
   return ajl_write (fopen (filename, "w"));
}

ajl_t
ajl_write_mem (unsigned char **buffer, size_t *len)
{
   return ajl_write (open_memstream ((char **) buffer, len));
}

void
ajl_pretty (const ajl_t j)
{
   j->pretty = 1;
}

void
ajl_fwrite_string (FILE * o, const unsigned char *value, size_t len)
{                               // Write file UTF-8 string, escaped as necessary - to file
   if (!value)
   {
      fprintf (o, "null");
      return;
   }
   fputc ('"', o);
   while (len--)
   {
      unsigned char c = *value++;
#define esc(a,b) if(c==b){fputc('\\',o);fputc(a,o);} else
      escapes
#undef esc
         if (c < ' ')
         fprintf (o, "\\u00%02X", c);
      else
         fputc (c, o);
   }
   fputc ('"', o);
}

void
ajl_write_string (ajl_t j, const unsigned char *value, size_t len)
{                               // Write file UTF-8 string, escaped as necessary
   if (!value)
   {
      ajl_puts (j, "null");
      return;
   }
   ajl_put (j, '"');
   while (len--)
   {
      unsigned char c = *value++;
#define esc(a,b) if(c==b){ajl_put(j,'\\');ajl_put(j,a);} else
      escapes
#undef esc
         if (c < ' ')
      {
         ajl_puts (j, "\\u00");
         ajl_put (j, "0123456789ABCDEF"[c >> 4]);
         ajl_put (j, "0123456789ABCDEF"[c & 15]);
      } else
         ajl_put (j, c);
   }
   ajl_put (j, '"');
}

static void
add_binary (const ajl_t j, const unsigned char *value, size_t len)
{                               // Add binary data, escaped as necessary
   if (!value)
   {
      ajl_puts (j, "null");
      return;
   }
   ajl_put (j, '"');
   while (len--)
   {
      unsigned char c = *value++;
#define esc(a,b) if(c==b){ajl_put(j,'\\');ajl_put(j,a);} else
      escapes
#undef esc
         if (c < ' ' || c >= 0x80)
      {
         ajl_puts (j, "\\u00");
         ajl_put (j, "0123456789ABCDEF"[c >> 4]);
         ajl_put (j, "0123456789ABCDEF"[c & 15]);
      } else
         ajl_put (j, c);
   }
   ajl_put (j, '"');
}

static void
j_indent (const ajl_t j)
{
   if (j->started && j->pretty)
   {
      ajl_put (j, '\n');
      ajl_flush (j);
   }
   j->started = 1;
   if (j->pretty)
      for (int q = 0; q < j->level; q++)
         ajl_put (j, ' ');
}

static const char *
add_tag (const ajl_t j, const unsigned char *tag)
{                               // Add prefix tag or comma
   validate (j);
   if (j->flags[j->level] & COMMA)
      ajl_put (j, ',');
   j->flags[j->level] |= COMMA;
   j_indent (j);
   if (tag)
   {
      if (!(j->flags[j->level] & OBJECT))
         return j->error = "Not in object";
      ajl_write_string (j, tag, strlen ((char *) tag));
      ajl_put (j, ':');
   } else if (j->flags[j->level] & OBJECT)
      return j->error = "Tag required";
   return j->error;
}

const char *
ajl_add (const ajl_t j, const unsigned char *tag, const unsigned char *value)
{                               // Add pre-formatted value (expects quotes, escapes, etc)
   validate (j);
   add_tag (j, tag);
   while (*value)
      ajl_put (j, *value++);
   return j->error;
}

const char *
ajl_add_string (const ajl_t j, const unsigned char *tag, const unsigned char *value)
{                               // Add UTF-8 String, escaped for JSON
   validate (j);
   add_tag (j, tag);
   if (!value)
      ajl_puts (j, "null");
   else
      ajl_write_string (j, value, strlen ((char *) value));
   return j->error;
}

const char *
ajl_add_literal (const ajl_t j, const unsigned char *tag, const unsigned char *value)
{
   validate (j);
   add_tag (j, tag);
   ajl_puts (j, (char *) value ? : "null");
   return j->error;
}

const char *
ajl_add_stringn (const ajl_t j, const unsigned char *tag, const unsigned char *value, size_t len)
{                               // Add UTF-8 String, escaped for JSON
   validate (j);
   add_tag (j, tag);
   if (!value)
      ajl_puts (j, "null");
   else
      ajl_write_string (j, value, len);
   return j->error;
}

const char *
ajl_add_binary (const ajl_t j, const unsigned char *tag, const unsigned char *value, size_t len)
{                               // Add binary data as string, escaped for JSON
   validate (j);
   add_tag (j, tag);
   add_binary (j, value, len);
   return j->error;
}

const char *
ajl_add_number (const ajl_t j, const unsigned char *tag, const char *fmt, ...)
{                               // Add number (formattted)
   validate (j);
   add_tag (j, tag);
   va_list ap;
   va_start (ap, fmt);
   char *s = NULL;
   if (vasprintf (&s, fmt, ap) < 0)
      j->error = "malloc";
   va_end (ap);
   ajl_puts (j, s);
   free (s);
   return j->error;
}

const char *
ajl_add_boolean (const ajl_t j, const unsigned char *tag, unsigned char value)
{
   validate (j);
   add_tag (j, tag);
   ajl_puts (j, value ? "true" : "false");
   return j->error;
}

const char *
ajl_add_null (const ajl_t j, const unsigned char *tag)
{
   validate (j);
   add_tag (j, tag);
   ajl_puts (j, "null");
   return j->error;
}

const char *
ajl_add_object (const ajl_t j, const unsigned char *tag)
{                               // Start an object
   validate (j);
   add_tag (j, tag);
   ajl_put (j, '{');
   if (j->level + 1 >= j->maxlevel)
      j->flags = realloc (j->flags, j->maxlevel += 10);
   j->level++;
   j->flags[j->level] = OBJECT;
   return j->error;
}

const char *
ajl_add_array (const ajl_t j, const unsigned char *tag)
{                               // Start an array
   validate (j);
   add_tag (j, tag);
   ajl_put (j, '[');
   if (j->level + 1 >= j->maxlevel)
      j->flags = realloc (j->flags, j->maxlevel += 10);
   j->level++;
   j->flags[j->level] = 0;
   return j->error;
}

const char *
ajl_add_close (const ajl_t j)
{                               // close current array or object
   validate (j);
   if (!j->level)
      return j->error = "Too many closes";
   j->level--;
   j_indent (j);
   ajl_put (j, (j->flags[j->level + 1] & OBJECT) ? '}' : ']');
   return j->error;
}
