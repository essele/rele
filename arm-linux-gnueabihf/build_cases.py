#!/usr/bin/python3

import os
import pathlib
import argparse
import sys
import re
import random
import string
from contextlib import redirect_stdout


#
# We'll have a random cache so that each gen of a specific size gets
# reused, this will help with string dup lookups later.
#
RANDOM_CACHE = {}


#
# Build a list of case information from the text file and return
# an array containing all cases.
#
def build_cases(filename):
    cases = []
    case = {}
    joiner = "\n"
    group = pathlib.Path(filename).stem

    with open(filename) as f:
        for line in f:
            line = line.rstrip()
            if re.match('^\\s*#', line):
                continue

            # Name...
            if line[:2] == "N:":
                case["name"] = line[2:]
                continue

            # Regex
            if line[:1] == "/":
                case["regex"] = line[1:]
                continue

            # Description
            if line[:2] == "D:":
                case["desc"] = line[2:]
                continue

            # Expected failure scenario
            if line[:2] == "E:":
                case["error"] = line[2:]
                continue

            if line[:2] == "I:":
                case["iter"] = line[2:]
                continue

            if line[:3] == "CF:":
                if (not "cflags" in case):
                    case["cflags"] = []
                if (line == "CF:CASELESS"):
                    case["cflags"].append("F_ICASE")
                elif (line == "CF:NEWLINE"):
                    case["cflags"].append("F_NEWLINE")
                else:
                    print("Unknown flag: " + line[2:])
                    sys.exit(1)
                continue

            if line[:2] == "J:":
                if (line == "J:NONE"):
                    joiner = ""
                elif (line == "J:NL"):
                    joiner = "\n"
                elif (line == "J:CR"):
                    joiner = "\r"
                elif (line == "J:CRLF"):
                    joiner = "\r\n"
                else:
                    print("Unknown joiner type\n");
                    sys.exit(0)
                continue

            # Line of text
            if line[:2] == "T:":
                if "text" in case:
                    case["text"] += joiner + line[2:]
                else:
                    case["text"] = line[2:]
                continue

            # Generate some text
            if line[:4] == "GEN:":
                match = re.match('(.*),(\\d+)(K?)', line[4:])
                if (match):
                    if match.group(1) == "random":
                        count = int(match.group(2))
                        if (match.group(3)):
                            count *= 1024

                        if (count in RANDOM_CACHE):
                            x = RANDOM_CACHE[count]
                        else:
                            x = ''.join(random.choices(string.ascii_letters + string.digits + string.punctuation, k=count))
                            RANDOM_CACHE[count] = x
                        print(len(x))
                        if "text" in case:
                            case["text"] += joiner + x
                        else:
                            case["text"] = x
                        continue
                    else:
                        print("Unknown gen type: " + match.group[1])
                        sys.exit(0)
                else:
                    print("MALFORMED GEN LINE\n")
                    sys.exit()

            # Result
            match = re.match('^(\\d+):\\s*(\\-?\\d+),\\s*(\\-?\\d+)', line)
            if (match):
                (num, r1, r2) = match.groups()
                num = int(num)
                r1 = int(r1)
                r2 = int(r2)

                if not "res" in case:
                    case["res"] = []

                if (num != len(case["res"])):
                    print(">>> " + line)
                    print("RESULTS MUST BE IN ORDER STARTING AT 0")
                    sys.exit()

                case["res"].append([ r1, r2 ])
                continue

            # An empty line marks the end of a test case, so we can write out what we have
            # (if anything) at this point.
            if len(line) == 0:
                #
                # Make sure everything is there...
                #
                if not "name" in case:
                    case = {}
                    joiner = "\n"
                    continue

                if not "desc" in case:
                    case["desc"] = ""

                case["group"] = group

                cases.append(case)
                case = {}
                joiner = "\n"
                continue

            # Anything else is an error
            print(">>> " + line)
            print("SOMETHING UNKNOWN")
            sys.exit()

    return cases

def quote(s):
    s = s.replace('\\', '\\\\')   # backslash first
    s = s.replace('"', '\\"')     # escape quotes
    s = s.replace('\n', '\\n')    # newlines
    s = s.replace('\t', '\\t')    # tabs
    return s

def do_hex(s):
    if isinstance(s, str):
        s = s.encode('utf-8')
    hex_values = [f'0x{b:02X}' for b in s]
    hex_values.append("0x00")

    # Join values, 12 per line for readability
    lines = []
    line = []
    for i, val in enumerate(hex_values):
        line.append(val)
        if (i + 1) % 12 == 0:
            lines.append(', '.join(line))
            line = []
    if line:
        lines.append(', '.join(line))
    
    array_body = ',\n    '.join(lines)
    #return f'unsigned char {name}[] = {{\n    {array_body}\n}};'
    return array_body


# -------------------------------------------------------------------------------
# MAIN ENTRY POINT
#
# Process the command line, should be -o <outfile> infile infile infile
#
# -------------------------------------------------------------------------------


parser = argparse.ArgumentParser(description="Process text case files and produce output file.")

parser.add_argument(
    "-o", "--output",
    dest="outfile",
    help="Output file name",
    required=True
)

parser.add_argument(
    "test_file",
    nargs="+",
    help="Test Case file(s)"
)

args = parser.parse_args()

print("Output file:", args.outfile)
print("Input files:", args.test_file)

cases = []
for file in args.test_file:
    cases += build_cases(file)

CACHE_STR = {}

with open(args.outfile,'w') as outfile:
    with redirect_stdout(outfile):

        print("/**")
        print(" * AUTOMATICALLY GENERATED - DO NOT EDIT")
        print(" */")
        print("#include \"test.h\"")        
        print("")

        i = 0
        for case in cases:
            num = f"{i:03}"
            str_num = num

            #
            # We need to output the text first so we can reference it...
            #
            if (case["text"] in CACHE_STR):
                str_num = CACHE_STR[case["text"]]
            else:
                CACHE_STR[case["text"]] = num
                print(F"const char text_{num}[] = {{\n    {do_hex(case["text"])} }};")


            print(F"const struct testcase case_{num} = {{")
            print(F"\t.group = \"{case["group"]}\"," )
            print(F"\t.name = \"{case["name"]}\"," )
            print(F"\t.desc = \"{case["desc"]}\"," )
            print(F"\t.regex = \"{quote(case["regex"])}\"," )
            print(F"\t.text = (char *)text_{str_num}," )
            print(F"\t.groups = {len(case["res"])}," )
            if ("error" in case):
                print(F"\t.error = E_{case["error"]},")
            else:
                print(F"\t.error = E_OK,")
            if ("iter" in case):
                print(F"\t.iter = {case["iter"]},")
            else:
                print(F"\t.iter = 100000,")
            if ("cflags" in case):
                print(F"\t.cflags = " + "|".join(case["cflags"]) + ",")
            else:
                print(F"\t.cflags = 0,")

            out = "\t.res = { " + ", ".join(f"{{ {a}, {b} }}" for a, b in case["res"]) + " },"
            print(out)

            print("};")
            i += 1

        print("const struct testcase *cases[] = {")
        i = 0
        for case in cases:
            num = f"{i:03}"
            print(F"\t&case_{num},")
            i += 1
        
        print("\t0\n};")
