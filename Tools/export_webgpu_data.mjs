// Exports Gielinor Lite (WebGPU branch) game data to Content/Data/*.json for the Unreal content loader.
// Usage: node Tools/export_webgpu_data.mjs <path-to-Gielinor-Lite/js> [outDir]
import fs from 'fs'; import path from 'path'; import os from 'os';
const jsDir = path.resolve(process.argv[2] || '../Gielinor-Lite/js');
const out = path.resolve(process.argv[3] || 'Content/Data');
fs.mkdirSync(out, { recursive: true });
// Copy the web js into a temp dir next to a generated three.js stub (world.js imports three; we only read constants).
const tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'gielinor-export-'));
const src = path.join(tmp, 'js');
fs.cpSync(jsDir, src, { recursive: true });
{
  const names = new Set();
  for (const f of fs.readdirSync(src).filter(f => f.endsWith('.js'))) {
    const t = fs.readFileSync(path.join(src, f), 'utf8');
    for (const m of t.matchAll(/import\s*\{([^}]*)\}\s*from\s*['"]three[^'"]*['"]/g))
      for (const n of m[1].split(',')) { const k = n.trim().split(/\s+as\s+/)[0].trim(); if (/^[A-Za-z_$][\w$]*$/.test(k)) names.add(k); }
    for (const m of t.matchAll(/THREE\.([A-Za-z_$][\w$]*)/g)) names.add(m[1]);
  }
  const dir = path.join(tmp, 'node_modules', 'three');
  fs.mkdirSync(dir, { recursive: true });
  fs.writeFileSync(path.join(dir, 'package.json'), JSON.stringify({ name: 'three', type: 'module', main: 'index.js', exports: { '.': './index.js', './*': './index.js' } }));
  const head = 'function F(){return new Proxy(F,h)};const h={get:(t,k)=>k===Symbol.toPrimitive?(()=>0):k===Symbol.iterator?function*(){}:P,apply:()=>P,construct:()=>P};const P=new Proxy(F,h);\nexport default P;\n';
  fs.writeFileSync(path.join(dir, 'index.js'), head + [...names].sort().map(n => `export const ${n}=P;`).join('\n') + '\n');
}
const I = async m => import('file://' + path.join(src, m + '.js'));
const items = await I('items'), gp = await I('gameplay'), sk = await I('skills'), pr = await I('prayer'),
  po = await I('potions'), qs = await I('quests'), ab = await I('abilities'), ps = await I('playerState'), sv = await I('save');
