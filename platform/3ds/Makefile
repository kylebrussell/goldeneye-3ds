#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM or use ../../scripts/build_3ds.sh")
endif

TOPDIR ?= $(CURDIR)
include $(DEVKITARM)/3ds_rules

TARGET      := goldeneye-3ds
BUILD       := build
SOURCES     := source ../../port/src
DATA        := data
GAME_SOURCES := ../../src/game ../../src ../../src/libultra/audio ../../src/libultrare/audio ../../src/libultra/gu ../../assets/obseg/text ../../assets/font
GAME_CFILES := frametiming.c quaternion.c matrixmath.c matrixmath_misc.c math_floor.c math_ceil.c math_unk_05A9E0.c perspective.c mtxutil.c lookatref.c align.c normalize.c rotate.c bondview2.c bondview_r.c prop.c loadobjectmodel.c stan.c stanintersection.c bg.c chrai.c deb_loadallmodels.c joy.c boss.c lv.c blood_animation.c blood_decrypt.c mainbus.c auxbus.c save.c cspgetstate.c cseq.c cspsetseq.c cspsetvol.c cspplay.c cspstop.c event.c copy.c sl.c fontZurichBold.c fontBankGothic.c
GAME_CFILES += cents2ratio.c csplayer.c filter.c heapalloc.c heapinit.c load.c resample.c seqplayer.c seqpsetbank.c synaddplayer.c synallocfx.c synallocvoice.c syndelete.c synfreevoice.c synsetfxmix.c synsetpan.c synsetpitch.c synsetpriority.c synsetvol.c synstartvoice.c synstartvoiceparam.c synstopvoice.c synthesizer.c drvrNew.c env.c reverb.c
GAME_CFILES += sins.c title3.c
SETUP_SOURCES := ../../assets/obseg/setup
SETUP_CFILES := UsetupdamZ.c
SOLO_LANGUAGE_NAMES := LdamE LarkE LrunE LsevxE LsevE LsiloE LdestE \
                       LsevxbE LsevbE LstatE LarchE LpeteE LdepoE LtraE \
                       LjunE LarecE LcaveE LcradE LaztE LcrypE LlenE
SOLO_LANGUAGE_CFILES := $(addprefix $(TOPDIR)/../../assets/obseg/text/,\
                           $(addsuffix .c,$(SOLO_LANGUAGE_NAMES)))
INCLUDES    := include ../../port/include ../../src ../..

APP_TITLE       := GoldenEye 007 3DS
APP_DESCRIPTION := Native port bring-up build
APP_AUTHOR      := GoldenEye 007 decomp contributors

ARCH        := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft
CFLAGS      := -g -Wall -Wextra -Werror -O2 -std=gnu11 -mword-relocations \
               -ffunction-sections $(ARCH)
CFLAGS      += $(INCLUDE) -D__3DS__ -DGE_PLATFORM_3DS
CFLAGS      += -D_LANGUAGE_C -iquote$(TOPDIR)/../.. \
               -I$(TOPDIR)/../../include/PR -idirafter $(TOPDIR)/../../include
# bondtypes.h models the original C ABI with anonymous inherited records.
# Every producer and consumer must see the same promoted parent fields; a
# mixed build shifts ObjectRecord by four bytes and turns authored obj/pad
# words into a fake prop-definition header.
CFLAGS      += -DGE_PORT_MS_INHERITS -fms-extensions
# These platform renderer adapters dominate the live frame on New 3DS.  They
# retain explicit scalar floating-point order and are covered by byte-exact
# publication tests, so enable GCC's higher non-fast-math optimization level
# for them without changing any canonical gameplay translation unit.
main.o ge_3ds_material.o ge_draw_batch_visibility.o \
ge_dam_dynamic_scene.o ge_dam_visibility_runtime.o \
ge_original_model_scene.o ge_original_first_person_scene.o \
ge_original_stage_guard_runtime.o ge_original_stage_active_props.o \
ge_original_bond_live.o ge_original_bond_camera_live.o \
ge_original_gun_live.o: CFLAGS += -O3
title3.o: CFLAGS += -DGE_PORT_GUNBARREL_HOLE_SLICE
ASFLAGS     := -g $(ARCH)
LDFLAGS     := -specs=3dsx.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)
LDFLAGS     += -Wl,-u,ge_dam_setup_world_mission_monitor_render_tick
LDFLAGS     += -Wl,-u,ge_original_global_ai_find \
               -Wl,-u,ge_original_global_ai_count
LDFLAGS     += -Wl,-u,ge_original_stage_autogun_lifecycle_construct \
               -Wl,-u,ge_original_stage_autogun_lifecycle_is_live \
               -Wl,-u,ge_original_stage_autogun_lifecycle_tick_exact \
               -Wl,-u,ge_original_stage_autogun_lifecycle_advance_beam_exact \
               -Wl,-u,ge_original_stage_autogun_lifecycle_beam_snapshot \
               -Wl,-u,ge_original_stage_autogun_lifecycle_runtime_snapshot \
               -Wl,-u,ge_original_stage_autogun_lifecycle_cleanup_exact \
               -Wl,-u,ge_original_stage_autogun_lifecycle_cleanup_owned_exact \
               -Wl,-u,ge_original_stage_autogun_lifecycle_cleanup_pitem_exact \
               -Wl,-u,ge_3ds_original_autogun_beams_build_draw_list \
               -Wl,-u,gunAdvanceBeamTimer
LDFLAGS     += -Wl,-u,ge_original_watch_mission_abort_reset \
               -Wl,-u,ge_original_watch_mission_abort_navigation_tick \
               -Wl,-u,ge_original_watch_mission_abort_presentation_tick \
               -Wl,-u,ge_original_watch_mission_abort_snapshot
LDFLAGS     += -Wl,-u,ge_original_gunbarrel_reset \
               -Wl,-u,ge_original_gunbarrel_tick \
               -Wl,-u,ge_original_gunbarrel_assets \
               -Wl,-u,ge_original_gunbarrel_build_hole \
               -Wl,-u,ge_original_gunbarrel_contract_sha256
LDFLAGS     += -Wl,-u,ge_original_frontend_cast_reset \
               -Wl,-u,ge_original_frontend_cast_begin_current \
               -Wl,-u,ge_original_frontend_cast_tick \
               -Wl,-u,ge_original_frontend_cast_apply_pose \
               -Wl,-u,ge_original_frontend_cast_snapshot \
               -Wl,-u,ge_original_frontend_cast_contract_sha256
LDFLAGS     += -Wl,-u,ge_original_frontend_cast_model_create \
               -Wl,-u,ge_original_frontend_cast_model_destroy \
               -Wl,-u,ge_original_frontend_cast_model_begin_selection \
               -Wl,-u,ge_original_frontend_cast_model_tick \
               -Wl,-u,ge_original_frontend_cast_model_status_name
LDFLAGS     += -Wl,-u,ge_original_music_bank_open \
               -Wl,-u,ge_original_music_bank_close \
               -Wl,-u,ge_original_music_bank_native \
               -Wl,-u,ge_original_music_bank_stats \
               -Wl,-u,alCSeqNew \
               -Wl,-u,alCSeqNextEvent
LDFLAGS     += -Wl,-u,ge_original_music_port_bind_instrument_bank \
               -Wl,-u,ge_original_music_port_unbind_instrument_bank
LDFLAGS     += -Wl,-u,ge_original_music_runtime_open \
               -Wl,-u,ge_original_music_runtime_open_asset_pack \
               -Wl,-u,ge_original_music_runtime_close \
               -Wl,-u,ge_original_music_runtime_output \
               -Wl,-u,ge_original_music_runtime_render \
               -Wl,-u,ge_original_music_runtime_tick_60hz \
               -Wl,-u,ge_original_music_runtime_stats \
               -Wl,-u,alAudioFrame
LDFLAGS     += -Wl,-u,ge_original_stage_active_props_compose \
               -Wl,-u,ge_original_stage_active_props_tick_exact \
               -Wl,-u,ge_original_stage_active_props_pre_tick_exact \
               -Wl,-u,ge_original_stage_active_props_close
LDFLAGS     += -Wl,-u,ge_original_stage_mission_runtime_reset_globals \
               -Wl,-u,ge_original_stage_mission_runtime_begin \
               -Wl,-u,ge_original_stage_mission_runtime_observe_tick
LDFLAGS     += -Wl,-u,ge_original_stage_objectives_build \
               -Wl,-u,ge_original_stage_objectives_find_tag \
               -Wl,-u,ge_original_stage_objectives_close
LDFLAGS     += -Wl,-u,ge_original_stage_objective_runtime_begin \
               -Wl,-u,ge_original_stage_objective_runtime_close \
               -Wl,-u,ge_original_stage_objective_runtime_cleanup \
               -Wl,-u,ge_original_stage_objective_runtime_evaluate \
               -Wl,-u,ge_original_stage_objective_runtime_collect_status_changes \
               -Wl,-u,ge_original_stage_objective_runtime_check_room_entered \
               -Wl,-u,ge_original_stage_objective_runtime_check_deposit \
               -Wl,-u,ge_original_stage_objective_runtime_take_picture \
               -Wl,-u,ge_original_stage_objective_runtime_status_name \
               -Wl,-u,ge_original_stage_objective_runtime_blocker_name \
               -Wl,-u,ge_original_stage_objective_prop_in_inventory_exact \
               -Wl,-u,ge_original_stage_objective_stage_flag_set_exact \
               -Wl,-u,ge_original_stage_objective_key_analyzer_complete_exact \
               -Wl,-u,ge_original_stage_objective_photograph_binding_ready_exact \
               -Wl,-u,ge_original_stage_objective_photograph_bounds_inside_view_exact
LDFLAGS     += -Wl,-u,ge_original_stage_objective_live_bind \
               -Wl,-u,ge_original_stage_objective_live_check_room_entered \
               -Wl,-u,ge_original_stage_objective_live_check_deposit \
               -Wl,-u,ge_original_stage_objective_live_take_picture \
               -Wl,-u,ge_original_stage_objective_live_evaluate \
               -Wl,-u,ge_original_stage_objective_live_collect_status_changes \
               -Wl,-u,ge_original_stage_objective_live_cleanup \
               -Wl,-u,ge_original_stage_objective_live_count \
               -Wl,-u,ge_original_stage_objective_live_difficulty \
               -Wl,-u,ge_original_stage_objective_live_all_complete \
               -Wl,-u,ge_original_stage_objective_live_status \
               -Wl,-u,objectiveTakePictureHandler \
               -Wl,-u,cleanupObjectives
LDFLAGS     += -Wl,-u,ge_original_dam_objective_status_present \
               -Wl,-u,ge_original_dam_objective_status_last \
               -Wl,-u,display_objective_status_text_on_status_change \
               -Wl,-u,objectiveIsAllComplete
LDFLAGS     += -Wl,-u,ge_original_stage_security_construct \
               -Wl,-u,ge_original_stage_security_required_capabilities \
               -Wl,-u,ge_original_stage_security_status_name
LDFLAGS     += -Wl,-u,ge_original_stage_safe_runtime_bind \
               -Wl,-u,ge_original_stage_safe_runtime_register_relation \
               -Wl,-u,ge_original_stage_safe_runtime_can_pickup \
               -Wl,-u,objCanPickupFromSafe
