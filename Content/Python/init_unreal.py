"""Auto-run by the Python Script Plugin at editor start: adds the top-level "Gielinor" menu."""
import unreal

MENU_OWNER = "GielinorTools"

ENTRIES = [
    ("CreateInputAssets", "1. Create Input Assets", "Enhanced Input actions + desktop/XR mapping contexts",
     "import create_input_assets, importlib; importlib.reload(create_input_assets); create_input_assets.run()"),
    ("GenerateStarterData", "2. Generate Starter Data", "Skills, items, nodes, recipes, NPCs, mobs, Granny's Pie, GameDatabase",
     "import generate_starter_data, importlib; importlib.reload(generate_starter_data); generate_starter_data.run()"),
    ("BuildStarterTown", "3. Build Starter Town", "Brightwater greybox level -> /Game/Gielinor/Maps/StarterTown",
     "import build_starter_town, importlib; importlib.reload(build_starter_town); build_starter_town.run()"),
    ("RunAll", "Run All (1-3)", "Everything above, in order",
     "import gielinor_all, importlib; importlib.reload(gielinor_all); gielinor_all.run()"),
]


def register_menu():
    menus = unreal.ToolMenus.get()
    main = menus.find_menu("LevelEditor.MainMenu")
    if not main:
        unreal.log_warning("[Gielinor] LevelEditor.MainMenu not found; menu not added.")
        return
    sub = main.add_sub_menu(MENU_OWNER, "", "Gielinor", "Gielinor", "Gielinor project tools")
    for name, label, tip, cmd in ENTRIES:
        entry = unreal.ToolMenuEntry(name=name, type=unreal.MultiBlockType.MENU_ENTRY)
        entry.set_label(label)
        entry.set_tool_tip(tip)
        entry.set_string_command(unreal.ToolMenuStringCommandType.PYTHON, "", cmd)
        sub.add_menu_entry("Gielinor", entry)
    menus.refresh_all_widgets()
    unreal.log("[Gielinor] Menu registered.")


register_menu()
