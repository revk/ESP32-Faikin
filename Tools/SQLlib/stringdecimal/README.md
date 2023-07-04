# stringdecimal

This library provides simple arbitrary precision decimal maths.

There are two levels, the stringdecimal\_\*() functions work on strings, and return a malloc'd string, and the sd\_\*() functions work on an internal sd\_p type.

There are options to control output formatting and rounding.

See stringdecimal.h for the various calls available.

This includes functions for add, subtract, multiply, compare, divide and rounding.

A general purpose "eval" function in the stringdecimaleval.o variant parses sums using +, -, ÷, ×, ^, |x|, (, and ) to produce an answer. ASCII/alternative versions /, \*, also work. Internally this uses rational numbers if you have any division other than by a power of 10, so only does the one division at the end to specified limit of decimal places. Hence 1000/7\*7 is 1, not 994 or some other "nearly 1" answer. It also understands operator precedence, so 1+2\*3 is 7, not 9.

A lot of unicode is handled, e.g. you can work out 2¹²⁸ if you want.

Additional comparison operators return 1 for true and 0 for false: <, >, ≤, ≥, =, ≠. ASCII/alternative versions >=, <=, !=, == also work.

Additional logic operators return 1 for true and 0 for false, and assume non zero is true: ∧, ∨, and unary ¬. ASCII/alternative versions &&, ||, ! also work.

The code actually includes a general expression parse which allows extensions on the stringdecimal logic if needed.

And yes, it will happily (if you have the memory) work out 1e1000000000+1 if you ask it to.

Note that these library calls have C++ style, even though they are C code. This allows additional options to be added later with no change to calling code.

e.g. sd\_output(v) outputs v with defaults, but sd\_output(v,round:'F') does floor rounding, etc

See https://www.revk.uk/2020/08/pseudo-c-using-cpp.html
