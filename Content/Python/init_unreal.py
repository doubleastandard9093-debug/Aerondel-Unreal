"""Auto-run by the Python Script Plugin at editor start: adds the top-level "Aerondel" menu."""
import unreal

MENU_OWNER = "AerondelTools"

ENTRIES = [
    ("BuildFullWorld", "0. BUILD FULL GAME WORLD (all steps + whole World map)", "Runs 1-3 + smooth shapes + high fidelity, then the FULL World map with every tree, rock, fishing spot and monster. Use this before packaging.",
     "import aerondel_full_world, importlib; importlib.reload(aerondel_full_world); aerondel_full_world.run()"),
    ("CreateInputAssets", "1. Create Input Assets", "Enhanced Input actions + desktop/XR mapping contexts",
     "import create_input_assets, importlib; importlib.reload(create_input_assets); create_input_assets.run()"),
    ("GenerateStarterData", "2. Generate Starter Data", "Skills, items, nodes, recipes, NPCs, mobs, Granny's Pie, GameDatabase",
     "import generate_starter_data, importlib; importlib.reload(generate_starter_data); generate_starter_data.run()"),
    ("BuildSmoothShapes", "2b. Build Smooth Shapes", "Rounder cylinder/sphere/cone + bevelled cube (smooth normals, LODs) used by the builders",
     "import build_smooth_shapes, importlib; importlib.reload(build_smooth_shapes); build_smooth_shapes.run()"),
    ("BuildStarterTown", "3. Build Starter Town", "Larkhaven greybox level -> /Game/Gielinor/Maps/StarterTown",
     "import build_starter_town, importlib; importlib.reload(build_starter_town); build_starter_town.run()"),
    ("BuildWorldBlockout", "4. Build World Blockout (FULL world)", "Whole web world from Content/Data/world.json + placements.json (trees, rocks, fishing, monsters) -> /Game/Gielinor/Maps/World",
     "import aerondel_safe, build_starter_town, build_aerondel_regions, world_placements, build_world_blockout, importlib; [importlib.reload(m) for m in (aerondel_safe, build_starter_town, build_aerondel_regions, world_placements, build_world_blockout)]; build_world_blockout.run()"),
    ("BuildWorldBlockoutSmall", "4s. SMALL TEST ONLY - partial World (do not package)", "Only a slice of the world (10 of each kind) to test a weak GPU / laptop. The packager refuses this build; run 0 or 4 before packaging.",
     "import aerondel_safe, build_starter_town, build_aerondel_regions, world_placements, build_world_blockout, importlib; [importlib.reload(m) for m in (aerondel_safe, build_starter_town, build_aerondel_regions, world_placements, build_world_blockout)]; build_world_blockout.run(small=True)"),
    ("BuildRelicDungeon", "5. Build Relic Dungeon", "Sample Relicry instance dungeon (rooms, skill gates, boss, chest) -> /Game/Gielinor/Maps/RelicDungeon",
     "import build_relic_dungeon, importlib; importlib.reload(build_relic_dungeon); build_relic_dungeon.run()"),
    ("BuildFestivalDecor", "6. Build Festival Decor", "Seasonal event decorations around Larkhaven square in the open World map (hidden at runtime unless the event runs)",
     "import build_festival_decor, importlib; importlib.reload(build_festival_decor); build_festival_decor.run()"),
    ("ImportLiteModels", "7. Import Lite Models", "Interchange-import the Lite CC0 glTF models (rigged monsters, player, NPCs + props) and write Content/Data/rigs.json",
     "import import_lite_models, importlib; importlib.reload(import_lite_models); import_lite_models.run()"),
    ("ImportLiteModelsSmall", "7b. Import Lite Models (10 per run, weak laptop)", "Same import, but stops after 10 new models; run it again to continue (already-imported models are skipped)",
     "import aerondel_safe, import_lite_models, importlib; importlib.reload(aerondel_safe); importlib.reload(import_lite_models); import_lite_models.run(batch=import_lite_models.BATCH_SMALL)"),
    ("BuildRealismMaterials", "8. Build Realism Materials", "Skin (subsurface), fur/cloth (sheen) and leather/metal masters for rigged bodies; High/Ultra only",
     "import aerondel_safe, build_realism_materials, importlib; importlib.reload(aerondel_safe); importlib.reload(build_realism_materials); build_realism_materials.run()"),
    ("BuildCharacterLods", "9. Build Character LODs (NPCs/monsters, not the player)", "Adds 2 auto-reduced LODs to shared rigged NPC/monster meshes (LOD0 unchanged); player rig skipped. Run after 7/7b.",
     "import build_character_lods, importlib; importlib.reload(build_character_lods); build_character_lods.run()"),
    ("RunAll", "Run All setup (1-3 + smooth shapes; NOT the World map)", "Setup steps only; use 0 for the full game world",
     "import aerondel_all, importlib; importlib.reload(aerondel_all); aerondel_all.run()"),
]


def register_menu():
    menus = unreal.ToolMenus.get()
    main = menus.find_menu("LevelEditor.MainMenu")
    if not main:
        unreal.log_warning("[Aerondel] LevelEditor.MainMenu not found; menu not added.")
        return
    sub = main.add_sub_menu(MENU_OWNER, "", "Aerondel", "Aerondel", "Aerondel project tools")
    for name, label, tip, cmd in ENTRIES:
        entry = unreal.ToolMenuEntry(name=name, type=unreal.MultiBlockType.MENU_ENTRY)
        entry.set_label(label)
        entry.set_tool_tip(tip)
        entry.set_string_command(unreal.ToolMenuStringCommandType.PYTHON, "", cmd)
        sub.add_menu_entry("Aerondel", entry)
    menus.refresh_all_widgets()
    unreal.log("[Aerondel] Menu registered.")


register_menu()
