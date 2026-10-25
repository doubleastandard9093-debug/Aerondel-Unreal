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
const clean = v => JSON.parse(JSON.stringify(v, (k, x) => (typeof x === 'function' ? undefined : x instanceof Set ? [...x] : x)));
const w = (n, v) => fs.writeFileSync(path.join(out, n + '.json'), JSON.stringify(clean(v), null, 1));
w('skills', { skills: sk.SKILLS, perks: sk.PERKS, perkLevels: sk.PERK_LEVELS, perkCaps: sk.PERK_CAPS, milestones: sk.MILESTONES });
w('items', { items: items.ITEMS, metals: items.METALS, sets: items.SETS, starterTools: items.STARTER_TOOLS, toolTypes: items.TOOL_TYPES, healScale: items.HEAL_SCALE });
w('recipes', { recipes: items.RECIPES, potionRecipes: items.POTION_RECIPES });
w('gathering', { trees: gp.TREES, rocks: gp.ROCKS, fishSpots: gp.FISH_SPOTS, herbPatches: gp.HERB_PATCHES });
w('mobs', { monsters: gp.MONSTER_TYPES, loot: gp.LOOT_TABLES, rareDrops: gp.RARE_DROPS, bounties: gp.BOUNTIES });
w('combat', { melee: gp.MELEE_STYLES, ranged: gp.RANGED_STYLES, spells: gp.SPELLS, spellTicks: gp.SPELL_TICKS, abilities: ab.ABILITIES, adren: ab.ADREN, drCap: gp.DR_CAP, tickMs: gp.TICK_MS });
w('activities', { agilityCourses: gp.AGILITY_COURSES, agilityObstacles: gp.AGILITY_OBSTACLES, thieveTargets: gp.THIEVE_TARGETS, runeAltars: gp.RUNE_ALTARS });
w('prayers', { prayers: pr.PRAYERS }); w('potions', { potions: po.POTIONS, potionTicks: po.POTION_TICKS });
w('shops', { shops: gp.SHOPS, general: gp.SHOP_STOCK, tools: gp.TOOL_STOCK, herbs: gp.HERB_STOCK, armour: gp.ARMOUR_STOCK, weapons: gp.WEAPON_STOCK, magic: gp.MAGIC_STOCK, magicBasic: gp.MAGIC_STOCK_BASIC, runes: gp.RUNE_STOCK });
w('quests', { quests: qs.QUESTS, npcs: qs.NPC_INFO, towns: qs.QUEST_TOWNS, qpMilestones: qs.QP_MILESTONES });
w('player', { invSize: ps.INV_SIZE, defaults: ps.DEFAULT_SETTINGS, saveVersion: sv.SAVE_VERSION });
// World layout (world.js imports three.js; only its exported constants are read).
try {
  const wd = await I('world');
  w('world', { worldHalf: wd.WORLD_HALF, spawn: wd.SPAWN, lake: wd.LAKE, towns: wd.TOWNS, paths: wd.PATHS, buildings: wd.BUILDINGS, guilds: wd.GUILDS, landmarks: wd.LANDMARKS, ponds: wd.PONDS, cave: wd.CAVE });
} catch (e) { console.warn('world.json skipped :', e.message); }
fs.rmSync(tmp, { recursive: true, force: true });
console.log('exported to', out);
