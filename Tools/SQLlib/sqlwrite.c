// (c) 1997/2002 Andrews & Arnold Ltd
// This software is provided under the terms of the GPL v2 or later.
// This software is provided free of charge with a full "Money back" guarantee.
// Use entirely at your own risk. We accept no liability. If you don't like that - don't use it.

// Adrian Kennard
// Update or insert in to an SQL table using command line
// and environment variables... Syntax :
//
//  sqlwrite database table {[-]name[=[value]]}
//
// command line take priority over environment variables.
// -name means that the name is assumed missing
// name without an = means its default value is used (i.e. assumed not missing)
// Only updates/stores variables found in command line or environment and not prefixed -
// Works out what is the key so that it can update existing records
// Prints final value of first variable if one is specified (typically it is auto inc)
//

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <popt.h>
#include <err.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "sqllib.h"

//#define DEBUG

SQL sql;
SQL_RES *res;
SQL_ROW row = 0;
SQL_FIELD *field;

void
fail (char *e)
{
   fprintf (stderr, "Error: %s [%s]\n", sql_error (&sql), e);
   sql_close (&sql);
   exit (-1);
}

int
main (int argc, const char *argv[])
{
   int quiet = 0;
   int showdiff = 0;
   int onlylisted = 0;
   int nulllisted = 0;
   const char *sqlhost = NULL,
      *sqldatabase = NULL,
      *sqltable = NULL,
      *sqluser = NULL,
      *sqlpass = NULL,
      *sqlconf = NULL;
   unsigned int sqlport = 0;
   int count = 0;
   int n,
     f;
   char *e,
     s;
   sql_s_t query = { 0 };
   sql_s_t where = { 0 };

   poptContext popt;            // context for parsing command-line options
   const struct poptOption optionsTable[] = {
      {"sql-conf", 0, POPT_ARGFLAG_SHOW_DEFAULT | POPT_ARG_STRING, &sqlconf, 0, "Client config file", "filename"},
      {"sql-host", 'h', POPT_ARG_STRING, &sqlhost, 0, "SQL server host",
       "hostname/ip"},
      {"sql-port", 0, POPT_ARG_INT, &sqlport, 0, "SQL server port",
       "port"},
      {"sql-user", 'u', POPT_ARG_STRING, &sqluser, 0, "SQL username",
       "username"},
      {"sql-pass", 'p', POPT_ARG_STRING, &sqlpass, 0, "SQL password",
       "password"},
      {"sql-database", 'd', POPT_ARG_STRING, &sqldatabase, 0, "SQL database",
       "database"},
      {"sql-table", 't', POPT_ARG_STRING, &sqltable, 0, "SQL table", "table"},
      {"show-diff", 's', POPT_ARG_NONE, &showdiff, 0, "Show diff, don't update",
       0},
      {"only-listed", 'o', POPT_ARG_NONE, &onlylisted, 0, "Only updated fields listed on command line", 0},
      {"null-listed", 'n', POPT_ARG_NONE, &nulllisted, 0, "Null updated fields listed on command line if missing", 0},
      {"count", 'c', POPT_ARG_NONE, &count, 0, "Report change count (normally just $status)", 0},
      {"quiet", 'q', POPT_ARG_NONE, &quiet, 0, "Quiet", 0},
      {"debug", 'v', POPT_ARG_NONE, &sqldebug, 0, "Debug", 0},
      POPT_AUTOHELP {NULL, 0, 0, NULL, 0}
   };

   popt = poptGetContext (NULL, argc, argv, optionsTable, 0);
   poptSetOtherOptionHelp (popt, "[database] [table] {name[=[value]/$var/@file]}");
   {
      int c = poptGetNextOpt (popt);
      if (c < -1)
         errx (1, "%s: %s\n", poptBadOption (popt, POPT_BADOPTION_NOALIAS), poptStrerror (c));
   }

   if (!sqldatabase && poptPeekArg (popt))
      sqldatabase = poptGetArg (popt);
   if (!sqltable && poptPeekArg (popt))
      sqltable = poptGetArg (popt);
   if (!sqldatabase || !sqltable)
   {
      poptPrintUsage (popt, stderr, 0);
      return 2;
   }

   char **values = malloc (argc * sizeof (*values));
   int valuen = 0;
   while (valuen < argc && (values[valuen] = (char *) poptGetArg (popt)))
      valuen++;

   // updating local environment variables based on command line, only the "default" entries need later processing
   for (n = 0; n < valuen; n++)
   {
      char *p = values[n];
      while (*p && (isalnum (*p) || *p == '_' || *p == '-'))
         p++;
      if (*p == '=')
      {
         putenv (values[n]);
      } else if (*p == '$')
      {                         // defined by reference to environment variable of another name...
         *p = 0;
         setenv (values[n], getenv (p + 1), 1);
         *p = '$';
      } else if (*p == '@')
      {                         // defined as reference to contents of a file
         int f = open (p + 1, O_RDONLY);
         if (f < 0)
            err (1, "%s", p + 1);
         struct stat s = { };
         fstat (f, &s);
         if (!fstat (f, &s))
         {
            char *buf = malloc (s.st_size + 1);
            if (read (f, buf, s.st_size) != s.st_size)
               errx (1, "Bad read %s", p + 1);
            buf[s.st_size] = 0;
            *p = 0;
            setenv (values[n], buf, 1);
            *p = '@';
            free (buf);
         }
         close (f);
      } else if (!onlylisted)
         unsetenv (values[n]);  // using default even if exists, so not taken from environment
   }

   if (!sqlconf)
      sqlconf = getenv ("SQL_CNF_FILE");

   // connect to database
   sql_real_connect (&sql, sqlhost, sqluser, sqlpass, sqldatabase, sqlport, NULL, 0, 1, sqlconf);
   sql_transaction (&sql);
   // construct a WHERE line based on a unique key for which we have values
   res = sql_safe_query_store_free (&sql, sql_printf ("SHOW INDEX FROM `%#S`", sqltable));
   {
      char key[257],
        ok = 0;
      *key = 0;
      while ((row = sql_fetch_row (res)))
      {
         e = getenv (row[4]);
         if (strcmp (row[2], key))
         {                      // start new key
            if (ok)
               break;           // found a key already
            strcpy (key, row[2]);
            sql_free_s (&where);
            ok = 1;
         }
         if (*row[1] == '1')
            continue;           // only interested in unique keys
         if (!e)
            ok = 0;             // missing part of a key
         else
         {                      // unique key
            if (!sql_len_s (&where))
               sql_sprintf (&where, "WHERE ");
            else
               sql_sprintf (&where, " AND ");
            sql_sprintf (&where, "`%#S`=", row[4]);
            if (isdigit (e[0]) && isdigit (e[1]) && e[2] == '/'
                && isdigit (e[3]) && isdigit (e[4]) && e[5] == '/' && isdigit (e[6]) && isdigit (e[7]) && isdigit (e[8])
                && isdigit (e[9]) && (!e[10]
                                      || (isspace (e[10]) && isdigit (e[11]) && isdigit (e[12]) && e[13] == ':' && isdigit (e[14])
                                          && isdigit (e[15]) && e[16] == ':' && isdigit (e[17]) && isdigit (e[18]) && !e[19])))
            {                   // funny date format
               sql_sprintf (&where, "'%.4s-%.2s-%.2s", e + 6, e + 3, e + 0);
               while (*e && (isdigit (*e) || *e == ':' || *e == ' '))
                  sql_sprintf (&where, "%c", *e++);
               sql_sprintf (&where, "'");
            } else
               sql_sprintf (&where, "%#s", e);
         }
      }
      if (!ok)
         sql_free_s (&where);
   }
   sql_free_result (res);
   if (!sql_len_s (&where))
      return fprintf (stderr, "Could not find unique key set for which we have data\n");
   res = sql_safe_query_store_free (&sql, sql_printf ("SELECT * FROM `%#S` %s", sqltable, sql_close_s (&where)));
   row = sql_fetch_row (res);
   field = sql_fetch_field (res);
   sql_free_s (&query);
   if (row)
   {                            // UPDATE
      sql_sprintf (&query, "UPDATE `%#S` SET", sqltable);
      s = ' ';
   } else
   {                            // INSERT
      sql_sprintf (&query, "INSERT INTO `%#S` ", sqltable);
      s = '(';
      for (f = 0; f < sql_num_fields (res); f++)
      {
         for (n = 0; n < valuen && strcmp (values[n], field[f].name); n++);
         if (n < valuen || (!onlylisted && getenv (field[f].name)))
         {
            sql_sprintf (&query, "%c`%#S`", s, field[f].name);
            s = ',';
         }
      }
      sql_sprintf (&query, ") VALUES ");
      s = '(';
   }
   // values
   for (f = 0; f < sql_num_fields (res); f++)
   {
      for (n = 0; n < valuen && strcmp (values[n], field[f].name); n++);
      e = getenv (field[f].name);
      if (!e && !nulllisted && onlylisted)
         continue;
      if (n < valuen || (!onlylisted && e))
      {
         void showfield (char *p)
         {
            if (!p)
            {
               printf ("<i>NULL</i>");
               return;
            }
            while (*p)
            {
               if (*p == '<')
                  printf ("&lt;");
               else if (*p == '>')
                  printf ("&gt;");
               else if (*p == '&')
                  printf ("&amp;");
               else if (*p < ' ')
                  putchar (' ');
               else
                  putchar (*p);
               p++;
            }
         }
         if (!e && n < valuen && ((field[f].def && *field[f].def) || (field[f].flags & NOT_NULL_FLAG)))
            e = field[f].def;
         if (field[f].flags & SET_FLAG)
            for (char *p = e; *p; p++)
               if (*p == '\t')
                  *p = ',';     // SET, so comma not tab
         sql_sprintf (&query, "%c", s);
         s = ',';
         if (row)
         {
            sql_sprintf (&query, "`%#S`=", field[f].name);
            if (showdiff && strcmp (e ? : "", row[f] ? : ""))
            {
               if (!
                   ((!e || !*e) && (!row[f] || !strncmp (row[f], "0000", 4))
                    && (field[f].type == FIELD_TYPE_DATE || field[f].type == MYSQL_TYPE_NEWDATE
                        || field[f].type == FIELD_TYPE_TIMESTAMP || field[f].type == FIELD_TYPE_DATETIME
                        || field[f].type == FIELD_TYPE_TIME)))
               {
                  if (showdiff++ == 1)
                     printf ("Change");
                  printf (" %s=", field[f].name);
                  showfield (row[f]);
                  printf ("→");
                  showfield (e);
               }
            }
         } else if (showdiff)
         {
            if (showdiff++ == 1)
               printf ("New");
            printf (" %s=", field[f].name);
            showfield (e);
         }
         if (!e)
         {
            sql_sprintf (&query, "NULL");
         } else
         {
            if ((field[f].type == FIELD_TYPE_DATE || field[f].type == MYSQL_TYPE_NEWDATE
                 || field[f].type == FIELD_TYPE_TIMESTAMP
                 || field[f].type == FIELD_TYPE_DATETIME) && isdigit (e[0])
                && isdigit (e[1]) && e[2] == '/' && isdigit (e[3])
                && isdigit (e[4]) && e[5] == '/' && isdigit (e[6]) && isdigit (e[7]) && isdigit (e[8]) && isdigit (e[9]) && (!e[10]
                                                                                                                             ||
                                                                                                                             (isspace
                                                                                                                              (e
                                                                                                                               [10])
                                                                                                                              &&
                                                                                                                              isdigit
                                                                                                                              (e
                                                                                                                               [11])
                                                                                                                              &&
                                                                                                                              isdigit
                                                                                                                              (e
                                                                                                                               [12])
                                                                                                                              &&
                                                                                                                              e[13]
                                                                                                                              == ':'
                                                                                                                              &&
                                                                                                                              isdigit
                                                                                                                              (e
                                                                                                                               [14])
                                                                                                                              &&
                                                                                                                              isdigit
                                                                                                                              (e
                                                                                                                               [15])
                                                                                                                              &&
                                                                                                                              e[16]
                                                                                                                              == ':'
                                                                                                                              &&
                                                                                                                              isdigit
                                                                                                                              (e
                                                                                                                               [17])
                                                                                                                              &&
                                                                                                                              isdigit
                                                                                                                              (e
                                                                                                                               [18])
                                                                                                                              &&
                                                                                                                              !e
                                                                                                                              [19])))
            {                   // funny date format
               sql_sprintf (&query, "'%.4s-%.2s-%.2s", e + 6, e + 3, e + 0);
               while (*e && (isdigit (*e) || *e == ':' || *e == ' '))
                  sql_sprintf (&query, "%c", *e++);
               sql_sprintf (&query, "'");
            } else if (field[f].type == FIELD_TYPE_DATE || field[f].type == MYSQL_TYPE_NEWDATE)
               sql_sprintf (&query, "%#10U", sql_time_utc (e));
            else if (field[f].type == FIELD_TYPE_TIMESTAMP || field[f].type == FIELD_TYPE_DATETIME)
               sql_sprintf (&query, "%#U", sql_time_utc (e));
            else if (!*e && field[f].type == FIELD_TYPE_TIME)
               sql_sprintf (&query, "%#s", "00:00:00");
            else if (!*e
                     && (field[f].type == FIELD_TYPE_NEWDECIMAL || field[f].type == FIELD_TYPE_DECIMAL
                         || field[f].type == FIELD_TYPE_TINY || field[f].type == FIELD_TYPE_SHORT
                         || field[f].type == FIELD_TYPE_LONG || field[f].type == FIELD_TYPE_DOUBLE
                         || field[f].type == FIELD_TYPE_LONGLONG || field[f].type == FIELD_TYPE_INT24))
               sql_sprintf (&query, "%#s", "0");
            else
               sql_sprintf (&query, "%#s", e);
         }
      }
   }
   if (!row)
      sql_sprintf (&query, ")");
   else
      sql_sprintf (&query, " %s", sql_close_s (&where));
   sql_free_result (res);
   sql_free_s (&where);
   if (s == ',' && !showdiff)
      sql_safe_query_s (&sql, &query);
   int changed = sql_affected_rows (&sql);
   if (!quiet)
   {
      if (count)
         printf ("%d", changed);
      else
      {
         int id = sql_insert_id (&sql);
         if (id)
            printf ("%d", id);
      }
   }
   sql_safe_commit (&sql);
   sql_close (&sql);
   return changed;
}
