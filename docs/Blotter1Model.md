# Desk blotter model milestone

`PROP_BLOTTER1` is the first ordinary GoldenEye game model prepared for the
portable GBI renderer. It is a small, textured desk blotter: four render
vertices, one matrix load, one texture reference, and two triangles. That makes
it useful for testing the complete model path without using a special-case logo
or a large level resource.

## Source and verification

The original model definition is
`assets/obseg/prop/blotter1/Model.c`. Both `scripts/filelist.u.csv` and the
US linker map place the compressed `Pblotter1Z` resource at ROM offset
`0x7b2790`; the next resource begins at `0x7b2870`, giving an exact compressed
size of 224 bytes. `assets/obseg/ob_seg.s` independently associates that symbol
with the prop resource.

The extraction tool accepts only the verified US ROM with SHA-1
`abe01e4aeb033b6c0836819f549c791b26cfde83`. It verifies these values before
replacing any prior output:

| Data | Bytes | SHA-256 |
| --- | ---: | --- |
| ROM slice | 224 | `5980a683a8a46ffad779d342b9ad420738cad636bd85e8fb6eecf3b04bf4cd41` |
| Decompressed model | 400 | `ef7456e5edde110e8b2632e1ee1feb059ceffab4401fb165c8fcdf375d69e668` |
| Texture table | 12 | `fe08de7c6378c409fc73fb16f3c844ccd8460e125f8363340dcc628f87092155` |
| Render vertices | 64 | `ec38236f0927cba752ea9bdf2b74f8792c32dc906cbdeb4b7171c8c5047c5229` |
| Display list | 80 | `8b49b83f8170b9964902e506d4c40b3e8cb98f69aa1f404a6166ffd1440caf46` |
| Preview identity matrix | 64 | `3cc03f81f5d9a6c8cca5b9b3db46efaa284d60c34d7d5d5083ff809e30bbc52e` |

The model references texture ID 182, `IMAGE_BLOTTER`, whose converted source is
`BLOTTER.bin` (64 by 32 pixels). The display list remains in original
big-endian GBI form and contains 10 commands. No private ROM-derived bytes are
checked into the repository.

## Runtime layout

The private output under `build/3ds-models/blotter1` has stable, separately
loadable sections:

| Segment | Pack path | Purpose |
| ---: | --- | --- |
| 5 | `converted/models/blotter1/display_list.bin` | Root display list at offset zero |
| 4 | `converted/models/blotter1/vertices.bin` | Four original render vertices |
| 3 | `converted/models/blotter1/matrix_identity.bin` | Portable preview matrix |

The model's matrix command points to segment 3, offset zero. GoldenEye normally
supplies that model-view matrix at draw time; it is not embedded in the model.
The extracted `matrix_identity.bin` is therefore explicitly labeled
`portable-preview-identity` in the manifest. A gameplay renderer should replace
that binding with the prop instance's live transform. Display lists, render
vertices, the original model file, and the texture table are preserved byte for
byte.

The generated `manifest.json` records the ROM provenance, section hashes,
segment bindings, texture reference, and expected portable-pipeline result:
10 visited commands, one vertex batch, four fetched vertices, one matrix, one
draw call, and two triangles.

## Build and test

Extract only this model:

```sh
python3 scripts/extract_3ds_blotter_model.py \
  --rom baserom.u.z64 \
  --output build/3ds-models/blotter1
```

`scripts/build_3ds_assets.sh` runs that extraction and packs the directory at
`converted/models/blotter1`. The focused Python tests use a synthetic resource;
the host C test uses the privately extracted files to run the preserved display
list through the portable decoder, traversal, matrix, state, and pipeline code.
Run both, along with the other portable tests, with:

```sh
scripts/test_port.sh
```

`ge_blotter_model_build` is the portable frontend boundary for those three
blobs. It runs the preserved display list, validates the expected topology and
material, and returns two self-contained processed triangles plus the complete
Rare texture state. Pack consumers can use the path constants in
`ge_blotter_model.h`, then release their input buffers as soon as the build
returns. A live prop instance replaces only the segment-3 matrix blob.

The remaining platform work is intentionally narrow: translate the six returned
vertices into Citro3D submission and resolve the returned texture ID 182 through
the texture cache.
