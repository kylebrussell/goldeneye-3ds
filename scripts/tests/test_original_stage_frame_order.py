#!/usr/bin/env python3
"""Pin the decompiled boss/lv mission-AI and prop-frame boundaries."""

from pathlib import Path


REPO = Path(__file__).resolve().parents[2]


def main() -> None:
    source = (REPO / "platform/3ds/source/main.c").read_text()
    loop = source.index("for (original_tick = 0U;")
    pre = source.index(
        "ge_original_stage_active_props_pre_tick_exact(", loop)
    shuffle = source.index("shuffle_player_ids();", pre)
    move = source.index("ge_original_bond_move_live_tick(", shuffle)
    props = source.index("ge_original_stage_active_props_tick_exact(", move)
    autoaim = source.index("chrpropUpdateAutoaimTarget();", props)
    assert loop < pre < shuffle < move < props < autoaim
    music = source.index("ge_original_music_runtime_tick_60hz(original_music)", shuffle)
    assert shuffle < music < move
    assert "const bool render_music = audio_active && original_music != NULL;" in source[shuffle:music]
    assert "? ge_original_music_runtime_tick_60hz(original_music) : GE_AUDIO_ABI_OK" in source[music - 2:move]
    assert "if (music_status != GE_AUDIO_ABI_OK)" in source[music:move]
    assert "Exact bossMainloop/lvlManageMpGame boundary" in source[loop:pre]

    owner = (REPO / "port/src/ge_original_stage_active_props.c").read_text()
    pre_body = owner[
        owner.index("ge_original_stage_active_props_pre_tick_exact("):
        owner.index("ge_original_stage_active_props_tick_exact(")
    ]
    props_body = owner[owner.index("ge_original_stage_active_props_tick_exact("):]
    assert "ge_original_dam_guard_all_chr_tick_exact();" in pre_body
    assert "ge_original_dam_guard_props_tick_exact();" not in pre_body
    assert "ge_original_dam_guard_all_chr_tick_exact();" not in props_body
    assert "ge_original_dam_guard_props_tick_exact();" in props_body
    assert "if(!state->pre_tick_pending)" in props_body
    print("Original frame order: mission AI -> shuffle -> MoveBond -> propsTick -> autoaim")


if __name__ == "__main__":
    main()
