"""Auto-run by the Python Script Plugin at editor start: adds the top-level "Aerondel" menu."""
import unreal

MENU_OWNER = "AerondelTools"

ENTRIES = [
    ("CreateInputAssets", "1. Create Input Assets", "Enhanced Input actions + desktop/XR mapping contexts",
     "import create_input_assets, importlib; importlib.reload(create_input_assets); create_input_assets.run()"),
    ("GenerateStarterData", "2. Generate Starter Data", "Skills, items, nodes, recipes, NPCs, mobs, Granny's Pie, GameDatabase",
     "import generate_starter_data, importlib; importlib.reload(generate_starter_data); generate_starter_data.run()"),
    ("BuildSmoothShapes", "2b. Build Smooth Shapes", "Rounder cylinder/sphere/cone + bevelled cube (smooth normals, LODs) used by the builders",
     "import build_smooth_shapes, importlib; importlib.reload(build_smooth_shapes); build_smooth_shapes.run()"),
    ("BuildStarterTown", "3. Build Starter Town", "Larkhaven greybox level -> /Game/Gielinor/Maps/StarterTown",
     "import build_starter_town, importlib; importlib.reload(build_starter_town); build_starter_town.run()"),
    ("BuildWorldBlockout", "4. Build World Blockout", "Whole web world greybox from Content/Data/world.json -> /Game/Gielinor/Maps/World",
     "import build_world_blockout, importlib; importlib.reload(build_world_blockout); build_world_blockout.run()"),
    ("BuildRelicDungeon", "5. Build Relic Dungeon", "Sample Relicry instance dungeon (rooms, skill gates, boss, chest) -> /Game/Gielinor/Maps/RelicDungeon",
     "import build_relic_dungeon, importlib; importlib.reload(build_relic_dungeon); build_relic_dungeon.run()"),
    ("BuildFestivalDecor", "6. Build Festival Decor", "Seasonal event decorations around Larkhaven square in the open World map (hidden at runtime unless the event runs)",
     "import build_festival_decor, importlib; importlib.reload(build_festival_decor); build_festival_decor.run()"),
    ("ImportLiteModels", "7. Import Lite Models", "Interchange-import the Lite CC0 glTF models (rigged monsters, player, NPCs + props) and write Content/Data/rigs.json",
     "import import_lite_models, importlib; importlib.reload(import_lite_models); import_lite_models.run()"),
    ("RunAll", "Run All (1-3 + smooth shapes)", "Everything above, in order",
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
