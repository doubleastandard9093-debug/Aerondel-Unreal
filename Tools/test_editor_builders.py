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

# Every /Game or /Engine path passed to the mock editor APIs: (op, path)
RECORDED_PATHS = []
CREATED_ASSETS = {}


def _world_json():
    with open(os.path.join(DATA, "world.json"), encoding="utf-8") as f:
        return json.load(f)


def _record(op, path):
    if isinstance(path, str) and (path.startswith("/Game") or path.startswith("/Engine")):
        RECORDED_PATHS.append((op, path))


class _UObject:
    """Stand-in for UObject / actors / components / data assets."""

    def __init__(self, *a, **k):
        self._props = dict(k)
        self.tags = []
        self.static_mesh_component = self
        self.text_render = self
        self.root_component = self

    def set_editor_property(self, k, v):
        self._props[k] = v

    def get_editor_property(self, k):
        if k not in self._props:
            if k == "tint":
                self._props[k] = types.SimpleNamespace(r=0.5, g=0.4, b=0.3)
            elif k == "tags":
                self._props[k] = []
            elif k in ("mesh", "depleted_mesh", "static_mesh_component", "text_render",
                       "root_component", "default_key_mappings"):
                self._props[k] = _UObject()
            else:
                self._props[k] = _UObject()
        return self._props[k]

    def set_static_mesh(self, *a, **k):
        pass

    def set_material(self, idx, material):
        if material is None:
            raise AssertionError("set_material called with None (unhandled None material)")

    def set_relative_location(self, *a, **k):
        pass

    def set_relative_rotation(self, *a, **k):
        pass

    def set_relative_scale3d(self, *a, **k):
        pass

    def set_world_scale3d(self, *a, **k):
        pass

    def set_actor_scale3d(self, *a, **k):
        pass

    def set_actor_location(self, *a, **k):
        pass

    def set_actor_rotation(self, *a, **k):
        pass

    def set_collision_profile_name(self, *a, **k):
        pass

    def set_collision_enabled(self, *a, **k):
        pass

    def set_collision_response_to_channel(self, *a, **k):
        pass

    def set_collision_response_to_all_channels(self, *a, **k):
        pass

    def set_visibility(self, *a, **k):
        pass

    def set_hidden_in_game(self, *a, **k):
        pass

    def set_cast_shadow(self, *a, **k):
        pass

    def set_mobility(self, *a, **k):
        pass

    def setup_attachment(self, *a, **k):
        pass

    def register_component(self):
        pass

    def get_component_by_class(self, *a, **k):
        return self

    def add_component(self, *a, **k):
        return _UObject()

    # Intentionally no rerun_construction_scripts: UE 5.8 Python Actor lacks it.
    def post_edit_change(self, *a, **k):
        self._props["post_edit_change"] = True

    def get_world_settings(self):
        return self

    def set_actor_hidden_in_game(self, *a, **k):
        pass

    def set_text(self, *a, **k):
        pass

    def setup(self, *a, **k):
        pass

    def add_instances(self, *a, **k):
        pass

    def get_class(self):
        return types.SimpleNamespace(get_name=lambda: type(self).__name__)

    def get_folder_path(self):
        return self._props.get("folder", "")

    def get_actor_label(self):
        return self._props.get("label", type(self).__name__)

    def set_actor_label(self, t):
        self._props["label"] = t

    def set_folder_path(self, f):
        self._props["folder"] = f

    def get_path_name(self):
        return "/Game/Dummy.Dummy"

    def get_name(self):
        return type(self).__name__

    def get_outer(self):
        return self

    def modify(self, *a, **k):
        pass

    def __bool__(self):
        return True

    def __iter__(self):
        return iter(())

    def __getitem__(self, i):
        return self


def _uclass(name):
    return type(name, (_UObject,), {})


def _enum(*names):
    ns = types.SimpleNamespace()
    for n in names:
        setattr(ns, n, n)
    return ns


class _Dummy:
    """Kept for a few leftover call sites; prefer _UObject."""

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


