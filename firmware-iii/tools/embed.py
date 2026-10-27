"""Embed a text file as a C byte array: <name>_data[] and <name>_size.

usage: embed.py <file> <output_dir>
"""
import os
import sys

if len(sys.argv) != 3:
    sys.exit("usage: embed.py <file> <output_dir>")

src, out_dir = sys.argv[1], sys.argv[2]
os.makedirs(out_dir, exist_ok=True)
with open(src, "r", encoding="utf-8") as f:
    data = f.read().encode("utf-8")

slug = os.path.basename(src).replace(".", "_")
with open(os.path.join(out_dir, slug + ".h"), "w") as h:
    h.write(f"#ifndef __RESOURCE_{slug.upper()}_H__\n#define __RESOURCE_{slug.upper()}_H__\n")
    h.write(f"extern unsigned const char {slug}_data[];\nextern unsigned const int {slug}_size;\n#endif\n")
with open(os.path.join(out_dir, slug + ".c"), "w") as c:
    c.write(f'#include "{slug}.h"\nunsigned const char {slug}_data[] = {{\n')
    for i in range(0, len(data), 16):
        c.write("".join(f"0x{b:02x}," for b in data[i:i + 16]) + "\n")
    c.write("0x00};\n")
    c.write(f"unsigned const int {slug}_size = {len(data) + 1};\n")
