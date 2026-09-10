#!/usr/bin/env python3
"""Convert all canonical multiplayer arenas from a verified private US ROM.

Only reads the ROM and canonical source; all binary output is task-local.
Solo descriptors and bundles remain separate.
"""
from __future__ import annotations
import argparse
import csv
import hashlib
import json
from pathlib import Path
import re
import struct
import tempfile
import zlib

import generate_3ds_stage_inventory as inv
import generate_3ds_stage_registry as registry
import extract_3ds_stage_collision as collision
import extract_3ds_dam_rooms as rooms
import build_3ds_dam_room_bounds as bounds
from extract_3ds_solo_stages import identity, replace_directory
from extract_3ds_runtime_segments import SUPPORTED_US_SHA1


def arenas(root: Path):
    ids = inv.parse_level_ids((root / "src/bondconstants.h").read_text())
    setup_names = inv.parse_setup_table((root / "src/game/chraidata.c").read_text())
    backgrounds = inv.parse_background_table((root / "src/game/bg.c").read_text())
    info = registry.parse_level_info(root / "src/game/bg.c")
    front = inv.initializer((root / "src/game/front.c").read_text(), "struct mp_stage_setup multi_stage_setups[]")
    front = re.sub(r"//[^\n]*", "", front)
    found = re.findall(r"\{[^{}]*?\b(LEVELID_[A-Z0-9_]+)\s*,\s*([^,]+),\s*(\d+),\s*(\d+)\s*\}", front)
    result = []
    for symbol, unlock, minimum, maximum in found:
        if symbol == "LEVELID_NONE": continue
        level_id = ids[symbol]
        base_setup = setup_names[level_id]
        if not base_setup or not base_setup.startswith("U"): raise ValueError("missing setup mapping")
        bg, stan = backgrounds[symbol]
        result.append(dict(symbol=symbol, level_id=level_id,
            key=symbol.removeprefix("LEVELID_").lower(), max_players=int(maximum),
            unlock_after=unlock.strip(), background=bg, stan=stan,
            setup="Ump_" + base_setup[1:], info=info[symbol]))
    if len(result) != 11: raise ValueError(f"expected 11 canonical arenas, found {len(result)}")
    return result


def rom_entries(root, rom):
    if hashlib.sha1(rom).hexdigest() != SUPPORTED_US_SHA1:
        raise ValueError("unsupported ROM SHA-1; expected canonical US retail revision")
    entries = {}
    for offset, length, path, compressed, _ in csv.reader((root / "scripts/filelist.u.csv").open()):
        if not path.startswith(("assets/obseg/bg/", "assets/obseg/setup/", "assets/obseg/stan/")): continue
        stem = Path(path).stem
        if stem in entries: raise ValueError(f"duplicate ROM file stem {stem}")
        entries[stem] = (int(offset), int(length), int(compressed))
    def read(stem):
        offset, length, compressed = entries[stem]
        if offset < 0 or offset + length > len(rom): raise ValueError("ROM entry outside file")
        raw = rom[offset:offset + length]
        if compressed:
            if raw[:2] != b"\x11\x72": raise ValueError("invalid Rare compressed stream")
            raw = zlib.decompress(raw[2:], -15)
        return raw
    return read


def validate_stan_binary(raw, tiles):
    encoded = bytearray()
    for tile in tiles:
        encoded += struct.pack(">IHH", tile["id"] << 8 | tile["room"], tile["mid"], tile["tail"])
        for point in tile["points"]: encoded += struct.pack(">hhhH", *point)
    offset = struct.unpack_from(">I", raw, 4)[0]
    if offset < 12 or raw[offset:offset + len(encoded)] != encoded:
        raise ValueError("canonical STAN source geometry differs from verified ROM")


def validate_spawn(raw, record, pads):
    pad = record["pad_index"]
    if pad < 0 or pad >= len(pads): raise ValueError("invalid multiplayer spawn pad")
    offset = struct.unpack_from(">I", raw, 24)[0] + pad * 44
    actual = struct.unpack_from(">9f", raw, offset)
    authored = pads[pad]["position"] + pads[pad]["up"] + pads[pad]["look"]
    if any(abs(x-y)>0.0001 for x,y in zip(actual,authored)):
        raise ValueError("multiplayer source spawn geometry differs from ROM")
    name_offset = struct.unpack_from(">I", raw, offset + 36)[0]
    end = raw.find(b"\0", name_offset)
    if not 0 < name_offset < end < len(raw) or raw[name_offset:end].decode() != pads[pad]["stan_name"]:
        raise ValueError("multiplayer source spawn STAN differs from ROM")


