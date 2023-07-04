// SQL client library
// Copyright (c) Adrian Kennard 2007
// This software is provided under the terms of the GPL v2 or later.
// This software is provided free of charge with a full "Money back" guarantee.
// Use entirely at your own risk. We accept no liability. If you don't like that - don't use it.

#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <err.h>
#include <stdlib.h>
#include <ctype.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef	STRINGDECIMAL
#include "stringdecimal/stringdecimal.h"
#endif
#include "sqllib.h"

int sqldebug = 0;               // Set 1 to print queries & errors, 2 for just errors, -ve to not do any actual updates just print
int sqlsyslogerror = -1;
int sqlsyslogquery = -1;
const char *sqlcnf = "~/.my.cnf";       // Default
const char *capem = "/etc/mysql/cacert.pem";

SQL *
sql_real_connect (MYSQL * sql, const char *host, const char *user, const char *passwd, const char *db, unsigned int port,
                  const char *unix_socket, unsigned long client_flag, char safe, const char *mycnf)
{                               // Connect but check config file
   const char *sslca = NULL;
   const char *sslcert = NULL;
   const char *sslkey = NULL;
   const char *skipssl = NULL;
   if (!access (capem, R_OK))
      sslca = capem;            // Default CA certificate PEM file, if present we assume we should use SSL/TLS
   if (safe && sql)
      sql_init (sql);
   if (!mycnf)
   {
      mycnf = getenv ("SQL_CNF");
      if (!mycnf)
         mycnf = sqlcnf;
   }
   if (mycnf && *mycnf)
   {
      char *fn = (char *) mycnf;
      if (*fn == '~')
         if (asprintf (&fn, "%s%s", getenv ("MYSQL_HOME") ? : getenv ("HOME") ? : "/etc", fn + 1) < 0)
            errx (255, "malloc at line %d", __LINE__);
      struct stat s;
      if (!stat (fn, &s) && S_ISREG (s.st_mode))
      {
         if ((s.st_mode & 0577) != 0400)
            warnx ("%s is not user only read, not using.", fn);
         else
         {
            FILE *f = fopen (fn, "r");
            if (!f)
               warnx ("Cannot open %s", fn);
            else
            {
               char *l = NULL;
               size_t lspace = 0;
               ssize_t len = 0;
               while ((len = getline (&l, &lspace, f)) > 0)
                  if (!strncasecmp (l, "[client]", 8))
                     break;
               while ((len = getline (&l, &lspace, f)) > 0 && *l != '[')
               {                // read client lines
                  char *e = l + len,
                     *p;
                  while (e > l && e[-1] < ' ')
                     e--;
                  *e = 0;
                  for (p = l; isalnum (*p) || *p == '-'; p++);
                  char *v = NULL;
                  for (v = p; isspace (*v); v++);
                  if (*v++ == '=')
                     for (; isspace (*v); v++);
                  else
                     v = NULL;
                  if (v && *v == '\'' && e > v && e[-1] == '\'')
                  {             // Quoted
                     v++;
                     *--e = 0;
                  }
                  const char **set = NULL;
                  if ((p - l) == 4 && !strncasecmp (l, "port", p - l))
                  {
                     if (!port && v)
                        port = atoi (v);
                  } else if ((p - l) == 4 && !strncasecmp (l, "user", p - l))
                     set = &user;
                  else if ((p - l) == 4 && !strncasecmp (l, "host", p - l))
                     set = &host;
                  else if ((p - l) == 8 && !strncasecmp (l, "password", p - l))
                     set = &passwd;
                  else if ((p - l) == 8 && !strncasecmp (l, "database", p - l))
                     set = &db;
                  else if ((p - l) == 6 && !strncasecmp (l, "ssl-ca", p - l))
                     set = &sslca;
                  else if ((p - l) == 8 && !strncasecmp (l, "ssl-cert", p - l))
                     set = &sslcert;
                  else if ((p - l) == 7 && !strncasecmp (l, "ssl-key", p - l))
                     set = &sslkey;
                  else if ((p - l) == 8 && !strncasecmp (l, "skip-ssl", p - l))
                     set = &skipssl;
                  if (set && !*set && v)
                  {
                     if (!*v)
                        *set = NULL;    // Allow unset
                     else
                        *set = strdupa (v);
                  }
               }
               if (l)
                  free (l);
               fclose (f);
            }
         }
      }
      if (fn != mycnf)
         free (fn);
   }
   _Bool reconnect = 1;
   sql_options (sql, MYSQL_OPT_RECONNECT, &reconnect);
   int allow = 1;
   sql_options (sql, MYSQL_OPT_LOCAL_INFILE, &allow);   // Was previously allowed
   if (host && (!*host || !strcasecmp (host, "localhost")))
      host = NULL;              // A blank host as local connection
   if (host && (sslkey || sslcert || sslca) && !skipssl)
   {                            // SSL (TLS) settings
#if MYSQL_VERSION < 10
      errx (255, "No SSL available in this SQL library build");
#else
      if (sslkey)
         sql_options (sql, MYSQL_OPT_SSL_KEY, sslkey);
      if (sslcert)
         sql_options (sql, MYSQL_OPT_SSL_CERT, sslcert);
      if (sslca)
      {
         sql_options (sql, MYSQL_OPT_SSL_CA, sslca);
         int check = 1;
         sql_options (sql, MYSQL_OPT_SSL_VERIFY_SERVER_CERT, &check);
      }
      client_flag |= CLIENT_SSL;
#endif
   }
   SQL *s = mysql_real_connect (sql, host, user, passwd, db, port, unix_socket, client_flag);
   if (!s)
   {
      if (sqlsyslogerror >= 0)
         syslog (sqlsyslogerror, "%s", sql_error (sql));
      if (safe)
         errx (200, "SQL error accessing '%s': %s", host ? : "(local)", sql_error (sql));
      else if (sqldebug)
         fprintf (stderr, "SQL error accessing '%s': %s\n", host ? : "(local)", sql_error (sql));
   } else
   {
      sql_options (s, MYSQL_SET_CHARSET_NAME, "utf8mb4");       // Seems to be needed after connect?
      sql_set_character_set (s, "utf8mb4");     // Seems needed fro mariadb
   }
   return s;
}