// Aerondel expansion: these modules register their items/monsters into ITEMS / MONSTER_TYPES at import time.
const rg = await I('relicgear'), rd = await I('relicdata'), gt = await I('gates'), dv = await I('delving'), rc = await I('relicry'), reg = await I('regiondata');
const clean = v => JSON.parse(JSON.stringify(v, (k, x) => (typeof x === 'function' ? undefined : x instanceof Set ? [...x] : x)));
const w = (n, v) => fs.writeFileSync(path.join(out, n + '.json'), JSON.stringify(clean(v), null, 1));
w('skills', { skills: sk.SKILLS, perks: sk.PERKS, perkLevels: sk.PERK_LEVELS, perkCaps: sk.PERK_CAPS, milestones: sk.MILESTONES });
// UE-only extras: the web draws uncut gem icons but never defines the items; the Unreal port defines them for treasure digs.
const UE_EXTRA_ITEMS = {"Uncut sapphire": {"value": 25, "type": "resource"}, "Uncut emerald": {"value": 50, "type": "resource"}, "Uncut ruby": {"value": 100, "type": "resource"}, "Uncut diamond": {"value": 200, "type": "resource"}};
for (const [k, v] of Object.entries(UE_EXTRA_ITEMS)) if (!items.ITEMS[k]) items.ITEMS[k] = v;
w('items', { items: items.ITEMS, metals: items.METALS, sets: items.SETS, starterTools: items.STARTER_TOOLS, toolTypes: items.TOOL_TYPES, healScale: items.HEAL_SCALE });
w('recipes', { recipes: items.RECIPES, potionRecipes: items.POTION_RECIPES });
w('gathering', { trees: gp.TREES, rocks: gp.ROCKS, fishSpots: gp.FISH_SPOTS, herbPatches: gp.HERB_PATCHES });
w('mobs', { monsters: gp.MONSTER_TYPES, loot: gp.LOOT_TABLES, rareDrops: gp.RARE_DROPS, bounties: gp.BOUNTIES });
w('combat', { melee: gp.MELEE_STYLES, ranged: gp.RANGED_STYLES, spells: gp.SPELLS, spellTicks: gp.SPELL_TICKS, abilities: ab.ABILITIES, adren: ab.ADREN, drCap: gp.DR_CAP, tickMs: gp.TICK_MS });
w('activities', { agilityCourses: gp.AGILITY_COURSES, agilityObstacles: gp.AGILITY_OBSTACLES, thieveTargets: gp.THIEVE_TARGETS, runeAltars: gp.RUNE_ALTARS });
w('prayers', { prayers: pr.PRAYERS }); w('potions', { potions: po.POTIONS, potionTicks: po.POTION_TICKS });
w('shops', { shops: gp.SHOPS, general: gp.SHOP_STOCK, tools: gp.TOOL_STOCK, herbs: gp.HERB_STOCK, armour: gp.ARMOUR_STOCK, weapons: gp.WEAPON_STOCK, magic: gp.MAGIC_STOCK, magicBasic: gp.MAGIC_STOCK_BASIC, runes: gp.RUNE_STOCK });
// UE-only: the web places these feature NPCs in world.js code; the port needs them as NpcDefs with a service and a spot.
const UE_EXTRA_NPCS = {"shipwright": {"name": "Shipwright Halvard", "examine": "He builds ships for anyone with the coin and the nerve.", "service": "shipwright", "x": -77, "z": 70, "tint": "#6a4a2a"}, "corvane": {"name": "Huntmaster Corvane", "examine": "He hands out hunts and keeps the tally.", "service": "tasks", "x": -70, "z": 58, "tint": "#4a3a2a"}, "deedwarden": {"name": "Deedwarden Oswin", "examine": "He sells Hearthstead deeds behind Larkhaven.", "service": "hearth", "x": -14, "z": 20, "tint": "#5a4a6a"}, "sefa": {"name": "Arenamaster Sefa", "examine": "She runs the Proving Grounds.", "service": "arena", "x": 128, "z": -34, "tint": "#7a2a2a"}, "ysolde": {"name": "Apothecary Ysolde", "examine": "She treats scars and quirks.", "service": "quirks", "x": 9, "z": 18, "tint": "#4a6a5a"}, "maelis": {"name": "Writkeeper Maelis", "examine": "She posts the daily writs and the weekly Hearthboard.", "service": "writs", "x": -8, "z": 19, "tint": "#6a5a3a"}, "hesk": {"name": "Warcaptain Hesk", "examine": "A scarred captain raising a warband.", "service": "siege", "x": -50, "z": -88, "tint": "#6a3a2a"}, "brannoch": {"name": "Factor Brannoch", "examine": "The Charter Company factor in Thistlewell.", "service": "charter", "x": 37, "z": 84, "tint": "#5a4a7a"}, "ysmay": {"name": "Elder Ysmay", "examine": "She speaks for the Thistlewell commons.", "service": "charter", "x": 44, "z": 98, "tint": "#4a6a3a"}, "hale": {"name": "Roostmaster Hale", "examine": "He keeps the Skyrooks and their flight routes.", "service": "roost", "x": 14, "z": 20, "tint": "#6a5a3a"}, "corvin": {"name": "Delvemaster Corvin", "examine": "He sends parties into the Delves of Three.", "service": "delves", "x": -46, "z": -80, "tint": "#3a4a3a"}, "idris": {"name": "Pathwarden Idris", "examine": "He teaches the Combat Paths.", "service": "paths", "x": -54, "z": -76, "tint": "#5a5a6a"}, "mama_sorrel": {"name": "Mama Sorrel", "examine": "A hex-witch who brews runes from bog and bone.", "service": "hexer", "x": -88, "z": 54, "tint": "#3a5a2a"}, "chronicler": {"name": "Chronicler Corwin", "examine": "He writes down the stories worth keeping.", "service": "saga", "x": 4, "z": 7, "tint": "#4a3a5a"}, "deed_sign": {"name": "Hearthstead plot sign", "examine": "\"Hearthstead plots for sale. Ask Deedwarden Oswin.\"", "service": "hearth", "x": -12, "z": 24, "tint": "#8a6a3a"}, "harbour_g": {"name": "Harbourmaster Pell", "examine": "He watches the Gullrest moorings.", "service": "harbour", "x": -68.7, "z": 950.9, "tint": "#2a3a5a"}, "chandler": {"name": "Chandler Wenna", "examine": "She sells rope, tar and spades.", "service": "shop:chandlery", "x": -76.5, "z": 956.1, "tint": "#c8a06a"}, "gullfisher": {"name": "Old Gaffer Brune", "examine": "Saltgrave were a captain once, a cruel one. The sea took him and gave him back wrong.", "service": "say", "x": -62.2, "z": 962.6, "tint": "#5a6a7a"}, "harbour_b": {"name": "Harbourmistress Ines", "examine": "She keeps Brineholt harbour.", "service": "harbour", "x": 71.6, "z": 1028.8, "tint": "#4a2a3a"}, "dealer_crown": {"name": "Dealer Quill", "examine": "He runs the Crown & Anchor table.", "service": "crown", "x": 80, "z": 1037.2, "tint": "#7a1a1a"}, "dealer_tides": {"name": "Dealer Maris", "examine": "She deals Twenty-One Tides.", "service": "tides", "x": 86.0, "z": 1036.0, "tint": "#1a4a5a"}, "gunsmith": {"name": "Gunsmith Harrow", "examine": "He sells flintlocks, shot and cutlasses.", "service": "shop:armsBrine", "x": 82.4, "z": 1022.8, "tint": "#3a3a3a"}};
for (const [k, v] of Object.entries(UE_EXTRA_NPCS)) if (!qs.NPC_INFO[k]) qs.NPC_INFO[k] = v;
w('quests', { quests: qs.QUESTS, npcs: qs.NPC_INFO, towns: qs.QUEST_TOWNS, qpMilestones: qs.QP_MILESTONES });
w('relicry', { sites: rd.SITES, reforge: rd.REFORGE, gates: gt.GATES, relicGear: rg.RELIC_GEAR.map(g => g[0]), delveGear: rg.DELVE_GEAR.map(g => g[0]), delveSizes: dv.SIZES,
  dungeonOrigin: (await I('dungeon')).DUNGEON_ORIGIN, room: { size: 16, step: 22, wallH: 3.4 } });
w('player', { invSize: ps.INV_SIZE, defaults: ps.DEFAULT_SETTINGS, saveVersion: sv.SAVE_VERSION });
// World layout (world.js imports three.js; only its exported constants are read).
try {
  const wd = await I('world');
  let places = {};
  try { places = (await I('quests2')).PLACES || {}; } catch (e) { console.warn('places skipped :', e.message); }
  w('world', { worldHalf: wd.WORLD_HALF, spawn: wd.SPAWN, lake: wd.LAKE, towns: wd.TOWNS, paths: wd.PATHS, buildings: wd.BUILDINGS, guilds: wd.GUILDS, landmarks: wd.LANDMARKS, ponds: wd.PONDS, cave: wd.CAVE, places, regions: reg.REGION_DATA, biomes: reg.BIOMES });
} catch (e) { console.warn('world.json skipped :', e.message); }
fs.rmSync(tmp, { recursive: true, force: true });
console.log('exported to', out);
