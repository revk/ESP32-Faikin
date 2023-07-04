// SQL client library
// This software is provided under the terms of the GPL v2 or later.
// This software is provided free of charge with a full "Money back" guarantee.
// Use entirely at your own risk. We accept no liability. If you don't like that - don't use it.

#include <stdarg.h>

#include <mysql.h>
#include <mysqld_error.h>

// Mappings to MYSQL
#define	SQL			MYSQL
#define	SQL_ROW			MYSQL_ROW
#define	SQL_RES			MYSQL_RES
#define	SQL_FIELD		MYSQL_FIELD

#define	sql_init		mysql_init
#define	sql_real_query		mysql_query
#define	sql_free_result		mysql_free_result
#define	sql_close		mysql_close
#define	sql_num_fields		mysql_num_fields
#define	sql_fetch_field		mysql_fetch_field
#define	sql_fetch_fields	mysql_fetch_fields
#define	sql_fetch_row		mysql_fetch_row
#define	sql_list_fields		mysql_list_fields
#define	sql_use_result		mysql_use_result
#define	sql_store_result	mysql_store_result
#define	sql_error		mysql_error
#define	sql_insert_id		mysql_insert_id
#define	sql_affected_rows	mysql_affected_rows
#define	sql_num_rows		mysql_num_rows
#define	sql_options		mysql_options
#define	sql_errno		mysql_errno
#define	sql_data_seek		mysql_data_seek
#define	sql_select_db		mysql_select_db
#define	sql_stat		mysql_stat
#define	sql_ping		mysql_ping
#define	sql_set_character_set	mysql_set_character_set

#define	SQL_OPT_RECONNECT	MYSQL_OPT_RECONNECT

// Types
typedef struct {
   size_t len; // Len of string
   char *string; // Malloc'd space
   FILE *f;	// open_memstream for query/len
   long long dummy;	// To catch uninitialised structure
} sql_s_t;

// Data
extern int sqldebug;            // Set +ve to print, -ve to not do updates but just print
extern int sqlsyslogquery;
extern int sqlsyslogerror;

// Functions

SQL *sql_real_connect(SQL * mysql, const char *host, const char *user, const char *passwd, const char *db, unsigned int port, const char *unix_socket, unsigned long client_flag, char safe, const char *mycnf);
#define sql_connect(mysql,host,user,passwd,db,port,unix_socket,client_flag) sql_real_connect(mysql,host,user,passwd,db,port,unix_socket,client_flag,0,NULL)
#define sql_safe_connect(mysql,host,user,passwd,db,port,unix_socket,client_flag) sql_real_connect(mysql,host,user,passwd,db,port,unix_socket,client_flag,1,NULL)
#define sql_cnf_connect(mysql,mycnf) sql_real_connect(mysql,NULL,NULL,NULL,NULL,0,NULL,0,1,mycnf)

int sql_safe_select_db(SQL * sql, const char *db);      // Select database

void sql_safe_query(SQL * sql, char *q);        // does query and aborts if error (auto retry once for deadlock)
SQL_RES *sql_safe_query_use(SQL * sql, char *q);        // does query and fetch result and aborts if error or no result
SQL_RES *sql_safe_query_store(SQL * sql, char *q);      // does query and fetch result and aborts if error or no result
SQL_RES *sql_query_use(SQL * sql, char *q);     // does query and fetch result and returns 0 if no result
SQL_RES *sql_query_store(SQL * sql, char *q);   // does query and fetch (store) result and returns 0 if no result
int __attribute__((warn_unused_result)) sql_query(SQL * sql, char *q);  // sql query

