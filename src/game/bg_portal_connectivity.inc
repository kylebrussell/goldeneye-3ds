/**
 * Create a list of rooms connected to roomIndex.
 * @param roomIndex    Room to query.
 * @param list         Output buffer for connected room indices.
 * @param max          Max number of entries to write.
 * @return             Number of rooms written to the list.
 */
s32 bgGetConnectedRooms(s32 roomIndex, s32* list, s32 max)
{
    s32 len = 0;
    s32 i;
    s32 p;
    s32 connectedRoom1;
    s32 connectedRoom2;

    for (p = 0; g_BgPortals[p].offset_portal != NULL; p++) {
        connectedRoom1 = g_BgPortals[p].connectedRoom1;
        connectedRoom2 = g_BgPortals[p].connectedRoom2;

        if (connectedRoom1 == roomIndex) {
            connectedRoom1 = connectedRoom2;
            connectedRoom2 = roomIndex;
        }

        if (connectedRoom2 == roomIndex) {
            for (i = 0; i < len; i++) {
                if (list[i] == connectedRoom1) {
                    goto end;
                }
            }

            list[len] = connectedRoom1;
            len++;

            if (len >= max) {
                return len;
            }
end:
            if (1);
        }
    }

    return len;
}


// Scan all portals to see if these rooms are connected
//
// Room data doesn't contain a list of its portals, so it goes through
// the whole list of portals which seems naive and inefficient.
//
// Address: 0x7F0B8FD0
bool bgRoomsSharePortal(s32 room1, s32 room2)
{
    s32 i;

    for (i = 0; g_BgPortals[i].offset_portal != NULL; i++)
    {
        s32 v0 = g_BgPortals[i].connectedRoom1;
        s32 v1 = g_BgPortals[i].connectedRoom2;

        if (v0 == room1 && v1 == room2)
        {
            return TRUE;
        }

        if (v1 == room1 && v0 == room2)
        {
            return TRUE;
        }
    }
    return FALSE;
}


bool bgIsBboxOverlapping(coord3d *portalbbmin, coord3d *portalbbmax,
                         coord3d *propbbmin, coord3d *propbbmax)
{
    s32 i;

    for (i = 0; i < 3; i++)
    {
        if (propbbmin->f[i] > portalbbmax->f[i]
                || propbbmax->f[i] < portalbbmin->f[i])
        {
            return FALSE;
        }
    }

    return TRUE;
}


/* Exact portal-overlap expansion from sub_GAME_7F0BA2D4. The canonical
 * globals D_80044904..18 are simply three FLT_MAX/-FLT_MAX sentinels; spelling
 * them as coordinates avoids depending on linker adjacency of six scalars. */
void sub_GAME_7F0BA2D4(coord3d *bbmin, coord3d *bbmax,
                       s32 *room_list, s32 *count, s32 max_count)
{
    bg_portal_entry *portal_pts;
    f32 v;
    s32 cur_room;
    coord3d scaled_bbmin;
    coord3d scaled_bbmax;
    s32 cur_count;
    s32 i;
    s32 j;
    s32 k;
    s32 saved_count;
    coord3d portal_min;
    coord3d portal_max;
    s32 portal_idx;
    s32 other_room;

    cur_count = *count;
    i = 0;
    scaled_bbmin.x = bbmin->x * room_data_float1;
    scaled_bbmin.y = bbmin->y * room_data_float1;
    scaled_bbmin.z = bbmin->z * room_data_float1;
    scaled_bbmax.x = bbmax->x * room_data_float1;
    scaled_bbmax.y = bbmax->y * room_data_float1;
    scaled_bbmax.z = bbmax->z * room_data_float1;
    saved_count = cur_count;

    while (1)
    {
        if (i < cur_count)
        {
            do {
                cur_room = room_list[i];
                portal_idx = 0;

                if (g_BgPortals != NULL
                        && g_BgPortals[0].offset_portal != NULL)
                {
                    do {
                        if ((g_BgPortals[portal_idx].controlbytes1 & 1)
                                || ((cur_room != g_BgPortals[portal_idx].connectedRoom1)
                                && (cur_room != g_BgPortals[portal_idx].connectedRoom2)))
                        {
                            goto next_portal;
                        }

                        portal_min.x = portal_min.y = portal_min.z = 3.402823466e+38F;
                        portal_max.x = portal_max.y = portal_max.z = -3.402823466e+38F;
                        portal_pts = g_BgPortals[portal_idx].offset_portal;

                        for (j = 0; j < portal_pts->numPoints; j++)
                        {
                            for (k = 0; k < 3; k++)
                            {
                                v = (&portal_pts->point)[j].f[k];
                                if (v < portal_min.f[k]) portal_min.f[k] = v;
                                if (portal_max.f[k] < v) portal_max.f[k] = v;
                            }
                        }

                        if (bgIsBboxOverlapping(&portal_min, &portal_max,
                                                &scaled_bbmin, &scaled_bbmax))
                        {
                            if (cur_room == g_BgPortals[portal_idx].connectedRoom1)
                                other_room = g_BgPortals[portal_idx].connectedRoom2;
                            else
                                other_room = g_BgPortals[portal_idx].connectedRoom1;

                            for (k = 0; k < cur_count; k++)
                                if (room_list[k] == other_room) break;

                            if (k == cur_count)
                            {
                                if (cur_count < max_count)
                                    room_list[cur_count++] = other_room;
                                if (cur_count >= max_count)
                                {
                                    *count = cur_count;
                                    return;
                                }
                            }
                        }

next_portal:
                        portal_idx++;
                    } while (g_BgPortals[portal_idx].offset_portal != NULL);
                }

                i++;
            } while (i < saved_count);
        }

        if (cur_count == saved_count) break;
        saved_count = cur_count;
    }

    *count = cur_count;
}
