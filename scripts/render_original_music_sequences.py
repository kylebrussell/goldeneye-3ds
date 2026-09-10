#!/usr/bin/env python3
"""Privately render authored CSeq music through the complete native ARM32 synth.

Requires a binary built by test_original_music_sequences.sh. No audio device,
emulator configuration, timing benchmark, or ROM-derived output is published.
"""
from __future__ import annotations

import argparse
import array
import hashlib
import json
import math
from pathlib import Path
import re
import resource
import shlex
import shutil
import subprocess
import sys

from generate_original_stage_music import generate


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def metrics(path: Path) -> dict:
    values = array.array("h", path.read_bytes())
    if sys.byteorder != "little":
        values.byteswap()
    tail = values[-22050 * 2:]
    return {
        "pcm_sha256": sha(path),
        "pcm_bytes": path.stat().st_size,
        "tail_rms": math.sqrt(sum(x * x for x in tail) / len(tail)),
        "tail_left_mean": sum(tail[0::2]) / (len(tail) // 2),
        "tail_right_mean": sum(tail[1::2]) / (len(tail) // 2),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--assets", type=Path, required=True, help="private extracted music directory")
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--runner", default="", help="e.g. qemu-arm -L /usr/arm-linux-gnueabihf")
    parser.add_argument("--frames", type=int, default=300, help="736-sample play blocks per track")
    parser.add_argument("--tail", type=int, default=300, help="736-sample blocks after all layers stop")
    parser.add_argument("--track", action="append", help="optional specific asset stems; default all 63")
    parser.add_argument("--timeout", type=int, default=120, help="maximum seconds per offline fixture")
    args = parser.parse_args()
    if args.timeout <= 0:
        parser.error("--timeout must be positive")
    # Assertion failures must not create multi-gigabyte QEMU guest cores.
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    root = Path(__file__).resolve().parents[1]
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    assets = args.assets.resolve()
    registry = generate(root)
    (output / "stage-music-registry.inc").write_text(registry)
    stems = re.findall(r'GE_ORIGINAL_MUSIC_TRACK\(\d+, "[^"]+", "music/(M[^"/]+)\.bin"\)', registry)
    assert len(stems) == 63
    if args.track:
        assert all(track in stems for track in args.track)
        stems = args.track
    inputs = output / "inputs" / "music"
    inputs.mkdir(parents=True, exist_ok=True)
    for name in ["instruments.ctl", "instruments.tbl", *[f"{stem}.bin" for stem in stems]]:
        source = assets / name
        shutil.copyfile(source, inputs / name)
    # Transition fixtures also run in a filtered invocation, so copy their
    # explicit dependencies. The source assets are always read-only.
    transition_tracks = ["Mfolders", "Mnint_rare_logo", "Mintro_eye", "Mwatchmusic",
                         "Mdam", "Mdamx", "Mwindblowing", "Mwatercaverns", "Mdeathsolo"]
    for stem in transition_tracks:
        shutil.copyfile(assets / f"{stem}.bin", inputs / f"{stem}.bin")
    pack = output / "music.gepack"
    subprocess.run([sys.executable, str(root / "scripts/pack_3ds_assets.py"),
                    "--assets", str(inputs.parent), "--output", str(pack),
                    "--source-sha1", "abe01e4aeb033b6c0836819f549c791b26cfde83"], check=True)
    source_paths = [root / "port/tests/test_original_music_sequences.c",
                    root / "scripts/test_original_music_sequences.sh",
                    *sorted((root / "src/libultra/audio").glob("*.c")),
                    *sorted((root / "src/libultrare/audio").glob("*.c")),
                    *[root / "port/src" / name for name in ["ge_original_music_runtime.c",
                      "ge_original_music_bank.c", "ge_audio_abi.c", "ge_audio_output.c", "ge_asset_pack.c"]]]
    report = {
        "environment": "Offline ARM32 native synthesis; no NDSP, speaker output, game route or hardware timing",
        "sample_rate": 22050, "block_samples": 736, "volume": 26212,
        "play_frames": args.frames, "stop_tail_frames": args.tail,
        "binary_sha256": sha(args.binary), "runner": args.runner,
        "asset_pack_sha256": sha(pack),
        "input_sha256": {p.name: sha(p) for p in sorted(inputs.iterdir())},
        "source_sha256": {str(p.relative_to(root)): sha(p) for p in source_paths},
        "results": [],
    }
    def render(case: str, track: str, frames: int, schedule: Path | None = None) -> dict:
        pcm = output / f"{case}.s16le"
        command = [*shlex.split(args.runner), str(args.binary.resolve()), str(pack),
                   f"music/{track}.bin", str(frames), str(args.tail), str(pcm)]
        if schedule:
            command.append(str(schedule))
        result = subprocess.run(command, capture_output=True, text=True, timeout=args.timeout)
        (output / f"{case}.stdout").write_text(result.stdout)
        (output / f"{case}.stderr").write_text(result.stderr)
        if result.returncode or result.stderr:
            raise RuntimeError(f"{case}: exit {result.returncode}; inspect {output / (case + '.stderr')}")
        row = {"case": case, **json.loads(result.stdout), **metrics(pcm)}
        report["results"].append(row)
        (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
        print(json.dumps(row), flush=True)
        return row
    for stem in stems:
        render(stem, stem, args.frames)
    # New runtime/bank/heap creation must be reproducible after full teardown.
    for track in ["Mfolders", "Mdam"]:
        first = next((r for r in report["results"] if r["case"] == track), None)
        if first is None:
            first = render(track, track, args.frames)
        again = render(track + "-repeat", track, args.frames)
        assert first["pcm_sha256"] == again["pcm_sha256"]
    schedule = output / "transitions.txt"
    schedule.write_text(
        "15 stop 0 0 -\n30 set 0 26212 music/Mnint_rare_logo.bin\n"
        "90 set 0 26212 music/Mintro_eye.bin\n180 set 0 26212 music/Mfolders.bin\n"
        "270 set 0 26212 music/Mwatchmusic.bin\n360 set 0 26212 music/Mdam.bin\n"
        "390 set 1 18000 music/Mdamx.bin\n420 set 2 10000 music/Mwindblowing.bin\n"
        "480 volume 0 0 -\n510 volume 0 26212 -\n540 stop 1 0 -\n570 stop 2 0 -\n"
        "600 stop 0 0 -\n630 set 0 26212 music/Mdam.bin\n"
        "720 set 0 26212 music/Mwatercaverns.bin\n810 set 0 26212 music/Mdeathsolo.bin\n"
        "900 stop 0 0 -\n930 set 0 26212 music/Mfolders.bin\n")
    first = render("transitions", "Mfolders", 1050, schedule)
    again = render("transitions-repeat", "Mfolders", 1050, schedule)
    assert first["pcm_sha256"] == again["pcm_sha256"]
    report["repeat_pcm_exact"] = True
    report["completed"] = True
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
