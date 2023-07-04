// ==========================================================================
//
//                       Adrian's XML Library - axl.c
//
// ==========================================================================

    /*
       Copyright (C) 2008-2017  RevK and Andrews & Arnold Ltd

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

#ifndef	_GNU_SOURCE
#define	_GNU_SOURCE
#endif

//#define       PARSEDEBUG
//#define       CPP             // CPP processing used by FireBrick stuff
//#define       CURLCB          // CURL CB function

// TODO change to allow XML comments, and interleaved text elements
// TODO document that carefully
// TODO C14N formatting
// TODO Clear logic for element with blank name being an intermix text element maybe? parse and write (what of json?)
// TODO Clear logic for element name "!--" being comment

/* Suggested vim command to fix from last version :-

:g/xml_element_t/s//xml_t/g
:g/xml_tree_t/s//xml_t/g
:g/xml_tree_write/s//xml_write/g
:g/xml_tree_root (\([a-z0-9]*\))/s//\1/g
:g/xml_tree (\([a-z0-9]*\))/s//\1/g
:g/xml_tree_new ()/s//xml_tree_new(NULL)/g

*/

/* notes on non expat
   Parse tags <...> and non tags to buffer
   Non tag ends at end or <
   Tag ends at > but check for comment and cdata and so on
   Ending with partial tag is error
   Check & syntax as we go for line number on errors be careful of cdata
   The tag can be checked at end of tag if we can't check as we go
   Compress & in content before passing on
   Compress & in tag after splitting and quotes, etc
   Is & valid in names? Maybe.
   */

#ifdef	EXPAT
#include <expat.h>
#endif

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <err.h>
#include <ctype.h>
#include <stdarg.h>
#include <time.h>
#include <curl/curl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "axl.h"

const char BASE64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
const char BASE32[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
const char BASE16[] = "0123456789ABCDEF";

#ifndef	strdupa
#define	strdupa(s)	strcpy(alloca(strlen(s)+1),s)
#endif

#ifndef	strndupa
#define	strndupa(s,n)	strcpy(alloca((strlen(s)>((size_t)(n))?((size_t)(n)):strlen(s))+1),s)
#endif

const char empty[1] = "";

static struct xml_namespace_s nullns = { 0 };

static void *xml_alloc(size_t s)
{
   void *m = malloc(s);
   if (!m)
      errx(1, "Malloc failed, %d bytes", (int) s);
   memset(m, 0, s);
   return m;
}

static void *xml_free(void *s)
{
   if (s && s != empty)
      free(s);
   return NULL;
}

static int strcmp_l(int al, const char *a, int bl, const char *b)
{
   if (!al && !bl)
      return 0;
   if (!a || al < bl)
      return -1;
   if (!b || bl > al)
      return 1;
   if (!al)
      return 0;
   return memcmp(a, b, al);
}

static char *xml_dup_l(int l, const char *s)
{
   if (!s)
      return NULL;
   if (!l)
      return (char *) empty;
   char *m = xml_alloc(l + 1);
   memmove(m, s, l);
   return m;
}

static char *xml_dup(const char *s)
{
   if (!s)
      return NULL;
   return xml_dup_l(strlen(s), s);
}

// This was added for saving element filenames, but could be used more generally
static char *savestring(const char *s, int length, xml_root_t t)
{
   xml_stringlist_t p;
   if (!s)
      return NULL;
   if (!*s)
      return (char *) empty;
   for (p = t->strings; p && (p->length != length || strncmp(p->string, s, length)); p = p->next);
   if (p)
      return p->string;
   p = xml_alloc(sizeof(*p));
   p->string = xml_alloc(length + 1);
   p->length = length;
   memmove(p->string, s, length);
   p->next = t->strings;
   t->strings = p;
   return p->string;
}

#ifdef	EXPAT
typedef struct xml_namespacestack_s {
   struct xml_namespacestack_s *prev;
   xml_namespacelist_t ns;
   xml_t base;
} *xml_namespacestack_t;

typedef struct xml_parser_s {
   xml_root_t tree;             // Tree
   xml_t here;                  // Where we are in parsing
   xml_namespacestack_t namespacestack;
   const char *current_file;
   xml_callback_t *callback;    // called at end of each top level element
   int line_offset;
   XML_Parser parser;
} xml_parser_t;
#endif

#ifndef	EXPAT
typedef void xml_error_func(const char *filename, int line, int character, const char *error, const unsigned char *posn);

static xml_t xml_parse(const char *filename, const unsigned char *xml, xml_error_func * fail)
{                               // Parse null terminated XML data
   // https://www.w3.org/TR/xml/#NT-document
   xml_t root = xml_tree_new(NULL);
   struct {                     // Name space stack during parse
      int tagl;
      const unsigned char *tag;
      xml_namespace_t namespace;
   } *nss = NULL;
   int nssp = 0,
       nssn = 0;
   void ns_stack(int tagl, const unsigned char *tag, int namespacel, char *namespace) { // Add to stack
      if (nssp + 1 > nssn)
         nss = realloc(nss, sizeof(*nss) * (++nssn));
      nss[nssp].tagl = tagl;
      nss[nssp].tag = tag;
      nss[nssp].namespace = xml_namespace_l(root, tagl, (const char *) tag, namespacel, (const char *) namespace);
      nssp++;
   }
   ns_stack(3, (unsigned char *) "xml", 36, "http://www.w3.org/XML/1998/namespace");
   xml_namespace_t ns_find(int tagl, const unsigned char *tag) {        // Find namespace from stack
      int p = nssp;
      while (p--)
         if (!strcmp_l(nss[p].tagl, (const char *) nss[p].tag, tagl, (const char *) tag))
            return nss[p].namespace;
      return NULL;
   }
   const unsigned char *p = xml,
       *posn = xml;
   int line = 0,
       character = 0;
   const char *er = NULL;       // Error
   if (!p)
   {
      p = (const unsigned char *) "";
      er = "NULL file";
   };
   unsigned int utf8(void) {    // return next letter as UTF8
      if (p[0] < 0x80)
         return p[0];
      if (p[0] >= 0xC2 && p[0] <= 0xDF && p[1] >= 0x80 && p[1] < 0xC0)
         return ((p[0] & 0x1F) << 6) + (p[1] & 0x3F);
      if (p[0] >= 0xE0 && p[0] <= 0xEF && p[1] >= 0x80 && p[1] < 0xC0 && p[2] >= 0x80 && p[2] < 0xC0)
         return ((p[0] & 0xF) << 12) + ((p[1] & 0x3F) << 6) + (p[2] & 0x3F);
      if (p[0] >= 0xF0 && p[0] <= 0xF7 && p[1] >= 0x80 && p[1] < 0xC0 && p[2] >= 0x80 && p[2] < 0xC0 && p[3] >= 0x80 && p[3] < 0xC0)
         return ((p[0] & 0x7) << 18) + ((p[1] & 0x3F) << 12) + ((p[2] & 0x3F) << 6) + (p[3] & 0x3F);
      return 0xFFFD;            // Bad
   }
   void write_utf8(FILE * o, unsigned int c) {
      if (c < 0x80)
         fputc(c, o);
      else if (c < 0x800)
      {
         fputc(0xC0 + (c >> 6), o);
         fputc(0x80 + (c & 0x3F), o);
      } else if (c < 0x10000)
      {
         fputc(0xE0 + (c >> 12), o);
         fputc(0x80 + ((c >> 6) & 0x3F), o);
         fputc(0x80 + (c & 0x3F), o);
      } else if (c < 0x200000)
      {
         fputc(0xF0 + (c >> 18), o);
         fputc(0x80 + ((c >> 12) & 0x3F), o);
         fputc(0x80 + ((c >> 6) & 0x3F), o);
         fputc(0x80 + (c & 0x3F), o);
      } else
         er = "Bad UTF-8 write";
   }
   void next(int n) {           // Move on p, but account for line, character, and so on - keeps going until *p==0 even after error but does not count line/char/posn once er set
      while (n-- && *p)
      {
         if (!er)
         {                      // Record position...
            posn = p;
            if (*p == '\n')
            {
               line++;
               character = 0;
            }
            character++;
#ifdef	CPP
            // Handle CPP lines as special cases
#error CPP not coded
#endif
         }
         // Move on
         if (*p < 0x80)
            p++;                // ASCII
         else if (*p < 0xC0)
            p++;                // Bad UTF-8
         else
         {                      // Normal UTF8
            if (!utf8())
               p++;             // Bad UTF8, cannot be 0 so cannot be valid
            else if (*p >= 0xF0)
               p += 4;          // UTF-8 sizes
            else if (*p >= 0xE0)
               p += 3;          // UTF-8 sizes
            else
               p += 2;          // UTF-8 sizes
         }
      }
   }
   inline int iss(unsigned char c) {    // Is "S" (i.e. whitespace)
      if (c == ' ' || c == 9 || c == 10 || c == 13)
         return c;
      return 0;
   }
   inline unsigned int isnamestartchar(unsigned int c) {        // https://www.w3.org/TR/xml/#NT-NameStartChar
      if (c == ':' || c == '_' || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= 0xC0 && c <= 0xD6)
          || (c >= 0xF8 && c <= 0x2FF) || (c >= 0x370 && c <= 0x37D) || (c >= 0x37F && c <= 0x1FFF) || (c >= 0x200C && c <= 0x200D)
          || (c >= 0x2070 && c <= 0x218F) || (c >= 0x2C00 && c <= 0x2FEF) || (c >= 0x3001 && c <= 0xD7FF) || (c >= 0xF900 && c <= 0xFDCF) || (c >= 0xFDF0 && c <= 0xFFFD) || (c >= 0x10000 && c <= 0xEFFFF))
         return c;
      return 0;
   }
   inline unsigned int isnamechar(unsigned int c) {     // https://www.w3.org/TR/xml/#NT-NameChar
      if (isnamestartchar(c) || c == '-' || c == '.' || (c >= '0' && c <= '9') || c == 0xB7 || (c >= 0x300 && c <= 0x36F) || (c >= 0x203F && c <= 0x2040))
         return c;
      return 0;
   }
   inline unsigned int isnames(unsigned int c) {        // https://www.w3.org/TR/xml/#NT-Names
      if (isnamechar(c) || c == ' ')
         return c;
      return 0;
   }
   const unsigned char *parse_name(void) {      // Parse name, NULL if not a valid name
      const unsigned char *start = p;
      if (!isnamestartchar(utf8()))
         return NULL;           // Not a name
      next(1);
      while (isnamechar(utf8()))
         next(1);
      return start;
   }
   int parse_pi(xml_t e) {      // Parse a PI
      if (er)
         return 0;
      if (*p != '<' || p[1] != '?')
         return 0;
      next(2);
      const unsigned char *name = parse_name();
      if (!name)
      {
         er = "Bad name in PI";
         return 0;
      }
      int namel = p - name;
      while (iss(*p))
         next(1);
      const unsigned char *content = p;
      while (*p && (*p != '?' || p[1] != '>'))
         next(1);
      if (*p)
      {                         // found end
         xml_pi_add_l(e, namel, (const char *) name, p - content, (const char *) content);
         next(2);
         return 1;
      }
      er = "Bad PI";
      return 0;
   }
   int parse_comment(xml_t e) { // Parse a comment
      e = e;
      if (er)
         return 0;
      if (memcmp(p, "<!--", 4))
         return 0;
      next(4);
      while (*p && memcmp(p, "-->", 3))
         next(1);
      if (*p)
      {
         next(3);
         // nice if we have a means to store comments some day
         return 1;
      }
      er = "Bad comment";
      return 0;
   }
   int parse_reference(FILE * o) {
      if (*p != '&')
         return 0;
      next(1);
      const unsigned char *s = p;
      while (*p && *p != ';')
         next(1);
      if (*p && !er)
      {
         int l = p - s;
         if (l == 3 && !memcmp(s, "amp", l))
            fputc('&', o);
         else if (l == 2 && !memcmp(s, "lt", l))
            fputc('<', o);
         else if (l == 2 && !memcmp(s, "gt", l))
            fputc('>', o);
         else if (l == 4 && !memcmp(s, "apos", l))
            fputc('\'', o);
         else if (l == 4 && !memcmp(s, "quot", l))
            fputc('"', o);
         else if (*s == '#')
         {
            unsigned int c = 0;
            if (s[1] == 'x')
               for (s += 2; isxdigit(*s); s++)
                  c = c * 16 + (isalpha(*s) ? 9 : 0) + (*s & 0xF);
            else
               for (s++; isdigit(*s); s++)
                  c = c * 10 + (*s - '0');
            if (s != p)
               er = "Bad character reference";
            write_utf8(o, c);
         } else
            er = "Unknown reference";
         next(1);
         return 1;
      }
      er = "Bad reference";
      return 0;
   }
   int parse_chardata(FILE * o) {       // Write character
      unsigned int c = utf8();
      if (c)
         write_utf8(o, c);
      next(1);
      return 1;
   }
   int parse_cdsect(FILE * o) {
      if (er)
         return 0;
      if (memcmp(p, "<![CDATA[", 9))
         return 0;
      next(9);
      while (*p && memcmp(p, "]]>", 3) && !er)
         parse_chardata(o);
      if (*p)
      {
         next(3);
         return 1;
      }
      er = "Bad CDATA";
      return 0;
   }
   int parse_xmldecl(xml_t e) {
      e = e;
      if (er)
         return 0;
      if (memcmp(p, "<?xml", 5) || !iss(p[5]))
         return 0;
      next(5);
      // TODO process encoding, standalone, version...
      while (*p >= ' ' && (*p != '?' || p[1] != '>'))
         next(1);
      if (*p)
      {                         // found end
         next(2);
         return 1;
      }
      er = "Invalid XML declaration";
      return 0;
   }
   void parse_misc(xml_t e) {   // Comment, PI or S
      while (!er)
      {
         if (iss(*p))
         {                      // S
            next(1);
            continue;
         }
         if (parse_comment(e))
            continue;
         if (parse_pi(e))
            continue;
         // No more misc
         return;
      }
   }
   int parse_doctypedecl(xml_t e) {
      if (er)
         return 0;
      if (memcmp(p, "<!DOCTYPE", 9) || !iss(p[9]))
         return 0;
      next(9);
      while (iss(*p))
         next(1);
      const unsigned char *content = p;
      while (*p && *p != '>' && !er)
      {
         if (*p == '\'' || *p == '"')
         {                      // Quoted string
            unsigned char q = *p;
            next(1);
            while (*p && *p != q)
               next(1);
            if (*p)
               next(1);
            else
               er = "Unclosed quoted string";
            continue;
         }
         next(1);
      }
      if (*p && !er)
      {                         // End
         xml_pi_add_l(e, 8, "!DOCTYPE", p - content, (const char *) content);
         next(1);
         return 1;
      }
      er = "Bad doctype";
      return 0;
   }
   int parse_prolog(xml_t e) {  // Prolog
      int r = parse_xmldecl(e);
      parse_misc(e);
      if (parse_doctypedecl(e))
         parse_misc(e);
      return r;
   }
   int parse_element(xml_t e) {
      if (er)
         return 0;
      int parse_attribute(xml_t e) {
         if (er)
            return 0;
         while (iss(*p))
            next(1);            // S
         if (*p == '/' || *p == '>')
            return 0;           // No more attributes
         const unsigned char *atag = parse_name();
         if (!atag)
         {
            er = "Bad attribute name";
            return 0;
         }
         int atagl = p - atag;
         const unsigned char *name = atag;
         int namel = atagl;
         {
            const unsigned char *q;
            for (q = atag; q < p && *q != ':'; q++);
            if (q > atag && q < p)
            {
               name = q + 1;
               namel = p - (q + 1);
            }
         }
         while (iss(*p))
            next(1);
         if (*p != '=')
         {
            er = "Missing attribute = sign";
            return 0;
         }
         next(1);
         while (iss(*p))
            next(1);
         if (*p != '\'' && *p != '"')
         {
            er = "Unquoted/missing attribute value";
            return 0;
         }
         unsigned char q = *p;
         next(1);
         char *content = NULL;
         size_t contentl = 0;
         FILE *o = open_memstream(&content, &contentl);
         while (*p && *p != q && *p != '<')
         {
            if (*p == '&')
               parse_reference(o);
            else
               parse_chardata(o);
         }
         fclose(o);
         if (*p == q)
         {
            next(1);
            xml_namespace_t ns = NULL;
            if (name == atag && namel == 5 && !memcmp(name, "xmlns", 5))
            {
               ns_stack(0, NULL, contentl, content);    // Default namespace
               name = NULL;
            } else if (name != atag)
            {                   // prefix
               if (name - atag == 6 && !memcmp(atag, "xmlns", 5))
               {
                  ns_stack(namel, name, contentl, content);     // Define namespace
                  name = NULL;
               } else if (!(ns = ns_find(name - atag - 1, atag)))
                  er = "Unknown namespace";
            }
            if (name)
               xml_attribute_set_ns_l(e, ns, namel, (const char *) name, contentl, (const char *) content);
            if (content)
               free(content);
            return 1;
         }
         if (content)
            free(content);
         er = "Unclosed quote";
         return 0;
      }
      if (er)
         return 0;
      if (*p != '<' || p[1] == '/' || p[1] == '!' || p[1] == '-' || p[1] == '?')
         return 0;              // not an element
      next(1);
      const unsigned char *stag = parse_name();
      if (!stag)
      {
         er = "Bad element name";
         return 0;
      }
      int stagl = p - stag;
      // Namespace
      const unsigned char *name = stag;
      int namel = stagl;
      {
         const unsigned char *q;
         for (q = name; q < p && *q != ':'; q++);
         if (q > name && q < p)
         {                      // Namespace prefix
            name = q + 1;
            namel = p - (q + 1);
         }
      }
      xml_t n = e;
      if (e == root && !root->name)
         root->name = xml_dup_l(namel, (const char *) name);
      else
         n = xml_element_add_ns_after_l(e, NULL, namel, (const char *) name, NULL);
      if (!n)
      {
         er = "Something wrong";
         return 0;
      }
      while (parse_attribute(n));
      if (er)
         return 0;
      // Work out element namespace
      int nsl = 0;
      if (name > stag)
         nsl = name - stag - 1;
      xml_namespace_t ns = ns_find(nsl, stag);
      if (!ns && nsl)
         er = "Unknown element namespace";
      n->namespace = ns;
      if (*p == '/' && p[1] == '>')
      {                         // EmptyElemTag
         next(2);
         return 1;
      }
      // STag
      if (*p != '>')
      {
         er = "Unclosed STag";
         return 0;
      }
      next(1);
      char *content = NULL;
      size_t contentl = 0;
      FILE *o = open_memstream(&content, &contentl);
      while (*p && (*p != '<' || p[1] != '/') && !er)
      {                         // content
         while (*p && *p != '<' && *p != '&')
            parse_chardata(o);
         if (parse_reference(o))
            continue;
         if (parse_cdsect(o))
            continue;
         if (parse_pi(n))
            continue;           // At some point we may want to break the content around the pi
         if (parse_comment(n))
            continue;           // At some point we may want to break the content around the elements
         int nsssave = nssp;    // Retain NS stack position
         parse_element(n);
         nssp = nsssave;
      }
      fclose(o);
      if (content)
      {
         xml_element_set_content_l(n, contentl, content);
         free(content);
      }
      if (er)
         return 0;
      // Check ETag
      if (*p != '<' || p[1] != '/')
      {
         er = "Unclosed element";
         return 0;
      }
      next(2);
      const unsigned char *etag = parse_name();
      if (!etag)
      {
         er = "Bad ETag";
         return 0;
      }
      int etagl = p - etag;
      while (iss(*p))
         next(1);
      if (*p != '>')
      {
         er = "Bad Etag";
         return 0;
      }
      next(1);
      if (etagl != stagl || memcmp(etag, stag, stagl))
      {
         er = "Mismatched STag/Etag";
         return 0;
      }
      return 1;
   }
   parse_prolog(root);
   if (!parse_element(root) && !er)
      er = "No root element";
   parse_misc(root);
   if (!er && *p)
      er = "Extra data after xml";
   if (er)
   {                            // Error
      xml_tree_delete(root);
      root = NULL;
      if (fail)
         fail(filename, line, character, er, posn);
   }
   if (nss)
      free(nss);
   return root;                 // OK - no error
}
#endif

