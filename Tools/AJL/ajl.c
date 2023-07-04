// ==========================================================================
//
//                       Adrian's JSON Library - ajl.c
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

#include <time.h>
#include "ajlcurl.h"
#include "ajlparse.c"
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <err.h>
#undef NDEBUG                   // Uses side effects in assert
#include <assert.h>
#ifdef	JCURL
#include <curl/curl.h>
#endif

#ifndef	strdupa
#define strdupa(x) strcpy(alloca(strlen(x)+1),x)
#endif

// This is a point in the JSON object
// A key feature is that the content of a point in a JSON object can be replaced in situ if needed.
// This means a root j_t passed to a function can be input and output JSON object if needed, replacing in-situ at the pointer (see j_replace)
struct j_s
{                               // JSON point strucccture
   j_t parent;                  // Parent (NULL if root)
   unsigned char *tag;          // Always malloced, present when this is a child of an object and so tagged
   union
   {                            // Based on children flag
      j_t *child;               // Array of (len) child pointer entries
      unsigned char *val;       // Malloced if malloc set, can be static, NULL if object, array, or null
   };
   int len;                     // Len of val or len of child (0 for null)
   int posn;                    // Position in parent
   unsigned char children:1;    // Is object or array, child is used not val
   unsigned char isarray:1;     // This is an array rather than an object (children is set)
   unsigned char isstring:1;    // This is a string rather than a literal (children is NULL)
   unsigned char malloc:1;      // val is malloc'd
};

static unsigned char valnull[] = "null";
static unsigned char valtrue[] = "true";
static unsigned char valfalse[] = "false";
static unsigned char valempty[] = "";
static unsigned char valzero[] = "";

