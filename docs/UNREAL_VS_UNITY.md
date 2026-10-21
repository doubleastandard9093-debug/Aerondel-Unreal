# Unreal vs Unity for Gielinor

Aaron's goals: a **console release**, **VR**, and the **highest graphics ceiling** (RS3 quality and beyond), single-player first.

| Goal | Unreal Engine 5.6+ | Unity 6 (URP/HDRP) |
|---|---|---|
| **Graphics ceiling** | **Best in class out of the box:** Lumen GI and reflections, Nanite geometry, Virtual Shadow Maps, TSR, MetaHuman, PCG and World Partition for big open worlds | HDRP can look great but needs more custom work; URP (needed for Quest) has a lower ceiling. Unity is phasing HDRP into a unified renderer, so its long-term path is in flux |
| **Console** | Ships many AAA and indie console titles. Console support comes after Sony/Microsoft developer approval. **5 % royalty after $1M gross per product** (no per-seat fee) | Strong console support (also after platform approval). Unity Pro seat cost applies above the revenue threshold; no runtime royalty |
| **VR (Quest standalone)** | Works well (OpenXR, Meta XR plugin, mobile forward, multiview), but you must design for Quest from the start: no Lumen or Nanite, and a strict draw budget. Builds are heavier and iteration is slower | **Most Quest titles ship on Unity**. Lighter runtime, faster Android iteration, and the most mature Meta SDK samples |
| **VR (PC)** | Excellent, and the full desktop renderer runs in VR on strong GPUs | Good |
| **Single dev / iteration speed** | C++ compile times; Blueprints help. The editor is heavier (needs 32–64 GB RAM ideally) | C# with fast iteration; lighter editor |
| **Open world tooling** | World Partition, HLOD, PCG and Landscape are built in and battle-tested | Terrain plus third-party tools or your own streaming |
| **Cost** | Free until $1M gross per product, then 5 % | Personal free under $200k; Pro seat above |

## Recommendation
- If **console + top-end graphics** is the priority (and the web game already covers mobile), **lead with Unreal**. It gives the biggest jump towards "RS3 and beyond" for the least custom rendering work, and console pipelines are standard.
- If **standalone Quest is the main VR target**, Unity is the safer engine for that SKU. Unreal on Quest is fine but needs discipline: this scaffold's Quest tier, the forward renderer and HISM everywhere.
- **Practical path:** keep both scaffolds while prototyping. They share the same data model and interaction design, so content definitions port across. Pick one after the first playable on real hardware, meaning a PC build at High and a Quest 3 build at 72/90 Hz. Don't maintain both long term, because doubling every feature isn't sustainable for a small team.
