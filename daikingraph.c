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
   char *tempcol = "#f00";
   char *date = NULL;
   double xsize = 36;
   double ysize = 36;
   double tempstep = 1;
   double templine = 0;
   int debug = 0;
   int nogrid = 0;
   int noaxis = 0;
   int nodate = 0;
   int days = 1;
   int spacing = 5;
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
         { "temp-step", 0, POPT_ARG_DOUBLE | POPT_ARGFLAG_SHOW_DEFAULT, &tempstep, 0, "Temp per Y step", "Celsius" },
         { "temp-line", 0, POPT_ARG_DOUBLE | POPT_ARGFLAG_SHOW_DEFAULT, &templine, 0, "Temp line", "Celsius" },
         { "tag", 'i', POPT_ARG_STRING, &tag, 0, "Device ID", "tag" },
         { "date", 'D', POPT_ARG_STRING, &date, 0, "Date", "YYYY-MM-DD" },
         { "title", 'T', POPT_ARG_STRING, &title, 0, "Title", "text" },
         { "days", 'N', POPT_ARG_INT | POPT_ARGFLAG_SHOW_DEFAULT, &days, 0, "Days", "N" },
         { "temp-top", 0, POPT_ARG_INT, &temptop, 0, "Top temp", "C" },
         { "back", 0, POPT_ARG_INT, &back, 0, "Back days", "N" },
         { "control", 'C', POPT_ARG_STRING, &control, 0, "Control", "[-]N[T/C/R]" },
         { "spacing", 0, POPT_ARG_INT | POPT_ARGFLAG_SHOW_DEFAULT, &spacing, 0, "Spacing", "N" },
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

      if (poptPeekArg(optCon) || !tag)
      {
         poptPrintUsage(optCon, stderr, 0);
         return -1;
      }
      poptFreeContext(optCon);
   }

   if (control)
   {
      if (*control == '-')
      {
         control++;
         days = 1;
         if (isdigit(*control))
         {
            back = 0;
            while (isdigit(*control))
               back = back * 10 + *control++ - '0';
         }

      }
      if (isdigit(*control))
      {
         days = 0;
         while (isdigit(*control))
            days = days * 10 + *control++ - '0';
      }
      while (isalpha(*control))
      {
         char t = tolower(*control++);
         int v = 0;
         while (isdigit(*control))
            v = v * 10 + *control++ - '0';
         char *col = NULL;
         if (*control == '=')
         {
            control++;
            col = (char *) control;
            while (isxdigit(*control))
               control++;
            col = strndup(col - 1, control + 1 - col);
            *col = '#';
         }
      }
   }

   SQL sql;
   sql_real_connect(&sql, sqlhostname, sqlusername, sqlpassword, sqldatabase, 0, NULL, 0, 1, sqlconffile);

   xml_t svg = xml_tree_new("svg");
   if(me)
	   xml_addf(svg,"a@rel=me@href",me);
   xml_element_set_namespace(svg, xml_namespace(svg, NULL, "http://www.w3.org/2000/svg"));
   xml_t top = xml_element_add(svg, "g");
   xml_t grid = xml_element_add(top, "g");
   xml_t axis = xml_element_add(top, "g");

   // TODO width/height
   
   // Write out
   xml_write(stdout, svg);
   xml_tree_delete(svg);
   sql_close(&sql);
   return 0;
}
