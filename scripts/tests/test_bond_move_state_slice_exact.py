#!/usr/bin/env python3
"""Verify the generated MoveBond state/service tranche is source-exact."""

from pathlib import Path
import importlib.util
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[2]
GENERATOR = ROOT / "scripts/extract_bond_move_state_slice.py"


def main() -> None:
    spec = importlib.util.spec_from_file_location("move_state_generator", GENERATOR)
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    paths = {
        "bondview": ROOT / "src/game/bondview.c",
        "bondview2": ROOT / "src/game/bondview2.c",
        "player": ROOT / "src/game/player.c",
        "lv": ROOT / "src/game/lv.c",
        "debugmenu": ROOT / "src/game/debugmenu_handler.c",
        "model": ROOT / "src/game/model.c",
        "stan": ROOT / "src/game/stan.c",
    }
    sources = {name: path.read_text() for name, path in paths.items()}

    with tempfile.TemporaryDirectory() as directory:
        output = Path(directory) / "state.c"
        subprocess.run(
            ["python3", str(GENERATOR), *(str(paths[name]) for name in (
                "bondview", "bondview2", "player", "lv", "debugmenu",
                "model", "stan")), str(output)],
            check=True,
        )
        generated = output.read_text()
        checked = 0
        for source_name, names in module.VARIABLES.items():
            for name in names:
                declaration = module.extract_variable(sources[source_name], name)
                assert declaration in generated, name
                assert f"/* {name} sha256={module.digest(declaration)} */" in generated
                checked += 1
        for source_name, names in module.FUNCTIONS.items():
            for name in names:
                body = module.extract_function(sources[source_name], name)
                assert body in generated, name
                assert f"/* {name} sha256={module.digest(body)} */" in generated
                checked += 1
        assert checked == 21
    print("bond move state exact-slice tests passed (21 declarations/bodies)")


if __name__ == "__main__":
    main()
