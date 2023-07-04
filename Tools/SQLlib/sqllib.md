# SQL wrapper library

The `sqllib.o` and `sqllibsd.o` are libraries that provide a nicer and safer way to access underlying SQL libraries. They are currently defined for mysql, but do not use the mysql name. The idea is that they could, in future, provide wrappers for other back end SQL systems without major change to code using the library.

This manual does not go in to detail on the function prototypes themselves, see `sqllib.h` for details of these.

## mysql equivalent functions and datatypes

In general, anything that says `mysql` changes to just `sql`. This includes `MYSQL_RES*` now `SQL_RES*` and so on.
A number of functions are directly mapped by `#define` in the `sqllib.h` include file, e.g. `sql_affected_rows(...)` as `mysql_affected_rows(...)`.

## Connection and config

Ensuring the correct database connection details is a challenge. For this a function `sql_safe_connect(...)` is defined. If the connection fails an error is reported to stderr and the program aborts. This will make use of a config file if available, in `$MYSQL_HOME` or `$HOME` or `/etc` by the name of `.my.cnf`. This is the default for the `mysql` command as well, so allows credentials to be defaulted in a common way. 

In addition `sql_cnf_connect(...)` can be used, which just uses a specified configuration file for the connection. This is ideal for ensuring credentials are not included in code.

At the end, close with `sql_close(&sql)`.

## Queries

In addition to just mapping `mysql` to `sql` the library also provides a comprehensive set of functions for managing SQL queries. The reason for this is that SQL queries require memory management (they can be arbitrarily long) and escaping (to avoid Bobby Tables). Constructing safe queries directly in C code can be problematic and error prone.

### Error handling

Another key aspect of SQL is error handling. If a query fails. In a most cases a query would not fail, SQL is quite robust, an `UPDATE` may do nothing if no rows match a `WHERE` clause, a `SELECT` may return no rows. So for the vast majority of SQL coding you don't ever expect an error. However, good code needs to check the response from every single query to be sure.

To accommodate this all of the query wraps include a `_safe_` version of the query. In the even there is an error with the query this version reports the query and error to stderr, and aborts the code. Used in conjunction with SQL transactions this provides a safe abort for any unexpected error and provides the details needed to work out what happened.

### Debug

Another aspect of any SQL code is working out exactly what happened. A variable is defined `sqldebug` which is an `int` and so can be used with `POPT_ARG_NONE` in a `popt` argument list to allow debug to be enabled. The effect is that every query is logged to stderr, along with execution time and number of rows returned.

### Formatting queries

A query can be constructed step by step, or can be constructed in the query function directly allowing a simple one line SQL query to be constructed and executed. In either case the use of a `printf` style formatting string us used to construct the query. All of the normal `printf` `%` coding work as normal, however there are a number of specific codes provided as well.

| % code | meaning |
| ------ | ------- |
| `%#s` | This outputs a string (`char*`) with quotes and escaping needed for an SQL string, e.g. `"O'Neil"` comes out as `'O\'Neil'`. If string is NULL, outputs NULL. |
| `%#S` | This does the same but without the quotes, so you can say things like `'Name is %#S'` and have it come out as `'Name is O\'Neil'`. |
| `%#c` | Outputs a `char` in quotes with necessary escaping. |
| `%#T` | Outputs a `time_t` in SQL format, quoted. |
| `%T` | Outputs a `time_t` in SQL format, unquoted. |
| `%B` | Outputs `TRUE` or `FALSE` based on `int` argument being non zero or zero. |
| `%#B` | Outputs `'Y'` or `'N'` based on `int` argument being non zero or zero. |
| `%D` | Outputs a string decimal library `sd_p` value, or NULL if no pointer passed. Can have precision, e.g. `%.2D` and also, in square brackets, rounding code, e.g. `%.2[B]D` for bankers rounding to 2 places. Bankers rounding is default. This needs the `sqllibsd.o` library, else it aborts with an error. |

An additional modifier `!` can be used with `%s` and `%D` which means free the value after use, e.g. `%!s`, `%!#s`, `%!#S` will take a `char*` string and free it (if not NULL) after use. `%!D` takes a string decimal library `sd_p` and does `sd_free(...)` on it after use. This allows for dynamically created allocated values to be used which are freed without the need to put them in variables and call a free function afterwards.

### Field names

Mysql allows field names to be placed in back ticks, and this is recommended. If you have a field name from a string you could just use `%s`, or better `` `%s` ``, but it is recommended to use  `` `%#S` `` in such cases. This allows the library to handle any escaping needed for field names in back ticks to be escaped.

### Step by step query construction

To construct a query step by step, you create a query string object, e.g. `sql_s_t s={0};`. **The `{0}` is important, do not omit the 0**

You can then print to it with `sql_sprintf(&s,...)` e.g. `` sql_sprintf(&s,"INSERT INTO `sometable` SET `ID`=%d,`name`=%#s",id,name); ``

It is quite common to want to add multiple parts like `` `field`='value', `` which have a trailing comma, and then you need to remove the final comma. For this you can use `sql_back_s(&s)` which removes and returns the last character. Test for a comma, and if present you know you added some fields. You can then add more, like `` WHERE `ID`=%d `` or some such and do the query.

You can test how much you have written with `sql_len_s(&s)`.

Normally you then execute a query using the string, this resets it for future use. The query functions that take an `sql_s_t` pointer all end in `_s`, e.g. `sql_safe_query_store_s(...)`. if you need to discard the switch, use `sql_free_s(&s);`, and if you need to get the string for other user, use `sql_close_s(&s)` which returns a malloc'd string. You still need to free this at some point, either using `sql_free_s(&s);` (preferred) or `free(...)` on the pointer returned (but don't then use `&s` later).

