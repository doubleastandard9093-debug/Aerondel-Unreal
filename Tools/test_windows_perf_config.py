"""Static checks for Windows cook/render perf knobs (no Unreal). Run: python3 Tools/test_windows_perf_config.py"""
import os, re, json, unittest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ENG = open(os.path.join(ROOT, "Config/DefaultEngine.ini"), encoding="utf-8").read()
WIN = open(os.path.join(ROOT, "Config/Windows/WindowsEngine.ini"), encoding="utf-8").read()
SCAL = open(os.path.join(ROOT, "Config/DefaultScalability.ini"), encoding="utf-8").read()
CPP = open(os.path.join(ROOT, "Source/Aerondel/World/AerondelScalability.cpp"), encoding="utf-8").read()
UPROJECT = json.load(open(os.path.join(ROOT, "Aerondel.uproject"), encoding="utf-8"))
BUILD_BAT = open(os.path.join(ROOT, "Build.bat"), encoding="utf-8").read()
FORBIDDEN = ["r.DynamicGlobalIlluminationMethod", "r.ReflectionMethod", "r.AntiAliasingMethod", "r.Nanite"]


def low_block():
    m = re.search(r"case EAerondelTier::Low:(.*?)break;", CPP, re.S)
    return m.group(1) if m else ""


def plugin(name):
    for p in UPROJECT["Plugins"]:
        if p["Name"] == name:
            return p
    return None


class WindowsPerf(unittest.TestCase):
    def test_nanite_disabled_in_default_engine(self):
        self.assertRegex(ENG, r"(?m)^\s*r\.Nanite\.ProjectEnabled\s*=\s*False")
        self.assertRegex(ENG, r"(?m)^\s*r\.Nanite\s*=\s*0")

    def test_windows_disables_stereo_and_multiview(self):
        self.assertRegex(WIN, r"(?m)^\s*vr\.InstancedStereo\s*=\s*False")
        self.assertRegex(WIN, r"(?m)^\s*vr\.MobileMultiView\s*=\s*False")
        self.assertRegex(WIN, r"(?m)^\s*r\.Nanite\.ProjectEnabled\s*=\s*False")

    def test_scalability_still_cook_safe(self):
        for k in FORBIDDEN:
            self.assertNotRegex(SCAL, r"(?m)^\s*" + re.escape(k) + r"\s*=", k)

    def test_low_stays_cheap(self):
        low = low_block()
        self.assertTrue(low, "Low case not found")
        for k in (
            'r.DynamicGlobalIlluminationMethod"), 1',
            'r.Lumen.Reflections.Allow"), 1',
            'r.VolumetricFog"), 1',
            'r.Shadow.Virtual.Enable"), 1',
        ):
            self.assertNotIn(k, low)
        # Cheap path helper must still clear Lumen/VSM/fog for Low
        self.assertIn("ApplyCheapPath()", low)

    def test_openxr_android_only(self):
        for name in ("OpenXR", "OpenXRHandTracking"):
            p = plugin(name)
            self.assertIsNotNone(p, name)
            self.assertTrue(p["Enabled"], name)
            self.assertEqual(p.get("SupportedTargetPlatforms"), ["Android"], name)

    def test_editor_plugins_not_in_game(self):
        for name in ("PythonScriptPlugin", "EditorScriptingUtilities", "ModelingToolsEditorMode", "GeometryScripting"):
            p = plugin(name)
            self.assertIsNotNone(p, name)
            self.assertEqual(p.get("TargetAllowList"), ["Editor"], name)

    def test_commonui_disabled(self):
        p = plugin("CommonUI")
        self.assertIsNotNone(p)
        self.assertFalse(p["Enabled"])

    def test_build_bat_defaults_incremental(self):
        self.assertIn("AerondelEditor", BUILD_BAT)
        self.assertIn("MaxParallelActions=1", BUILD_BAT)
        self.assertIn("-Rebuild", BUILD_BAT)
        # Default path must NOT force -Rebuild
        self.assertIn('set "EXTRA=-WaitMutex"', BUILD_BAT)

    def test_quest_shadow_cheaper_than_low(self):
        m0 = re.search(r"\[ShadowQuality@0\](.*?)\[ShadowQuality@1\]", SCAL, re.S)
        m1 = re.search(r"\[ShadowQuality@1\](.*?)\[ShadowQuality@2\]", SCAL, re.S)
        self.assertTrue(m0 and m1)
        self.assertRegex(m0.group(1), r"r\.Shadow\.MaxResolution\s*=\s*512")
        self.assertRegex(m1.group(1), r"r\.Shadow\.MaxResolution\s*=\s*1024")


if __name__ == "__main__":
    unittest.main(verbosity=1)
