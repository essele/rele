# RELE
This is RELE (pronounced ReeLee), a regular expression library for
embedded environments.

This work is part of a Sunderland University MSc Computer Science project, however
this is a bit of development I've been keen to do for some time!

This library is written in C and is intended for use in embedded systems with limited
resources.

## Features
- Single source file, single header - Just statically link with your own code
- MIT license - Permits use in open and closed source projects
- Written in C - RELE should run on more or less any platform
- No external dependencies
- Non-recursive design, stack usage is tightly controlled
- Dynamic memory is used, but is considered scarce
- Supports captures and backreferences

## Syntax
RELE currently supports the following regular expression syntax:
```
.         Matches any character
\n        Matches new line/line feed character (ASCII code 0x0A)
\r        Matches carriage return character (ASCII code 0x0D)
\t        Matches horizontal tab character (ASCII code 0x09)
\xXX      Matches ASCII code 0xXX
\d        Matches any digit character (0-9, ASCII codes 0x30-0x39)
\D        Matches any non-digit character
\s        Matches any whitespace character (as defined by isspace())
\S        Matches any non-whitespace character
\w        Matches any word character (0-9, A-Z, a-z or _)
\W        Matches any non-word character

\? \*     Matches meta character
\+ etc.

?         Matches zero or one time
*         Matches zero or more times
+         Matches one or more times
          (additional ? means lazy mode)
a|b       Matches a or b
[ace]     Matches characters in given set (']' must be first character if in set)
[A-Za-z]  Matches characters in given ranges (character sets and ranges can be mixed)
[^ace]    Matches characters not in given set
[^A-Za-z] Matches characters not in given ranges
()        Capturing group
(?:)      Non-capturing group
\1        Backreference to a group (can be \1 \g1 \{1} \g{1})

^         Matches the beginning of the text (or line if using multiline)
$         Matches the end of the text (or line if using multiline)
\A        Matches the beginning of the text
\Z        Matches the end of the text
\b        Matches a word boundary
```
A full explaination of regular expression syntax and operation is outside the scope of this document. See https://en.wikipedia.org/wiki/Regular_expression for further info.

## Usage

RELE consists of only a single source file and a single header file. To use RELE in your own project,
just link `rele.c` with the rest of your source code and ensure `rele.h` is in your include path.

RELE exposes a small number of public functions:

TODO

```C
int subreg_match(const char* regex, const char* input, subreg_capture_t captures[],
    unsigned int max_captures, unsigned int max_depth);
```
This function takes the following arguments:

|Argument  |Description|
|----------|-----------|
|`regex`|Null-terminated string containing regular expression.|
|`input`|Null-terminated string to match against regex.|
|`captures`|Pointer to array of captures to populate.|
|`max_captures`|Maximum permitted number of captures (should be equal to or less than the number of elements in the array pointed to by captures).|
|`max_depth`|Maximum depth of nested groups to allow in regex. This value is used to limit SubReg's system stack usage. A value of 4 is probably enough to cover most use cases. Must not exceed INT_MAX as defined in `limits.h`.|

If `input` matches against `regex` then the number of captures made will be returned (the first capture spanning the entire input). If there is no match, the function will return zero otherwise one of the following return codes will be returned:

|#define|Value|Description|
|-------|-----|-----------|
|`SUBREG_RESULT_NO_MATCH`|0|No match occurred|
|`SUBREG_RESULT_INVALID_ARGUMENT`|-1|Invalid argument passed to function.|
|`SUBREG_RESULT_ILLEGAL_EXPRESSION`|-2|Syntax error found in regular expression. This is a general syntax error response - If SubReg can provide a more descriptive syntax error code (as defined below), then it will.|
|`SUBREG_RESULT_MISSING_BRACKET`|-3|A closing group bracket is missing from the regular expression.|
|`SUBREG_RESULT_SURPLUS_BRACKET`|-4|A closing group bracket without a matching opening group bracket has been found.|
|`SUBREG_RESULT_INVALID_METACHARACTER`|-5|The regular expression contains an invalid metacharacter (typically a malformed \ escape sequence)|
|`SUBREG_RESULT_MAX_DEPTH_EXCEEDED`|-6|The nesting depth of groups contained within the regular expression exceeds the limit specified by `max_depth`.|
|`SUBREG_RESULT_CAPTURE_OVERFLOW`|-7|Capture array not large enough.|
|`SUBREG_RESULT_INVALID_OPTION`|-8|Invalid inline option specified.|

If a match occurs and `max_captures` = 0, this function still returns 1 but won't store the capture. This function may modify the captures array, even if an error occurs.

Captures are represented using the struct type `subreg_capture_t`. The struct contains the following fields:

|Field|Description|
|-----|-----------|
|`start`|Pointer to beginning of capture in input string provided to `subreg_match`.|
|`length`|Number of characters in capture.|

## Testing

TODO

A basic test suite for SubReg is provided in the `tests` directory of SubReg's Git repository. [CMake](https://cmake.org/) is required to build the tests:
```bash
# from the root of your cloned SubReg repository:
cd tests
cmake .
make
./subreg-tests
```

## Bug Reports

Please send bug reports/comments/suggestions regarding RELE via GitHub issue reporting.

## License

```
Copyright (c) 2025 Lee Essen

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISIN
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
```
