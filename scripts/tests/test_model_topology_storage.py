#!/usr/bin/env python3
"""Exercise the actual private metadata sizing helper, including ARM limits."""
from pathlib import Path
import re
import subprocess
import tempfile

repo = Path(__file__).resolve().parents[2]
source = (repo / "port/src/ge_original_model_scene.c").read_text()


def function(name):
    match = re.search(rf"static int {name}\([^;]*?\)\s*\{{", source)
    assert match, name
    depth = 0
    for end in range(source.index("{", match.start()), len(source)):
        depth += (source[end] == "{") - (source[end] == "}")
        if depth == 0:
            return source[match.start():end + 1]
    raise AssertionError(name)


test = r'''
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#ifdef GE_STORAGE_SIMULATE_32BIT
#undef SIZE_MAX
#define SIZE_MAX UINT32_MAX
#endif
''' + function("add_size") + "\n" + function("cache_append_topology_storage") + r'''
int main(void)
{
    const size_t counts[] = {0, 1, 2, 3, 7, 15, 255, 65535};
    /* Simulate both ABIs' offset/hash alignment; odd counts require padding
     * before uint64_t slices under the 32-bit size_t layout. */
    for (size_t abi = 0; abi < 2; ++abi) {
        const size_t sizes[] = {44, 4 + abi * 4, 4 + abi * 4,
            4 + abi * 4, 4 + abi * 4, 8, 8, 8};
        const size_t alignments[] = {4, 4 + abi * 4, 4 + abi * 4,
            4 + abi * 4, 4 + abi * 4, 8, 8, 8};
        for (size_t c = 0; c < sizeof(counts) / sizeof(counts[0]); ++c) {
            size_t bytes = 0;
            for (size_t slice = 0; slice < 8; ++slice) {
                const size_t before = bytes;
                size_t offset;
                assert(cache_append_topology_storage(&bytes, counts[c],
                    sizes[slice], alignments[slice], &offset));
                assert(offset >= before && offset % alignments[slice] == 0);
                assert(offset - before < alignments[slice]);
                assert(bytes == offset + counts[c] * sizes[slice]);
            }
        }
    }
    size_t bytes = 0, offset = 0;
    assert(!cache_append_topology_storage(&bytes, SIZE_MAX, 8, 8, &offset));
    assert(bytes == 0);
    bytes = SIZE_MAX - 1;
    assert(!cache_append_topology_storage(&bytes, 1, 4, 8, &offset));
    assert(bytes == SIZE_MAX - 1);
    bytes = SIZE_MAX - 7;
    assert(!cache_append_topology_storage(&bytes, 1, 8, 8, &offset));
    assert(bytes == SIZE_MAX - 7);
    bytes = 0;
    assert(cache_append_topology_storage(&bytes, SIZE_MAX, 1, 1, &offset));
    assert(bytes == SIZE_MAX && offset == 0);
    puts("topology storage: aligned/nonoverlapping slices, multiplication/padding/end overflow passed");
}
'''
with tempfile.TemporaryDirectory(prefix="ge-topology-storage-") as temporary:
    directory = Path(temporary)
    (directory / "test.c").write_text(test)
    for flags in ([], ["-DGE_STORAGE_SIMULATE_32BIT"]):
        subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
                        "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
                        *flags, str(directory / "test.c"), "-o", str(directory / "test")],
                       check=True)
        subprocess.run([str(directory / "test")], check=True)
