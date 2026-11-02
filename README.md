# Aerondel (Unreal Engine 5)

*Aerondel* — a classic tick-based **single-player** open-world RPG, built **VR-ready from day one** with OpenXR.
This is the Unreal sister of the Unity scaffold ([Gielinor-Unity](https://github.com/doubleastandard9093-debug/Gielinor-Unity)).
It has the same design, the same starter content (Larkhaven, Granny Hilda, Granny's Pie) and the same classic XP curve.

The in-game world is called **Aerondel**; every town, NPC, monster, item, skill, spell and prayer uses original names (old saves are converted, see `Data/TownRenames.h` and `Data/ContentRenames.h`). "Aerondel" remains only as the repo/module/C++ identifier name.

> **Status: scaffold, never compiled.** It was written without the Unreal Editor available. The C++ passed static checks
> (see *Verification* below), but your first build is the real test. Paste any compile errors back and they'll be fixed.
> Multiplayer is paused. There is no networking code, but progression lives on `APlayerState` so it can replicate later.

## What's in the box

| Area | What you get |
|---|---|
| Data | `UItemDef`, `USkillDef` (+ `UXpCurve`), `UResourceNodeDef`, `URecipeDef`, `UQuestDef` (talk/gather/kill/deliver), `UNpcDef`, `UMobDef`, `UGameDatabase` loaded by `UAerondelAssetManager` |
| Progression | `USkillsComponent`, `UInventoryComponent` (28 slots, stacking), `UEquipmentComponent`, `UBankComponent` (400 slots), `UQuestComponent`, `UHealthComponent`, all on `AAerondelPlayerState` so pawns can swap |
| Events | `UAerondelEvents` game-instance event bus (xp, level-up, item gained, mob killed, npc talked, quest changed, messages, dialogue) |
| Interaction | `IAerondelInteractable`, implemented by `AResourceNode`, `AStation`, `ABankChest`, `ANpcTalk`, `ADoor`, `AGroundItem` and `AAerondelMob` |
| Desktop | `AAerondelCharacter` (spring-arm orbit, WASD/gamepad, click-to-move on the NavMesh, click-to-interact/attack) and `AAerondelPlayerController` |
| VR | `AAerondelVRPawn` (OpenXR: HMD camera, grip/aim controllers, teleport arc projected onto the NavMesh, smooth locomotion, snap/smooth turn, vignette, wrist UMG, laser), `UVRHandComponent` (grab/poke, context tool, velocity swing, hand-tracking pinch), `UVRInteractionBridge` |
| Mode switch | `UPlatformModeSubsystem` detects an HMD and possesses the matching pawn live, keeping state (`-vr` / `-novr` override) |
| Combat | `UCombatMath` (classic accuracy/max hit), `AAerondelMob` (code state machine: Idle/Wander/Chase/Attack/Return/Dead, drops, respawn), `UPlayerCombatComponent` (auto-attack + VR swing cooldown) |
| UI | `UAerondelWidgetBase` → HUD / Inventory / Skills / QuestLog C++ bases for UMG; `AAerondelHUD` Canvas fallback so it's playable before any widget exists |
| Save | `UAerondelSaveGame` + `UAerondelSaveSubsystem` (autosave every 60 s and on quit, content referenced by stable ids) |
| Graphics | `UAerondelScalability` tiers Quest / Low / High / Ultra; desktop Lumen + Nanite + VSM + TSR; Quest mobile forward + MSAA 4x + multiview + Vulkan + foveation |
| Editor | `Content/Python`: **Aerondel** menu with Create Input Assets, Generate Starter Data, Build Starter Town, Run All |

## First-time setup (Windows PC)

1. **Install Unreal Engine 5.6 or newer** from the Epic Games Launcher (Library → `+` → 5.6.x). In *Options* tick **Android** if you'll target Quest.
2. **Install Visual Studio 2022** (Community is fine) with the workloads **Game development with C++** and **Desktop development with C++**. In *Individual components*, add the **.NET 8 SDK** (or newer) and a **Windows 11 SDK**. Epic's page "Setting up Visual Studio for Unreal Engine" lists the exact toolchain for your version.
3. Install **Git LFS** (`git lfs install`), then clone this repo.
4. Right-click **`Aerondel.uproject`** → **Generate Visual Studio project files**. If you don't see that option, run `UnrealVersionSelector.exe /projectfiles` on the file, or just double-click the .uproject and accept *"rebuild modules?"*.
5. Open `Aerondel.sln`, set the configuration to **Development Editor | Win64**, startup project **Aerondel**, and **Build** (Ctrl+Shift+B). Then press F5 or double-click the .uproject.
6. When the editor opens it may ask to restart for plugins. Accept.
7. In the top menu bar click **Aerondel → Run All (1-3)**. That's the same as running these in order:
   1. *Create Input Assets* → `/Game/Gielinor/Input` (IA_* plus IMC_Desktop and IMC_XR)
   2. *Generate Starter Data* → `/Game/Gielinor/Data` (9 skills, 29 items, 6 nodes, 7 recipes, 3 NPCs, 2 mobs, Granny's Pie, DA_GameDatabase) and `M_Tint` materials
   3. *Build Starter Town* → `/Game/Gielinor/Maps/StarterTown`
8. **Restart the editor once** so the asset manager picks up `DA_GameDatabase` at startup. (It also lazy-loads, so this is belt and braces.)
9. Press **Play**. Click the ground to walk, click a tree to chop, right-drag to orbit, use the wheel to zoom, and press E or gamepad A to interact with the nearest thing.
10. Check the XP maths: **Tools → Session Frontend → Automation**, then run `Aerondel.Skills.XpCurve`.

### VR on PC (Quest Link / Air Link, SteamVR, WMR)
- The OpenXR plugin is already enabled. Set your OpenXR runtime (Meta Quest Link app → *Settings → General → OpenXR Runtime*, or SteamVR → *Settings → OpenXR*).
- Use **Play → VR Preview**. `UPlatformModeSubsystem` sees the headset and spawns `AAerondelVRPawn`. Take the headset off or launch with `-novr` to get the desktop pawn back.
- Controls: left stick moves, right stick X turns (snap 45° by default), right stick forward aims the teleport arc (release to go), grip grabs or draws a tool, trigger pokes or clicks UI, left menu toggles the wrist panel. With hand tracking, pinch acts as grip.

### Standalone Quest (Android)
Follow Epic's **"Setting Up Android SDK and NDK for Unreal"** page for *your* engine version. It pins exact versions of Android Studio, the SDK, NDK and JDK, and running `SetupAndroid.bat` from `Engine/Extras/Android` installs them. Then:
1. Enable **Developer Mode** on the headset (Meta Horizon phone app) and connect it over USB (allow debugging).
2. Project Settings → **Platforms → Android**: click *Configure Now* and accept the SDK license. `Config/DefaultEngine.ini` already sets Vulkan, arm64, ASTC, Quest 2/3 packaging, `bStartInVR` (Android only), mobile multiview, MSAA 4x and no Lumen or Nanite on Android.
3. Pick one XR path. **OpenXR** (default here, vendor-neutral) works on Quest through the Meta OpenXR loader that UE ships. Or add the **Meta XR plugin** from Meta's developer site to get Meta-only features (passthrough, Meta hand meshes, Quest-specific tooling). If you add it, enable it in the .uproject (`OculusXR` is listed but disabled) and follow Meta's "Unreal Engine 5 integration" setup.
4. Project Settings → **Project Validation** (Meta XR) or **XR → OpenXR**: fix anything flagged.
5. Platforms → **Android → Package Project** (ASTC), or use **Quick Launch** to your connected headset. Install with `adb install -r Aerondel-arm64.apk` if you package manually.

## Aerondel climate regions, Relicry and Delving (data mirror of the web build)

- `Content/Data/world.json` now carries `regions` and `biomes` (six climate regions: Rimeholt Tundra, Skyreach Highlands,
  Ashveil Wastes, Sunscour Dunes, Verdant Tangle, Mirelow Fen), their camp towns, lodges and ponds; `worldHalf` is 420.
- `skills.json` adds **Relicry** and **Delving** (ids `relicry` / `delving`, constants in `Source/Aerondel/Data/AerondelSkills.h`).
  `items.json` adds mattocks, dig-site materials, artifacts, relic gear and Delving gear; `relicry.json` holds dig sites,
  skill gates and dungeon themes; `mobs.json` adds the regional and dungeon monsters.
- Steps in the editor: **Aerondel > 2. Generate Starter Data** (re-imports the JSON), then **4. Build World Blockout**
  (now also builds region ground, biome props, dig sites and the Delver's Descent), then **5. Build Relic Dungeon**
  (sample instance at `/Game/Gielinor/Maps/RelicDungeon`). Dig / gate / boss gameplay is still web-only for now.

## Rename note (Gielinor -> Aerondel)

The project, C++ module (`Source/Aerondel`, `AERONDEL_API`) and every `Gielinor*` type are now `Aerondel*`.
`Config/DefaultEngine.ini` `[CoreRedirects]` maps `/Script/Gielinor` and each old class/enum to the new names, so existing
assets, Blueprints and save files keep loading. Saves are written to `Aerondel_Slot0`; if it doesn't exist yet the old
`Gielinor_Slot0` is loaded (and left untouched). The content folder stays `/Game/Gielinor` so generated maps and data
assets don't need moving. After pulling: delete `Binaries/`, `Intermediate/` and the old `Gielinor.sln`, right-click
`Aerondel.uproject` > Generate Visual Studio project files, then build.

## Repo layout
```
Aerondel.uproject
Config/            DefaultEngine/Game/Input/Editor/Scalability.ini + Android/ overrides
Source/Aerondel/   Core Data Skills Inventory Bank Quests Interaction Player XR Combat World UI Save Tests
Content/Python/    init_unreal.py (menu), create_input_assets.py, generate_starter_data.py, build_starter_town.py
Scripts/           lint_cpp.py + validate_project.py (static checks run on the dev box)
docs/              ARCHITECTURE.md, PERFORMANCE.md, NEXT_STEPS.md, UNREAL_VS_UNITY.md
```

## Verification done (no Unreal Editor on the build box)
- `Scripts/lint_cpp.py`: brace/paren balance, every reflected header includes `<File>.generated.h` last, the UCLASS/USTRUCT/UINTERFACE count matches the GENERATED_BODY count, A/U/F/E prefixes, every `.cpp` includes its own header first, project includes resolve, and no raw-pointer UPROPERTYs.
- `Scripts/validate_project.py`: `.uproject` JSON and plugin list, ini syntax, and the XP curve recomputed independently (level 2 = 83, level 10 = 1,154, level 99 = 13,034,431).
- `python -m py_compile` on every editor script.
- **Not done:** a real compile against engine headers, running the Python inside the editor, or a play test. Expect a handful of compile fixes on the first build.

## World streaming (World Partition)

`4. Build World Blockout` creates `/Game/Gielinor/Maps/World` as a World Partition map
(`Content/Python/world_partition.py`): 128 m runtime cells, 384 m loading range, every actor
spatially loaded except PlayerStart, lighting/sky/fog, ground, sea and nav bounds. The graphics
tier shrinks the range at runtime (Android 160 m, Low 256 m, High 384 m, Ultra 512 m) in
`AerondelScalability.cpp`. To grow the map, add coordinates to `Content/Data/world.json` and re-run
the blockout; new areas stream automatically. Optional: Build > Build HLODs for distant proxies.
If the log says the map isn't partitioned, open it and use Tools > Convert Level, then re-run.

## Aerondel update (events, guilds, villages, quests)
`Content/Data/aerondel_update.json` mirrors Lite's goblin village Snarlwick (190,40), ogre village Gorrumhold (-200,-30),
quest-warded elven town Sylvaranth (30,230; unlocked by `moonlit_ward`), generated NPCs/waymarks and the ~1,120 generated quests.
Seasonal events, guilds and the animation pass live in the Lite web build; Unreal reads this data for blockout/quest tables.