### Inline queries

In a lot of cases you can construct a whole query in one go, and for this the various query functions have a `_f` version. These take a format string and variables to format and execute a query. e.g. `` sql_safe_query_f(&sql,"INSERT INTO `mytable` SET `field1`=%d,`field2`=%#s",f1,f2); ``

It is also possible to make a query string and have it as a mallo'c `char*` pointer using `sql_printf(...)`. There are also `_free` versions of the query functions which take a `char*` malloc'd pointer and use it and free after use. So the above is the same as `` sql_safe_query_free(&sql,sql_printf("INSERT INTO `mytable` SET `field1`=%d,`field2`=%#s",f1,f2)); ``. This can be useful if you need the query string for anything else.

### SQL query functions

The query functions are available in combinations with `_safe_` or not, and `_f` or `_s` or `_free`, etc.

The format is:-
- `sql_`
- Optional `safe_` meaning to abort on error
- `query`
- Optional `_store` or `_use` for queries that return rows
- Optional suffix `_f` (for format inline), `_free` for use and free malloc'd query, `_s` for use `sql_s_t*` composed string, otherwise a query string is passed and not freed.

The simplest, `sql_query(&sql,query)`, does the query, does not free the query, does not expect a result, and returns an `int` with 0 if it worked else and error. In practice this is rarely what you want.

A more complex one like `sql_safe_query_store_f(&sql,"whatever",a,b,c)` will format a query with variables, use that to do the query, free the formatted query, store the result of the query, abort on error or return the result row set.

A query like `SELECT` or `DESCRIBE` returns a value, and so you have to use a `_store` or `_use` query functions, which return `SQL_RES*`. The `_use` version streams the result from the SQL server, and is not usually what you want. Only use `_use` if you have some stupidly big query result that won't fit in memory. The `_store` versions stores the result - this has the advantage you can see how many rows there are `sql_num_rows(res)` and it completes the `SQL` query connection to allow other queries even when working through the rows of the result. You free the result with `sql_free_result(res)`. The `_safe` version always returns an `SQL_RES*` or errors, the non `_safe` returns NULL if error.

A query like `UPDATE` or `INSERT` does not return a result, though `sql_insert_id(&sql)` is useful after inserts with auto incrementing IDs. These functions do not have `_use` or `_store` and have no return value (so `void` for `_safe`) or an `int` return if not `_safe` version so you know if an error (non zero return).

### Accessing fields

Another big problem with the underlying C mysql libraries is accessing the result of a query. The result is an `SQL_ROW` which is an array of strings (which can be NULL) for each row of the result. Use `sql_fetch_row(res)` to get each row, and reference the first row as `row[0]` for example.

The problem is that this means a `SELECT` with a list of field names and then using `row[0]`, `row[1]`, etc, exacting matching the field in the `SELECT` and that is not easy to read and very prone to errors.

To address this there are functions `sql_col(res,name)` which returns the row value for a named row, e.g. `sql_col(res,"ID")`. A variation `sql_colz(res,name)` can be used to return an empty string rather than a NULL.

This means you do not need the `SQL_ROW` as such, you can simply `while(sql_fetch_row(res))` and access fields by name using `sql_col(res,name)`. This also makes it idea for cases where fields name be option - you can do a `SELECT * FROM...` and then use fields that exist in the result. The `sql_colnum(res,name)` function gets a column number, with -1 for not existing, allowing an easy test for whether a name column even exists in the row. `sql_col_format(res,name)` is useful to get the `SQL_FIELD` field type for a column, but name.

## Transactions

The transaction functions are only slightly modified. `sql_transaction(&sql)` to start, but `sql_safe_commit(&sql)` to commit, and `sql_safe_rollback(&sql)` to roll back. These abort on error with an error to stderr.

Setting up a transaction for your code is recommended, and ideal when using the `_safe` functions that abort on error.

Also, when not in transactions, and single SQL query can fail with a *failed to get lock* error. In such cases the query is automatically retried once before failing.

One trick for using debug is to end with something like 

```
 if(sqldebug)sql_safe_rollback(&sql);
 else sql_safe_commit(&sql);
```

This means you can use the debug mode to print all the queries but not actually do any of the actual SQL at the end, rolling back.

# Examples

## Scanning a table

Doing a query, getting (stored) results, and doing some updates while scanning the result.

```
SQL_RES *att = sql_safe_query_store_f (sqlp, "SELECT * FROM `CalendarAttendee` WHERE `ServerId`=%lld", serverid);
while (sql_fetch_row (att))
{
   if(sometestmeaningchangefield())
     sql_safe_query_f(&sql,"UPDATE `CalendarAttendee` SET `MeetingStatus`=`MeetingStatus`|4 WHERE `ID`=%#s",sql_colz (att, "ID")));
}
sql_free_result (att);
```

## Making an update

Constructing an UPDATE with optional fields, and doing it if any fields actually added.

```
sql_s_t q={0};
sql_sprintf(&q,"UPDATE `mytable` SET ");
if(s1) sql_sprintf(&q,"`v1`=%#s,",s1);
if(d2>=0) sql_sprintf(&q,"`d2`=%d,",d2);
if(sql_back_s(&q) == ',')
{
 sql_sprintf(&q, " WHERE `ID`=%d",id);
 sql_safe_query_s(&sql,&q);
}
else sql_free_s(&q); // No updates
```
