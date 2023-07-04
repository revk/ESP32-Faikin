// ==========================================================================
//
//                       Adrian's XML Library - axl.h
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

#ifndef AXL_H
#define	AXL_H
// Parser, object management and output

// Requires -lexpat -lcurl if not axllight

// Types

typedef char *xml_name_t;
typedef char *xml_content_t;

typedef struct xml_namespace_s *xml_namespace_t;
typedef struct xml_namespacelist_s *xml_namespacelist_t;
typedef struct xml_pi_s *xml_pi_t;
typedef struct xml_attribute_s *xml_attribute_t;
typedef struct xml_root_s *xml_root_t;
typedef struct xml_s *xml_t;
typedef struct xml_stringlist_s *xml_stringlist_t;

struct xml_stringlist_s {
   xml_stringlist_t next;
   char *string;
   int length;
};

struct xml_namespace_s {
   int count;                   // Count of usage (i.e. element or explicit attribute)
   int level;                   // Level of common parent
   xml_t parent;                // Common parent - top level where namespace used
   unsigned char fixed:1;       // Fixed tag
   unsigned char nsroot:1;      // Root level XML
   unsigned char always:1;      // Include even if seemingly not used
   char *outputtag;
   char uri[];
};

struct xml_namespacelist_s {
   xml_namespacelist_t next;
   xml_namespace_t namespace;
   char *tag;
};

struct xml_pi_s {
   xml_pi_t next,
    prev;
   xml_root_t tree;
   xml_name_t name;
   xml_content_t content;
};

struct xml_attribute_s {
   xml_t parent;
   xml_attribute_t prev,
    next;
   unsigned char json_unquoted:1;       // not quoted in json
   xml_namespace_t namespace;
   xml_name_t name;
   xml_content_t content;
};

struct xml_root_s {
   xml_t root;
   xml_namespacelist_t namespacelist,
    namespacelist_end;
   xml_pi_t first_pi,
    last_pi;
   char *encoding;
   xml_stringlist_t strings;
};

struct xml_s {
   xml_t parent;
   xml_t prev,
    next;
   xml_t first_child,
    last_child;
   xml_attribute_t first_attribute,
    last_attribute;
   xml_namespace_t namespace;
   xml_name_t name;
   xml_content_t content;
   xml_root_t tree;
   int line;
   const char *filename;
   unsigned char json_single:1; // Output in JSON not as an array - i.e. only ever one instance of this object
   unsigned char json_unquoted:1;       // content not quoted in json
};

// Functions

#define	xml_tree_encoding(x)		((x)->tree->encoding)
#define	xml_element_namespace(x)	((x)->namespace)
#define	xml_element_name(x)		((x)->name)
#define	xml_element_content(x)		((x)->content)
#define	xml_element_parent(x)		((x)->parent)
#define	xml_attribute_namespace(x)	((x)->namespace)
#define	xml_attribute_name(x)		((x)->name)
#define	xml_attribute_content(x)	((x)->content)
#define	xml_attribute_parent(x)		((x)->parent)
#define	xml_pi_name(x)			((x)->name)
#define	xml_pi_content(x)		((x)->content)
#define	xml_tree_root(x)		((x)->tree->root)
xml_t xml_element_by_name_ns(xml_t e, xml_namespace_t namespace, const char *name);
#define		xml_element_by_name(e,n)	xml_element_by_name_ns(e,NULL,n)
xml_attribute_t xml_attribute_by_name_ns(xml_t e, xml_namespace_t namespace, const char *name);
#define		xml_attribute_by_name(e,n)	xml_attribute_by_name_ns(e,NULL,n)
xml_t xml_element_next(xml_t parent, xml_t prev);
#define		xml_element_next_by_name(p,e,n) xml_element_next_by_name_ns(p,e,NULL,n)
xml_t xml_element_next_by_name_ns(xml_t parent, xml_t prev, xml_namespace_t namespace, const char *name);
xml_attribute_t xml_attribute_next(xml_t e, xml_attribute_t prev);