int
sql_safe_select_db (SQL * sql, const char *db)
{
   if (sqlsyslogquery >= 0)
      syslog (sqlsyslogquery, "USE %s", db);
   if (sqldebug)
      fprintf (stderr, "USE %s\n", db);
   int e = sql_select_db (sql, db);
   if (e)
   {
      if (sqlsyslogerror >= 0)
         syslog (sqlsyslogerror, "USE %s", sql_error (sql));
      if (!sqldebug)
         fprintf (stderr, "SQL failed: %s\nUSE %s\n", sql_error (sql), db);
      errx (201, "SQL query failed");
   }
   return e;
}

void
sql_safe_query (SQL * sql, char *q)
{
   if (!q)
      return;
   if (sqldebug < 0)
   {                            // don't do
      if (sqlsyslogquery >= 0)
         syslog (sqlsyslogquery, "%s", q);
      fprintf (stderr, "%s\n", q);
      return;
   }
   int e = sql_query (sql, q);
   if (e == ER_LOCK_DEADLOCK)   // && strcasecmp(q, "COMMIT"))
      e = sql_query (sql, q);   // auto retry once for deadlock
   if (e == ER_LOCK_DEADLOCK)   // && strcasecmp(q, "COMMIT"))
      e = sql_query (sql, q);   // auto retry once for deadlock
   if (e)
   {
      if (!sqldebug)
         fprintf (stderr, "SQL failed (%s): %s\n%s\n", sql->db, sql_error (sql), q);
      errx (201, "SQL query failed");
   }
}

SQL_RES *
sql_safe_query_use (SQL * sql, char *q)
{
   if (!q)
      return NULL;
   SQL_RES *r;
   sql_safe_query (sql, q);
   r = sql_use_result (sql);
   if (!r)
   {
      if (!sqldebug)
         fprintf (stderr, "%s\n", q);
      errx (202, "SQL query no result");
   }
   return r;
}

