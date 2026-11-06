"""Static checks for Source/Aerondel/World/AerondelRelevance.h band table (no Unreal). Run: python3 Tools/test_relevance.py"""
import os, re, unittest
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
H = open(os.path.join(ROOT, "Source/Aerondel/World/AerondelRelevance.h"), encoding="utf-8").read()
B = {m.group(1): (float(m.group(2)), float(m.group(3))) for m in re.finditer(r"case EAerondelTier::(\w+):\s*return \{ ([\d.]+)f, ([\d.]+)f \}", H)}
m = re.search(r"default:\s*return \{ ([\d.]+)f, ([\d.]+)f \}", H); B["Ultra"] = (float(m.group(1)), float(m.group(2)))


def classify(d, tier, pinned=False):
    near, mid = B[tier]
    return "near" if pinned or d <= near else "mid" if d <= mid else "far"


class Bands(unittest.TestCase):
    def test_all_tiers(self):
        self.assertEqual(set(B), {"Quest", "Low", "High", "Ultra"})

    def test_near_before_mid(self):
        for t, (n, md) in B.items():
            self.assertLess(n, md, t)

    def test_low_caps_harder_than_high(self):
        self.assertLess(B["Low"][1], B["High"][1]); self.assertLess(B["High"][1], B["Ultra"][1]); self.assertLessEqual(B["Quest"][1], B["Low"][1])

    def test_interaction_ranges_stay_near(self):
        # fishing range 600 cm, NPC talk ~220 cm, mob aggro/leash well under 30 m: all inside every tier's Near band
        for t, (n, _) in B.items():
            self.assertGreaterEqual(n, 3000, t)

    def test_pinned_is_always_near(self):
        self.assertEqual(classify(1e9, "Low", pinned=True), "near")
        self.assertEqual(classify(1e9, "Low"), "far")

    def test_mid_band_wider_than_one_cell_half(self):
        # mid must reach past half a 128 m World Partition cell on High so loaded neighbours aren't all far
        self.assertGreater(B["High"][1], 6400)


if __name__ == "__main__":
    unittest.main(verbosity=1)
