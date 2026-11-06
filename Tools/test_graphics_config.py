"""Static checks that keep Low light and the cook safe (no Unreal). Run: python3 Tools/test_graphics_config.py"""
import os, re, unittest
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SCAL = open(os.path.join(ROOT, "Config/DefaultScalability.ini"), encoding="utf-8").read()
CPP = open(os.path.join(ROOT, "Source/Aerondel/World/AerondelScalability.cpp"), encoding="utf-8").read()
# Not scalability cvars: an ensure in the cook (ExitCode 25) if they appear in DefaultScalability.ini.
FORBIDDEN = ["r.DynamicGlobalIlluminationMethod", "r.ReflectionMethod", "r.AntiAliasingMethod", "r.Nanite"]  # r.Shadow.Virtual.Enable IS a ShadowQuality cvar (cooked fine)


def low_block():
    m = re.search(r"case EAerondelTier::Low:(.*?)break;", CPP, re.S)
    return m.group(1) if m else ""


class Graphics(unittest.TestCase):
    def test_scalability_ini_has_only_scalability_cvars(self):
        for k in FORBIDDEN:
            self.assertNotRegex(SCAL, r"(?m)^\s*" + re.escape(k) + r"\s*=", k)

    def test_low_has_no_lumen_or_heavy_effects(self):
        low = low_block()
        self.assertTrue(low, "Low case not found")
        for k in ("r.DynamicGlobalIlluminationMethod\"), 1", "r.Lumen.Reflections.Allow\"), 1", "r.VolumetricFog\"), 1", "r.Shadow.Virtual.Enable\"), 1"):
            self.assertNotIn(k, low)


if __name__ == "__main__":
    unittest.main(verbosity=1)
