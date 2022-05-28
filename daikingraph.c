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
   const char *skip = NULL;
   const char *title = NULL;
   const char *control = NULL;
   const char *me = NULL;
   const char *href = NULL;
   const char *targetcol = "#080";
   const char *tempcol = "#080";
   const char *envcol = "#800";
   const char *homecol = "#880";
   const char *liquidcol = "#008";
   const char *inletcol = "#808";
   const char *outsidecol = "#088";
   const char *heatcol = "#f00";
   const char *coolcol = "#00f";
   const char *antifreezecol = "#ff0";
   const char *slavecol = "#0f0";
   const char *fanrpmcol = "#000";
   const char *date = NULL;
   double xsize = 36;           // Per hour
   double ysize = 36;           // Per degree
   double left = 36;            // Left margin
   int debug = 0;
   int nogrid = 0;
   int noaxis = 0;
   int nolabels = 0;
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
         { "date", 'D', POPT_ARG_STRING, &date, 0, "Date", "YYYY-MM-DD" },
         { "tag", 'i', POPT_ARG_STRING, &tag, 0, "Device ID", "tag" },
         { "skip", 0, POPT_ARG_STRING, &skip, 0, "Fields not to show", "tags" },
         { "title", 'T', POPT_ARG_STRING, &title, 0, "Title", "text" },
         { "temp-top", 0, POPT_ARG_INT, &temptop, 0, "Top temp", "C" },
         { "back", 0, POPT_ARG_INT, &back, 0, "Back days", "N" },
         { "control", 'C', POPT_ARG_STRING, &control, 0, "Control", "[-]N[T/C/R]" },
         { "no-grid", 0, POPT_ARG_NONE, &nogrid, 0, "No grid lines" },
         { "no-axis", 0, POPT_ARG_NONE, &noaxis, 0, "No axis labels" },
         { "no-labels", 0, POPT_ARG_NONE, &nolabels, 0, "No labels" },
         { "debug", 'V', POPT_ARG_NONE, &debug, 0, "Debug" },
         { "target-colour", 0, POPT_ARG_STRING | POPT_ARGFLAG_SHOW_DEFAULT, &targetcol, 0, "Target colour", "#rgb" },
         { "temp-colour", 0, POPT_ARG_STRING | POPT_ARGFLAG_SHOW_DEFAULT, &tempcol, 0, "Temp colour", "#rgb" },
         { "env-colour", 0, POPT_ARG_STRING | POPT_ARGFLAG_SHOW_DEFAULT, &envcol, 0, "Env colour", "#rgb" },
         { "home-colour", 0, POPT_ARG_STRING | POPT_ARGFLAG_SHOW_DEFAULT, &homecol, 0, "Home colour", "#rgb" },
         { "liquid-colour", 0, POPT_ARG_STRING | POPT_ARGFLAG_SHOW_DEFAULT, &liquidcol, 0, "Liquid colour", "#rgb" },
         { "inlet-colour", 0, POPT_ARG_STRING | POPT_ARGFLAG_SHOW_DEFAULT, &inletcol, 0, "Inlet colour", "#rgb" },
         { "outside-colour", 0, POPT_ARG_STRING | POPT_ARGFLAG_SHOW_DEFAULT, &outsidecol, 0, "Outside colour", "#rgb" },
         { "heat-colour", 0, POPT_ARG_STRING | POPT_ARGFLAG_SHOW_DEFAULT, &heatcol, 0, "Heat colour", "#rgb" },
         { "cool-colour", 0, POPT_ARG_STRING | POPT_ARGFLAG_SHOW_DEFAULT, &coolcol, 0, "Cool colour", "#rgb" },
         { "anti-freeze-colour", 0, POPT_ARG_STRING | POPT_ARGFLAG_SHOW_DEFAULT, &antifreezecol, 0, "Anti freeze colour", "#rgb" },
         { "slave-colour", 0, POPT_ARG_STRING | POPT_ARGFLAG_SHOW_DEFAULT, &slavecol, 0, "Slave colour", "#rgb" },
         { "fanrpm-colour", 0, POPT_ARG_STRING | POPT_ARGFLAG_SHOW_DEFAULT, &fanrpmcol, 0, "Fan RPM/100 colour", "#rgb" },
         { "me", 0, POPT_ARG_STRING | POPT_ARGFLAG_DOC_HIDDEN, &me, 0, "Me link (e.g. for mastodon)", "URL" },
         { "href", 0, POPT_ARG_STRING | POPT_ARGFLAG_DOC_HIDDEN, &href, 0, "Self link", "URL" },
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
   if (noaxis)
      left = 0;

   if (control)
   {                            // PATH_INFO typically
      if (*control == '/')
         control++;
      date = control;
      char *s = strchr(control, '/');
      if (s)
      {
         *s++ = 0;
         tag = s;
         s = strchr(tag, '/');
         if (s)
         {
            *s++ = 0;
            skip = s;
         }
      }
   }

   if (!tag || !*tag)
      errx(1, "Specify --tag");
   if (!date || !*date)
      errx(1, "Specify --date");
   if (skip)
      for (const char *s = skip; *s; s++)
         switch (*s)
         {
         case 'H':
            homecol = NULL;
            break;
         case 'S':
            tempcol = NULL;
            break;
         case 'L':
            liquidcol = NULL;
            break;
         case 'I':
            inletcol = NULL;
            break;
         case 'O':
            outsidecol = NULL;
            break;
         case 'T':
            targetcol = NULL;
            break;
         case 'E':
            envcol = NULL;
            break;
         case 'F':
            fanrpmcol = NULL;
            break;
         case 'h':
            heatcol = NULL;
            break;
         case 'c':
            coolcol = NULL;
            break;
         case 's':
            slavecol = NULL;
            break;
         case 'a':
            antifreezecol = NULL;
            break;
         }

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
      t.tm_isdst = -1;
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
   xml_t grid = xml_element_add(top, "g");      // Grid 1C/1hour
   xml_t bands = xml_element_add(top, "g");     // Bands (booleans)
   xml_t ranges = xml_element_add(top, "g");    // Ranges
   xml_t traces = xml_element_add(top, "g");    // Traces
   xml_t axis = xml_element_add(svg, "g");      // Axis labels (not offset as text ends up upside down)
   xml_t labels = xml_element_add(svg, "g");    // Title (not offset)

   double utcx(SQL_RES * res) {
      char *utc = sql_colz(res, "utc");
      if (!utc)
         return NAN;
      time_t t = sql_time_utc(utc);
      return xsize * (t - sod) / 3600;
   }

   double tempy(SQL_RES * res, const char *field) {
      char *val = sql_col(res, field);
      if (!val || !*val)
         return NAN;
      double temp = strtod(val, NULL);
      if (isnan(mintemp) || mintemp > temp)
         mintemp = temp;
      if (isnan(maxtemp) || maxtemp < temp)
         maxtemp = temp;
      return temp * ysize;
   }

   void addpos(FILE * f, char *m, double x, double y) {
      if (isnan(x) || isnan(y))
         return;
      fprintf(f, "%c%.2f,%.2f", *m, x, y);
      *m = 'L';
   }

   const char *range(xml_t g, const char *field, const char *colour, int group) {       // Plot a temp range based on min/max of field
      if (!colour || !*colour)
         return NULL;
      char *path;
      size_t len;
      FILE *f = open_memstream(&path, &len);
      char m = 'M';
      double last;
      SQL_RES *select(const char *order) {      // Trust field name
         return sql_safe_query_store_free(&sql, sql_printf("SELECT min(`utc`) AS `utc`,max(max%s) AS `max`,min(min%s) AS `min` FROM `%#S` WHERE `tag`=%#s AND `utc`>=%#U AND `utc`<=%#U GROUP BY substring(`utc`,1,%d) ORDER BY `utc` %s", field, field, sqltable, tag, sod, eod, group, order));
      }
      // Forward
      last = NAN;
      SQL_RES *res = select("asc");
      while (sql_fetch_row(res))
      {
         double t = tempy(res, "max");
         addpos(f, &m, utcx(res), isnan(last) || t > last ? t : last);
         last = t;
      }
      sql_free_result(res);
      // Reverse
      res = select("desc");
      last = NAN;
      double lastx = NAN;
      while (sql_fetch_row(res))
      {
         double t = tempy(res, "min");
         if (!isnan(lastx))
            addpos(f, &m, lastx, isnan(last) || t < last ? t : last);
         last = t;
         lastx = utcx(res);
      }
      if (!isnan(lastx))
         addpos(f, &m, lastx, last);
      sql_free_result(res);
      fclose(f);
      if (*path)
      {
         xml_t p = xml_element_add(g, "path");
         xml_add(p, "@d", path);
         xml_add(p, "@fill", colour);
         xml_add(p, "@stroke", colour);
         xml_add(p, "@opacity", "0.1");
      } else
         colour = NULL;
      free(path);
      return colour;
   }
   const char *trace(xml_t g, const char *field, const char *colour) {  // Plot trace
      if (!colour || !*colour)
         return NULL;
      char *path;
      size_t len;
      FILE *f = open_memstream(&path, &len);
      char m = 'M';
      // Forward (trust the trace field name)
      SQL_RES *res = sql_safe_query_store_free(&sql, sql_printf("SELECT `utc`,%s AS `val` FROM `%#S` WHERE `tag`=%#s AND `utc`>=%#U AND `utc`<=%#U ORDER BY `utc`", field, sqltable, tag, sod, eod));
      double lastx = NAN;
      while (sql_fetch_row(res))
      {
         double x = utcx(res);
         if (!isnan(lastx) || x - lastx > xsize / 4)
            m = 'M';            // gap
         addpos(f, &m, x, tempy(res, "val"));
      }
      sql_free_result(res);
      fclose(f);
      if (*path)
      {
         xml_t p = xml_element_add(g, "path");
         xml_add(p, "@d", path);
         xml_add(p, "@fill", "none");
         xml_add(p, "@stroke", colour);
      } else
         colour = NULL;
      free(path);
      return colour;
   }
   const char *rangetrace(xml_t g, xml_t g2, const char *field, const char *colour) {   // Plot a temp range based on min/max of field and trace
      const char *col = range(g, field, colour, 15);
      trace(g2, field, colour);
      return col;
   }

   targetcol = range(ranges, "target", targetcol, 19);
   if (targetcol)
   {
      trace(traces, "IF(mintarget=maxtarget,mintarget,NULL)", targetcol);
      tempcol = NULL;
   }
   fanrpmcol = rangetrace(ranges, traces, "fanrpm/100", fanrpmcol);
   tempcol = rangetrace(ranges, traces, "temp", tempcol);
   envcol = rangetrace(ranges, traces, "env", envcol);
   homecol = rangetrace(ranges, traces, "home", homecol);
   liquidcol = rangetrace(ranges, traces, "liquid", liquidcol);
   inletcol = rangetrace(ranges, traces, "inlet", inletcol);
   outsidecol = rangetrace(ranges, traces, "outside", outsidecol);

   // Set range of temps shown
   if (isnan(mintemp))
   {
      mintemp = -1;
      maxtemp = 1;
   }
   if (maxtemp < 5)
      maxtemp = 5;
   mintemp = floor(mintemp) - 0.5;
   maxtemp = ceil(maxtemp) + 0.5;

   // Bands (booleans)
   const char *band(const char *field, const char *colour) {
      if (!colour || !*colour)
         return NULL;
      char *path;
      size_t len;
      FILE *f = open_memstream(&path, &len);
      char m = 'M';
      SQL_RES *res = sql_safe_query_store_free(&sql, sql_printf("SELECT `utc`,%s AS `val` FROM `%#S` WHERE `tag`=%#s AND `utc`>=%#U AND `utc`<=%#U ORDER BY `utc`", field, sqltable, tag, sod, eod));
      double lastx = NAN;
      double startx = NAN;
      void end(double x, double v) {    // End
         double endx = lastx * (1 - v) + x * v;
         m = 'M';
         addpos(f, &m, startx, ysize * mintemp);
         addpos(f, &m, startx, ysize * maxtemp);
         addpos(f, &m, endx, ysize * maxtemp);
         addpos(f, &m, endx, ysize * mintemp);
         startx = NAN;
      }
      while (sql_fetch_row(res))
      {
         double x = utcx(res);
         double v = strtod(sql_colz(res, "val"), NULL);
         if (v < 0)
            v = 0;
         if (v > 1)
            v = 1;
         if (!isnan(lastx))
         {
            if (v > 0 && isnan(startx))
               startx = lastx * v + x * (1 - v);
            if (v < 1 && !isnan(startx))
               end(x, v);
         }
         lastx = x;
      }
      if (!isnan(startx))
         end(lastx, 1);
      sql_free_result(res);
      fclose(f);
      if (*path)
      {
         xml_t p = xml_element_add(bands, "path");
         xml_add(p, "@d", path);
         xml_add(p, "@fill", colour);
         xml_add(p, "@stroke", "none");
         xml_add(p, "@opacity", "0.25");
      } else
         colour = NULL;
      free(path);
      return colour;
   }
   heatcol = band("least(`power`,`heat`)-`slave`", heatcol);
   coolcol = band("`power`-`heat`-`slave`-`antifreeze`", coolcol);
   antifreezecol = band("`antifreeze`", antifreezecol);
   slavecol = band("least(`slave`,`power`)", slavecol);

   // Grid
   if (!nogrid)
   {
      char *path;
      size_t len;
      FILE *f = open_memstream(&path, &len);
      char m;
      for (int h = 0; h <= hours; h++)
      {
         m = 'M';
         addpos(f, &m, xsize * h, ysize * mintemp);
         addpos(f, &m, xsize * h, ysize * maxtemp);
      }
      for (double t = ceil(mintemp); t <= floor(maxtemp); t += 1)
      {
         m = 'M';
         addpos(f, &m, 0, ysize * t);
         addpos(f, &m, xsize * hours, ysize * t);
      }
      m = 'M';                  // Extra on zero
      addpos(f, &m, 0, 0);
      addpos(f, &m, xsize * hours, 0);
      fclose(f);
      if (*path)
      {
         xml_t p = xml_element_add(grid, "path");
         xml_add(p, "@d", path);
         xml_add(p, "@fill", "none");
         xml_add(p, "@stroke", "black");
         xml_add(p, "@opacity", "0.25");
      }
      free(path);
   }
   // Axis
   if (!noaxis)
   {
      for (int h = 0; h < hours; h++)
      {
         struct tm tm;
         time_t when = sod + 3600 * h;
         localtime_r(&when, &tm);
         xml_t t = xml_addf(axis, "+text", "%02d", tm.tm_hour);
         xml_addf(t, "@x", "%.2f", left + xsize * h + 1);
         xml_addf(t, "@y", "%.2f", ysize * maxtemp - 1);
      }
      for (double temp = ceil(mintemp); temp <= floor(maxtemp); temp += 1)
      {
         xml_t t = xml_addf(axis, "+text", "%.0f", temp);
         xml_addf(t, "@x", "%.2f", left - 1);
         xml_addf(t, "@y", "%.2f", ysize * maxtemp - (ysize * temp - 6));
         xml_add(t, "@text-anchor", "end");
      }
   }
   // Title
   {
      int y = 0;
      if (title)
      {
         char *txt = strdupa(title);
         while (txt && *txt)
         {
            char *e = strchr(txt, '/');
            if (e)
               *e++ = 0;
            xml_t t = xml_element_add(labels, "text");
            if (*txt == '-')
            {
               y += 9;
               txt++;
               xml_add(t, "@font-size", "7");
            } else
               y += 17;
            xml_element_set_content(t, txt);
            xml_addf(t, "@x", "%.2f", xsize * hours + left - 1);
            xml_addf(t, "@y", "%d", y);
            xml_add(t, "@text-anchor", "end");
            txt = e;
         }
      }
      if (!nolabels)
      {
         void label(const char *text, const char *colour, char zap) {
            if (!colour)
               return;
            if (!href)
               zap = 0;
            y += 17;
            xml_t t = xml_element_add(labels, zap ? "a" : "text");
            if (zap)
            {
               xml_addf(t, "@href", "%s/%s/%s/%s%c", href, date, tag, skip ? : "", zap);
               t = xml_element_add(t, "text");
            }
            xml_element_set_content(t, text);
            xml_addf(t, "@x", "%.2f", xsize * hours + left - 1);
            xml_addf(t, "@y", "%d", y);
            xml_add(t, "@text-anchor", "end");
            xml_add(t, "@fill", colour);
         }
         if (href)
         {
            y += 17;
            struct tm tm;
            localtime_r(&sod, &tm);
            tm.tm_mday--;
            tm.tm_isdst = 0;
            mktime(&tm);
            xml_t t = xml_element_add(labels, "a");
            xml_addf(t, "@href", "%s/%04d-%02d-%02d/%s/%s", href, tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tag, skip);
            t = xml_element_add(t, "text");
            xml_element_set_content(t, "<");
            xml_addf(t, "@x", "%.2f", xsize * hours + left - 41);
            xml_addf(t, "@y", "%d", y);
            xml_add(t, "@text-anchor", "end");
            if (skip && *skip)
            {
               t = xml_element_add(labels, "a");
               xml_addf(t, "@href", "%s/%s/%s", href, date, tag);
               t = xml_element_add(t, "text");
               xml_element_set_content(t, "*");
               xml_addf(t, "@x", "%.2f", xsize * hours + left - 21);
               xml_addf(t, "@y", "%d", y);
               xml_add(t, "@text-anchor", "end");
            }
            t = xml_element_add(labels, "a");
            localtime_r(&sod, &tm);
            tm.tm_mday++;
            tm.tm_isdst = 0;
            mktime(&tm);
            xml_addf(t, "@href", "%s/%04d-%02d-%02d/%s/%s", href, tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tag, skip);
            t = xml_element_add(t, "text");
            xml_element_set_content(t, ">");
            xml_addf(t, "@x", "%.2f", xsize * hours + left - 1);
            xml_addf(t, "@y", "%d", y);
            xml_add(t, "@text-anchor", "end");
         }
         label(date, "black", 0);
         label(tag, "black", 0);
         label("Home", homecol, 'H');
         label("TempSet", tempcol, 'S');
         label("Liquid", liquidcol, 'L');
         label("Inlet", inletcol, 'I');
         label("Outside", outsidecol, 'O');
         label("EnvTarget", targetcol, 'T');
         label("Env", envcol, 'E');
         label("FanRPM/100", fanrpmcol, 'F');
         label("Heat", heatcol, 'h');
         label("Cool", coolcol, 'c');
         label("Slave", slavecol, 's');
         label("Anti-Freeze", antifreezecol, 'a');
      }
   }
   // Set width/height/offset
   xml_addf(svg, "@width", "%.0f", xsize * hours + left);
   xml_addf(svg, "@height", "%.0f", ysize * (maxtemp - mintemp));
   xml_addf(top, "@transform", "translate(%.1f,%.1f)scale(1,-1)", left, ysize * maxtemp);
   xml_add(svg, "@font-family", "sans-serif");
   xml_add(svg, "@font-size", "15");
   // Write out
   xml_write(stdout, svg);
   xml_tree_delete(svg);
   sql_close(&sql);
   return 0;
}
