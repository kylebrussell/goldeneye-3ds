#!/usr/bin/env python3
"""Token-check the exact non-tank MoveBond dependency slice."""

from __future__ import annotations

import argparse
import importlib.util
import re
import subprocess
import tempfile
from pathlib import Path


def tokens(text: str) -> list[str]:
    text = re.sub(r"/\*.*?\*/|//[^\n]*", "", text, flags=re.S)
    return re.findall(r"[A-Za-z_]\w*|0[xX][0-9A-Fa-f]+|\d+(?:\.\d*)?(?:[eE][+-]?\d+)?[fFlL]?|==|!=|<=|>=|&&|\|\||->|\S", text)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("repo", type=Path)
    args = parser.parse_args()
    path = args.repo / "scripts/extract_bond_move_non_tank_slice.py"
    spec = importlib.util.spec_from_file_location("move_non_tank_extract", path)
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    generated = module.generate(args.repo)
    constants = (args.repo / "src/bondconstants.h").read_text()
    bondview2 = (args.repo / "src/game/bondview2.c").read_text()
    assert re.search(r"BITFLAG\(PLAYERFLAG,\s*LOCKCONTROLS,\s*NOCONTROL,\s*NOTIMER", constants)
    assert "#define PLAYERFLAG_NOTIMER (1 << 2)" in generated
    assert "#define PLAYERFLAG_LOCKCONTROLS (1 << 0)" in generated
    assert re.search(r"#else.*?#define PLAYER_TICKEXPLODE_FACTOR 15", bondview2, re.S)
    assert "#define PLAYER_TICKEXPLODE_FACTOR 15" in generated
    checked = 0
    for relative, names in module.VARIABLES.items():
        for name in names:
            canonical = module.extract_variable((args.repo / relative).read_text(), name)
            assert tokens(canonical) == tokens(module.extract_variable(generated, name)), name
            assert f"{name} sha256={module.digest(canonical)}" in generated
            checked += 1
    for relative, names in module.FUNCTIONS.items():
        for name in names:
            canonical = module.extract_function((args.repo / relative).read_text(), name)
            assert f"{name} sha256={module.digest(canonical)}" in generated
            if name == "sub_GAME_7F0C0BF0":
                canonical = canonical.replace(
                    "    get_mTrack2Vol();", "    return get_mTrack2Vol();")
            assert tokens(canonical) == tokens(module.extract_function(generated, name)), name
            checked += 1
    assert checked == 141
    # Execute the emitted getter/forwarder with optimization enabled. A
    # token-only check previously preserved the missing-return C bug.
    fixture = "\n".join((
        "#include <stdint.h>", "#include <assert.h>", "typedef uint16_t u16;",
        module.extract_variable(generated, "mTrack2Vol"),
        module.extract_function(generated, "get_mTrack2Vol"),
        module.extract_function(generated, "sub_GAME_7F0C0BF0"),
        "int main(void) { for (uint32_t v = 0; v <= 0x7fff; ++v) {",
        "mTrack2Vol = (u16)v; assert(sub_GAME_7F0C0BF0() == v); } return 0; }",
    ))
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        (path / "volume.c").write_text(fixture)
        subprocess.run(["cc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
                        "-fsanitize=undefined", str(path / "volume.c"),
                        "-o", str(path / "volume")], check=True)
        subprocess.run([str(path / "volume")], check=True)
    print(f"bond move non-tank exactness: {checked - 1} canonical bodies/data; "
          "explicit native music-volume return passes all 32768 settings")


if __name__ == "__main__":
    main()
