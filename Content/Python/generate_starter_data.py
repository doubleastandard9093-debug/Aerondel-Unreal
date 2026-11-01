"""
Gielinor > Generate Starter Data
Creates/updates the data assets for the starter slice (same content as the Unity scaffold's
StarterDataGenerator.cs, names ported from the web build: Larkhaven, Granny Hilda, Granny's Pie)
and the GameDatabase. Idempotent: rerun after editing values here.
"""
import unreal
from gielinor_util import ROOT, load_or_create, set_props, color, save, tint_material

D = ROOT + "/Data"
items, skills, nodes, npcs, mobs = {}, {}, {}, {}, {}
db_lists = {"items": [], "skills": [], "resource_nodes": [], "recipes": [], "quests": [], "npcs": [], "mobs": []}

T = unreal.ToolType
S = unreal.EquipSlot


def skill(id_, name, rgb, start=1):
    a = load_or_create(f"{D}/Skills/Skill_{name}", unreal.SkillDef)
    set_props(a, id=id_, display_name=name, color=color(*rgb), start_level=start, max_level=99)
    skills[id_] = a
    db_lists["skills"].append(a)
    return a


def item(id_, name, rgb, stack=False, value=1, heal=0):
    a = load_or_create(f"{D}/Items/Item_{id_}", unreal.ItemDef)
    set_props(a, id=id_, display_name=name, tint=color(*rgb), stackable=stack, value=value,
              heal_amount=heal, examine=f"A {name.lower()}.",
              tool_type=T.NONE, tool_tier=0, equip_slot=S.NONE,
              attack_bonus=0, strength_bonus=0, defence_bonus=0, required_level=1)
    items[id_] = a
    db_lists["items"].append(a)
    return a


def tool(id_, name, rgb, tool_type, tier, atk):
    a = item(id_, name, rgb, value=16 * tier)
    set_props(a, tool_type=tool_type, tool_tier=tier)
    if tool_type in (T.AXE, T.PICKAXE):
        set_props(a, equip_slot=S.WEAPON, attack_bonus=atk, strength_bonus=atk)
    return a


def gear(id_, name, rgb, slot, atk, stre, dfn, lvl):
    a = item(id_, name, rgb, value=20)
    set_props(a, equip_slot=slot, attack_bonus=atk, strength_bonus=stre, defence_bonus=dfn, required_level=lvl)
    return a


def node(id_, name, kind, skill_id, lvl, tool_type, xp, base, deplete, respawn, product, rgb):
    a = load_or_create(f"{D}/Resources/Node_{id_}", unreal.ResourceNodeDef)
    set_props(a, id=id_, display_name=name, kind=kind, skill=skills[skill_id], level_required=lvl,
              tool_required=tool_type, xp=xp, base_success=base, deplete_chance=deplete,
              respawn_seconds=respawn, product=items[product], tint=color(*rgb))
    nodes[id_] = a
    db_lists["resource_nodes"].append(a)
    return a


def amount(item_id, n):
    s = unreal.ItemAmount()
    s.set_editor_property("item", items[item_id])
    s.set_editor_property("amount", n)
    return s


def recipe(id_, name, station, skill_id, lvl, xp, tool_type, out, out_n, *inputs, fail=None, fail_chance=0.0, stop_fail=99):
    a = load_or_create(f"{D}/Recipes/Recipe_{id_}", unreal.RecipeDef)
    set_props(a, id=id_, display_name=name, station=station, skill=skills[skill_id], level_required=lvl, xp=xp,
              tool_required=tool_type, output=items[out], output_amount=out_n,
              inputs=[amount(i, n) for i, n in inputs],
              fail_item=items[fail] if fail else None, fail_chance_at_req=fail_chance, stop_fail_level=stop_fail)
    db_lists["recipes"].append(a)
    return a