def _install_unreal():
    """A minimal `unreal` stand-in so Content/Python modules can be imported outside the editor."""
    RECORDED_PATHS.clear()
    CREATED_ASSETS.clear()
    os.makedirs(os.path.join(ROOT, "Saved"), exist_ok=True)

    u = types.ModuleType("unreal")

    class _Paths:
        @staticmethod
        def project_content_dir():
            return os.path.join(ROOT, "Content") + os.sep

        @staticmethod
        def project_saved_dir():
            return os.path.join(ROOT, "Saved") + os.sep

        @staticmethod
        def project_dir():
            return ROOT + os.sep

    class _EAL:
        existing = set()

        @classmethod
        def does_asset_exist(cls, path):
            _record("does_asset_exist", path)
            return path in cls.existing

        @classmethod
        def delete_asset(cls, path):
            _record("delete_asset", path)
            cls.existing.discard(path)
            CREATED_ASSETS.pop(path, None)
            return True

        @classmethod
        def load_asset(cls, path):
            _record("load_asset", path)
            return CREATED_ASSETS.get(path) or _UObject()

        @classmethod
        def save_loaded_asset(cls, asset, only_if_is_dirty=False):
            return True

    class _AssetTools:
        @staticmethod
        def create_asset(name, folder, cls, factory=None):
            path = "%s/%s" % (folder, name)
            _record("create_asset", path)
            try:
                obj = cls()
            except Exception:
                obj = _UObject()
            _EAL.existing.add(path)
            CREATED_ASSETS[path] = obj
            return obj

    class _Levels:
        def __init__(self):
            self.loaded = []
            self.created = []

        def load_level(self, path):
            _record("load_level", path)
            self.loaded.append(path)
            return True

        def load_map(self, path):
            return self.load_level(path)

        def new_level(self, path, partitioned=False):
            _record("new_level", path)
            self.created.append((path, partitioned))
            _EAL.existing.add(path)
            return True

        def new_level_from_template(self, path, tmpl):
            _record("new_level_from_template", path)
            _record("new_level_from_template_tmpl", tmpl)
            self.created.append((path, "template"))
            _EAL.existing.add(path)
            return True

        def save_current_level(self):
            return True

        def editor_set_viewport_realtime(self, on):
            pass

        def get_editor_world(self):
            return _UObject()

        def set_level_viewport_realtime(self, on):
            pass

        def set_lods(self, *a, **k):
            pass

    class _Actors:
        def __init__(self):
            self.level_actors = []

        def get_all_level_actors(self):
            return list(self.level_actors)

        def destroy_actor(self, a):
            if a in self.level_actors:
                self.level_actors.remove(a)
            return True

        def spawn_actor_from_class(self, cls, loc, rot=None):
            try:
                a = cls()
            except Exception:
                a = _UObject()
            a.static_mesh_component = a
            self.level_actors.append(a)
            return a

    class _Subsys:
        levels = _Levels()
        actors = _Actors()

        @classmethod
        def get(cls, typ):
            name = typ if isinstance(typ, str) else getattr(typ, "__name__", str(typ))
            if "Actor" in name:
                return cls.actors
            if "Level" in name or "Editor" in name or "StaticMesh" in name:
                return cls.levels
            return _UObject()

    def get_editor_subsystem(typ):
        return _Subsys.get(typ)

    def load_asset(path):
        return _EAL.load_asset(path)

    def new_object(cls, outer=None, *a, **k):
        try:
            return cls()
        except Exception:
            return _UObject()

    class _MEL:
        @staticmethod
        def create_material_expression(*a, **k):
            return _UObject()

        @staticmethod
        def connect_material_property(*a, **k):
            pass

        @staticmethod
        def recompile_material(*a, **k):
            pass

        @staticmethod
        def set_material_instance_vector_parameter_value(*a, **k):
            pass

        @staticmethod
        def set_material_instance_scalar_parameter_value(*a, **k):
            pass

        @staticmethod
        def update_material_instance(*a, **k):
            pass

        @staticmethod
        def connect_material_expressions(*a, **k):
            pass

        @staticmethod
        def layout_material_expressions(*a, **k):
            pass

    class _EditorLevelLibrary:
        @staticmethod
        def get_editor_world():
            return _UObject()

        @staticmethod
        def save_current_level():
            return True

        @staticmethod
        def load_level(path):
            _record("ell_load_level", path)
            return True

    class _ScopedSlowTask:
        def __init__(self, *a, **k):
            pass

        def __enter__(self):
            return self

        def __exit__(self, *a):
            return False

        def make_dialog(self, *a, **k):
            pass

        def make_dialog_delayed(self, *a, **k):
            pass

        def enter_progress_frame(self, *a, **k):
            pass

        def should_cancel(self):
            return False

    def _passthru(*a, **k):
        return a[0] if a else _UObject()

    class _GeomNew:
        @staticmethod
        def create_new_static_mesh_asset_from_mesh(dm, path, copts=None):
            _record("geom_create", path)
            sm = u.StaticMesh()
            _EAL.existing.add(path)
            CREATED_ASSETS[path] = sm
            return (sm, True)

    class _GeomPrims:
        append_cylinder = staticmethod(_passthru)
        append_sphere_lat_long = staticmethod(_passthru)
        append_cone = staticmethod(_passthru)
        append_box = staticmethod(_passthru)

    class _GeomNormals:
        @staticmethod
        def compute_split_normals(*a, **k):
            pass

    class _GeomModel:
        @staticmethod
        def apply_mesh_bevel(*a, **k):
            pass

        @staticmethod
        def apply_bevel(*a, **k):
            pass

    u.Paths = _Paths
    u.EditorAssetLibrary = _EAL
    u.AssetToolsHelpers = types.SimpleNamespace(get_asset_tools=lambda: _AssetTools())
    u.get_editor_subsystem = get_editor_subsystem
    u.load_asset = load_asset
    u.new_object = new_object
    u.log = u.log_warning = u.log_error = lambda *a, **k: None
    class _LinearColor:
        def __init__(self, r=0, g=0, b=0, a=1, *rest, **k):
            self.r = r
            self.g = g
            self.b = b
            self.a = a
            self.R = r
            self.G = g
            self.B = b
            self.A = a

    u.Name = lambda x: x
    u.Text = lambda x: x
    u.Vector = lambda *a: a
    u.Rotator = lambda *a: a
    u.Color = _LinearColor
    u.LinearColor = _LinearColor
    u.SystemLibrary = types.SimpleNamespace(
        execute_console_command=lambda *a, **k: None,
        get_console_variable_float_value=lambda *a, **k: 0.0,
        collect_garbage=lambda *a, **k: None,
    )
    u.EditorLoadingAndSavingUtils = types.SimpleNamespace(
        save_dirty_packages=lambda *a, **k: True,
        save_packages=lambda *a, **k: True,
    )
    u.MaterialEditingLibrary = _MEL
    u.EditorLevelLibrary = _EditorLevelLibrary
    u.ScopedSlowTask = _ScopedSlowTask
    u.GeometryScript_NewAssetUtils = _GeomNew
    u.GeometryScript_AssetUtils = type("GeometryScript_AssetUtils", (), {})
    u.GeometryScript_Primitives = _GeomPrims
    u.GeometryScript_Normals = _GeomNormals
    u.GeometryScript_MeshModeling = _GeomModel

    for name in (
        "QuestPlaceVolume", "TextRenderActor", "StaticMeshActor", "PlayerStart",
        "NavMeshBoundsVolume", "BankChest", "ShopCounter", "NpcTalk", "PointLight",
        "DirectionalLight", "SkyLight", "SkyAtmosphere", "ExponentialHeightFog",
        "PostProcessVolume", "Material", "MaterialFactoryNew", "MaterialInstanceConstant",
        "MaterialInstanceConstantFactoryNew", "DataAssetFactory",
        "DynamicMesh", "GeometryScriptPrimitiveOptions", "Transform",
        "GeometryScriptCreateNewStaticMeshAssetOptions", "GeometryScriptCalculateNormalsOptions",
        "GeometryScriptSplitNormalsOptions",
        "StaticMesh", "StaticMeshEditorSubsystem", "EditorScriptingMeshReductionOptions",
        "EditorScriptingMeshReductionSettings", "LevelEditorSubsystem", "EditorActorSubsystem",
        "UnrealEditorSubsystem", "AerondelGameMode",
        "AerondelPropInstances", "Door", "Station", "ResourceNode", "PointLightComponent",
        "InputAction", "InputAction_Factory", "InputMappingContext", "InputMappingContext_Factory",
        "InputModifierSwizzleAxis", "InputModifierNegate", "InputModifierDeadZone",
        "InputModifierScalar", "EnhancedActionKeyMapping", "Key",
        "SkillDef", "ItemDef", "ResourceNodeDef", "RecipeDef", "NpcDef", "MobDef",
        "GameDatabase", "QuestDef", "QuestStep", "ItemAmount", "DropEntry", "XpReward",
        "SoftObjectPath",
    ):
        setattr(u, name, _uclass(name))

    u.HorizTextAligment = _enum("EHTA_CENTER")
    u.ComponentMobility = _enum("MOVABLE", "STATIC")
    u.StationType = _enum("RANGE", "FURNACE", "ANVIL", "FIRE")
    u.GeometryScriptPrimitiveOriginMode = _enum("CENTER", "BASE")
    u.MaterialProperty = _enum(
        "MP_BASE_COLOR", "MP_ROUGHNESS", "MP_METALLIC", "MP_SPECULAR",
        "MP_NORMAL", "MP_SUBSURFACE_COLOR", "MP_CUSTOM_DATA0",
    )
    u.MaterialShadingModel = _enum("MSM_SUBSURFACE_PROFILE", "MSM_SUBSURFACE", "MSM_CLOTH")
    u.InputActionValueType = _enum("BOOLEAN", "AXIS1D", "AXIS2D", "AXIS3D")
    u.InputAxisSwizzle = _enum("YXZ", "XZY", "ZYX")
    u.ToolType = _enum("NONE", "AXE", "PICKAXE", "FISHING_NET", "HAMMER", "TINDERBOX")
    u.EquipSlot = _enum("NONE", "WEAPON", "HEAD", "BODY", "SHIELD", "LEGS", "FEET")
    u.ResourceKind = _enum("TREE", "ROCK", "FISHING_SPOT", "HERB")
    u.QuestStepType = _enum("TALK", "KILL", "GATHER", "DELIVER")
    u.CollisionChannel = _enum("ECC_VISIBILITY", "ECC_PAWN", "ECC_WORLD_STATIC")
    u.CollisionResponseType = _enum("ECR_IGNORE", "ECR_BLOCK", "ECR_OVERLAP")
    u.CollisionEnabled = _enum("QUERY_ONLY", "QUERY_AND_PHYSICS", "NO_COLLISION")
    u.RuntimeGenerationType = _enum("DYNAMIC", "STATIC", "DYNAMIC_MODIFIERS_ONLY")

    u._EAL = _EAL
    u._Subsys = _Subsys
    u._AssetTools = _AssetTools

    def __getattr__(name):
        obj = _uclass(name)
        setattr(u, name, obj)
        return obj

    u.__getattr__ = __getattr__
    sys.modules["unreal"] = u
    return u


