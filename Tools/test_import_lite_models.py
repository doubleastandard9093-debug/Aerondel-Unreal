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
        cls.feet_aabb = rig.mesh_vertex_aabb(cls.gltf, cls.blob, "Adventurer_Feet")

    def test_baked_character_stands_in_gltf_y_up_with_feet_on_the_ground(self):
        self.assertIsNotNone(self.aabb)
        mn, mx = self.aabb
        height = float(mx[1] - mn[1])
        self.assertGreater(height, 1.5)
        self.assertLess(height, 2.1)
        self.assertGreater(float(mn[1]), -0.05)
        self.assertLess(float(mx[1]), 2.1)
        # Feet mesh is a pair of boots at the origin, not a 2 m-wide upright slab.
        self.assertIsNotNone(self.feet_aabb)
        fmin, fmax = self.feet_aabb
        self.assertGreater(float(fmin[1]), -0.05)
        self.assertLess(float(fmax[1]), 0.40)
        self.assertLess(float(fmax[0] - fmin[0]), 1.0)
        # Head is above the feet in the bind pose.
        self.assertGreater(self.lm["head"][1], self.lm["feet"][1] + 1.2)

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
        def W(i):
            if worlds[i] is not None:
                return worlds[i]
            import numpy as np
            L = rig._node_local(nodes[i])
            if i == root or parent[i] < 0 or parent[i] != parent[i]:
                pass
            if i == root:
                worlds[i] = L
            elif parent[i] >= 0 and _under_root(i):
                worlds[i] = W(parent[i]) @ L
            else:
                worlds[i] = L
            return worlds[i]
        def _under_root(i):
            while i >= 0:
                if i == root:
                    return True
                i = parent[i]
            return False
        pos = {}
        for i, n in enumerate(nodes):
            if not _under_root(i):
                continue
            M = W(i)
            pos[n.get("name") or str(i)] = (float(M[0, 3]), float(M[1, 3]), float(M[2, 3]))
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



if __name__ == "__main__":
    unittest.main(verbosity=1)