LDFLAGS     += -Wl,-u,ge_original_stage_alarm_interact_exact
LDFLAGS     += -Wl,-u,ge_original_stage_supply_construct_exact \
               -Wl,-u,ge_original_stage_supply_status_name
# Keep the stage-generic PitemZ callback boundary link-visible before main.c
# activates it. The provider pulls the canonical PitemZ table/count with it.
LDFLAGS     += -Wl,-u,ge_original_pitem_model_provider_create \
               -Wl,-u,ge_original_pitem_model_provider_destroy \
               -Wl,-u,ge_original_pitem_model_load \
               -Wl,-u,ge_original_pitem_model_available \
               -Wl,-u,ge_original_pitem_model_resolve_instance \
               -Wl,-u,ge_original_pitem_model_scene_part_for_node
LDFLAGS     += -Wl,-u,ge_original_stage_interactive_materialize \
               -Wl,-u,ge_original_stage_interactive_entry \
               -Wl,-u,ge_original_stage_interactive_expected_item_count \
               -Wl,-u,ge_original_stage_interactive_live_item_count \
               -Wl,-u,ge_original_stage_interactive_active_item \
               -Wl,-u,ge_original_stage_item_construct_standard_exact \
               -Wl,-u,ge_original_stage_item_construct_embedded_exact \
               -Wl,-u,ge_original_stage_interactive_close
LDFLAGS     += -Wl,-u,ge_original_stage_monitor_construct \
               -Wl,-u,ge_original_stage_monitor_tick \
               -Wl,-u,ge_original_stage_monitor_controller_initialize \
               -Wl,-u,ge_original_stage_monitor_render_screen_tick \
               -Wl,-u,ge_original_stage_monitor_surface_apply \
               -Wl,-u,ge_original_stage_monitor_surface_apply_part
LDFLAGS     += -Wl,-u,ge_original_ammo_icon_asset_count \
               -Wl,-u,ge_original_ammo_icon_asset_at \
               -Wl,-u,ge_original_ammo_icon_asset_for_ammo_type \
               -Wl,-u,ge_original_ammo_icon_asset_for_segmented_address
LDFLAGS     += -Wl,-u,ge_original_door_release \
               -Wl,-u,ge_original_door_capacity
LDFLAGS     += -Wl,-u,ge_original_character_model_provider_create \
               -Wl,-u,ge_original_character_model_provider_destroy \
               -Wl,-u,ge_original_character_model_dependency_count \
               -Wl,-u,ge_original_character_model_dependency_metadata \
               -Wl,-u,ge_original_character_model_load \
               -Wl,-u,ge_original_character_model_available \
               -Wl,-u,ge_original_character_model_resolve_instance \
               -Wl,-u,ge_original_character_model_resolve_pair \
               -Wl,-u,ge_original_character_model_instance_set_root \
               -Wl,-u,ge_original_character_model_prepare_instance_relations \
               -Wl,-u,ge_original_character_model_scene_part_count \
               -Wl,-u,ge_original_character_model_scene_part \
               -Wl,-u,ge_original_character_model_instance_scene_part_count \
               -Wl,-u,ge_original_character_model_instance_scene_part \
               -Wl,-u,ge_original_character_model_instance_shadow_count \
               -Wl,-u,ge_original_character_model_instance_shadow \
               -Wl,-u,ge_original_stage_guard_runtime_create \
               -Wl,-u,ge_original_stage_guard_runtime_destroy \
               -Wl,-u,ge_original_stage_guard_runtime_materializer \
               -Wl,-u,ge_original_stage_guard_runtime_actor \
               -Wl,-u,ge_original_stage_guard_runtime_bind_authored_weapons \
               -Wl,-u,ge_original_stage_guard_runtime_bind_authored_hats \
               -Wl,-u,ge_original_stage_guard_runtime_active_prop \
               -Wl,-u,ge_original_stage_guard_actor_equip_weapon \
               -Wl,-u,ge_original_stage_guard_actor_apply_hat \
               -Wl,-u,ge_original_stage_guard_runtime_update_lighting \
               -Wl,-u,ge_original_stage_guard_runtime_shadow \
               -Wl,-u,ge_original_stage_guard_runtime_update_matrices \
               -Wl,-u,ge_original_stage_guard_runtime_build_scene \
               -Wl,-u,ge_original_stage_guard_runtime_append_scene
LIBS        := -lcitro3d -lctru -lm
LIBDIRS     := $(CTRULIB)

GE_DAM_FULL_PROPS_AUDIT ?= 0
GE_DAM_FULL_PROPS_LIVE ?= 1
GE_DAM_STAGE_RUNTIME_LIVE ?= 1
ifeq ($(GE_DAM_STAGE_RUNTIME_LIVE),1)
CFLAGS += -DGE_DAM_STAGE_RUNTIME_LIVE=1
endif
ifeq ($(GE_DAM_FULL_PROPS_AUDIT),1)
DAM_PROPS_MODE := audit
DAM_GUARD_SCHEDULER_SUPPORT_C := \
    ge_original_dam_guard_chr_scheduler_full_props_support_slice.c
LDFLAGS += -Wl,-u,ge_original_dam_guard_props_tick_exact
else ifeq ($(GE_DAM_FULL_PROPS_LIVE),1)
DAM_PROPS_MODE := live
DAM_GUARD_SCHEDULER_SUPPORT_C := \
    ge_original_dam_guard_chr_scheduler_full_props_support_slice.c
CFLAGS += -DGE_DAM_FULL_PROPS_LIVE=1
LDFLAGS += -Wl,-u,ge_original_dam_guard_props_tick_exact
else
DAM_PROPS_MODE := default
DAM_GUARD_SCHEDULER_SUPPORT_C := \
    ge_original_dam_guard_chr_scheduler_support_slice.c
endif

ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT  := $(CURDIR)/$(TARGET)
export TOPDIR  := $(CURDIR)
export VPATH   := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
                  $(foreach dir,$(GAME_SOURCES),$(CURDIR)/$(dir)) \
                  $(foreach dir,$(SETUP_SOURCES),$(CURDIR)/$(dir)) \
                  $(foreach dir,$(DATA),$(CURDIR)/$(dir))
export DEPSDIR := $(CURDIR)/$(BUILD)

CFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c))) \
            ge_original_stage_environment_tables.c \
            ge_original_bg_visibility_slice.c \
            ge_original_bond_input_live_state_slice.c \
            ge_original_bond_input_gun_data_slice.c \
            ge_original_player_thrown_object_slice.c \
            ge_original_bond_move_runtime_slice.c \
            ge_original_bond_move_collision_slice.c \
            ge_original_bond_move_non_tank_slice.c \
            ge_original_bond_move_explosion_slice.c \
            ge_original_move_model_tables.c \
            ge_original_animation_table_abi.c \
            ge_original_bond_move_state_slice.c \
            ge_original_door_collision_slice.c \
            ge_original_door_character_collision_slice.c \
            ge_original_player_gait_model_slice.c \
            ge_original_guard_bullet_hit_slice.c \
            ge_original_object_bullet_hit_slice.c \
            ge_original_guard_damage_slice.c \
            ge_original_guard_damage_support_slice.c \
            ge_original_guard_animation_init_slice.c \
            ge_original_dam_guard_ai_tick_slice.c \
            ge_original_dam_guard_ai_support_slice.c \
            ge_original_dam_guard_chr_scheduler_slice.c \
            ge_original_stage_pickup_slice.c \
            ge_original_stage_guard_actor_slice.c \
            ge_original_global_ai.c \
            $(DAM_GUARD_SCHEDULER_SUPPORT_C) \
            ge_original_dam_projectile_explosion_slice.c \
            ge_original_gun_update_and_fire_slice.c \
            ge_original_gun_pose_helpers_slice.c \
            ge_original_gun_casing_slice.c \
            ge_original_gun_secondary_sinks.c \
            ge_original_gun_secondary_dependencies.c \
            ge_original_dam_mission_object_state_slice.c \
            ge_original_dam_mission_hud_slice.c \
            ge_original_dam_monitor_animation_slice.c \
            $(addsuffix .c,$(SOLO_LANGUAGE_NAMES)) LgunE.c LmiscE.c \
            LoptionsE.c LpropobjE.c LtitleE.c \
            $(GAME_CFILES) $(SETUP_CFILES)
SFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
PICAFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.v.pica)))
BINFILES := $(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

export LD := $(CC)
export OFILES_SOURCES := $(CFILES:.c=.o) $(SFILES:.s=.o)
export OFILES_BIN := $(addsuffix .o,$(BINFILES)) $(PICAFILES:.v.pica=.shbin.o)
export OFILES := $(OFILES_BIN) $(OFILES_SOURCES)
export HFILES := $(PICAFILES:.v.pica=_shbin.h) $(addsuffix .h,$(subst .,_,$(BINFILES)))
export INCLUDE := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
                  $(foreach dir,$(LIBDIRS),-I$(dir)/include) \
                  -I$(CURDIR)/$(BUILD)
export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)
export _3DSXDEPS := $(OUTPUT).smdh
export _3DSXFLAGS += --smdh=$(OUTPUT).smdh

.PHONY: all clean

all: $(BUILD)
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

$(BUILD):
	@mkdir -p $@

clean:
	@echo clean ...
	@rm -fr $(BUILD) $(TARGET).3dsx $(TARGET).smdh $(TARGET).elf $(TARGET).map

else

$(OUTPUT).3dsx: $(OUTPUT).elf $(_3DSXDEPS)
$(OFILES_SOURCES): $(HFILES)

# Command-line Make variables change the selected scheduler object and the
# live-only code in main/runtime, but Make does not otherwise consider those
# values part of an object's identity. Keep one content-addressed mode stamp
# so default -> audit -> live transitions rebuild and relink reproducibly.
DAM_PROPS_MODE_STAMP := .ge_dam_full_props_mode
.PHONY: FORCE_DAM_PROPS_MODE
FORCE_DAM_PROPS_MODE:
$(DAM_PROPS_MODE_STAMP): FORCE_DAM_PROPS_MODE
	@if ! test -f $@ || ! grep -qx '$(DAM_PROPS_MODE)' $@; then \
		printf '%s\n' '$(DAM_PROPS_MODE)' > $@; \
	fi
main.o ge_original_dam_guard_runtime.o: $(DAM_PROPS_MODE_STAMP)
$(OUTPUT).elf: $(DAM_PROPS_MODE_STAMP)

joy.o: CFLAGS += -Wno-parentheses -Wno-empty-body -Wno-unused-variable -Wno-unused-parameter
boss.o ge_original_boss.o: CFLAGS += -DGE_PORT_BOSS_STAGE_SLICE
boss.o: CFLAGS += -Wno-unused-variable
lv.o: CFLAGS += -DGE_PORT_LV_STAGE_TICK_SLICE -DVERSION_US
mainbus.o auxbus.o save.o: CFLAGS += -Wno-builtin-declaration-mismatch
cseq.o event.o copy.o sl.o: CFLAGS += -Wno-builtin-declaration-mismatch
cseq.o: CFLAGS += -Wno-maybe-uninitialized -DGE_PORT_CSEQ_BIG_ENDIAN
csplayer.o seqplayer.o filter.o heapalloc.o heapinit.o load.o resample.o \
synaddplayer.o synallocfx.o synallocvoice.o syndelete.o synfreevoice.o \
synsetfxmix.o synsetpan.o synsetpitch.o synsetpriority.o synsetvol.o \
synstartvoice.o synstartvoiceparam.o synstopvoice.o synthesizer.o \
drvrNew.o env.o reverb.o: CFLAGS += -Wno-builtin-declaration-mismatch \
    -Wno-unused-variable -Wno-unused-parameter -Wno-implicit-fallthrough \
    -I$(TOPDIR)/../../src/libultra -DGE_PORT_LIBAUDIO_NATIVE