xml_t xml_element_by_name_ns(xml_t e, xml_namespace_t namespace, const char *name)
{
   if (!e)
      return 0;
   xml_t c = e->first_child;
   while (c && ((namespace && c->namespace != namespace) || (name && strcmp(c->name, name))))
      c = c->next;
   return c;
}

xml_attribute_t xml_attribute_by_name_ns(xml_t e, xml_namespace_t namespace, const char *name)
{
   if (!e)
      return 0;
   xml_attribute_t a = e->first_attribute;
   while (a && ((a->namespace != namespace && (namespace || a->namespace != e->namespace)) || (name && strcmp(a->name, name))))
      a = a->next;
   return a;
}

xml_t xml_element_next_by_name_ns(xml_t parent, xml_t prev, xml_namespace_t namespace, const char *name)
{
   if (!parent)
      return 0;
   if (prev)
      prev = prev->next;
   else
      prev = parent->first_child;
   while (prev && ((namespace && prev->namespace != namespace) || (name && strcmp(prev->name, name))))
      prev = prev->next;
   return prev;
}

xml_t xml_element_next(xml_t parent, xml_t prev)
{
   if (!parent)
      return 0;
   if (prev)
      prev = prev->next;
   else
      prev = parent->first_child;
   return prev;
}

xml_attribute_t xml_attribute_next(xml_t e, xml_attribute_t prev)
{
   if (!e)
      return 0;
   if (prev)
      prev = prev->next;
   else
      prev = e->first_attribute;
   return prev;
}

xml_t xml_element_add_ns_after_l(xml_t parent, xml_namespace_t namespace, int namel, const char *name, xml_t prev)
{                               // prev can be null to be last in parent. Parent can be null is assumed parent of prev. Special case of prev=parent means insert at start
   if (!parent && prev)
      parent = prev->parent;
   if (prev && prev->parent != parent && prev != parent)
      errx(1, "Not siblings (xml_element_add_ns_after)");
   xml_t e = xml_alloc(sizeof(*e));
   e->parent = parent;
   e->tree = parent->tree;
   e->namespace = (namespace ? : parent->namespace);
   e->name = xml_dup_l(namel, name);
   if (!parent->first_child)
      parent->first_child = e;  // only child
   else
   {                            // Insert in to chain
      if (!prev && parent->first_child)
         prev = parent->last_child;     // At end
      if (prev == parent)
      {                         // At start
         e->next = parent->first_child;
         parent->first_child = e;
      } else
      {                         // after prev
         e->prev = prev;
         e->next = prev->next;
         prev->next = e;
      }
      if (e->next)
         e->next->prev = e;
   }
   if (!e->next)
      parent->last_child = e;   // We are last
   return e;
}

xml_t xml_element_add_ns_after(xml_t parent, xml_namespace_t namespace, const char *name, xml_t prev)
{
   return xml_element_add_ns_after_l(parent, namespace, strlen(name ? : ""), name, prev);
}

xml_attribute_t xml_attribute_set_ns_l(xml_t e, xml_namespace_t namespace, int namel, const char *name, int contentl, const char *content)
{
   if (!name)
      errx(1, "Null name (xml_attribute_set_ns)");
   if (!e)
      errx(1, "Null element xml_attribute_set_ns)");
   // see if exists
   xml_attribute_t a = e->first_attribute;
   while (a && ((a->namespace != namespace && (namespace || a->namespace != e->namespace)) || (name && strcmp_l(strlen(a->name ? : ""), a->name, namel, name))))
      a = a->next;
   if (!content)
   {
      if (a)
         xml_attribute_delete(a);
      return NULL;              // deleted
   }
   if (a)
   {                            // change content
      xml_content_t new = xml_dup_l(contentl, content);
      xml_free(a->content);
      a->content = new;
      return a;
   }
   // new attribute
   a = xml_alloc(sizeof(*a));
   a->parent = e;
   if (e->first_attribute)
      e->last_attribute->next = a;
   else
      e->first_attribute = a;
   a->prev = e->last_attribute;
   e->last_attribute = a;
   a->name = xml_dup_l(namel, name);
   a->content = xml_dup(content);
   if (namespace)
      a->namespace = namespace;
   return a;
}

xml_attribute_t xml_attribute_set_ns(xml_t e, xml_namespace_t namespace, const char *name, const char *content)
{
   return xml_attribute_set_ns_l(e, namespace, strlen(name ? : ""), name, strlen(content ? : ""), content);
}

xml_t xml_element_delete(xml_t e)
{                               // delete an element and all subordinate elements
   if (!e)
      return NULL;
   if (e->prev)
      e->prev->next = e->next;
   else if (e->parent)
      e->parent->first_child = e->next;
   if (e->next)
      e->next->prev = e->prev;
   else if (e->parent)
      e->parent->last_child = e->prev;
   // Delete attributes
   xml_attribute_t a = e->first_attribute;
   while (a)
   {
      xml_attribute_t n = a->next;
      xml_attribute_delete(a);
      a = n;
   }
   // Delete children
   xml_t c = e->first_child;
   while (c)
   {
      xml_t n = c->next;
      xml_element_delete(c);
      c = n;
   }
   // Clean up element
   e->name = xml_free(e->name);
   e->content = xml_free(e->content);
   e->namespace = NULL;
   if (e->parent)
      xml_free(e);              // Else leave in place as empty root element on tree
   return NULL;
}

xml_t xml_element_duplicate(xml_t e)
{
   void copy(xml_t x, xml_t e) {
      xml_element_set_content(x, e->content);
      for (xml_attribute_t a = e->first_attribute; a; a = a->next)
         xml_attribute_set(x, a->name, a->content);
      for (xml_t c = e->first_child; c; c = c->next)
         copy(xml_element_add_ns(x, c->namespace, c->name), c);
   }
   xml_t x = xml_element_add_ns_after(e->parent, e->namespace, e->name, e);
   copy(x, e);
   return x;
}

void xml_element_explode(xml_t e)
{                               // delete an element but making its subordinate elements take its place
   if (!e)
      errx(1, "No element to explode (xml_element_explode)");
   if (e->prev)
      e->prev->next = e->next;
   else if (e->parent)
      e->parent->first_child = e->next;
   if (e->next)
      e->next->prev = e->prev;
   else if (e->parent)
      e->parent->last_child = e->prev;
   xml_attribute_t a = e->first_attribute;
   while (a)
   {
      xml_attribute_t n = a->next;
      xml_attribute_delete(a);
      a = n;
   }
   xml_t c = e->first_child;
   if (c)
   {
      while (c)
      {
         c->parent = e->parent;
         c = c->next;
      }
      if (e->prev)
      {
         e->prev->next = e->first_child;
         e->first_child->prev = e->prev;
      } else if (e->parent)
         e->parent->first_child = e->first_child;
      if (e->next)
      {
         e->next->prev = e->last_child;
         e->last_child->next = e->next;
      } else if (e->parent)
         e->parent->last_child = e->last_child;
   }
   xml_free(e->name);
   xml_free(e->content);
   xml_free(e);
}