def npc(id_, name, rgb, *lines):
    a = load_or_create(f"{D}/Npcs/Npc_{id_}", unreal.NpcDef)
    set_props(a, id=id_, display_name=name, tint=color(*rgb), idle_lines=list(lines))
    npcs[id_] = a
    db_lists["npcs"].append(a)
    return a


def drop(item_id, mn, mx, chance):
    d = unreal.DropEntry()
    for k, v in (("item", items[item_id]), ("min", mn), ("max", mx), ("chance", chance)):
        d.set_editor_property(k, v)
    return d


def mob(id_, name, lvl, hp, max_hit, aggressive, rgb, size_m, *drops):
    a = load_or_create(f"{D}/Mobs/Mob_{id_}", unreal.MobDef)
    # Unity size is (x width, y height, z length) in metres; UE BodySize is (width, length, height) in cm.
    w, h, l = size_m
    set_props(a, id=id_, display_name=name, combat_level=lvl, hitpoints=hp, max_hit=max_hit,
              aggressive=aggressive, tint=color(*rgb), body_size=unreal.Vector(w * 100, l * 100, h * 100),
              drops=list(drops))
    mobs[id_] = a
    db_lists["mobs"].append(a)
    return a


def step(type_, desc, npc_id=None, item_id=None, mob_id=None, n=1):
    s = unreal.QuestStep()
    s.set_editor_property("type", type_)
    s.set_editor_property("description", desc)
    s.set_editor_property("amount", n)
    if npc_id:
        s.set_editor_property("npc", npcs[npc_id])
    if item_id:
        s.set_editor_property("item", items[item_id])
    if mob_id:
        s.set_editor_property("mob", mobs[mob_id])
    return s


