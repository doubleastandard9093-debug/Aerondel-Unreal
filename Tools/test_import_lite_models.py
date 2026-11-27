"""Offline tests for Lite glTF import options and the player-rig safety net.

Run: python3 Tools/test_import_lite_models.py
"""
from __future__ import annotations

import json
import os
import re
import struct
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
IMPORT_PY = ROOT / "Content" / "Python" / "import_lite_models.py"
LIB_CPP = ROOT / "Source" / "Aerondel" / "Models" / "AerondelModelLibrary.cpp"
LIB_H = ROOT / "Source" / "Aerondel" / "Models" / "AerondelModelLibrary.h"
ADVENTURER = ROOT / "Tools" / "ImportSource" / "models_ue" / "rigged" / "adventurer.glb"
RIG_MAP = ROOT / "Tools" / "ImportSource" / "rig_map.json"


def load_glb(path: Path):
    data = path.read_bytes()
    magic, version, length = struct.unpack_from("<III", data, 0)
    if magic != 0x46546C67:
        raise ValueError("not a glb")
    off = 12
    gltf = None
    while off < length:
        clen, ctype = struct.unpack_from("<I4s", data, off)
        chunk = data[off + 8 : off + 8 + clen]
        off += 8 + clen
        if ctype == b"JSON":
            gltf = json.loads(chunk)
    return gltf


def quat_axis_angle_x(q):
    """Return degrees of rotation around X for a nearly-pure-X quaternion (x, y, z, w)."""
    x, y, z, w = q
    import math
    angle = 2 * math.degrees(math.atan2(math.sqrt(x * x + y * y + z * z), w))
    if x < 0:
        angle = -angle
    return angle


class AdventurerGlb(unittest.TestCase):
    def setUp(self):
        self.assertTrue(ADVENTURER.is_file(), ADVENTURER)
        self.gltf = load_glb(ADVENTURER)

    def test_first_animation_is_death_so_t0_must_stay_off(self):
        names = [a.get("name") for a in self.gltf.get("animations") or []]
        self.assertTrue(names, "adventurer.glb has no animations")
        self.assertEqual(names[0].lower(), "death")
        self.assertIn("Idle", names)

    def test_armature_has_minus_90_x_and_scale_100(self):
        armature = next((n for n in self.gltf["nodes"] if n.get("name") == "CharacterArmature"), None)
        self.assertIsNotNone(armature, "CharacterArmature node missing")
        rot = armature.get("rotation")
        scale = armature.get("scale")
        self.assertIsNotNone(rot)
        self.assertEqual(scale, [100, 100, 100])
        angle = quat_axis_angle_x(rot)
        self.assertAlmostEqual(abs(angle), 90, delta=1.0)

    def test_armature_is_not_a_skin_joint(self):
        nodes = self.gltf["nodes"]
        armature_i = next(i for i, n in enumerate(nodes) if n.get("name") == "CharacterArmature")
        for skin in self.gltf.get("skins") or []:
            self.assertNotIn(armature_i, skin.get("joints") or [])


