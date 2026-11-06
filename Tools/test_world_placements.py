"""Offline tests for Content/Python/world_placements.py (no Unreal). Run: python3 Tools/test_world_placements.py"""
import json, math, os, sys, types, unittest
from unittest import mock

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "Content", "Python"))
import world_placements as wp  # noqa: E402

DATA = os.path.join(ROOT, "Content", "Data")
P = json.load(open(os.path.join(DATA, "placements.json"), encoding="utf-8"))
W = json.load(open(os.path.join(DATA, "world.json"), encoding="utf-8"))


class Plan(unittest.TestCase):
    def test_full_keeps_everything(self):
        n, m = wp.plan(P)
        self.assertEqual(len(n), len(P["nodes"])); self.assertEqual(len(m), len(P["mobs"]))
        self.assertGreater(len([x for x in n if x["type"] == "fish"]), 0)

    def test_small_is_capped(self):
        n, m = wp.plan(P, small=True)
        self.assertLess(len(n), len(P["nodes"]))
        for t in {x["type"] for x in n}:
            self.assertLessEqual(len([x for x in n if x["type"] == t]), 10)

    def test_every_type_has_a_look(self):
        for x in P["nodes"]:
            self.assertIn(x["type"], wp.NODE_LOOK)


class Shore(unittest.TestCase):
    def test_blocker_per_water(self):
        self.assertEqual(len(wp.shore_blockers(W)), (1 if W.get("lake") else 0) + len(W.get("ponds", [])))

    def test_fishing_reachable_from_shore(self):
        for name, x, z, dia, h in wp.shore_blockers(W):
            for f in [n for n in P["nodes"] if n["type"] == "fish"]:
                d = math.hypot(f["x"] - x, f["z"] - z)
                if d <= dia / 2:  # spot is over this water: distance from the blocker edge must be within cast range
                    self.assertLess((dia / 2 - d) * 100 + 60, wp.FISH_RANGE_CM, (name, f))

    def test_nothing_solid_in_water(self):
        for name, x, z, dia, h in wp.shore_blockers(W):
            for e in [n for n in P["nodes"] if n["type"] != "fish"] + P["mobs"]:
                self.assertGreater(math.hypot(e["x"] - x, e["z"] - z), dia / 2, (name, e))


class EditorBuild(unittest.TestCase):
    """Runs build() against a fake unreal module and checks what would be spawned."""
    def test_build_spawns_with_ids(self):
        spawned = []

        class Obj:
            def __init__(self, cls): self.cls, self.props, self.static_mesh_component = cls, {}, mock.MagicMock()
            def set_editor_property(self, k, v): self.props[k] = v
            def get_editor_property(self, k): return mock.MagicMock()
            def __getattr__(self, k): return mock.MagicMock()

        fake = mock.MagicMock()
        fake.Paths.project_content_dir.return_value = ROOT + "/Content/"
        bst = types.SimpleNamespace(CONE="c", SPHERE="s", CYL="y", V=lambda x, y, z: (x, y, z), mesh=lambda *a: None,
                                    set_mesh=lambda *a, **k: None, label=lambda a, *r: a,
                                    spawn=lambda cls, loc, rot=None: spawned.append(Obj(cls)) or spawned[-1])
        util = types.SimpleNamespace(mat=lambda *a: None)
        with mock.patch.object(wp, "unreal", fake), mock.patch.dict(sys.modules, {"build_starter_town": bst, "aerondel_util": util}):
            n, m = wp.build(W)
        self.assertEqual(n, len(P["nodes"])); self.assertEqual(m, len(P["mobs"]))
        ids = [o.props.get("definition_id") for o in spawned if o.cls is fake.ResourceNode]
        self.assertTrue(all(ids)); self.assertEqual(len(ids), n)
        self.assertEqual(len([o for o in spawned if o.cls is fake.AerondelMob and o.props.get("definition_id")]), m)


if __name__ == "__main__":
    unittest.main(verbosity=1)