def run():
    with unreal.ScopedSlowTask(6, "Gielinor: generating starter data") as task:
        task.make_dialog(True)
        tint_material()

        task.enter_progress_frame(1, "Skills")
        skill("attack", "Attack", (0.8, 0.2, 0.2))
        skill("strength", "Strength", (0.2, 0.7, 0.3))
        skill("defence", "Defence", (0.4, 0.55, 0.9))
        skill("hitpoints", "Hitpoints", (0.9, 0.3, 0.3), 10)
        skill("woodcutting", "Woodcutting", (0.4, 0.6, 0.25))
        skill("mining", "Mining", (0.55, 0.5, 0.45))
        skill("smithing", "Smithing", (0.6, 0.6, 0.6))
        skill("fishing", "Fishing", (0.4, 0.6, 0.9))
        skill("cooking", "Cooking", (0.6, 0.3, 0.6))

        task.enter_progress_frame(1, "Items")
        bronze, iron = (0.72, 0.45, 0.2), (0.55, 0.55, 0.58)
        item("coins", "Coins", (1, 0.85, 0.2), stack=True, value=1)
        tool("bronze_axe", "Bronze axe", bronze, T.AXE, 1, 1)
        tool("iron_axe", "Iron axe", iron, T.AXE, 2, 1)
        tool("bronze_pickaxe", "Bronze pickaxe", bronze, T.PICKAXE, 1, 1)
        tool("iron_pickaxe", "Iron pickaxe", iron, T.PICKAXE, 2, 1)
        tool("small_fishing_net", "Small fishing net", (0.8, 0.8, 0.7), T.FISHING_NET, 1, 0)
        tool("hammer", "Hammer", iron, T.HAMMER, 1, 0)
        tool("tinderbox", "Tinderbox", (0.5, 0.35, 0.2), T.TINDERBOX, 1, 0)
        gear("bronze_sword", "Bronze sword", bronze, S.WEAPON, 4, 5, 0, 1)
        gear("iron_sword", "Iron sword", iron, S.WEAPON, 10, 7, 0, 1)
        gear("bronze_med_helm", "Bronze med helm", bronze, S.HEAD, 0, 0, 3, 1)
        gear("bronze_platebody", "Bronze platebody", bronze, S.BODY, 0, 0, 15, 1)
        gear("iron_platebody", "Iron platebody", iron, S.BODY, 0, 0, 21, 1)
        gear("bronze_sq_shield", "Bronze sq shield", bronze, S.SHIELD, 0, 0, 6, 1)
        item("logs", "Logs", (0.55, 0.38, 0.2), value=4)
        item("oak_logs", "Oak logs", (0.62, 0.45, 0.25), value=20)
        item("copper_ore", "Copper ore", (0.75, 0.45, 0.3), value=5)
        item("tin_ore", "Tin ore", (0.7, 0.7, 0.68), value=5)
        item("iron_ore", "Iron ore", (0.5, 0.35, 0.3), value=17)
        item("bronze_bar", "Bronze bar", bronze, value=8)
        item("iron_bar", "Iron bar", iron, value=28)
        item("raw_shrimps", "Raw shrimps", (0.9, 0.6, 0.55), value=5)
        item("shrimps", "Shrimps", (0.95, 0.5, 0.35), value=5, heal=3)
        item("burnt_shrimp", "Burnt shrimp", (0.2, 0.15, 0.1))
        item("raw_beef", "Raw beef", (0.8, 0.3, 0.3), value=1)
        item("cooked_meat", "Cooked meat", (0.55, 0.3, 0.15), value=4, heal=3)
        item("burnt_meat", "Burnt meat", (0.15, 0.1, 0.08))
        item("cowhide", "Cowhide", (0.9, 0.88, 0.8), value=2)
        item("bones", "Bones", (0.95, 0.95, 0.9), value=1)

        task.enter_progress_frame(1, "Resource nodes + recipes")
        K = unreal.ResourceKind
        node("tree", "Tree", K.TREE, "woodcutting", 1, T.AXE, 25, 0.45, 1.0, 8, "logs", (0.25, 0.5, 0.2))
        node("oak_tree", "Oak tree", K.TREE, "woodcutting", 15, T.AXE, 37.5, 0.3, 0.125, 14, "oak_logs", (0.3, 0.45, 0.15))
        node("copper_rock", "Copper rocks", K.ROCK, "mining", 1, T.PICKAXE, 17.5, 0.45, 1.0, 4, "copper_ore", (0.75, 0.45, 0.3))
        node("tin_rock", "Tin rocks", K.ROCK, "mining", 1, T.PICKAXE, 17.5, 0.45, 1.0, 4, "tin_ore", (0.7, 0.7, 0.68))
        node("iron_rock", "Iron rocks", K.ROCK, "mining", 15, T.PICKAXE, 35, 0.35, 1.0, 9, "iron_ore", (0.5, 0.3, 0.25))
        node("shrimp_spot", "Fishing spot", K.FISHING_SPOT, "fishing", 1, T.FISHING_NET, 10, 0.5, 0.0, 0, "raw_shrimps", (0.3, 0.55, 0.9))

        ST = unreal.StationType
        recipe("smelt_bronze", "Smelt Bronze bar", ST.FURNACE, "smithing", 1, 6, T.NONE, "bronze_bar", 1, ("copper_ore", 1), ("tin_ore", 1))
        recipe("smelt_iron", "Smelt Iron bar", ST.FURNACE, "smithing", 15, 12, T.NONE, "iron_bar", 1, ("iron_ore", 1))
        recipe("smith_bronze_sword", "Smith Bronze sword", ST.ANVIL, "smithing", 4, 12.5, T.HAMMER, "bronze_sword", 1, ("bronze_bar", 1))
        recipe("smith_iron_sword", "Smith Iron sword", ST.ANVIL, "smithing", 19, 25, T.HAMMER, "iron_sword", 1, ("iron_bar", 1))
        recipe("smith_bronze_platebody", "Smith Bronze platebody", ST.ANVIL, "smithing", 18, 62.5, T.HAMMER, "bronze_platebody", 1, ("bronze_bar", 5))
        recipe("cook_shrimps", "Cook Shrimps", ST.RANGE, "cooking", 1, 30, T.NONE, "shrimps", 1, ("raw_shrimps", 1),
               fail="burnt_shrimp", fail_chance=0.45, stop_fail=34)
        recipe("cook_meat", "Cook Meat", ST.RANGE, "cooking", 1, 30, T.NONE, "cooked_meat", 1, ("raw_beef", 1),
               fail="burnt_meat", fail_chance=0.45, stop_fail=34)

        task.enter_progress_frame(1, "NPCs + mobs")
        npc("hilda", "Granny Hilda", (0.55, 0.35, 0.55), "Mind the stove, dearie.", "The cows are just past the trees.")
        npc("osric", "Osric the Banker", (0.25, 0.3, 0.5), "Welcome to the Bank of Larkhaven.", "Your items are safe with us.")
        npc("doric", "Brena the Smith", (0.4, 0.3, 0.25), "Copper and tin make bronze. Smelt them at the furnace.", "Bring a hammer to the anvil.")
        mob("cow", "Cow", 2, 8, 1, False, (0.9, 0.88, 0.82), (0.9, 1.1, 1.7),
            drop("raw_beef", 1, 1, 1.0), drop("cowhide", 1, 1, 1.0), drop("bones", 1, 1, 1.0))
        mob("goblin", "Goblin", 2, 5, 1, True, (0.35, 0.6, 0.25), (0.6, 1.1, 0.5),
            drop("bones", 1, 1, 1.0), drop("coins", 3, 15, 0.6), drop("bronze_sq_shield", 1, 1, 0.03))

        task.enter_progress_frame(1, "Quest: Granny's Pie")
        Q = unreal.QuestStepType
        q = load_or_create(f"{D}/Quests/Quest_GrannysPie", unreal.QuestDef)
        xp = unreal.XpReward()
        xp.set_editor_property("skill", skills["cooking"])
        xp.set_editor_property("xp", 1200.0)
        set_props(q, id="hilda_pie", display_name="Granny's Pie", giver=npcs["hilda"], difficulty="Novice", quest_points=1,
                  intro_lines=["Oh, hello dearie! My grandchildren are visiting and my old hands can't manage the stove any more.",
                               "Could you cook me some meat and shrimps? The range is in the general store."],
                  end_line="Smells just like I used to make! Here, take some of my shrimps too.",
                  steps=[
                      step(Q.TALK, "Talk to Granny Hilda west of the town square.", npc_id="hilda"),
                      step(Q.KILL, "Kill cows in the field for their beef.", mob_id="cow", n=2),
                      step(Q.GATHER, "Net raw shrimps at the pond.", item_id="raw_shrimps", n=2),
                      step(Q.DELIVER, "Cook the beef on the range and bring Hilda 2 cooked meat.", npc_id="hilda", item_id="cooked_meat", n=2),
                      step(Q.DELIVER, "Bring Hilda 2 cooked shrimps.", npc_id="hilda", item_id="shrimps", n=2),
                  ],
                  xp_rewards=[xp], item_rewards=[amount("shrimps", 5)], coins=300)
        db_lists["quests"].append(q)

        task.enter_progress_frame(1, "GameDatabase + save")
        db = load_or_create(f"{D}/DA_GameDatabase", unreal.GameDatabase)
        for k, v in db_lists.items():
            db.set_editor_property(k, v)
        for group in (skills, items, nodes, npcs, mobs):
            for a in group.values():
                save(a)
        for a in db_lists["recipes"] + db_lists["quests"]:
            save(a)
        save(db)

    unreal.log(f"[Gielinor] Starter data: {len(items)} items, {len(skills)} skills, {len(nodes)} nodes, "
               f"{len(db_lists['recipes'])} recipes, {len(npcs)} npcs, {len(mobs)} mobs, {len(db_lists['quests'])} quest.")
    return db


if __name__ == "__main__":
    run()