xml_t xml_element_add_ns_after_l(xml_t parent, xml_namespace_t namespace, int namel, const char *name, xml_t prev);
xml_t xml_element_add_ns_after(xml_t parent, xml_namespace_t namespace, const char *name, xml_t prev);
#define		xml_element_add(p,n)	xml_element_add_ns_after(p,NULL,n,NULL)
#define		xml_element_add_ns(p,ns,n)	xml_element_add_ns_after(p,ns,n,NULL)
xml_t xml_element_attach_after(xml_t parent, xml_t prev, xml_t element);
#define	xml_element_insert(parent,e) xml_element_attach_after(parent,parent,e)  // At start under parent
#define	xml_element_attach(parent,e) xml_element_attach_after(parent,NULL,e)    // At end under parent
#define	xml_element_append(prev,e) xml_element_attach_after(NULL,prev,e)        // After previous entry
xml_attribute_t xml_attribute_printf_ns(xml_t e, xml_namespace_t namespace, const char *name, const char *format, ...);
xml_attribute_t xml_attribute_set_ns_l(xml_t e, xml_namespace_t namespace, int namel, const char *name, int contentl, const char *content);
xml_attribute_t xml_attribute_set_ns(xml_t e, xml_namespace_t namespace, const char *name, const char *content);
#define		xml_attribute_set(e,n,c)	xml_attribute_set_ns(e,NULL,n,c)
void xml_attribute_delete(xml_attribute_t a);
void xml_pi_delete(xml_pi_t p);
xml_t xml_element_delete(xml_t e);
xml_t xml_element_duplicate(xml_t e);
void xml_element_explode(xml_t e);
void xml_element_set_name(xml_t e, const char *name);
void xml_element_set_namespace(xml_t e, xml_namespace_t ns);
void xml_element_set_content_l(xml_t e, int contentl, const char *content);
void xml_element_set_content(xml_t e, const char *content);
void xml_element_printf_content(xml_t e, const char *format, ...);
xml_t xml_tree_new(const char *name);   // Create new tree with dummy root (optionalal naming root)
xml_t xml_tree_add_root_ns(xml_t tree, xml_namespace_t namespace, const char *name);    // Sets root name and namespace and returns root
#define		xml_tree_add_root(t,n)	xml_tree_add_root_ns(t,NULL,n)
xml_namespace_t xml_namespace_l(xml_t tree, int tagl, const char *tag, unsigned int namespacel, const char *namespace);
xml_namespace_t xml_namespace(xml_t t, const char *tag, const char *namespace); // tag prefix *(always) ^(root) :(not-fixed)
xml_t xml_tree_delete(xml_t t); // Returns null
void xml_element_write(FILE * fp, xml_t e, int headers, int pack);      // Write XML
void xml_element_write_json(FILE * fp, xml_t e);        // Write XML as JSON
xml_t xml_element_compress(xml_t e);    // Reduce single text only sub objects to attributes of parent and returns passed argument for use in line
#define xml_tree_compress(t) xml_element_compress(t)
xml_pi_t xml_pi_next(xml_t tree, xml_pi_t prev);
xml_pi_t xml_pi_add_l(xml_t tree, int namel, const char *name, int contentl, const char *content);      // start name with ! to send things like !DOCTYPE
xml_pi_t xml_pi_add(xml_t tree, const char *name, const char *content); // start name with ! to send things like !DOCTYPE
void xml_quoted(FILE *, const char *);

// Top level tree functions
#define		xml_write(f,t)	xml_element_write(f,t,1,0)      // fmemopen to write to memory
#define		xml_write_json(f,t)	xml_element_write_json(f,t)     // fmemopen to write to memory
xml_t xml_tree_parse(const char *xml);
xml_t xml_tree_parse_json(const char *json, const char *rootname);
xml_t xml_tree_read(FILE * fp);
xml_t xml_tree_read_json(FILE * fp, const char *rootname);
xml_t xml_tree_read_file(const char *filename);
xml_t xml_tree_read_file_json(const char *filename);
xml_t xml_curl(void *curl, const char *soapaction, xml_t, const char *url, ...);        // Post XML (if tree supplied) or Get a URL and collect response. curl is expected to be initialised and can be set for posting data using curl_formadd and called with no input. URL can be vsprint. Response can be XML or JSON
typedef void xml_callback_t(xml_t);     // call back
void xml_curl_cb(void *curlv, xml_callback_t * cb, const char *soapaction, xml_t input, const char *url, ...);  // Parse sequence of responses via callback
void xml_log(int debug, const char *who, const char *what, xml_t tx, xml_t rx);