char *sql_printf(char *, ...);  // Formatted print, return malloc'd string - usually used in the following
void sql_safe_query_free(SQL * sql, char *);    // does query and aborts if error, frees q
#define sql_safe_query_f(sqlp,...) sql_safe_query_free(sqlp,sql_printf(__VA_ARGS__))
SQL_RES *sql_safe_query_use_free(SQL * sql, char *);    // does query and fetch result and aborts if error or no result, frees q
#define	sql_safe_query_use_f(sqlp,...) sql_safe_query_use_free(sqlp,sql_printf(__VA_ARGS__))
SQL_RES *sql_safe_query_store_free(SQL * sql, char *);  // does query and fetch (store) result and aborts if error or no result, frees q
#define sql_safe_query_store_f(sqlp,...) sql_safe_query_store_free(sqlp,sql_printf(__VA_ARGS__))
SQL_RES *sql_query_use_free(SQL * sql, char *); // does query and fetch result and returns 0 if no result, frees q
#define sql_query_use_f(sqlp,...) sql_query_use_free(sqlp,sql_prinf(__VA_ARGS__))
SQL_RES *sql_query_store_free(SQL * sql, char *);       // does query and fetch (store) result and returns 0 if no result, frees q
#define sql_query_store_f(sqlp,...) sql_query_store_free(sqlp,sql_printf(__VA_ARGS__))
int __attribute__((warn_unused_result)) sql_query_free(SQL * sql, char *);      // sql query, frees q
#define sql_query_f(sqlp,...) sql_query_free(sqlp,sql_printf(__VA_ARGS__))

char *sql_safe_query_value(SQL *, char *);      // does query, returns strdup of first column of first row of result, or NULL
char *sql_safe_query_value_free(SQL *, char *); // does query, returns strdup of first column of first row of result, or NULL

void sql_vsprintf(sql_s_t *, const char *, va_list);       // Formatted print, append to query string
void sql_sprintf(sql_s_t *, const char *, ...);    // Formatted print, append to query string
void sql_safe_query_s(SQL * sql, sql_s_t *);       // does query and aborts if error, frees and clears query string
void sql_open_s(sql_s_t*);	// Open or continue an sql_s_t
char *sql_close_s(sql_s_t*);	// Close an sql_s_t, return the string
size_t sql_len_s(sql_s_t*);	// Return current length of string - does not close it
SQL_RES *sql_safe_query_use_s(SQL * sql, sql_s_t *);       // does query and fetch result and aborts if error or no result, frees and clears query string
SQL_RES *sql_safe_query_store_s(SQL * sql, sql_s_t *);     // does query and fetch result and aborts if error or no result, frees and clears query string
SQL_RES *sql_query_use_s(SQL * sql, sql_s_t *);    // does query and fetch result and returns 0 if no result, frees and clears query string
SQL_RES *sql_query_store_s(SQL * sql, sql_s_t *);  // does query and fetch result and returns 0 if no result, frees and clears query string
int __attribute__((warn_unused_result)) sql_query_s(SQL * sql, sql_s_t *); // sql query, frees and clears query string
void sql_free_s(sql_s_t *);        // free a query that has been created
char sql_back_s(sql_s_t *);        // remove last character and return it, don't close
void sql_seek_s(sql_s_t *,size_t);        // seek output to a position

int sql_colnum(SQL_RES *, const char *fieldname);       // Return row number for field name, -1 for not available. Case insensitive
const char *sql_colname(SQL_RES * res, int c);  // Name of column by number
char *sql_col(SQL_RES *, const char *fieldname);        // Return current row value for field name, NULL for not available. Case insensitive
SQL_FIELD *sql_col_format(SQL_RES *, const char *fieldname);    // Return data type for column by name. Case insensitive
#define	sql_colz(r,f)	(sql_col(r,f)?:"")      // Non null return sql_col

time_t sql_time_z(const char *datetime, int utc);       // return time_t for SQL time
#define sql_time(d) sql_time_z(d,0)
#define sql_time_utc(d) sql_time_z(d,1)

void sql_transaction(SQL * sql);        // Begin a transaction
int __attribute__((warn_unused_result)) sql_commit(SQL * sql);  // Commit a transaction
void sql_safe_commit(SQL * sql);        // Commit a transaction (fatal if error)
void sql_safe_rollback(SQL * sql);      // Rollback a transaction (fatal if error)

int sql_field_len(MYSQL_FIELD *);