seqplayer.o: CFLAGS += -DGE_PORT_LIBAUDIO_NATIVE
load.o: CFLAGS += -Wno-sign-compare -Wno-return-type
synsetfxmix.o synstartvoiceparam.o: CFLAGS += -Wno-type-limits
synthesizer.o: CFLAGS += -Wno-unused-but-set-variable -DNDEBUG
ge_original_decompress_source.o: CFLAGS += -Wno-incompatible-pointer-types \
    -Wno-empty-body -Wno-int-conversion
ge_original_zlib_source.o: CFLAGS += -Wno-sign-compare -Wno-unused-variable \
    -Wno-builtin-declaration-mismatch -Wno-incompatible-pointer-types
ge_original_zlib_source.o: CFLAGS += -Wno-maybe-uninitialized
save.o: CFLAGS += -DNDEBUG
matrixmath.o: CFLAGS += -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE \
    -Wno-error -Wno-unused-parameter -Wno-unused-variable
matrixmath_misc.o: CFLAGS += -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE \
    -DGE_PORT_SETUP_DATA -DGE_PORT_VEC3_LERP_SLICE \
    -Wno-error -Wno-unused-parameter -Wno-unused-variable
ge_original_math_angles.o: CFLAGS += -DGE_PORT_USE_ORIGINAL_TYPES \
    -fno-builtin-atan2f -fno-builtin-acosf -fno-builtin-asinf \
    -Wno-incompatible-pointer-types -Wno-error
ge_original_music_source.o: CFLAGS += \
    -DGE_PORT_USE_ORIGINAL_TYPES -DGE_PORT_SETUP_DATA -DAIPARSE \
    -DVERSION_US -DBUGFIX_R0 -DGE_PORT_MS_INHERITS -fms-extensions \
    -I$(TOPDIR)/../../src/game -Wno-unused-variable -Wno-unused-parameter \
    -Wno-incompatible-pointer-types -Wno-int-conversion \
    -Wno-pointer-to-int-cast -Wno-unused-value -Wno-error
bondview2.o ge_original_bond_camera.o: CFLAGS += -DGE_PORT_USE_ORIGINAL_TYPES \
    -DGE_PORT_BOND_CAMERA_SLICE -DGE_PORT_BOND_PLAYER_SPAWN_SLICE \
    -DAIPARSE -Wno-pointer-to-int-cast
ge_original_bond_movement_source.o ge_original_bond_movement.o: CFLAGS += \
    -DGE_PORT_USE_ORIGINAL_TYPES -DGE_PORT_BOND_CAMERA_SLICE \
    -DGE_PORT_BOND_MOVEMENT_SLICE -DGE_PORT_SETUP_DATA -DAIPARSE \
    -Wno-pointer-to-int-cast \
    -Wno-unused-variable -Wno-unused-parameter -Wno-comment -Wno-error
ge_original_bond_move_runtime_slice.o: CFLAGS += \
    -DGE_PORT_USE_ORIGINAL_TYPES -DGE_PORT_SETUP_DATA -DAIPARSE \
    -DVERSION_US -DBUGFIX_R0 -DGE_PORT_EXACT_MOVEBOND_FRONTIER \
    -DGE_PORT_MS_INHERITS \
    -fms-extensions -I$(TOPDIR)/../../src/game \
    -Wno-unused-variable -Wno-unused-parameter -Wno-incompatible-pointer-types \
    -Wno-pointer-to-int-cast -Wno-int-conversion -Wno-error
ge_original_bond_move_collision_slice.o: CFLAGS += \
    -DGE_PORT_USE_ORIGINAL_TYPES -DGE_PORT_SETUP_DATA -DAIPARSE \
    -DVERSION_US -DGE_PORT_MS_INHERITS -fms-extensions \
    -I$(TOPDIR)/../../src/game -Wno-unused-variable -Wno-unused-parameter \
    -Wno-incompatible-pointer-types -Wno-pointer-to-int-cast -Wno-error
ge_original_bond_move_non_tank_slice.o: CFLAGS += \
    -DGE_PORT_USE_ORIGINAL_TYPES -DGE_PORT_SETUP_DATA -DAIPARSE \
    -DVERSION_US -DBUGFIX_R0 -DGE_PORT_MS_INHERITS \
    -DGE_PORT_BOND_MOVE_ANIMATION_OFFSETS -fms-extensions \
    -I$(TOPDIR)/../../src/game \
    -Wno-unused-variable -Wno-unused-parameter \
    -Wno-incompatible-pointer-types -Wno-int-conversion \
    -Wno-pointer-to-int-cast -Wno-unused-value -Wno-error
ge_original_bond_move_explosion_slice.o: CFLAGS += \
    -DGE_PORT_USE_ORIGINAL_TYPES -DGE_PORT_SETUP_DATA -DAIPARSE \
    -DVERSION_US -DBUGFIX_R0 -DGE_PORT_MS_INHERITS -fms-extensions \
    -I$(TOPDIR)/../../src/game \
    -Wno-unused-variable -Wno-unused-parameter \
    -Wno-incompatible-pointer-types -Wno-int-conversion \
    -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast -Wno-error
ge_original_effect_buffers.o: CFLAGS += \
    -DGE_PORT_USE_ORIGINAL_TYPES -DGE_PORT_SETUP_DATA -DAIPARSE \
    -DVERSION_US -DBUGFIX_R0 -DGE_PORT_MS_INHERITS -fms-extensions \
    -I$(TOPDIR)/../../src/game \
    -Wno-unused-variable -Wno-unused-parameter \
    -Wno-incompatible-pointer-types -Wno-int-conversion \
    -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast -Wno-error
ge_original_move_model_tables.o: CFLAGS += \
    -DGE_PORT_USE_ORIGINAL_TYPES -DGE_PORT_SETUP_DATA -DAIPARSE \
    -DVERSION_US -DGE_PORT_MS_INHERITS -fms-extensions \
    -I$(TOPDIR)/../../src/game -Wno-error
ge_original_animation_table_abi.o: CFLAGS += -fno-data-sections -fno-toplevel-reorder
ge_original_bond_move_state_slice.o ge_original_bond_move_state.o: CFLAGS += \
    -DGE_PORT_USE_ORIGINAL_TYPES -DGE_PORT_SETUP_DATA -DAIPARSE \
    -DVERSION_US -Wno-unused-variable -Wno-unused-parameter \
    -Wno-incompatible-pointer-types -Wno-pointer-to-int-cast \
    -Wno-missing-braces -Wno-comment -Wno-error
ge_original_bond_input_live_state_slice.o: CFLAGS += \
    -DGE_PORT_USE_ORIGINAL_TYPES -DGE_PORT_SETUP_DATA -DAIPARSE \
    -DVERSION_US -DBUGFIX_R0 -Wno-incompatible-pointer-types \
    -Wno-int-conversion -Wno-error -fno-builtin-atan2f
ge_original_bond_input_body_source.o: CFLAGS += \
    -DGE_PORT_USE_ORIGINAL_TYPES -DGE_PORT_SETUP_DATA -DAIPARSE \
    -DGE_PORT_BOND_INPUT_FULL_SLICE -DVERSION_US -DBUGFIX_R0 \
    -I$(TOPDIR)/../../src/game \
    -Wno-logical-not-parentheses -Wno-empty-body -Wno-unused-variable \
    -Wno-unused-parameter -Wno-incompatible-pointer-types \
    -Wno-int-conversion -Wno-pointer-to-int-cast -Wno-error
ge_original_bond_speed_helpers_source.o: CFLAGS += \
    -DGE_PORT_USE_ORIGINAL_TYPES -DGE_PORT_SETUP_DATA -DAIPARSE \
    -DGE_PORT_BOND_INPUT_SPEED_HELPERS_SLICE -DVERSION_US \
    -I$(TOPDIR)/../../src/game -Wno-unused-parameter \
    -Wno-unused-variable -Wno-empty-body -Wno-incompatible-pointer-types \
    -Wno-int-conversion -Wno-pointer-to-int-cast -Wno-error
ge_original_bond_watch_helpers_source.o: CFLAGS += \
    -DGE_PORT_USE_ORIGINAL_TYPES -DGE_PORT_SETUP_DATA -DAIPARSE \
    -DGE_PORT_BOND_INPUT_WATCH_HELPERS_SLICE -DVERSION_US \
    -I$(TOPDIR)/../../src/game -Wno-unused-parameter \
    -Wno-unused-variable -Wno-empty-body -Wno-incompatible-pointer-types \
    -Wno-int-conversion -Wno-pointer-to-int-cast -Wno-error
ge_original_bond_state_helpers_source.o: CFLAGS += \
    -DGE_PORT_USE_ORIGINAL_TYPES -DGE_PORT_SETUP_DATA -DAIPARSE \
    -DGE_PORT_BOND_INPUT_STATE_HELPERS_SLICE -DVERSION_US \
    -I$(TOPDIR)/../../src/game -Wno-unused-parameter \
    -Wno-unused-variable -Wno-empty-body -Wno-incompatible-pointer-types \
    -Wno-int-conversion -Wno-pointer-to-int-cast -Wno-error
ge_original_bond_live.o: CFLAGS += \
    -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA \
    -DVERSION_US -DBUGFIX_R0 -Wno-unused-parameter -Wno-error
ge_original_gameplay_services.o ge_original_embedment_pool.o: CFLAGS += \
    -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA \
    -DVERSION_US -DBUGFIX_R0 -I$(TOPDIR)/../../src/game \
    -Wno-unused-parameter -Wno-incompatible-pointer-types -Wno-error
ge_original_dam_mission_exit_services.o: CFLAGS += \
    -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA \
    -DVERSION_US -DBUGFIX_R0 -DPLAYERFLAG=int \
    -I$(TOPDIR)/../../src/game -Wno-comment -Wno-pragma-pack \
    -Wno-unused-parameter -Wno-incompatible-pointer-types \
    -Wno-int-conversion -Wno-pointer-to-int-cast \
    -Wno-int-to-pointer-cast -Wno-error
ge_original_player_body.o: CFLAGS += \
    -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA \
    -DVERSION_US -DBUGFIX_R0 -DPLAYERFLAG=int \
    -I$(TOPDIR)/../../src/game -Wno-comment -Wno-pragma-pack \
    -Wno-unused-parameter -Wno-incompatible-pointer-types \
    -Wno-int-conversion -Wno-pointer-to-int-cast \
    -Wno-int-to-pointer-cast -Wno-error
blood_animation.o blood_decrypt.o: CFLAGS += \
    -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA \
    -DVERSION_US -DBUGFIX_R0 -DPLAYERFLAG=int \
    -DGE_PORT_BLOOD_DECODE_SLICE \
    -I$(TOPDIR)/../../src/game -Wno-comment -Wno-pointer-sign \
    -Wno-incompatible-pointer-types -Wno-int-conversion \
    -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast -Wno-error
