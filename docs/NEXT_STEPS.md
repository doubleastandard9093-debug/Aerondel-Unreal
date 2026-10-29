# Next steps

## WebGPU-parity port (Oct 2026): manual editor steps
The C++ now mirrors Gielinor-Lite's `webgpu` branch: rules/formulas (`UGielinorRules`), 0.6 s game tick, HP x10, prayer, potions/food boosts,
run energy, poison/venom/stun, adrenaline abilities, melee/ranged/magic combat with ammo/runes, procs, block/reflect/riposte, leash,
bounties, drops, gathering/cooking via rules, activity actors and data-driven quests (requirements, Visit/Xp/Bury steps, QP milestones).
- [ ] JSON content (Content/Data/*.json) is merged into the GameDatabase at startup; check the Output Log for loader warnings on first Play.
- [ ] Run **Gielinor → 4. Build World Blockout** (Content/Python/build_world_blockout.py) to greybox the whole web world into `/Game/Gielinor/Maps/World`
      from `world.json` (buildings, guilds + stations + bank chests, paths, water, landmarks, cave, and an `AQuestPlaceVolume` per quest place).
      Set it as the default map, then add resource nodes, NPCs and mobs (positions aren't in world.json yet).
- [ ] Place the remaining activity actors in the map: `AAgilityObstacleActor` (set CourseId + Index; drag the End widget), `AThieveTarget` (TargetId),
      `ARuneAltar` (AltarId), `APrayerAltar`, `AShopCounter` (shop id) and `AQuestPlaceVolume` (PlaceId matching quests.json `place`).
- [ ] Bind `AShopCounter::OnShopOpened` to a shop widget calling `Buy`/`Sell`; implement `AAgilityObstacleActor::OnCrossed` for the crossing animation.
- [ ] Starter mobs placed before the port still use the old HP scale; re-import or multiply their HP by 10.
- [ ] 22 web quests are flagged `bNeedsStepAuthoring`; author their steps in the Quest DataAssets.
- [ ] UMG still needed for: prayer book, spellbook/autocast, ability bar, run toggle, shop, bank, quest requirements text,
      worn-equipment panel (`UEquipmentComponent::TotalBonuses`, toolbelt, ammo count), graphics menu (`UGielinorScalability::SetAndSaveTier`),
      and a "Light" option on logs that calls `AGielinorFire::LightFrom` (desktop context menu + VR).
- [ ] Optional: make `BP_Fire` (child of `AGielinorFire`) with a Niagara flame and pass it as FireClass.
- [ ] Save v2 is backward compatible: v1 saves get HP x10, starter tools on the toolbelt and pack tools moved there. Test with an old save.
- [ ] Re-export from the web build (`Tools/export_webgpu_data.mjs`) now also writes `places` into world.json.
- [ ] `UCombatMath` is only kept for the `Gielinor.Skills.XpCurve` test; gameplay uses `UGielinorRules`.

## 0. First build (do this first)
- [x] Build Development Editor (laptop build passes: `Result: Succeeded`).
- [ ] Run **Gielinor → Run All**, then Play and VR Preview.
- [ ] Run the automation test `Gielinor.Skills.XpCurve`.
- [x] Swap `GetMotionControllerData` for `GetHandTrackingState` in `UVRHandComponent::UpdateHandTracking`.

## 1. Make the slice feel like Gielinor
- [ ] UMG widgets: make `WBP_HUD`, `WBP_Inventory`, `WBP_Skills` and `WBP_QuestLog` inheriting the C++ bases, then set `AGielinorHUD::HudWidgetClass` and `AGielinorVRPawn::WristWidgetClass` in Blueprint subclasses. Add a **bank window** (withdraw/deposit), a **recipe picker** for stations, and **dialogue choices** (accept/decline the quest). These have APIs but no widgets yet.
- [ ] Doors open automatically when a click-to-move path goes through them.
- [x] Shop buy/sell logic (`AShopCounter`); widget still needed.
- [x] Firemaking (`AGielinorFire`), Prayer and Ranged/Magic are done in C++.
- [ ] Death: keep 3 items plus a gravestone, matching the web build rules.
- [ ] Eat food from the inventory with a delay (the Canvas fallback can't click slots, so the UMG inventory is needed).

## 2. Art pipeline (towards RS3 quality and beyond)
- [ ] Replace greybox shapes: Nanite rocks, cliffs and buildings on desktop with LODs for Quest; trees via Foliage/PCG (HISM).
- [ ] Characters: MetaHuman or a stylised skeletal mesh, a modular equipment system (skeletal mesh merge), Control Rig and Motion Matching for locomotion, and IK for gathering.
- [ ] VR: full-body IK (IK Retargeter + VR IK) or hands-only, and tool/weapon meshes from `UItemDef::WorldMesh` (the hand already loads them).
- [ ] World Partition open world built with PCG for resources and decor. Interiors as Level Instances.
- [ ] Day/night (sun rotation + sky atmosphere already there) and weather.

## 3. Systems
- [ ] More skills in data only: add a `USkillDef`, `UResourceNodeDef` and `URecipeDef`. No code needed for gather/craft skills.
- [ ] Quest branching: `FQuestStep` conditions (skill level, item held, quest done) and choice nodes.
- [ ] StateTree for mobs once behaviours grow (ranged, fleeing, packs, bosses with telegraphs).
- [ ] Settings menu: graphics tier (`UGielinorScalability`), VR comfort (snap/smooth, vignette, seated mode, height calibration), keybinds (Enhanced Input user settings are on).

## 4. Platforms
- [ ] Quest: on-device profiling (OVR Metrics), Application SpaceWarp, Meta store requirements (VRCs).
- [ ] PC VR: SteamVR store build.
- [ ] **Consoles:** apply to the **PlayStation Partners** and **ID@Xbox** programs to get devkits and the platform SDKs. Unreal supports both once you're approved, and Epic gives console source access through the platform-specific UDN or GitHub groups after approval. Nintendo Switch would need a lower tier, similar to the Quest one.

## 5. Multiplayer (paused)
When it resumes: add `GetLifetimeReplicatedProps` with `UPROPERTY(Replicated)` on PlayerState components, turn interaction calls into Server RPCs on the PlayerController, and make mobs server-authoritative (they already run all their logic in one place).

## Web v18 parity port (2026-10-28)
All v18 systems now exist as C++ game-instance subsystems in Source/Gielinor/Features (logic + JSON save blocks):
Companions, Standing, Hearthstead, Errands, Stillwatch, Flow/dodge, Shiftsteeds, Skywing, Stable Codex, Saga, Silver Tongue,
Quirks, Writs, Arena, Paths, Hunt tasks, Delves, Charter, Siege, Wilds, Colossi, Boss mechanics, Sailing, Cards, Economy,
True Aim; plus XP re-curve (save v4), skill synergy, magic kit (Hexcraft/elements/path abilities), gamepad + pad menus.
Content/Data was re-exported from Lite v18 (Tools/export_webgpu_data.mjs).

**Nothing has been compiled.** First job: build in UE 5.8.4, fix compile errors, run the save round-trip test.

Fixed since (2026-10-29): Challenge taunt + attack delay, element weakness (x1.3), uncut gems from treasure digs, Spade
in the toolbelt, True Aim from the camera, Hexer brewing (Mama Sorrel), Sound water + islands + docks, procedural ships /
cannonballs / deck (ASailingVisuals), 21 JSON feature NPCs (service, home position, tint) placed by build_world_blockout.
Gear upgrades and notoriety: not in Lite v18 (only ship hull upgrades, already ported), so nothing to port.

UI: every feature panel is C++ UMG (UI/GielinorPanels + UGielinorPanelWidget, no Blueprint needed): pad menus
(inventory, settings, world map, nearby), settings (tier, Nanite opt-in, True Aim, sensitivity, invert, rumble),
companions, saga, hearth, shipwright, sea helm (WASD/stick steers, broadsides, sails), card tables, shops, and every NPC
service. NPCs open panels via UGielinorEvents::OnServiceRequested. True Aim strikes flash AGielinorAimMarker.

Graphics (High/Ultra only; Low/Quest keep the strict cheap path and never load High content):
- UGielinorScalability: Lumen GI + reflections (software), VSM, volumetric fog + clouds, DFAO, contact shadows;
  Ultra = Epic scalability + finer volumetrics. Low/Quest explicitly turn all of that off.
- AGielinorEnvironment: tunes the level's sun / sky / skylight / fog / unbound post volume per tier, live; adds
  volumetric clouds (engine cloud material) only on High+.
- AGielinorHighDetail: swaps M_Tint surfaces to M_TintHigh (procedural variation, animated water) and scatters grass,
  flowers and pebbles around the player; reverts on Low. Run Content/Python/build_high_fidelity.py once
  (gielinor_all does it); /Game/Gielinor/Materials/High is always cooked.
- Nanite stays off (crash fix 9b07fa6). Settings has an opt-in on High+ that sets r.Nanite=1, but r.Nanite.ProjectEnabled
  is still False and no mesh has Nanite data, so the opt-in does nothing until someone enables both and tests on weak GPUs.

Remaining simplifications:
- Nothing compiled yet; build in UE 5.8.4 and fix errors first.
- Panels are functional, not styled (default button style, no icons). The Bonds dialogue wheel, Errand Ledger, Standing,
  Stable Codex and Flow/Focus meters still use messages or the generic panels.
- Arena panel shows the ladder and token shop but cannot start a duel; duels are still started from the world.
- Character/building fidelity on High comes from materials only: no new meshes, skeletal characters or LODs.
- Water is the tinted plane plus an animated shimmer; no waves, no displacement, no UE Water plugin.
- Shop restock looks up caps per shop each tick; officers only help as the active companion.
- Multiplayer stays offline.
