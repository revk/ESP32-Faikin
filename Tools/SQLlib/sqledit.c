// Simple SQL table edit designed to be used in a script with envcgi parsing get/post
// Copyright (c) 2011 Adrian Kennard Andrews & Arnold Ltd

#include <stdio.h>
#include <string.h>
#include <popt.h>
#include <time.h>
#include <sys/time.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <ctype.h>
#include <err.h>
#include <syslog.h>
#include <execinfo.h>

#include "sqllib.h"

void
htmlwrite (const char *p)
{
   if (!p)
   {
      printf ("<i>null</i>");
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
      else if (*p == '"')
         printf ("&quot;");
      else if ((unsigned char) *p < ' ')
         printf ("&#%u;", (unsigned char) *p);
      else
         putchar (*p);
      p++;
   }
}

void
urlwrite (char *p)
{
   if (!p)
      return;
   while (*p)
   {
      if (*p == ' ')
         putchar ('+');
      else if (*p == '+' || *p == '=' || *p == '?' || *p == '#' || *p == '&' || *p == '<' || *p == '>' || *p == '\'' || *p == '"'
               || *p < ' ')
         printf ("%02X", (unsigned char) *p);
      else
         putchar (*p);
      p++;
   }
}

int
main (int argc, const char *argv[])
{
   int c;
   const char *sqlconf = NULL;
   const char *sqlhost = NULL;
   const char *sqluser = NULL;
   const char *sqlpass = NULL;
   const char *sqldatabase = NULL;
   const char *sqltable = NULL;
   const char *hidden = NULL;
   const char *form = "?";
   int cmdlist = 0,
      cmdedit = 0,
      cmdsave = 0,
      cmdnew = 0,
      cmdview = 0;
   int allowerase = 0;
   int sqllimit = 0;
   SQL sql;
   SQL_RES *res;
   SQL_ROW row;

   poptContext optCon;          // context for parsing command-line options
   const struct poptOption optionsTable[] = {
      {"sql-conf", 0, POPT_ARG_STRING, &sqlconf, 0, "SQL Config", "filename"},
      {"sql-host", 0, POPT_ARG_STRING, &sqlhost, 0, "SQL Server", "hostname"},
      {"sql-user", 0, POPT_ARG_STRING, &sqluser, 0, "SQL User", "username"},
      {"sql-pass", 0, POPT_ARG_STRING, &sqlpass, 0, "SQL Password", "password"},
      {"sql-database", 'd', POPT_ARG_STRING, &sqldatabase, 0, "SQL Database", "database"},
      {"sql-table", 't', POPT_ARG_STRING, &sqltable, 0, "SQL Table", "table"},
      {"list", 'l', POPT_ARG_NONE, &cmdlist, 0, "List records", NULL},
      {"edit", 'e', POPT_ARG_NONE, &cmdedit, 0, "Edit records (keys in environment)", NULL},
      {"save", 's', POPT_ARG_NONE, &cmdsave, 0, "Save records (data in environment)", NULL},
      {"new", 'n', POPT_ARG_NONE, &cmdnew, 0, "New record", NULL},
      {"view", 'r', POPT_ARG_NONE, &cmdview, 0, "View record", NULL},
      {"allow-erase", 0, POPT_ARG_NONE, &allowerase, 0, "Allow erase (on edit/save)", NULL},
      {"limit", 0, POPT_ARG_INT, &sqllimit, 0, "Limit for list", NULL},
      {"hidden", 0, POPT_ARG_STRING, &hidden, 0, "Add hidden fields", "env-var-name"},
      {"form", 'f', POPT_ARG_STRING, &form, 0, "form action", "url"},
      {"debug", 'v', POPT_ARG_NONE, &sqldebug, 0, "Debug", 0},
      POPT_AUTOHELP {NULL, 0, 0, NULL, 0}
   };

   optCon = poptGetContext (NULL, argc, argv, optionsTable, 0);
   //poptSetOtherOptionHelp (optCon, "");

   if ((c = poptGetNextOpt (optCon)) < -1)
      errx (1, "%s: %s\n", poptBadOption (optCon, POPT_BADOPTION_NOALIAS), poptStrerror (c));

   if (!sqldatabase && poptPeekArg (optCon))
      sqldatabase = poptGetArg (optCon);
   if (!sqltable && poptPeekArg (optCon))
      sqltable = poptGetArg (optCon);

   if (poptPeekArg (optCon) || !sqldatabase || !sqltable || (cmdlist + cmdedit + cmdsave + cmdnew + cmdview) != 1)
   {
      poptPrintUsage (optCon, stderr, 0);
      return -1;
   }

   if (!sqlconf)
      sqlconf = getenv ("SQL_CNF_FILE");

   sql_real_connect (&sql, sqlhost, sqluser, sqlpass, sqldatabase, 0, NULL, 0, 1, sqlconf);

   typedef struct fdef_s fdef_t;
   struct fdef_s
   {
      fdef_t *next;
      char *name;
      char **elist;
      char *defval;
      char *comment;
      char hide:1;
   };
   fdef_t *fdef = NULL;
   // enums and defaults
   res = sql_safe_query_use (&sql, sql_printf ("SHOW FULL COLUMNS FROM `%s`", sqltable));
   while ((row = sql_fetch_row (res)))
   {
      fdef_t *n = malloc (sizeof (fdef_t));
      memset (n, 0, sizeof (*n));
      n->name = strdup (sql_colz (res, "Field"));
      char *t = sql_col (res, "Type");
      if (t && *t && !strncmp (t, "enum(", 5))
      {
         // count entries in list...
         char *q;
         int c = 0;
         q = t + 5;
         while (*q == '\'')
         {
            q++;
            while (*q)
            {
               if (*q == '\'' && q[1] == '\'')
                  q++;
               else if (*q == '\\' && q[1])
                  q++;
               else if (*q == '\'')
                  break;
               q++;
            }
            if (*q == '\'')
               q++;
            if (*q == ',')
               q++;
            c++;
         }
         n->elist = malloc (sizeof (char *) * (c + 1));
         c = 0;
         q = t + 5;
         while (*q == '\'')
         {
            q++;
            int l = 0;
            char *z = q;
            while (*z)
            {
               if (*z == '\'' && z[1] == '\'')
                  z++;
               else if (*z == '\\' && z[1])
                  z++;
               else if (*z == '\'')
                  break;
               z++;
               l++;
            }
            n->elist[c] = malloc (l + 1);
            l = 0;
            while (*q)
            {
               n->elist[c][l] = *q;
               if (*q == '\'' && q[1] == '\'')
                  q++;
               else if (*q == '\\' && q[1])
                  q++;
               else if (*q == '\'')
                  break;
               q++;
               l++;
            }
            n->elist[c][l] = 0;
            if (*q == '\'')
               q++;
            if (*q == ',')
               q++;
            c++;
         }
         n->elist[c] = NULL;
      }
      t = sql_col (res, "Default");
      if (t && *t)
      {
         n->defval = strdup (t);
         if (!strcasecmp (n->defval, "CURRENT_TIMESTAMP") || !strcasecmp (n->defval, "CURRENT_TIMESTAMP()"))
            n->hide = 1;
      }
      t = sql_col (res, "Comment");
      if (t && *t)
      {
         n->comment = strdup (t);
         if (*n->comment == '*')
            n->hide = 1;
      }
      n->next = fdef;
      fdef = n;
   }
   sql_free_result (res);

   if (cmdlist)
   {                            // List entries in a table
      sql_s_t s = { 0 };
      sql_sprintf (&s, "SELECT * FROM `%s`", sqltable);
      if (sqllimit)
         sql_sprintf (&s, " LIMIT %d", sqllimit);
      res = sql_safe_query_use_s (&sql, &s);
      int f,
        n = sql_num_fields (res);
      SQL_FIELD *fields = mysql_fetch_fields (res);
      printf ("<table class='%s'>\n", sqldatabase);
      {
         printf ("<thead class='%s'>\n", sqltable);
         printf ("<tr>");
         void th (void)
         {
            printf ("<th>");
            htmlwrite (fields[f].name);
            printf ("</th>");
         }
         for (f = 0; f < n; f++)
            if (fields[f].flags & (PRI_KEY_FLAG | UNIQUE_KEY_FLAG))
               th ();
         for (f = 0; f < n; f++)
            if (!(fields[f].flags & (PRI_KEY_FLAG | UNIQUE_KEY_FLAG)) && (fields[f].flags & MULTIPLE_KEY_FLAG))
               th ();
         for (f = 0; f < n; f++)
            if (!(fields[f].flags & (PRI_KEY_FLAG | UNIQUE_KEY_FLAG | MULTIPLE_KEY_FLAG)))
               th ();
         printf ("</tr>\n");
         printf ("</thead>\n");
      }
      printf ("<tbody class='%s'>\n", sqltable);
      while ((row = sql_fetch_row (res)))
      {
         void td (int l)
         {
            printf ("<td>");
            if (l)
            {                   // link
               printf ("<a href='");
               int q;
               char c = '?';
               for (q = 0; q < n; q++)
                  if (fields[q].flags & (PRI_KEY_FLAG | UNIQUE_KEY_FLAG))
                  {
                     putchar (c);
                     urlwrite (fields[q].name);
                     if (row && row[q])
                     {
                        putchar ('=');
                        urlwrite (row[q]);
                     }
                     c = '&';
                  }
               printf ("'>");
            }
            if (row)
               htmlwrite (row[f]);
            if (l)
               printf ("</a>");
            printf ("</td>");
         }
         printf ("<tr>");
         for (f = 0; f < n; f++)
            if (fields[f].flags & (PRI_KEY_FLAG | UNIQUE_KEY_FLAG))
               td (1);
         for (f = 0; f < n; f++)
            if (!(fields[f].flags & (PRI_KEY_FLAG | UNIQUE_KEY_FLAG)) && (fields[f].flags & MULTIPLE_KEY_FLAG))
               td (0);
         for (f = 0; f < n; f++)
            if (!(fields[f].flags & (PRI_KEY_FLAG | UNIQUE_KEY_FLAG | MULTIPLE_KEY_FLAG)))
               td (0);
         printf ("</tr>\n");
      }
      printf ("</tbody>\n");
      printf ("</table>\n");
      sql_free_result (res);
   }

   if (cmdedit || cmdnew || cmdview)
   {                            // form for editing
      sql_s_t s = { 0 };
      sql_sprintf (&s, "SELECT * FROM `%s` LIMIT 1", sqltable); // just to get fields
      res = sql_safe_query_use_s (&sql, &s);
      int f,
        n = sql_num_fields (res);
      SQL_FIELD *fields = mysql_fetch_fields (res);
      row = NULL;
      if (cmdedit || cmdview)
      {
         sql_sprintf (&s, "SELECT * FROM `%s`", sqltable);      // get the actual data
         char *c = "WHERE";
         for (f = 0; f < n; f++)
            if (fields[f].flags & (PRI_KEY_FLAG | UNIQUE_KEY_FLAG))
            {
               char *v = getenv (fields[f].name);
               sql_sprintf (&s, " %s `%s`=%#s", c, fields[f].name, v);
               c = "AND";
            }
         sql_free_result (res);
         res = sql_safe_query_use_s (&sql, &s);
         n = sql_num_fields (res);
         fields = mysql_fetch_fields (res);
         row = sql_fetch_row (res);
      }
      printf ("<form method='post' action='%s'>", form);
      if (!cmdview)
      {
         printf ("<input type='submit' value='Save'/>");
         if (allowerase)
            printf ("<input type='submit' value='Erase' name='__ERASE__'/>");
      }
      if (hidden)
      {
         char *v = getenv (hidden);     // TODO CSV?
         if (v)
         {
            printf ("<input type='hidden' name='");
            htmlwrite (hidden);
            printf ("' value='");
            htmlwrite (v);
            printf ("'/>\n");
         }
      }
      printf ("<table class='%s'>\n", sqldatabase);
      printf ("<tbody class='%s'>\n", sqltable);
      void td (int l)
      {
         fdef_t *q;
         for (q = fdef; q && strcmp (q->name, fields[f].name); q = q->next);
         if (q && q->hide)
         {
            if (cmdnew)
               return;
            l = 2;
         }
         printf ("<tr>");
         printf ("<td>");
         if (!(fields[f].flags & NOT_NULL_FLAG))
            printf ("<label for='C%d'>", f);
         htmlwrite (fields[f].name);
         if (!(fields[f].flags & NOT_NULL_FLAG))
            printf ("</label>");
         printf ("</td>");
         printf ("<td>");
         if (l && cmdedit)
         {
            if (l < 2)
            {
               printf ("<input type='hidden' name=\"");
               htmlwrite (fields[f].name);
               printf ("\"");
               if (row && row[f])
               {
                  printf (" value=\"");
                  htmlwrite (row[f]);
                  printf ("\"");
               }
               printf ("/>");
            }
            if (row)
               htmlwrite (row[f]);      // key field, cannot edit
         } else
         {
            if (!q)
               return;
            char *v = NULL;     // value
            if (cmdnew)
               v = getenv (fields[f].name) ? : q->defval;
            else if (row)
               v = row[f];
            if (!(fields[f].flags & NOT_NULL_FLAG))
            {
               printf ("<input type='checkbox' id='C%d'", f);
               if (v)
                  printf (" checked='checked'");
               printf
                  (" onchange='document.getElementById(\"E%d\").disabled=!this.checked;if(this.checked)document.getElementById(\"E%d\").focus();'",
                   f, f);
               printf ("/>");
            }
            if (q->elist)
            {
               printf ("<select id='E%d'", f);
               printf (" name=\"");
               htmlwrite (fields[f].name);
               printf ("\"");
               if ((!(fields[f].flags & NOT_NULL_FLAG) && !v) || (fields[f].flags & AUTO_INCREMENT_FLAG))
                  printf (" disabled='disabled'");
               printf (">");
               char **e;
               for (e = q->elist; *e; e++)
               {
                  printf ("<option");
                  if (v && !strcmp (v, *e))
                     printf (" selected='selected'");
                  printf (">");
                  htmlwrite (*e);
                  printf ("</option>");
               }
               printf ("</select>");
            } else
            {
               int l = fields[f].length;
               if (l == 65535)
               {
                  printf ("<textarea cols='50' rows='10'");
                  printf (" id='E%d'", f);
                  printf (" name=\"");
                  htmlwrite (fields[f].name);
                  printf ("\"");
                  if ((!(fields[f].flags & NOT_NULL_FLAG) && !v) || (fields[f].flags & AUTO_INCREMENT_FLAG))
                     printf (" disabled='disabled'");
                  printf (">");
                  if (v)
                     htmlwrite (v);
                  printf ("</textarea>");
               } else
               {
                  printf ("<input");
                  printf (" id='E%d'", f);
                  printf (" name=\"");
                  htmlwrite (fields[f].name);
                  printf ("\"");
                  if ((!(fields[f].flags & NOT_NULL_FLAG) && !v) || (fields[f].flags & AUTO_INCREMENT_FLAG))
                     printf (" disabled='disabled'");
                  if (l)
                     printf (" maxlength='%d'", l);
                  if (!l || l > 50)
                     l = 50;
                  printf (" size='%d'", l);
                  if (v)
                  {
                     printf (" value=\"");
                     htmlwrite (v);
                     printf ("\"");
                  }
                  printf ("/>");
               }
            }
         }
         printf ("</td>");
         if (q && q->comment && *q->comment)
         {
            printf ("<td>");
            htmlwrite (*q->comment == '*' ? q->comment + 1 : q->comment);
            printf ("</td>");
         }
         printf ("</tr>\n");
      }
      for (f = 0; f < n; f++)
         if (fields[f].flags & (PRI_KEY_FLAG | UNIQUE_KEY_FLAG))
            td (1);
      for (f = 0; f < n; f++)
         if (!(fields[f].flags & (PRI_KEY_FLAG | UNIQUE_KEY_FLAG)) && (fields[f].flags & MULTIPLE_KEY_FLAG))
            td (0);
      for (f = 0; f < n; f++)
         if (!(fields[f].flags & (PRI_KEY_FLAG | UNIQUE_KEY_FLAG | MULTIPLE_KEY_FLAG)))
            td (0);
      printf ("</tbody>\n");
      printf ("</table>\n");
      if (!cmdview)
      {
         printf ("<input type='submit' value='Save'/>");
         if (allowerase)
            printf ("<input type='submit' value='Erase' name='__ERASE__'/>");
      }
      printf ("</form>");
      sql_free_result (res);
   }

   if (cmdsave && allowerase && getenv ("__ERASE__"))
   {
      sql_s_t s = { 0 };
      sql_sprintf (&s, "SELECT * FROM `%s` LIMIT 1", sqltable); // just to get fields
      res = sql_safe_query_use_s (&sql, &s);
      int f,
        n = sql_num_fields (res);
      SQL_FIELD *fields = mysql_fetch_fields (res);
      sql_sprintf (&s, "DELETE FROM `%s`", sqltable);
      int c = 0;
      for (f = 0; f < n; f++)
         if (fields[f].flags & (PRI_KEY_FLAG | UNIQUE_KEY_FLAG))
         {
            char *v = getenv (fields[f].name);
            sql_sprintf (&s, " %s `%s`", c ? "AND" : "WHERE", fields[f].name);
            if (!v)
               sql_sprintf (&s, " IS NULL");
            else
               sql_sprintf (&s, "=%#s", v);
            c++;
         }
      sql_free_result (res);
      if (c)
         sql_safe_query_s (&sql, &s);
   } else if (cmdsave)
   {                            // Save values
      sql_s_t s = { 0 };
      sql_sprintf (&s, "SELECT * FROM `%s` LIMIT 1", sqltable); // just to get fields
      res = sql_safe_query_use_s (&sql, &s);
      int f,
        n = sql_num_fields (res);
      SQL_FIELD *fields = mysql_fetch_fields (res);
      sql_sprintf (&s, "REPLACE INTO `%s` ", sqltable);
      char *c = "(";
      for (f = 0; f < n; f++)
      {
         fdef_t *q;
         for (q = fdef; q && strcmp (q->name, fields[f].name); q = q->next);
         if (!q || q->hide)
            continue;
         sql_sprintf (&s, "%s`%s`", c, fields[f].name);
         c = ",";
      }
      sql_sprintf (&s, ") VALUES ");
      c = "(";
      for (f = 0; f < n; f++)
      {
         fdef_t *q;
         for (q = fdef; q && strcmp (q->name, fields[f].name); q = q->next);
         if (!q || q->hide)
            continue;
         sql_sprintf (&s, "%s%#s", c, getenv (fields[f].name));
         c = ",";
      }
      sql_sprintf (&s, ")");
      sql_free_result (res);
      sql_safe_query_s (&sql, &s);
   }
   sql_close (&sql);
   return 0;
}