ge_original_mission_result.o: CFLAGS += \
    -DGE_PORT_USE_ORIGINAL_TYPES -DVERSION_US -Wno-error
ge_3ds_save_provider.o: CFLAGS += \
    -DGE_PORT_USE_ORIGINAL_TYPES -DVERSION_US -Wno-pointer-sign \
    -Wno-missing-braces -Wno-error
ge_original_guard_animation_table.o: CFLAGS += \
    -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA \
    -DVERSION_US -DBUGFIX_R0 -I$(TOPDIR)/../../src/game \
    -Wno-unused-parameter -Wno-incompatible-pointer-types -Wno-error
ge_original_dam_guard_death_link.o: CFLAGS += \
    -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA \
    -DVERSION_US -DBUGFIX_R0 -I$(TOPDIR)/../../src/game \
    -Wno-unused-parameter -Wno-incompatible-pointer-types -Wno-error
ge_original_gun_frame_arena.o: CFLAGS += \
    -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA \
    -DVERSION_US -DBUGFIX_R0 -I$(TOPDIR)/../../src/game \
    -Wno-unused-parameter -Wno-incompatible-pointer-types -Wno-error
ge_original_gun_live.o: CFLAGS += \
    -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA \
    -DVERSION_US -DBUGFIX_R0 -I$(TOPDIR)/../../src/game \
    -Wno-unused-parameter -Wno-incompatible-pointer-types -Wno-error
ge_original_first_person_scene.o: CFLAGS += \
    -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA \
    -DVERSION_US -DBUGFIX_R0 -DGE_PORT_MS_INHERITS -fms-extensions \
    -I$(TOPDIR)/../../src/game \
    -Wno-unused-parameter -Wno-incompatible-pointer-types -Wno-error
ge_original_covert_modem_object.o: CFLAGS += \
    -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA \
    -DVERSION_US -DBUGFIX_R0 -I$(TOPDIR)/../../src/game \
    -Wno-unused-parameter -Wno-incompatible-pointer-types -Wno-error
ge_original_covert_modem_projectile.o: CFLAGS += \
    -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA \
    -DVERSION_US -DBUGFIX_R0 -I$(TOPDIR)/../../src/game \
    -Wno-unused-parameter -Wno-incompatible-pointer-types \
    -Wno-int-conversion -Wno-error
ge_original_player_thrown_object_slice.o: CFLAGS += \
    -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA \
    -DVERSION_US -DBUGFIX_R0 -DGE_PORT_MS_INHERITS -fms-extensions \
    -I$(TOPDIR)/../../src/game -Wno-unused-parameter \
    -Wno-unused-variable -Wno-incompatible-pointer-types \
    -Wno-pointer-to-int-cast -Wno-int-conversion -Wno-error
ge_original_covert_modem_fire.o: CFLAGS += \
    -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA \
    -DVERSION_US -DBUGFIX_R0 -I$(TOPDIR)/../../src/game \
    -Wno-unused-parameter -Wno-incompatible-pointer-types \
    -Wno-int-conversion -Wno-error
ge_original_pp7_fire.o: CFLAGS += \
    -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA \
    -DVERSION_US -DBUGFIX_R0 -I$(TOPDIR)/../../src/game \
    -Wno-unused-parameter -Wno-incompatible-pointer-types \
    -Wno-int-conversion -Wno-error
ge_original_object_bullet_hit_slice.o: CFLAGS += \
    -DGE_PORT_USE_ORIGINAL_TYPES -DGE_PORT_SETUP_DATA -DAIPARSE \
    -DVERSION_US -DBUGFIX_R0 -DGE_PORT_MS_INHERITS -fms-extensions \
    -DPLAYERFLAG=int -I$(TOPDIR)/../../src/game \
    -Wno-unused-parameter -Wno-unused-variable \
    -Wno-incompatible-pointer-types -Wno-pointer-to-int-cast \
    -Wno-int-conversion -Wno-error
ge_original_bug_model.o: CFLAGS += \
    -DGE_PORT_USE_ORIGINAL_TYPES -DGE_PORT_SETUP_DATA -DAIPARSE \
    -DVERSION_US -Wno-missing-braces -Wno-unused-variable \
    -Wno-unused-const-variable -Wno-incompatible-pointer-types -Wno-error
ge_original_dam_guard_model.o: CFLAGS += \
    -DGE_PORT_USE_ORIGINAL_TYPES -DGE_PORT_SETUP_DATA -DAIPARSE \
    -DVERSION_US -Wno-missing-braces -Wno-unused-variable \
    -Wno-unused-const-variable -Wno-incompatible-pointer-types \
    -Wno-pointer-to-int-cast -Wno-error
ge_original_dam_guard_weapon_model.o: CFLAGS += \
    -DGE_PORT_USE_ORIGINAL_TYPES -DGE_PORT_SETUP_DATA -DAIPARSE \
    -DVERSION_US -Wno-missing-braces -Wno-unused-variable \
    -Wno-unused-const-variable -Wno-incompatible-pointer-types \
    -Wno-pointer-to-int-cast -Wno-error
ge_original_guard_grenade_model.o: CFLAGS += \
    -DGE_PORT_USE_ORIGINAL_TYPES -DGE_PORT_SETUP_DATA -DAIPARSE \
    -DVERSION_US -Wno-missing-braces -Wno-unused-variable \
    -Wno-unused-const-variable -Wno-incompatible-pointer-types \
    -Wno-pointer-to-int-cast -Wno-error
ge_original_guard_grenade_object.o: CFLAGS += \
    -DGE_PORT_USE_ORIGINAL_TYPES -DGE_PORT_SETUP_DATA -DAIPARSE \
    -DVERSION_US -DBUGFIX_R0 -DGE_PORT_MS_INHERITS -fms-extensions \
    -I$(TOPDIR)/../../src/game \
    -Wno-unused-parameter -Wno-unused-variable \
    -Wno-incompatible-pointer-types -Wno-int-conversion -Wno-error
ge_original_dam_guards.o: CFLAGS += \
    -DGE_PORT_USE_ORIGINAL_TYPES -DGE_PORT_SETUP_DATA -DAIPARSE \
    -DVERSION_US -DBUGFIX_R0 -DGE_PORT_MS_INHERITS -fms-extensions \
    -I$(TOPDIR)/../../src/game \
    -Wno-unused-parameter -Wno-unused-variable \
    -Wno-incompatible-pointer-types -Wno-int-conversion -Wno-error
ge_original_dam_guard_runtime.o: CFLAGS += \
    -DGE_PORT_USE_ORIGINAL_TYPES -DGE_PORT_SETUP_DATA -DAIPARSE \
    -DVERSION_US -DBUGFIX_R0 -I$(TOPDIR)/../../src/game \
    -Wno-unused-parameter -Wno-unused-variable \
    -Wno-incompatible-pointer-types -Wno-int-conversion -Wno-error
ge_original_dam_guard_scene.o: CFLAGS += \
    -DGE_PORT_USE_ORIGINAL_TYPES -DGE_PORT_SETUP_DATA -DAIPARSE \
    -DVERSION_US -DBUGFIX_R0 -I$(TOPDIR)/../../src/game \
    -Wno-unused-parameter -Wno-unused-variable \
    -Wno-incompatible-pointer-types -Wno-int-conversion -Wno-error
ge_original_guard_bullet_hit.o: CFLAGS += \
    -DGE_PORT_USE_ORIGINAL_TYPES -DGE_PORT_SETUP_DATA -DAIPARSE \
    -DVERSION_US -DBUGFIX_R0 -I$(TOPDIR)/../../src/game \
    -Wno-unused-parameter -Wno-unused-variable \
    -Wno-incompatible-pointer-types -Wno-int-conversion -Wno-error
ge_original_guard_animation_table.o: CFLAGS += \
    -DGE_PORT_USE_ORIGINAL_TYPES -DGE_PORT_SETUP_DATA -DAIPARSE \
    -DVERSION_US -DBUGFIX_R0 -DGE_PORT_MS_INHERITS -fms-extensions \
    -I$(TOPDIR)/../../src/game -Wno-unused-parameter \
    -Wno-unused-variable -Wno-incompatible-pointer-types \
    -Wno-int-conversion -Wno-error
ge_original_guard_bullet_hit_slice.o: CFLAGS += \
    -DGE_PORT_USE_ORIGINAL_TYPES -DGE_PORT_SETUP_DATA -DAIPARSE \
    -DVERSION_US -DBUGFIX_R0 -DGE_PORT_MS_INHERITS -fms-extensions \
    -I$(TOPDIR)/../../src/game -Wno-unused-parameter \
    -Wno-unused-variable -Wno-incompatible-pointer-types \
    -DGE_PORT_MODEL_HIT_NATIVE_ABI \
    -Wno-int-conversion -Wno-pointer-to-int-cast \
    -Wno-int-to-pointer-cast -Wno-error
ge_original_guard_damage_slice.o: CFLAGS += \
    -DGE_PORT_USE_ORIGINAL_TYPES -DGE_PORT_SETUP_DATA -DAIPARSE \
    -DVERSION_US -DBUGFIX_R0 -DGE_PORT_MS_INHERITS -fms-extensions \
    -DGE_PORT_DAMAGE_ANIMATION_OFFSETS \
    -I$(TOPDIR)/../../src/game -Wno-unused-parameter \
    -Wno-unused-variable -Wno-incompatible-pointer-types \
    -Wno-int-conversion -Wno-pointer-to-int-cast \
    -Wno-int-to-pointer-cast -Wno-error
ge_original_guard_damage_support_slice.o: CFLAGS += \
    -DGE_PORT_USE_ORIGINAL_TYPES -DGE_PORT_SETUP_DATA -DAIPARSE \
    -DVERSION_US -DBUGFIX_R0 -DGE_PORT_MS_INHERITS -fms-extensions \
    -I$(TOPDIR)/../../src/game -Wno-unused-parameter \
    -Wno-unused-variable -Wno-incompatible-pointer-types \
    -Wno-int-conversion -Wno-pointer-to-int-cast \
    -Wno-int-to-pointer-cast -Wno-error
ge_original_guard_animation_init_slice.o: CFLAGS += \
    -DGE_PORT_USE_ORIGINAL_TYPES -DGE_PORT_SETUP_DATA -DAIPARSE \
    -DVERSION_US -DBUGFIX_R0 -DGE_PORT_MS_INHERITS -fms-extensions \
    -DGE_PORT_ANIMATION_INIT_OFFSETS -DGE_PORT_ANIMATION_INIT_NATIVE_ABI \
    -I$(TOPDIR)/../../src/game -Wno-unused-parameter \
    -Wno-unused-variable -Wno-incompatible-pointer-types \
    -Wno-int-conversion -Wno-pointer-to-int-cast \
    -Wno-int-to-pointer-cast -Wno-error
ge_original_dam_guard_ai_tick_slice.o: CFLAGS += \
    -DGE_PORT_USE_ORIGINAL_TYPES -DGE_PORT_SETUP_DATA -DAIPARSE \
    -DVERSION_US -DBUGFIX_R0 -DGE_PORT_MS_INHERITS -fms-extensions \
    -DPLAYERFLAG=int -I$(TOPDIR)/../../src/game \
    -Wno-unused-parameter -Wno-unused-variable \
    -Wno-incompatible-pointer-types -Wno-int-conversion \
    -Wno-pointer-to-int-cast -Wno-switch -Wno-error