SQL_RES *
sql_safe_query_store (SQL * sql, char *q)
{
   if (!q)
      return NULL;
   SQL_RES *r;
   if (sql_query (sql, q))
   {
      if (!sqldebug)
         fprintf (stderr, "SQL failed (%s): %s\n%s\n", sql->db, sql_error (sql), q);
      errx (201, "SQL query failed");
   }
   r = sql_store_result (sql);
   if (!r)
   {
      if (!sqldebug)
         fprintf (stderr, "%s\n", q);
      errx (202, "SQL query no result");
   } else if (sqldebug)
      fprintf (stderr, "(%llu row%s)\n", sql_num_rows (r), sql_num_rows (r) == 1 ? "" : "s");
   return r;
}

SQL_RES *
sql_query_use (SQL * sql, char *q)
{
   if (!q)
      return NULL;
   if (sql_query (sql, q))
      return NULL;
   return sql_use_result (sql);
}

SQL_RES *
sql_query_store (SQL * sql, char *q)
{
   if (!q)
      return NULL;
   if (sql_query (sql, q))
      return 0;
   return sql_store_result (sql);
}

int
sql_query (SQL * sql, char *q)
{
   if (!q)
      return 0;
   struct timeval a = { 0 }, b = { 0 };
   gettimeofday (&a, NULL);
   int r = sql_real_query (sql, q);
   gettimeofday (&b, NULL);
   long long us =
      ((long long) b.tv_sec * 1000000LL + (long long) b.tv_usec) - ((long long) a.tv_sec * 1000000LL + (long long) a.tv_usec);
   if (sqlsyslogquery >= 0)
      syslog (sqlsyslogquery, "%lluus: %s", us, q);
   if (sqlsyslogerror >= 0 && r)
   {
      if (sqlsyslogquery < 0)
         syslog (sqlsyslogerror, "%lluus: %s", us, q);
      syslog (sqlsyslogerror, "%s", sql_error (sql));
   }
   if (sqldebug == 1)
   {
      fprintf (stderr, "%llu.%06llus: %s\n", us / 1000000LL, us % 1000000LL, q);
      if (r)
         fprintf (stderr, "SQL failed (%s): %s\n", sql->db, sql_error (sql));
   }
   return r;
}

char *
sql_close_s (sql_s_t * q)
{                               // Close a string - return string (malloc'd)
   if (!q)
      return NULL;
   if (q->f)
   {
      fclose (q->f);
      q->f = NULL;
   }
   return q->string;
}

size_t
sql_len_s (sql_s_t * q)
{
   if (!q)
      return 0;
   if (q->f)
      fflush (q->f);
   return q->len;
}

void
sql_open_s (sql_s_t * q)
{                               // Open (or continue) a string
   if (!q)
      errx (1, "sql_s_t NULL");
   if (q->dummy)
   {
      warnx ("sql_s_t uninitialised len=%lu string=%p f=%p dummy=%llX", q->len, q->string, q->f, q->dummy);
      //*((char *) 0) = 0;        // Cause something valgrind can see
   }
   if (q->f)
      return;                   // already open, or not valid
   sql_free_s (q);
   q->f = open_memstream (&q->string, &q->len);
}

void
sql_free_s (sql_s_t * q)
{                               // Free string
   sql_close_s (q);
   free (q->string);
   q->string = NULL;
   q->len = 0;
}

char
sql_back_s (sql_s_t * q)
{                               // Remove last character and return it, don't close
   if (!sql_len_s (q))
      return 0;
   char r = q->string[q->len - 1];
   if (q->f)
      fseek (q->f, -1, SEEK_CUR);       // Move back but don't close
   else
      q->string[--q->len] = 0;  // Already allocated and closed, so just take off end string
   return r;
}

void
sql_seek_s (sql_s_t * q, size_t pos)
{                               // Seek to a position
   if (!sql_len_s (q))
      return;
   if (q->f)
      fseek (q->f, pos, SEEK_SET);
   else if (q->len > pos)
      q->string[q->len = pos] = 0;
}

// Freeing versions for use with malloc'd queries (e.g. from sql_printf...
void
sql_safe_query_s (SQL * sql, sql_s_t * q)
{
   sql_safe_query (sql, sql_close_s (q));
   sql_free_s (q);
}

SQL_RES *
sql_safe_query_use_s (SQL * sql, sql_s_t * q)
{
   SQL_RES *r = sql_safe_query_use (sql, sql_close_s (q));
   sql_free_s (q);
   return r;
}