void xml_attribute_delete(xml_attribute_t a)
{
   if (!a)
      errx(1, "Null attribute (xml_attribute_delete)");
   if (a->prev)
      a->prev->next = a->next;
   else
      a->parent->first_attribute = a->next;
   if (a->next)
      a->next->prev = a->prev;
   else
      a->parent->last_attribute = a->prev;
   xml_free(a->name);
   xml_free(a->content);
   xml_free(a);
}

xml_t xml_tree_new(const char *name)
{                               // Create a new tree - return the (unnamed) root
   xml_root_t tree = xml_alloc(sizeof(*tree));
   tree->encoding = "UTF-8";
   xml_t e = xml_alloc(sizeof(*e));
   tree->root = e;
   e->tree = tree;
   if (name)
      e->name = xml_dup(name);
   xml_namespace(e, "xml", "http://www.w3.org/XML/1998/namespace");
   return e;
}

xml_t xml_tree_add_root_ns(xml_t e, xml_namespace_t namespace, const char *name)
{                               // Rename root and namespace root - trees always have roots now
   xml_root_t tree = e->tree;
   e = tree->root;
   e->namespace = namespace;
   if (name)
   {
      if (e->name)
         e->name = xml_free(e->name);
      e->name = xml_dup(name);
   }
   return e;
}

xml_namespace_t xml_namespace_l(xml_t tree, int tagl, const char *tag, unsigned int namespacel, const char *namespace)
{
   xml_root_t t = tree->tree;
   if (!namespace)
      return NULL;
   if (!t)
      errx(1, "Null tree (xml_namespace)");
   xml_namespacelist_t l = t->namespacelist;
   while (l && (strlen(l->namespace->uri) != namespacel || strncmp(l->namespace->uri, namespace, namespacel)))
      l = l->next;
   if (!l)
   {
      l = xml_alloc(sizeof(*l));
      l->namespace = xml_alloc(sizeof(*l->namespace) + namespacel + 1);
      strncpy(l->namespace->uri, namespace, namespacel);
      if (t->namespacelist)
         t->namespacelist_end->next = l;
      else
         t->namespacelist = l;
      t->namespacelist_end = l;
   }
   if (tag)
   {
      //if (l->namespace->fixed && l->tag && strcmp (l->tag, tag)) errx (1, "Two tags for ns %s (%s/%s)", namespace, tag, l->tag);
      xml_free(l->tag);
      l->namespace->fixed = 1;
      while (tagl && *tag && strchr(":^*", *tag))
      {                         // prefixes
         if (tagl && *tag == ':')
         {                      // Not a fixed tag
            l->namespace->fixed = 0;
            tag++;
            tagl--;
         }
         if (tagl && *tag == '^')
         {                      // Put at root level regardless of usage
            l->namespace->nsroot = 1;
            tag++;
            tagl--;
         }
         if (tagl && *tag == '*')
         {                      // Always include, regardless of usage
            l->namespace->always = 1;
            tag++;
            tagl--;
         }
      }
      l->tag = xml_dup_l(tagl, tag);
   }
   return l->namespace;
}

xml_namespace_t xml_namespace(xml_t tree, const char *tag, const char *namespace)
{
   if (!namespace)
      return NULL;
   return xml_namespace_l(tree, strlen(tag ? : ""), tag, strlen(namespace ? : ""), namespace);
}

void xml_pi_delete(xml_pi_t p)
{
   if (!p)
      errx(1, "Null PI (xml_pi_delete)");
   if (p->prev)
      p->prev->next = p->next;
   else
      p->tree->first_pi = p->next;
   if (p->next)
      p->next->prev = p->prev;
   else
      p->tree->last_pi = p->prev;
   xml_free(p->name);
   xml_free(p->content);
   xml_free(p);
}

static xml_t xml_real_tree_delete(xml_root_t t)
{
   if (t->root)
   {
      xml_element_delete(t->root);
      xml_free(t->root);        // Not freed by element delete as root element
   }
   xml_namespacelist_t l = t->namespacelist;
   while (l)
   {
      xml_namespacelist_t n = l->next;
      xml_free((char *) l->namespace);
      xml_free(l->tag);
      xml_free(l);
      l = n;
   }
   xml_pi_t p = t->first_pi;
   while (p)
   {
      xml_pi_t n = p->next;
      xml_pi_delete(p);
      p = n;
   }
   xml_stringlist_t s = t->strings;
   while (s)
   {
      xml_stringlist_t n = s->next;
      xml_free(s->string);
      xml_free(s);
      s = n;
   }
   xml_free(t);
   return NULL;
}

xml_t xml_tree_delete(xml_t tree)
{
   if (!tree)
      errx(1, "No tree to delete (xml_tree_delete)");
   return xml_real_tree_delete(tree->tree);
}

static void change_namespace(xml_t e, xml_namespace_t o, xml_namespace_t n, xml_root_t t)
{
   e->tree = t;
   if (e->namespace == o)
      e->namespace = n;
   xml_attribute_t a = e->first_attribute;
   while (a)
   {
      if (a->namespace == o)
         a->namespace = n;
      a = a->next;
   }
   xml_t c = e->first_child;
   while (c)
   {
      change_namespace(c, o, n, t);
      c = c->next;
   }
}

static void update_treerefs(xml_t e, xml_root_t t)
{
   e->tree = t;
   if (e->filename)
      e->filename = savestring(e->filename, strlen(e->filename), t);
   e = e->first_child;
   while (e)
   {
      update_treerefs(e, t);
      e = e->next;
   }
}

xml_t xml_element_attach_after(xml_t pe, xml_t prev, xml_t e)
{
   // Detaches e from its tree, and attaches it under pe
   // if e is root element, then copies e first and clears e as empty root, leaving e still valid as tree pointer and safe to delete
   // Returns the attached element, usually the same as e, unless it was root
   // At present, attaching under empty root barfs, but in future may replace it
   if (!pe && prev)
      pe = prev->parent;
   if (!pe || !e)
      errx(1, "Null element (xml_element_attach)");
   if (prev && prev != pe && prev->parent != pe)
      errx(1, "Prev is not child of parent (xml_element_attach)");
   xml_root_t pt = pe->tree;
   xml_root_t t = e->tree;
   if (pt == t && !e->parent)
      errx(1, "Moving root of same tree (xml_element_attach)");
   // detach from old parent
   if (!e->parent)
   {                            // root
      if (t)
      {                         // In theory could be a detached element,
         xml_t n = xml_alloc(sizeof(*n));       // the new root
         // Move over the attributes to the new detached element leaving the tree with the original pointer (e) that is empty root
#define m(x) n->x=e->x;e->x=NULL;
         m(name);
         m(content);
         m(namespace);
         m(first_child);
         m(last_child);
         m(first_attribute);
         m(last_attribute);
         m(filename);
#undef m
         n->line = e->line;
         e->line = 0;
         n->json_single = e->line;
         e->json_single = 0;
         e = n;
         // Link children back to this new copy parent not old replaced parent
         for (n = e->first_child; n; n = n->next)
            n->parent = e;
         xml_attribute_t a;
         for (a = e->first_attribute; a; a = a->next)
            a->parent = e;
      }
   } else
   {
      if (e->prev)
         e->prev->next = e->next;
      else if (e->parent)
         e->parent->first_child = e->next;
      if (e->next)
         e->next->prev = e->prev;
      else if (e->parent)
         e->parent->last_child = e->prev;
   }
   e->prev = e->next = e->parent = NULL;        // detached
   // Fix name space
   if (t && t != pt)
   {
      xml_namespacelist_t o = t->namespacelist;
      while (o)
      {
         xml_namespace_t new = 0;
         xml_namespacelist_t n = pt->namespacelist;
         while (n && strcmp(n->namespace ? n->namespace->uri : "", o->namespace ? o->namespace->uri : ""))
            n = n->next;
         if (!n)
            new = xml_namespace(pe, 0, o->namespace->uri);
         else
            new = n->namespace;
         change_namespace(e, o->namespace, new, t);
         o = o->next;
      }
   }
   // Fix tree reference
   if (t != pt)
      update_treerefs(e, pt);   // Set tree refs
   // attach to new tree
   if (!pe->parent && !pe->name && !pe->prev && !pe->next && !pe->first_child && !pe->first_attribute)
   {                            // pe is a lone null root
      errx(1, "Attaching at root");
      // Ideally pe needs to stay valid as the root but become the new element
   } else
   {                            // attach as new child under pe
      if (prev)
      {                         // Link in after specified previous element
         if (prev == pe)
         {                      // Insert at start
            if (pe->first_child)
               pe->first_child->prev = e;
            else
               pe->last_child = e;
            e->next = pe->first_child;
            pe->first_child = e;
         } else
         {                      // Append
            e->prev = prev;
            e->next = prev->next;
            prev->next = e;
            if (e->next)
               e->next->prev = e;
            else
               pe->last_child = e;
         }
      } else
      {                         // Attach at end
         if (pe->first_child)
            pe->last_child->next = e;
         else
            pe->first_child = e;
         e->prev = pe->last_child;
         pe->last_child = e;
      }
   }
   e->parent = pe;
   return e;
}

// output
static void write_name(FILE * fp, const char *t)
{
   while (*t)
   {
      unsigned int v = (unsigned char) (*t++);
      if (v <= ' ' || v == '"' || v == '<' || v == '>' || v == '&' || v == '=')
         fprintf(fp, "&#%u;", v);
      else
         fputc(v, fp);
   }
}

void xml_quoted(FILE * fp, const char *t)
{
   while (*t)
   {
      unsigned int v = (unsigned char) (*t++);
      if ((v & 0xE0) == 0xC0 && (t[0] & 0xC0) == 0x80)
      {                         // valid utf8
         fputc(v, fp);
         fputc(*t++, fp);
         continue;
      }
      if ((v & 0xF0) == 0xE0 && (t[0] & 0xC0) == 0x80 && (t[1] & 0xC0) == 0x80)
      {                         // valid utf8
         fputc(v, fp);
         fputc(*t++, fp);
         fputc(*t++, fp);
         continue;
      }
      if ((v & 0xF8) == 0xF0 && (t[0] & 0xC0) == 0x80 && (t[1] & 0xC0) == 0x80 && (t[2] & 0xC0) == 0x80)
      {                         // valid utf8
         fputc(v, fp);
         fputc(*t++, fp);
         fputc(*t++, fp);
         fputc(*t++, fp);
         continue;
      }
      if (v >= 0x80)
      {                         // Not valid UTF-8
         fputc(0xEF, fp);       // REPLACEMENT CHARACTER
         fputc(0xBF, fp);
         fputc(0xBD, fp);
         continue;
      }
      // TODO looks like I expect v to be unicode
      if (v == 9 || v == 10 || v == 13 || (v >= 0x20 && v <= 0xD7FF) || (v >= 0xE000 && v <= 0xFFFD) || (v >= 0x10000 && v <= 0x10FFFF))
      {                         // XML 1.0 code points
         if (v == '"')
            fprintf(fp, "&quot;");
         else if (v == '&')
            fprintf(fp, "&amp;");
         else if (v == '<')
            fprintf(fp, "&lt;");
         else if (v == '>')
            fprintf(fp, "&gt;");
         else if (!v || (v < ' ' && v != '\t' && v != '\n' && v != '\r'))
            fprintf(fp, "?");
         else if (v < ' ' || v >= 0x80)
            fprintf(fp, "&#%u;", v);
         else
            fputc(v, fp);
      }
   }
}

static void write_content(FILE * fp, const char *t)
{
   while (*t)
   {
      unsigned int v = (unsigned char) (*t++);
      if ((v & 0xE0) == 0xC0 && (t[0] & 0xC0) == 0x80)
      {                         // valid utf8
         fputc(v, fp);
         fputc(*t++, fp);
         continue;
      }
      if ((v & 0xF0) == 0xE0 && (t[0] & 0xC0) == 0x80 && (t[1] & 0xC0) == 0x80)
      {                         // valid utf8
         fputc(v, fp);
         fputc(*t++, fp);
         fputc(*t++, fp);
         continue;
      }
      if ((v & 0xF8) == 0xF0 && (t[0] & 0xC0) == 0x80 && (t[1] & 0xC0) == 0x80 && (t[2] & 0xC0) == 0x80)
      {                         // valid utf8
         fputc(v, fp);
         fputc(*t++, fp);
         fputc(*t++, fp);
         fputc(*t++, fp);
         continue;
      }
      if (v >= 0x80)
      {                         // Not valid UTF-8
         fputc(0xEF, fp);       // REPLACEMENT CHARACTER
         fputc(0xBF, fp);
         fputc(0xBD, fp);
         continue;
      }
      // TODO looks like I expect v to be unicode
      if (v == 9 || v == 10 || v == 13 || (v >= 0x20 && v <= 0xD7FF) || (v >= 0xE000 && v <= 0xFFFD) || (v >= 0x10000 && v <= 0x10FFFF))
      {                         // XML 1.0 code points
         if (v == '&')
            fprintf(fp, "&amp;");
         else if (v == '<')
            fprintf(fp, "&lt;");
         else if (v == '>')
            fprintf(fp, "&gt;");
         else if (v < 0x80 && (v >= ' ' || v == '\n' || v == '\r' || v == '\t'))
            fputc(v, fp);
         else
            fprintf(fp, "&#%u;", v);
      }
   }
}