class ImportScript(unittest.TestCase):
    def setUp(self):
        self.src = IMPORT_PY.read_text(encoding="utf-8")

    def test_pipeline_bakes_meshes_and_does_not_use_t0(self):
        # Death is animation[0]; T0-as-ref-pose would rebind to a corpse.
        self.assertIn("use_t0_as_ref_pose=False", self.src)
        self.assertIn("bake_meshes=True", self.src)
        self.assertIn("bake_pivot_meshes=True", self.src)
        self.assertIn("try_auto_select_skeleton=True", self.src)
        self.assertIn("force_front_x_axis=True", self.src)
        self.assertIn("convert_scene=True", self.src)
        self.assertIn("def tune_anims", self.src)
        self.assertRegex(self.src, r"tune_anims\(folder, mesh\)")
        self.assertIn("bake_glb_for_unreal", self.src)

    def test_rigs_json_writes_pitch_yaw_roll(self):
        self.assertIn('"pitch": rmap.get("pitch", 0)', self.src)
        self.assertIn('"roll": rmap.get("roll", 0)', self.src)
        self.assertIn('"yaw": rmap.get("yaw", 0)', self.src)

    def test_rig_map_has_rotation_defaults(self):
        rmap = json.loads(RIG_MAP.read_text(encoding="utf-8"))
        self.assertEqual(rmap.get("yaw"), 0)
        self.assertEqual(rmap.get("pitch"), 0)
        self.assertEqual(rmap.get("roll"), 0)
        self.assertEqual(rmap.get("player"), "adventurer")


    def test_import_points_at_baked_path_and_does_not_fall_back_to_source(self):
        self.assertIn("baked_glb_destination", self.src)
        self.assertIn("Saved/Aerondel/baked_glb", self.src)
        self.assertIn("validate_baked_for_interchange", self.src)
        self.assertIn("not importing the source glb", self.src)
        self.assertNotIn("; importing the source glb", self.src)

    def test_missing_ue58_cvars_are_only_set_if_they_exist(self):
        self.assertIn("_cvar_exists", self.src)
        self.assertIn("if not safe._cvar_exists(k):", self.src)
        safe = (ROOT / "Content" / "Python" / "aerondel_safe.py").read_text(encoding="utf-8")
        self.assertIn("def _cvar_exists(name):", safe)
        for name in (
            "Editor.AsyncAnimationCompilationMaxConcurrency",
            "r.ShaderCompiler.MaxShaderJobs",
            "Interchange.FeatureFlags.Import.ThumbnailGeneration",
        ):
            self.assertIn(name, self.src)  # still listed as desired throttles
            self.assertIn('"%s"' % name, safe)  # known-missing, skipped unless a probe finds them

    def test_scan_rig_warns_when_clips_are_missing(self):
        self.assertIn("no AnimSequence assets found", self.src)
        self.assertIn("scan_paths_synchronous", self.src)

    def test_flush_rendering_commands_is_not_issued_as_a_console_command(self):
        # UE 5.8 has no FlushRenderingCommands cvar; calling it spams FindConsoleObject.
        self.assertNotIn('_cmd("FlushRenderingCommands")', self.src)
        self.assertNotIn("_cmd('FlushRenderingCommands')", self.src)


class CppSafetyNet(unittest.TestCase):
    def setUp(self):
        self.cpp = LIB_CPP.read_text(encoding="utf-8")
        self.hdr = LIB_H.read_text(encoding="utf-8")

    def test_fitrig_returns_bool_and_clear_rig_exists(self):
        self.assertRegex(self.hdr, r"static bool FitRig\(")
        self.assertIn("static void ClearRig(", self.hdr)
        self.assertIn("bool UAerondelModelLibrary::FitRig", self.cpp)
        self.assertIn("void UAerondelModelLibrary::ClearRig", self.cpp)

    def test_mismatched_or_additive_anim_holds_ref_pose(self):
        self.assertIn("AnimUsable", self.cpp)
        self.assertIn("GetAdditiveAnimType()", self.cpp)
        self.assertIn("AAT_None", self.cpp)
        self.assertIn("anim skeleton", self.cpp)
        self.assertIn("HoldRefPose", self.cpp)
        self.assertIn("posed skeleton bunched vs bind pose", self.cpp)
        self.assertIn("AlignBindPose", self.cpp)
        self.assertIn("ignoring legacy yaw -90", self.cpp)
        self.assertIn("LowestFootZ", self.cpp)
        self.assertIn("clip laid the body down", self.cpp)
        self.assertIn("AlignFromMeshBounds", self.cpp)
        self.assertIn("using mesh bounds", self.cpp)
        self.assertIn("degenerate head-feet / bind pose missing", self.cpp)

    def test_unmapped_rig_is_logged_once_then_verbose(self):
        self.assertIn("MissingWarned", self.cpp)
        self.assertIn("later misses for this id are Verbose", self.cpp)
        self.assertIn("UE_LOG(LogTemp, Verbose", self.cpp)
        # Placeholder fallback is at the call sites
        char = (ROOT / "Source" / "Aerondel" / "Player" / "AerondelCharacter.cpp").read_text(encoding="utf-8")
        self.assertIn("ClearRig", char)
        self.assertIn("PlaceholderBody->SetVisibility(true)", char)
        npc = (ROOT / "Source" / "Aerondel" / "Interaction" / "NpcTalk.cpp").read_text(encoding="utf-8")
        self.assertIn("ClearRig", npc)
        self.assertIn("Mesh->SetVisibility(true)", npc)

    def test_configurable_pitch_yaw_roll(self):
        self.assertIn("TryGetNumberField(TEXT(\"pitch\")", self.cpp)
        self.assertIn("TryGetNumberField(TEXT(\"roll\")", self.cpp)
        self.assertIn("RotationFor", self.cpp)



