import fs from 'fs'; import path from 'path'; import { createRequire } from 'module';
const require = createRequire(process.env.M + '/');
const { NodeIO } = require('@gltf-transform/core');
const { ALL_EXTENSIONS } = require('@gltf-transform/extensions');
const { dequantize, textureCompress, unpartition } = require('@gltf-transform/functions');
const { MeshoptDecoder } = require('meshoptimizer');
const sharp = require('sharp'); const validator = require('gltf-validator');
const [src, dst] = process.argv.slice(2);
await MeshoptDecoder.ready;
const io = new NodeIO().registerExtensions(ALL_EXTENSIONS).registerDependencies({ 'meshopt.decoder': MeshoptDecoder });
const files = []; (function walk(d){ for (const f of fs.readdirSync(d)) { const p = path.join(d,f); fs.statSync(p).isDirectory()? walk(p) : f.endsWith('.glb') && files.push(p);} })(src);
let bad = 0;
for (const f of files) {
  const doc = await io.read(f);
  await doc.transform(dequantize(), textureCompress({ encoder: sharp, targetFormat: 'png', formats: /webp/ }));
  for (const e of doc.getRoot().listExtensionsUsed()) if (['EXT_meshopt_compression','EXT_texture_webp','KHR_mesh_quantization','KHR_materials_unlit'].includes(e.extensionName)) e.dispose();
  for (const s of doc.getRoot().listSkins()) s.setSkeleton(null); // optional; invalid 'not a common root' in 5 Quaternius rigs
  for (const t of doc.getRoot().listTextures()) if (t.getMimeType()==='image/webp') console.log('STILL WEBP', f);
  const out = path.join(dst, path.relative(src, f)); fs.mkdirSync(path.dirname(out), {recursive:true});
  await io.write(out, doc);
  const r = await validator.validateBytes(new Uint8Array(fs.readFileSync(out)), { maxIssues: 50 });
  const used = r.info?.extensionsUsed || [];
  if (r.issues.numErrors) { bad++; console.log('INVALID', out, r.issues.messages.filter(m=>m.severity===0).slice(0,3)); }
  if (used.length) console.log('ext', path.relative(dst,out), used.join(','));
}
console.log('files', files.length, 'errors', bad);
