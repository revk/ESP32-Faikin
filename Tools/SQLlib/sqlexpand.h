// SQL variable expansion library
// This tool is specifically to allow $variable expansion in an SQL query in a safe way with correct quoting.
// Copyright ©2022 Andrews & Arnold Ltd, Adrian Kennard
// This software is provided under the terms of the GPL - see LICENSE file for more details

#define	SQLEXPANDPREFIX	"?#,@*%+-={$</\\_"	// Characters allowed after $ to be considered valid for expansion (in addition to isalpha)

// Low level dollar expansion
typedef struct dollar_expand_s dollar_expand_t;

// Parse dollar expansion, sourcep starts after the $ and is moved forward, errp is set for an error or warning, returns control structure
dollar_expand_t *dollar_expand_parse(const char **sourcep,const char **errp);

// Passed the parsed dollar_expand_t, and a pointer to the value, returns processed value, e.g. after applying flags and suffixes, and so on
char *dollar_expand_process(dollar_expand_t*,const char *value,const char **errp,unsigned int flags);

// Frees space created (including any used for return from dollar_expand_process), sets to NULL
void dollar_expand_free(dollar_expand_t**);

const char *dollar_expand_name(dollar_expand_t*); // The extracted variable name
unsigned char dollar_expand_literal(dollar_expand_t*);	// Flags $%
unsigned char dollar_expand_list(dollar_expand_t*);	// Flags $,
unsigned char dollar_expand_query(dollar_expand_t*);	// Flags $?
unsigned char dollar_expand_underscore(dollar_expand_t*);	// Flags $-
unsigned char dollar_expand_quote(dollar_expand_t*);	// Flags quoting needed

// SQL query expansion
typedef char *sqlexpandgetvar_t(const char *);

// If success, returns malloced query string, and sets *errp to NULL
// If success but warning, returns malloced query string, and sets *errp to warning text 
// If failure, returns NULL, and sets *errp to error text
// If $?name is used and variable does not exist return is NULL and error set to NULL if expansion of name fails
char *sqlexpand(const char *query, sqlexpandgetvar_t * getvar, const char **errp, const char **posp,unsigned int flags);

#define	SQLEXPANDSTDIN		1       // Handle $- as stdin
#define	SQLEXPANDFILE		2       // Handle $@ file
#define	SQLEXPANDUNSAFE		4       // Handle $% unsafe expansion
#define	SQLEXPANDPPID 		8       // Handle $$ as parent pid
#define	SQLEXPANDZERO		16      // Handle missing unquoted expansion as a number (0)
#define	SQLEXPANDBLANK		32      // Handle missing expansion as a blank (no error or warning)
