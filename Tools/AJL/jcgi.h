// ==========================================================================
//
//                       Adrian's JSON Library - jcgi.c
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

// This library allows C code to work as apache cgi, checking the environment and
// if necessary stdin for posted data. It uses the JSON library to provide the data
// and handle posted JSON.
//
// Formdata:-
// For multipart posted, any uploaded file is done as an object in the JSON with details of file, and where tmp file stored
// For multipart uploaded JSON the JSON is also read in to the object
// For posted JSON, formdata is the posted JSON
// Any QUERY string, even if there is posted JSON/form-data, is also processed as additional form-data compounding on the form-data object
// Temp files deleted on exit, so need renaming, or hard link if to be retained as files
//
// The macro jcgi simply takes a j_t and creates an object with a full set of sub objects
//
// Return error is non null for error (mallo'd)

#include "ajl.h"

typedef struct {
   j_t all;                     // If set, use as default for info, formdata, cookie and header
   const char *session;
   j_t info;                    // Load info
   j_t formdata;                // Load formdata
   j_t cookie;                  // Load cookie
   j_t header;                  // Load header
   unsigned char notmp:1;       // Don't make tmp files, just put raw data in "data":
   unsigned char noclean:1;     // Don't clean up tmp files on exit
   unsigned char nojson:1;      // Don't load JSON objects
   unsigned char jsontmp:1;     // Make tmp files if loading JSON
   unsigned char jsonerr:1;     // Fail if json load fails
   unsigned char small:1;       // Small file expected (set one of these)
   unsigned char medium:1;      // Medium file expected
   unsigned char large:1;       // Large file expected
} jcgi_t;
#define j_cgi(...) j_cgi_opts((jcgi_t){__VA_ARGS__})
char * __attribute__((warn_unused_result)) j_cgi_opts(jcgi_t);

char * __attribute__((warn_unused_result)) j_parse_formdata_sep(j_t, const char *, char sep);
#define j_parse_formdata(j,f) j_parse_formdata_sep(j,f,'&')
