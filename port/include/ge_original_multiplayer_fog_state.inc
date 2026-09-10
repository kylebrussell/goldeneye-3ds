/* Same native translation unit: direct typed reads after pointer validation. */
static int ge_fog_table_binding(const EnvironmentRecord *record, int32_t *index)
{
    const uintptr_t base = (uintptr_t)fog_tables, address = (uintptr_t)record;
    if (record == NULL) { *index = -1; return 1; }
    if (address < base || address-base >= sizeof(fog_tables)
            || (address-base)%sizeof(fog_tables[0]) != 0) return 0;
    *index = (int32_t)((address-base)/sizeof(fog_tables[0]));
    return 1;
}

/* SkyBoxRecord, CurrentEnvironmentRecord and EnvironmentFoglessRecord share
 * named members, not an assumed memory prefix. Every assignment is typed. */
#define GE_FOG_READ_SKY(source, colors, images, values) do { \
    (colors)[0]=(source).Red; (colors)[1]=(source).Green; \
    (colors)[2]=(source).Blue; (colors)[3]=(source).Clouds; \
    (colors)[4]=(source).IsWater; \
    (images)[0]=(source).SkyImageId; (images)[1]=(source).WaterImageId; \
    (values)[0]=(source).CloudRepeat; (values)[1]=(source).CloudRed; \
    (values)[2]=(source).CloudGreen; (values)[3]=(source).CloudBlue; \
    (values)[4]=(source).WaterRepeat; (values)[5]=(source).WaterRed; \
    (values)[6]=(source).WaterGreen; (values)[7]=(source).WaterBlue; \
    (values)[8]=(source).WaterConcavity; \
} while (0)

int ge_original_multiplayer_fog_state_read_exact(GeOriginalMultiplayerFogState *out)
{
    const size_t count=sizeof(fog_tables)/sizeof(fog_tables[0]);
    const size_t fogless=sizeof(fog_tables2)/sizeof(fog_tables2[0]);
    _Static_assert(sizeof(fog_tables)/sizeof(fog_tables[0])<=GE_ORIGINAL_FOG_MAX_ROWS,
        "Review native fog table capacity");
    _Static_assert(sizeof(fog_tables2)/sizeof(fog_tables2[0])<=GE_ORIGINAL_FOG_MAX_ROWS,
        "Review native fogless table capacity");
    if (!out || fog_tables[count-1].Id != 0 || fog_tables2[fogless-1].Id != 0
            || !ge_fog_table_binding(g_EnvironmentFoundp,&out->bindings[0])
            || !ge_fog_table_binding(g_EnvironmentMainp,&out->bindings[1])
            || !ge_fog_table_binding(g_EnvironmentAltp,&out->bindings[2])) return 0;
    out->bindings[3] = -1;
    if (g_NearFogValuesP != NULL) {
        if (g_NearFogValuesP == &ge_fog_transition_environment.Visibility.Nfd) {
            out->bindings[3] = -2;
            out->transition_near[0]=g_NearFogValuesP->NearFog;
            out->transition_near[1]=g_NearFogValuesP->MaxVisRange;
            out->transition_near[2]=g_NearFogValuesP->MaxObfuscationRange;
        } else {
            size_t index;
            for (index=0; index<count; ++index)
                if (g_NearFogValuesP == &fog_tables[index].Visibility.Nfd) break;
            if (index==count) return 0;
            out->bindings[3]=(int32_t)index;
        }
    }
    out->enabled=g_FogSkyIsEnabled;
    viGetZRange(out->zrange);
    out->current_fog[0]=g_CurrentEnvironment.DifferenceFromFarIntensity;
    out->current_fog[1]=g_CurrentEnvironment.FarIntensity;
    GE_FOG_READ_SKY(g_CurrentEnvironment,out->current_sky,
        out->current_images,out->current_parameters);
    out->details[0]=g_CurFogDetails.g_CurFogDetails;
    out->details[1]=g_CurFogDetails.scaled_far_fog_dist;
    out->details[2]=g_CurFogDetails.far_fog_dist_scaled;
    out->details[3]=g_CurFogDetails.near_fog_dist_scaled;
    out->intensities[0]=g_FarFogIntensity;
    out->intensities[1]=g_DifferenceFromFarFogIntensity;
    out->intensities[2]=g_ScaledFarFogIntensity;
    out->intensities[3]=g_ScaledDifferenceFromFarFogIntensity;
    out->environment_count=(uint32_t)count;
    out->fogless_count=(uint32_t)fogless;
    for (size_t i=0;i<count;++i) {
        const EnvironmentRecord *r=&fog_tables[i];
        out->environment_u32[i*2]=r->Id;
        out->environment_u32[i*2+1]=r->Visibility.Intensity;
        out->environment_i32[i*2]=r->Fog.DifferenceFromFarIntensity;
        out->environment_i32[i*2+1]=r->Fog.FarIntensity;
        float *f=&out->environment_f32[i*15];
        f[0]=r->Visibility.BlendMultiplier; f[1]=r->Visibility.FarFog;
        f[2]=r->Visibility.Nfd.NearFog; f[3]=r->Visibility.Nfd.MaxVisRange;
        f[4]=r->Visibility.Nfd.MaxObfuscationRange; f[5]=r->Visibility.MinVisrange;
        GE_FOG_READ_SKY(r->Sky,&out->environment_u8[i*5],
            &out->environment_i16[i*2],&f[6]);
    }
    for (size_t i=0;i<fogless;++i) {
        out->fogless_ids[i]=fog_tables2[i].Id;
        GE_FOG_READ_SKY(fog_tables2[i],&out->fogless_u8[i*5],
            &out->fogless_i16[i*2],&out->fogless_f32[i*9]);
    }
    return 1;
}
#undef GE_FOG_READ_SKY