ge_original_dam_guard_ai_support_slice.o: CFLAGS += \
    -DGE_PORT_USE_ORIGINAL_TYPES -DGE_PORT_SETUP_DATA -DAIPARSE \
    -DVERSION_US -DBUGFIX_R0 -DGE_PORT_MS_INHERITS -fms-extensions \
    -DPLAYERFLAG=int -DREFRESH_NTSC \
    -DGE_PORT_DAM_GUARD_AI_ANIMATION_OFFSETS \
    -I$(TOPDIR)/../../src/game -Wno-unused-parameter \
    -Wno-unused-variable -Wno-incompatible-pointer-types \
    -Wno-int-conversion -Wno-pointer-to-int-cast -Wno-error
ge_original_dam_guard_chr_scheduler_slice.o: CFLAGS += \
    -DGE_PORT_USE_ORIGINAL_TYPES -DGE_PORT_SETUP_DATA -DAIPARSE \
    -DVERSION_US -DBUGFIX_R0 -DGE_PORT_MS_INHERITS -fms-extensions \
    -DPLAYERFLAG=int -I$(TOPDIR)/../../src/game \
    -Wno-unused-parameter -Wno-unused-variable \
    -Wno-unused-but-set-variable -Wno-incompatible-pointer-types \
    -Wno-int-conversion -Wno-pointer-to-int-cast \
    -Wno-int-to-pointer-cast -Wno-comment -Wno-error
ge_original_stage_guard_actor_slice.o: CFLAGS += \
    -DGE_PORT_USE_ORIGINAL_TYPES -DGE_PORT_SETUP_DATA -DAIPARSE \
    -DVERSION_US -DBUGFIX_R0 -DGE_PORT_MS_INHERITS -fms-extensions \
    -DPLAYERFLAG=int -I$(TOPDIR)/../../src/game \
    -Wno-unused-parameter -Wno-unused-variable \
    -Wno-incompatible-pointer-types -Wno-pointer-to-int-cast -Wno-error
ge_original_global_ai.o: CFLAGS += \
    -DGE_PORT_USE_ORIGINAL_TYPES -DGE_PORT_SETUP_DATA -DAIPARSE \
    -DVERSION_US -DGE_PORT_MS_INHERITS -fms-extensions \
    -I$(TOPDIR)/../../src/game
ge_original_stage_active_props.o ge_original_stage_mission_runtime.o: CFLAGS += \
    -DGE_PORT_USE_ORIGINAL_TYPES -DGE_PORT_SETUP_DATA -DAIPARSE \
    -DVERSION_US -DGE_PORT_MS_INHERITS -fms-extensions \
    -DPLAYERFLAG=int -I$(TOPDIR)/../../src/game -Wno-comment
ge_original_dam_guard_chr_scheduler_support_slice.o \
ge_original_dam_guard_chr_scheduler_full_props_support_slice.o: CFLAGS += \
    -DGE_PORT_USE_ORIGINAL_TYPES -DGE_PORT_SETUP_DATA -DAIPARSE \
    -DVERSION_US -DBUGFIX_R0 -DGE_PORT_MS_INHERITS -fms-extensions \
    -DPLAYERFLAG=int -I$(TOPDIR)/../../src/game \
    -Wno-unused-parameter -Wno-unused-variable \
    -Wno-unused-but-set-variable -Wno-incompatible-pointer-types \
    -Wno-int-conversion -Wno-pointer-to-int-cast \
    -Wno-int-to-pointer-cast -Wno-comment -Wno-error
# The full retained chr/prop scheduler contains compiler-emitted local
# .part.0 helpers and shared anonymous rodata referenced across its exact
# bodies. Keep that translation unit whole so --gc-sections cannot discard a
# local helper while retaining its canonical caller.
ge_original_dam_guard_chr_scheduler_full_props_support_slice.o: CFLAGS += \
    -fno-function-sections -fno-data-sections
ge_original_dam_projectile_explosion_slice.o: CFLAGS += \
    -DGE_PORT_USE_ORIGINAL_TYPES -DGE_PORT_SETUP_DATA -DAIPARSE \
    -DVERSION_US -DBUGFIX_R0 -DGE_PORT_MS_INHERITS -fms-extensions \
    -DPLAYERFLAG=int -I$(TOPDIR)/../../src/game -Wno-unused-variable \
    -Wno-unused-parameter -Wno-incompatible-pointer-types \
    -Wno-pointer-to-int-cast -Wno-int-conversion \
    -Wno-int-to-pointer-cast -Wno-missing-braces -Wno-sign-compare \
    -Wno-uninitialized -Wno-unused-value -Wno-error
ge_original_gun_update_and_fire_slice.o: CFLAGS += \
    -DGE_PORT_USE_ORIGINAL_TYPES -DGE_PORT_SETUP_DATA -DAIPARSE \
    -DVERSION_US -DBUGFIX_R0 -DGE_PORT_MS_INHERITS -fms-extensions \
    -I$(TOPDIR)/../../src/game -Wno-unused-parameter \
    -Wno-unused-variable -Wno-incompatible-pointer-types \
    -Wno-int-conversion -Wno-pointer-to-int-cast \
    -Wno-array-bounds -Wno-empty-body -Wno-error
ge_original_gun_pose_helpers_slice.o: CFLAGS += \
    -DGE_PORT_USE_ORIGINAL_TYPES -DGE_PORT_SETUP_DATA -DAIPARSE \
    -DVERSION_US -DBUGFIX_R0 -DGE_PORT_MS_INHERITS -fms-extensions \
    -I$(TOPDIR)/../../src/game -Wno-unused-parameter \
    -Wno-unused-variable -Wno-incompatible-pointer-types \
    -Wno-int-conversion -Wno-pointer-to-int-cast -Wno-error
ge_original_gun_casing_slice.o: CFLAGS += \
    -DGE_PORT_USE_ORIGINAL_TYPES -DGE_PORT_SETUP_DATA -DAIPARSE \
    -DVERSION_US -DBUGFIX_R0 -DGE_PORT_MS_INHERITS -fms-extensions \
    -I$(TOPDIR)/../../src/game -Wno-unused-parameter \
    -Wno-unused-variable -Wno-incompatible-pointer-types \
    -Wno-int-conversion -Wno-pointer-to-int-cast \
    -Wno-maybe-uninitialized -Wno-error
ge_original_gun_secondary_sinks.o: CFLAGS += \
    -DGE_PORT_USE_ORIGINAL_TYPES -DGE_PORT_SETUP_DATA -DAIPARSE \
    -DVERSION_US -DBUGFIX_R0 -DGE_PORT_MS_INHERITS -fms-extensions \
    -I$(TOPDIR)/../../src/game -Wno-unused-parameter \
    -Wno-unused-variable -Wno-incompatible-pointer-types \
    -Wno-int-conversion -Wno-pointer-to-int-cast -Wno-error
ge_original_gun_secondary_dependencies.o: CFLAGS += \
    -DGE_PORT_USE_ORIGINAL_TYPES -DGE_PORT_SETUP_DATA -DAIPARSE \
    -DVERSION_US -DBUGFIX_R0 -DGE_PORT_MS_INHERITS -fms-extensions \
    -I$(TOPDIR)/../../src/game -Wno-unused-parameter \
    -Wno-unused-variable -Wno-incompatible-pointer-types \
    -Wno-int-conversion -Wno-pointer-to-int-cast -Wno-error
ge_original_bond_input_gun_data_slice.o: CFLAGS += \
    -DGE_PORT_USE_ORIGINAL_TYPES -DGE_PORT_SETUP_DATA -DAIPARSE \
    -DVERSION_US -DBUGFIX_R0 -Wno-missing-braces -Wno-error
ge_original_bond_head_source.o: CFLAGS += \
    -DGE_PORT_USE_ORIGINAL_TYPES -DGE_PORT_BOND_HEAD_MOTION_SLICE \
    -DGE_PORT_BOND_HEAD_ANIMATION_SLICE \
    -DGE_PORT_BOND_HEAD_UPDATE_SLICE \
    -DGE_PORT_SETUP_DATA -DAIPARSE -Wno-unused-parameter \
    -Wno-incompatible-pointer-types -Wno-error
ge_original_bond_head_update.o: CFLAGS += \
    -DGE_PORT_USE_ORIGINAL_TYPES -DGE_PORT_SETUP_DATA -DAIPARSE \
    -Wno-unused-parameter -Wno-incompatible-pointer-types -Wno-error
ge_original_model_root_source.o ge_original_animation_root.o: CFLAGS += \
    -DGE_PORT_MODEL_ROOT_MOTION_SLICE -Wno-unused-value \
    -Wno-incompatible-pointer-types -Wno-error
ge_original_model_clock_source.o: CFLAGS += \
    -DGE_PORT_USE_ORIGINAL_TYPES -DGE_PORT_SETUP_DATA -DAIPARSE \
    -DGE_PORT_MODEL_ANIMATION_CLOCK_SLICE -Wno-unused-parameter \
    -Wno-unused-variable -Wno-empty-body -Wno-incompatible-pointer-types \
    -Wno-int-conversion -Wno-unused-value -Wno-error
ge_original_player_gait.o ge_original_player_gait_data_source.o \
ge_original_player_gait_model_slice.o: CFLAGS += \
    -DGE_PORT_USE_ORIGINAL_TYPES -DGE_PORT_SETUP_DATA -DAIPARSE \
    -Wno-unused-parameter -Wno-unused-variable -Wno-incompatible-pointer-types \
    -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast -Wno-missing-braces \
    -Wno-missing-field-initializers -Wno-error
ge_original_gunbarrel_bond.o: CFLAGS += \
    -DGE_PORT_USE_ORIGINAL_TYPES -DGE_PORT_SETUP_DATA -DAIPARSE \
    -DVERSION_US -DBUGFIX_R0 -Wno-incompatible-pointer-types -Wno-error
ge_original_frontend_cast_model.o: CFLAGS += \
    -DGE_PORT_USE_ORIGINAL_TYPES -DGE_PORT_SETUP_DATA -DAIPARSE \
    -DVERSION_US -DBUGFIX_R0 -Wno-incompatible-pointer-types -Wno-error
bondview_r.o: CFLAGS += -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA \
    -DGE_PORT_BOND_INTRO_SPAWN_SLICE -Wno-unused-parameter -Wno-error
prop.o: CFLAGS += -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA \
    -DGE_PORT_PROP_SETUP_PAD_SLICE -Wno-unused-parameter -Wno-empty-body -Wno-error
loadobjectmodel.o: CFLAGS += -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE \
    -DGE_PORT_SETUP_DATA -DGE_PORT_PROPDEF_SIZE_SLICE -Wno-error
ge_dam_setup_world_materializer.o: CFLAGS += -DGE_PORT_USE_ORIGINAL_TYPES \
    -DAIPARSE -DGE_PORT_SETUP_DATA -Wno-error
ge_original_dam_monitor.o: CFLAGS += -DGE_PORT_USE_ORIGINAL_TYPES \
    -DAIPARSE -DGE_PORT_SETUP_DATA -Wno-unused-parameter \
    -Wno-missing-declarations -Wno-error