// General conversions common to xml
extern const char BASE16[];
extern const char BASE32[];
extern const char BASE64[];

#define	xml_boolean(v)	((v)?"true":"false")    // boolean to string
#define	xml_number(v)	xml_number22(alloca(22),v)      // convert number and return local stack space for variable
char *xml_number22(char *t, long long v);       // convert number to string requiring 22 characters of space
#define	xml_datetime(v)	xml_datetime21(alloca(21),v)    // convert datetime return local stack space for variable
char *xml_datetime21(char *t, time_t v);        // convert time_t to XML datetime requiring 21 characters of space
#define	xml_datetimelocal(v)	xml_datetime20(alloca(20),v)    // convert datetime return local stack space for variable
char *xml_datetime20(char *t, time_t v);        // convert time_t to XML datetime requiring 20 characters of space (local)
#define	xml_date(v)	xml_date11(alloca(11),v)        // convert datetime return local stack space for variable
char *xml_date11(char *t, time_t v);    // convert time_t to XML date requiring 11 characters of space
#define	xml_datepretty(v)	xml_date14(alloca(14),v)        // convert datetime return local stack space for variable
char *xml_date14(char *t, time_t v);    // convert time_t to XML date requiring 14 characters of space NNth Mmm YYYY
time_t xml_timez(const char *t, int z); // convert xml time to time_t
#define	xml_time(t) xml_timez(t,0)      // Normal XML time, assumes local if no time zone
#define	xml_time_utc(t) xml_timez(t,1)  // Expects time to be UTC even with no Z suffix
char *xml_baseN(size_t, const unsigned char *, size_t, char *, const char *, unsigned int);
#define	xml_base64(len,buf)	xml_base64N(len,buf,((len)+2)/3*4+3,alloca(((len)+2)/3*4+3))
#define xml_base64N(slen,src,dlen,dst) xml_baseN(slen,src,dlen,dst,BASE64,6)
#define	xml_base32(len,buf)	xml_base32N(len,buf,((len)+4)/5*8+3,alloca(((len)+4)/5*8+3))
#define xml_base32N(slen,src,dlen,dst) xml_baseN(slen,src,dlen,dst,BASE32,5)
#define	xml_base16(len,buf)	xml_base16N(len,buf,(len)*2+3,alloca((len)*2+3))
#define xml_base16N(slen,src,dlen,dst) xml_baseN(slen,src,dlen,dst,BASE16,4)
size_t xml_based(const char *src, unsigned char **buf, const char *alphabet, unsigned int bits);
#define xml_base64d(src,dst) xml_based(src,dst,BASE64,6)
#define xml_base32d(src,dst) xml_based(src,dst,BASE32,5)
#define xml_base16d(src,dst) xml_based(src,dst,BASE16,4)

// Shortcuts
#define		xml_c		xml_element_set_content
#define		xml_fc		xml_element_printf_content
#define		xml_a		xml_element_add
#define		xml_atn		xml_attribute_set_ns
#define		xml_at		xml_attribute_set
#define		xml_e		xml_element_by_name

// Generic shortcut for generating XML, usually #defined as just X or xml in the app
// Path has elements separated by / characters in an compact xpath style and may end with @attribute
// Path starts / for root relative, else is relative to starting element.
// @attribute can be non final, meaning to delete that attribute at that level regardless of value passed
// @attribute=value can be non final, meaning set value at that point (not containing / or @)
// Objects created as you go, normally in same name space as parent unless prefix: used. Just : means no prefix.
// If object exists then not created again unless + prefix used to force new object
// Prefix an object * to mark that in JSON it is not an array
// Object .. means up a level
// Returns final element
xml_t xml_add(xml_t, const char *path, const char *value);
xml_t xml_add_free(xml_t, const char *path, char *value); // Frees value
xml_t xml_addf(xml_t e, const char *path, const char *fmt, ...);
// Similar generic path based functions
xml_t xml_find(xml_t e, const char *path);      // return an element using a path
char *xml_get(xml_t, const char *path); // return content of element or attribute using a path (attribute as final part)

#endif                          // AXL_H
