#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define GE_PORT_SETUP_DATA
#include "ge_original_dam_mission_hud.h"
#include "bondtypes.h"
#include "game/bondview.h"

extern void hudmsgTopShow(char *message);
extern unsigned char *langGet(int32_t slot_id);
extern char *LsevxbE[];
extern char *LsevbE[];
extern char *LstatE[];
extern char *LarchE[];
extern char *LpeteE[];
extern char *LdepoE[];
extern char *LtraE[];
extern char *LjunE[];
extern char *LarecE[];
extern char *LcaveE[];
extern char *LcradE[];
extern char *LaztE[];
extern char *LcrypE[];

int32_t g_ClockTimer = 1;
int32_t g_UpperTextDisplayFlag;
int32_t clock_drawn_flag = 1;
typedef struct AmmoStats {
    uint32_t MaxAmmo;
    uint32_t IconImage;
    float IconYOffset;
} AmmoStats;
AmmoStats ammo_related[30];
static struct player ge_test_player;
struct player *g_CurrentPlayer = &ge_test_player;
static int ge_test_weapons[2];

int getCurrentPlayerWeaponId(int hand)
{
    return ge_test_weapons[hand];
}

int get_ammo_type_for_weapon(int weapon)
{
    return weapon;
}

uint32_t bondwalkItemCheckBitflags(int item, uint32_t mask)
{
    (void)item;
    (void)mask;
    return 0U;
}

int getPlayerCount(void)
{
    return 1;
}

int get_cur_playernum(void)
{
    return 0;
}

