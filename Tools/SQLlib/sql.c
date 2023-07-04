// General SQL client application
// Designed to execute distinct queries
// Expands environment variables with quoting for sql
// Various output formats
// (c) Adrian Kennard 2007

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <popt.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <uuid/uuid.h>
#include <syslog.h>
#include <signal.h>
#include <execinfo.h>
#include "sqlexpand.h"
#include "sqllib.h"
#ifndef	NOXML
#include "axl.h"
#endif

const char *sqlconf = NULL;
const char *sqlhost = NULL;
unsigned int sqlport = 0;
const char *sqluser = NULL;
const char *sqlpass = NULL;
const char *sqldatabase = NULL;
#ifndef	NOXML
const char *xmlroot = "sql";
const char *xmlinfo = NULL;
const char *xmlcol = "col";
const char *xmlresult = "result";
const char *xmlrow = "row";
#endif
const char *csvsol = "";
const char *csveol = "";
const char *csvnull = "";
const char *csvcomma = ",";
int jsarray = 0;
int slow = 0;
int trans = 0;
int debug = 0;
int headers = 0;
int noexpand = 0;
int expand = 0;
int reportid = 0;
int reportchanges = 0;
int statuschanges = 0;
int ret = 0;
int linesplit = 0;
int safe = 0;
int unsafe = 0;
#ifndef	NOXML
int xmlout = 0;
#endif
int jsonout = 0;
int csvout = 0;
int abortdeadlock = 0;
#ifndef NOXML
xml_t xml = NULL;
#endif
char uuid[37] = "";

SQL sql;
SQL_RES *res;
SQL_ROW row;

