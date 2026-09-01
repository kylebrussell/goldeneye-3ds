#!/usr/bin/env python3
"""Emit native exact global AI bytecode from the matching MIPS object."""

from __future__ import annotations

import argparse
import struct
from pathlib import Path


def c_string(blob: bytes) -> str:
    rows = []
    for start in range(0, len(blob), 16):
        rows.append("    " + ", ".join(f"0x{x:02x}" for x in blob[start:start + 16]))
    return ",\n".join(rows)


def parse_elf(path: Path):
    blob = path.read_bytes()
    if blob[:6] != b"\x7fELF\x01\x02":
        raise ValueError("expected 32-bit big-endian ELF")
    shoff = struct.unpack_from(">I", blob, 32)[0]
    shentsize, shnum, shstrndx = struct.unpack_from(">HHH", blob, 46)
    sections = [struct.unpack_from(">10I", blob, shoff + i * shentsize)
                for i in range(shnum)]
    shstr = sections[shstrndx]
    shnames = blob[shstr[4]:shstr[4] + shstr[5]]

    def name(table: bytes, offset: int) -> str:
        end = table.index(0, offset)
        return table[offset:end].decode("ascii")

    names = [name(shnames, section[0]) for section in sections]
    data_index = names.index(".data")
    data_section = sections[data_index]
    data = blob[data_section[4]:data_section[4] + data_section[5]]
    sym_index = names.index(".symtab")
    sym_section = sections[sym_index]
    str_section = sections[sym_section[6]]
    strings = blob[str_section[4]:str_section[4] + str_section[5]]
    symbols = []
    for offset in range(sym_section[4], sym_section[4] + sym_section[5],
                        sym_section[9]):
        word = struct.unpack_from(">IIIBBH", blob, offset)
        symbols.append((name(strings, word[0]) if word[0] else "",
                        word[1], word[2], word[5]))
    by_name = {symbol[0]: symbol for symbol in symbols}
    global_table = by_name["g_GlobalAILists"]
    relocations = {}
    for section in sections:
        if section[1] != 9 or section[7] != data_index:
            continue
        for offset in range(section[4], section[4] + section[5], section[9]):
            target, info = struct.unpack_from(">II", blob, offset)
            relocations[target] = symbols[info >> 8][0]
    records = []
    for offset in range(global_table[1], global_table[1] + global_table[2], 8):
        ai_id = struct.unpack_from(">i", data, offset + 4)[0]
        symbol = relocations.get(offset)
        if symbol is None:
            if ai_id != 0:
                raise ValueError("non-null global AI record lacks relocation")
            break
        entry = by_name[symbol]
        if entry[3] != data_index or entry[2] == 0:
            raise ValueError(f"invalid global AI symbol {symbol}")
        records.append((symbol, ai_id, data[entry[1]:entry[1] + entry[2]]))
    return records


def render(records) -> str:
    pieces = [
        "/* Generated exact global AI bytecode from chraidata.o. */",
        "#include <ultra64.h>",
        "#include <bondtypes.h>",
        '#include "ge_original_global_ai.h"',
        "",
    ]
    for symbol, _, data in records:
        pieces.append(f"static u8 ge_{symbol}[] = {{\n{c_string(data)}\n}};")
    # Keep the canonical public table name so the unchanged campaign chrai
    # resolver can consume the same exact bytecode records directly.
    pieces.append("AIListRecord g_GlobalAILists[] = {")
    for symbol, ai_id, _ in records:
        pieces.append(f"    {{(AIRecord *)(void *)ge_{symbol}, {ai_id}}},")
    pieces.extend([
        "    {NULL, 0}",
        "};",
        "",
        "AIRecord *ge_original_global_ai_find(int32_t ai_list_id)",
        "{",
        "    uint32_t index;",
        "    for(index=0U;g_GlobalAILists[index].ailist!=NULL;++index)",
        "        if(g_GlobalAILists[index].ID==ai_list_id)",
        "            return g_GlobalAILists[index].ailist;",
        "    return NULL;",
        "}",
        "",
        "uint32_t ge_original_global_ai_count(void)",
        "{",
        "    return (uint32_t)(sizeof(g_GlobalAILists)",
        "        /sizeof(g_GlobalAILists[0])-1U);",
        "}",
        "",
    ])
    return "\n".join(pieces)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("object", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    records = parse_elf(args.object)
    if len(records) != 18:
        raise ValueError(f"expected 18 global AI lists, got {len(records)}")
    args.output.write_text(render(records))
    print(f"generated {len(records)} exact global AI lists")


if __name__ == "__main__":
    main()