ge_original_dam_monitor_animation_slice.o: CFLAGS += \
    -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA \
    -Wno-unused-parameter -Wno-unused-variable -Wno-unused-but-set-variable \
    -Wno-empty-body -Wno-int-conversion -Wno-int-to-pointer-cast \
    -Wno-pointer-to-int-cast -Wno-missing-declarations -Wno-error
ge_original_prop_state.o ge_original_prop_state_source.o: CFLAGS += \
    -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA \
    -Wno-unused-parameter -Wno-error
ge_original_default_object.o ge_original_default_object_source.o: CFLAGS += \
    -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA \
    -Wno-unused-variable -Wno-unused-parameter -Wno-error
ge_original_getposstan_source.o ge_original_objinit_source.o: CFLAGS += \
    -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA \
    -Wno-unused-variable -Wno-unused-parameter -Wno-error
ge_original_model62.o ge_original_model104.o ge_original_model178.o \
ge_original_dam_objective_models.o \
ge_original_first_person_assets.o ge_original_first_person_item_model.o: CFLAGS += -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE \
    -DGE_PORT_SETUP_DATA -Wno-unused-parameter -Wno-error
ge_original_door.o ge_original_door_source.o ge_original_door_interaction.o \
ge_original_door_runtime_source.o ge_original_door_scene.o: CFLAGS += \
    -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA \
    -Wno-unused-variable -Wno-unused-parameter -Wno-error
ge_original_first_person_pose.o ge_original_bond_camera_live.o: CFLAGS += \
    -DGE_PORT_USE_ORIGINAL_TYPES \
    -DAIPARSE -DGE_PORT_SETUP_DATA -Wno-unused-variable \
    -Wno-unused-parameter -Wno-error
ge_original_guard_bullet_hit.o: CFLAGS += -DGE_PORT_USE_ORIGINAL_TYPES \
    -DAIPARSE -DGE_PORT_SETUP_DATA -DVERSION_US -DBUGFIX_R0 \
    -I$(TOPDIR)/../../src/game -Wno-unused-variable -Wno-unused-parameter \
    -Wno-incompatible-pointer-types -Wno-comment -Wno-error
ge_original_door_collision.o ge_original_door_collision_slice.o \
ge_original_door_character_collision_slice.o: CFLAGS += \
    -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA \
    -I$(TOPDIR)/../../src/game -Wno-unused-variable -Wno-unused-parameter \
    -Wno-incompatible-pointer-types -Wno-pointer-to-int-cast -Wno-error
ge_original_dam_setup.o: CFLAGS += -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE \
                                  -DGE_PORT_SETUP_DATA -Wno-error
ge_original_stage_setup.o: CFLAGS += -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE \
                                    -DGE_PORT_SETUP_DATA
ge_original_stage_prop_materializer.o ge_original_stage_prop_native.o \
ge_original_stage_special_objects.o \
ge_original_stage_safe_runtime.o \
ge_original_stage_alarm_interaction.o \
ge_original_stage_monitor.o \
ge_original_stage_model_publication.o \
ge_original_stage_security.o \
ge_original_stage_autogun_lifecycle.o \
ge_original_stage_autogun_pitem_cleanup.o \
ge_original_stage_autogun_beam_exact.o \
ge_3ds_original_autogun_beam.o \
ge_original_stage_objective_runtime.o \
ge_original_stage_objective_photo_exact.o \
ge_original_stage_objective_services.o \
ge_original_stage_supplies.o \
ge_original_stage_guard_actor.o \
ge_original_stage_items.o \
ge_original_stage_interactive_objects.o \
ge_original_pitem_models.o ge_original_character_models.o \
ge_original_character_appearance.o ge_original_stage_guard_runtime.o: CFLAGS += \
    -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA
ge_original_stage_guard_runtime.o: CFLAGS += -Wno-comment
ge_original_stage_autogun_beam_exact.o: CFLAGS += -DVERSION_US
ge_original_stage_pickup_slice.o: CFLAGS += \
    -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA \
    -DVERSION_US -DPLAYERFLAG=int -I$(TOPDIR)/../../src/game \
    -Wno-unused-variable -Wno-unused-parameter \
    -Wno-incompatible-pointer-types -Wno-comment -Wno-error
ge_original_stage_safe_runtime.o: CFLAGS += -Wno-unused-variable
ge_original_stage_environment.o ge_original_stage_environment_tables.o: \
    CFLAGS += -Wno-missing-braces
ge_original_dam_mission_flow.o \
ge_original_dam_mission_stage_storage.o: CFLAGS += \
    -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA \
    -DGE_PORT_DAM_MISSION_FLOW_SLICE -DGE_PORT_DAM_MISSION_SFX_SLICE \
    -DVERSION_US -DPLAYERFLAG=int \
    -I$(TOPDIR)/../../src/game -Wno-comment -Wno-pragma-pack \
    -Wno-unused-variable -Wno-unused-function -Wno-unused-but-set-variable \
    -Wno-incompatible-pointer-types -Wno-int-conversion \
    -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast -Wno-error
chrai.o: CFLAGS += -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE \
                   -DGE_PORT_SETUP_DATA -DVERSION_US -DPLAYERFLAG=int \
                   -DGE_PORT_FULL_CAMPAIGN_INTERPRETER \
                   -DGE_PORT_MS_INHERITS -fms-extensions \
                   -I$(TOPDIR)/../../src/game -Wno-comment -Wno-pragma-pack \
                   -Wno-unused-variable -Wno-unused-function \
                   -Wno-unused-but-set-variable -Wno-incompatible-pointer-types \
                   -Wno-int-conversion -Wno-pointer-to-int-cast \
                   -Wno-int-to-pointer-cast -Wno-error
ge_original_dam_mission_object_state_slice.o: CFLAGS += \
    -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA \
    -Wno-unused-parameter -Wno-incompatible-pointer-types -Wno-error
ge_original_dam_mission_hud_slice.o: CFLAGS += \
    -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA \
    -Wno-unused-parameter -Wno-error
deb_loadallmodels.o: CFLAGS += \
    -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA \
    -DGE_PORT_DAM_STAGE_AI_ALLOC_SLICE -DPLAYERFLAG=int \
    -I$(TOPDIR)/../../src/game -Wno-comment -Wno-pragma-pack \
    -Wno-unused-variable -Wno-missing-braces -Wno-error
UsetupdamZ.o: CFLAGS += -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA \
                       -Wno-error -Wno-incompatible-pointer-types -Wno-missing-braces \
                       -Wno-pragmas
stan.o: CFLAGS += -DGE_PORT_STAN_GEOMETRY_SLICE \
    -DGE_PORT_STAN_DYNAMIC_PROP_COLLISION \
    -DGE_PORT_BOND_MOVEMENT_SLICE -Wno-uninitialized -Wno-unused-variable \
    -Wno-unused-parameter -Wno-empty-body
stanintersection.o: CFLAGS += -DGE_PORT_STAN_GEOMETRY_SLICE \
    -DGE_PORT_BOND_MOVEMENT_SLICE
bg.o: CFLAGS += -DGE_PORT_BG_CONNECTIVITY_SLICE -Wno-empty-body
ge_dam_world.o: CFLAGS += -DGE_PORT_BG_VISIBILITY_AVAILABLE
ge_original_bg_visibility.o ge_original_bg_visibility_slice.o: CFLAGS += \
    -DVERSION_US -DLEFTOVERDEBUG -Wno-unused-variable -Wno-empty-body \
    -Wno-incompatible-pointer-types -Wno-return-type -Wno-unused-parameter
ge_original_bg_visibility_slice.c: $(TOPDIR)/../../src/game/bg.c \
        $(TOPDIR)/../../scripts/extract_bg_visibility_slice.py
	python3 $(TOPDIR)/../../scripts/extract_bg_visibility_slice.py \
		$(TOPDIR)/../../src/game/bg.c $@
ge_original_door_collision_slice.c: \
        $(TOPDIR)/../../src/game/propobj.c \
        $(TOPDIR)/../../src/game/chrprop.c \
        $(TOPDIR)/../../src/game/bondview2.c \
        $(TOPDIR)/../../src/game/player.c \
        $(TOPDIR)/../../src/game/debugmenu_handler.c \
        $(TOPDIR)/../../src/game/glass2.c \
        $(TOPDIR)/../../src/game/bondview2.c \
        $(TOPDIR)/../../src/game/stan.c \
        $(TOPDIR)/../../src/game/vtxstore.c \
        $(TOPDIR)/../../scripts/extract_door_collision_slice.py
	python3 $(TOPDIR)/../../scripts/extract_door_collision_slice.py \
		$(TOPDIR)/../.. $@
ge_original_door_character_collision_slice.c: \
        $(TOPDIR)/../../src/game/chr.c \
        $(TOPDIR)/../../src/game/chrprop.c \
        $(TOPDIR)/../../src/game/explosion.c \
        $(TOPDIR)/../../src/game/front.c \
        $(TOPDIR)/../../scripts/extract_door_character_collision_slice.py
	python3 $(TOPDIR)/../../scripts/extract_door_character_collision_slice.py \
		$(TOPDIR)/../.. $@
ge_original_bond_input_live_state_slice.c: \
        $(TOPDIR)/../../src/game/gun.c \
        $(TOPDIR)/../../src/game/gunfire.c \
        $(TOPDIR)/../../src/game/lv.c \
        $(TOPDIR)/../../src/game/mp_music.c \
        $(TOPDIR)/../../src/game/music_0D2720.c \
        $(TOPDIR)/../../src/game/file.c \
        $(TOPDIR)/../../src/game/front.c \
        $(TOPDIR)/../../src/game/lv.c \
        $(TOPDIR)/../../src/game/propobj.c \
        $(TOPDIR)/../../src/game/player.c \
        $(TOPDIR)/../../src/game/glass.c \
        $(TOPDIR)/../../src/game/bondview.c \
        $(TOPDIR)/../../src/game/bondview2.c \
        $(TOPDIR)/../../src/game/file.c \
        $(TOPDIR)/../../src/game/file2.c \
        $(TOPDIR)/../../src/game/chraction.c \
        $(TOPDIR)/../../src/game/front.c \
        $(TOPDIR)/../../src/game/bondinv.c \
        $(TOPDIR)/../../src/game/player.c \
        $(TOPDIR)/../../src/game/propobj.c \
        $(TOPDIR)/../../src/game/chr.c \
        $(TOPDIR)/../../scripts/extract_bond_input_live_state_slice.py
	python3 $(TOPDIR)/../../scripts/extract_bond_input_live_state_slice.py \
		$(TOPDIR)/../.. $@