int main(void)
{
    static const struct {
        int32_t text_id;
        const char *const *bank;
        size_t index;
    } later_solo_texts[] = {
        { getStringID(LSEVXB, 4), (const char *const *)LsevxbE, 4U },
        { getStringID(LSEVB, 4), (const char *const *)LsevbE, 4U },
        { getStringID(LSTAT, 4), (const char *const *)LstatE, 4U },
        { getStringID(LARCH, 4), (const char *const *)LarchE, 4U },
        /* Streets' first authored objective deliberately uses text slot 6. */
        { getStringID(LPETE, 6), (const char *const *)LpeteE, 6U },
        { getStringID(LDEPO, 4), (const char *const *)LdepoE, 4U },
        { getStringID(LTRA, 4), (const char *const *)LtraE, 4U },
        { getStringID(LJUN, 4), (const char *const *)LjunE, 4U },
        { getStringID(LAREC, 4), (const char *const *)LarecE, 4U },
        /* Caverns skips the unused slot 8 before its fifth objective. */
        { getStringID(LCAVE, 9), (const char *const *)LcaveE, 9U },
        { getStringID(LCRAD, 4), (const char *const *)LcradE, 4U },
        { getStringID(LAZT, 4), (const char *const *)LaztE, 4U },
        { getStringID(LCRYP, 4), (const char *const *)LcrypE, 4U },
    };
    static const struct {
        int ammo_type;
        uint32_t address;
        uint16_t image_id;
        uint8_t width;
        uint8_t height;
        float y_offset;
        const char *source;
    } expected_icons[] = {
        { AMMO_9MM, UINT32_C(0x02000C84), 2231, 5, 12, 0.0f,
          "9MMAMMO.bin" },
        { AMMO_RIFLE, UINT32_C(0x02000C90), 2232, 5, 28, -2.0f,
          "RIFLEAMMO.bin" },
        { AMMO_SHOTGUN, UINT32_C(0x02000C9C), 2167, 6, 20, 0.0f,
          "SHOTAMMO.bin" },
        { AMMO_GRENADE, UINT32_C(0x02000CD8), 2163, 14, 18, 0.0f,
          "GRENADEAMMO.bin" },
        { AMMO_ROCKETS, UINT32_C(0x02000CC0), 2161, 7, 22, -2.0f,
          "ROCKETAMMO.bin" },
        { AMMO_REMOTEMINE, UINT32_C(0x02000CFC), 2234, 14, 14, 1.0f,
          "MINEAMMO.bin" },
        { AMMO_PROXMINE, UINT32_C(0x02000D14), 2235, 14, 14, 1.0f,
          "PROXAMMO.bin" },
        { AMMO_TIMEDMINE, UINT32_C(0x02000D08), 2238, 14, 14, 1.0f,
          "TIMEAMMO.bin" },
        { AMMO_KNIFE, UINT32_C(0x02000CA8), 2166, 6, 24, 0.0f,
          "KNIFEAMMO.bin" },
        { AMMO_GRENADEROUND, UINT32_C(0x02000CB4), 2165, 8, 21, 0.0f,
          "GLAMMO.bin" },
        { AMMO_MAGNUM, UINT32_C(0x02000CE4), 2164, 5, 15, 0.0f,
          "MAGAMMO.bin" },
        { AMMO_GGUN, UINT32_C(0x02000CF0), 2233, 5, 12, 0.0f,
          "GGAMMO.bin" },
        { AMMO_TANK, UINT32_C(0x02000D20), 2464, 7, 22, -1.0f,
          "TANKAMMO.bin" },
    };
    GeOriginalDamMissionHudRenderSnapshot snapshot;
    GeOriginalBottomHudRenderSnapshot bottom;
    GeOriginalGameplayHudRenderSnapshot gameplay;
    char first[] = "Objective A: complete";
    char second[] = "Objective B: failed";
    size_t icon;
    size_t tick;
    size_t language;

    memset(&ge_test_player, 0, sizeof(ge_test_player));
    assert(strcmp((const char *)langGet(getStringID(LDAM, 4)),
                  "Neutralize all alarms\n") == 0);
    assert(strcmp((const char *)langGet(getStringID(LARK, 4)),
                  "Gain entry to laboratory area\n") == 0);
    assert(strcmp((const char *)langGet(getStringID(LGUN, 3)),
                  "   PP7\n") == 0);
    assert(strcmp((const char *)langGet(getStringID(LPROPOBJ, 0)),
                  "Picked up ") == 0);
    assert(strcmp((const char *)langGet(getStringID(LOPTIONS, 4)),
                  "pause\n") == 0);
    assert(strcmp((const char *)langGet(getStringID(LRUN, 4)),
                  "Find plane ignition key\n") == 0);
    for (language = 0U;
            language < sizeof(later_solo_texts)
                / sizeof(later_solo_texts[0]); ++language) {
        const unsigned char *resolved = langGet(
            later_solo_texts[language].text_id);
        assert(resolved != NULL);
        assert((const char *)resolved
            == later_solo_texts[language].bank[
                later_solo_texts[language].index]);
    }
    ge_original_dam_mission_hud_reset();
    assert(ge_original_dam_mission_hud_render_snapshot(&snapshot));
    assert(snapshot.count == 0U && snapshot.timer == -1 && !snapshot.visible);

    hudmsgTopShow(first);
    assert(ge_original_dam_mission_hud_render_snapshot(&snapshot));
    assert(snapshot.count == 1U && snapshot.timer == -1 && !snapshot.visible);
    ge_original_dam_mission_hud_tick();
    assert(ge_original_dam_mission_hud_render_snapshot(&snapshot));
    assert(snapshot.count == 1U && snapshot.timer == 0xf0 && snapshot.visible);
    assert(strcmp(snapshot.messages[0], first) == 0);

    hudmsgTopShow(second);
    ge_original_dam_mission_hud_tick();
    assert(ge_original_dam_mission_hud_render_snapshot(&snapshot));
    assert(snapshot.count == 2U && snapshot.timer == 0x3c);
    for (tick = 0U; tick < 61U; ++tick)
        ge_original_dam_mission_hud_tick();
    assert(ge_original_dam_mission_hud_render_snapshot(&snapshot));
    assert(snapshot.count == 1U && snapshot.timer == 0xf0 && snapshot.visible);
    assert(strcmp(snapshot.messages[0], second) == 0);

    ge_original_bottom_hud_reset();
    assert(ge_original_bottom_hud_render_snapshot(&bottom));
    assert(bottom.count == 0U && bottom.timer == -1 && !bottom.visible);
    ge_original_hud_bottom_show_exact(first);
    assert(ge_original_bottom_hud_render_snapshot(&bottom));
    assert(bottom.count == 1U && bottom.timer == -1 && !bottom.visible);
    ge_original_bottom_hud_tick();
    assert(ge_original_bottom_hud_render_snapshot(&bottom));
    assert(bottom.count == 1U && bottom.timer == 0x78 && bottom.visible);
    assert(bottom.x == 70 && bottom.y == 218);
    assert(strcmp(bottom.message, first) == 0);
    ge_original_hud_bottom_show_exact(second);
    ge_original_bottom_hud_tick();
    assert(ge_original_bottom_hud_render_snapshot(&bottom));
    assert(bottom.count == 2U && bottom.timer == 0x1e);
    for (tick = 0U; tick < 31U; ++tick)
        ge_original_bottom_hud_tick();
    assert(ge_original_bottom_hud_render_snapshot(&bottom));
    assert(bottom.count == 1U && bottom.timer == 0x78 && bottom.visible);
    assert(strcmp(bottom.message, second) == 0);

    ge_test_weapons[0] = 1;
    ge_test_weapons[1] = 1;
    ammo_related[1].IconImage =
        GE_ORIGINAL_AMMO_ICON_9MM_SEGMENTED_ADDRESS;
    ge_test_player.hands[0].weapon_ammo_in_magazine = 7;
    ge_test_player.hands[1].weapon_ammo_in_magazine = 6;
    ge_test_player.ammoheldarr[1] = 93;
    ge_test_player.healthshowtime = 1;
    ge_test_player.apparenthealth = 1.0f;
    ge_test_player.apparentarmour = 0.5f;
    assert(ge_original_gameplay_hud_render_snapshot(&gameplay));
    assert(gameplay.gauges_visible);
    assert(gameplay.ammo_visible && gameplay.left_ammo_visible);
    assert(gameplay.magazine_ammo == 7 && gameplay.reserve_ammo == 93);
    assert(gameplay.left_magazine_ammo == 6
        && gameplay.left_reserve_ammo == 93);
    assert(gameplay.icon_x == 301 && gameplay.icon_y == 220);
    assert(gameplay.left_icon_x == 99 && gameplay.left_icon_y == 220);
    assert(gameplay.icon_image
        == GE_ORIGINAL_AMMO_ICON_9MM_SEGMENTED_ADDRESS);
    assert(gameplay.ammo_type == AMMO_9MM
        && gameplay.left_ammo_type == AMMO_9MM);
    /* The displayed-frame snapshot must republish the same canonical hand
     * storage after gunTickHandState changes it; reserve ammo is independent
     * until the unchanged reload body transfers rounds. */
    ge_test_player.hands[GUNRIGHT].weapon_ammo_in_magazine = 5;
    assert(ge_original_gameplay_hud_render_snapshot(&gameplay));
    assert(gameplay.magazine_ammo == 5 && gameplay.reserve_ammo == 93);

    /* Join every nonzero ammo_related icon address to the exact oddtextures
     * image ID/source/dimensions already converted from the original ROM. */
    assert(ge_original_ammo_icon_asset_count()
        == GE_ORIGINAL_AMMO_ICON_ASSET_COUNT);
    assert(sizeof(expected_icons) / sizeof(expected_icons[0])
        == GE_ORIGINAL_AMMO_ICON_ASSET_COUNT);
    for (icon = 0U; icon < GE_ORIGINAL_AMMO_ICON_ASSET_COUNT; ++icon) {
        const GeOriginalAmmoIconAsset *asset =
            ge_original_ammo_icon_asset_at(icon);
        assert(asset != NULL);
        assert(asset->ammo_type == expected_icons[icon].ammo_type);
        assert(asset->segmented_address == expected_icons[icon].address);
        assert(asset->image_id == expected_icons[icon].image_id);
        assert(asset->width == expected_icons[icon].width);
        assert(asset->height == expected_icons[icon].height);
        assert(fabsf(asset->y_offset - expected_icons[icon].y_offset)
            < 0.0001f);
        assert(strcmp(asset->source, expected_icons[icon].source) == 0);
        assert(ge_original_ammo_icon_asset_for_ammo_type(asset->ammo_type)
            == asset);
        assert(ge_original_ammo_icon_asset_for_segmented_address(
            asset->segmented_address) == asset);
        ammo_related[asset->ammo_type].IconImage = asset->segmented_address;
        ammo_related[asset->ammo_type].IconYOffset = asset->y_offset;
        ge_test_weapons[GUNLEFT] = ITEM_UNARMED;
        ge_test_weapons[GUNRIGHT] = asset->ammo_type;
        ge_test_player.hands[GUNRIGHT].weapon_ammo_in_magazine = 3;
        ge_test_player.ammoheldarr[asset->ammo_type] = 9;
        assert(ge_original_gameplay_hud_render_snapshot(&gameplay));
        assert(gameplay.icon_image == asset->segmented_address);
        assert(gameplay.icon_y == (int16_t)(220 + asset->y_offset));
        assert(gameplay.magazine_x == 301 - asset->width / 2 - 4);
        assert(gameplay.reserve_x == 301 + (asset->width + 1) / 2 + 3);
    }
    assert(ge_original_ammo_icon_asset_at(
        GE_ORIGINAL_AMMO_ICON_ASSET_COUNT) == NULL);
    assert(ge_original_ammo_icon_asset_for_ammo_type(AMMO_9MM_2) == NULL);
    assert(ge_original_ammo_icon_asset_for_segmented_address(0U) == NULL);

    /* The snapshot observes the canonical gunammooff reason bitfield. Any
     * active option/control/damage reason suppresses both counters without
     * erasing the other reasons maintained by gunSetGunAmmoVisible. */
    ge_test_player.gunammooff =
        GUNAMMOREASON_OPTION | GUNAMMOREASON_DAMAGE;
    assert(ge_original_gameplay_hud_render_snapshot(&gameplay));
    assert(gameplay.ammo_suppression_reasons
        == (GUNAMMOREASON_OPTION | GUNAMMOREASON_DAMAGE));
    assert(!gameplay.ammo_visible && !gameplay.left_ammo_visible);

    ge_test_player.gunammooff = 0;
    ge_test_weapons[GUNLEFT] = ITEM_UNARMED;
    ge_test_weapons[GUNRIGHT] = AMMO_RIFLE;
    ammo_related[AMMO_RIFLE].IconImage = UINT32_C(0x02000C90);
    ammo_related[AMMO_RIFLE].IconYOffset = -2.0f;
    ge_test_player.hands[GUNRIGHT].weapon_ammo_in_magazine = 30;
    ge_test_player.ammoheldarr[AMMO_RIFLE] = 200;
    assert(ge_original_gameplay_hud_render_snapshot(&gameplay));
    assert(gameplay.ammo_type == AMMO_RIFLE);
    assert(gameplay.icon_image == UINT32_C(0x02000C90));
    assert(gameplay.icon_y == 218);
    assert(gameplay.magazine_ammo == 30 && gameplay.reserve_ammo == 200);

    puts("Dam HUD: exact queues, gauge/ammo state and all 13 ROM-backed "
         "ammo icon mappings retained");
    return 0;
}
