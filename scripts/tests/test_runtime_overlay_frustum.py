#!/usr/bin/env python3
"""Pin conservative frustum rejection to every 3DS world coordinate space."""

from pathlib import Path


REPO = Path(__file__).resolve().parents[2]


def main() -> None:
    source = (REPO / "platform/3ds/source/main.c").read_text()
    start = source.index("static bool renderer_world_batch_may_draw(")
    end = source.index("static RuntimeGbiModel rareware_body_model", start)
    body = source[start:end]
    for matrix in (
        "preview->authored_world_to_clip",
        "preview->runtime_world_to_clip",
        "preview->eye_to_clip",
    ):
        assert matrix in body
    assert "source_index >= room_batch_count" not in body
    assert source.count("renderer_world_batch_may_draw(") == 3
    print("3DS frustum culling: authored rooms plus runtime/eye-space overlays")


if __name__ == "__main__":
    main()