static void write_namespace(FILE * fp, xml_namespace_t ns)
{
   if (ns && ns->outputtag && *ns->outputtag)
   {
      write_name(fp, ns->outputtag);
      fputc(':', fp);
   }
}

static int cmp_namespace(const void *A, const void *B)
{
   xml_namespacelist_t a = *(xml_namespacelist_t *) A,
       b = *(xml_namespacelist_t *) B;
   return strcmp(a->namespace && a->namespace->outputtag ? a->namespace->outputtag : "", b->namespace && b->namespace->outputtag ? b->namespace->outputtag : "");
}

static void sort_namespace(xml_root_t t)
{
   xml_namespacelist_t q;
   int c = 0,
       i = 0;
   for (q = t->namespacelist; q; q = q->next)
      c++;
   xml_namespacelist_t l[c];
   for (q = t->namespacelist; q; q = q->next)
      l[i++] = q;
   qsort(l, c, sizeof(*l), cmp_namespace);
   xml_namespacelist_t *nn = &t->namespacelist;
   for (i = 0; i < c; i++)
   {
      *nn = l[i];
      nn = &l[i]->next;
   }
   *nn = NULL;
   t->namespacelist_end = (c ? l[c - 1] : NULL);
}

static int cmp_attribute(const void *A, const void *B)
{
   xml_attribute_t a = *(xml_attribute_t *) A,
       b = *(xml_attribute_t *) B;
   int r = strcmp(a->namespace && a->namespace->uri ? a->namespace->uri : "", b->namespace && b->namespace->uri ? b->namespace->uri : "");
   if (r)
      return r;
   return strcmp(a->name, b->name);
}

static void sort_attributes(xml_t e)
{
   xml_attribute_t q;
   int c = 0,
       i = 0;
   for (q = e->first_attribute; q; q = q->next)
      c++;
   xml_attribute_t l[c];
   for (q = e->first_attribute; q; q = q->next)
      l[i++] = q;
   qsort(l, c, sizeof(*l), cmp_attribute);
   xml_attribute_t *aa = &e->first_attribute,
       p = NULL;
   for (i = 0; i < c; i++)
   {
      l[i]->prev = p;
      *aa = l[i];
      aa = &l[i]->next;
   }
   *aa = NULL;
   e->last_attribute = p;
}

static void count_namespace_e(xml_t e)
{
   if (e->namespace)
      e->namespace->count++;
   xml_attribute_t a;
   for (a = e->first_attribute; a; a = a->next)
      if (a->namespace)
         a->namespace->count++;
   for (e = e->first_child; e; e = e->next)
      count_namespace_e(e);
}

static void count_namespace(xml_t e)
{
   xml_namespacelist_t l;
   for (l = e->tree->namespacelist; l; l = l->next)
      l->namespace->count = l->namespace->always;
   count_namespace_e(e);
}

static void write_element(FILE * fp, xml_t e, int indent, xml_namespace_t defns)
{
   xml_root_t t = e->tree;
   if (e->name && *e->name)
   {
      if (indent > 0)
      {
         int i = indent;
         while (i--)
            fputc(' ', fp);
      }
      xml_namespace_t wasdef = NULL;    // replaced def namespace
      {                         // change defns?
         count_namespace(e);
         xml_namespacelist_t ns;
         xml_namespace_t b = defns;
         for (ns = t->namespacelist; ns; ns = ns->next)
            if ((!ns->namespace->fixed || !*ns->tag) && ns->namespace->parent == e && (!b || b->count < ns->namespace->count))
               b = ns->namespace;
         if (b && b != defns)
         {
            if (defns && defns->count)
               wasdef = defns;
            defns = b;
         }
      }
      fputc('<', fp);
      if (e->namespace != defns)
         write_namespace(fp, e->namespace);
      write_name(fp, e->name);
      {                         // namespaces
         xml_namespacelist_t ns;
         if (defns && (defns->parent == e || indent == -1))
         {
            fputc(' ', fp);
            write_name(fp, "xmlns");
            fputc('=', fp);
            fputc('"', fp);
            if (defns && defns->parent == e && defns->uri)
               xml_quoted(fp, defns->uri);
            fputc('"', fp);
         }
         for (ns = t->namespacelist; ns; ns = ns->next)
            if (ns->namespace != defns && (ns->namespace->parent == e || ns->namespace == wasdef) && ns->namespace->count && *ns->namespace->uri)
            {
               fputc(' ', fp);
               write_name(fp, "xmlns");
               fputc(':', fp);
               write_name(fp, ns->tag);
               fputc('=', fp);
               fputc('"', fp);
               if (ns->namespace && ns->namespace->uri)
                  xml_quoted(fp, ns->namespace->uri);
               fputc('"', fp);
            }
      }
      if (indent == -1)
         sort_attributes(e);
      xml_attribute_t a = e->first_attribute;
      while (a)
      {
         if (strncmp(a->name, "xmlns", 5) || ((a->name)[5] != ':' && (a->name)[5]))
         {
            fputc(' ', fp);
            if (a->namespace && a->namespace != defns)
               write_namespace(fp, a->namespace);
            write_name(fp, a->name);
            fputc('=', fp);
            fputc('"', fp);
            xml_quoted(fp, a->content);
            fputc('"', fp);
         }
         a = a->next;
      }
   }
   char *content = e->content;
   if (indent >= 0 && content && e->first_child)
   {                            // If only white space around other sub elements, do not consider as content
      char *p = content;
      while (*p == ' ' || *p == 9 || *p == 10 || *p == 13)
         p++;
      if (!*p)
         content = NULL;
   }
   if (content || e->first_child || indent < 0)
   {
      if (e->name && *e->name)
         fputc('>', fp);
      if (!e->name && content)
         fprintf(fp, "%s", content);    // bodge for raw XML inclusions (HMRC DPS crap)
      else if (content)
         write_content(fp, content);
      xml_t c = e->first_child;
      if (c)
      {
         if (indent >= 0)
            fputc('\n', fp);
         while (c)
         {
            write_element(fp, c, indent < 0 ? indent - 1 : indent + 3, defns);
            c = c->next;
         }
         if (indent > 0)
         {
            int i = indent;
            while (i--)
               fputc(' ', fp);
         }
      }
      if (e->name && *e->name)
      {
         fputc('<', fp);
         fputc('/', fp);
         if (e->namespace != defns)
            write_namespace(fp, e->namespace);
         write_name(fp, e->name);
         fputc('>', fp);
      }
   } else if (e->name && *e->name)
   {                            // self closing
      if (*e->name == '?')
         fputc('?', fp);        // inline processing directive?!?
      else
         fputc('/', fp);
      fputc('>', fp);
   }
   if (indent >= 0)
      fputc('\n', fp);
}

static void ns_used(xml_namespace_t n, xml_t e, int l)
{
   if (!n)
      return;
   if (!n->parent || l < n->level)
   {
      n->parent = e;
      n->level = l;
      return;
   }
   while (l > n->level)
   {
      e = e->parent;
      l--;
   }
   if (e == n->parent)
      return;
   xml_t p = n->parent;
   while (l && e != p)
   {
      e = e->parent;
      p = p->parent;
      l--;
   }
   n->parent = p;
   n->level = l;
}

static void scan_namespace_element(xml_t e, int l)
{
   l++;
   ns_used(e->namespace, e, l);
   xml_attribute_t a;
   for (a = e->first_attribute; a; a = a->next)
      ns_used(a->namespace, e, l);
   for (e = e->first_child; e; e = e->next)
      scan_namespace_element(e, l);
}

static void scan_namespace(xml_root_t t, xml_t r)
{
   xml_namespacelist_t ns;
   for (ns = t->namespacelist; ns; ns = ns->next)
   {
      ns->namespace->level = 0;
      ns->namespace->parent = NULL;
   }
   scan_namespace_element(r, 0);
   for (ns = t->namespacelist; ns; ns = ns->next)
      if (ns->namespace->always || (ns->namespace->nsroot && ns->namespace->parent))
         ns->namespace->parent = t->root;
}

