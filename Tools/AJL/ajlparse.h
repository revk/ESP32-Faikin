// ==========================================================================
//
//                       Adrian's JSON Library - ajlparse.h
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

//
// THIS IS PROBABLY NOT THE FILE YOU ARE LOOKING FOR, MOVE ALONG PLEASE, NO DROIDS HERE
//
// This used for low level parsing, and not manipulating JSON objects in memory
// You probably want to look at ajl.h for that

#pragma once
#include <stdio.h>

// This library provides simple means to parse underlying JSON elements, and to output them
// Error handling is by means of setting an error state which stops further processing
// The error string is returned from most functions, and the line/character of the error can be checked

// Types
typedef struct ajl_s *ajl_t;    // The JSON parse control structure
typedef ssize_t ajl_func_t(void *, void *, size_t);     // Read or write functions (like read() or write())

ssize_t ajl_file_read(void *arg, void *buf, size_t l);  // Standard functions
ssize_t ajl_file_write(void *arg, void *buf, size_t l);
ssize_t ajl_fd_read(void *arg, void *buf, size_t l);
ssize_t ajl_fd_write(void *arg, void *buf, size_t l);

// Those functions returning const char * return NULL for OK, else return error message. Once a parse error is found it is latched

// Common functions
const char *ajl_end(const ajl_t);       // Close control structure.
void ajl_delete(ajl_t *);       // Free the handle, sets NULL
const char *ajl_error(const ajl_t);     // Return if error set in JSON object, or NULL if not error

// Allocate control structure for parsing, from file or from memory
ajl_t ajl_text(const char *text);       // Parse start simple text string to null termination - no file handling done
const char *ajl_done(ajl_t * jp);       // Get end of parse from text and free j, sets NULL
ajl_t ajl_read(FILE *);         // Parse start file handle
ajl_t ajl_read_file(const char *filename);      // Parse start read file
ajl_t ajl_read_mem(const char *buffer, ssize_t len);    // Parse start mem region
ajl_t ajl_read_fd(int);         // Start read from file
ajl_t ajl_read_func(ajl_func_t *, void *);      // Read using functions
int ajl_line(const ajl_t);      // Return current line number in source
int ajl_char(const ajl_t);      // Return current character position in source
int ajl_level(const ajl_t);     // return current level of nesting
int ajl_isobject(const ajl_t);  // return non zero if we are within an object and so fields should be tagged
const char *ajl_reset(ajl_t j); // Reset parse

typedef enum {                  // Parse types
   AJL_ERROR,                   // Error is set
   AJL_EOF,                     // End of file reached (at expected point, no error)
   AJL_STRING,
   AJL_NUMBER,
   AJL_BOOLEAN,
   AJL_NULL,
   AJL_CLOSE,                   // End of object or array
   AJL_OBJECT,                  // Start of object
   AJL_ARRAY,                   // Start of array
} ajl_type_t;

// The basic parsing function consumes next element, and returns a type as above
// If the element is within an object, then the tag is parsed and mallocd and stored in tag
// The value of the element is parsed, and malloced and stored in value (a null is appended, not included in len)
// The length of the value is stored in len - this is mainly to allow for strings that contain a null
ajl_type_t ajl_parse(const ajl_t, unsigned char **tag, unsigned char **value, size_t *len);

// Generate

// Allocate control structure for generating, to file or to memory
ajl_t ajl_write(FILE *);
ajl_t ajl_write_file(const char *filename);
ajl_t ajl_write_mem(unsigned char **buffer, size_t *len);
ajl_t ajl_write_fd(int);        // Start read from file
ajl_t ajl_write_func(ajl_func_t *, void *);     // Read using functions
void *ajl_arg(ajl_t);           // Return arg for func - use with care and remember i/o is buffered
void ajl_pretty(const ajl_t);   // Mark for pretty output (i.e. additional whitespace)

const char *ajl_add(const ajl_t, const unsigned char *tag, const unsigned char *value); // Add pre-formatted value (expects quotes, escapes, etc)
const char *ajl_add_string(const ajl_t, const unsigned char *tag, const unsigned char *value);  // Add UTF-8 string, escaped for JSON
const char *ajl_add_literal(const ajl_t, const unsigned char *tag, const unsigned char *value); // Add unescaped literal
const char *ajl_add_stringn(const ajl_t, const unsigned char *tag, const unsigned char *value, size_t len);     // Add UTF-8 string, escaped for JSON
const char *ajl_add_binary(const ajl_t, const unsigned char *tag, const unsigned char *value, size_t len);      // Add binary data as string, escaped for JSON
const char *ajl_add_number(const ajl_t, const unsigned char *tag, const char *fmt, ...);
const char *ajl_add_boolean(const ajl_t, const unsigned char *tag, unsigned char value);
const char *ajl_add_null(const ajl_t, const unsigned char *tag);
const char *ajl_add_object(const ajl_t, const unsigned char *tag);      // Start an object
const char *ajl_add_array(const ajl_t, const unsigned char *tag);       // Start an array
const char *ajl_add_close(const ajl_t); // close current array or object

// Low level.
int ajl_peek(const ajl_t j);    // Peek next character, -1 for eof, -2 for error
void ajl_next(const ajl_t j);   // Advance character
void ajl_copy(const ajl_t j, FILE * o); // Advance character (copy to o)
int ajl_isws(unsigned char c);  // Check if whitespace
void ajl_skip_ws(const ajl_t j);        // Skip any whitespace
const char *ajl_string(const ajl_t j, FILE * o);        // Process a string (i.e. starting and ending with quotes and using escapes), writing decoded string to file if not zero
const char *ajl_number(const ajl_t j, FILE * o);        // Process a number, writing number to file
void ajl_fwrite_string(FILE * o, const unsigned char *value, size_t len);       // Write escaped string