SQL_RES *
sql_safe_query_store_s (SQL * sql, sql_s_t * q)
{
   SQL_RES *r = sql_safe_query_store (sql, sql_close_s (q));
   sql_free_s (q);
   return r;
}

SQL_RES *
sql_query_use_s (SQL * sql, sql_s_t * q)
{
   SQL_RES *r = sql_query_use (sql, sql_close_s (q));
   sql_free_s (q);
   return r;
}

SQL_RES *
sql_query_store_s (SQL * sql, sql_s_t * q)
{
   SQL_RES *r = sql_query_store (sql, sql_close_s (q));
   sql_free_s (q);
   return r;
}

int
sql_query_s (SQL * sql, sql_s_t * q)
{
   int r = sql_query (sql, sql_close_s (q));
   sql_free_s (q);
   return r;
}

void
sql_safe_query_free (SQL * sql, char *q)
{
   if (!q)
      return;
   sql_safe_query (sql, q);
   free (q);
}

SQL_RES *
sql_safe_query_use_free (SQL * sql, char *q)
{
   if (!q)
      return 0;
   SQL_RES *r = sql_safe_query_use (sql, q);
   free (q);
   return r;
}

SQL_RES *
sql_safe_query_store_free (SQL * sql, char *q)
{
   if (!q)
      return 0;
   SQL_RES *r = sql_safe_query_store (sql, q);
   free (q);
   return r;
}

SQL_RES *
sql_query_use_free (SQL * sql, char *q)
{
   if (!q)
      return 0;
   SQL_RES *r = sql_query_use (sql, q);
   free (q);
   return r;
}

SQL_RES *
sql_query_store_free (SQL * sql, char *q)
{
   if (!q)
      return 0;
   SQL_RES *r = sql_query_store (sql, q);
   free (q);
   return r;
}

int
sql_query_free (SQL * sql, char *q)
{
   if (!q)
      return 0;
   int r = sql_query (sql, q);
   free (q);
   return r;
}

char *
sql_safe_query_value (SQL * sql, char *q)
{                               // does query, returns strdup of first column of first row of result, or NULL
   if (!q)
      return NULL;
   char *val = NULL;
   SQL_RES *r = sql_safe_query_store (sql, q);
   SQL_ROW row = sql_fetch_row (r);
   if (row && row[0])
      val = strdup (row[0]);
   sql_free_result (r);
   return val;
}

char *
sql_safe_query_value_free (SQL * sql, char *q)
{                               // does query, returns strdup of first column of first row of result, or NULL
   if (!q)
      return NULL;
   char *val = sql_safe_query_value (sql, q);
   free (q);
   return val;
}

// SQL formatting print:-
// Formatting is printf style and try to support most printf functions
// Special prefixes
// #    Alternative form, used on 's' and 'c' causes sql quoting, so use %#s for quoted sql strings, %#S for unquoted escaped
// does not support *m$ format width of precision
// Extra format controls
// T    Time, takes time_t argument and formats as sql datetime
// B    Bool, makes 'true' or 'false' based in int argument. With # makes quoted "Y" or "N"