ge_original_bond_input_gun_data_slice.c: \
        $(TOPDIR)/../../src/game/gun.c \
        $(TOPDIR)/../../assets/embedded/skeletons/standard_gun.inc.c \
        $(TOPDIR)/../../assets/embedded/skeletons/gun_unassigned.inc.c \
        $(TOPDIR)/../../assets/obseg/gun/cartridge/Model.c \
        $(TOPDIR)/../../assets/obseg/gun/gunWeaponStats.inc.c \
        $(TOPDIR)/../../assets/obseg/gun/gunModelFileRecord.inc.c \
        $(TOPDIR)/../../assets/obseg/gun/fist/gunFileRecord.inc.c \
        $(TOPDIR)/../../assets/obseg/gun/knife/gunFileRecord.inc.c \
        $(TOPDIR)/../../assets/obseg/gun/throwknife/gunFileRecord.inc.c \
        $(TOPDIR)/../../assets/obseg/gun/wppk/gunFileRecord.inc.c \
        $(TOPDIR)/../../assets/obseg/gun/wppksil/gunFileRecord.inc.c \
        $(TOPDIR)/../../assets/obseg/gun/bug/gunFileRecord.inc.c \
        $(TOPDIR)/../../assets/obseg/gun/wppk/ModelFileHeader.inc.c \
        $(TOPDIR)/../../assets/obseg/gun/wppksil/ModelFileHeader.inc.c \
        $(TOPDIR)/../../assets/obseg/gun/bug/ModelFileHeader.inc.c \
        $(TOPDIR)/../../scripts/extract_bond_input_gun_data_slice.py
	python3 $(TOPDIR)/../../scripts/extract_bond_input_gun_data_slice.py \
		$(TOPDIR)/../.. $@
ge_original_gun_update_and_fire_slice.c: \
        $(TOPDIR)/../../src/game/gunfire.c \
        $(TOPDIR)/../../scripts/extract_gun_update_and_fire_slice.py
	python3 $(TOPDIR)/../../scripts/extract_gun_update_and_fire_slice.py \
		$(TOPDIR)/../.. $@
ge_original_gun_pose_helpers_slice.c: \
        $(TOPDIR)/../../src/game/gun.c \
        $(TOPDIR)/../../src/game/matrixmath_misc.c \
        $(TOPDIR)/../../src/game/bondview.c \
        $(TOPDIR)/../../src/game/bondview2.c \
        $(TOPDIR)/../../src/game/options.c \
        $(TOPDIR)/../../src/game/model.c \
        $(TOPDIR)/../../src/game/objecthandler.c \
        $(TOPDIR)/../../src/game/dyn.c \
        $(TOPDIR)/../../assets/embedded/skeletons/gun_kf7.inc.c \
        $(TOPDIR)/../../assets/embedded/skeletons/gun_revolver.inc.c \
        $(TOPDIR)/../../scripts/extract_gun_pose_helpers_slice.py
	python3 $(TOPDIR)/../../scripts/extract_gun_pose_helpers_slice.py \
		$(TOPDIR)/../.. $@
ge_original_gun_casing_slice.c: \
        $(TOPDIR)/../../src/game/gunfire.c \
        $(TOPDIR)/../../scripts/extract_gun_casing_slice.py
	python3 $(TOPDIR)/../../scripts/extract_gun_casing_slice.py \
		$(TOPDIR)/../.. $@
ge_original_gun_secondary_sinks.c: \
        $(TOPDIR)/../../src/game/gun.c \
        $(TOPDIR)/../../src/game/gunfire.c \
        $(TOPDIR)/../../src/game/bondview2.c \
        $(TOPDIR)/../../src/game/model.c \
        $(TOPDIR)/../../src/game/propobj.c \
        $(TOPDIR)/../../src/game/explosion.c \
        $(TOPDIR)/../../scripts/extract_gun_secondary_sinks.py
	python3 $(TOPDIR)/../../scripts/extract_gun_secondary_sinks.py \
		$(TOPDIR)/../.. $@
ge_original_gun_secondary_dependencies.c: \
        $(TOPDIR)/../../src/game/bondview2.c \
        $(TOPDIR)/../../src/game/model.c \
        $(TOPDIR)/../../src/game/propobj.c \
        $(TOPDIR)/../../src/game/explosion.c \
        $(TOPDIR)/../../src/game/gunfire.c \
        $(TOPDIR)/../../scripts/extract_gun_secondary_dependencies.py
	python3 $(TOPDIR)/../../scripts/extract_gun_secondary_dependencies.py \
		$(TOPDIR)/../.. $@
ge_original_player_thrown_object_slice.c: \
        $(TOPDIR)/../../src/game/gun.c \
        $(TOPDIR)/../../src/game/gunfire.c \
        $(TOPDIR)/../../src/game/objective_status2.c \
        $(TOPDIR)/../../src/game/bondview.c \
        $(TOPDIR)/../../src/game/bondview2.c \
        $(TOPDIR)/../../src/game/bondinv.c \
        $(TOPDIR)/../../src/game/chr.c \
        $(TOPDIR)/../../src/game/chraction.c \
        $(TOPDIR)/../../src/game/chrprop.c \
        $(TOPDIR)/../../src/game/propobj.c \
        $(TOPDIR)/../../scripts/extract_player_thrown_object_slice.py
	python3 $(TOPDIR)/../../scripts/extract_player_thrown_object_slice.py \
		$(TOPDIR)/../.. $@
ge_original_bond_move_runtime_slice.c: \
        $(TOPDIR)/../../src/game/bondview2.c \
        $(TOPDIR)/../../src/game/bondview.c \
        $(TOPDIR)/../../src/game/stan.c \
        $(TOPDIR)/../../scripts/extract_bond_move_runtime_slice.py
	python3 $(TOPDIR)/../../scripts/extract_bond_move_runtime_slice.py \
		$(TOPDIR)/../../src/game/bondview2.c \
		$(TOPDIR)/../../src/game/bondview.c \
		$(TOPDIR)/../../src/game/stan.c $@
ge_original_bond_move_collision_slice.c: \
        $(TOPDIR)/../../src/game/chr.c \
        $(TOPDIR)/../../src/game/bondview2.c \
        $(TOPDIR)/../../src/game/chrprop.c \
        $(TOPDIR)/../../src/game/propobj.c \
        $(TOPDIR)/../../src/game/loadobjectmodel.c \
        $(TOPDIR)/../../scripts/extract_bond_move_collision_slice.py
	python3 $(TOPDIR)/../../scripts/extract_bond_move_collision_slice.py \
		$(TOPDIR)/../.. $@
ge_original_bond_move_non_tank_slice.c: \
        $(TOPDIR)/../../src/game/bondview.c \
        $(TOPDIR)/../../src/game/bondview2.c \
        $(TOPDIR)/../../src/game/debugmenu_handler.c \
        $(TOPDIR)/../../src/game/front.c \
        $(TOPDIR)/../../src/game/gun.c \
        $(TOPDIR)/../../src/game/gunfire.c \
        $(TOPDIR)/../../src/game/lv.c \
        $(TOPDIR)/../../src/game/mp_music.c \
        $(TOPDIR)/../../src/game/music_0D2720.c \
        $(TOPDIR)/../../src/game/options.c \
        $(TOPDIR)/../../src/game/propobj.c \
        $(TOPDIR)/../../src/snd.c \
        $(TOPDIR)/../../scripts/extract_bond_move_non_tank_slice.py
	python3 $(TOPDIR)/../../scripts/extract_bond_move_non_tank_slice.py \
		$(TOPDIR)/../.. $@
ge_original_bond_move_explosion_slice.c: \
        $(TOPDIR)/../../src/game/chraction.c \
        $(TOPDIR)/../../src/game/chr.c \
        $(TOPDIR)/../../src/game/chrprop.c \
        $(TOPDIR)/../../src/game/debugmenu_handler.c \
        $(TOPDIR)/../../src/game/explosion.c \
        $(TOPDIR)/../../src/game/glass.c \
        $(TOPDIR)/../../src/game/gun.c \
        $(TOPDIR)/../../src/game/gunfire.c \
        $(TOPDIR)/../../src/game/model.c \
        $(TOPDIR)/../../src/game/objecthandler.c \
        $(TOPDIR)/../../src/game/stan.c \
        $(TOPDIR)/../../src/game/bondview2.c \
        $(TOPDIR)/../../src/game/propobj.c \
        $(TOPDIR)/../../assets/obseg/prop/propExplosionDetailsRecords.inc.c \
        $(TOPDIR)/../../assets/embedded/skeletons/prop_weapon.inc.c \
        $(TOPDIR)/../../scripts/extract_bond_move_explosion_slice.py
	python3 $(TOPDIR)/../../scripts/extract_bond_move_explosion_slice.py \
		$(TOPDIR)/../.. $@
ge_original_move_model_tables.c: \
        $(TOPDIR)/../../assets/obseg/prop/propModelFileHeaders.inc.c \
        $(TOPDIR)/../../assets/embedded/skeletons/props.inc.c \
        $(TOPDIR)/../../assets/embedded/skeletons/suit_lf_hand.inc.c \
        $(TOPDIR)/../../assets/obseg/prop/propItemModelFileRecord.inc.c \
        $(TOPDIR)/../../assets/obseg/chr/chrModelFileHeaders.inc.c \
        $(TOPDIR)/../../assets/obseg/chr/chrModelFileRecords.inc.c \
        $(TOPDIR)/../../assets/obseg/chr/chrHeadHats.inc.c \
        $(TOPDIR)/../../scripts/generate_move_model_tables.py
	python3 $(TOPDIR)/../../scripts/generate_move_model_tables.py \
		$(TOPDIR)/../.. $@
ge_original_animation_table_abi.c: \
        $(TOPDIR)/../../assets/animationtable_data.c \
        $(TOPDIR)/../../assets/animationtable_entries.c \
        $(TOPDIR)/../../assets/animationtable_entries.h \
        $(TOPDIR)/../../scripts/generate_animation_table_abi.py
	python3 $(TOPDIR)/../../scripts/generate_animation_table_abi.py \
		$(TOPDIR)/../../assets/animationtable_data.c $@
ge_original_bond_move_state_slice.c: \
        $(TOPDIR)/../../src/game/bondview.c \
        $(TOPDIR)/../../src/game/bondview2.c \
        $(TOPDIR)/../../src/game/player.c \
        $(TOPDIR)/../../src/game/lv.c \
        $(TOPDIR)/../../src/game/debugmenu_handler.c \
        $(TOPDIR)/../../src/game/model.c \
        $(TOPDIR)/../../src/game/stan.c \
        $(TOPDIR)/../../scripts/extract_bond_move_state_slice.py
	python3 $(TOPDIR)/../../scripts/extract_bond_move_state_slice.py \
		$(TOPDIR)/../../src/game/bondview.c \
		$(TOPDIR)/../../src/game/bondview2.c \
		$(TOPDIR)/../../src/game/player.c $(TOPDIR)/../../src/game/lv.c \
		$(TOPDIR)/../../src/game/debugmenu_handler.c \
		$(TOPDIR)/../../src/game/model.c $(TOPDIR)/../../src/game/stan.c $@
ge_original_player_gait_model_slice.c: $(TOPDIR)/../../src/game/model.c \
        $(TOPDIR)/../../src/game/objecthandler.c \
        $(TOPDIR)/../../src/game/bondview.c \
        $(TOPDIR)/../../src/game/initBondDATAdefaults.c \
        $(TOPDIR)/../../scripts/extract_player_gait_model_slice.py
	python3 $(TOPDIR)/../../scripts/extract_player_gait_model_slice.py \
		$(TOPDIR)/../../src/game/model.c \
		$(TOPDIR)/../../src/game/initBondDATAdefaults.c $@