class BindSpaceBakeAndAlign(unittest.TestCase):
    """Run adventurer.glb through the same bake + alignment math the importer / FitRig use."""

    @classmethod
    def setUpClass(cls):
        import sys
        sys.path.insert(0, str(ROOT / "Content" / "Python"))
        import aerondel_rig_math as rig
        cls.rig = rig
        cls.baked = Path("/tmp/aerondel_test_adventurer_baked.glb")
        rig.bake_glb_for_unreal(ADVENTURER, cls.baked)
        cls.gltf, cls.blob = rig.load_glb(cls.baked)
        cls.lm = rig.landmarks(rig.joint_world_positions(cls.gltf, cls.blob))
        cls.aabb = rig.mesh_vertex_aabb(cls.gltf, cls.blob)
        cls.src_gltf, cls.src_blob = rig.load_glb(ADVENTURER)

    def test_baked_character_stands_in_gltf_y_up_with_feet_on_the_ground(self):
        self.assertIsNotNone(self.aabb)
        mn, mx = self.aabb
        height = float(mx[1] - mn[1])
        self.assertGreater(height, 1.5)
        self.assertLess(height, 2.1)
        self.assertGreater(float(mn[1]), -0.05)
        self.assertLess(float(mx[1]), 2.1)
        # Feet joints sit on the ground, not a 2 m-wide upright slab.
        for name, p in self.rig.joint_world_positions(self.gltf).items():
            if self.rig.is_foot_name(name):
                self.assertGreater(p[1], -0.05, name)
                self.assertLess(p[1], 0.40, name)
        self.assertGreater(self.lm["head"][1], self.lm["feet"][1] + 1.2)
        # After the glTF Y-up -> UE Z-up conversion, head Z is above feet Z.
        head_ue, feet_ue = self.rig.gltf_to_ue(self.lm["head"]), self.rig.gltf_to_ue(self.lm["feet"])
        self.assertGreater(head_ue[2], feet_ue[2] + 1.2)

    def test_alignment_maps_baked_pose_to_head_up_feet_down_facing_x(self):
        rig = self.rig
        lm = self.lm
        head = rig.gltf_to_ue(lm["head"])
        feet = rig.gltf_to_ue(lm["feet"])
        left = rig.gltf_to_ue(lm["left"]) if lm["left"] else None
        right = rig.gltf_to_ue(lm["right"]) if lm["right"] else None
        q = rig.align_bind_pose(head, feet, left, right)
        up = rig.q_rot(q, rig._vnorm(rig._vsub(head, feet))[0])
        self.assertGreater(up[2], 0.98)
        self.assertLess(abs(up[0]), 0.1)
        self.assertLess(abs(up[1]), 0.1)
        head2 = rig.q_rot(q, head)
        feet2 = rig.q_rot(q, feet)
        self.assertGreater(head2[2], feet2[2] + 1.2)
        if left and right:
            across, _ = rig._vnorm(rig._vsub(right, left))
            fwd, _ = rig._vnorm(rig._vcross(across, rig._vnorm(rig._vsub(head, feet))[0]))
            fwd2 = rig.q_rot(q, fwd)
            self.assertGreater(fwd2[0], 0.98)
            self.assertLess(abs(fwd2[2]), 0.1)

    def test_alignment_stands_up_a_lying_unbaked_skeleton(self):
        """Without the armature node (how Interchange may drop it) and after a Y-up conversion,
        head-feet points along +Y — the 'lying on the ground' case. Alignment must stand it up."""
        rig = self.rig
        # Recreate the "Root as top, then UE +90 X" positions from the source file.
        src, _ = rig.load_glb(ADVENTURER)
        nodes = src["nodes"]
        parent = rig.node_parents(nodes)
        root = next(i for i, n in enumerate(nodes) if n.get("name") == "Root")
        # World as if Root has no ancestors.
        worlds = [None] * len(nodes)
        def _under_root(i):
            while i >= 0:
                if i == root:
                    return True
                i = parent[i]
            return False
        def W(i):
            if worlds[i] is not None:
                return worlds[i]
            L = rig._node_local(nodes[i])
            if i == root:
                worlds[i] = L
            elif parent[i] >= 0 and _under_root(i):
                worlds[i] = rig.mat4_mul(W(parent[i]), L)
            else:
                worlds[i] = L
            return worlds[i]
        pos = {}
        for i, n in enumerate(nodes):
            if not _under_root(i):
                continue
            pos[n.get("name") or str(i)] = rig.mat4_translation(W(i))
        lm = rig.landmarks(pos)
        self.assertIsNotNone(lm)
        head = rig.gltf_to_ue(lm["head"])
        feet = rig.gltf_to_ue(lm["feet"])
        left = rig.gltf_to_ue(lm["left"]) if lm["left"] else None
        right = rig.gltf_to_ue(lm["right"]) if lm["right"] else None
        # Before align, up should not be +Z (lying).
        up0, _ = rig._vnorm(rig._vsub(head, feet))
        self.assertLess(abs(up0[2]), 0.5)
        q = rig.align_bind_pose(head, feet, left, right)
        up = rig.q_rot(q, up0)
        head2, feet2 = rig.q_rot(q, head), rig.q_rot(q, feet)
        self.assertGreater(up[2], 0.98)
        self.assertGreater(head2[2], feet2[2])

    def test_legacy_yaw_minus_90_is_not_applied_on_an_already_upright_pose(self):
        """An extra yaw=-90 on a baked, already-upright mesh must not be how FitRig works;
        auto-align alone keeps the head up. Applying the old default would only spin facing."""
        rig = self.rig
        lm = self.lm
        head, feet = rig.gltf_to_ue(lm["head"]), rig.gltf_to_ue(lm["feet"])
        q = rig.align_bind_pose(head, feet, extra_yaw=0.0)
        up = rig.q_rot(q, rig._vnorm(rig._vsub(head, feet))[0])
        self.assertGreater(up[2], 0.98)
        # And the C++ loader must ignore the legacy default so it cannot stack.
        cpp = LIB_CPP.read_text(encoding="utf-8")
        self.assertIn("ignoring legacy yaw -90", cpp)
        self.assertNotIn("Yaw = -90.f", cpp)

    def test_baked_file_is_interchange_legal_one_skin_root_identity_consistent_ibm(self):
        rig, gltf, blob = self.rig, self.gltf, self.blob
        problems = rig.validate_baked_for_interchange(self.baked, require_upright=True)
        self.assertEqual(problems, [], problems)
        self.assertEqual(len(gltf.get("skins") or []), 1)
        roots = rig.scene_root_set(gltf)
        worlds, parent = rig.node_worlds(gltf["nodes"])
        skinned = rig.skinned_mesh_nodes(gltf)
        self.assertTrue(skinned, "no skinned mesh node")
        for i in skinned:
            self.assertIn(i, roots, gltf["nodes"][i].get("name"))
            self.assertLess(parent[i], 0)
            self.assertTrue(rig.is_identity_node(gltf["nodes"][i]), gltf["nodes"][i])
        # One IBM per joint, matching inv(world).
        skin = gltf["skins"][0]
        ibms = rig.ibm_matrices(gltf, blob, skin)
        self.assertEqual(len(ibms), len(skin["joints"]))
        for k, j in enumerate(skin["joints"]):
            expected = rig.mat4_inv(worlds[j])
            self.assertTrue(rig.mat4_close(ibms[k], expected, 1e-3), gltf["nodes"][j].get("name"))

    def test_baked_animations_are_present_and_translations_are_in_the_new_joint_space(self):
        rig, gltf, blob = self.rig, self.gltf, self.blob
        names = [a.get("name") for a in gltf.get("animations") or []]
        self.assertEqual(len(names), len(self.src_gltf.get("animations") or []))
        for need in ("Idle", "Walk", "Run", "Sword_Slash", "Death"):
            self.assertIn(need, names)
        # Death stays first so use_t0_as_ref_pose remains off.
        self.assertEqual(names[0].lower(), "death")
        # Import CLIP patterns would pick idle/walk/run/attack from these names.
        src = IMPORT_PY.read_text(encoding="utf-8")
        self.assertIn('r"^idle$"', src)
        self.assertIn('r"^walk$"', src)
        self.assertIn('r"^run$"', src)
        self.assertIn("sword_slash", src.lower())

        def first_translation(g, b, clip, node_name):
            nodes = g["nodes"]
            ni = next(i for i, n in enumerate(nodes) if n.get("name") == node_name)
            anim = next(a for a in g["animations"] if a.get("name") == clip)
            for ch in anim["channels"]:
                if ch["target"].get("node") == ni and ch["target"]["path"] == "translation":
                    return rig._read_accessor(g, b, anim["samplers"][ch["sampler"]]["output"])[0]
            return None

        # Root was scaled 100; every descendant translation key must be * 100.
        for clip, node in (("Idle", "Body"), ("Walk", "Shoulder.L"), ("Run", "Foot.L")):
            src_t = first_translation(self.src_gltf, self.src_blob, clip, node)
            dst_t = first_translation(gltf, blob, clip, node)
            self.assertIsNotNone(src_t, "%s %s missing on source" % (clip, node))
            self.assertIsNotNone(dst_t, "%s %s missing on bake" % (clip, node))
            for a, b in zip(src_t, dst_t):
                self.assertAlmostEqual(float(b), float(a) * 100.0, delta=1e-3, msg="%s %s" % (clip, node))

    def test_khronos_gltf_validator_accepts_the_baked_adventurer(self):
        """Independent parser/validator: gltf-validator via node, if npm can install it."""
        import json as _json
        import shutil
        import subprocess
        import os
        node = shutil.which("node")
        npm = shutil.which("npm")
        if not node or not npm:
            self.skipTest("node/npm not available")
        cache = Path("/tmp/aerondel_gltf_validator")
        pkg = cache / "node_modules" / "gltf-validator"
        if not pkg.is_dir():
            cache.mkdir(parents=True, exist_ok=True)
            r = subprocess.run(
                [npm, "install", "--prefix", str(cache), "gltf-validator"],
                capture_output=True, text=True, timeout=120,
            )
            if r.returncode != 0:
                self.skipTest("npm install gltf-validator failed: %s" % (r.stderr or r.stdout)[:400])
        js = cache / "validate_one.js"
        js.write_text(
            "const fs = require('fs');\n"
            "const validator = require('gltf-validator');\n"
            "const bytes = new Uint8Array(fs.readFileSync(process.argv[2]));\n"
            "validator.validateBytes(bytes).then((report) => {\n"
            "  process.stdout.write(JSON.stringify(report));\n"
            "}).catch((err) => { console.error(err); process.exit(2); });\n"
        )
        r = subprocess.run(
            [node, str(js), str(self.baked)],
            capture_output=True, text=True, timeout=60,
            cwd=str(cache),
            env={**os.environ, "NODE_PATH": str(cache / "node_modules")},
        )
        self.assertEqual(r.returncode, 0, r.stderr or r.stdout)
        report = _json.loads(r.stdout)
        issues = report.get("issues") or {}
        errors = int(issues.get("numErrors") or 0)
        # Some infos/warnings (unused accessors from the source BIN) are acceptable.
        self.assertEqual(errors, 0, report)