void xml_element_write(FILE * fp, xml_t e, int headers, int pack)
{
   if (!e)
      errx(1, "Null element (xml_element_write)");
   xml_root_t t = e->tree;
   if (!t)
      errx(1, "No tree to write (xml_element_write)");
   if (!fp)
      errx(1, "No file (xml_element_write)");
   if (!t->root)
      errx(1, "No root (xml_element_write)");
   if (headers)
   {
      if (t->encoding)
      {
         fprintf(fp, "<?xml version=\"1.0\" encoding=\"%s\"?>", t->encoding);
         if (!pack)
            fputc('\n', fp);
      }
      {                         // PI
         xml_pi_t pi = t->first_pi;
         while (pi)
         {
            if (*pi->name == '!')
               fprintf(fp, "<%s %s>", pi->name, pi->content);
            else
               fprintf(fp, "<?%s %s?>", pi->name, pi->content);
            if (!pack)
               fputc('\n', fp);
            pi = pi->next;
         }
      }
   }
   int nsn = 0;
   xml_namespacelist_t a,
    b;
   scan_namespace(t, e);
   sort_namespace(t);
   for (a = t->namespacelist; a; a = a->next)
   {
      if (!a->namespace->fixed)
      {
         if (a->tag)
         {                      // ensure no duplicates
            for (b = t->namespacelist; b && b != a && strcmp(a->tag ? : "", b->tag ? : ""); b = b->next);
            if (b && b != a)
            {
               xml_free(a->tag);
               a->tag = 0;
            }
         }
         if (!a->tag || !*a->tag)
         {                      // make one up
            char temp[10];
            while (1)
            {
               const char c[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
               int n = nsn++;
               char *p = temp;
               *p++ = c[n % 52];
               n /= 52;
               while (n)
               {
                  *p++ = c[n % 62];
                  n /= 62;
               }
               *p = 0;
               for (b = t->namespacelist; b && b != a && strcmp(temp, (b->tag ? : "")); b = b->next);
               if (!b || b == a)
                  break;
            }
            a->tag = xml_dup(temp);
         }
      }
   }
   for (a = t->namespacelist; a; a = a->next)
      a->namespace->outputtag = a->tag;
   write_element(fp, e, pack < 0 ? -2 : pack ? -1 : 0, NULL);
}

void xml_element_write_json(FILE * fp, xml_t e)
{                               // Write XML as JSON
   void string(const char *p, char name, char unquoted) {
      if (!p)
      {
         fprintf(fp, "null");
         return;
      }
      if (!unquoted)
         fputc('"', fp);
      while (*p)
      {
         unsigned char c = *p;
         if (c == '-' && name)
            fputc('_', fp);
         else if (c == '\n')
            fprintf(fp, "\\n");
         else if (c == '\r')
            fprintf(fp, "\\r");
         else if (c == '\t')
            fprintf(fp, "\\t");
         else if (c == '\b')
            fprintf(fp, "\\b");
         else if (c == '\\' || c == '"' || c == '/')    // Note / is technically an optional escape, but used here because of danger of </script>
            fprintf(fp, "\\%c", (char) c);
         else if (c < ' ')
            fprintf(fp, "\\u%04X", c);
         else
            fputc(c, fp);
         p++;
      }
      if (!unquoted)
         fputc('"', fp);
   }
   if (!xml_element_next(e, NULL) && !xml_attribute_next(e, NULL))
   {                            // Content only
      char *c = xml_element_content(e);
      if (c)
         string(c, 0, e->json_unquoted);
      else
         fprintf(fp, "null");
      return;
   }
   xml_t q;
   xml_attribute_t a;
   char sep = 0;
   a = NULL;
   while ((a = xml_attribute_next(e, a)))
   {
      if (sep)
         fputc(sep, fp);
      else
      {
         fputc('{', fp);
         sep = ',';
      }
      string(xml_attribute_name(a), 1, 0);
      fputc(':', fp);
      string(xml_attribute_content(a), 0, a->json_unquoted);
   }
   q = NULL;
   while ((q = xml_element_next(e, q)))
   {
      const char *n = xml_element_name(q);
      if (xml_element_next_by_name(e, NULL, n) != q)
         continue;              // done already
      if (*n)
      {
         if (sep)
            fputc(sep, fp);
         else
         {
            fputc('{', fp);
            sep = ',';
         }
         string(n, 1, 0);
         fputc(':', fp);
      }
      if (!q->json_single)
         fputc('[', fp);
      xml_t z = NULL;
      char sep2 = 0;
      while ((z = xml_element_next_by_name(e, z, n)))
      {
         if (sep2)
            fputc(sep2, fp);
         else
            sep2 = ',';
         xml_element_write_json(fp, z);
         if (q->json_single)
            break;
      }
      if (!q->json_single)
         fputc(']', fp);
   }
   if (sep)
      fputc('}', fp);
}

xml_t xml_element_compress(xml_t e)
{                               // Reduce single text only sub objects to attributes of parent
   xml_t q,
    next;
   q = NULL;
   next = xml_element_next(e, q);
   while (next)
   {
      q = next;
      next = xml_element_next(e, q);
      if (xml_attribute_next(q, NULL))
         continue;              // has attributes
      if (xml_element_next(q, NULL))
         continue;              // has sub elements
      const char *v = xml_element_content(q);
      const char *n = xml_element_name(q);
      if (xml_attribute_by_name(e, n))
         continue;              // parent has attribute the same
      if (xml_element_next_by_name(e, NULL, n) != q)
         continue;              // object already done
      if (xml_element_next_by_name(e, q, n))
         continue;              // more than one object
      if (v)
         xml_attribute_set(e, n, v);
      xml_element_delete(q);
   }
   q = NULL;
   while ((q = xml_element_next(e, q)))
      xml_element_compress(q);
   return e;
}

#ifdef	EXPAT
// Parser
static void parse_element_start(void *ud, const XML_Char * name, const XML_Char ** attr)
{
   xml_parser_t *p = ud;
#ifdef	PARSEDEBUG
   fprintf(stderr, "Start %s%s\n", name, p->here ? "" : " root");
#endif
   if (!p->tree && p->callback)
      p->tree = xml_tree_new(NULL)->tree;
   xml_root_t t = p->tree;
   xml_t e = xml_alloc(sizeof(*e));
   e->tree = t;
   e->line = (int) XML_GetCurrentLineNumber(p->parser) + p->line_offset;
   e->filename = p->current_file;
   {                            // Namespaces
      char foundxml = 0;
      xml_namespacestack_t stack = 0;
      const XML_Char **a = attr;
      if (a)
         while (*a)
         {
            if (!strncmp(*a, "xmlns", 5) && (((*a)[5] == ':' && (*a)[6]) || !(*a)[5]))
            {                   // name space attribute
               if ((*a)[5] && !strcmp((char *) (*a) + 6, "xml"))
                  foundxml = 1;
               // global list
               xml_namespace_t namespace = 0;
               xml_namespacelist_t ns;
               for (ns = p->tree->namespacelist; ns && strcmp(ns->namespace->uri, a[1]); ns = ns->next);
               if (ns)
                  namespace = ns->namespace;
               else
               {                // new namespace
                  ns = xml_alloc(sizeof(*ns));
                  ns->namespace = xml_alloc(sizeof(*ns->namespace) + strlen((char *) a[1]) + 1);
                  strcpy(ns->namespace->uri, (char *) a[1]);
                  namespace = ns->namespace;
                  if ((*a)[5] == ':' && !ns->tag)
                  {
                     ns->tag = xml_dup((char *) (*a) + 6);
                     namespace->fixed = 1;
                  }
                  if (p->tree->namespacelist)
                     p->tree->namespacelist_end->next = ns;
                  else
                     p->tree->namespacelist = ns;
                  p->tree->namespacelist_end = ns;
               }
               // local namespace stack
               if (!stack)
               {
                  stack = xml_alloc(sizeof(*stack));
                  stack->base = e;
               }
               ns = xml_alloc(sizeof(*ns));
               if ((*a)[5] && !ns->tag)
                  ns->tag = xml_dup((char *) (*a) + 6);
               ns->namespace = namespace;
               ns->next = stack->ns;
               stack->ns = ns;
            }
            a += 2;
         }
      if (!p->here && !foundxml)
      {                         // default xml namespace
         xml_namespacelist_t ns;
         for (ns = p->tree->namespacelist; ns && strcmp(ns->namespace->uri, "http://www.w3.org/XML/1998/namespace"); ns = ns->next);
         if (ns)
         {
            xml_namespace_t namespace = ns->namespace;
            if (!stack)
            {
               stack = xml_alloc(sizeof(*stack));
               stack->base = e;
            }
            ns = xml_alloc(sizeof(*ns));
            ns->tag = xml_dup("xml");
            ns->namespace = namespace;
            ns->next = stack->ns;
            stack->ns = ns;
         }
      }
      if (stack)
      {
         stack->prev = p->namespacestack;
         p->namespacestack = stack;
      }
   }
   const XML_Char *c;
   {                            // name uses a namespace
      for (c = name; *c && *c != ':'; c++);
      int l = 0;
      if (*c)
         l = (int) (c - name);
      xml_namespacestack_t s = p->namespacestack;
      while (s && !e->namespace)
      {
         xml_namespacelist_t n = s->ns;
         while (n && !e->namespace)
         {
            if ((!l && !n->tag) || (l && n->tag && !strncmp(n->tag, name, l) && !n->tag[l]))
               e->namespace = n->namespace;
            n = n->next;
         }
         s = s->prev;
      }
      if (*c && !e->namespace)
         errx(1, "Bad namespace on %s", name);
      if (l)
         name += l + 1;
   }
   e->name = xml_dup((char *) name);
   while (*attr)
   {                            // Attributes
      xml_attribute_t a = xml_alloc(sizeof(*a));
      a->parent = e;
      a->namespace = 0;
      name = (*attr);
      if (strncmp(*attr, "xmlns", 5) || ((*attr)[5] != ':' && (*attr)[5]))
      {
         for (c = name; *c && *c != ':'; c++);
         if (*c)
         {                      // NS prefix
            int l = 0;
            if (*c)
               l = (int) (c - name);
            xml_namespacestack_t s = p->namespacestack;
            while (s && !a->namespace)
            {
               xml_namespacelist_t n = s->ns;
               while (n && !a->namespace)
               {
                  if ((!l && !n->tag) || (n->tag && l && !strncmp(n->tag, name, l) && !n->tag[l]))
                     a->namespace = n->namespace;
                  n = n->next;
               }
               s = s->prev;
            }
            if (l)
               name += l + 1;
            if (!a->namespace)
               errx(1, "Bad name space on %s in %s", (*attr), e->name);
         }
      }
      a->name = xml_dup((char *) name);
      a->content = xml_dup((char *) attr[1]);
      if (e->first_attribute)
         e->last_attribute->next = a;
      else
         e->first_attribute = a;
      a->prev = e->last_attribute;
      e->last_attribute = a;
      attr += 2;
   }
   if (p->here)
   {
      if (p->here->first_child)
         p->here->last_child->next = e;
      else
         p->here->first_child = e;
      e->prev = p->here->last_child;
      p->here->last_child = e;
      e->parent = p->here;
   } else
      t->root = e;              // root element
   p->here = e;
}

static void parse_element_end(void *ud, const XML_Char * name)
{
   xml_parser_t *p = ud;
#ifdef	PARSEDEBUG
   fprintf(stderr, "End %s%s\n", name, p->here ? "" : " root");
#endif
   const XML_Char *q;
   for (q = name; *q && *q != ':'; q++);
   if (*q)
      name = q + 1;
   if (!p->here || strcmp(p->here->name, name))
      errx(1, "Invalid close tag %s", name);
   if (p->here->first_child)
   {
      char *c = p->here->content;
      if (c)
      {
         while (*c && isspace(*c))
            c++;
         if (!*c)
         {                      // only whitespace, and child entries, no content
            xml_free(p->here->content);
            p->here->content = 0;
         }
      }
   }
   if (p->namespacestack && p->namespacestack->base == p->here)
   {
      xml_namespacestack_t s = p->namespacestack;
      p->namespacestack = s->prev;
      xml_namespacelist_t ns = s->ns;
      xml_free(s);
      while (ns)
      {
         xml_namespacelist_t next = ns->next;
         xml_namespacelist_t m = p->tree->namespacelist;
         while (m && m->namespace != ns->namespace)
            m = m->next;
         // not free ns->namespace as copy of global
         xml_free(ns->tag);
         xml_free(ns);
         ns = next;
      }
   }
   p->here = p->here->parent;
   if (p->callback && !p->here && p->tree)
   {
      p->callback(p->tree->root);
      xml_tree_delete(p->tree->root);
      p->tree = NULL;
   }
}

static void parse_character_data(void *ud, const XML_Char * s, int len)
{
   xml_parser_t *p = ud;
#ifdef	PARSEDEBUG
   fprintf(stderr, "Character data %d\n", len);
#endif
   if (!len)
      return;
   if (!p->here)
   {
      while (len && isspace((*s++)))
         len--;
      if (!len)
         return;                // was just space
      errx(1, "Content not in xml document");
   }
   if (p->here->first_child)
   {
      int i;
      for (i = 0; i < len && isspace(s[i]); i++);
      if (i == len)
         return;                // whitespace between child entries
   }
   if (p->here->content)
   {
      int l = strlen(p->here->content);
      p->here->content = realloc(p->here->content, l + len + 1);
      if (!p->here->content)
         errx(1, "Malloc content");
      memmove(p->here->content + l, s, len);
      p->here->content[l + len] = 0;    // realloc used so not already null
   } else
   {
      p->here->content = xml_alloc(len + 1);
      memmove(p->here->content, s, len);
   }
}

static void parse_pi(void *ud, const XML_Char * target, const XML_Char * data)
{
   xml_parser_t *p = ud;
   xml_pi_t pi = xml_alloc(sizeof(*pi));
   pi->tree = p->tree;
   pi->name = xml_dup((char *) target);
   pi->content = xml_dup((char *) data);
   if (p->tree->first_pi)
      p->tree->last_pi->next = pi;
   else
      p->tree->first_pi = pi;
   pi->prev = p->tree->last_pi;
   p->tree->last_pi = pi;
}
#endif

static const char *json_parse_string(const char *json, char **name)
{
   char *v = NULL;
   int l = 0,
       a = 0;
   void addc(char c) {
      if (l + 2 > a)
      {
         a += 100;
         v = realloc(v, a);
         if (!v)
            errx(1, "malloc at line %d", __LINE__);
      }
      v[l++] = c;
      v[l] = 0;                 // realloc so not already 0
   }
   if (*json == '"')
   {                            // properly formatted
      addc(0);
      l--;                      // Ensure empty string allocated
      json++;
      while (*json && *json != '"')
      {
         if (*json == '\\')
         {
            json++;
            if (*json == 'u' && isxdigit(json[1]) && isxdigit(json[2]) && isxdigit(json[3]) && isxdigit(json[4]))
            {                   // utf coding
               int c = 0;
               sscanf(json + 1, "%4X", &c);
               if (c >= 0x800)
               {
                  addc(0xE0 + (c >> 12));
                  addc(0x80 + ((c >> 6) & 0x3F));
                  addc(0x80 + (c & 0x3F));
               } else if (c >= 0x80)
               {
                  addc(0xC0 + (c >> 6));
                  addc(0x80 + (c & 0x3F));
               } else
                  addc(c);
               json += 4;
            } else if (*json == 'n')
               addc('\n');
            else if (*json == 'r')
               addc('\r');
            else if (*json == 'b')
               addc('\b');
            else if (*json == 'f')
               addc('\f');
            else if (*json == 't')
               addc('\t');
            else
               addc(*json);
         } else
            addc(*json);
         json++;
      }
      if (*json == '"')
         json++;
   } else if (*json)
   {                            // special value
      while (isalnum(*json) || *json == '_' || *json == '.' || *json == '-' || *json == '+' || *json == '.')
         addc(*json++);
   }
   *name = v;
   return json;
}

static const char *json_parse_object(xml_t e, const char *json)
{                               // parse a JSON object, return next char after object or NULL for error
   char *name = NULL;
   void addvalue(int a, int inarray) {
      while (isspace(*json))
         json++;
      if (*json == '{')
      {                         // object
         xml_t o = xml_element_add(e, name && *name ? name : e->name);
         if (!inarray)
            o->json_single = 1;
         json = json_parse_object(o, json);
      } else if (*json == '[')
      {                         // array (treat as multiple objects)
         json++;
         while (isspace(*json))
            json++;
         while (*json && *json != ']')
         {
            addvalue(1, 1);
            if (!json)
               return;
            if (*json == ',')
               json++;
         }
         if (*json == ']')
            json++;
      } else
      {                         // value (treating null, false, true, numeric as just strings)
         char *value = NULL;
         char q = *json;
         json = json_parse_string(json, &value);
         if (value)
         {
            if (a)
            {
               xml_t o = xml_element_add(e, name && *name ? name : e->name);
               xml_element_set_content(o, value);
               if (q != '"')
                  o->json_unquoted = 1;
            } else
            {
               xml_attribute_t a = xml_attribute_set(e, name && *name ? name : e->name, value);
               if (q != '"')
                  a->json_unquoted = 1;
            }
            free(value);
         }
      }
   }
   while (isspace(*json))
      json++;
   if (*json == '[' || *json == '"')
   {                            // FFS, this is a JSON array or string not an object, create as if an object
      name = "";
      addvalue(0, 1);           // Works out if an array
      return json;
   }
   if (*json != '{')
      return NULL;              // err
   json++;
   while (isspace(*json))
      json++;
   while (*json && *json != '}')
   {
      // attribute name
      name = NULL;
      json = json_parse_string(json, &name);
      if (!name)
         return NULL;
      while (isspace(*json))
         json++;
      if (*json != ':')
         return NULL;           // err
      json++;
      addvalue(0, 0);
      free(name);
      name = NULL;
      if (!json)
         return json;
      while (isspace(*json))
         json++;
      if (*json == ',')
         json++;
      while (isspace(*json))
         json++;
   }
   if (*json == '}')
      json++;
   while (isspace(*json))
      json++;
   return json;
}

xml_t xml_tree_parse_json(const char *json, const char *rootname)
{
   while (isspace(*json))
      json++;
   xml_t t = xml_tree_new(rootname);
   if (*json == '{' || *json == '[')
      json = json_parse_object(t, json);
   else
   {                            // top level string or object somehow
      char *value = NULL;
      if (*json != '"')
         t->json_unquoted = 1;
      json = json_parse_string(json, &value);
      if (value)
      {
         xml_element_set_content(t, value);
         free(value);
      }
      while (isspace(*json))
         json++;
      if (*json || !value)
      {                         // not valid
         xml_tree_delete(t);
         t = NULL;
      }
   }
   return t;
}

#ifdef	EXPAT
xml_t xml_tree_parse(const char *xml)
{                               // parse an in-memory string, null terminated
   xml_parser_t parser = {
   };
   parser.tree = xml_tree_new(NULL)->tree;
   XML_Parser xml_parser = XML_ParserCreate(0);
   parser.parser = xml_parser;
   XML_SetUserData(xml_parser, &parser);
   XML_SetElementHandler(xml_parser, parse_element_start, parse_element_end);
   XML_SetCharacterDataHandler(xml_parser, parse_character_data);
   XML_SetProcessingInstructionHandler(xml_parser, parse_pi);
   if (!XML_Parse(xml_parser, xml, strlen(xml), 1))
   {
      //warnx ("Parse failed at %d:%d %s", (int) XML_GetCurrentLineNumber (xml_parser) + parser.line_offset, (int) XML_GetCurrentColumnNumber (xml_parser), XML_ErrorString (XML_GetErrorCode (xml_parser)));
      xml_tree_delete(parser.tree->root);
      parser.tree = NULL;
   }
   XML_ParserFree(xml_parser);
   if (!parser.tree)
      return NULL;
   return parser.tree->root;
}
#else
xml_t xml_tree_parse(const char *xml)
{                               // parse an in-memory string, null terminated
   void er(const char *filename, int line, int character, const char *error, const unsigned char *posn) {
      warnx("XML parse error in %s line %d:%d (%s) %s", filename ? : "stdin", line, character, error, posn);
   }
   return xml_parse(NULL, (const unsigned char *) xml, er);
}
#endif

#ifdef EXPAT
static xml_t xml_tree_read_f(FILE * fp, const char *file)
{                               // parse a file stream
   xml_parser_t parser = {
      0
   };
   parser.tree = xml_tree_new(NULL)->tree;
   if (!fp)
      errx(1, "Bad FP");
   parser.current_file = file ? savestring(file, strlen(file), parser.tree) : NULL;
   XML_Parser xml_parser = XML_ParserCreate(0);
   parser.parser = xml_parser;
   XML_SetUserData(xml_parser, &parser);
   XML_SetElementHandler(xml_parser, parse_element_start, parse_element_end);
   XML_SetCharacterDataHandler(xml_parser, parse_character_data);
   XML_SetProcessingInstructionHandler(xml_parser, parse_pi);
   int nextline = 1;            // This is what expat thinks the next line number is  
   char *buf = NULL;
   size_t bufspace = 0;
   ssize_t len = 0;
   while ((len = getline(&buf, &bufspace, fp)) > 0)
   {
#ifdef	CPP
      if (len > 2 && buf[0] == '#' && buf[1] == ' ')    // cpp line directive
      {
         char *p,
         *q;
         int n = strtol(buf + 2, &p, 10);
         if (p != buf + 2)
         {
            while (isspace(*p))
               p++;
            if (*p == '"')
            {
               q = strrchr(p + 1, '"');
               if (q)
                  parser.current_file = savestring(p + 1, q - p - 1, parser.tree);
            }
            parser.line_offset = n - nextline;
            continue;
         }
      }
#endif
      if (nextline == 1 && len == 1 && *buf == '\n')
      {
         // ignore initial blank lines
         parser.line_offset++;
         continue;
      }
      if (!XML_Parse(xml_parser, buf, len, 0))
      {
         const char *s1 = parser.current_file ? " in " : "";
         const char *s2 = parser.current_file ? parser.current_file : "";
         warnx("Parse failed at %d:%d%s%s %s", (int) XML_GetCurrentLineNumber(xml_parser) + parser.line_offset, (int) XML_GetCurrentColumnNumber(xml_parser), s1, s2, XML_ErrorString(XML_GetErrorCode(xml_parser)));
         xml_tree_delete(parser.tree->root);
         parser.tree = NULL;
         break;
      }
      nextline++;
   }
   if (buf)
      free(buf);
   if (parser.tree && !feof(fp))
   {
      warnx("Parse failed reading");
      xml_tree_delete(parser.tree->root);
      parser.tree = NULL;
   }
   if (parser.tree && !XML_Parse(xml_parser, 0, 0, 1))
   {
      xml_tree_delete(parser.tree->root);
      parser.tree = NULL;
   }
   XML_ParserFree(xml_parser);
   if (!parser.tree)
      return NULL;
   return parser.tree->root;
}
#else
static xml_t xml_tree_read_f(FILE * i, const char *file)
{
   char *xml = NULL;
   size_t len = 0;
   FILE *o = open_memstream(&xml, &len);
   char buf[10240];
   size_t l;
   while ((l = fread(buf, 1, sizeof(buf), i)) > 0)
      fwrite(buf, l, 1, o);
   fputc(0, o);
   fclose(o);
   void er(const char *filename, int line, int character, const char *error, const unsigned char *posn) {
      warnx("XML parse error in %s line %d:%d (%s)", filename ? : "stdin", line, character, error);
      if (posn)
         warnx("%02X %02X %02X %02X %02X", posn[0], posn[1], posn[2], posn[3], posn[4]);
   }
   xml_t e = xml_parse(file, (const unsigned char *) xml, &er);
   free(xml);
   return e;
}
#endif

xml_t xml_tree_read_json(FILE * fp, const char *rootname)
{
   char *mem = NULL;
   char buf[10240];
   int a = 0,
       p = 0;
   int l;
   while ((l = read(fileno(fp), buf, sizeof(buf))) > 0)
   {
      if (p + l + 1 > a)
      {
         a = p + l + 1;
         mem = realloc(mem, a);
         if (!mem)
            errx(1, "malloc at line %d", __LINE__);
      }
      memmove(mem + p, buf, l);
      p += l;
   }
   if (!a)
      return NULL;
   mem[p] = 0;
   xml_t t = xml_tree_parse_json(mem, rootname);
   free(mem);
   return t;
}

xml_t xml_tree_read(FILE * fp)
{
   return xml_tree_read_f(fp, NULL);
}

xml_t xml_tree_read_file_json(const char *filename)
{                               // Parse a file by name
   FILE *fp = fopen(filename, "r");
   if (!fp)
      return NULL;
   const char *l = strrchr(filename, '/');
   if (l)
      l++;
   else
      l = filename;
   xml_t tree = xml_tree_read_json(fp, l);
   fclose(fp);
   return tree;
}

xml_t xml_tree_read_file(const char *filename)
{                               // Parse a file by name
   FILE *fp = fopen(filename, "r");
   if (!fp)
      return NULL;
   xml_t tree = xml_tree_read_f(fp, filename);
   fclose(fp);
   return tree;
}

xml_pi_t xml_pi_next(xml_t parent, xml_pi_t prev)
{
   if (!parent)
      errx(1, "Null tree (xml_pi_next)");
   if (prev)
      prev = prev->next;
   else
      prev = parent->tree->first_pi;
   return prev;
}

xml_pi_t xml_pi_add_l(xml_t t, int namel, const char *name, int contentl, const char *content)
{
   if (!t)
      errx(1, "Null tree (xml_pi_add)");
   if (!name)
      errx(1, "Null PI name (xml_pi_add)");
   if (!content)
      errx(1, "Null PI content (xml_pi_add)");
   xml_pi_t p = xml_alloc(sizeof(*p));
   p->tree = t->tree;
   p->name = xml_dup_l(namel, name);
   p->content = xml_dup_l(contentl, content);
   if (t->tree->first_pi)
      t->tree->last_pi->next = p;
   else
      t->tree->first_pi = p;
   p->prev = t->tree->last_pi;
   t->tree->last_pi = p;
   return p;
}

xml_pi_t xml_pi_add(xml_t t, const char *name, const char *content)
{
   if (!name || !content)
      return NULL;
   return xml_pi_add_l(t, strlen(name ? : ""), name, strlen(content ? : ""), content);
}

void xml_element_set_namespace(xml_t e, xml_namespace_t ns)
{
   if (!e)
      errx(1, "Null element (xml_element_set_namespace)");
   e->namespace = ns;
}

void xml_element_set_name(xml_t e, const char *name)
{                               // Set element name, prefix * to indicate json_single.
   if (!e)
      errx(1, "Null element (xml_element_set_name)");
   if (*name == '*')
   {
      name++;
      e->json_single = 1;
   } else
      e->json_single = 0;
   if (!*name)
      return;
   xml_free(e->name);
   e->name = xml_dup(name);
}

void xml_element_set_content_l(xml_t e, int contentl, const char *content)
{
   if (!e)
      errx(1, "Null element (xml_element_set_content)");
   xml_free(e->content);
   e->content = xml_dup_l(contentl, content);
}

void xml_element_set_content(xml_t e, const char *content)
{
   if (content)
      xml_element_set_content_l(e, strlen(content), content);
   else
      xml_element_set_content_l(e, 0, NULL);
}

// Formatted print in to growing string supporting most printf options
// Returns malloced string
// T    Time in XSD format UTC.  #T is localtime with no timezone specifier
// B    Boolean (true/false)

char *xml_vsprintf(const char *f, va_list ap)
{                               // Formatted print
   char *output = NULL;
   int ptr = 0;
   int len = 0;
   while (*f)
   {
      // check enough space for anything (except string expansion which is handled separately)...
      if (len + 100 >= ptr && !(output = realloc(output, len += 1000)))
         errx(1, "malloc at line %d", __LINE__);
      if (*f != '%')
      {
         output[ptr++] = *f++;
         continue;
      }
      // formatting  
      const char *base = f++;
      char flagalt = 0;
      char flagleft = 0;
      char flagplus = 0;
      char flaglong = 0;
      char flaglonglong = 0;
#if 0
      char flagcomma = 0;
      char flagspace = 0;
      char flagzero = 0;
      char flagaltint = 0;
#endif
      int width = 0;
      int precision = 0;
      // format modifiers
      while (*f)
      {
         if (*f == '#')
            flagalt = 1;
         else if (*f == '-')
            flagleft = 1;
         else if (*f == '+')
            flagplus = 1;
#if 0
         else if (*f == ' ')
            flagspace = 1;
         else if (*f == '\'')
            flagcomma = 1;
         else if (*f == 'I')
            flagaltint = 1;
         else if (*f == '0')
            flagzero = 1;
#endif
         else
            break;
         f++;
      }
      // width
      if (*f == '*')
      {
         width = -1;
         f++;
      } else
         while (isdigit(*f))
            width = width * 10 + (*f++) - '0';
      if (*f == '.')
      {                         // precision
         f++;
         if (*f == '*')
         {
            precision = -1;
            f++;
         } else
            while (isdigit(*f))
               precision = precision * 10 + (*f++) - '0';
      }
      // length modifier
      if (*f == 'h' && f[1] == 'h')
         f += 2;
      else if (*f == 'l' && f[1] == 'l')
      {
         flaglonglong = 1;
         f += 2;
      } else if (*f == 'l' || *f == 'L')
      {
         flaglong = 1;
         f++;
      } else if (strchr("hLqjzt", *f))
         f++;
      if (*f == '%')
      {                         // literal!
         output[ptr++] = *f++;
         continue;
      }

      if (!strchr("diouxXeEfFgGaAcsCSpnmTB", *f) || f - base > 20)
      {                         // cannot handle, output as is
         while (base < f)
            output[ptr++] = *base++;
         continue;
      }
      char fmt[22];
      memmove(fmt, base, f - base + 1);
      fmt[f - base + 1] = 0;
      if (strchr("sTB", *f))
      {                         // our formatting
         if (width < 0)
            width = va_arg(ap, int);
         if (precision < 0)
            precision = va_arg(ap, int);
         switch (*f)
         {
         case 's':             // string
            {
               char *a = va_arg(ap, char *);
               if (!a)
                  break;
               int l = strlen(a),
                   p = l;
               if (width && l < width)
                  l = width;
               if (precision && precision < p)
                  p = precision;
               if (len + l * 3 + 100 >= ptr && !(output = realloc(output, len += l * 3 + 1000)))
                  errx(1, "malloc at line %d", __LINE__);
               if (width && !flagleft && l < width)
               {                // pre padding
                  while (l < width)
                  {
                     output[ptr++] = ' ';
                     l++;
                  }
               }
               if (flagplus)
               {
                  while (*a && p--)
                  {
                     if (*a == ' ')
                        output[ptr++] = '+';
                     else if (*a < ' ' || (*a & 0x80) || *a == '+' || *a == '%' || *a == '?' || *a == '&')
                     {
                        const char hex[] = "0123456789ABCDEF";
                        output[ptr++] = '%';
                        output[ptr++] = hex[*a >> 4];
                        output[ptr++] = hex[*a & 0xF];
                     } else
                        output[ptr++] = *a;
                     a++;
                  }
               } else
                  while (*a && p--)
                     output[ptr++] = *a++;
               if (width && flagleft && l < width)
               {                // post padding
                  while (l < width)
                  {
                     output[ptr++] = ' ';
                     l++;
                  }
               }
            }
            break;
         case 'T':             // time
            {
               time_t a = va_arg(ap, time_t);
               if (!a)
                  break;
               if (flagalt)
                  ptr += strftime(output + ptr, len - ptr, "%Y-%m-%dT%H:%M:%S", localtime(&a));
               else
                  ptr += strftime(output + ptr, len - ptr, "%Y-%m-%dT%H:%M:%SZ", gmtime(&a));
            }
            break;
         case 'B':             // bool
            {
               long long a;
               if (flaglong)
                  a = va_arg(ap, long);
               else if (flaglonglong)
                  a = va_arg(ap, long long);
               else
                  a = va_arg(ap, int);
               ptr += sprintf(output + ptr, a ? "true" : "false");
            }
            break;
         }
      } else
      {
#ifdef NONPORTABLE
         ptr += vsnprintf(output + ptr, len - ptr, fmt, ap);
#else
         va_list xp;
         va_copy(xp, ap);
         ptr += vsnprintf(output + ptr, len - ptr, fmt, xp);
         va_end(xp);
         // move pointer forward
         if (width < 0)
            (void) va_arg(ap, int);
         if (precision < 0)
            (void) va_arg(ap, int);
         if (strchr("diouxXc", *f))
         {                      // int
            if (flaglong)
               (void) va_arg(ap, long);
            else if (flaglonglong)
               (void) va_arg(ap, long long);
            else
               (void) va_arg(ap, int);
         } else if (strchr("eEfFgGaA", *f))
         {
            if (flaglong)
               (void) va_arg(ap, long double);
            else
               (void) va_arg(ap, double);
         } else if (strchr("s", *f))
            (void) va_arg(ap, char *);
         else if (strchr("p", *f))
            (void) va_arg(ap, void *);
#endif
      }
      f++;
   }
   if (output)
   {
      output = realloc(output, ptr + 1);
      output[ptr++] = 0;
   }
   return output;
}

xml_attribute_t xml_attribute_printf_ns(xml_t e, xml_namespace_t namespace, const char *name, const char *format, ...)
{
   if (!name)
      errx(1, "Null name (xml_attribute_printf_ns)");
   if (!e)
      errx(1, "Null element (xml_attribute_printf_ns)");
   // see if exists
   xml_attribute_t a = e->first_attribute;
   while (a && ((a->namespace != namespace && (namespace || a->namespace != e->namespace)) || (name && strcmp(a->name, name))))
      a = a->next;
   va_list ap;
   va_start(ap, format);
   xml_content_t new = xml_dup(xml_vsprintf(format, ap));
   va_end(ap);
   if (a)
      xml_free(a->content);
   else
   {
      // new attribute
      a = xml_alloc(sizeof(*a));
      a->parent = e;
      if (e->first_attribute)
         e->last_attribute->next = a;
      else
         e->first_attribute = a;
      a->prev = e->last_attribute;
      e->last_attribute = a;
      a->name = xml_dup(name);
      if (namespace)
         a->namespace = namespace;
   }
   a->content = new;
   return a;
}

void xml_element_printf_content(xml_t e, const char *format, ...)
{
   if (!e)
      errx(1, "Null element (xml_element_printf_content)");
   va_list ap;
   va_start(ap, format);
   xml_free(e->content);
   e->content = xml_dup(xml_vsprintf(format, ap));
   va_end(ap);
}

// Generic shortcut for generating XML, usually #defined as just X or xml in the app
// Path has elements separated by / characters in an compact xpath style and may end with @attribute
// Path starts / for root relative, else is relative to starting element.
// @attribute can be non final, meaning to delete that attribute at that level regardless of value passed
// @attribute=value can be non final, meaning set value at that point (not containing / or @)
// Objects created as you go, normally in same name space as parent unless prefix: used. Just : means no prefix.
// If object exists then not created again unless + prefix used to force new object
// Object .. means up a level
// Returns final element
// maybe allow [n] suffix for objects some time...
xml_t xml_add(xml_t e, const char *path, const char *value)
{
   if (!path)
      return NULL;
   const char *full = path;
   if (*path == '/')
      e = e->tree->root;        // absolute
   while (*path)
   {
      char add = 0,
          att = 0,
          single = 0,
          unquoted = 0;
      while (*path == '/')
         path++;
      // Special prefixes
      for (; *path; path++)
      {
         if (*path == '+')
            add = 1;
         else if (*path == '@')
            att = 1;
         else if (*path == '*')
            single = 1;
         else if (*path == '-')
            unquoted = 1;
         else
            break;
      }
      char *prefix = NULL;
      const char *name = path;
      const char *p = path;
      while (*p && *p != ':' && *p != '/' && *p != '=' && *p != '@')
         p++;
      if (*p == ':')
      {
         prefix = strndupa(path, p - path);
         name = ++p;
         while (*p && *p != ':' && *p != '/' && *p != '=' && *p != '@')
            p++;
         if (*p == ':')
            errx(1, "Bad path, two namespace prefixes in [%s]", full);
      }
      if (p == name)
         errx(1, "Empty name in path [%s]", full);
      name = strndupa(name, p - name);
      xml_namespace_t ns = e->namespace;
      if (prefix)
      {                         // find prefix in namespace
         if (!*prefix)
            ns = &nullns;
         else
         {
            xml_namespacelist_t l = e->tree->namespacelist;
            while (l && (!l->tag || strcmp(l->tag, prefix)))
               l = l->next;
            if (!l)
               errx(1, "Cannot find prefix [%s:] in [%s]", prefix, full);
            ns = l->namespace;
         }
      }
      if (att)
      {                         // attribute setting
         if (!prefix)
            ns = NULL;          // no prefix means no name space not parent name space
         if (*p == '=')
         {                      // fixed attribute setting in-line
            p++;
            const char *v = p;
            while (*p && *p != '@' && *p != '/')
               p++;
            path = p;
            v = strndupa(v, p - v);
            xml_attribute_t a = xml_attribute_set_ns(e, ns, name, v);
            if (a && unquoted)
               a->json_unquoted = 1;
            continue;
         }
         if (*p == '@' || *p == '/')
         {                      // zapping attribute, in line
            path = p;
            xml_attribute_set_ns(e, ns, name, NULL);
            continue;
         }
         xml_attribute_t a = xml_attribute_set_ns(e, ns, name, value);
         if (a && unquoted)
            a->json_unquoted = 1;
         value = NULL;
         break;
      }
      path = p;
      if (!strcmp(name, ".."))
      {                         // special case for up tree
         if (!e->parent)
            errx(1, ".. path used at root in [%s]", full);
         e = e->parent;
         if (add && !e->parent)
            errx(1, "+.. cannot make second root in [%s]", full);
         if (add)
            e = xml_element_add_ns(e->parent, e->namespace, e->name);
         continue;
      }
      xml_t s = NULL;
      if (!add)
         s = xml_element_next_by_name_ns(e, NULL, ns, name);
      if (!s)
         s = xml_element_add_ns(e, ns, name);
      e = s;
      if (single)
         e->json_single = 1;
      if (unquoted)
         e->json_unquoted = 1;
      continue;
   }
   if (e && value)
      xml_element_set_content(e, value);
   return e;
}

xml_t xml_add_free(xml_t e, const char *path, char *value)
{
	xml_t x=xml_add(e,path,value);
	free(value);
	return x;
}

xml_t xml_addf(xml_t e, const char *path, const char *fmt, ...)
{
   char *value = NULL;
   if (fmt)
   {
      va_list ap;
      va_start(ap, fmt);
      value = xml_vsprintf(fmt, ap);
      va_end(ap);
   }
   xml_t r = xml_add(e, path, value);
   if (value)
      free(value);
   return r;
}

// Generic XML query to get element using a path
// Path much like xml_add, returns NULL if it cannot find any level
// Omitting namespace prefix means matching any namespace
xml_t xml_find(xml_t e, const char *path)
{
   if (!path)
      return NULL;
   if (!e)
      return NULL;
   xml_root_t t = e->tree;
   if (!t)
      return NULL;
   const char *full = path;
   if (*path == '/')
      e = NULL;
   while (*path)
   {
      while (*path == '/')
         path++;
      if (*path == '@')
         errx(1, "Cannot fetch attribute with xml_find");
      char *prefix = NULL;
      const char *name = path;
      const char *p = path;
      while (*p && *p != ':' && *p != '/' && *p != '=' && *p != '@')
         p++;
      if (*p == ':')
      {
         prefix = strndupa(path, p - path);
         name = ++p;
         while (*p && *p != ':' && *p != '/' && *p != '=' && *p != '@')
            p++;
         if (*p == ':')
            errx(1, "Bad path, two namespace prefixes in [%s]", full);
      }
      if (p == name)
         errx(1, "Empty name in path [%s]", full);
      name = strndupa(name, p - name);
      xml_namespace_t ns = NULL;
      if (prefix)
      {                         // find prefix in namespace
         xml_namespacelist_t l = e->tree->namespacelist;
         while (l && (!l->tag || strcmp(l->tag, prefix)))
            l = l->next;
         if (!l)
         {
            return NULL;
         }
         ns = l->namespace;
      }
      if (!strcmp(name, ".."))
      {
         if (!e)
            errx(1, ".. at root in [%s]", full);
         e = e->parent;
      } else if (e)
         e = xml_element_by_name_ns(e, ns, name);
      else
      {                         // root, so just check root name matches
         if (strcmp(xml_element_name(t->root), name))
            return NULL;        // TODO check name space?
         e = t->root;
      }
      if (!e)
         return NULL;
      path = p;
   }
   return e;
}

// Generic XML query to get contents of object or attribute
// Path much like xml_add, returns NULL if it cannot find any level
// Omitting namespace prefix means matching any namespace
char *xml_get(xml_t e, const char *path)
{
   if (!path)
      return NULL;
   if (!e)
      return NULL;
   xml_root_t t = e->tree;
   if (!t)
      return NULL;
   const char *full = path;
   if (*path == '/')
      e = NULL;
   while (*path)
   {
      char att = 0;
      while (*path == '/')
         path++;
      if (*path == '@')
      {
         att = 1;
         path++;
      }
      char *prefix = NULL;
      const char *name = path;
      const char *p = path;
      while (*p && *p != ':' && *p != '/' && *p != '=' && *p != '@')
         p++;
      if (*p == ':')
      {
         prefix = strndupa(path, p - path);
         name = ++p;
         while (*p && *p != ':' && *p != '/' && *p != '=' && *p != '@')
            p++;
         if (*p == ':')
            errx(1, "Bad path, two namespace prefixes in [%s]", full);
      }
      if (p == name)
         errx(1, "Empty name in path [%s]", full);
      name = strndupa(name, p - name);
      xml_namespace_t ns = NULL;
      if (prefix)
      {                         // find prefix in namespace
         xml_namespacelist_t l = e->tree->namespacelist;
         while (l && (!l->tag || strcmp(l->tag, prefix)))
            l = l->next;
         if (!l)
         {
            return NULL;
         }
         ns = l->namespace;
      }
      if (att)
      {                         // we want the attribute value
         if (*p)
            errx(1, "Attribute has to be last in path in [%s]", full);
         xml_attribute_t a = NULL;
         a = xml_attribute_by_name_ns(e, ns, name);
         if (!a)
            return NULL;
         return xml_attribute_content(a);
      }
      if (!strcmp(name, ".."))
      {
         if (!e)
            errx(1, ".. at root in [%s]", full);
         e = e->parent;
      } else if (e)
         e = xml_element_by_name_ns(e, ns, name);
      else
      {                         // root, so just check root name matches
         if (strcmp(xml_element_name(t->root), name))
            return NULL;        // TODO check name space?
         e = t->root;
      }
      if (!e)
         return NULL;
      path = p;
   }
   return xml_element_content(e);
}

// Some general conversion tools
char *xml_number22(char *t, long long v)        // convert number to string requiring 22 characters of space
{
   static char temp[22];
   if (!t)
      t = temp;
   sprintf(t, "%lld", v);
   return t;
}

char *xml_datetime20(char *t, time_t v) // convert time_t to XML datetime requiring 20 characters of space (local time)
{
   static char temp[20];
   if (!t)
      t = temp;
   if (!v || v == -1)           // Technically 0 is valid, but we'll assume not for sanity sake here
      return NULL;
   strftime(t, 20, "%Y-%m-%dT%H:%M:%S", localtime(&v));
   return t;
}

char *xml_datetime21(char *t, time_t v) // convert time_t to XML datetime requiring 21 characters of space
{
   static char temp[21];
   if (!t)
      t = temp;
   if (!v || v == -1)           // Technically 0 is valid, but we'll assume not for sanity sake here
      return NULL;
   strftime(t, 21, "%Y-%m-%dT%H:%M:%SZ", gmtime(&v));
   return t;
}

char *xml_date11(char *t, time_t v)     // convert time_t to XML date requiring 11 characters of space
{
   static char temp[11];
   if (!t)
      t = temp;
   if (!v || v == -1)           // Technically 0 is valid, but we'll assume not for sanity sake here
      return NULL;
   strftime(t, 11, "%Y-%m-%d", localtime(&v));
   return t;
}

char *xml_date14(char *t, time_t v)     // convert time_t to XML date requiring 14 characters of space
{
   static char temp[14];
   if (!t)
      t = temp;
   if (!v || v == -1)           // Technically 0 is valid, but we'll assume not for sanity sake here
      return NULL;
   struct tm *w = localtime(&v);
   strftime(t, 14, "%eth %b %Y", w);
   if (w->tm_mday == 1 || w->tm_mday == 21 || w->tm_mday == 31)
      memmove(t + 2, "st", 2);
   else if (w->tm_mday == 2 || w->tm_mday == 22)
      memmove(t + 2, "nd", 2);
   else if (w->tm_mday == 3 || w->tm_mday == 23)
      memmove(t + 2, "rd", 2);
   return t;
}

time_t xml_timez(const char *t, int z)  // convert xml time to time_t
{                               // Can do HH:MM:SS, or YYYY-MM-DD or YYYY-MM-DD HH:MM:SS, 0 for invalid
   if (!t)
      return 0;
   unsigned int Y = 0,
       M = 0,
       D = 0,
       h = 0,
       m = 0,
       s = 0;
   int hms(void) {
      while (isdigit(*t))
         h = h * 10 + *t++ - '0';
      if (*t++ != ':')
         return 0;
      while (isdigit(*t))
         m = m * 10 + *t++ - '0';
      if (*t++ != ':')
         return 0;
      while (isdigit(*t))
         s = s * 10 + *t++ - '0';
      if (*t == '.')
      {                         // fractions
         t++;
         while (isdigit(*t))
            t++;
      }
      return 1;
   }
   if (isdigit(t[0]) && isdigit(t[1]) && t[2] == ':')
   {
      if (!hms())
         return 0;
      return h * 3600 + m * 60 + s;
   } else
   {
      while (isdigit(*t))
         Y = Y * 10 + *t++ - '0';
      if (*t++ != '-')
         return 0;
      while (isdigit(*t))
         M = M * 10 + *t++ - '0';
      if (*t++ != '-')
         return 0;
      while (isdigit(*t))
         D = D * 10 + *t++ - '0';
      if (*t == 'T' || *t == ' ')
      {                         // time
         t++;
         if (!hms())
            return 0;
      }
   }
   if (!Y && !M && !D)
      return 0;
   struct tm tm = {
    tm_year: Y - 1900, tm_mon: M - 1, tm_mday: D, tm_hour: h, tm_min: m, tm_sec:s
   };
   if (*t == 'Z' || z)
      return timegm(&tm);       // UTC
   if (*t == '+' || *t == '-')
   {                            // Explicit time zone
      int Z = 0;
      char s = *t++;
      if (isdigit(t[0]) && isdigit(t[1]))
      {                         // Hours
         Z = ((t[0] - '0') * 10 + t[1] - '0') * 3600;
         t += 2;
         if (*t == ':')
            t++;
         if (isdigit(t[0]) && isdigit(t[1]))
         {                      // Minutes
            Z += ((t[0] - '0') * 10 + t[1] - '0') * 60;
            t += 2;
         }
         if (s == '-')
            Z = 0 - Z;
      }
      return timegm(&tm) - Z;
   }
   tm.tm_isdst = -1;            // work it out
   return timelocal(&tm);       // Local time
}

size_t xml_based(const char *src, unsigned char **buf, const char *alphabet, unsigned int bits)
{                               // Base16/32/64 string to binary
   if (!buf || !src)
      return -1;
   *buf = NULL;
   int b = 0,
       v = 0;
   size_t len = 0;
   FILE *out = open_memstream((char **) buf, &len);
   while (*src && *src != '=')
   {
      char *q = strchr(alphabet, bits < 6 ? toupper(*src) : *src);
      if (!q)
      {                         // Bad character
         if (isspace(*src) || *src == '\r' || *src == '\n')
         {
            src++;
            continue;
         }
         if (*buf)
            free(*buf);
         return -1;
      }
      v = (v << bits) + (q - alphabet);
      b += bits;
      src++;
      if (b >= 8)
      {                         // output byte
         b -= 8;
         fputc(v >> b, out);
      }
   }
   fclose(out);
   return len;
}

char *xml_baseN(size_t slen, const unsigned char *src, size_t dmax, char *dst, const char *alphabet, unsigned int bits)
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

xml_t xml_curl(void *curlv, const char *soapaction, xml_t input, const char *url, ...)
{                               // Post (if tree supplied) or Get a URL and collect response
   xml_t output = NULL;
   CURL *curl = curlv;
   if (!curl)
      curl = curl_easy_init();
   // response file
   char *reply = NULL,
       *request = NULL;
   size_t replylen = 0,
       requestlen = 0;
   FILE *o = open_memstream(&reply, &replylen);
   char *fullurl = NULL;
   va_list ap;
   va_start(ap, url);
   if (vasprintf(&fullurl, url, ap) < 0)
      errx(1, "malloc at line %d", __LINE__);
   va_end(ap);
   curl_easy_setopt(curl, CURLOPT_URL, fullurl);
   curl_easy_setopt(curl, CURLOPT_WRITEDATA, o);
   struct curl_slist *headers = NULL;
   if (input)
   {                            // posting XML input
      headers = curl_slist_append(headers, "Content-Type: text/xml");   // posting XML
      if (soapaction)
      {
         char *sa;
         if (asprintf(&sa, "SOAPAction: %s", soapaction) < 0)
            errx(1, "malloc at line %d", __LINE__);
         headers = curl_slist_append(headers, sa);
         free(sa);
      }
      headers = curl_slist_append(headers, "Expect:");
      curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
      curl_easy_setopt(curl, CURLOPT_POST, 1L);
      FILE *i = open_memstream(&request, &requestlen);
      xml_element_write(i, input, 1, 1);
      fclose(i);
      curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request);
      curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long) requestlen);
   }                            // if not input, then assume a GET or args preset before call
   CURLcode result = curl_easy_perform(curl);
   fclose(o);
   // Put back to GET as default
   curl_easy_setopt(curl, CURLOPT_HTTPHEADER, NULL);
   curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
   free(fullurl);
   if (headers)
      curl_slist_free_all(headers);
   if (request)
      free(request);
   if (result)
   {
      if (reply)
         free(reply);
      if (!curlv)
         curl_easy_cleanup(curl);
      return NULL;              // not a normal result
   }
   if (replylen)
   {
      output = xml_tree_parse(reply);
      if (!output)
         output = xml_tree_parse_json(reply, "json");   // Fall back to JSON
      if (!output)
         fprintf(stderr, "Failed: %s\n", reply);
      free(reply);
   }
   if (!curlv)
      curl_easy_cleanup(curl);
   return output;
}