ge_original_guard_bullet_hit_slice.c: \
        $(TOPDIR)/../../src/game/objecthandler.c \
        $(TOPDIR)/../../src/game/propobj.c $(TOPDIR)/../../src/game/chr.c \
        $(TOPDIR)/../../src/game/chrprop.c $(TOPDIR)/../../src/game/bg.c \
        $(TOPDIR)/../../src/game/line_tri_intersect.c \
        $(TOPDIR)/../../scripts/extract_guard_bullet_hit_slice.py
	python3 $(TOPDIR)/../../scripts/extract_guard_bullet_hit_slice.py \
		$(TOPDIR)/../.. $@
ge_original_guard_damage_slice.c: $(TOPDIR)/../../src/game/chr.c \
        $(TOPDIR)/../../src/game/chraction.c \
        $(TOPDIR)/../../src/game/gun.c \
        $(TOPDIR)/../../src/game/gunfire.c \
        $(TOPDIR)/../../src/game/chr.h \
        $(TOPDIR)/../../scripts/extract_guard_damage_slice.py \
        $(TOPDIR)/../../assets/oddtextures.c \
        $(TOPDIR)/../../assets/images.def \
        $(TOPDIR)/../../scripts/extract_guard_bullet_hit_slice.py \
        $(TOPDIR)/../../scripts/extract_bond_move_explosion_slice.py
	python3 $(TOPDIR)/../../scripts/extract_guard_damage_slice.py \
		$(TOPDIR)/../.. $@
ge_original_guard_damage_support_slice.c: \
        $(TOPDIR)/../../src/game/bondinv.c \
        $(TOPDIR)/../../src/game/bondview2.c \
        $(TOPDIR)/../../src/boss.c \
        $(TOPDIR)/../../src/game/cheat.c \
        $(TOPDIR)/../../src/game/options.c \
        $(TOPDIR)/../../src/game/propobj.c \
        $(TOPDIR)/../../src/game/bgroomtrans.c \
        $(TOPDIR)/../../src/game/bg.c \
        $(TOPDIR)/../../src/game/gunfire.c \
        $(TOPDIR)/../../src/joy.c \
        $(TOPDIR)/../../src/game/language.c \
        $(TOPDIR)/../../src/game/lv.c \
        $(TOPDIR)/../../src/memp.c \
        $(TOPDIR)/../../scripts/extract_guard_damage_support_slice.py
	python3 $(TOPDIR)/../../scripts/extract_guard_damage_support_slice.py \
		$(TOPDIR)/../.. $@ --arm-missing-only
ge_original_guard_animation_init_slice.c: \
        $(TOPDIR)/../../src/game/chr.c \
        $(TOPDIR)/../../src/game/initactorpropstuff.c \
        $(TOPDIR)/../../src/game/initanitable.c \
        $(TOPDIR)/../../scripts/extract_guard_animation_init_slice.py \
        $(TOPDIR)/../../scripts/extract_guard_bullet_hit_slice.py \
        $(TOPDIR)/../../scripts/extract_bond_move_explosion_slice.py
	python3 $(TOPDIR)/../../scripts/extract_guard_animation_init_slice.py \
		$(TOPDIR)/../.. $@
ge_original_dam_guard_ai_tick_slice.c: \
        $(TOPDIR)/../../assets/obseg/setup/UsetupdamZ.c \
        $(TOPDIR)/../../src/game/chrai.c \
        $(TOPDIR)/../../src/game/chraction.c \
        $(TOPDIR)/../../scripts/extract_dam_guard_ai_tick_slice.py
	python3 $(TOPDIR)/../../scripts/extract_dam_guard_ai_tick_slice.py \
		$(TOPDIR)/../.. $@
ge_original_dam_guard_ai_support_slice.c: \
        $(TOPDIR)/../../src/game/bg.c \
        $(TOPDIR)/../../src/game/bgfog.c \
        $(TOPDIR)/../../src/game/chr.c \
        $(TOPDIR)/../../src/game/chrprop.c \
        $(TOPDIR)/../../src/game/chraction.c \
        $(TOPDIR)/../../src/game/file.c \
        $(TOPDIR)/../../src/game/front.c \
        $(TOPDIR)/../../src/game/initanitable.c \
        $(TOPDIR)/../../src/game/model.c \
        $(TOPDIR)/../../src/game/objective_status2.c \
        $(TOPDIR)/../../src/game/padhalllv.c \
        $(TOPDIR)/../../src/game/propobj.c \
        $(TOPDIR)/../../src/game/stan.c \
        $(TOPDIR)/../../scripts/extract_dam_guard_ai_support_slice.py
	python3 $(TOPDIR)/../../scripts/extract_dam_guard_ai_support_slice.py \
		$(TOPDIR)/../.. $@
ge_original_stage_environment_tables.c: \
        $(TOPDIR)/../../src/game/bgfog.c \
        $(TOPDIR)/../../scripts/extract_stage_environment_tables.py
	python3 $(TOPDIR)/../../scripts/extract_stage_environment_tables.py \
		$(TOPDIR)/../.. $@
ge_original_dam_guard_chr_scheduler_slice.c: \
        $(TOPDIR)/../../src/game/chr.c \
        $(TOPDIR)/../../src/game/chrprop.c \
        $(TOPDIR)/../../scripts/extract_dam_guard_chr_scheduler_slice.py
	python3 $(TOPDIR)/../../scripts/extract_dam_guard_chr_scheduler_slice.py \
		$(TOPDIR)/../.. $@
ge_original_stage_pickup_slice.c: \
        $(TOPDIR)/../../src/game/propobj.c \
        $(TOPDIR)/../../src/game/chrprop.c \
        $(TOPDIR)/../../src/game/bondinv.c \
        $(TOPDIR)/../../src/game/bondview2.c \
        $(TOPDIR)/../../src/game/gunfire.c \
        $(TOPDIR)/../../scripts/extract_stage_pickup_slice.py
	python3 $(TOPDIR)/../../scripts/extract_stage_pickup_slice.py \
		$(TOPDIR)/../.. $@
ge_original_object_bullet_hit_slice.c: \
        $(TOPDIR)/../../src/game/propobj.c \
        $(TOPDIR)/../../src/game/gunfire.c \
        $(TOPDIR)/../../scripts/extract_object_bullet_hit_slice.py \
        $(TOPDIR)/../../scripts/extract_dam_guard_chr_scheduler_slice.py
	python3 $(TOPDIR)/../../scripts/extract_object_bullet_hit_slice.py \
		$(TOPDIR)/../.. $@
ge_original_stage_guard_actor_slice.c: \
        $(TOPDIR)/../../src/game/chr.c \
        $(TOPDIR)/../../src/game/chrprop.c \
        $(TOPDIR)/../../src/game/model.c \
        $(TOPDIR)/../../src/game/file.c \
        $(TOPDIR)/../../src/game/propobj.c \
        $(TOPDIR)/../../scripts/extract_stage_guard_actor_slice.py \
        $(TOPDIR)/../../scripts/extract_dam_guard_chr_scheduler_slice.py
	python3 $(TOPDIR)/../../scripts/extract_stage_guard_actor_slice.py \
		$(TOPDIR)/../.. $@
ge_original_global_ai.c: \
        $(TOPDIR)/../../build/u/src/game/chraidata.o \
        $(TOPDIR)/../../scripts/extract_global_ai_lists.py
	python3 $(TOPDIR)/../../scripts/extract_global_ai_lists.py \
		$(TOPDIR)/../../build/u/src/game/chraidata.o $@
ge_original_dam_guard_chr_scheduler_support_slice.c: \
        $(TOPDIR)/../../src/game/chr.c \
        $(TOPDIR)/../../src/game/chraction.c \
        $(TOPDIR)/../../src/game/propobj.c \
        $(TOPDIR)/../../src/game/debugmenu_handler.c \
        $(TOPDIR)/../../src/game/model.c \
        $(TOPDIR)/../../src/game/objecthandler.c \
        $(TOPDIR)/../../src/game/chrprop.c \
        $(TOPDIR)/../../src/game/explosion.c \
        $(TOPDIR)/../../src/game/player.c \
        $(TOPDIR)/../../src/game/file2.c \
        $(TOPDIR)/../../scripts/extract_dam_guard_chr_scheduler_support_slice.py \
        $(TOPDIR)/../../scripts/extract_dam_guard_chr_scheduler_slice.py
	python3 $(TOPDIR)/../../scripts/extract_dam_guard_chr_scheduler_support_slice.py \
		$(TOPDIR)/../.. $@
ge_original_dam_guard_chr_scheduler_full_props_support_slice.c: \
        $(TOPDIR)/../../scripts/extract_dam_guard_chr_scheduler_support_slice.py \
        $(TOPDIR)/../../scripts/extract_dam_guard_chr_scheduler_slice.py \
        $(TOPDIR)/../../assets/image_externs.h \
        $(wildcard $(TOPDIR)/../../src/game/*.c) \
        $(TOPDIR)/../../src/fr.c $(TOPDIR)/../../src/fr.h
	python3 $(TOPDIR)/../../scripts/extract_dam_guard_chr_scheduler_support_slice.py \
		--full-props $(TOPDIR)/../.. $@
ge_original_dam_projectile_explosion_slice.c: \
        $(TOPDIR)/../../src/game/propobj.c \
        $(TOPDIR)/../../src/game/model.c \
        $(TOPDIR)/../../scripts/extract_dam_projectile_explosion_slice.py
	python3 $(TOPDIR)/../../scripts/extract_dam_projectile_explosion_slice.py \
		$(TOPDIR)/../.. $@
ge_original_dam_mission_object_state_slice.c: \
        $(TOPDIR)/../../src/game/chraction.c \
        $(TOPDIR)/../../src/game/objective_status.c \
        $(TOPDIR)/../../src/game/objective.c \
        $(TOPDIR)/../../src/game/propobj.c \
        $(TOPDIR)/../../scripts/extract_dam_mission_object_state_slice.py
	python3 $(TOPDIR)/../../scripts/extract_dam_mission_object_state_slice.py \
		$(TOPDIR)/../.. $@
ge_original_dam_mission_hud_slice.c: \
        $(TOPDIR)/../../assets/images.def \
        $(TOPDIR)/../../assets/oddtextures.c \
        $(SOLO_LANGUAGE_CFILES) \
        $(TOPDIR)/../../src/game/bondview2.c \
        $(TOPDIR)/../../src/game/glass2.c \
        $(TOPDIR)/../../src/game/gun.c \
        $(TOPDIR)/../../scripts/extract_dam_mission_hud_slice.py
	python3 $(TOPDIR)/../../scripts/extract_dam_mission_hud_slice.py \
		$(TOPDIR)/../.. $@
ge_original_dam_monitor_animation_slice.c: \
        $(TOPDIR)/../../src/game/propobj.c \
        $(TOPDIR)/../../assets/oddtextures.c \
        $(TOPDIR)/../../assets/image_externs.h \
        $(TOPDIR)/../../scripts/extract_dam_monitor_animation_slice.py
	python3 $(TOPDIR)/../../scripts/extract_dam_monitor_animation_slice.py \
		$(TOPDIR)/../.. $@
$(OUTPUT).elf: $(OFILES)

%.bin.o %_bin.h: %.bin
	@$(bin2o)

.PRECIOUS: %.shbin
%.shbin.o %_shbin.h: %.shbin
	$(SILENTMSG) $(notdir $<)
	$(bin2o)

-include $(DEPSDIR)/*.d

endif
