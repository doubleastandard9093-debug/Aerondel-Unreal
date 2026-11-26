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

    def test_rigs_json_writes_pitch_yaw_roll(self):
        self.assertIn('"pitch": rmap.get("pitch", 0)', self.src)
        self.assertIn('"roll": rmap.get("roll", 0)', self.src)
        self.assertIn('"yaw": rmap.get("yaw", -90)', self.src)

    def test_rig_map_has_rotation_defaults(self):
        rmap = json.loads(RIG_MAP.read_text(encoding="utf-8"))
        self.assertEqual(rmap.get("yaw"), -90)
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
        self.assertIn("BindUprightFix", self.cpp)

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


if __name__ == "__main__":
    unittest.main(verbosity=1)