#ifdef	CURLCB
void xml_curl_cb(void *curlv, xml_callback_t * cb, const char *soapaction, xml_t input, const char *url, ...)
{                               // Post (if tree supplied) or Get a URL and collect responses - using callback for each complete response as arrives
   // Note this does not yet handle PI at start, expects sequence of XML objects only, calling back for each as received
   CURL *curl = curlv;
   if (!curl)
   {
      curl = curl_easy_init();
      curl_easy_setopt(curl, CURLOPT_TIMEOUT, 3600L);   // Assume a hanging get
   }
#ifdef	EXPAT
   xml_parser_t parser = {
   };
   parser.callback = cb;
   XML_Parser xml_parser = XML_ParserCreate(0);
   parser.parser = xml_parser;
   XML_SetUserData(xml_parser, &parser);
   XML_SetElementHandler(xml_parser, parse_element_start, parse_element_end);
   XML_SetCharacterDataHandler(xml_parser, parse_character_data);
   XML_SetProcessingInstructionHandler(xml_parser, parse_pi);
   XML_Parse(xml_parser, "<xml>", 5, 0);        // wrap multiple instances - messy
   xml_tree_delete(parser.tree->root);
   parser.tree = NULL;
   parser.here = NULL;
   int start = 1;
   size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata) {
      size_t len = size * nmemb;
      if (start && len > 2 && *ptr == '<' && ptr[1] == '?')
      {                         // Lets hope whole declaration in one block - this is a bodge
         while (len-- && *ptr++ != '>');
      }
      start = 0;
      if (!XML_Parse(xml_parser, ptr, len, 0))
      {
         warnx("Parse failed at %d:%d %s", (int) XML_GetCurrentLineNumber(xml_parser) + parser.line_offset, (int) XML_GetCurrentColumnNumber(xml_parser), XML_ErrorString(XML_GetErrorCode(xml_parser)));
         if (parser.tree)
            xml_tree_delete(parser.tree->root);
         parser.tree = NULL;
      }
      return size * nmemb;
   }
