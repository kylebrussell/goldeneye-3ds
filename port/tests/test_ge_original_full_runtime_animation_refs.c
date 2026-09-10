/* Real authored animation records, wide native reference owners and rebinding.
 * No character scheduler or gameplay consumer is replaced by this fixture. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "ge_original_animation_table_refs.h"
#include "ge_original_guard_animation_table.h"

static unsigned char *read_file(const char *path, size_t *size)
{
    FILE *file = fopen(path, "rb");
    assert(file && fseek(file, 0, SEEK_END) == 0);
    long length = ftell(file);
    assert(length > 0 && fseek(file, 0, SEEK_SET) == 0);
    unsigned char *data = malloc((size_t)length);
    assert(data && fread(data, 1, (size_t)length, file) == (size_t)length);
    fclose(file); *size = (size_t)length; return data;
}

int main(int argc, char **argv)
{
    assert(argc == 3);
    size_t segment_size, entries_size;
    unsigned char *segment = read_file(argv[1], &segment_size);
    unsigned char *entries = read_file(argv[2], &entries_size);
    uintptr_t previous[ANIM_MAX];
    unsigned count = 0;
    assert(!ge_original_guard_animation_legacy_resolve(1,0));
    assert(!ge_original_guard_animation_legacy_token((void *)(uintptr_t)1,0));
    for (unsigned lifetime = 0; lifetime < 3; ++lifetime) {
        assert(ge_original_guard_animation_table_bind(segment, segment_size));
        assert(ge_original_guard_animation_entries_bind(entries, entries_size));
        assert(ge_original_animation_table_refs_materialize());
        count = 0;
        for (unsigned index = 0; index < ANIM_MAX; ++index) {
            uintptr_t reference = animation_table_ptrs1[index];
            if (reference == 1) continue;
            assert(reference > UINT32_MAX);
            ModelAnimation *animation = (ModelAnimation *)reference;
            assert(ge_port_guard_animation_owns(animation));
            for(unsigned variant=0;variant<2;++variant){
                uintptr_t base=variant?UINT32_C(0xffffff00):UINT32_C(0x81000000);
                int32_t token=ge_original_guard_animation_legacy_token(animation,base);
                const void *resolved=NULL;uint32_t record_offset=UINT32_MAX;
                assert(token&&ge_original_guard_animation_legacy_reference(token,base,&resolved,&record_offset));
                assert(resolved==animation&&record_offset<segment_size);
                assert(ge_original_guard_animation_legacy_offset(record_offset,base)==token);
                assert(ge_original_guard_animation_legacy_resolve(token,base)==animation);
            }

            assert(animation->unk04 > 0 && animation->bitDescriptors && animation->bitStream);
            int32_t first = ge_port_guard_animation_load_frame(animation, 0);
            int32_t last = ge_port_guard_animation_load_frame(animation, animation->unk04 - 1);
            if (first >= 0 || last > first)
                fprintf(stderr,"animation %u offset %d frames %u frame bits %u; bank %zu\n",
                    index,animation->address,animation->unk04,animation->unk0E,entries_size);
            assert(first < 0 && last <= first);
            assert(ge_port_guard_animation_frame_data(first));
            assert(ge_port_guard_animation_frame_data(last));
            if (lifetime) assert(previous[index] == reference);
            previous[index] = reference;
            ++count;
        }
        for (unsigned index = 0; index < AIRCRAFT_ANIMATION_MAX; ++index)
            assert(ge_port_guard_animation_owns(animation_table_ptrs2[index]));
        assert(animation_table_ptrs1[ANIM_MAX] == 0);
    }
    ge_original_guard_animation_table_reset();
    free(entries); free(segment);
    printf("full native animation refs: %u authored character records + %u aircraft; first/last frames, legacy32 tokens and 3 stage binds pass\n", count, AIRCRAFT_ANIMATION_MAX);
    return 0;
}
