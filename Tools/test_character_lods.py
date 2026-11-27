"""Offline tests for Content/Python/build_character_lods.py. Run: python3 Tools/test_character_lods.py"""
import os, sys, unittest
sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "Content", "Python"))
import build_character_lods as b  # noqa: E402

RIGS = {"player": "adventurer", "rigs": {"adventurer": {"mesh": "/Game/Aerondel/Models/Rigged/adventurer/adventurer.adventurer"},
                                          "matt": {"mesh": "/Game/Aerondel/Models/Rigged/matt/matt.matt"}}}


class Lods(unittest.TestCase):
    def test_player_skipped(self):
        skip = b.player_mesh_paths(RIGS)
        todo = b.plan(["/Game/Aerondel/Models/Rigged/adventurer/adventurer.adventurer", "/Game/Aerondel/Models/Rigged/matt/matt.matt"], {}, skip)
        self.assertEqual(todo, ["/Game/Aerondel/Models/Rigged/matt/matt.matt"])

    def test_already_lodded_left_alone(self):
        self.assertEqual(b.plan(["/a.a"], {"/a.a": 3}, set()), [])

    def test_no_player_mapping(self):
        self.assertEqual(b.player_mesh_paths({}), set())


if __name__ == "__main__":
    unittest.main(verbosity=1)