def _lite_rigged_glbs():
    roots = [
        ROOT / "Tools" / "ImportSource" / "models_ue" / "rigged",
        ROOT / "Tools" / "ImportSource" / "models_ue" / "characters",
    ]
    files = []
    for folder in roots:
        if folder.is_dir():
            files.extend(sorted(folder.glob("*.glb")))
    return files


def _run_khronos_validator(glb_path):
    """Return the gltf-validator report dict, or None if node/npm/package are unavailable."""
    import json as _json
    import shutil
    import subprocess
    import os
    node = shutil.which("node")
    npm = shutil.which("npm")
    if not node or not npm:
        return None
    cache = Path("/tmp/aerondel_gltf_validator")
    pkg = cache / "node_modules" / "gltf-validator"
    if not pkg.is_dir():
        cache.mkdir(parents=True, exist_ok=True)
        r = subprocess.run(
            [npm, "install", "--prefix", str(cache), "gltf-validator"],
            capture_output=True, text=True, timeout=120,
        )
        if r.returncode != 0:
            return None
    js = cache / "validate_one.js"
    js.write_text(
        "const fs = require('fs');\n"
        "const validator = require('gltf-validator');\n"
        "const bytes = new Uint8Array(fs.readFileSync(process.argv[2]));\n"
        "validator.validateBytes(bytes).then((report) => {\n"
        "  process.stdout.write(JSON.stringify(report));\n"
        "}).catch((err) => { console.error(err); process.exit(2); });\n"
    )
    r = subprocess.run(
        [node, str(js), str(glb_path)],
        capture_output=True, text=True, timeout=60,
        cwd=str(cache),
        env={**os.environ, "NODE_PATH": str(cache / "node_modules")},
    )
    if r.returncode != 0:
        raise AssertionError("gltf-validator failed on %s: %s" % (glb_path, r.stderr or r.stdout))
    return _json.loads(r.stdout)