void
dosql (const char *origquery)
{
   const char *e,
    *ep;
   char *query;
   query = strdupa (origquery);
   int l = strlen (query);
   while (l && isspace (query[l - 1]))
      l--;
   if (l && query[l - 1] == ';')
   {
      l--;
      warnx ("Trailing ; on query %s", origquery);
   }
   query[l] = 0;
   if (!l)
      return;
   if (!noexpand)
   {
      query = sqlexpand (query, getenv, &e, &ep, SQLEXPANDSTDIN | SQLEXPANDFILE | SQLEXPANDBLANK | SQLEXPANDUNSAFE | SQLEXPANDZERO);
      if (!query)
         errx (1, "Expand failed: %s\n[%s]\n[%s]", e, origquery, ep);
      if (e)
         fprintf (stderr, "Expand issue: %s\n[%s]\n[%s]\n[%s]\n", e, origquery, query, ep);
   }
   if (expand)
   {                            // Just expanding
      printf ("%s", query);
      free (query);
      return;
   }
   int err = sql_query (&sql, query);
   if (err && !safe && !unsafe && sql_errno (&sql) == ER_UPDATE_WITHOUT_KEY_IN_SAFE_MODE)
   {
      warnx ("SQL warning:%s\n[%s]\n[%s]", sql_error (&sql), query, origquery);
      sql_safe_query (&sql, "SET SQL_SAFE_UPDATES=0");
      err = sql_query (&sql, query);
   }
   if (err == ER_LOCK_DEADLOCK && !abortdeadlock && !trans)
      err = sql_query (&sql, query);    // Auto retry - just the once
   if (err)
   {
      trans = 0;                // Abort
      ret++;
      if (trans)
         errx (1, "SQL error:%s\n[%s]\n[%s]", sql_error (&sql), query, origquery);
      else
         warnx ("SQL error:%s\n[%s]\n[%s]", sql_error (&sql), query, origquery);
   } else
   {
      if (debug)
         fprintf (stderr, "[%s]\n", query);
      res = sql_store_result (&sql);
      if (res)
      {
         int fields = sql_num_fields (res),
            f;
         SQL_FIELD *field = sql_fetch_field (res);
#if 0
         if (debug)
         {
            fprintf (stderr, "Type\tLen\tName\n");
            for (f = 0; f < fields; f++)
               fprintf (stderr, "%d\t%lu\t%s\t", field[f].type, field[f].length, field[f].name);
         }
#endif
#ifndef NOXML
         if (xml)
         {
            if (xmlinfo && *xmlinfo && *xmlresult)
            {
               xml_t x = xml_element_add (xml, xmlinfo);
               for (f = 0; f < fields; f++)
               {
                  xml_t c = xml_element_add (x, xmlcol);
                  xml_attribute_set (c, "name", field[f].name);
                  if (!(field[f].flags & NOT_NULL_FLAG))
                     xml_attribute_set (c, "null", "true");
                  if (field[f].flags & PRI_KEY_FLAG)
                     xml_attribute_set (c, "key", "primary");
                  else if (field[f].flags & UNIQUE_KEY_FLAG)
                     xml_attribute_set (c, "key", "unique");
                  else if (field[f].flags & MULTIPLE_KEY_FLAG)
                     if (field[f].flags & UNSIGNED_FLAG)
                        xml_attribute_set (c, "unsigned", "true");
                  if (field[f].flags & ZEROFILL_FLAG)
                     xml_attribute_set (c, "zero-fill", "true");
                  if (debug)
                  {
                     if (field[f].db)
                        xml_attribute_set (c, "database", field[f].db);
                     if (field[f].table)
                        xml_attribute_set (c, "table", field[f].table);
                     if (field[f].table && field[f].org_table && strcmp (field[f].table, field[f].org_table))
                        xml_attribute_set (c, "org-table", field[f].org_table);
                  }
                  if (!(field[f].flags & BLOB_FLAG))
                  {
                     if (field[f].length)
                        xml_attribute_set (c, "length", xml_number (field[f].length));
                     if (field[f].max_length)
                        xml_attribute_set (c, "max-length", xml_number (field[f].max_length));
                     if (field[f].decimals)
                        xml_attribute_set (c, "decimals", xml_number (field[f].decimals));
                  }
#define t(x) if(field[f].type==MYSQL_TYPE_##x)xml_attribute_set(c,"type",#x);
                  t (DECIMAL);
                  t (TINY);
                  t (SHORT);
                  t (LONG);
                  t (FLOAT);
                  t (DOUBLE);
                  t (NULL);
                  t (TIMESTAMP);
                  t (LONGLONG);
                  t (INT24);
                  t (DATE);
                  t (TIME);
                  t (DATETIME);
                  t (YEAR);
                  t (NEWDATE);
#ifdef	MYSQL_TYPE_VARCHAR
                  t (VARCHAR);
#endif
#ifdef	MYSQL_TYPE_BIT
                  t (BIT);
#endif
#ifdef	MYSQL_TYPE_NEWDECIMAL
                  t (NEWDECIMAL);
#endif
                  t (ENUM);
                  t (SET);
                  t (TINY_BLOB);
                  t (MEDIUM_BLOB);
                  t (LONG_BLOB);
                  t (BLOB);
                  t (VAR_STRING);
                  t (STRING);
                  t (GEOMETRY);
#undef t
               }
            }
            xml_t x = xml;
            if (*xmlresult)
               x = xml_element_add (x, xmlresult);
            if (headers)
               xml_attribute_set (x, "query", query);
            while ((row = sql_fetch_row (res)))
            {
               xml_t r = xml_element_add (x, xmlrow);
               for (f = 0; f < fields; f++)
                  if (row[f])
                     xml_attribute_set (r, field[f].name, row[f]);
            }
         } else
#endif
         if (csvout)
         {
            void s (char *s)
            {
               putchar ('"');
               while (*s)
               {
                  if (*s == '\n')
                     printf ("\\n");
                  else if (*s == '\r')
                     printf ("\\r");
                  else if (*s == '\t')
                     printf ("\\t");
                  else
                  {
                     if (*s == '\\' || *s == '\"')
                        putchar ('\\');
                     putchar (*s);
                  }
                  s++;
               }
               putchar ('"');
            }
            int line = 0;
            if (jsarray)
               printf ("%s", csvsol);
            if (headers)
            {
               if (line++)
                  printf ("%s\n", jsarray ? csvcomma : "");
               printf ("%s", csvsol);
               for (f = 0; f < fields; f++)
               {
                  if (f)
                     printf ("%s", csvcomma);
                  s (field[f].name);
               }
               printf ("%s", csveol);
            }
            while ((row = sql_fetch_row (res)))
            {
               if (line++)
                  printf ("%s\n", jsarray ? csvcomma : "");
               printf ("%s", csvsol);
               for (f = 0; f < fields; f++)
               {
                  if (f)
                     printf ("%s", csvcomma);
                  // NULL is nothing, i.e. ,, or configured null value
                  // Strings are quoted even if empty
                  // Numerics are unquoted
                  if (row[f] && !strncasecmp (field[f].name, "json_", 5))
                     printf ("%s", row[f]);     // Special case, sql already JSON coded
                  else if (row[f])
                  {
                     if (IS_NUM (field[f].type) && field[f].type != FIELD_TYPE_TIMESTAMP)
                        printf ("%s", row[f]);
                     else
                        s (row[f]);
                  } else
                     printf ("%s", csvnull);
               }
               printf ("%s", csveol);
            }
            printf ("%s\n", jsarray ? csveol : "");
         } else
         {
            if (headers)
            {
               for (f = 0; f < fields; f++)
               {
                  if (f)
                     putchar ('\t');
                  printf ("%s", field[f].name);
               }
               putchar ('\n');
            }
            while ((row = sql_fetch_row (res)))
            {
               for (f = 0; f < fields; f++)
               {
                  if (f)
                     putchar (linesplit ? '\n' : '\t');
                  if (!row[f])
                     printf ("NULL");
                  else if (*row[f])
                     printf ("%s", row[f]);
                  else if (linesplit)
                     putchar (' ');
               }
               putchar ('\n');
            }
         }
         sql_free_result (res);
      } else
      {
         if (reportid)
         {
            unsigned long long i = sql_insert_id (&sql);
            if (i)
               printf ("%llu\n", sql_insert_id (&sql));
            else if (*uuid)
               printf ("%s\n", uuid);
         }
         if (reportchanges)
            printf ("%llu\n", sql_affected_rows (&sql));
         if (statuschanges && !sql_affected_rows (&sql))
            ret++;
      }
   }
   free (query);
   if (slow)
      usleep (10000);
}

