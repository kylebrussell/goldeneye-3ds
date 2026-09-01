#!/usr/bin/env python3
"""Keep cast and gunbarrel's numerically-equal PropType 7 semantics apart."""

from pathlib import Path


REPO = Path(__file__).resolve().parents[2]


def main() -> None:
    source = (REPO / "platform/3ds/source/main.c").read_text()
    cast = source.index("menu == MENU_DISPLAY_CAST")
    eye = source.index("menu == MENU_EYE_INTRO", cast)
    cast_draw = source[cast:eye]
    assert "viewer_uses_vertex_alpha_lighting" not in cast_draw
    assert "scene->render_prop_type == 7U" not in cast_draw
    assert "generic TRILERP/MODULATEIA path" in cast_draw

    gunbarrel_end = source.index("} else if (menu != MENU_LEGAL_SCREEN", eye)
    gunbarrel = source[eye:gunbarrel_end]
    assert "viewer_uses_vertex_alpha_lighting != 0U" in gunbarrel
    assert "bond_scene->render_prop_type == 7U" in gunbarrel
    print("Frontend model render modes: cast generic; gunbarrel VIEWER alpha-lighting")


if __name__ == "__main__":
    main()
