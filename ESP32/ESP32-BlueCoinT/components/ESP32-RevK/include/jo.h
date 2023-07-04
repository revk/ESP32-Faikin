// Lightweight JSON syntax management tool kit
// This toolkit works solely at a syntax level, and does not all management of whole JSON object hierarchy

#include <stddef.h>
#include <unistd.h>

// Types

typedef struct jo_s *jo_t;      // The JSON cursor used by all calls
typedef enum {                  // The parse data value type we are at
   JO_END,                      // Not a value, we are at the end, or in an error state
   // JO_END always at start
   JO_CLOSE,                    // at close of object or array
   // After JO_END
   JO_TAG,                      // at a tag
   JO_OBJECT,                   // value is the '{' of an object, jo_next() goes in to object if it has things in it
   JO_ARRAY,                    // value is the '[' of an array, jo_next() goes in to array if it has things in it
   JO_STRING,                   // value is the '"' of a string
   JO_NUMBER,                   // value is start of an number
   // Can test >= JO_NULL as test for literal
   JO_NULL,                     // value is the 'n' in a null
   // Can test >= JO_TRUE as test for bool
   JO_TRUE,                     // value is the 't' in true, can compare >= JO_TRUE for boolean
   JO_FALSE,                    // value if the 'f' in false
} jo_type_t;

const char *jo_debug(jo_t j);   // Debug string

// Setting up

jo_t jo_parse_str(const char *buf);
// Start parsing a null terminated JSON object string

jo_t jo_parse_query(const char *buf);
// Parse a query string format into a JSON object string (allocated)

jo_t jo_parse_mem(const void *buf, size_t len);
// Start parsing a JSON string in memory - does not need a null

jo_t jo_create_mem(void *buf, size_t len);
// Start creating JSON in memory at buf, max space len.

jo_t jo_create_alloc(void);
// Start creating JSON in memory, allocating space as needed.

jo_t jo_object_alloc(void);
// As so common, this does jo_create_alloc(), and jo_object()

jo_t jo_pad(jo_t *, int);
// Attempt to ensure padding on jo, else free jo and return NULL

jo_t jo_copy(jo_t);
// Copy object - copies the object, and if allocating memory, makes copy of the allocated memory too

const char *jo_rewind(jo_t);
// Move to start for parsing. If was writing, closed and set up to read instead. Clears error if reading. Safe to call with NULL
// If safe with terminating NULL then returns pointer to the complete JSON

int jo_level(jo_t);
// Current level, 0 being the top level

const char *jo_error(jo_t, int *pos);
// Return NULL if no error, else returns an error string.
// Note, for creating JSON, this reports and error if not enough space to finish creating
// If pos is set then the offset in to the JSON is retported

void jo_free(jo_t *);
// Free jo_t and any allocated memory (safe to call with NULL or pointer to NULL)

int jo_isalloc(jo_t);
// If it is allocated so finisha can be used

char *jo_finish(jo_t *);
// Finish creating static JSON, return start of static JSON if no error. Frees j. It is an error to use with jo_create_alloc

char *jo_finisha(jo_t *);
// Finish creating allocated JSON, returns start of alloc'd memory if no error. Frees j. If NULL returned then any allocated space also freed
// It is an error to use with non jo_create_alloc

int jo_len(jo_t);	// Current length of object (including any closing needed)

// Creating
// Note that tag is required if in an object and must be null if not

void jo_array(jo_t, const char *tag);
// Start an array

void jo_object(jo_t, const char *tag);
// Start an object

void jo_close(jo_t);
// Close current array or object

void jo_json(jo_t j,const char *tag,jo_t json);
// Add a json sub object

void jo_stringn(jo_t, const char *tag, const char *string, ssize_t len);
#define jo_string(j,t,s) jo_stringn(j,t,s,-1)
// Add a string

void jo_stringf(jo_t, const char *tag, const char *format, ...);
// Add a string (formatted)

void jo_lit(jo_t, const char *tag, const char *lit);
void jo_litf(jo_t, const char *tag, const char *format, ...);
// Add a literal string (formatted) - caller is expected to meet JSON rules - used typically for numeric values

void jo_datetime(jo_t j, const char *tag, time_t t);
// Add a datetime (ISO, Z)

extern const char JO_BASE64[];
extern const char JO_BASE32[];
extern const char JO_BASE16[];

void jo_baseN(jo_t j, const char *tag, const void *src, size_t slen, uint8_t bits, const char *alphabet);
// Store string in different bases
#define	jo_base64(j,t,m,l) jo_baseN(j,t,m,l,6,JO_BASE64)
#define	jo_base32(j,t,m,l) jo_baseN(j,t,m,l,5,JO_BASE32)
#define	jo_base16(j,t,m,l) jo_baseN(j,t,m,l,4,JO_BASE16)

ssize_t jo_strncpyd(jo_t j, void *dst, size_t dlen, uint8_t bits, const char *alphabet);
#define	jo_strncpy64(j,d,dl) jo_strncpyd(j,d,dl,6,JO_BASE64)
#define	jo_strncpy32(j,d,dl) jo_strncpyd(j,d,dl,5,JO_BASE32)
#define	jo_strncpy16(j,d,dl) jo_strncpyd(j,d,dl,4,JO_BASE16)

void jo_int(jo_t, const char *tag, int64_t);
// Add an integer

void jo_bool(jo_t, const char *tag, int);
// Add a bool (true if non zero passed)

void jo_null(jo_t, const char *tag);
// Add a null

// Parsing

jo_type_t jo_here(jo_t);
// Here returns where we are in the parse

jo_type_t jo_next(jo_t);
// Move to next element we can parse (validating what we move over as we go)

jo_type_t jo_skip(jo_t);
// Skip this value to next value AT THE SAME LEVEL, typically used where a tag is not what you are looking for, etc

jo_type_t jo_find(jo_t,const char *);
// Rewind and look for path, e.g. tag.tag... and return type of value for that point. Does not do arrays, yet. JO_END for no find

ssize_t jo_strlen(jo_t);
// Return byte length, if a string or tag this is the decoded byte length, else length of literal

ssize_t jo_strncpy(jo_t, void *, size_t max);
// Copy from current point to a string. If a string or a tag, remove quotes and decode/deescape - max is size of target in to which a null is always added

ssize_t jo_strncmp(jo_t, void *, size_t max);
// Compare from current point to a string. If a string or a tag, remove quotes and decode/deescape

#define jo_strcmp(j,s) jo_strncmp(j,s,strlen(s))

// Allocate a copy of string
char *jo_strdup(jo_t);

// Get a number
int64_t jo_read_int(jo_t);
long double jo_read_float(jo_t);

// Get a datetime
time_t jo_read_datetime(jo_t);