int
main (int argc, const char *argv[])
{
#ifndef NOXML
   const char *defxmlroot = xmlroot;
   const char *defxmlinfo = xmlinfo;
   const char *defxmlcol = xmlcol;
   const char *defxmlresult = xmlresult;
   const char *defxmlrow = xmlrow;
#endif
   const char *defcsvsol = csvsol;
   const char *defcsveol = csveol;
   const char *defcsvnull = csvnull;
   const char *defcsvcomma = csvcomma;
   poptContext popt;            // context for parsing command-line options
   const struct poptOption optionsTable[] = {
      {"sql-conf", 0, POPT_ARGFLAG_SHOW_DEFAULT | POPT_ARG_STRING, &sqlconf, 0, "Client config file ($SQL_CNF_FILE)", "filename"},
      {"sql-host", 'h', POPT_ARG_STRING, &sqlhost, 0, "SQL server host", "hostname/ip"},
      {"sql-port", 0, POPT_ARG_INT, &sqlport, 0, "SQL server port", "port"},
      {
       "sql-user", 'u', POPT_ARG_STRING, &sqluser, 0, "SQL username", "username"},
      {
       "sql-pass", 'p', POPT_ARG_STRING, &sqlpass, 0, "SQL password", "password"},
      {
       "sql-database", 'd', POPT_ARG_STRING, &sqldatabase, 0, "SQL database", "database"},
      {"debug", 'v', POPT_ARG_NONE, &debug, 0, "Debug", 0},
      {"sql-debug", 'V', POPT_ARG_NONE, &sqldebug, 0, "SQL Debug", 0},
      {"id", 'i', POPT_ARG_NONE, &reportid, 0, "Print insert ID", 0},
      {"changes", 'c', POPT_ARG_NONE, &reportchanges, 0, "Report how many rows changes", 0},
      {"safe", 0, POPT_ARG_NONE, &safe, 0, "Use safe mode (default is to warn but continue)", 0},
      {"unsafe", 0, POPT_ARG_NONE, &unsafe, 0, "Use unsafe mode (default is to warn but continue)", 0},
      {"status-changes", 'C', POPT_ARG_NONE, &statuschanges, 0, "Return non zero status if no changes were made", 0},
      {"no-expand", 'x', POPT_ARG_NONE, &noexpand, 0, "Don't expand env variables", 0},
      {"expand", 0, POPT_ARG_NONE, &expand, 0, "Just expand the queries and write to stdout", 0},
      {"transaction", 't', POPT_ARG_NONE, &trans, 0, "Run sequence of commands as a transaction", 0},
      {"abort-deadlock", 'A', POPT_ARG_NONE, &abortdeadlock, 0, "Do not retry single command on deadlock error", 0},
#ifndef NOXML
      {"XML", 0, POPT_ARGFLAG_DOC_HIDDEN | POPT_ARG_NONE, &xmlout, 0, "Output in XML", 0},
      {"xml", 'X', POPT_ARG_NONE, &xmlout, 0, "Output in XML", 0},
#endif
      {"csv", 0, POPT_ARG_NONE, &csvout, 0, "Output in CSV", 0},
      {"json", 0, POPT_ARG_NONE, &jsonout, 0, "Output in JSON", 0},
#ifndef NOXML

      {"xml-root", 0, POPT_ARGFLAG_SHOW_DEFAULT | POPT_ARG_STRING, &xmlroot, 0, "Label for root object", 0},
      {"xml-info", 0, POPT_ARG_STRING, &xmlinfo, 0, "Label for info object (column type info)", 0},
      {
       "xml-result", 0, POPT_ARGFLAG_SHOW_DEFAULT | POPT_ARG_STRING, &xmlresult, 0, "Label for result object", 0},
      {"xml-row", 0, POPT_ARGFLAG_SHOW_DEFAULT | POPT_ARG_STRING, &xmlrow, 0, "Label for row object", 0},
#endif
      {"csv-sol", 0, POPT_ARG_STRING, &csvsol, 0, "Start of each line for CSV", 0},
      {"csv-eol", 0, POPT_ARG_STRING, &csveol, 0, "End of each line for CSV", 0},
      {"csv-null", 0, POPT_ARG_STRING, &csvnull, 0, "NULL in CSV", 0},
      {"csv-comma", 0, POPT_ARGFLAG_SHOW_DEFAULT | POPT_ARG_STRING, &csvcomma, 0, "Comma in CSV", 0},
      {"jsarray", 0, POPT_ARG_NONE, &jsarray, 0, "Javascript array", 0},
      {
       "line-split", 'l', POPT_ARG_NONE, &linesplit, 0,
       "Put each field on a new line, and a single space if empty string, for use in (\"`...`\") in csh", 0},
      {"headers", 'H', POPT_ARG_NONE, &headers, 0, "Headers", 0},
      {"slow", 0, POPT_ARG_NONE, &slow, 0, "Pause between commands", 0},
      POPT_AUTOHELP {
                     NULL, 0, 0, NULL, 0}
   };

   popt = poptGetContext (NULL, argc, argv, optionsTable, 0);
   poptSetOtherOptionHelp (popt, "[database] '<sql-commands>'");
   /* Now do options processing, get portname */
   {
      int c = poptGetNextOpt (popt);
      if (c < -1)
         errx (1, "%s: %s\n", poptBadOption (popt, POPT_BADOPTION_NOALIAS), poptStrerror (c));
   }

   if (safe && unsafe)
      errx (1, "Do the safety dance");
#ifndef NOXML
   if (jsonout)
   {                            // Alternative defaults for json
      if (defxmlroot == xmlroot)
         xmlroot = "";
      if (defxmlrow == xmlrow)
         xmlrow = "";
      if (defxmlinfo == xmlinfo)
         xmlinfo = "";
      if (defxmlresult == xmlresult)
         xmlresult = "";
      if (defxmlcol == xmlcol)
         xmlcol = "";
   }
#endif
   if (jsarray)
   {                            // Alternative defaults for jsarray
      csvout = 1;
      if (defcsvsol == csvsol)
         csvsol = "[";
      if (defcsveol == csveol)
         csveol = "]";
      if (defcsvnull == csvnull)
         csvnull = "null";
      if (defcsvcomma == csvcomma)
         csvcomma = ",";
   }
#ifndef NOXML
   if (xmlout || jsonout)
   {
      xml = xml_tree_new (NULL);
      xml_tree_add_root (xml, xmlroot);
   }
#endif

   if (!expand && !sqldatabase && poptPeekArg (popt))
      sqldatabase = poptGetArg (popt);

   if (sqldatabase && !*sqldatabase)
      sqldatabase = NULL;

   if (sqldatabase && expand)
      errx (1, "Don't specify database for --expand");
   if (expand && noexpand)
      errx (1, "Make your bloody mind up");

   if (!sqlconf)
      sqlconf = getenv ("SQL_CNF_FILE");

   if (!expand)
      sql_real_connect (&sql, sqlhost, sqluser, sqlpass, sqldatabase, sqlport, 0, 0, 1, sqlconf);

   if (trans)
      dosql ("START TRANSACTION");
   if (!expand && !unsafe)
      sql_safe_query (&sql, "SET SQL_SAFE_UPDATES=1");
   if (!poptPeekArg (popt))
   {                            // stdin
      char *line = NULL;
      size_t linespace = 0;
      ssize_t len = 0;
      while ((len = getline (&line, &linespace, stdin)) > 0)
      {
         if (len && line[len - 1] == '\n')
            len--;
         if (len && line[len - 1] == '\r')
            len--;
         while (len && isspace (line[len - 1]))
            len--;
         if (len && line[len - 1] == ';')
            len--;              // We allow trailing ; in files as normal for SQL...
         line[len] = 0;
         dosql (line);
      }
      if (line)
         free (line);
   } else
      while (poptPeekArg (popt))
         dosql (poptGetArg (popt));
   if (trans)
      dosql ("COMMIT");
   if (!expand)
      sql_close (&sql);
#ifndef NOXML
   if (xml)
   {
      if (headers)
      {
         if (sqlhost)
            xml_attribute_set (xml, "host", sqlhost);
         xml_attribute_set (xml, "database", sqldatabase);
      }
      if (xmlout)
         xml_write (stdout, xml);
      if (jsonout)
         xml_write_json (stdout, xml);
      xml = xml_tree_delete (xml);
   }
#endif
   return ret;
}
