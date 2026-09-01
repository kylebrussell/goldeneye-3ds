#!/usr/bin/env python3
"""Verify exact watch extraction and the measured GBI frontier."""

from __future__ import annotations

import importlib.util
import json
import subprocess
import tempfile
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
EXTRACT = REPO / "scripts/extract_watch_render_slice.py"


def load_extractor():
    spec = importlib.util.spec_from_file_location("watch_extract", EXTRACT)
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def main() -> None:
    extractor = load_extractor()
    options = (REPO / "src/game/options.c").read_text()
    bondview = (REPO / "src/game/bondview2.c").read_text()
    live_slice = (REPO / "scripts/extract_bond_move_non_tank_slice.py").read_text()
    with tempfile.TemporaryDirectory() as directory:
        output = Path(directory) / "watch.c"
        bondview_output = Path(directory) / "bondview_watch.inc.c"
        report = Path(directory) / "watch.json"
        subprocess.run([
            "python3", str(EXTRACT), str(REPO), str(output),
            "--report", str(report),
            "--bondview-output", str(bondview_output),
        ], check=True)
        generated = output.read_text()
        retained_bondview = bondview_output.read_text()
        frontier = json.loads(report.read_text())
    body = extractor.function_text(options, "draw_watch_current_page")
    canonical_body = body[body.index("{"):]
    assert canonical_body in generated
    watch_body = extractor.function_text(bondview, "bondviewRenderWatch")
    canonical_watch_body = watch_body[watch_body.index("{"):]
    assert canonical_watch_body in retained_bondview
    assert "ge_original_bondview_render_watch_exact" in retained_bondview
    assert "draw_background_health_and_armor_transitioning" \
        in frontier["draw_watch_current_page_calls"]
    assert len(frontier["page_calls"]) == 6
    assert frontier["missing_gbi_commands"] == []
    for navigation in (
        "watch_screen0_navigation",
        "watch_screen1_navigation",
        "watch_screen2_navigation",
        "watch_screen3_navigation",
        "watch_screen4_navigation",
        "game_options_inventory_navigation",
        "mission_brief_background_navigation",
        "mission_brief_m_briefing_navigation",
        "mission_brief_q_branch_navigation",
        "mission_brief_moneypenny_navigation",
        "mission_brief_objectives_navigation",
    ):
        assert f'"{navigation}"' in live_slice
    print(
        "Watch frontier: exact bodies retained; canonical navigation present; "
        "texture-rectangle GBI frontier closed"
    )


if __name__ == "__main__":
    main()
