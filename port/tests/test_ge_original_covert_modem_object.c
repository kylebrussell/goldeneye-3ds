#include "ge_original_covert_modem_object.h"
#include "ge_original_prop_state.h"
#include "ge_original_bug_model.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>

#define TEST_WEAPON_SLOT_CAPACITY 30U

int main(void)
{
    GeOriginalPropState prop_state;
    GeOriginalCovertModemObjectStats stats;
    ObjectRecord *objects[TEST_WEAPON_SLOT_CAPACITY];
    size_t index;

    assert(ge_original_prop_state_reset(&prop_state, 137U));
    ge_original_covert_modem_object_reset();
    assert(ge_original_covert_modem_object_model_capacity()
           == TEST_WEAPON_SLOT_CAPACITY);
    assert(ge_original_covert_modem_object_create(
               PROP_CHRWPPKSIL, ITEM_BUG) == NULL);
    assert(ge_original_covert_modem_object_create(
               PROP_CHRBUG, ITEM_WPPKSIL) == NULL);

    for (index = 0U; index < TEST_WEAPON_SLOT_CAPACITY; index++) {
        ObjectRecord *object;
#if defined(GE_PORT_MS_INHERITS)
        const PropDefHeaderRecord *header;
#endif
        PropRecord *prop;
        Model *model;
        int32_t weapon_id;
        int32_t linked_weapon_id;
        int32_t timer;
        uint16_t extrascale;
        uint8_t definition_type;
        objects[index] = ge_original_covert_modem_object_create(
            PROP_CHRBUG, ITEM_BUG);
        assert(objects[index] != NULL);
        object = objects[index];
#if defined(GE_PORT_MS_INHERITS)
        header = (const PropDefHeaderRecord *)(const void *)object;
#endif
        prop = object->prop;
        model = object->model;
        assert(ge_original_covert_modem_object_inspect(
            object, &weapon_id, &linked_weapon_id, &timer,
            &extrascale, &definition_type));
        assert(weapon_id == ITEM_BUG);
        assert(linked_weapon_id == -1);
        assert(timer == -1);
        assert(definition_type == PROPDEF_COLLECTABLE);
#if defined(GE_PORT_MS_INHERITS)
        assert(header->type == PROPDEF_COLLECTABLE);
        assert(header->state == 0U);
        assert(header->extrascale == 0x100U);
#endif
        assert(object->obj == PROP_CHRBUG);
        assert(object->pad == 1);
        assert(extrascale == 0x100U);
        assert(object->damage == 1000.0f);
        assert(prop != NULL && prop->obj == object);
        assert(prop->type == PROP_TYPE_WEAPON);
        assert(prop->rooms[0] == UINT8_MAX);
        assert(!ge_original_prop_state_is_active(prop));
        assert(!ge_original_prop_state_is_enabled(prop));
        assert(model != NULL);
        assert(model->obj == ge_original_bug_model_header());
        assert(model->scale == 0.1f);
        assert(object->projectile == NULL);
        if (index == 0U) {
            assert(ge_original_covert_modem_object_prepare_throw(object, 2U));
            assert(ge_original_covert_modem_object_inspect(
                object, NULL, NULL, &timer, NULL, NULL));
            assert(timer == 1);
            assert(((object->runtime_bitflags & RUNTIMEBITFLAG_OWNER)
                    >> RUNTIMEBITSHIFT_OWNER) == 2U);
            assert(!ge_original_covert_modem_object_prepare_throw(object, 4U));
        }
    }
    assert(ge_original_prop_state_active_count() == 0U);
    assert(ge_original_covert_modem_object_create(
               PROP_CHRBUG, ITEM_BUG) == NULL);

    ge_original_covert_modem_object_snapshot(&stats);
    assert(stats.construction_calls == TEST_WEAPON_SLOT_CAPACITY + 3U);
    assert(stats.successful_constructions == TEST_WEAPON_SLOT_CAPACITY);
    assert(stats.weapon_slot_exhaustions == 1U);
    assert(stats.model_slot_exhaustions == 1U);
    puts("original covert-modem fresh-slot construction: ok");
    return 0;
}
