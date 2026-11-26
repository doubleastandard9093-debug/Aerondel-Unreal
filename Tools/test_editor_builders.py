"""Pure-Python checks for the editor builders (no Unreal). Run: python3 Tools/test_editor_builders.py"""
import ast
import importlib
import json
import os
import sys
import types
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PY = os.path.join(ROOT, "Content", "Python")
DATA = os.path.join(ROOT, "Content", "Data")


def _world_json():
    with open(os.path.join(DATA, "world.json"), encoding="utf-8") as f:
        return json.load(f)


class _Dummy:
    def __init__(self, *a, **k):
        self._a, self._k = a, k

    def __getattr__(self, name):
        if name.startswith("_"):
            raise AttributeError(name)
        return lambda *a, **k: _Dummy(name, *a, **k)

    def __iter__(self):
        return iter(())

    def __bool__(self):
        return True

    def __getitem__(self, i):
        return self


def _enum(*names):
    ns = types.SimpleNamespace()
    for n in names:
        setattr(ns, n, n)
    return ns


def _install_unreal():
    """A minimal `unreal` stand-in so Content/Python modules can be imported outside the editor."""
    u = types.ModuleType("unreal")

    class _Paths:
        @staticmethod
        def project_content_dir():
            return os.path.join(ROOT, "Content") + os.sep

        @staticmethod
        def project_saved_dir():
            return os.path.join(ROOT, "Saved") + os.sep

    class _EAL:
        existing = set()

        @classmethod
        def does_asset_exist(cls, path):
            return path in cls.existing

        @classmethod
        def delete_asset(cls, path):
            cls.existing.discard(path)
            return True

        @classmethod
        def load_asset(cls, path):
            return _Dummy("asset", path)

        @classmethod
        def save_loaded_asset(cls, asset, only_if_is_dirty=False):
            return True

    class _Levels:
        def __init__(self):
            self.loaded = []
            self.created = []

        def load_level(self, path):
            self.loaded.append(path)
            return True

        def new_level(self, path, partitioned=False):
            self.created.append((path, partitioned))
            _EAL.existing.add(path)
            return True

        def save_current_level(self):
            return True

    class _Actors:
        def get_all_level_actors(self):
            return []

        def destroy_actor(self, a):
            return True

        def spawn_actor_from_class(self, cls, loc, rot=None):
            return _Dummy("actor", cls, loc)

    class _Subsys:
        levels = _Levels()
        actors = _Actors()

        @classmethod
        def get(cls, typ):
            name = typ if isinstance(typ, str) else getattr(typ, "__name__", str(typ))
            if "Level" in name:
                return cls.levels
            if "Actor" in name:
                return cls.actors
            return _Dummy(name)

    def get_editor_subsystem(typ):
        return _Subsys.get(typ)

    u.Paths = _Paths
    u.EditorAssetLibrary = _EAL
    u.AssetToolsHelpers = types.SimpleNamespace(get_asset_tools=lambda: _Dummy("tools"))
    u.get_editor_subsystem = get_editor_subsystem
    u.log = u.log_warning = u.log_error = lambda *a, **k: None
    u.Name = lambda x: x
    u.Text = lambda x: x
    u.Vector = lambda *a: a
    u.Rotator = lambda *a: a
    u.Color = lambda **k: k
    u.LinearColor = lambda *a: a
    u.SystemLibrary = types.SimpleNamespace(execute_console_command=lambda *a, **k: None)
    # Class names referenced at import / spawn time.
    for name in (
        "QuestPlaceVolume", "TextRenderActor", "StaticMeshActor", "PlayerStart",
        "NavMeshBoundsVolume", "BankChest", "ShopCounter", "NpcTalk", "PointLight",
        "DirectionalLight", "SkyLight", "SkyAtmosphere", "ExponentialHeightFog",
        "PostProcessVolume", "Material", "MaterialFactoryNew", "MaterialInstanceConstant",
        "MaterialInstanceConstantFactoryNew", "MaterialEditingLibrary", "DataAssetFactory",
        "DynamicMesh", "GeometryScriptPrimitiveOptions", "Transform",
        "GeometryScriptCreateNewStaticMeshAssetOptions", "GeometryScriptCalculateNormalsOptions",
        "GeometryScriptSplitNormalsOptions", "GeometryScript_Normals",
        "GeometryScript_Primitives", "GeometryScript_NewAssetUtils", "GeometryScript_AssetUtils",
        "StaticMesh", "StaticMeshEditorSubsystem", "EditorScriptingMeshReductionOptions",
        "EditorScriptingMeshReductionSettings", "LevelEditorSubsystem", "EditorActorSubsystem",
        "UnrealEditorSubsystem", "EditorLevelLibrary", "AerondelGameMode",
        "GeometryScript_MeshModeling",
    ):
        setattr(u, name, type(name, (), {}))
    u.HorizTextAligment = _enum("EHTA_CENTER")
    u.ComponentMobility = _enum("MOVABLE", "STATIC")
    u.StationType = _enum("RANGE", "FURNACE", "ANVIL", "FIRE")
    u.GeometryScriptPrimitiveOriginMode = _enum("CENTER", "BASE")
    u.MaterialProperty = _enum("MP_BASE_COLOR", "MP_ROUGHNESS", "MP_METALLIC")
    u.InputActionValueType = _enum("BOOLEAN", "AXIS1D", "AXIS2D", "AXIS3D")
    u._EAL = _EAL
    u._Subsys = _Subsys

    def __getattr__(name):
        obj = type(name, (), {})
        setattr(u, name, obj)
        return obj
    u.__getattr__ = __getattr__
    sys.modules["unreal"] = u
    return u


