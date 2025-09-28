#!/usr/bin/python3

import os
import sys
import re

print("Hello")

#
# Build a list of case information from the text file and return
# an array containing all cases.
#
def build_cases():
    cases = []
    case = {}

    with open("test_cases.txt") as f:
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
                    continue

                if not "desc" in case:
                    case["desc"] = ""

                cases.append(case)
                case = {}
                continue

            # Anything else is going to be part of the text input to the test case 
            if "text" in case:
                case["text"] += "\n" + line
            else:
                case["text"] = line

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

cases = build_cases()

i = 0
for case in cases:
    num = f"{i:03}"

    print(case)
    
    #
    # We need to output the text first so we can reference it...
    #
    print(F"const char text_{num}[] = {{\n    {do_hex(case["text"])} }};")


    print(F"const struct case case_{num} = {{")
    print(F"\t.name = \"{case["name"]}\"," )
    print(F"\t.desc = \"{case["desc"]}\"," )
    print(F"\t.regex = \"{quote(case["regex"])}\"," )
    print(F"\t.text = (char *)text_{num}," )
    #print(F"\t.text = {{ {do_hex(case["text"])} }},")
    print(F"\t.groups = {len(case["res"])}," )

    out = "\t.res = { " + ", ".join(f"{{ {a}, {b} }}" for a, b in case["res"]) + " },"
    print(out)

    print("};")
    i += 1