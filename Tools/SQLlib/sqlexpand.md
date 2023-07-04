# SQL expansion command

The command `sqlexpand` can be used to expand environment variables within an SQL query in a safe way (i.e. handling embedded quotes).

The library `sqlexpand.o` can be used in other tools to achieve the same objective.

Note that some options are enabled / disabled by flags on the `sqlexpand` command, see `--help` for details.

## Simple expansion

The simplest expansion is just `$` followed by a variable name (starts as a letter, or underscore, then letters, underscore or number).

However, if the next thing is a letter or number you can include the name in curly braces, e.g `${variable}`.

## Prefix

Immediately before the variable name (and inside `{...}` if present) can be a number of prefixes.

|Prefix|Meaning|
|------|-------|
|`?`|	Used in some special cases, such as testing if a variable exists.|
|`@`|	Consider the expanded value as a filename and read the value from that file.|
|`-`|	Replace all `'`, `"` or `` ` `` with `_` (underscore).|
|`,`|	Ensures quotes around the expansion. Changes tab or comma to quote,comma,quote to make a clean list.|
|`+`|	URL encode the value. e.g. `?name=$+value`. Use `++` to double escape. Note encodes space a `%20` not `+` so can be used in path. Does not escape `/`.|
|`#`|	Made MD5 of value, use `##` for SHA1. Value is hex (lower case).|
|`=`|	Base64 encode the value. If used with `#` or `##` then this is base64 of the binary hash instead of hex.|
|`%`|	Trust this variable (use with care) so allowing expansion outside quotes as is with no escaping.||

Only use `$%variable` with great care, and ensure any construction of the variable value uses sqlexpand to safely escape what is being set.

## Special variable

Instead of the variable name itself there are some special variables. Some of these match a prefix, and so these work where not followed by a letter or underscore (use `{...}` if needed, e.g. `xx${@}xx`).

|Variable|Meaning|
|--------|-------|
|`$variable`|	If the variable name itself starts with a $ then the variable contains a variable name. E.g. If variable `X` contains `A` then `${$X}` is the same as `$A`.|
|`$`|	Generate the parent PID (deprecated)|
|`@`|	Generate a timestamp of current directory|
|`<`|	Read stdin|
|`\`|	Generate a back tick (so starts/ends back tick quoting if used outside other quotes).|
|`/`|	Generate a single quote (so starts/ends single quoting if used outside other quotes).|

## Index

Immediately after the variable name (and inside `{...}` if present) can be an index of the form `[number]`. If present then the number is used as a n index in to the value. The value is split on tab characters and index `[1]` if the first part. This is done before considering any suffix.

## Suffix

Immediately after the variable name (and inside `{...}`, and after `[...]`, if present) can be a number of suffixes.

|Suffix|Meaning|
|------|-------|
|`:h`|	Head of path, removes all after last slash.|
|`:t`|	Tail of path, only from after last slash (unchanged if no slash).|
|`:e`|	Extension. Everything after final dot if after last slash. Blank if no dot after last slash.|
|`:r`|	Remove extension. Removes last dot and anything after if it, if dot is after a slash.|

## Checking

The final SQL has to then pass some tests to help catch any remaining possible SQL injection (e.g. from `$%variable`).

- Must have correctly balanced quotes (`'`, `"` and `` ` ``).
- Must not contain an unquoted `;`, i.e. an attempt to make multiple commands.
- Must not contain an unquoted `#`, `/*` or `--` , i.e. an attempt to include a comment.

