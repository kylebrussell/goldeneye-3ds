#ifndef GE_ORIGINAL_STAGE_ENVIRONMENT_TABLE_TYPES_H
#define GE_ORIGINAL_STAGE_ENVIRONMENT_TABLE_TYPES_H

#include <stdint.h>

/*
 * Platform-neutral mirrors of the non-EU environment table ABI in bgfog.h.
 * These types intentionally contain only fixed-width scalar fields so the
 * exact authored table can be linked by host tools without importing the
 * original game's broad ultra64/bondtypes header graph.
 */
typedef struct GeOriginalNearFogRecord {
    float NearFog;
    float MaxVisRange;
    float MaxObfuscationRange;
} GeOriginalNearFogRecord;

typedef struct GeOriginalEnvironmentRecord {
    uint32_t Id;
    struct {
        float BlendMultiplier;
        float FarFog;
        GeOriginalNearFogRecord Nfd;
        float MinVisrange;
        uint32_t Intensity;
    } Visibility;
    struct {
        int32_t DifferenceFromFarIntensity;
        int32_t FarIntensity;
    } Fog;
    struct {
        uint8_t Red;
        uint8_t Green;
        uint8_t Blue;
        uint8_t Clouds;
        float CloudRepeat;
        int16_t SkyImageId;
        uint16_t Reserved;
        float CloudRed;
        float CloudGreen;
        float CloudBlue;
        uint8_t IsWater;
        uint8_t Padding[3];
        float WaterRepeat;
        int16_t WaterImageId;
        uint16_t Reserved2;
        float WaterRed;
        float WaterGreen;
        float WaterBlue;
        float WaterConcavity;
    } Sky;
} GeOriginalEnvironmentRecord;

typedef struct GeOriginalEnvironmentFoglessRecord {
    uint32_t Id;
    uint8_t Red;
    uint8_t Green;
    uint8_t Blue;
    uint8_t Clouds;
    float CloudRepeat;
    int16_t SkyImageId;
    uint16_t Reserved;
    float CloudRed;
    float CloudGreen;
    float CloudBlue;
    uint8_t IsWater;
    uint8_t Padding[3];
    float WaterRepeat;
    int16_t WaterImageId;
    uint16_t Reserved2;
    float WaterRed;
    float WaterGreen;
    float WaterBlue;
    float WaterConcavity;
} GeOriginalEnvironmentFoglessRecord;

_Static_assert(sizeof(GeOriginalEnvironmentRecord) == 92U,
    "non-EU EnvironmentRecord ABI changed");
_Static_assert(sizeof(GeOriginalEnvironmentFoglessRecord) == 56U,
    "non-EU EnvironmentFoglessRecord ABI changed");

#endif