#endif
   char *request = NULL;
   size_t requestlen = 0;
   char *fullurl = NULL;
   va_list ap;
   va_start(ap, url);
   if (vasprintf(&fullurl, url, ap) < 0)
      errx(1, "malloc at line %d", __LINE__);
   va_end(ap);
   curl_easy_setopt(curl, CURLOPT_URL, fullurl);
#ifdef	EXPAT
   curl_easy_setopt(curl, CURLOPT_WRITEDATA, &parser);
   curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
#else
   errx(1, "Change to curl in to memory and parse");
#endif
   struct curl_slist *headers = NULL;
   if (input)
   {                            // posting XML input
      headers = curl_slist_append(headers, "Content-Type: text/xml");   // posting XML
      if (soapaction)
      {
         char *sa;
         if (asprintf(&sa, "SOAPAction: %s", soapaction) < 0)
            errx(1, "malloc at line %d", __LINE__);
         headers = curl_slist_append(headers, sa);
         free(sa);
      }
      headers = curl_slist_append(headers, "Expect:");
      curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
      curl_easy_setopt(curl, CURLOPT_POST, 1L);
      FILE *i = open_memstream(&request, &requestlen);
      xml_element_write(i, input, 1, 1);
      fclose(i);
      curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request);
      curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long) requestlen);
   }                            // if not input, then assume a GET or args preset before call
   CURLcode result = curl_easy_perform(curl);
