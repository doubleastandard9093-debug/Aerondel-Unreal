# Next steps

## 0. First build (do this first)
- [ ] Build Development Editor and paste any compile errors back for fixes. Likely spots: engine API drift in 5.6, such as hand-tracking calls and Enhanced Input mapping storage.
- [ ] Run **Gielinor → Run All**, then Play and VR Preview.
- [ ] Run the automation test `Gielinor.Skills.XpCurve`.
- [ ] Swap `GetMotionControllerData` (deprecated) for `UHeadMountedDisplayFunctionLibrary::GetHandTrackingState` in `UVRHandComponent::UpdateHandTracking`.

## 1. Make the slice feel like Gielinor
- [ ] UMG widgets: make `WBP_HUD`, `WBP_Inventory`, `WBP_Skills` and `WBP_QuestLog` inheriting the C++ bases, then set `AGielinorHUD::HudWidgetClass` and `AGielinorVRPawn::WristWidgetClass` in Blueprint subclasses. Add a **bank window** (withdraw/deposit), a **recipe picker** for stations, and **dialogue choices** (accept/decline the quest). These have APIs but no widgets yet.
- [ ] Doors open automatically when a click-to-move path goes through them.
- [ ] Shop buy/sell (`UShopDef` + `AShopCounter`, the same pattern as stations).
- [ ] Firemaking (tinderbox + logs → fire actor → cook on fire), Prayer (bury bones), Ranged/Magic styles.
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
