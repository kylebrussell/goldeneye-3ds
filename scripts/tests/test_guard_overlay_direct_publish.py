#!/usr/bin/env python3
"""Pin the 3DS animated-guard cache directly to the scene overlay tail."""

from pathlib import Path


REPO = Path(__file__).resolve().parents[2]


def main() -> None:
    source = (REPO / "platform/3ds/source/main.c").read_text()
    start = source.index("static bool refresh_stage_guard_overlay_impl(")
    end = source.index("static const RuntimeStageScenePartRange", start)
    body = source[start:end]
    direct = body.index(
        "storage.vertices = dynamic_scene->overlay_vertices")
    build = body.index(
        "ge_original_stage_guard_runtime_build_scene_cached_exact(", direct)
    commit = body.index(
        "ge_dam_dynamic_scene_commit_overlay_batches(", build)
    assert direct < build < commit
    replacement = body.index("replace_topology:", commit)
    assert "ge_original_stage_guard_runtime_build_scene_cached(" in body[replacement:]
    published = body.index("objects->guard_scene = scene.required_vertex_count", replacement)
    assert "objects->guard_scene.status = GE_ORIGINAL_STAGE_GUARD_RUNTIME_OK;" in body[published:]
    model = (REPO / "port/src/ge_original_model_scene.c").read_text()
    collector = model.split("static int collect_model_draw(", 1)[1].split("\n}\n", 1)[0]
    publication = collector.index("if (context->write_output != 0U)")
    assert "ge_pica_material_translate(" not in collector[:publication]
    assert "ge_pica_material_translate(" in collector[publication:]
    # Sizing may omit material translation only while every non-NULL render
    # state translates successfully; unsupported features retain fallbacks.
    translator = (REPO / "port/src/ge_pica_material.c").read_text().split(
        "GePicaMaterialStatus ge_pica_material_translate(", 1)[1].split("\n}\n", 1)[0]
    assert "if (state == NULL || material == NULL)" in translator
    assert translator.count("return ") == 2
    assert translator.count("return GE_PICA_MATERIAL_INVALID_ARGUMENT;") == 1
    assert translator.count("return GE_PICA_MATERIAL_OK;") == 1
    sizing = model.index("|| (exact_size && (cache->required_vertex_count")
    assert sizing < model.index("cache_prepare_publication_matrices(\n", sizing)
    assert "ge_dam_dynamic_scene_update_overlay_segment(" not in body
    pose_only = body.index("if (!range->static_data_changed)")
    pose_commit = body.index("ge_dam_dynamic_scene_commit_overlay_rooms(", pose_only)
    pose_end = body.index("continue;", pose_commit)
    assert "ge_dam_dynamic_scene_commit_overlay_batches(" not in body[pose_only:pose_end]
    assert pose_commit < body.index("GeDamRoomDrawBatch *batch =", pose_commit)
    assert "*batch = segment->batches[batch_index];" in body[pose_commit:]
    # Empty guard sets still own their insertion point after ordinary props
    # and doors. The live all-stage installer must preserve it just like the
    # older Dam-only installer; otherwise the first visible guard is inserted
    # at zero while subsequent publication assumes it occupies the tail.
    start = source.index("static bool install_stage_ordinary_object_scenes(")
    end = source.index("fail_stage_scene:", start)
    install = source[start:end]
    # Reuse the existing validated sizing pass when writing prop/door output.
    assert "ge_original_model_scene_build_preflighted(\n" in install
    assert "&inputs[input_index], &queries[input_index], &storage, &built)" in install
    boundary = install.index("guard_vertex_offset = vertex_count;")
    capture = install.index("&guard_candidate, vertices, guard_vertex_offset")
    assert boundary < install.index(
        "guard_candidate.vertex_offset = guard_vertex_offset;") < capture
    assert boundary < install.index(
        "guard_candidate.batch_offset = guard_batch_offset;") < capture
    door_capture = install.index("&door_candidate, vertices, door_vertex_offset")
    assert boundary < install.index(
        "door_candidate.vertex_offset = door_vertex_offset;") < door_capture
    assert boundary < install.index(
        "door_candidate.batch_offset = door_batch_offset;") < door_capture
    empty = install[install.index("if (input_count == 0U"):
                    install.index("RUNTIME_STAGE_SCENE_INSTALL_ALLOCATE_INPUTS;")]
    assert "dam_overlay_segment_close(&objects->door_overlay);" in empty
    assert "dam_overlay_segment_close(&objects->guard_overlay);" in empty
    print("3DS guard overlay: exact-size scene-tail publication; shrink/growth replace once, batch-only commit retained")


if __name__ == "__main__":
    main()