def _import_builders():
    _install_unreal()
    if PY not in sys.path:
        sys.path.insert(0, PY)
    # Re-import if a previous test left stale modules.
    for name in list(sys.modules):
        if name in (
            "aerondel_util", "aerondel_safe", "build_starter_town", "world_partition",
            "build_world_blockout", "aerondel_full_world", "build_smooth_shapes",
            "build_relic_dungeon", "build_aerondel_regions", "world_placements",
        ):
            del sys.modules[name]
    import aerondel_util  # noqa: F401
    import build_world_blockout
    import build_smooth_shapes
    import build_starter_town
    import build_relic_dungeon
    return (
        sys.modules["aerondel_util"],
        build_world_blockout,
        build_smooth_shapes,
        build_starter_town,
        build_relic_dungeon,
    )


class EditorBuilders(unittest.TestCase):
    def test_world_json_is_an_object_with_towns(self):
        w = _world_json()
        self.assertIsInstance(w, dict)
        self.assertIsInstance(w.get("worldHalf"), (int, float))
        self.assertIsInstance(w.get("towns"), dict)
        self.assertTrue(w["towns"])

    def test_run_does_not_rebind_world_dict_as_w(self):
        """Catch `for i, (w, d, ...) in ...` inside run() — that made build_towns(w) receive a float."""
        with open(os.path.join(PY, "build_world_blockout.py"), encoding="utf-8") as fh:
            src = fh.read()
        tree = ast.parse(src)
        run = next(n for n in tree.body if isinstance(n, ast.FunctionDef) and n.name == "run")

        def rebound(node):
            if isinstance(node, ast.For):
                def names(tgt):
                    if isinstance(tgt, ast.Name):
                        yield tgt.id, tgt.lineno
                    elif isinstance(tgt, (ast.Tuple, ast.List)):
                        for elt in tgt.elts:
                            yield from names(elt)
                for nid, ln in names(node.target):
                    if nid == "w":
                        self.fail(f"run() rebinds world dict as loop variable `w` at line {ln}")
            for ch in ast.iter_child_nodes(node):
                rebound(ch)

        rebound(run)

    def test_build_towns_and_other_build_calls_accept_world_json(self):
        util, blockout, *_ = _import_builders()
        w = blockout.load_world()
        self.assertIsInstance(w, dict)
        self.assertIn("towns", w)
        # Stub editor-only helpers so build_towns can walk world.json without creating assets.
        blockout.mat = lambda *a, **k: object()
        blockout.box = lambda *a, **k: None
        blockout.text3d = lambda *a, **k: None
        import build_starter_town as bst
        bst.interactable = lambda *a, **k: types.SimpleNamespace(set_editor_property=lambda *aa, **kk: None)
        # The original crash: w.get("towns") when w was a float.
        blockout.build_towns(w)
        # Other build_* call sites in run / full_world take the same world dict, not a scalar.
        with open(os.path.join(PY, "build_world_blockout.py"), encoding="utf-8") as fh:
            src = fh.read()
        self.assertIn("build_towns(w)", src)
        self.assertIn("build_aerondel_regions.build(w", src)
        self.assertIn("world_placements.build(w", src)
        with open(os.path.join(PY, "aerondel_full_world.py"), encoding="utf-8") as fh:
            full_src = fh.read()
        self.assertIn("build_world_blockout.run(small=False)", full_src)

    def test_open_or_create_map_is_idempotent(self):
        util, *_ = _import_builders()
        import unreal
        levels = unreal._Subsys.levels
        path = "/Game/Gielinor/Maps/StarterTown"
        # First time: create
        unreal._EAL.existing.clear()
        levels.loaded.clear(); levels.created.clear()
        self.assertTrue(util.open_or_create_map(path, levels))
        self.assertEqual(levels.created, [(path, False)])
        self.assertEqual(levels.loaded, [])
        # Second time: load, do not NewLevel
        levels.loaded.clear(); levels.created.clear()
        self.assertTrue(util.open_or_create_map(path, levels))
        self.assertEqual(levels.loaded, [path])
        self.assertEqual(levels.created, [])

    def test_smooth_shapes_resolves_ue58_names(self):
        _, _, smooth, *_ = _import_builders()
        import unreal

        def factory(*_a, **_k):
            return object()

        unreal.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh = staticmethod(factory)
        fn = smooth._first_attr(
            ("GeometryScript_NewAssetUtils", "create_new_static_mesh_asset_from_mesh_v2"),
            ("GeometryScript_NewAssetUtils", "create_new_static_mesh_asset_from_mesh"),
            ("GeometryScript_AssetUtils", "create_new_static_mesh_asset_from_mesh"),
        )
        self.assertIs(fn, unreal.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh)
        # Missing names fall through without raising until none remain.
        self.assertIsNone(smooth._first_attr(("GeometryScript_AssetUtils", "no_such_fn")))


if __name__ == "__main__":
    unittest.main(verbosity=1)