void
sql_vsprintf (sql_s_t * s, const char *f, va_list ap)
{                               // Formatted print, append to query string
   if (!s)
      return;
   if (s->dummy)
      warnx ("Dummy=%llX at %s", s->dummy, f);
   sql_open_s (s);
   char backtick = 0;           // Tracked for `%#S` usage, and only within the specific print
   while (*f)
   {
      // check enough space for anything but a string expansion...
      if (*f != '%')
      {
         if (*f == '`')
            backtick = !backtick;
         fputc (*f++, s->f);
         continue;
      }
      // formatting  
      const char *base = f++;
#if 0
      char flagaltint = 0;
      char flagcomma = 0;
      char flagplus = 0;
      char flagspace = 0;
      char flagzero = 0;
#endif
#ifdef	STRINGDECIMAL
      const char *flagformat = NULL;
#endif
      char flagfree = 0;
      char flagalt = 0;
      char flagleft = 0;
      char flaglong = 0;
      char flaglonglong = 0;
      char flaglongdouble = 0;
      int width = 0;
      int precision = -2;       // indicate not set
      // format modifiers
      while (*f)
      {
#if 0
         if (*f == 'I')
            flagaltint = 1;
         else if (*f == '\'')
            flagcomma = 1;
         else if (*f == '+')
            flagplus = 1;
         else if (*f == ' ')
            flagspace = 1;
         else if (*f == '0')
            flagzero = 1;
         else
#endif
         if (*f == '!')
            flagfree = 1;
         else if (*f == '#')
            flagalt = 1;
         else if (*f == '-')
            flagleft = 1;
#ifdef	STRINGDECIMAL
         else if (*f == '[')
         {
            f++;
            flagformat = f;
            while (*f && *f != ']')
               f++;
         }
#endif
         else
            break;
         f++;
      }
      // width
      if (*f == '*')
      {
         width = -1;
         f++;
      } else
         while (isdigit (*f))
            width = width * 10 + (*f++) - '0';
      if (*f == '.')
      {                         // precision
         f++;
         if (*f == '*')
         {
            precision = -1;     // get later
            f++;
         } else
         {
            precision = 0;
            while (isdigit (*f))
               precision = precision * 10 + (*f++) - '0';
         }
      }
      // length modifier
      if (*f == 'h' && f[1] == 'h')
         f += 2;
      else if (*f == 'l' && f[1] == 'l')
      {
         flaglonglong = 1;
         f += 2;
      } else if (*f == 'l')
      {
         flaglong = 1;
         f++;
      } else if (*f == 'L')
      {
         flaglongdouble = 1;
         f++;
      } else if (strchr ("hqjzt", *f))
         f++;

      if (*f == '%')
      {                         // literal!
         fputc (*f++, s->f);
         continue;
      }

      if (!strchr ("diouxXeEfFgGaAcsCSpnmTUBZD", *f) || f - base > 20)
      {                         // cannot handle, output as is
         while (base < f)
            fputc (*base++, s->f);
         continue;
      }
      void add (char a)
      {                         // add an escaped character
         if (flagalt && a == '`' && backtick)
         {
            fputc ('`', s->f);
            fputc ('`', s->f);
         } else if (flagalt && a == '\n')
         {
            fputc ('\\', s->f);
            fputc ('n', s->f);
         } else if (flagalt && a == '\b')
         {
            fputc ('\\', s->f);
            fputc ('b', s->f);
         } else if (flagalt && a == '\r')
         {
            fputc ('\\', s->f);
            fputc ('r', s->f);
         } else if (flagalt && a == '\t')
         {
            fputc ('\\', s->f);
            fputc ('t', s->f);
         } else if (flagalt && a == 26)
         {
            fputc ('\\', s->f);
            fputc ('Z', s->f);
         } else if (a)
         {
            if (flagalt && strchr ("\\\"'", a))
               fputc ('\\', s->f);
            fputc (a, s->f);
         }
      }
      char fmt[22];
      memmove (fmt, base, f - base + 1);
      fmt[f - base + 1] = 0;
      if (strchr ("scTUBSZD", *f))
      {                         // our formatting
         if (width < 0)
            width = va_arg (ap, int);
         if (precision == -1)
            precision = va_arg (ap, int);
         switch (*f)
         {
         case 'S':
         case 's':             // string
            {
               char *a = va_arg (ap, char *);
               if (!a)
               {
                  if (flagalt)
                     fprintf (s->f, "NULL");
                  break;
               }
               char *aa = a;    // for freeing later
               int l = 0,       // work out length and quoted length
                  q = 0;
               while ((precision < 0 || l < precision) && a[l])
               {
                  if (a[l] == '\'' || a[l] == '\\' || a[l] == '\n' || a[l] == '\r' || a[l] == '`' || a[l] == '"')
                     q++;
                  l++;          // find width
               }
               if (flagalt)
                  q = l + q;    // quoted length
               else
                  q = l;
               if (width && l < width)
                  q += width - l;
               if (flagalt && *f == 's')
                  fputc ('\'', s->f);
               if (width && !flagleft && l < width)
               {                // pre padding
                  while (l < width)
                  {
                     fputc (' ', s->f);
                     l++;
                  }
               }
               while (*a)
               {
                  add (*a++);
                  if (precision > 0 && !--precision)
                     break;     // length limited
               }
               if (width && flagleft && l < width)
               {                // post padding
                  while (l < width)
                  {
                     fputc (' ', s->f);
                     l++;
                  }
               }
               if (flagalt && *f == 's')
                  fputc ('\'', s->f);
               if (flagfree)
                  free (aa);
            }
            break;
         case 'c':             // char
            {
               long long a;
               if (flaglong)
                  a = va_arg (ap, long);
               else if (flaglonglong)
                  a = va_arg (ap, long long);
               else
                  a = va_arg (ap, int);
               if (flagalt)
                  fputc ('\'', s->f);
               add (a);
               if (flagalt)
                  fputc ('\'', s->f);
            }
            break;
         case 'Z':             // time (utc)
         case 'T':             // time (local)
         case 'U':             // time (utc)
            {
               time_t a = va_arg (ap, time_t);
               if (flagalt)
                  fputc ('\'', s->f);
               if (!a)
                  fprintf (s->f, "0000-00-00");
               else
               {
                  struct tm *t;
                  if (*f == 'T')
                     t = localtime (&a);
                  else
                     t = gmtime (&a);
                  char T[100];
                  int l = snprintf (T, sizeof (T), "%04u-%02u-%02u %02u:%02u:%02u", t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
                                    t->tm_hour, t->tm_min, t->tm_sec);
                  if (l > sizeof (T) - 1)
                     l = sizeof (T - 1);
                  if (width > 0 && l > width)
                     l = width;
                  T[l] = 0;
                  fprintf (s->f, "%s", T);
               }
               if (flagalt)
                  fputc ('\'', s->f);
            }
            break;
         case 'B':             // bool
            {
               long long a;
               if (flaglong)
                  a = va_arg (ap, long);
               else if (flaglonglong)
                  a = va_arg (ap, long long);
               else
                  a = va_arg (ap, int);
               if (flagalt)
                  fprintf (s->f, a ? "'Y'" : "'N'");
               else
                  fprintf (s->f, a ? "TRUE" : "FALSE");
            }
            break;
         case 'D':             // Stringdecimal
#ifdef	STRINGDECIMAL
            {
               sd_p a = va_arg (ap, sd_p);
               if (!a)
                  fprintf (s->f, "NULL");
               else
               {
                char *v = sd_output_f (a, round: flagformat ? *flagformat : 'B', places:(precision <
                   0 ? 0 : precision));
                  fprintf (s->f, v);
                  free (v);
                  if (flagfree)
                     sd_free (a);
               }
            }
#else
            warnx ("%%D used with no stringdecimal version - use sqllibsd.o and stringdecimal.o");
#endif
            break;
         }
      } else                    // use standard format (non portable code, assumes ap is moved on)
      {
#ifdef	NONPORTABLE
         vfprintf (s->f, fmt, ap);
#else
         va_list xp;
         va_copy (xp, ap);
         vfprintf (s->f, fmt, xp);
         va_end (xp);
         // move pointer forward
         if (width < 0)
            (void) va_arg (ap, int);
         if (precision == -1)
            (void) va_arg (ap, int);
         if (strchr ("diouxXc", *f))
         {                      // int
            if (flaglong)
               (void) va_arg (ap, long);
            else if (flaglonglong)
               (void) va_arg (ap, long long);
            else
               (void) va_arg (ap, int);
         } else if (strchr ("eEfFgGaA", *f))
         {
            if (flaglongdouble)
               (void) va_arg (ap, long double);
            else
               (void) va_arg (ap, double);
         } else if (strchr ("s", *f))
         {
            char *a = va_arg (ap, char *);
            if (a && flagfree)
               free (a);
         } else if (strchr ("p", *f))
            (void) va_arg (ap, void *);
#endif
      }
      f++;
   }
}

