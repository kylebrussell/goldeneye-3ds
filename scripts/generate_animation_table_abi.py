#!/usr/bin/env python3
"""Expose the authored animation blob with the N64 absolute-offset symbol ABI."""

from __future__ import annotations

import argparse
import re
from pathlib import Path


ARRAY_RE = re.compile(r"^u32\s+(ANIM_DATA_[A-Za-z0-9_]+)\s*\[\]\s*=", re.MULTILINE)
ENTRY_ARRAY_RE = re.compile(
    r"^u32\s+(ANIM_ENTRY_[A-Za-z0-9_]+)\s*\[\]\s*=", re.MULTILINE)


def render(source: Path) -> str:
    text = source.read_text()
    names = ARRAY_RE.findall(text)
    entries_source = source.with_name("animationtable_entries.c")
    entry_names = ENTRY_ARRAY_RE.findall(entries_source.read_text())
    if not names or names[0] != "ANIM_DATA_empty" or len(names) != len(set(names)):
        raise ValueError("unexpected animation-table definition layout")
    if not entry_names or entry_names[0] != "ANIM_ENTRY_idle" \
            or len(entry_names) != len(set(entry_names)):
        raise ValueError("unexpected animation-entry definition layout")

    lines = [
        "/* Generated ABI wrapper; the data body remains assets/animationtable_data.c. */",
        "#include <stddef.h>",
        "#include <ultra64.h>",
        "",
    ]
    lines.extend(f"#define {name} ge_port_storage_{name}" for name in names)
    lines.extend([
        '#include <assets/animationtable_data.c>',
        "",
    ])
    lines.extend(f"#undef {name}" for name in names)
    lines.extend(f"#define {name} ge_port_storage_{name}"
                 for name in entry_names)
    lines.extend([
        '#include <assets/animationtable_entries.c>',
        "",
    ])
    lines.extend(f"#undef {name}" for name in entry_names)
    lines.extend([
        "",
        "size_t ge_port_embedded_animation_entries_size(void)",
        "{",
        "    return (size_t)((u8 *)ge_port_storage_ANIM_ENTRY_helicopter_takeoff",
        "        + sizeof(ge_port_storage_ANIM_ENTRY_helicopter_takeoff)",
        "        - (u8 *)ge_port_storage_ANIM_ENTRY_idle);",
        "}",
        "",
        "int ge_port_embedded_animation_entries_read(size_t offset, void *dst, size_t size)",
        "{",
        "    u8 *output = dst;",
        "    const u32 *words = ge_port_storage_ANIM_ENTRY_idle;",
        "    size_t total = ge_port_embedded_animation_entries_size();",
        "    size_t index;",
        "    if (dst == NULL || offset > total || size > total - offset)",
        "        return 0;",
        "    for (index = 0; index < size; index++) {",
        "        size_t source = offset + index;",
        "        u32 word = words[source >> 2];",
        "        output[index] = (u8)(word >> (24u - 8u * (source & 3u)));",
        "    }",
        "    return 1;",
        "}",
        "",
        "struct animation_table_data { u8 data[0xffff]; };",
        '_Static_assert(offsetof(struct animation_table_data, data) == 0, "animation base ABI");',
        "struct animation_table_data *ptr_animation_table =",
        "    (struct animation_table_data *)(void *)ge_port_storage_ANIM_DATA_empty;",
        "",
        "/*",
        " * The original linker exposes ANIM_DATA_* as byte offsets from the",
        " * animation segment base.  Keep the storage labels private and recreate",
        " * those absolute symbols so unchanged decompiled pointer arithmetic",
        " * continues to address the authored table rather than double-adding a",
        " * native process address.",
        " */",
    ])
    lines.append("#ifndef GE_PORT_ANIMATION_TABLE_HOST_TEST")
    for name in names:
        lines.extend([
            f'__asm__(".global {name}\\n"',
            f'        ".set {name}, ge_port_storage_{name} - ge_port_storage_ANIM_DATA_empty\\n");',
        ])
    lines.extend([
        '__asm__(".global _animation_entriesSegmentRomStart\\n"',
        '        ".set _animation_entriesSegmentRomStart, ge_port_storage_ANIM_ENTRY_idle\\n");',
    ])
    lines.append("#endif")
    lines.append("")
    return "\n".join(lines)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    args.output.write_text(render(args.source))


if __name__ == "__main__":
    main()
