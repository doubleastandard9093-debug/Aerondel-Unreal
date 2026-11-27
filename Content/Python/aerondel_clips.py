"""Map imported AnimSequence names to idle/walk/run/attack (and fallbacks).

Pure Python so Tools tests can run it without Unreal. Interchange may name a clip
'<Clip>', 'Anim_<Clip>', '<Mesh>_Anim_<Clip>', or '<Mesh>_<Clip>'; matching is
case-insensitive and prefix-tolerant.
"""
from __future__ import annotations

import re

# Ordered regexes, applied to clip_name() output. Prefer an exact state token
# (possibly after a mesh prefix) over a looser substring, and never treat
# Idle_HitReact / Jump_ToIdle / Idle_Gun_Shoot as the standing idle.
CLIP = {
    "idle": [
        r"(^|_|-)idle$",
        r"(^|_|-)(spider_idle|flying_idle|idle_neutral|idle_2|idle_headlow)$",
        r"(^|_|-)(stand|breathe|rest)$",
        r"(^|_|-)idle_(neutral|2|headlow|head_low)$",
    ],
    "walk": [
        r"(^|_|-)walk$",
        r"spider_walk",
        r"fast_flying",
        r"(^|_|-)walk",
        r"walk",
    ],
    "run": [
        r"(^|_|-)run$",
        r"(^|_|-)(gallop|sprint)$",
        r"spider_walk",
        r"fast_flying",
        r"(^|_|-)(run|walk)",
        r"run|walk",
    ],
    "attack": [
        r"sword_slash",
        r"attack-melee-right",
        r"spider_attack",
        r"(^|_|-)(slash|stab|attack|punch|weapon)$",
        r"bite_front|headbutt|attack-melee|attack-kick|attack_kick|attack_headbutt",
        r"bite|headbutt|punch|weapon|slash|stab|kick|cast|swing|spell|shoot|attack",
    ],
    "hit": [
        r"hitre",
        r"(^|_|-)hit$",
        r"hit",
    ],
    "death": [
        r"(^|_|-)(death|die)$",
        r"death",
        r"^die$",
    ],
    "gather": [
        r"(^|_|-)(interact|pick-up|pickup|pick_up)$",
        r"interact|pick-up|pickup",
    ],
}

# Used only when the primary attack patterns miss (unusual clip names).
_ATTACK_ANY = re.compile(
    r"bite|headbutt|punch|weapon|slash|stab|kick|cast|swing|spell|shoot|attack",
    re.I,
)
_HIT_ANY = re.compile(r"hitre|hit", re.I)
_DEATH_ANY = re.compile(r"death|^die$|(^|_|-)die$", re.I)


def clip_name(asset_name):
    """Strip Interchange / Blender prefixes and lowercase the glTF clip name."""
    n = str(asset_name).split("|")[-1].strip()
    n = re.sub(r"(?i)^.*?_Anim_", "", n)
    n = re.sub(r"(?i)^Anim_", "", n)
    return n.lower()


def pick(anims, state):
    """Return the asset path for `state`, or None. `anims` is [(clip_name, path), ...]."""
    pats = CLIP.get(state) or []
    for pat in pats:
        rx = re.compile(pat, re.I)
        for name, path in anims:
            if rx.search(name):
                return path
    return None


def _pick_rx(anims, rx):
    for name, path in anims:
        if rx.search(name):
            return path
    return None


def apply_state_fallbacks(anims, states):
    """Fill missing core states from a related clip. Returns (states, anim_rates).

    If there is no idle, map idle to walk (or run) and set play rate 0 so the
    runtime holds the first frame instead of the T/reference pose. Attack / hit /
    death fall back to a sensible existing clip the same way.
    """
    states = dict(states)
    rates = {}
    if not states.get("idle"):
        fb = states.get("walk") or states.get("run") or pick(anims, "walk") or pick(anims, "run")
        if fb:
            states["idle"] = fb
            rates["idle"] = 0.0
    if not states.get("run"):
        fb = states.get("walk") or pick(anims, "walk")
        if fb:
            states["run"] = fb
    if not states.get("walk"):
        fb = states.get("run") or pick(anims, "run")
        if fb:
            states["walk"] = fb
    if not states.get("attack"):
        fb = _pick_rx(anims, _ATTACK_ANY) or states.get("hit") or pick(anims, "hit")
        if fb:
            states["attack"] = fb
    if not states.get("hit"):
        fb = _pick_rx(anims, _HIT_ANY) or states.get("attack")
        if fb:
            states["hit"] = fb
    if not states.get("death"):
        fb = _pick_rx(anims, _DEATH_ANY) or states.get("hit") or states.get("attack")
        if fb:
            states["death"] = fb
    return states, rates