void
sql_sprintf (sql_s_t * s, const char *f, ...)
{                               // Formatted print, append to query string
   va_list ap;
   va_start (ap, f);
   sql_vsprintf (s, f, ap);
   va_end (ap);
}

char *
sql_printf (char *f, ...)
{                               // Formatted print, return malloc'd string
   sql_s_t s = { 0 };
   va_list ap;
   va_start (ap, f);
   sql_vsprintf (&s, f, ap);
   va_end (ap);
   return sql_close_s (&s);
}

int
sql_colnum (SQL_RES * res, const char *fieldname)
{                               // Return row number for field name, -1 for not available. Case insensitive
   int n;
   if (!res || !res->fields)
      return -1;
   for (n = 0; n < res->field_count && strcasecmp (res->fields[n].name ? : "", fieldname); n++);
   if (n < res->field_count)
      return n;
   return -1;
}

const char *
sql_colname (SQL_RES * res, int c)
{
   return res->fields[c].name;
}

char *
sql_col (SQL_RES * res, const char *fieldname)
{                               // Return current row value for field name, NULL for not available. Case insensitive
   if (!res || !res->current_row)
      return NULL;
   int n = sql_colnum (res, fieldname);
   if (n < 0)
      return NULL;
   return res->current_row[n];
}

SQL_FIELD *
sql_col_format (SQL_RES * res, const char *fieldname)
{                               // Return data type for column by name. Case insensitive
   if (!res || !res->current_row)
      return NULL;
   int n = sql_colnum (res, fieldname);
   if (n < 0)
      return NULL;
   return &res->fields[n];
}

