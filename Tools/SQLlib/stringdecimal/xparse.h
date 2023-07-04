// Functions to perform simple expression parsing
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
#ifndef XPARSE_H
#define	XPARSE_H

// The operators take one or two args and return a result.
// The result must be malloced or one of the input args. The args should be left intact and are freed as needed later.
typedef void *xparse_operate(void *context, void *data, void **);
// Parse operand
typedef void *xparse_operand(void *context, const char *p, const char **end);   // Parse an operand, malloc value (or null if error), set end
// Final processing
typedef void *xparse_final(void *context, void *v);
// Disposing of an operand
typedef void xparse_free(void *context, void *v);
// Reporting an error
typedef void xparse_fail(void *context, const char *failure, const char *posn);

// The operator list - operators that are a subset of another must be later in the list
typedef struct {
   const char *op;              // Final entry should have NULL here, else this is the operator name
   const char *op2;             // Alternative operator name, e.g. unicode version
   char level;                  // Operator precedence 0-9
   xparse_operate *func;        // Function to do operation
   void *data;                  // Passed to xparse_operate
} xparse_op_t;

// This is the top level config
typedef struct xparse_config_s xparse_config_t;
struct xparse_config_s {
   xparse_op_t *unary;          // pre unary operators
   xparse_op_t *post;           // post unary operators
   xparse_op_t *binary;         // binary operators
   xparse_op_t *ternary;        // ternary operators
   xparse_op_t *bracket;        // bracketing operators
   xparse_operand *operand;     // operand parse
   xparse_final *final;         // final process operand
   xparse_free *dispose;        // Dispose of an operand
   xparse_fail *fail;           // Failure report
   unsigned char eol:1;         // Stop at end of line
};

// The parse function
void *xparse(xparse_config_t * config, void *context, const char *sum, const char **end);

extern const char *xparse_sub[];
extern const char *xparse_sup[];

#endif
