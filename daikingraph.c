// Daikin graph from mariadb
// Copyright (c) 2022 Adrian Kennard, Andrews & Arnold Limited, see LICENSE file (GPL)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <popt.h>
#include <err.h>
#include <curl/curl.h>
#include <sqllib.h>
#include <axl.h>
#include <math.h>

int debug = 0;

int main(int argc, const char *argv[])
{
   const char *sqlhostname = NULL;
   const char *sqldatabase = "env";
   const char *sqlusername = NULL;
   const char *sqlpassword = NULL;
   const char *sqlconffile = NULL;
   const char *sqltable = "daikin";
   const char *tag = NULL;
   const char *title = NULL;
   const char *control = NULL;
   const char *me = NULL;
   char *date = NULL;
   double xsize = 36;           // Per hour
   double ysize = 36;           // Per degree
   int debug = 0;
   int nogrid = 0;
   int noaxis = 0;
   int nodate = 0;
   int back = 0;
   int temptop = 0;
   {                            // POPT
      poptContext optCon;       // context for parsing command-line options
      const struct poptOption optionsTable[] = {
         { "sql-conffile", 'c', POPT_ARG_STRING, &sqlconffile, 0, "SQL conf file", "filename" },
         { "sql-hostname", 'H', POPT_ARG_STRING, &sqlhostname, 0, "SQL hostname", "hostname" },
         { "sql-database", 'd', POPT_ARG_STRING | POPT_ARGFLAG_SHOW_DEFAULT, &sqldatabase, 0, "SQL database", "db" },
         { "sql-username", 'U', POPT_ARG_STRING, &sqlusername, 0, "SQL username", "name" },
         { "sql-password", 'P', POPT_ARG_STRING, &sqlpassword, 0, "SQL password", "pass" },
         { "sql-table", 't', POPT_ARG_STRING | POPT_ARGFLAG_SHOW_DEFAULT, &sqltable, 0, "SQL table", "table" },
         { "sql-debug", 'v', POPT_ARG_NONE, &sqldebug, 0, "SQL Debug" },
         { "x-size", 0, POPT_ARG_DOUBLE | POPT_ARGFLAG_SHOW_DEFAULT, &xsize, 0, "X size per hour", "pixels" },
         { "y-size", 0, POPT_ARG_DOUBLE | POPT_ARGFLAG_SHOW_DEFAULT, &ysize, 0, "Y size per step", "pixels" },
         { "tag", 'i', POPT_ARG_STRING, &tag, 0, "Device ID", "tag" },
         { "date", 'D', POPT_ARG_STRING, &date, 0, "Date", "YYYY-MM-DD" },
         { "title", 'T', POPT_ARG_STRING, &title, 0, "Title", "text" },
         { "temp-top", 0, POPT_ARG_INT, &temptop, 0, "Top temp", "C" },
         { "back", 0, POPT_ARG_INT, &back, 0, "Back days", "N" },
         { "control", 'C', POPT_ARG_STRING, &control, 0, "Control", "[-]N[T/C/R]" },
         { "no-grid", 0, POPT_ARG_NONE, &nogrid, 0, "No grid" },
         { "no-axis", 0, POPT_ARG_NONE, &noaxis, 0, "No axis" },
         { "no-date", 0, POPT_ARG_NONE, &nodate, 0, "No date" },
         { "debug", 'V', POPT_ARG_NONE, &debug, 0, "Debug" },
         { "me", 0, POPT_ARG_STRING | POPT_ARGFLAG_DOC_HIDDEN, &me, 0, "Me link", "URL" },
         POPT_AUTOHELP { }
      };

      optCon = poptGetContext(NULL, argc, argv, optionsTable, 0);
      //poptSetOtherOptionHelp (optCon, "");

      int c;
      if ((c = poptGetNextOpt(optCon)) < -1)
         errx(1, "%s: %s\n", poptBadOption(optCon, POPT_BADOPTION_NOALIAS), poptStrerror(c));

      if (poptPeekArg(optCon))
      {
         poptPrintUsage(optCon, stderr, 0);
         return -1;
      }
      poptFreeContext(optCon);
   }

   if (control)
   {                            // PATH_INFO typically
      // TODO extract date and tag
   }

   if (!tag || !*tag)
      errx(1, "Specify --tag");
   if (!date || !*date)
      errx(1, "Specify --date");

   time_t sod,
    eod;                        // Start and end of day
   int hours = 0;               // Number of hours
   double mintemp = NAN,
       maxtemp = NAN;           // Min and max temps seen
   {
      int Y,
       M,
       D;
      if (sscanf(date, "%d-%d-%d", &Y, &M, &D) != 3)
         errx(1, "Bad date");
      struct tm t = {.tm_year = Y - 1900,.tm_mon = M - 1,.tm_mday = D,.tm_isdst = -1 };
      sod = mktime(&t);
      t.tm_mday++;
      t.tm_isdst = 0;
      eod = mktime(&t);
      hours = (eod - sod) / 3600;



   }

   SQL sql;
   sql_real_connect(&sql, sqlhostname, sqlusername, sqlpassword, sqldatabase, 0, NULL, 0, 1, sqlconffile);

   xml_t svg = xml_tree_new("svg");
   if (me)
      xml_addf(svg, "a@rel=me@href", me);
   xml_element_set_namespace(svg, xml_namespace(svg, NULL, "http://www.w3.org/2000/svg"));
   xml_t top = xml_element_add(svg, "g");       // Top level, adjusted for position as temps all plotted from 0C as Y=0
   xml_t axis = xml_element_add(top, "g");      // Axis labels
   xml_t grid = xml_element_add(top, "g");      // Grid 1C/1hour
   xml_t ranges = xml_element_add(top, "g");    // Ranges
   xml_t traces = xml_element_add(top, "g");    // Traces
   xml_t labels = xml_element_add(svg, "g");    // Title (not offset)

   // Set width/height/offset
   mintemp = floor(mintemp);
   maxtemp = ceil(maxtemp);
   xml_addf(svg, "@width", "%.0f", xsize * hours);
   xml_addf(svg, "@height", "%.0f", ysize * (maxtemp - mintemp));
   xml_addf(top, "@transform", "translate(0,%.1f)scale(1,-1)", ysize * maxtemp);
   // Write out
   xml_write(stdout, svg);
   xml_tree_delete(svg);
   sql_close(&sql);
   return 0;
}