def extract(root, entry, read, output):
    bg = read("bg_" + entry["background"] + "_all_p")
    setup = read(entry["setup"])
    stan = read(entry["stan"])
    setup_source = inv.source_for(root, "setup", entry["setup"])
    stan_source = inv.source_for(root, "stan", entry["stan"])
    text = setup_source.read_text()
    pads = inv.parse_pads(text)
    spawns = inv.parse_intro_binary(setup)
    source_spawns = [dict(record_index=int(i),pad_index=int(p),demo_slot=int(d))
        for i,p,d in re.findall(r'/\* Type = Spawn; index = (\d+) \*/\s*\n\s*_mkword\([^\n]+\),\s*(-?\d+),\s*(-?\d+),', inv.initializer(text,"s32 intro[] ="))]
    if spawns != source_spawns: raise ValueError("multiplayer source intro differs from ROM")
    normal = [s for s in spawns if s["demo_slot"] == 0]
    if not 1 <= len(normal) <= 16: raise ValueError("invalid multiplayer spawn count")
    shared = collision.load_dam_collision_module()
    tiles = shared.parse_tiles(re.sub(rb"/\*.*?\*/",b"",stan_source.read_bytes(),flags=re.S), None, entry["key"])
    validate_stan_binary(stan, tiles)
    spawn_manifest = []
    for s in normal:
        validate_spawn(setup, s, pads)
        pad = pads[s["pad_index"]]
        packed = collision.stan_pack_id(pad["stan_name"])
        matches = [i for i,t in enumerate(tiles) if t["id"] == packed]
        if len(matches) != 1: raise ValueError("multiplayer spawn STAN is ambiguous or absent")
        tile = matches[0]
        spawn_manifest.append(dict(pad=s["pad_index"], tile=tile, room=tiles[tile]["room"], stan=pad["stan_name"]))
    first = spawn_manifest[0]
    world = inv.analyze_background(bg)
    output.parent.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="ge-mp-",dir=output.parent) as temporary:
        staging=Path(temporary)/entry["key"]; staging.mkdir()
        (staging/"background.bin").write_bytes(bg)
        (staging/"setup.bin").write_bytes(setup)
        (staging/"collision.gestan").write_bytes(collision.encode_stan(tiles,first["tile"],first["room"]))
        rooms.extract_rooms(staging/"background.bin",staging/"rooms",expected_size=len(bg),expected_sha256=inv.sha256(bg),stage_key="mp_"+entry["key"],source_manifest_path="ROM/bg_"+entry["background"])
        bounds.build_asset(staging/"background.bin",staging/"rooms",staging/"room_bounds.gebounds",expected_source_size=len(bg),expected_source_sha256=inv.sha256(bg),expected_room_count=world["room_count_including_dummy_room_0"],expected_portal_count=world["portal_count"],stage_label=entry["key"])
        files={name:identity(staging/name) for name in ("background.bin","setup.bin","collision.gestan","room_bounds.gebounds")}
        manifest=dict(schema=1,**entry,source_rom_sha1=SUPPORTED_US_SHA1,
            source_setup_sha256=inv.sha256(setup_source.read_bytes()),source_stan_sha256=inv.sha256(stan_source.read_bytes()),
            files=files,spawns=spawn_manifest,room_count=world["room_count_including_dummy_room_0"],portal_count=world["portal_count"],
            tile_count=len(tiles),point_count=sum(len(t["points"]) for t in tiles))
        (staging/"manifest.json").write_text(json.dumps(manifest,indent=2,sort_keys=True)+"\n")
        replace_directory(staging, output)
    return manifest


def render_registry(manifests):
    lines=["/* Generated from verified US ROM by extract_3ds_multiplayer_stages.py. */"]
    for i,m in enumerate(manifests):
        f=m["files"]; info=m["info"]; spawn=m["spawns"][0]
        lines += ["GE_MP_STAGE(", f"    {i}, {m['level_id']}, {info['index']},",
            f"    {info['scale']}f, {info['visibility']}f, {info['distance']}f,",
            f"    \"{m['key']}\", \"{m['background']}\", \"{m['setup']}\", \"{m['stan']}\","]
        for name in ("background.bin","collision.gestan","room_bounds.gebounds","setup.bin"):
            lines += [f"    {f[name]['size']}U, UINT64_C({f[name]['fnv1a64']}),"]
        lines += [f"    {m['room_count']}U, {m['portal_count']}U, {m['tile_count']}U, {m['point_count']}U,",
            f"    {spawn['tile']}U, {spawn['room']}U, {len(m['spawns'])}U, {m['max_players']}U)"]
    return "\n".join(lines)+"\n"


def main():
    root=Path(__file__).resolve().parents[1]
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root",type=Path,default=root)
    parser.add_argument("--rom",type=Path,default=root/"baserom.u.z64")
    parser.add_argument("--output",type=Path,default=root/"build/3ds-multiplayer")
    parser.add_argument("--registry",type=Path,default=root/"port/include/ge_multiplayer_stage_registry.inc")
    parser.add_argument("--stage",action="append",default=[])
    parser.add_argument("--check",action="store_true")
    parser.add_argument("--list",action="store_true",help="print canonical arena keys without reading the ROM")
    args=parser.parse_args()
    try:
        selected=arenas(args.root)
        if args.list:
            print("\n".join(a["key"] for a in selected)); return
        unknown=set(args.stage)-{a["key"] for a in selected}
        if unknown: raise ValueError(f"unknown arenas {sorted(unknown)}")
        read=rom_entries(args.root,args.rom.read_bytes())
        manifests=[]
        for a in selected:
            if args.stage and a["key"] not in args.stage: continue
            manifests.append(extract(args.root,a,read,args.output/a["key"]))
        if not args.stage:
            encoded=render_registry(manifests)
            if args.check:
                if args.registry.read_text()!=encoded: raise ValueError("multiplayer registry is stale")
            else:
                args.registry.parent.mkdir(parents=True,exist_ok=True); args.registry.write_text(encoded)
        print(f"converted {len(manifests)} canonical multiplayer arenas -> {args.output}")
    except (OSError,ValueError,KeyError,zlib.error) as error:
        parser.error(str(error))

if __name__=="__main__": main()
