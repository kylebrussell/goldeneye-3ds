#!/usr/bin/env python3
"""Prove the extracted MoveBond runtime helpers remain canonical."""

from pathlib import Path
import re
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[2]
GENERATOR = ROOT / "scripts/extract_bond_move_runtime_slice.py"
FUNCTIONS = {
    "src/game/bondview2.c": (
        "bondviewMoveAnimationTick",
        "bondviewUpdatePlayerY",
        "bondviewUpdatePlayerCollisionPositionFields",
        "bondviewUpdatePlayerCollisionBounds",
        "MoveBond",
    ),
    "src/game/bondview.c": (
        "currentPlayerGetCrouchPos",
        "currentPlayerSetCameraMode",
    ),
    "src/game/stan.c": (
        "stanGetTileOrderedPointWorldPos",
        "stanGetMoveBondCollisionTiles",
    ),
}


def tokens(body: str) -> list[str]:
    body = re.sub(r"/\*.*?\*/", "", body, flags=re.S)
    body = re.sub(r"//.*", "", body)
    return re.findall(
        r"[A-Za-z_]\w*|0[xX][0-9a-fA-F]+|(?:\d+\.\d*|\.\d+|\d+)"
        r"(?:[eE][+-]?\d+)?[fFuUlL]*|==|!=|<=|>=|&&|\|\||<<|>>|->|"
        r"\+\+|--|[{}()\[\];,.~!+*/%<>=&|^-]",
        body,
    )


def function(text: str, name: str) -> str:
    match = re.search(
        rf"(?m)^[A-Za-z_][^;\n]*\b{re.escape(name)}\s*\([^;]*?\)\s*\{{",
        text,
    )
    assert match, name
    start = match.start()
    brace = text.find("{", match.start(), match.end())
    depth = 0
    for index in range(brace, len(text)):
        if text[index] == "{":
            depth += 1
        elif text[index] == "}":
            depth -= 1
            if depth == 0:
                return text[start:index + 1]
    raise AssertionError(f"unterminated function: {name}")


def main() -> None:
    with tempfile.TemporaryDirectory() as directory:
        output = Path(directory) / "slice.c"
        subprocess.run(
            [
                "python3",
                str(GENERATOR),
                str(ROOT / "src/game/bondview2.c"),
                str(ROOT / "src/game/bondview.c"),
                str(ROOT / "src/game/stan.c"),
                str(output),
            ],
            check=True,
        )
        generated = output.read_text()
        checked = 0
        for relative, names in FUNCTIONS.items():
            source = (ROOT / relative).read_text()
            for name in names:
                assert tokens(function(generated, name)) == tokens(
                    function(source, name)
                ), name
                assert f"/* {name} sha256=" in generated
                checked += 1
        assert checked == 9
    print("bond move runtime exact-slice tests passed (9 canonical bodies)")


if __name__ == "__main__":
    main()
