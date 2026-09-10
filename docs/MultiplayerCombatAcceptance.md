# Multiplayer character hit ownership acceptance

The character shot adapter now supports a separate canonical player roster in
addition to its existing authored-guard provider. This closes candidate and
victim ownership for the original character ray-to-damage path. It does not
publish a playable multiplayer menu or prove a complete match runtime.

## Integration boundary

Bind `ge_original_guard_bullet_hit_bind_players(context, count, player)` when
canonical two-to-four-player construction has succeeded. `count` returns the
fixed roster size; `player(context, slot)` returns the actual canonical
`struct player *` at that stable slot in `g_playerPointers`, cast to `void *`.
The roster must remain valid throughout synchronous shot tests and damage
application. Unbind before freeing players. Binding clears any pending shot;
clear pending shots at respawn boundaries too.

The adapter appends those players to the authored-guard candidates. It verifies
the VIEWER prop, reciprocal prop/character ownership, and matching player body
model. It excludes the shooter and players whose hit lists have not yet been
published. It does not synthesize visibility, bounding volumes, damage or
scores. `chrTestHit` retains its original dead/onscreen/depth/sphere/BBOX gates
and `chrpropAddBulletHit` retains hit ordering and penetration limits.

`chrHandleBulletHit` calls original `handles_shot_actors`, whose VIEWER branch
saves the attacker slot, switches to the struck player's slot, calls
`record_damage_kills`, and restores the attacker. That original routine owns
armor/health, death and kill attribution. The adapter now observes player
health/armor/death when reporting applied damage; player damage does not update
`ChrRecord.damage`, and repeated hits may leave character flags unchanged.
Existing authored-guard bindings and their damage observation remain intact.

## Required actor/camera scheduling

The full-props support extractor already retains the unchanged `playerTick`
body, including its multiplayer animation and `chrTick` dispatch. Merely adding
players to a list does not execute this branch or build usable hit matrices.
The active-prop scheduler must traverse their actual VIEWER props and retain the
canonical per-player ordering/freeze rules.

`chrTestHit` uses camera-space model matrices, hit lists and onscreen flags
published by `playerTick`/`chrTick`. Every shooter's simulation pass needs that
shooter's camera context before firing; publishing only the displayed camera
would test remote shots against the wrong matrices. A player whose list has
not been published is deliberately ineligible. Rendering the local camera and
ensuring deterministic full-roster simulation remain separate integration work.

## Verified fixture and limits

Run `bash scripts/test_multiplayer_combat.sh`. The asset-independent fixture
uses primitive GROUP/BBOX models with the actual canonical model intersection,
bounding sphere, hit traversal and ordered hit registration. It then executes
unchanged character-hit dispatch, body-part damage scaling, original PP7 weapon
stats, player switching, armor/health application, kill/death counters and kill
matrix updates. A source audit verifies every retained function body; only the
existing IDO aggregate-copy initializer receives a literal C expansion.

ASan/UBSan cover every ordered attacker/victim pair at two and four players
(14 pairings), with three independently intersected shots per pairing: armor,
remaining armor, and lethal health damage. Assertions verify the victim, intact
bystanders, restored attacker globals, attributed kill/death counts, and PP7
penetration ordering. Separate checks cover misses, range/depth, offscreen and
dead targets, missing hit lists, self-hit rejection, and the independent guard
binding. The modified production adapter also compiles for ARM with the original
ABI flags and `-Wall -Wextra -Werror` (existing header comment warning disabled).

This fixture does not construct ROM-derived character meshes or advance their
animation. Characters use the legitimate PREARGH state; its original reaction
eligibility routine is retained, while unused animation branches trap. Audio,
HUD, tracer/blood presentation and inventory-drop publication are test sinks.
A lethal hit verifies the drop callback is invoked, not that actual items are
created. No physics movement, pickups, respawn, simultaneous network match,
radio, emulator, frame timing or subjective quality is established by this
fixture. Those remain acceptance requirements for the integrated match runtime.
