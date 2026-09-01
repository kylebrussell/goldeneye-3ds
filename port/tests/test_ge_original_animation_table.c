#include <assert.h>
#include <stdint.h>
#include <stdio.h>

struct animation_table_data {
    uint8_t data[0xffff];
};

extern struct animation_table_data *ptr_animation_table;
extern uint32_t ge_port_storage_ANIM_DATA_empty[];
extern uint32_t ge_port_storage_ANIM_DATA_walking[];
extern uint32_t ge_port_storage_ANIM_DATA_bond_watch[];
int main(void)
{
    uintptr_t walking_offset =
        (uintptr_t)((uint8_t *)ge_port_storage_ANIM_DATA_walking -
                    (uint8_t *)ge_port_storage_ANIM_DATA_empty);
    uintptr_t watch_offset =
        (uintptr_t)((uint8_t *)ge_port_storage_ANIM_DATA_bond_watch -
                    (uint8_t *)ge_port_storage_ANIM_DATA_empty);

    assert((void *)ptr_animation_table == (void *)ge_port_storage_ANIM_DATA_empty);
    assert(walking_offset == 0x4018U);
    assert(watch_offset == 0x42c8U);
    assert(&ptr_animation_table->data[walking_offset] ==
           (uint8_t *)ge_port_storage_ANIM_DATA_walking);
    assert(ge_port_storage_ANIM_DATA_empty[2] == 0x0000043bU);
    assert(ge_port_storage_ANIM_DATA_walking[0] == 0x0005330cU);
    printf("animation table ABI: walking offset 0x%lx, watch offset 0x%lx\n",
           (unsigned long)walking_offset, (unsigned long)watch_offset);
    return 0;
}
