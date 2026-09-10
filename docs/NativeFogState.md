# Typed native fog state (EB)

`ge_mp_state_fog` owns one EB record, namespace255, slot0, for the native fog
stage lifetime. Main binds it after `ge_original_multiplayer_fog_load` and
releases it before fog close. The adapter's monotonic lifetime is validation
evidence only; it is never serialized. An in-stage environment load or switch
mutates the same owner. Close/reload at identical native addresses invalidates
the old owner, while a new protocol session uses the caller's fresh registry.

The exporter reads actual native `bgfog.c` storage through a typed, read-only
view. It does not call the older diagnostic snapshot, dereference unknown
bindings, allocate, reload an environment or write VI/camera state. Every
published float is copied and encoded by its binary32 bits. Non-finite values,
unowned pointers, roster replacement and stale registry/native lifetimes fail
without publishing a record. This is one OBJECTS subdomain; neither capture
nor its schema declares the aggregate simulation complete.

## Native reference ownership

`g_EnvironmentFoundp`, `g_EnvironmentMainp` and `g_EnvironmentAltp` reference
physical rows of the source-owned `fog_tables` array. Their wire values are
validated internal table indices, with -1 for null. The ID printed in a table
row does not replace its physical identity, and rows are never sorted.
All named table values, including the final sentinel row, are captured in
source order. The final sentinel must retain Id0 so the native unbounded
loader cannot walk beyond its actual array.

`g_NearFogValuesP` is either null (-1), a table row's `Visibility.Nfd`, or the
native transition record's `Visibility.Nfd` (-2). No reference leaves this
single domain owner. Unknown, interior or misaligned addresses are rejected
before dereference. `fogSwitchToSolosky2` keeps its original program-lifetime
static record, exposed under a private translation-unit name. Every switch
first overwrites the entire record from `g_EnvironmentMainp`. Between switches,
only its three `Nfd` floats are reachable, through `g_NearFogValuesP`; those
three values are captured when that binding is live. Other static-record
fields and unreachable old transition payloads are overwritten before any
future read, so they are excluded.

An in-stage fogless load clears only the selected environment pointer. It
retains main/alternate/near bindings and previous fog coefficients, while
resetting the two scaled intensities and replacing current sky values. Capture
preserves this source behavior exactly. Fog close clears external references;
it does not need to rewrite the now unreachable static transition payload.

## Field map

| IDs | Typed native values |
| --- | --- |
|256–259|Last loaded stage/roster/cinematic owner binding and explicit stage visibility scale.|
|260–261|Actual signed `g_FogSkyIsEnabled` and native VI near/far range.|
|262|Current `DifferenceFromFarIntensity`, `FarIntensity` as I32.|
|263|Current Red, Green, Blue, Clouds, IsWater as U8.|
|264|Current SkyImageId, WaterImageId as I16.|
|265|Current CloudRepeat, CloudRed/Green/Blue, WaterRepeat, WaterRed/Green/Blue, WaterConcavity as F32.|
|266|Four named live `g_CurFogDetails` coefficients in source order.|
|267|Far, difference, scaled-far and scaled-difference intensity floats.|
|268|Found, main, alternate and near internal reference bindings.|
|269–270|Actual fog and fogless table row counts, including sentinels.|
|271|Per fog row: Id and Visibility.Intensity as U32.|
|272|Per fog row: both Fog intensity integers.|
|273–274|Per fog row: five sky U8 fields and two image I16 fields in the same order as263/264.|
|275|Per fog row: BlendMultiplier, FarFog, NearFog, MaxVisRange, MaxObfuscationRange, MinVisrange, then the nine sky floats in265 order.|
|276|Fogless row IDs.|
|277–279|Fogless row U8, I16 and F32 sky values in263–265 order.|
|280|Three transition Nfd floats when near references the static record; empty otherwise.|

The source table values are captured directly, including fields not currently
used by the retained loader. Native configuration mutation is therefore
visible without relying on a cached content hash. `Reserved`, `Reserved2`,
padding arrays and the two unnamed/unused `FogDetails` words have no source
consumer; neither they nor compiler padding enter typed state. Unreferenced
removed globals are excluded. GPU commands, sky vertex buffers, renderer
residency and per-eye output belong to other owners. The sky offset's canonical
update and all gameplay state outside fog still require their own coverage.

## Verification

`scripts/test_mp_state_fog.sh` compiles the actual loader, switch, state reader,
exporter and codec with ASan/UBSan under native and short-enum ABIs. Primitive
player-count/VI/camera services supply the retained native function boundaries;
this fixture is not a full match.

Each two/four-player process checks1351 direct native scalar/table/reference
mutations with exact field-position assertions, invalid references, signed
zero, non-finite rejection, native transition/fogless retention, close/reload,
stale owners and fresh-session history independence. It captures64 authored
stage/cinematic selections for each roster, with byte-identical hashes across
ABIs. The existing native fog suite separately compares256 native/pure stage
selections and the original fog/fade/culling functions.