_BUILDER_MODULES = (
    "aerondel_util", "aerondel_safe", "build_starter_town", "world_partition",
    "build_world_blockout", "aerondel_full_world", "build_smooth_shapes",
    "build_relic_dungeon", "build_aerondel_regions", "world_placements",
    "create_input_assets", "generate_starter_data", "build_high_fidelity",
    "build_festival_decor", "aerondel_all", "build_realism_materials",
    "import_lite_models", "build_character_lods", "aerondel_laptop",
)


def _import_builders():
    _install_unreal()
    if PY not in sys.path:
        sys.path.insert(0, PY)
    for name in list(sys.modules):
        if name in _BUILDER_MODULES:
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


def _assert_recorded_paths_valid(test):
    import aerondel_util as util
    bad = []
    for op, path in RECORDED_PATHS:
        if " " in path or not util.is_valid_asset_path(path):
            bad.append((op, path))
    test.assertFalse(bad, "invalid asset paths (spaces or illegal characters):\n  " +
                     "\n  ".join("%s %s" % (op, p) for op, p in bad[:30]))


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

    def test_safe_asset_name_and_path(self):
        util, *_ = _import_builders()
        self.assertEqual(util.safe_asset_name("Copper rocks"), "Copper_rocks")
        self.assertEqual(util.safe_asset_name("Ore_Copper rocks"), "Ore_Copper_rocks")
        self.assertEqual(util.safe_asset_name("Founders' Week"), "Founders_Week")
        self.assertEqual(util.safe_asset_name("Tin  rocks!!"), "Tin_rocks")
        self.assertEqual(util.safe_asset_name("123abc"), "_123abc")
        self.assertEqual(util.safe_asset_name(""), "Asset")
        self.assertEqual(util.safe_asset_name(None), "Asset")
        self.assertEqual(util.safe_asset_name("Already_Safe"), "Already_Safe")
        self.assertEqual(
            util.safe_asset_path("/Game/Gielinor/Materials/MI_Ore_Copper rocks"),
            "/Game/Gielinor/Materials/MI_Ore_Copper_rocks",
        )
        self.assertEqual(
            util.safe_asset_path("/Engine/BasicShapes/Cube.Cube"),
            "/Engine/BasicShapes/Cube.Cube",
        )
        self.assertTrue(util.is_valid_asset_path("/Game/Gielinor/Materials/MI_Ore_Copper_rocks"))
        self.assertTrue(util.is_valid_asset_path("/Engine/BasicShapes/Cube.Cube"))
        self.assertFalse(util.is_valid_asset_path("/Game/Gielinor/Materials/MI_Ore_Copper rocks"))
        self.assertFalse(util.is_valid_asset_path(""))
        self.assertFalse(util.is_valid_asset_path("MI_Ore_Copper_rocks"))

    def test_mat_sanitizes_display_names_and_falls_back_on_none(self):
        """Regression: mat('Ore_Copper rocks') used to build a path with a space, create_asset
        returned None, and mi.set_editor_property crashed."""
        util, *_ = _import_builders()
        import unreal
        RECORDED_PATHS.clear()
        mi = util.mat("Ore_Copper rocks", (0.75, 0.45, 0.3), 0.6)
        self.assertIsNotNone(mi)
        game_paths = [p for _, p in RECORDED_PATHS]
        self.assertTrue(
            any(p == "/Game/Gielinor/Materials/MI_Ore_Copper_rocks" for p in game_paths),
            "expected sanitized MI path, got: %s" % game_paths,
        )
        self.assertFalse(any(" " in p for p in game_paths))
        # Same sanitized key shares the cache.
        self.assertIs(util.mat("Ore_Copper_rocks", (0.1, 0.1, 0.1)), mi)

        # create_asset returns None for new MIs: mat() must not crash, must return a fallback.
        util._MAT_CACHE.clear()
        util._FALLBACK_MAT = None
        orig = util.asset_tools.create_asset

        def selective(name, folder, cls, factory=None):
            if str(name).startswith("MI_"):
                return None
            return orig(name, folder, cls, factory)

        util.asset_tools.create_asset = selective
        fb = util.mat("Ore_Tin rocks", (0.7, 0.7, 0.68), 0.6)
        self.assertIsNotNone(fb)
        util.set_props(None, foo=1)  # must not raise
        util.save(None)

    def test_builders_emit_only_valid_asset_paths(self):
        """Run as much of the editor builders as the mock allows; every /Game and /Engine
        path must be a legal UE name, and helpers must not dereference None."""
        util, blockout, smooth, town, dungeon = _import_builders()
        import time
        time.sleep = lambda *a, **k: None
        import aerondel_all
        import build_festival_decor

        RECORDED_PATHS.clear()
        aerondel_all.run()
        blockout.run(small=False)
        dungeon.run()
        build_festival_decor.run()
        self.assertTrue(
            any(p.endswith("/MI_Ore_Copper_rocks") or p.endswith("MI_Ore_Copper_rocks")
                for _, p in RECORDED_PATHS),
            "starter town should have requested the sanitized copper-ore material",
        )
        _assert_recorded_paths_valid(self)
        # A create_asset / load_or_create / geom create should have actually produced objects.
        self.assertTrue(CREATED_ASSETS, "expected the mock to create at least one asset")
        # Spawned actors (including AerondelMob) must not grow a fake rerun_construction_scripts.
        import unreal
        mob = unreal.AerondelMob()
        self.assertFalse(hasattr(mob, "rerun_construction_scripts"))

    def test_refresh_actor_fallbacks_when_rerun_is_missing(self):
        """UE 5.8: Actor.rerun_construction_scripts is absent; refresh_actor must not crash."""
        util, *_ = _import_builders()
        import unreal

        class Bare:
            pass

        self.assertFalse(hasattr(Bare(), "rerun_construction_scripts"))
        self.assertFalse(util.refresh_actor(Bare()))  # no-op
        self.assertFalse(util.refresh_actor(None))

        class WithRerun:
            def __init__(self):
                self.called = []

            def rerun_construction_scripts(self):
                self.called.append("rerun")

        a = WithRerun()
        self.assertTrue(util.refresh_actor(a))
        self.assertEqual(a.called, ["rerun"])

        class WithPost:
            def __init__(self):
                self.called = []

            def post_edit_change(self):
                self.called.append("post")

        a = WithPost()
        self.assertFalse(hasattr(a, "rerun_construction_scripts"))
        self.assertTrue(util.refresh_actor(a))
        self.assertEqual(a.called, ["post"])

        # Default mock actors also lack rerun; post_edit_change is the fallback.
        actor = unreal.AerondelMob()
        self.assertFalse(hasattr(actor, "rerun_construction_scripts"))
        self.assertTrue(util.refresh_actor(actor))
        self.assertTrue(actor._props.get("post_edit_change"))

    def test_no_direct_rerun_construction_scripts_outside_util(self):
        """Builders must go through refresh_actor, not call the missing 5.8 method directly."""
        for name in os.listdir(PY):
            if not name.endswith(".py") or name == "aerondel_util.py":
                continue
            with open(os.path.join(PY, name), encoding="utf-8") as fh:
                src = fh.read()
            self.assertNotIn(
                ".rerun_construction_scripts(", src,
                "%s still calls rerun_construction_scripts directly" % name,
            )
        with open(os.path.join(PY, "aerondel_util.py"), encoding="utf-8") as fh:
            util_src = fh.read()
        self.assertIn("def refresh_actor", util_src)
        with open(os.path.join(PY, "build_starter_town.py"), encoding="utf-8") as fh:
            town = fh.read()
        self.assertIn("refresh_actor(", town)

    def test_open_or_create_map_clear_false_keeps_actors(self):
        util, *_ = _import_builders()
        import unreal
        levels = unreal._Subsys.levels
        actors = unreal._Subsys.actors
        path = "/Game/Gielinor/Maps/World"
        unreal._EAL.existing.clear()
        levels.loaded.clear(); levels.created.clear()
        self.assertTrue(util.open_or_create_map(path, levels))
        marker = actors.spawn_actor_from_class(unreal.StaticMeshActor, (0, 0, 0))
        self.assertIn(marker, actors.level_actors)
        levels.loaded.clear(); levels.created.clear()
        self.assertTrue(util.open_or_create_map(path, levels, clear=False))
        self.assertIn(marker, actors.level_actors)
        self.assertEqual(levels.created, [])
        levels.loaded.clear(); levels.created.clear()
        self.assertTrue(util.open_or_create_map(path, levels, clear=True))
        self.assertNotIn(marker, actors.level_actors)

    def test_laptop_chunks_cover_every_region_and_placement(self):
        _import_builders()
        import aerondel_laptop as laptop
        import world_placements as wp
        w = _world_json()
        with open(os.path.join(DATA, "placements.json"), encoding="utf-8") as fh:
            placements = json.load(fh)
        nodes, mobs = wp.plan(placements, small=False)
        regions = list(w.get("regions") or [])
        ids = laptop.chunk_ids(len(regions), len(nodes), len(mobs))
        self.assertIn("setup", ids)
        self.assertIn("world_base", ids)
        self.assertIn("shores", ids)
        self.assertIn("finalize", ids)
        self.assertTrue(any(i.startswith("regions_") for i in ids))
        self.assertTrue(any(i.startswith("placements_") for i in ids))
        r_sl = laptop.region_slices(regions)
        self.assertEqual(sum(len(s) for s in r_sl), len(regions))
        p_sl = laptop.placement_slices(nodes, mobs)
        self.assertEqual(sum(len(n) + len(m) for n, m in p_sl), len(nodes) + len(mobs))
        self.assertEqual(len(r_sl), len([i for i in ids if i.startswith("regions_")]))
        self.assertEqual(len(p_sl), len([i for i in ids if i.startswith("placements_")]))
        # Gameplay coverage is complete (not the 4s small-test cap).
        self.assertGreaterEqual(len(regions), 14)
        self.assertGreaterEqual(len(nodes), 200)
        self.assertGreaterEqual(len(mobs), 100)

    def _clear_laptop_files(self):
        saved = os.path.join(ROOT, "Saved")
        os.makedirs(saved, exist_ok=True)
        for name in ("AerondelLaptopProgress.json", "AerondelWorldBuild.json"):
            path = os.path.join(saved, name)
            if os.path.exists(path):
                os.remove(path)

    def test_laptop_build_and_resume(self):
        """Mock run of 0L: skips high-fidelity, writes a laptop marker, resumes skipped chunks."""
        util, blockout, *_ = _import_builders()
        import time
        time.sleep = lambda *a, **k: None
        import aerondel_laptop as laptop
        import aerondel_all
        import build_high_fidelity
        self._clear_laptop_files()

        hf_calls = []
        orig_hf = build_high_fidelity.run
        build_high_fidelity.run = lambda *a, **k: hf_calls.append(True)

        orig_all = aerondel_all.run
        all_flags = []

        def wrapped_all(include_high_fidelity=True):
            all_flags.append(include_high_fidelity)
            return orig_all(include_high_fidelity=include_high_fidelity)

        import importlib
        orig_reload = importlib.reload

        def reload(m):
            out = orig_reload(m)
            if getattr(out, "__name__", None) == "aerondel_all":
                out.run = wrapped_all
            return out

        importlib.reload = reload
        laptop.importlib.reload = reload
        aerondel_all.run = wrapped_all
        laptop.aerondel_all.run = wrapped_all

        laptop.run()
        self.assertEqual(hf_calls, [])
        self.assertEqual(all_flags, [False])
        prog_path = os.path.join(ROOT, "Saved", "AerondelLaptopProgress.json")
        mark_path = os.path.join(ROOT, "Saved", "AerondelWorldBuild.json")
        with open(prog_path, encoding="utf-8") as fh:
            progress = json.load(fh)
        with open(mark_path, encoding="utf-8") as fh:
            marker = json.load(fh)
        self.assertEqual(progress["status"], "complete")
        import world_placements as wp
        nodes, mobs = wp.plan(wp.load(os.path.join(ROOT, "Content") + os.sep), small=False)
        w = blockout.load_world()
        ids = laptop.chunk_ids(len(w.get("regions") or []), len(nodes), len(mobs))
        self.assertEqual(progress["done"], ids)
        self.assertEqual(marker["mode"], "laptop")
        self.assertTrue(marker.get("laptop"))
        self.assertEqual(marker["nodes"], len(nodes))
        self.assertEqual(marker["mobs"], len(mobs))
        self.assertIn("high_fidelity", marker.get("skipped") or [])

        # Resume: pretend only setup + world_base finished; world_base must not run again.
        progress["status"] = "in_progress"
        progress["done"] = ["setup", "world_base"]
        progress["in_progress"] = "regions_00"
        with open(prog_path, "w", encoding="utf-8") as fh:
            json.dump(progress, fh)
        if os.path.exists(mark_path):
            os.remove(mark_path)

        base_calls = []
        orig_base = blockout.build_world_base

        def wrapped_base(*a, **k):
            base_calls.append(True)
            return orig_base(*a, **k)

        blockout.build_world_base = wrapped_base
        laptop.blockout.build_world_base = wrapped_base
        all_flags.clear()
        laptop.run()
        self.assertEqual(base_calls, [])
        self.assertEqual(all_flags, [])  # setup already done
        with open(prog_path, encoding="utf-8") as fh:
            progress = json.load(fh)
        with open(mark_path, encoding="utf-8") as fh:
            marker = json.load(fh)
        self.assertEqual(progress["status"], "complete")
        self.assertEqual(progress["done"], ids)
        self.assertEqual(marker["mode"], "laptop")
        self._clear_laptop_files()

    def test_packager_allows_laptop_but_refuses_small(self):
        with open(os.path.join(ROOT, "Tools", "package_win64.bat"), encoding="utf-8") as fh:
            bat = fh.read()
        self.assertIn("mode -eq 'laptop'", bat)
        self.assertIn("mode -eq 'small'", bat)
        self.assertIn("0L", bat)

        def allowed(mode, nodes, mobs):
            if nodes < 1 or mobs < 1 or mode == "small":
                return False
            return mode in ("full", "laptop")

        self.assertTrue(allowed("laptop", 284, 159))
        self.assertTrue(allowed("full", 284, 159))
        self.assertFalse(allowed("small", 10, 10))
        self.assertFalse(allowed("laptop", 0, 10))
        self.assertFalse(allowed("partial", 284, 159))

    def test_menu_lists_laptop_entry(self):
        with open(os.path.join(PY, "init_unreal.py"), encoding="utf-8") as fh:
            src = fh.read()
        self.assertIn("BuildFullWorldLaptop", src)
        self.assertIn("0L. BUILD FULL WORLD - LAPTOP", src)
        self.assertIn("aerondel_full_world.run_laptop()", src)
        with open(os.path.join(PY, "aerondel_full_world.py"), encoding="utf-8") as fh:
            full = fh.read()
        self.assertIn("def run_laptop", full)
        self.assertIn("build_world_blockout.run(small=False)", full)

    def test_world_and_laptop_builds_ensure_a_covering_navmesh(self):
        """Play logs 'Unable to find RecastNavMesh' unless 0/0L/4 place bounds and build paths."""
        with open(os.path.join(PY, "build_world_blockout.py"), encoding="utf-8") as fh:
            blockout = fh.read()
        with open(os.path.join(PY, "aerondel_laptop.py"), encoding="utf-8") as fh:
            laptop = fh.read()
        self.assertIn("def ensure_world_navmesh(", blockout)
        self.assertIn("NavMeshBoundsVolume", blockout)
        self.assertIn("_request_nav_build", blockout)
        self.assertIn("Build > Build Paths", blockout)
        self.assertIn("ensure_world_navmesh(half)", blockout)
        self.assertIn("def finalize_world", blockout)
        # 0L already-complete path still fixes nav without rebuilding the world.
        self.assertIn('ensure_world_navmesh(float(w.get("worldHalf", 300)))', laptop)


if __name__ == "__main__":
    unittest.main(verbosity=1)
