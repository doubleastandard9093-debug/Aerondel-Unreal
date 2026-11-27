// Static check for Content/Data/placements.json against the other exported data. No Unreal needed.
// Usage: node Tools/check_placements.mjs [dataDir]   (exit 1 on any error)
import fs from 'fs'; import path from 'path';
const dir = path.resolve(process.argv[2] || 'Content/Data');
const J = n => JSON.parse(fs.readFileSync(path.join(dir, n + '.json'), 'utf8'));
const ueId = n => String(n).replace(/'/g, '').toLowerCase().replace(/[^a-z0-9]+/g, '_').replace(/^_+|_+$/g, '');
const P = J('placements'), G = J('gathering'), M = J('mobs'), W = J('world');
const defs = new Set([
  ...Object.keys(G.trees || {}).map(k => 'tree_' + ueId(k)), ...Object.keys(G.rocks || {}).map(k => 'rock_' + ueId(k)),
  ...Object.keys(G.fishSpots || {}).map(k => 'fish_' + ueId(k)), ...Object.keys(G.herbPatches || {}).map(k => 'herb_' + ueId(k))]);
const mobs = new Set(Object.keys(M.monsters || {}).map(ueId));
const errs = [], warn = [];
const lim = (W.worldHalf || 620) + 600; // the sea and islands lie beyond worldHalf (Gullrest is at z ~ 950)
const waters = [W.lake, ...(W.ponds || [])].filter(Boolean);
const count = {};
for (const n of P.nodes || []) {
  count[n.def] = (count[n.def] || 0) + 1;
  if (!defs.has(n.def)) errs.push(`node ${n.def} at ${n.x},${n.z}: no such gathering def`);
  if (!Number.isFinite(n.x) || !Number.isFinite(n.z) || Math.abs(n.x) > lim || Math.abs(n.z) > lim) errs.push(`node ${n.def}: bad position ${n.x},${n.z}`);
  if (n.type === 'fish' && !waters.some(w => Math.hypot(n.x - w.x, n.z - w.z) <= w.r + 3)) warn.push(`fishing spot ${n.def} at ${n.x},${n.z} is not by the lake/ponds (coast or river)`);
  if (n.type !== 'fish' && waters.some(w => Math.hypot(n.x - w.x, n.z - w.z) < w.r)) errs.push(`${n.def} at ${n.x},${n.z} is inside water`);
}
const mc = {};
for (const m of P.mobs || []) {
  mc[m.def] = (mc[m.def] || 0) + 1;
  if (!mobs.has(m.def)) errs.push(`mob ${m.def} (${m.type}) at ${m.x},${m.z}: no such monster in mobs.json`);
  if (!Number.isFinite(m.x) || !Number.isFinite(m.z) || Math.abs(m.x) > lim || Math.abs(m.z) > lim) errs.push(`mob ${m.def}: bad position ${m.x},${m.z}`);
  if (waters.some(w => Math.hypot(m.x - w.x, m.z - w.z) < w.r)) errs.push(`mob ${m.def} at ${m.x},${m.z} is inside water`);
}
const fish = (P.nodes || []).filter(n => n.type === 'fish');
console.log(`nodes ${(P.nodes || []).length} (${Object.keys(count).length} kinds), mobs ${(P.mobs || []).length} (${Object.keys(mc).length} kinds), fishing spots ${fish.length}`);
console.log('node kinds:', JSON.stringify(count)); console.log('mob kinds:', JSON.stringify(mc));
for (const w of warn) console.log('WARN', w);
for (const e of errs) console.log('ERROR', e);
if (!(P.nodes || []).length || !(P.mobs || []).length || !fish.length) { console.log('ERROR empty placements'); process.exit(1); }
process.exit(errs.length ? 1 : 0);