#ifdef	EXPAT
   if (parser.tree)
   {
      xml_tree_delete(parser.tree->root);
      parser.tree = NULL;
      warnx("Parse incomplete");
   }
   XML_ParserFree(xml_parser);
#else
   errx(1, "Finish curl");
#error CURL CB not donw
#endif
   // Put back to GET as default
   curl_easy_setopt(curl, CURLOPT_HTTPHEADER, NULL);
   curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
   free(fullurl);
   if (headers)
      curl_slist_free_all(headers);
   if (request)
      free(request);
   if (!curlv)
      curl_easy_cleanup(curl);
   result = result;
}
#endif

void xml_log(int debug, const char *who, const char *what, xml_t tx, xml_t rx)
{
   void log(xml_t t) {          // stderr log, so buffered...
      char *mem = NULL;
      size_t memlen = 0;
      FILE *memfile = open_memstream(&mem, &memlen);
      xml_write(memfile, t);
      fflush(memfile);
      if (write(fileno(stderr), mem, memlen) != (int) memlen)
         err(1, "write");
      free(mem);
   }
   if (!tx && !rx)
      return;
   char *norm(char *x) {
      char *p;
      for (p = x; *p; p++)
         if (!isalnum(*p))
            *p = '_';
      return x;
   }
   umask(0);
   char path[1000] = "",
       *p = path,
       *q;
   struct timeval tv;
   struct timezone tz;
   gettimeofday(&tv, &tz);
   p += strftime(path, sizeof(path) - 1, "/var/log/xml/%Y/%m/%d/%H%M%S", gmtime(&tv.tv_sec));
   p += snprintf(p, path + sizeof(path) - p, "%06lu-%s-%s", (long) tv.tv_usec, norm(strdupa(who ? : "-")), norm(strdupa(what ? : "-")));
   for (q = path + 12; q; q = strchr(q + 1, '/'))
   {
      *q = 0;
      if (access(path, W_OK) && mkdir(path, 0777) && access(path, W_OK))
         err(1, "Cannot make log path %s", path);
      *q = '/';
   }
   if (tx)
   {
      sprintf(p, ".tx");
      FILE *o = fopen(path, "w");
      if (!o)
         err(1, "Cannot make log file %s", path);
      xml_write(o, tx);
      fclose(o);
      if (debug)
         log(tx);
   }
   if (rx)
   {
      sprintf(p, ".rx");
      FILE *o = fopen(path, "w");
      if (!o)
         err(1, "Cannot make log file %s", path);
      xml_write(o, rx);
      fclose(o);
      if (debug)
         log(rx);
   }
}

#ifdef MAIN
#define	X	xml_add
int main(int argc, char *argv[])
{
   int a,
    json = 0,
       jsonout = 0,
       pretty = 0,
       tidy = 0;
   const char *attr = NULL;
   // TODO change to popt
   for (a = 1; a < argc; a++)
   {
      if (!strncmp(argv[a], "--attr=", 7))
      {                         // Special attribute to control formatting
         attr = argv[a] + 7;
         continue;
      }
      if (!strcmp(argv[a], "-j") || !strcmp(argv[a], "--json"))
      {
         json = 1;
         continue;
      }
      if (!strcmp(argv[a], "-J") || !strcmp(argv[a], "--json-out"))
      {
         jsonout = 1;
         continue;
      }
      if (!strcmp(argv[a], "-f") || !strcmp(argv[a], "--pretty"))
      {
         pretty = 1;
         continue;
      }
      if (!strcmp(argv[a], "-t") || !strcmp(argv[a], "--tidy"))
      {
         tidy = 1;
         continue;
      }
      xml_t t = NULL;
      if (!strcmp(argv[a], "-"))
      {
         if (json)
            t = xml_tree_read_json(stdin, "json");
         else
            t = xml_tree_read(stdin);
      } else
      {
#ifdef	CURLCB
         void cb(xml_t block) {
            if (!block)
               errx(1, "NULL tree");
            printf("Tree received...\n");
            xml_element_write(stdout, block, 1, pretty ? 0 : 1);
            printf("\n");
         }
#endif
         if (!strncasecmp(argv[a], "http://", 6))
#ifdef	CURLCB
            xml_curl_cb(NULL, &cb, NULL, NULL, "%s", argv[a]);
#else
            errx(1, "No curl CB");
#endif
         else if (json)
            t = xml_tree_read_file_json(argv[a]);
         else
            t = xml_tree_read_file(argv[a]);
      }
      if (!t)
         fprintf(stderr, "No tree %s (%s)\n", argv[a], json ? "json" : "xml");
      else
      {
         void scan(xml_t t) {   // Scan for special attribute
            xml_attribute_t a = xml_attribute_by_name(t, attr);
            if (a)
            {
               char *v = xml_attribute_content(a);
               if (!strcasecmp(v, "flat"))
                  t->json_single = 1;   // Not an array
               xml_attribute_set(t, attr, NULL);        // zap it
            }
            xml_t x = NULL;
            while ((x = xml_element_next(t, x)))
               scan(x);
         }
         if (attr)
            scan(t);
         if (jsonout)
            xml_element_write_json(stdout, t);
         else
            xml_element_write(stdout, t, 1, tidy ? -1 : pretty ? 0 : 1);
         xml_tree_delete(t);
      }
   }
   return 0;
}
#endif
