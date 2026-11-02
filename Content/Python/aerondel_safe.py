"""
Crash-safe editor runs for weak GPUs (Intel UHD / small iGPUs, 2 s TDR).

    with aerondel_safe.SafeRun("World blockout", total_frames=8, small=False) as safe:
        safe.frame("Buildings")          # progress step (ScopedSlowTask)
        ... spawns call safe.tick() ...   # build_starter_town.box()/spawn() already do

While a SafeRun is active:
  * editor scalability drops to Low (sg.* 0, r.ScreenPercentage 50, t.MaxFPS 15, Lumen/VSM/Nanite-heavy features off)
    and viewports stop rendering in realtime; everything is restored afterwards, even on error/cancel;
  * every BATCH spawns: garbage collect + a progress update, so the GPU/driver gets a breather (no TDR);
  * decorative spawns are capped per 128 m World Partition cell (CELL_CAP);
  * small=True ("small test") builds only a slice of the world so you can check the machine copes first.
"""
import math
import time
import unreal

BATCH = 40              # spawns between breathers
CELL_CM = 12800.0       # World Partition cell (world_partition.CELL_SIZE_CM)
CELL_CAP = 60           # max decorative actors per cell
SMALL_LIMIT = 10        # small test: max items per category (buildings, NPCs, places, regions...)

LOW_CVARS = {
    "sg.ViewDistanceQuality": 0, "sg.AntiAliasingQuality": 0, "sg.ShadowQuality": 0, "sg.GlobalIlluminationQuality": 0,
    "sg.ReflectionQuality": 0, "sg.PostProcessQuality": 0, "sg.TextureQuality": 0, "sg.EffectsQuality": 0,
    "sg.FoliageQuality": 0, "sg.ShadingQuality": 0,
    "r.ScreenPercentage": 50, "t.MaxFPS": 15,
    "r.Shadow.Virtual.Enable": 0, "r.DynamicGlobalIlluminationMethod": 0, "r.ReflectionMethod": 0,
    "r.VolumetricFog": 0, "r.VolumetricCloud": 0, "r.DistanceFieldAO": 0,
}

_active = None


def _cmd(c):
    try:
        unreal.SystemLibrary.execute_console_command(unreal.EditorLevelLibrary.get_editor_world(), c)
    except Exception as e:
        unreal.log_warning("[Aerondel] console '%s' failed: %s" % (c, e))


def _get(name):
    try:
        return unreal.SystemLibrary.get_console_variable_float_value(name)
    except Exception:
        return None


def _set_realtime(on):
    """Best effort across engine versions; harmless if the API is missing."""
    for owner, fn in ((unreal.get_editor_subsystem(unreal.LevelEditorSubsystem), "editor_set_viewport_realtime"),
                      (unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem), "set_level_viewport_realtime")):
        f = getattr(owner, fn, None)
        if f is None:
            continue
        try:
            f(on)
            return
        except Exception:
            pass


def active():
    return _active


def save_dirty():
    """Save every dirty package (used between import batches)."""
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)


def tick(n=1):
    if _active:
        _active.tick(n)


def allow(x_m, z_m, decor=True):
    """Per-cell cap for decorative spawns (web metres). Essential pieces always pass."""
    return _active.allow(x_m, z_m, decor) if _active else True


def small():
    return bool(_active and _active.small)


def limit(seq):
    """In small-test mode, only the first SMALL_LIMIT entries of a list / dict items."""
    items = list(seq.items()) if isinstance(seq, dict) else list(seq)
    return items[:SMALL_LIMIT] if small() else items


class Cancelled(Exception):
    pass


class SafeRun:
    def __init__(self, title, total_frames=1, small=False, save_fn=None):
        self.title, self.total, self.small, self.save_fn = title, max(1, total_frames), small, save_fn
        self.count, self.since, self.cells, self.skipped, self.task, self.saved = 0, 0, {}, 0, None, {}
        self.cancelled = False

    def __enter__(self):
        global _active
        for k, v in LOW_CVARS.items():
            self.saved[k] = _get(k)
            _cmd("%s %s" % (k, v))
        _set_realtime(False)
        self.task = unreal.ScopedSlowTask(self.total + 1, "Aerondel: " + self.title + (" (small test)" if self.small else ""))
        self.task.__enter__()
        self.task.make_dialog(True)
        _active = self
        unreal.log("[Aerondel] %s: low-GPU safe mode on (batch %d, cell cap %d%s)" % (self.title, BATCH, CELL_CAP, ", SMALL TEST" if self.small else ""))
        return self

    def frame(self, msg):
        self._check()
        self.task.enter_progress_frame(1, msg)
        self.breathe(save=True)

    def tick(self, n=1):
        self.count += n
        self.since += n
        if self.since >= BATCH:
            self.since = 0
            self.breathe()

    def breathe(self, save=False):
        self._check()
        try:
            unreal.SystemLibrary.collect_garbage()
        except Exception:
            pass
        self.task.enter_progress_frame(0, "%s: %d actors placed" % (self.title, self.count))
        time.sleep(0.05)  # let the driver drain queued GPU work (TDR is 2 s)
        if save and self.save_fn:
            try:
                self.save_fn()
            except Exception as e:
                unreal.log_warning("[Aerondel] intermediate save failed: %s" % e)

    def allow(self, x_m, z_m, decor):
        if not decor:
            return True
        key = (math.floor(x_m * 100.0 / CELL_CM), math.floor(z_m * 100.0 / CELL_CM))
        n = self.cells.get(key, 0)
        if n >= CELL_CAP:
            self.skipped += 1
            return False
        self.cells[key] = n + 1
        return True

    def _check(self):
        if self.task and self.task.should_cancel():
            raise Cancelled()

    def __exit__(self, et, ev, tb):
        global _active
        _active = None
        try:
            self.task.__exit__(None, None, None)
        except Exception:
            pass
        for k, v in self.saved.items():
            if v is not None:
                _cmd("%s %s" % (k, int(v) if float(v).is_integer() else v))
        _set_realtime(True)
        unreal.log("[Aerondel] %s: %d actors placed, %d decorative pieces skipped by the per-cell cap; editor scalability restored."
                   % (self.title, self.count, self.skipped))
        if et is Cancelled:
            self.cancelled = True
            unreal.log_warning("[Aerondel] %s cancelled; partial level kept (re-run to finish)." % self.title)
            return True
        return False
