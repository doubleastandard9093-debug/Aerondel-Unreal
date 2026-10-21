# Performance: desktop vs Quest standalone

Two very different machines share one project. **Content is authored for the Quest budget first, then scaled up on desktop.** Raising detail later is easy; rescuing frame rate on Quest is not.

| | Desktop / console (PC, PS5, Xbox Series) | Quest 2 / 3 / 3S standalone |
|---|---|---|
| Renderer | Deferred, SM6/DX12 | **Mobile forward** (Vulkan), `r.Mobile.ShadingPath=1` |
| GI / reflections | **Lumen** (software traces; HW RT on Ultra) | Baked or none: sky light + one directional; reflection captures |
| Geometry | **Nanite** for rocks, buildings and cliffs | **No Nanite.** Classic LODs (3–4 levels) with aggressive screen-size thresholds |
| Shadows | **Virtual Shadow Maps** | **One** CSM cascade (≤1024) from the sun, or blob shadows; no shadows from point lights |
| Anti-aliasing | **TSR** | **MSAA 4x** (forward); no TAA (smears in VR) |
| Upscaling | TSR at 67–100 % | None, 100 % eye buffer; **fixed foveated rendering** (High, dynamic) |
| Stereo | Instanced stereo | **Mobile multiview** (one draw for both eyes) |
| Post | Bloom, AO, auto-exposure, fog | Minimal: tonemapper only, no bloom or DoF, and **avoid the PostProcess vignette** (cost); swap the comfort vignette for a mesh-based one on Quest |
| Frame target | 60–120 fps (console 60) | **72 Hz** (Quest 2 baseline), **90 Hz** target on Quest 3; app-side SpaceWarp as a fallback |
| Draw calls | 2–5k fine | **≤ 150–250 per eye-pair** (multiview), as low as possible |
| Triangles | Millions (Nanite) | **≤ 750k–1M visible** on Quest 3, ~500k on Quest 2 |
| Textures | BC7 / BC5 | **ASTC** (6x6 for albedo, 4x4 for normals), mipped, ≤ 2k |
| Materials | Layered, POM | ≤ ~150 instructions; no masked overdraw-heavy foliage; use opaque cards with a few alpha-test leaves |
| Lights | Many movable | 1 movable directional + ≤ 4 cheap local lights (no shadows) |

## Rules the scaffold already follows
- **Instancing:** greybox trees and rocks are separate actors for clarity. When real meshes arrive, convert repeated props (trees, rocks, fences, grass) to **HISM / Instanced Static Mesh** or Foliage, which turns hundreds of draws into one per mesh+material.
- **Ticking:** mobs and player combat tick at 10 Hz, the bank chest at 4 Hz, and resource/station loops are timers, not ticks.
- **Interior lights** don't cast shadows. The furnace glow is a plain point light.
- **Android ini** caps the view distance (0.6), foliage density (0.4), material quality (0), one shadow cascade, MSAA 4x and multiview.
- **Startup tier:** desktop starts on **Low** (the same rule as the web build) and Android forces **Quest**.

## Culling and LOD checklist for real content
1. Set **cull distances** with a Cull Distance Volume: small props at 30–50 m, medium at 80 m. Quest at about 60 % of desktop.
2. Use **HLOD / World Partition** for the open world once it grows past the starter town. Stream cells and keep interiors as separate levels or Level Instances.
3. Give every non-Nanite mesh **LOD0–LOD3** and enable *auto LOD* in the static mesh editor. Desktop can keep Nanite enabled while Quest uses fallback meshes, since Nanite meshes carry a generated fallback; set its triangle percentage.
4. **Occlusion:** software occlusion on Quest (`r.Mobile.AllowSoftwareOcclusion=1`) for dense towns. Precomputed visibility volumes in interiors.
5. Profile on the device with **RenderDoc for Oculus**, `stat unit`, `stat rhi` and the **OVR Metrics Tool**. Desktop uses **Unreal Insights**.

## Console note
Consoles (PS5 / Xbox Series) use the desktop path (Lumen, Nanite, VSM, TSR) at 60 fps. Treat **High** as the console baseline and Ultra as PC-only. Console SDK access needs a developer agreement with Sony or Microsoft (see NEXT_STEPS).