class AllLiteRigsBake(unittest.TestCase):
    """Bake every Lite rigged/character glb and enforce Interchange + Khronos rules."""

    @classmethod
    def setUpClass(cls):
        import sys
        sys.path.insert(0, str(ROOT / "Content" / "Python"))
        import aerondel_rig_math as rig
        cls.rig = rig
        cls.files = _lite_rigged_glbs()
        cls.baked = {}
        cls.problems = {}
        outdir = Path("/tmp/aerondel_test_all_lite_baked")
        outdir.mkdir(parents=True, exist_ok=True)
        for src in cls.files:
            dst = outdir / src.name
            rig.bake_glb_for_unreal(str(src), str(dst))
            cls.baked[src.name] = dst
            cls.problems[src.name] = rig.validate_baked_for_interchange(
                str(dst), require_upright=(src.name == "adventurer.glb")
            )

    def test_every_lite_rig_is_present(self):
        self.assertGreaterEqual(len(self.files), 20, self.files)
        names = {p.name for p in self.files}
        for need in ("adventurer.glb", "cow.glb", "orc_enemy.glb", "wolf.glb", "wizard.glb", "yeti.glb", "folk_m_a.glb"):
            self.assertIn(need, names)

    def test_every_baked_glb_passes_interchange_and_animation_target_validation(self):
        failed = {name: probs for name, probs in self.problems.items() if probs}
        self.assertEqual(failed, {}, failed)

    def test_animation_channels_target_in_scene_joints_on_previously_broken_rigs(self):
        """cow/wolf IK bones and orc_enemy/wizard/yeti Head were left under the
        unused armature; Interchange then Ensure-failed NodeUidPtr on their clips."""
        rig = self.rig
        for name, must in (
            ("cow.glb", ("IKBackLeg.L", "IKFrontLeg.R", "Body")),
            ("wolf.glb", ("IKBackLeg.L", "IKFrontLeg.L", "Body")),
            ("orc_enemy.glb", ("Head", "Body")),
            ("wizard.glb", ("Head", "Body")),
            ("yeti.glb", ("Head", "Body")),
        ):
            gltf, _ = rig.load_glb(self.baked[name])
            nodes = gltf["nodes"]
            names = [n.get("name") for n in nodes]
            reach = rig._reachable_nodes(gltf)
            joints = set(gltf["skins"][0]["joints"])
            for bone in must:
                self.assertIn(bone, names, name)
                i = names.index(bone)
                self.assertIn(i, joints, "%s %s" % (name, bone))
                self.assertIn(i, reach, "%s %s not in the scene graph" % (name, bone))
            # At least one clip still targets the formerly-orphaned joint.
            targeted = set()
            for anim in gltf.get("animations") or []:
                for ch in anim.get("channels") or []:
                    ni = (ch.get("target") or {}).get("node")
                    if isinstance(ni, int) and 0 <= ni < len(names):
                        targeted.add(names[ni])
            for bone in must:
                self.assertIn(bone, targeted, "%s: no clip targets %s" % (name, bone))

    def test_khronos_validator_accepts_every_baked_lite_glb(self):
        skipped = 0
        errors = {}
        for name, path in self.baked.items():
            report = _run_khronos_validator(path)
            if report is None:
                skipped += 1
                continue
            nerr = int((report.get("issues") or {}).get("numErrors") or 0)
            if nerr:
                msgs = [m for m in (report.get("issues") or {}).get("messages") or [] if m.get("severity") == 0]
                errors[name] = (nerr, msgs[:5])
        if skipped == len(self.baked):
            self.skipTest("node/npm/gltf-validator not available")
        self.assertEqual(errors, {}, errors)


if __name__ == "__main__":
    unittest.main(verbosity=1)