#ifndef LIB
int
main (int argc, const char *argv[])
{
   char *x = malloc (10001);
   int i;
   for (i = 0; i < 10000; i++)
      x[i] = '\'';
   x[i] = 0;
   char *q = sql_printf ("Testing %d %d %d %B %#B %T %c %#c %c %#c %s %#s %#s %#s %#10s %#-10s %#10.4s %#s %#s",
                         1, 2, 3, 1, 1, time (0),
                         'a',
                         'a',
                         '\'', '\'', "test",
                         "test2", "te'st3", "te\\st4", "test5", "test6", "test7", (void *) 0, x);
   puts (q);
   return 0;
}
#endif

//--------------------------------------------------------------------------------

time_t
sql_time_z (const char *t, int utc)
{                               // time_t for sql date[time], -1 for error, 0 for 0000-00-00 00:00:00
   if (!t)
      return -1;
   unsigned int Y = 0,
      M = 0,
      D = 0,
      h = 0,
      m = 0,
      s = 0,
      c;
   c = 4;
   while (isdigit (*t) && c--)
      Y = Y * 10 + *t++ - '0';
   if (*t == '-')
      t++;
   c = 2;
   while (isdigit (*t) && c--)
      M = M * 10 + *t++ - '0';
   if (*t == '-')
      t++;
   c = 2;
   while (isdigit (*t) && c--)
      D = D * 10 + *t++ - '0';
   if (*t == ' ' || *t == 'T')
      t++;
   if (*t)
   {                            // time
      c = 2;
      while (isdigit (*t) && c--)
         h = h * 10 + *t++ - '0';
      if (*t == ':')
         t++;
      c = 2;
      while (isdigit (*t) && c--)
         m = m * 10 + *t++ - '0';
      if (*t == ':')
         t++;
      c = 2;
      while (isdigit (*t) && c--)
         s = s * 10 + *t++ - '0';
      if (*t == '.')
      {                         // fractions - skip over
         t++;
         while (isdigit (*t))
            t++;
      }
   }
   if (!Y && !M && !D && !h && !m && !s)
      return 0;                 // sql time 0000-00-00 00:00:00 is returned as 0 as special case
   if (!Y || !M || !D || M > 12 || D > 31)
      return -1;                // mktime does not treat these as invalid - we should for SQL times
 struct tm tm = { tm_year: Y - 1900, tm_mon: M - 1, tm_mday: D, tm_hour: h, tm_min: m, tm_sec:s };
   if (*t == 'Z' || utc)
      return timegm (&tm);      // UTC
   tm.tm_isdst = -1;            // Work it out
   return mktime (&tm);         // local time
}

void
sql_transaction (SQL * sql)
{
   sql_safe_query (sql, "START TRANSACTION");
}

int __attribute__((warn_unused_result)) sql_commit (SQL * sql)
{
   return sql_query (sql, "COMMIT");
}

void
sql_safe_commit (SQL * sql)
{
   return sql_safe_query (sql, "COMMIT");
}

void
sql_safe_rollback (SQL * sql)
{
   return sql_safe_query (sql, "ROLLBACK");
}

int
sql_field_len (MYSQL_FIELD * f)
{                               // Allow for common charset logic
   if (f->charsetnr == 45)
      return f->length / 4;
   if (f->charsetnr == 33)
      return f->length / 3;
   return f->length;
}
