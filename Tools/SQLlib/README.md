# SQL wrapper library

This library provides functions to make SQL access from C safer and simpler.

See https://github.com/revk/SQLlib/blob/master/sqllib.md for details of the library.

Functions are provided for safe construction of SQL queries.

## Examples

### Scanning a table

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

### Making an update

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

# SQL shell variable expansion

This library and command allows for SQL query expansion from environment variables.

See https://github.com/revk/SQLlib/blob/master/sqlexpand.md for details of the command and library.

