/* Included at the canonical adapter's end: private file-static state remains
 * owned by that adapter. No address or borrowed callback enters the snapshot. */
#ifndef GE_RENDERER_BG_VISIBILITY_NAMESPACE_H
_Static_assert(GE_GLOBAL_VIS_MAX_COMMANDS == 512 && GE_BG_PORTAL_MAX_POINTS == 9,
    "Review visibility state-source capacities");
static const GeOriginalBgVisibilityProgram *ge_state_published_program;

void ge_original_bg_visibility_state_begin(void)
{
    ge_state_published_program = NULL;
}

void ge_original_bg_visibility_state_publish(const GeOriginalBgVisibilityInput *input)
{
    ge_state_published_program = input->program;
}

void ge_original_bg_visibility_state_retire(const GeOriginalBgVisibilityProgram *program)
{
    if (program == ge_state_published_program) ge_state_published_program = NULL;
}

int ge_original_bg_visibility_state_read(const GeOriginalBgVisibilityProgram *program,
    const uint8_t *background, size_t background_size, GeOriginalBgVisibilityStateSource *out)
{
    if (out == NULL || program == NULL || program != ge_state_published_program
            || program->background != background || program->background_size != background_size
            || program->room_count != (size_t)g_MaxNumRooms
            || program->portal_count > PORTMAX
            || program->command_count > GE_GLOBAL_VIS_MAX_COMMANDS
            || program->command_count != ge_global_command_count
            || program->stream_hash != ge_global_stream_hash
            || g_BgPortals != ge_portals || g_CurrentPlayer != &ge_player
            || dword_CODE_bss_8007FF90 != (ge_global_command_count != 0
                ? (s32 *)ge_global_commands : NULL)) return 0;
    for (size_t i=0;i<program->portal_count;++i)
        if (ge_portals[i].offset_portal != (bg_portal_entry *)&ge_portal_geometry[i]
                || ge_portal_geometry[i].numPoints < 3
                || ge_portal_geometry[i].numPoints > GE_BG_PORTAL_MAX_POINTS) return 0;
    if (ge_portals[program->portal_count].offset_portal != NULL) return 0;
    memset(out,0,sizeof(*out));
    out->room_count=(uint32_t)program->room_count;
    out->portal_count=(uint32_t)program->portal_count;
    out->command_count=(uint32_t)ge_global_command_count;
    out->stream_hash=ge_global_stream_hash;
    memcpy(out->player_position,ge_player_position.f,sizeof(out->player_position));
    memcpy(out->world_to_screen,ge_world_to_screen.m,sizeof(out->world_to_screen));
    out->projection[0]=ge_player.c_screenleft; out->projection[1]=ge_player.c_screentop;
    out->projection[2]=ge_player.c_halfwidth; out->projection[3]=ge_player.c_halfheight;
    out->projection[4]=ge_player.c_recipscalex; out->projection[5]=ge_player.c_recipscaley;
    memcpy(out->screen_box,ge_player.screensize.f,sizeof(out->screen_box));
    out->clip_range[0]=ge_near_distance; out->clip_range[1]=ge_far_distance;
    memcpy(out->projection_offsets,ge_projection_offset_px,sizeof(out->projection_offsets));
    out->viewport[0]=ge_view_left; out->viewport[1]=ge_view_top;
    out->viewport[2]=ge_view_width; out->viewport[3]=ge_view_height;
    for (size_t i=0;i<ge_global_command_count;++i) {
        /* Commands are immutable authored input, relocated to portal indices.
         * Reject alteration/unresolved bindings before copying any reference. */
        if (ge_global_commands[i].type != program->commands[i].type
                || ge_global_commands[i].length != program->commands[i].length
                || ge_global_commands[i].arg != program->commands[i].arg) return 0;
        out->command_types[i]=ge_global_commands[i].type;
        out->command_lengths[i]=ge_global_commands[i].length;
        out->command_arguments[i]=ge_global_commands[i].arg;
    }
    return 1;
}
#endif