const char JBASE64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
const char JBASE32[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
const char JBASE16[] = "0123456789ABCDEF";

// Safe free and NULL value
#define freez(x)        do{if(x)free(x);(x)=NULL;}while(0)

char *
j_errs (const char *fmt, ...)
{                               // Create malloc'd error string to return
   char *err = NULL;
   va_list ap;
   va_start (ap, fmt);
   int e = vasprintf (&err, fmt, ap);
   va_end (ap);
   if (e <= 0)
      errx (1, "malloc");
   return err;
}

void
j_err_exit (const char *e, const char *fn, int l)
{
   if (e)
      errx (1, "JSON fail %s line %d: %s", fn, l, e);
}

time_t
j_timez (const char *t, int z)  // convert iso time to time_t
{                               // Can do HH:MM:SS, or YYYY-MM-DD or YYYY-MM-DD HH:MM:SS, 0 for invalid
   if (!t)
      return 0;
   unsigned int Y = 0,
      M = 0,
      D = 0,
      h = 0,
      m = 0,
      s = 0;
   int hms (void)
   {
      while (isdigit (*t))
         h = h * 10 + *t++ - '0';
      if (*t++ != ':')
         return 0;
      while (isdigit (*t))
         m = m * 10 + *t++ - '0';
      if (*t++ != ':')
         return 0;
      while (isdigit (*t))
         s = s * 10 + *t++ - '0';
      if (*t == '.')
      {                         // fractions
         t++;
         while (isdigit (*t))
            t++;
      }
      return 1;
   }
   if (isdigit (t[0]) && isdigit (t[1]) && t[2] == ':')
   {
      if (!hms ())
         return 0;
      return h * 3600 + m * 60 + s;
   } else
   {
      while (isdigit (*t))
         Y = Y * 10 + *t++ - '0';
      if (*t++ != '-')
         return 0;
      while (isdigit (*t))
         M = M * 10 + *t++ - '0';
      if (*t++ != '-')
         return 0;
      while (isdigit (*t))
         D = D * 10 + *t++ - '0';
      if (*t == 'T' || *t == ' ')
      {                         // time
         t++;
         if (!hms ())
            return 0;
      }
   }
   if (!Y && !M && !D)
      return 0;
   struct tm tm = {
    tm_year: Y - 1900, tm_mon: M - 1, tm_mday: D, tm_hour: h, tm_min: m, tm_sec:s
   };
   if (*t == 'Z' || z)
      return timegm (&tm);      // UTC
   if (*t == '+' || *t == '-')
   {                            // Explicit time zone
      int Z = 0;
      char s = *t++;
      if (isdigit (t[0]) && isdigit (t[1]))
      {                         // Hours
         Z = ((t[0] - '0') * 10 + t[1] - '0') * 3600;
         t += 2;
         if (*t == ':')
            t++;
         if (isdigit (t[0]) && isdigit (t[1]))
         {                      // Minutes
            Z += ((t[0] - '0') * 10 + t[1] - '0') * 60;
            t += 2;
         }
         if (s == '-')
            Z = 0 - Z;
      }
      return timegm (&tm) - Z;
   }
   tm.tm_isdst = -1;            // work it out as local time
   return timelocal (&tm);      // Local time
}

char j_iso8601utc = 0;
void
j_format_datetime (time_t t, char v[26])
{                               // Format datetime
   struct tm tm;
   if (j_iso8601utc)
   {                            // ISO8601 UTC
      gmtime_r (&t, &tm);
      strftime (v, 26, "%FT%TZ", &tm);
   } else
   {                            // Local time
      localtime_r (&t, &tm);
      strftime (v, 26, "%F %T", &tm);
   }
}

// Decoding to a location, returns the length, can return length with NULL buffer passed to allow size to be checked
// If max allows it, a NULL is also added on end as common for strings to be encoded
ssize_t
j_baseNd (unsigned char *dst, size_t max, const char *src, const char *alphabet, unsigned int bits)
{
   if (!src)
      return -1;
   int b = 0,
      v = 0;
   size_t len = 0;
   while (*src && *src != '=')
   {
      char *q = strchr (alphabet, bits < 6 ? toupper (*src) : *src);
      if (!q)
      {                         // Bad character
         if (isspace (*src))
         {                      // allow spaces, including tabs, newlines, etc
            src++;
            continue;
         }
         return -1;             // Bad character
      }
      v = (v << bits) + (q - alphabet);
      b += bits;
      src++;
      if (b >= 8)
      {                         // output byte
         b -= 8;
         if (dst && len < max)
            dst[len] = v >> b;
         len++;
      }
   }
   if (dst && len < max)
      dst[len] = 0;             // Useful extra null if space
   return len;
}

ssize_t
j_based (const char *src, unsigned char **bufp, const char *alphabet, unsigned int bits)
{                               // Allocate memory and put in *bufp, adds extra null on end anyway as common for this to be text anyway. -1 for bad
   *bufp = NULL;
   ssize_t len = j_baseNd (NULL, 0, src, alphabet, bits) + 1;
   if (len < 0)
      return len;
   return j_baseNd (*bufp = malloc (len), len, src, alphabet, bits);
}

// Encoding
char *
j_baseN (size_t slen, const unsigned char *src, size_t dmax, char *dst, const char *alphabet, unsigned int bits)
{                               // base 16/32/64 binary to string
   unsigned int i = 0,
      o = 0,
      b = 0,
      v = 0;
   while (i < slen)
   {
      b += 8;
      v = (v << 8) + src[i++];
      while (b >= bits)
      {
         b -= bits;
         if (o < dmax)
            dst[o++] = alphabet[(v >> b) & ((1 << bits) - 1)];
      }
   }
   if (b)
   {                            // final bits
      b += 8;
      v <<= 8;
      b -= bits;
      if (o < dmax)
         dst[o++] = alphabet[(v >> b) & ((1 << bits) - 1)];
      while (b)
      {                         // padding
         while (b >= bits)
         {
            b -= bits;
            if (o < dmax)
               dst[o++] = '=';
         }
         if (b)
            b += 8;
      }
   }
   if (o >= dmax)
      return NULL;
   dst[o] = 0;
   return dst;
}

j_t
j_create (void)
{                               // Allocate a new JSON object tree that is empty, ready to be added to or read in to, NULL for error
   return calloc (1, sizeof (struct j_s));
}

static void
j_unlink (const j_t j)
{                               // Unlink from parent
   j_t p = j->parent;
   if (!p)
      return;                   // No parent
   j->parent = NULL;
   assert (p->children);
   assert (p->len);
   p->len--;
   for (int q = j->posn; q < p->len; q++)
   {
      p->child[q] = p->child[q + 1];
      p->child[q]->posn = q;
   }
}

static j_t
j_add_child (const j_t j)
{                               // Extend children
   if (!j)
      return NULL;
   if (!j->children)
   {
      j_null (j);
      j->children = 1;
   }
   assert ((j->child = realloc (j->child, sizeof (*j->child) * (j->len + 1))));
   j_t n = NULL;
   assert ((j->child[j->len] = n = calloc (1, sizeof (*n))));
   n->parent = j;
   n->posn = j->len++;
   return j_null (n);
}

static j_t
j_findtag (const j_t j, const unsigned char *tag)
{
   if (!j || !j->children || j->isarray)
      return NULL;
   for (int q = 0; q < j->len; q++)
      if (j->child[q]->tag && !strcmp ((char *) j->child[q]->tag, (char *) tag))
         return j->child[q];
   return NULL;
}

void
j_delete (j_t * jp)
{                               // Delete this value (remove from parent object if not root) and all sub objects, returns NULL
   if (!jp || !*jp)
      return;
   j_t j = *jp;
   j_null (j);                  // Clear all sub content, etc.
   j_unlink (j);
   freez (j->tag);
   freez (*jp);
}

void
j_free (j_t j)
{                               // Same as j_delete but where you don't care about zapping the pointer itself
   if (!j)
      return;
   j_null (j);                  // Clear all sub content, etc.
   j_unlink (j);
   freez (j->tag);
   freez (j);
}

// Moving around the tree, these return the j_t of the new point (or NULL if does not exist)
j_t
j_root (const j_t j)
{                               // Return root point
   if (!j)
      return j;
   j_t r = j;
   while (r->parent)
      r = r->parent;
   return r;
}

j_t
j_parent (const j_t j)
{                               // Parent of this point (NULL if this point is root)
   if (!j)
      return j;
   return j->parent;
}

j_t
j_next (const j_t j)
{                               // Next in parent object or array, NULL if at end
   if (!j || !j->parent || !j->parent->children || j->posn + 1 >= j->parent->len)
      return NULL;
   return j->parent->child[j->posn + 1];
}

j_t
j_prev (const j_t j)
{                               // Previous in parent object or array, NULL if at start
   if (!j || !j->parent || !j->parent->children || j->posn <= 0)
      return NULL;
   return j->parent->child[j->posn - 1];
}

j_t
j_first (const j_t j)
{                               // First entry in an object or array (same as j_index with 0)
   if (!j || !j->children || !j->len)
      return NULL;
   return j->child[0];
}

static j_t
j_findmake (const j_t cj, const char *path, int make)
{                               // Find object within this object by tag/path - make if any in path does not exist and make is set
   if (!cj || !path)
      return NULL;
   j_t j = cj;
   unsigned char *t = (unsigned char *) strdupa (path);
   while (*t)
   {
      if (!make && !j->children)
         return NULL;           // Not array or object
      if (*t == '[')
      {                         // array
         t++;
         if (make)
            j_array (j);        // Ensure is an array
         if (*t == ']')
         {                      // append
            if (!make)
               return NULL;     // Not making, so cannot append
            j = j_append (j);
         } else
         {                      // 
            int i = 0;
            while (isdigit (*t))
               i = i * 10 + *t++ - '0'; // Index
            if (*t != ']')
               return NULL;
            if (!make && i >= j->len)
               return NULL;
            if (make)
               while (i < j->len)
                  j_append (j);
            j = j_index (j, i);
         }
         t++;
      } else
      {                         // tag
         if (!make && j->isarray)
            return NULL;
         if (make)
            j_object (j);       // Ensure is an object
         unsigned char *e = (unsigned char *) t;
         while (*e && *e != '.' && *e != '[')
            e++;
         unsigned char q = *e;
         *e = 0;
         j_t n = j_findtag (j, t);
         if (!make && !n)
            return NULL;        // Not found
         if (!n)
         {
            n = j_add_child (j);
            n->tag = (unsigned char *) strdup ((char *) t);
         }
         *e = q;
         t = e;
         j = n;
      }
      if (*t == '.')
         t++;
   }
   return j;
}

j_t
j_find (const j_t j, const char *path)
{                               // Find object within this object by tag/path - NULL if any in path does not exist
   if (!path)
      return j;
   return j_findmake (j, path, 0);
}

j_t
j_index (const j_t j, int n)
{                               // Find specific point in an array, or object - NULL if not in the array
   if (!j || !j->children || n < 0 || n >= j->len)
      return NULL;
   return j->child[n];
}

j_t
j_named (const j_t j, const char *name)
{                               // Find named entry in an object
   return j_findtag (j, (const unsigned char *) name);
}


// Information about a point
const char *
j_name (const j_t j)
{                               // The tag of this object in parent, if it is in a parent object, else NULL
   if (!j)
      return NULL;
   return (char *) j->tag;
}

int
j_pos (const j_t j)
{                               // Position in parent array or object, -1 if this is the root object so not in another array/object
   if (!j)
      return -1;
   return j->posn;
}

const char *
j_val (const j_t j)
{                               // The value of this object as a string. NULL if not found. Note that a "null" string is a valid literal value
   if (!j || j->children)
      return NULL;
   return (char *) (j->val ? : valnull);
}

int
j_len (const j_t j)
{                               // The length of this value (characters if string or number or literal), or number of entries if object or array
   if (!j)
      return -1;
   if (!j->children && !j->isstring && !j->val)
      return sizeof (valnull) - 1;      // NULL val is literal NULL
   return j->len;
}

const char *
j_get (const j_t j, const char *path)
{                               // Find and get val using path, NULL for not found
   return j_val (j_find (j, path));
}

const char *
j_get_not_null (const j_t j, const char *path)
{                               // Find and get val using path, NULL for not found or null
   j_t f = j_find (j, path);
   if (j_isnull (f))
      return NULL;
   return j_val (f);
}

char
j_test (const j_t jc, const char *path, char def)
{                               // Return 0 for false, 1 for true, else def
   j_t j = j_find (jc, path);
   if (!j_isbool (j))
      return def;
   if (j_istrue (j))
      return 1;
   return 0;
}

// Information about data type of this point
int
j_isarray (const j_t j)
{                               // True if is an array
   return j && j->isarray;
}

int
j_isobject (const j_t j)
{                               // True if is an object
   return j && j->children && !j->isarray;
}

int
j_isnull (const j_t j)
{                               // True if is null literal
   return j && !j->children && !j->isstring && !j->val;
}

int
j_isbool (const j_t j)
{                               // True if is a Boolean literal
   return j && !j->children && !j->isstring && j->val && (*j->val == 't' || *j->val == 'f');
}

int
j_istrue (const j_t j)
{                               // True if is true literal
   return j && !j->children && !j->isstring && j->val && *j->val == 't';
}

int
j_isnumber (const j_t j)
{                               // True if is a number
   return j && !j->children && !j->isstring && j->val && (*j->val == '-' || isdigit (*j->val));
}

int
j_isliteral (const j_t j)
{                               // True if is a literal
   return j && !j->children && !j->isstring;
}

int
j_isstring (const j_t j)
{                               // True if is a string (i.e. quoted, note "123" is a string, 123 is a number)
   return j && j->isstring;
}

char *
j_recv (j_t root, ajl_t p)
{                               // Stream read, empty string on EOF
   if (!root)
      return strdup ("Missing j_t");
   const char *e = ajl_reset (p);
   if (e)
      return strdup (e);
   j_null (root);
   j_t j = NULL;                // Current container, NULL means root is not a container (yet)
   while (1)
   {
      unsigned char *tag = NULL;
      unsigned char *value = NULL;
      size_t len = 0;
      ajl_type_t t = ajl_parse (p, &tag, &value, &len); // We expect logical use of tag and value
      if (t == AJL_EOF)
         e = "";
      if (t <= AJL_EOF)
         break;
      if (t == AJL_CLOSE)
      {                         // End of object or array
         if (j == root)
            break;
         j = j_parent (j);
         continue;
      }
      j_t n = root;
      if (j)
         n = j_add_child (j);
      if (tag)
      {                         // Tag in parent
         freez (n->tag);
         n->tag = tag;
         if (j_findtag (j, tag) != n)
         {                      // Should always find this tag just added, else duplicate
            j = n;
            e = "Duplicate tag";
            break;
         }
      }
      if (value)
      {                         // The value
         if (n->malloc)
            freez (n->val);
         n->malloc = 0;
         n->val = value;
         n->len = len;
         if (!strcmp ((char *) value, (char *) valempty))
            n->val = valempty;
         else if (!strcmp ((char *) value, (char *) valzero))
            n->val = valzero;
         else if (!strcmp ((char *) value, (char *) valtrue))
            n->val = valtrue;
         else if (!strcmp ((char *) value, (char *) valfalse))
            n->val = valfalse;
         else if (t != AJL_STRING && !strcmp ((char *) value, (char *) valnull))
         {
            n->val = NULL;
            n->len = 0;
         } else
            n->malloc = 1;
         if (!n->malloc)
            freez (value);
         if (t == AJL_STRING)
            n->isstring = 1;
      } else if (t == AJL_OBJECT || t == AJL_ARRAY)
      {
         n->children = 1;
         assert ((n->child = realloc (n->child, n->len = 0)));
         if (t == AJL_ARRAY)
            n->isarray = 1;
         j = n;
      } else if (n == root)
         break;
   }
   if (!e)
      e = ajl_error (p);
   char *ret = NULL;
   if (e && !*e)
      ret = strdup (e);         // EOF
   else if (e)
   {                            // report where in object tree we got to
      size_t len;
      FILE *f = open_memstream (&ret, &len);
      int v = ajl_line (p);
      fprintf (f, "Parse failed");
      if (v > 1)
         fprintf (f, " line %d", v);
      v = ajl_char (p);
      if (v)
         fprintf (f, " posn %d", v);
      fprintf (f, ": %s, ", e);
      while (j && j != root)
      {
         if (j->tag)
            fprintf (f, "\"%s\"", j->tag);
         else
            fprintf (f, "[%d]", j->posn);
         j = j_parent (j);
         if (j != root)
            fprintf (f, " in ");
      }
      fclose (f);
   }
   return ret;
}

// Loading an object. This replaces value at the j_t specified, which is usually a root from j_create()
// Returns NULL if all is well, else a malloc'd error string
char *
j_read_ajl (const j_t root, ajl_t p)
{                               // Read object from open file
   assert (root);
   char *e = j_recv (root, p);
   if (!e)
   {
      ajl_skip_ws (p);          // This will also skip any faked trailing whitespace and check for more actual data or eof
      if (ajl_peek (p) >= 0
          && asprintf (&e, "Extra data after object at line %d posn %d [%c]\n", ajl_line (p), ajl_char (p), ajl_peek (p)) < 0)
         errx (1, "malloc");
   }
   ajl_delete (&p);
   if (e && !*e)
      freez (e);                // EOF not an error
   return e;
}

char *
j_read (const j_t root, FILE * f)
{
   return j_read_ajl (root, ajl_read (f));
}

char *
j_read_fd (const j_t root, int f)
{
   return j_read_ajl (root, ajl_read_fd (f));
}

char *
j_read_close (const j_t root, FILE * f)
{                               // Read object from open file
   char *e = j_read (root, f);
   fclose (f);
   return e;
}

char *
j_read_file (const j_t j, const char *filename)
{                               // Read object from named file
   assert (j);
   assert (filename);
   FILE *f = fopen (filename, "r");
   if (!f)
   {
      char *e = NULL;
      assert (asprintf (&e, "Failed to open %s (%s)", filename, strerror (errno)) >= 0);
      return e;
   }
   return j_read_close (j, f);
}

char *
j_read_mem (const j_t root, const char *buffer, ssize_t len)
{                               // Read object from string in memory (NULL terminated)
   return j_read_ajl (root, ajl_read_mem (buffer, len));
}

// Output an object - note this allows output of a raw value, e.g. string or number, if point specified is not an object itself
// Returns NULL if all is well, else a malloc'd error string
static char *
j_write_flags (const j_t root, ajl_t p, char pretty, char aclose)
{
   if (!root)
      return "No JSON";
   if (pretty)
      ajl_pretty (p);
   j_t j = root;
   do
   {
      const unsigned char *tag = j->tag;
      if (j == root)
         tag = NULL;
      if (j->children)
      {                         // Object or array
         if (j->len)
         {                      // Non empty
            if (j->isarray)
               ajl_add_array (p, tag);
            else
               ajl_add_object (p, tag);
            j = j->child[0];    // In to child
            continue;
         }
         // Empty object or array
         ajl_add (p, tag, (const unsigned char *) (j->isarray ? "[]" : "{}"));  // For nicer pretty print
      } else if (j->isstring)
         ajl_add_stringn (p, tag, j->val, j->len);
      else
         ajl_add (p, tag, j->val ? : (unsigned char *) valnull);
      // Next
      j_t n = NULL;
      while (j && j != root && !(n = j_next (j)))
      {
         ajl_add_close (p);
         j = j_parent (j);
      }
      j = n;
   }
   while (j && j != root);
   char *e = (char *) ajl_error (p);
   if (e)
      e = strdup (e);
   if (aclose)
      ajl_delete (&p);
   return e;
}

char *
j_send (const j_t root, ajl_t p)
{
   const char *e = ajl_reset (p);
   if (e)
      return strdup (e);
   char *er = j_write_flags (root, p, 0, 0);
   ajl_flush (p);
   return er;
}

char *
j_write_func (const j_t root, ajl_func_t func, void *arg)
{
   return j_write_flags (root, ajl_write_func (func, arg), 0, 1);
}

char *
j_write (const j_t root, FILE * f)
{
   return j_write_flags (root, ajl_write (f), 0, 1);
}

char *
j_write_fd (const j_t root, int f)
{
   return j_write_flags (root, ajl_write_fd (f), 0, 1);
}

char *
j_write_close (const j_t root, FILE * f)
{
   char *e = j_write_flags (root, ajl_write (f), 0, 1);
   fclose (f);
   return e;
}

char *
j_write_pretty (const j_t root, FILE * f)
{
   return j_write_flags (root, ajl_write (f), 1, 1);
}

char *
j_write_pretty_close (const j_t root, FILE * f)
{
   char *e = j_write_flags (root, ajl_write (f), 1, 1);
   fclose (f);
   return e;
}

char *
j_write_file (const j_t j, const char *filename)
{
   assert (j);
   assert (filename);
   FILE *f = fopen (filename, "w");
   if (!f)
   {
      char *e = NULL;
      assert (asprintf (&e, "Failed to open %s (%s)", filename, strerror (errno)) >= 0);
      return e;
   }
   return j_write_close (j, f);
}

char *
j_write_mem (const j_t j, char **buffer, size_t *len)
{
   assert (j);
   assert (buffer);
   size_t l;
   if (!len)
      len = &l;
   FILE *f = open_memstream (buffer, len);
   char *err = j_write (j, f);
   fclose (f);
   return err;
}

char *
j_write_str (const j_t j)
{
   char *buffer;
   size_t len;
   char *e = j_write_mem (j, &buffer, &len);
   if (e)
   {
      free (e);
      return NULL;
   }
   return buffer;
}

// Changing an object/value
j_t
j_null (const j_t j)
{                               // Null this point - used a lot internally to clear a point before setting to correct type
   if (!j)
      return j;
   if (j->children)
   {                            // Object or array
      int n;
      for (n = 0; n < j->len; n++)
      {
         j_null (j->child[n]);
         freez (j->child[n]->tag);
         freez (j->child[n]);
      }
      freez (j->child);
      j->children = 0;
      j->isarray = 0;
   } else
   {                            // Value
      if (j->malloc)
      {
         freez (j->val);
         j->malloc = 0;
      }
      j->val = NULL;            // Even if not malloc'd
   }
   j->len = 0;
   j->isstring = 0;
   return j;
}

j_t
j_numberstring (const j_t j, const char *val)
{                               // Number if valid, else string;
   if (!j_number_ok (val, NULL))
      return j_literal (j, val);
   return j_string (j, val);
}

j_t
j_string (const j_t j, const char *val)
{                               // Simple set this value to a string (null terminated).
   if (!j)
      return j;
   j_null (j);
   if (!val)
      return j;
   j->val = (void *) strdup (val);
   j->len = strlen (val);
   j->malloc = 1;
   j->isstring = 1;
   return j;
}

j_t
j_stringn (const j_t j, const char *val, size_t len)
{                               // Simple set this value to a string with specified length (allows embedded nulls in string)
   if (!j)
      return j;
   if (len)
      assert (val);
   j_null (j);
   assert ((j->val = malloc (len + 1)));
   memcpy (j->val, val, len);
   j->val[len] = 0;
   j->len = len;
   j->malloc = 1;
   j->isstring = 1;
   return j;
}

static void
j_vstringf (const j_t j, const char *fmt, va_list ap, int isstring)
{
   char *v = NULL;              // Could be referencing self
   assert (vasprintf (&v, fmt, ap) >= 0);
   j_null (j);
   j->val = (unsigned char *) v;
   j->len = strlen ((char *) j->val);
   j->malloc = 1;
   j->isstring = isstring;
   if (!isstring && !strcmp ((char *) j->val, (char *) valnull))
   {                            // NULL is special case
      freez (j->val);
      j->malloc = 0;
      j->len = 0;
   }
}

j_t
j_stringf (const j_t j, const char *fmt, ...)
{                               // Simple set this value to a string, using printf style format
   if (!j)
      return j;
   j_null (j);
   va_list ap;
   va_start (ap, fmt);
   j_vstringf (j, fmt, ap, 1);
   va_end (ap);
   return j;
}

j_t
j_datetime (const j_t j, time_t t)
{
   if (!t)
      return j_null (j);
   char v[26];
   j_format_datetime (t, v);
   return j_string (j, v);
}

j_t
j_literalf (const j_t j, const char *fmt, ...)
{                               // Simple set this value to a number, i.e. unquoted, using printf style format
   if (!j)
      return j;
   j_null (j);
   va_list ap;
   va_start (ap, fmt);
   j_vstringf (j, fmt, ap, 0);
   va_end (ap);
   return j;
}

j_t
j_literal (const j_t j, const char *val)
{                               // Simple set this value to a literal, e.g. "null", "true", "false"
   if (!j)
      return j;
   j_null (j);
   if (val && !strcmp (val, (char *) valempty))
      val = (char *) valempty;
   else if (val && !strcmp (val, (char *) valzero))
      val = (char *) valzero;
   else if (val && !strcmp (val, (char *) valtrue))
      val = (char *) valtrue;
   else if (val && !strcmp (val, (char *) valfalse))
      val = (char *) valfalse;
   else if (val && !strcmp (val, (char *) valnull))
      val = NULL;
   else
   {
      val = strdup (val);
      j->malloc = 1;
   }
   j->val = (void *) val;
   if (val)
      j->len = strlen ((char *) val);
   return j;
}

j_t
j_literal_free (const j_t j, char *val)
{                               // Simple set this value to a literal, e.g. "null", "true", "false" - free arg
   j_t r = j_literal (j, val);
   freez (val);
   return r;
}

j_t
j_object (const j_t j)
{                               // Simple set this value to be an object if not already
   if (!j)
      return j;
   if (!j->children || j->isarray)
      j_null (j);
   if (!j->children)
   {
      assert ((j->child = malloc (j->len = 0)));
      j->children = 1;
   }
   return j;
}

j_t
j_array (const j_t j)
{                               // Simple set this value to be an array if not already
   if (!j)
      return j;
   if (!j->children || !j->isarray)
      j_null (j);
   if (!j->children)
   {
      j->children = 1;
      assert ((j->child = malloc (j->len = 0)));
   }
   j->isarray = 1;
   return j;
}

j_t
j_path (const j_t j, const char *path)
{                               // Find or create a path from j
   return j_findmake (j, path, 1);
}

j_t
j_append (const j_t j)
{                               // Create new point at end of array
   if (!j)
      return j;
   j_array (j);
   return j_add_child (j);
}

static int
j_sort_tag (const void *a, const void *b)
{
   return strcmp ((char *) (*(j_t *) a)->tag, (char *) (*(j_t *) b)->tag);
}

void
j_sort_f (const j_t j, j_sort_func * f, int recurse)
{                               // Apply a recursive sort
   if (!j || !j->children)
      return;
   if (recurse && j->children)
      for (int q = 0; q < j->len; q++)
         j_sort_f (j->child[q], f, 1);
   if ((recurse && j->isarray) || !j->len)
      return;
   qsort (j->child, j->len, sizeof (*j->child), f);
   for (int q = 0; q < j->len; q++)
      j->child[q]->posn = q;
}

void
j_sort (const j_t j)
{                               // Recursive tag sort
   j_sort_f (j, j_sort_tag, 1);
}

j_t
j_make (const j_t j, const char *name)
{
   if (!j)
      return NULL;
   if (!name)
      return j_append (j);
   j_t n = j_findtag (j, (const unsigned char *) name);
   if (!n)
   {
      n = j_add_child (j);
      n->tag = (unsigned char *) strdup (name);
   }
   return n;
}

// Additional functions to combine the above... Returns point for newly added value.

j_t
j_remove (const j_t j, const char *name)
{
   j_t n = j_findtag (j, (const unsigned char *) name);
   if (n)
      j_delete (&n);
   return n;
}

j_t
j_store_array (const j_t j, const char *name)
{                               // Store an array at specified name in object
   return j_array (j_make (j, name));
}

j_t
j_store_null (const j_t j, const char *name)
{                               // Store a null at specified name in an object
   return j_null (j_make (j, name));
}

j_t
j_store_object (const j_t j, const char *name)
{                               // Store an object at specified name in an object
   return j_object (j_make (j, name));
}

j_t
j_store_numberstring (const j_t j, const char *name, const char *val)
{                               // Store a number / string at specified name in an object
   return j_numberstring (j_make (j, name), val);
}

j_t
j_store_string (const j_t j, const char *name, const char *val)
{                               // Store a string at specified name in an object
   return j_string (j_make (j, name), val);
}

j_t
j_store_stringn (const j_t j, const char *name, const char *val, int len)
{                               // Store a string at specified name in an object
   return j_stringn (j_make (j, name), val, len);
}

j_t
j_store_stringf (const j_t j, const char *name, const char *fmt, ...)
{                               // Store a (formatted) string at specified name in an object
   if (!j)
      return NULL;
   va_list ap;
   va_start (ap, fmt);
   j_vstringf (j_make (j, name), fmt, ap, 1);
   va_end (ap);
   return j;
}

j_t
j_store_datetime (const j_t j, const char *name, time_t t)
{                               // Store a localtime string at a specified name in an object
   return j_datetime (j_make (j, name), t);
}

j_t
j_store_literalf (const j_t j, const char *name, const char *fmt, ...)
{                               // Store a formatted literal, normally for numbers, at a specified name in an object
   if (!j)
      return NULL;
   va_list ap;
   va_start (ap, fmt);
   j_vstringf (j_make (j, name), fmt, ap, 0);
   va_end (ap);
   return j;
}

j_t
j_store_literal (const j_t j, const char *name, const char *val)
{                               // Store a literal (typically for true/false) at a specified name in an object
   return j_literal (j_make (j, name), val);
}

j_t
j_store_literal_free (const j_t j, const char *name, char *val)
{                               // Store a literal (typically for true/false) at a specified name in an object, freeing the value
   j_t r = j_store_literal (j, name, val);
   freez (val);
   return r;
}

j_t
j_store_json (const j_t j, const char *name, j_t * vp)
{                               // Add a complete JSON entry, freeing second argument, returns first
   return j_replace (j_make (j, name), vp);
}

// Additional functions to combine the above... Returns point for newly added value.
j_t
j_append_null (const j_t j)
{                               // Append a new null to an array
   return j_null (j_append (j));
}

j_t
j_extend (const j_t j, int len)
{
   if (!j)
      return NULL;
   j_array (j);
   while (len > j_len (j))
      j_append_null (j);
   return j;
}

j_t
j_append_object (const j_t j)
{                               // Append a new (empty) object to an array
   return j_object (j_append (j));
}

j_t
j_append_array (const j_t j)
{                               // Append a new (empty) array to an array
   return j_array (j_append (j));
}

j_t
j_append_numberstring (const j_t j, const char *val)
{                               // Append a new number / string to an array
   return j_numberstring (j_append (j), val);
}

j_t
j_append_string (const j_t j, const char *val)
{                               // Append a new string to an array
   return j_string (j_append (j), val);
}

j_t
j_append_stringn (const j_t j, const char *val, int len)
{                               // Append a new string to an array
   return j_stringn (j_append (j), val, len);
}

j_t
j_append_stringf (const j_t j, const char *fmt, ...)
{                               // Append a new (formatted) string to an array
   j_t r = j_append (j);
   va_list ap;
   va_start (ap, fmt);
   j_vstringf (r, fmt, ap, 1);
   va_end (ap);
   return r;
}

j_t
j_append_datetime (const j_t j, time_t t)
{                               // Append a local time string to an array
   return j_datetime (j_append (j), t);
}

j_t
j_append_literalf (const j_t j, const char *fmt, ...)
{                               // Append a formatted literal (usually a number) to an array
   j_t r = j_append (j);
   va_list ap;
   va_start (ap, fmt);
   j_vstringf (r, fmt, ap, 0);
   va_end (ap);
   return r;
}

j_t
j_append_literal (const j_t j, const char *val)
{                               // Append a literal (usually true/false) to an array
   return j_literal (j_append (j), val);
}

j_t
j_append_literal_free (const j_t j, char *val)
{                               // Append a literal and free it
   j_t r = j_append_literal (j, val);
   freez (val);
   return r;
}

j_t
j_append_json (const j_t j, j_t * vp)
{                               // Append a complete JSON entry, freeing second argument, returns first
   return j_replace (j_append (j), vp);
}

// Moving parts of objects...
j_t
j_detach (j_t j)
{                               // Detach from parent so a to make a top level object in itself
   if (!j)
      return j;
   j_unlink (j);                // Unlink from parent, but otherwise leave intact
   return j;
}

j_t
j_replace (const j_t j, j_t * op)
{                               //            Overwrites j in situ with o, freeing and nulling the pointer at *op, and returning j
   if (!j)
   {
      j_delete (op);
      return j;
   }
   if (!op || !*op)
      return j_null (j);
   j_t o = *op;
   j_unlink (o);                // Unlink from parent
   j_null (j);                  // Done after unlink as may be replacing with a child
   j->children = o->children;
   j->isarray = o->isarray;
   if (j->children)
      j->child = o->child;      // Copy over the key components
   else
      j->val = o->val;
   j->isstring = o->isstring;
   j->len = o->len;
   j->malloc = o->malloc;
   // Don't copy parent, tag, and posn, as these apply to j still as it may be in a tree
   freez (*op);                 // Can safely free original as malloced children/val have been moved
   if (j->children)
      for (int c = 0; c < j->len; c++)
         j->child[c]->parent = j;       // Link parent
   return j;
}

const char *
j_string_ok (const char *n, const char **end)
{                               // Return is const fixed string if error
   if (!n)
      return "NULL pointer for string";
   ajl_t j = ajl_text (n);
   const char *err = ajl_string (j, NULL);
   const char *e = ajl_done (&j);
   if (end)
      *end = e;
   else if (*e)
      return "Extra on end of string";
   return err;
}

const char *
j_datetime_ok (const char *n, const char **end)
{                               // Checks if string is valid datetime, return error description if not
   if (!n)
      return "NULL pointer for datetime";
   int y,
     m,
     d,
     H,
     M,
     S;
   if (!isdigit (n[0]) || !isdigit (n[1]) || !isdigit (n[2]) || !isdigit (n[3]))
      return "No year";
   y = (n[0] - '0') * 1000 + (n[1] - '0') * 100 + (n[2] - '0') * 10 + n[3] - '0';
   n += 4;
   if (*n++ != '-')
      return "No - after year";
   if (!isdigit (n[0]) || !isdigit (n[1]))
      return "No month";
   m = (n[0] - '0') * 10 + n[1] - '0';
   n += 2;
   if (*n++ != '-')
      return "No - after month";
   if (!isdigit (n[0]) || !isdigit (n[1]))
      return "No day";
   d = (n[0] - '0') * 10 + n[1] - '0';
   n += 2;
   if (!*n || (*n != 'T' && *n != ' '))
   {
      if (!(!y && !m && !d) && (m < 1 || m > 12 || d < 1 || d > 31))
         return "Bad date";
      if (end)
         *end = n;
      else if (*n)
         return "Extra on end of date";
      return NULL;              // OK date
   }
   n++;                         // T/space
   if (!isdigit (n[0]) || !isdigit (n[1]))
      return "No hour";
   H = (n[0] - '0') * 10 + n[1] - '0';
   n += 2;
   if (*n++ != ':')
      return "No : after hour";
   if (!isdigit (n[0]) || !isdigit (n[1]))
      return "No minute";
   M = (n[0] - '0') * 10 + n[1] - '0';
   n += 2;
   if (*n++ != ':')
      return "No : after minute";
   if (!isdigit (n[0]) || !isdigit (n[1]))
      return "No second";
   S = (n[0] - '0') * 10 + n[1] - '0';
   n += 2;
   if (!(H == 24 && M == 60 && M < 62) && (H >= 24 || M >= 60 || S >= 60))
      return "Bad time";
   if (*n == 'Z')
      n++;
   else if (*n == '+' || *n == '-')
   {
      H = M = 0;
      char s = *n++;
      if (!isdigit (n[0]) || !isdigit (n[1]))
         return "Bad hours timezone";
      H = (n[0] - '0') * 10 + n[1] - '0';
      n += 2;
      if (*n)
      {                         // Minutes
         if (*n == ':')
            n++;
         if (!isdigit (n[0]) || !isdigit (n[1]))
            return "Bad minutes timezone";
         M = (n[0] - '0') * 10 + n[1] - '0';
         n += 2;
      }
      if (H > 27 || M >= 60 || (s == '-' && !H && !M))
         return "Bad timezone";
   }
   if (end)
      *end = n;
   else if (*n)
      return "Extra on end of datetime";
   return NULL;
}

const char *
j_number_ok (const char *n, const char **end)
{                               // Checks if a valid JSON number, returns error description if not
   if (!n)
      return "NULL pointer for number";
   ajl_t j = ajl_text (n);
   const char *err = ajl_number (j, NULL);
   const char *e = ajl_done (&j);
   if (end)
      *end = e;
   else if (*e)
      return "Extra on end of number";
   return err;
}

const char *
j_literal_ok (const char *n, const char **end)
{                               // Checks if a valid JSON literal (true/false/null/number), returns error description if not
   if (!n)
      return "NULL pointer for literal";
   if (!strncmp (n, "true", 4))
      n += 4;
   else if (!strncmp (n, "false", 5))
      n += 5;
   else if (!strncmp (n, "null", 4))
      n += 4;
   else
      return j_number_ok (n, end);      // Check number
   if (*n && isalnum (*n))
      return "Extra after literal";
   if (end)
      *end = n;
   else if (*n)
      return "Extra text on end of literal";
   return NULL;                 // OK
}

void
j_log (int debug, const char *who, const char *what, j_t a, j_t b)
{                               // Generate log files
   if (!a && !b)
      return;
   char *norm (char *x)
   {
      char *p;
      for (p = x; *p; p++)
         if (!isalnum (*p))
            *p = '_';
      return x;
   }
   umask (0);
   char path[1000] = "",
      *p = path;
   struct timeval tv;
   struct timezone tz;
   gettimeofday (&tv, &tz);
   p += strftime (path, sizeof (path) - 1, "/var/log/json/%Y/%m/%d/%H%M%S", gmtime (&tv.tv_sec));
   p +=
      snprintf (p, path + sizeof (path) - p, "%06u-%s-%s", (unsigned int) tv.tv_usec, norm (strdupa (who ? : "-")),
                norm (strdupa (what ? : "-")));
   for (char *q = path + 13; q; q = strchr (q + 1, '/'))
   {
      *q = 0;
      if (access (path, W_OK) && mkdir (path, 0777) && access (path, W_OK))
         warnx ("Cannot make log path %s", path);
      *q = '/';
   }
   if (a)
   {
      *p = 0;
      if (a && b)
         sprintf (p, ".a");
      char *er = j_write_file (a, path);
      if (er)
         warnx ("Failed: %s", er);
      if (debug)
         j_err (j_write_pretty (a, stderr));
   }
   if (b)
   {
      *p = 0;
      if (a && b)
         sprintf (p, ".b");
      char *er = j_write_file (b, path);
      if (er)
         warnx ("Failed: %s", er);
      if (debug)
         j_err (j_write_pretty (b, stderr));
   }
}

#define	FORMDIV "CUT-HERE-AJL-LIBRARY-POST-CUT-HERE"
char *
j_multipart (j_t j, size_t *lenp)
{
   if (!j)
      return NULL;
   size_t len;
   char *data;
   FILE *f = open_memstream (&data, &len);
   void quote (const char *v)
   {
      fputc ('"', f);
      while (*v)
      {
         if (isalnum (*v))
            fputc (*v, f);
         else
            fprintf (f, "%%%02X", *v);
         v++;
      }
      fputc ('"', f);
   }
   void add (const char *name, j_t j)
   {
      fprintf (f, "--" FORMDIV "\r\n");
      fprintf (f, "Content-Disposition: form-data");
      if (name)
      {
         fprintf (f, "; name=");
         quote (name);
      }
      if (j_isstring (j) || j_isliteral (j))
         fprintf (f, "\r\n\r\n%s", j_val (j));
      else if (j_isobject (j))
      {
         j_t v = j_find (j, "filename") ? : j_find (j, "file");
         if (j_isstring (v))
         {
            fprintf (f, "; filename=");
            quote (j_val (v));
         }
         fprintf (f, "\r\n");
         v = j_find (j, "type");
         if (j_isstring (v))
            fprintf (f, "Content-Type: %s\r\n", j_val (v));
         fprintf (f, "\r\n");
         v = j_find (j, "value");
         if (j_isstring (v) || j_isliteral (v))
            fprintf (f, "%s", j_val (v));
         else if (v)
            j_err (j_write (v, f));
         else if ((v = j_find (j, "file")) && j_isstring (v))
         {
            FILE *i = fopen (j_val (v), "r");
            if (i)
            {
               char buf[10240];
               size_t l;
               while ((l = fread (buf, 1, sizeof (buf), i)) > 0)
                  fwrite (buf, l, 1, f);
               fclose (i);
            }
         }
      } else
         fprintf (f, "\r\n\r\n");       // No clue
      fprintf (f, "\r\n");
   }
   if (j_isobject (j))
      for (j_t a = j_first (j); a; a = j_next (a))
         add (j_name (a), a);
   else if (j_isarray (j))
      for (j_t a = j_first (j); a; a = j_next (a))
         add (NULL, a);
   else if (!j_isnull (j))
      add (NULL, j);
   fprintf (f, "--" FORMDIV "--\r\n");
   fclose (f);
   if (lenp)
      *lenp = len;
   return data;
}

char *
j_formdata (j_t j)
{
   if (!j)
      return NULL;
   int n = 0;
   size_t len;
   char *data;
   FILE *f = open_memstream (&data, &len);
   void add (const char *s, int l, char bin)
   {
      if (!s)
         return;
      if (l < 0)
         l = strlen (s);
      while (l--)
      {
         if (bin)
            fprintf (f, "%%%02X", (unsigned char) *s);
         else if (*s == ' ')
            fprintf (f, "+");
         else if (!isalpha (*s) && !isdigit (*s) && !strchr ("-._~", *s))       // RFC 3986 2.3 unreserved characters.
            fprintf (f, "%%%02X", (unsigned char) *s);
         else
            fputc (*s, f);
         s++;
      }
   }
   void addv (j_t j)
   {
      if (j_isnull (j))
         return;
      fputc ('=', f);
      j_t b = NULL;
      if (j_isobject (j) && j_len (j) == 1 && (b = j_find (j, "binary")))
         add (j_val (b), j_len (b), 1);
      else if (j_isarray (j) || j_isobject (j))
      {                         // Expand
         size_t len;
         char *buf;
         const char *err = j_write_mem (j, &buf, &len);
         if (err)
            errx (1, "WTF formdata: %s", err);
         add (buf, len, 0);
         free (buf);
      } else
         add (j_val (j), j_len (j), 0);
   }
   if (j_isobject (j))
      for (j_t a = j_first (j); a; a = j_next (a))
      {
         if (n++)
            fputc ('&', f);
         add (j_name (a), -1, 0);
         addv (a);
   } else if (j_isarray (j))
      for (j_t a = j_first (j); a; a = j_next (a))
      {
         if (n++)
            fputc ('&', f);
         fprintf (f, "[%d]", j_pos (a));
         addv (a);
   } else if (!j_isnull (j))
      addv (j);
   fclose (f);
   return data;
}

#ifdef	JCURL
char *
j_curl (int type, CURL * curlv, j_t tx, j_t rx, const char *bearer, const char *url, ...)
{                               // Curl... can get, post form, or post JSON
   CURL *curl = curlv;
   if (!curl)
      curl = curl_easy_init ();
   char *reply = NULL;
   size_t replylen = 0;
   FILE *o = open_memstream (&reply, &replylen);
   char *fullurl = NULL;
   va_list ap;
   va_start (ap, url);
   if (vasprintf (&fullurl, url, ap) < 0)
      errx (1, "malloc at line %d", __LINE__);
   va_end (ap);
   struct curl_slist *headers = NULL;
   char *data = NULL;
   size_t datalen = 0;
   char *err = NULL;
   if (type && type != 4 && !tx)
   {
      freez (fullurl);
      return j_errs ("No JSON to send (%s)", url);
   }
   curl_easy_setopt (curl, CURLOPT_HTTPGET, 0L);
   curl_easy_setopt (curl, CURLOPT_POST, 0L);
   curl_easy_setopt (curl, CURLOPT_UPLOAD, 0L);
   switch (type)
   {
   case J_CURL_GET:            // GET using URL coded
      {
         curl_easy_setopt (curl, CURLOPT_HTTPGET, 1L);
         if (!tx)
            break;
         char *formdata = j_formdata (tx);
         if (!formdata)
            err = j_errs ("Failed to make formdata");
         else
         {
            char *u = fullurl;
            if (asprintf (&fullurl, "%s?%s", u, formdata) < 0)
               errx (1, "malloc");
            free (u);
            freez (formdata);
         }
      }
      break;
   case J_CURL_POST:           // POST using URL coded
      {
         curl_easy_setopt (curl, CURLOPT_POST, 1L);
         if (!tx)
            break;
         data = j_formdata (tx);
         if (!data)
            err = j_errs ("Failed to make formdata");
         else
            datalen = strlen (data);
      }
      break;
   case J_CURL_FORM:           // POST using multipart/form-data
      {
         headers = curl_slist_append (headers, "Content-Type: multipart/form-data; boundary=" FORMDIV);
         curl_easy_setopt (curl, CURLOPT_POST, 1L);
         if (!tx)
            break;
         data = j_multipart (tx, &datalen);
         if (!data)
            err = j_errs ("Failed to make formdata");
      }
      break;
   case J_CURL_SEND:           // POST JSON
      {
         curl_easy_setopt (curl, CURLOPT_POST, 1L);
         if (!tx)
            break;
         headers = curl_slist_append (headers, "Content-Type: application/json");       // posting JSON
         err = j_write_mem (tx, &data, &datalen);
      }
      break;
   case J_CURL_PUT:            // PUT JSON
      {
         //curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
         curl_easy_setopt (curl, CURLOPT_CUSTOMREQUEST, "PUT");
         if (!tx)
            break;
         headers = curl_slist_append (headers, "Content-Type: application/json");       // posting JSON
         err = j_write_mem (tx, &data, &datalen);
      }
      break;
   case J_CURL_DELETE:         // DELETE JSON
      {
         curl_easy_setopt (curl, CURLOPT_CUSTOMREQUEST, "DELETE");
         if (!tx)
            break;
         headers = curl_slist_append (headers, "Content-Type: application/json");       // posting JSON
         if (tx)
            err = j_write_mem (tx, &data, &datalen);
      }
      break;
   }
   if (!err && data && datalen)
   {
      curl_easy_setopt (curl, CURLOPT_POSTFIELDS, data);
      curl_easy_setopt (curl, CURLOPT_POSTFIELDSIZE, (long) datalen);
   }
   if (!err && bearer)
   {                            // Bearer auth (a common auth for JSON)
      char *sa = NULL;
      if (asprintf (&sa, "Authorization: Bearer %s", bearer) < 0)
         errx (1, "malloc at line %d", __LINE__);
      headers = curl_slist_append (headers, sa);
      freez (sa);
   }
   if (headers)
      curl_easy_setopt (curl, CURLOPT_HTTPHEADER, headers);
   curl_easy_setopt (curl, CURLOPT_URL, fullurl);
   curl_easy_setopt (curl, CURLOPT_WRITEDATA, o);
   j_null (rx);
   CURLcode result = 0;
   if (!err)
      result = curl_easy_perform (curl);
   fclose (o);
   freez (data);
   // Put back to GET as default
   curl_easy_setopt (curl, CURLOPT_HTTPHEADER, NULL);
   curl_easy_setopt (curl, CURLOPT_HTTPGET, 1L);
   if (headers)
      curl_slist_free_all (headers);
   long code = 0;
   if (!result)
      curl_easy_getinfo (curl, CURLINFO_RESPONSE_CODE, &code);
   if (!err && (code / 100) != 2)
   {
      char *s = strchr (fullurl, '?');
      if (s)
         *s = 0;                // Sanitisee
      if (result)
         err = j_errs ("Failed (%s) curl result %d: %s", fullurl, result, curl_easy_strerror (result));
      else
         err = j_errs ("Failed (%s) http return code %d", fullurl, code);
   }
   freez (fullurl);
   if (!result && reply && replylen && rx)
   {
      char *e = j_read_mem (rx, reply, replylen);
      if (e)
      {
         char *ct = NULL;
         if (curl_easy_getinfo (curl, CURLINFO_CONTENT_TYPE, &ct))
            ct = NULL;
         if (ct && !strcasecmp (ct, "application/json"))
         {
            if (err)
               err = j_errs ("%s: %s", err, e);
            else
               err = j_errs ("Failed to parse JSON: %s", e);
         } else
         {
            free (e);
            j_stringn (rx, reply, replylen);    // Return as a JSON string
         }
      }
   }
   freez (reply);
   if (!curlv)
      curl_easy_cleanup (curl);
   return err;
}
#endif

#ifndef	LIB                     // Build as command line for testing
#include <popt.h>
int
main (int __attribute__((unused)) argc, const char __attribute__((unused)) * argv[])
{
   int debug = 0;
   int pretty = 0;
   int formdata = 0;
   const char *doget = NULL;
   const char *dopost = NULL;
   const char *dosend = NULL;
   const char *doform = NULL;
   const char *doput = NULL;
   const char *dodelete = NULL;
   const char *test = NULL;
   poptContext optCon;          // context for parsing command-line options
   {                            // POPT
      const struct poptOption optionsTable[] = {
         {"pretty", 'p', POPT_ARG_NONE, &pretty, 0, "Output pretty", NULL},
         {"formdata", 'f', POPT_ARG_NONE, &formdata, 0, "Output as formdata", NULL},
         {"get", 'G', POPT_ARG_STRING, &doget, 0, "Curl GET URL", "URL"},
         {"post", 'P', POPT_ARG_STRING, &dopost, 0, "Curl POST URL", "URL"},
         {"form", 'P', POPT_ARG_STRING, &doform, 0, "Curl POST formdata", "URL"},
         {"put", 'P', POPT_ARG_STRING, &doput, 0, "Curl PUT JSON", "URL"},
         {"delete", 'P', POPT_ARG_STRING, &dodelete, 0, "Curl DELETE", "URL"},
         {"send", 'S', POPT_ARG_STRING, &dosend, 0, "Curl POST JSON", "URL"},
         {"test", 'T', POPT_ARG_STRING, &test, 0, "Run test on OK functions", "string"},
         {"debug", 'v', POPT_ARG_NONE, &debug, 0, "Debug", NULL},
         POPT_AUTOHELP {NULL, 0, 0, NULL, 0, NULL, NULL}
      };
      optCon = poptGetContext (NULL, argc, argv, optionsTable, 0);
      poptSetOtherOptionHelp (optCon, "[filename]");
      int c;
      if ((c = poptGetNextOpt (optCon)) < -1)
         errx (1, "%s: %s\n", poptBadOption (optCon, POPT_BADOPTION_NOALIAS), poptStrerror (c));
      CURL *curl = curl_easy_init ();
      if (debug)
         curl_easy_setopt (curl, CURLOPT_VERBOSE, 1L);
      void process (const char *fn)
      {
         j_t j = j_create ();
         char *e;
         if (strcmp (fn, "-"))
            e = j_read_file (j, fn);
         else
            e = j_read (j, stdin);
         if (e)
            fprintf (stderr, "%s: %s\n", fn, e);
         if (doget || dopost)
         {
            char *f = j_formdata (j);
            printf ("Coded:\n%s\n", f);
            free (f);
         }
         if (doform)
         {
            size_t l;
            char *f = j_multipart (j, &l);
            printf ("Coded:\n");
            fwrite (f, 1, l, stdout);
            free (f);
         }
         if (doget)
            j_curl_get (curl, j, j, NULL, "%s", doget);
         else if (dopost)
            j_curl_post (curl, j, j, NULL, "%s", dopost);
         else if (dosend)
            j_curl_send (curl, j, j, NULL, "%s", dosend);
         else if (doform)
            j_curl_form (curl, j, j, NULL, "%s", doform);
         else if (doput)
            j_curl_put (curl, j, j, NULL, "%s", doput);
         else if (dodelete)
            j_curl_delete (curl, j, j, NULL, "%s", dodelete);
         if (formdata)
         {
            char *f = j_formdata (j);
            printf ("Reply:\n%s\n", f);
            free (f);
         } else if (pretty)
            j_err (j_write_pretty (j, stdout));
         else
         {
            j_err (j_write (j, stdout));
            printf ("\n");
         }
         if (debug)
         {
            j_t f = j_find (j, "test");
            if (f)
               warnx ("test: %s len=%d val=%s isnull=%d", j_val (f), f->len, f->val, j_isnull (f));
         }
         //warnx("isnull=%d",j_isnull(j_find(j,"test")));
         j_delete (&j);
      }

      if (test)
      {
         const char *e = NULL;
         const char *er = j_string_ok (test, &e);
         if (er)
            printf ("String: %s\n", er);
         else if (!e)
            printf ("String: no end\n");
         else
            printf ("String: OK [%s]\n", e);
         er = j_number_ok (test, &e);
         if (er)
            printf ("Number: %s\n", er);
         else if (!e)
            printf ("Number: no end\n");
         else
            printf ("Number: OK [%s]\n", e);
         er = j_literal_ok (test, &e);
         if (er)
            printf ("Literal: %s\n", er);
         else if (!e)
            printf ("Literal: no end\n");
         else
            printf ("Literal: OK [%s]\n", e);
         er = j_datetime_ok (test, &e);
         if (er)
            printf ("Datetime: %s\n", er);
         else if (!e)
            printf ("Datetime: no end\n");
         else
            printf ("Datetime: OK [%s]\n", e);
         j_t j = j_create ();
         er = j_read_mem (j, test, -1);
         if (er)
            printf ("JSON: %s\n", er);
         else
            j_err (j_write_pretty (j, stdout));
         j_delete (&j);
      }

      const char *v;
      if (!poptPeekArg (optCon))
      {
         if (!test)
            process ("-");
      } else
         while ((v = poptGetArg (optCon)))
            process (v);
      curl_easy_cleanup (curl);
   }
   poptFreeContext (optCon);
   return 0;
}
#endif
