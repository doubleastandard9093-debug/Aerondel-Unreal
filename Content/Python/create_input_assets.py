"""
Gielinor > Create Input Assets
Creates Enhanced Input actions and two mapping contexts (paths match UGielinorInputConfig defaults):
  IMC_Desktop: keyboard + mouse + gamepad
  IMC_XR:      OpenXR controllers (Meta Quest Touch, Valve Index). Hand tracking pinch is handled in C++.
"""
import unreal
from gielinor_util import ROOT, load_or_create, save

P = ROOT + "/Input"
VT = unreal.InputActionValueType


def action(name, value_type):
    a = load_or_create(f"{P}/{name}", unreal.InputAction, unreal.InputAction_Factory())
    a.set_editor_property("value_type", value_type)
    save(a)
    return a


def key(name):
    k = unreal.Key()
    k.set_editor_property("key_name", name)
    return k


def mod(cls, outer, **props):
    m = unreal.new_object(cls, outer=outer)
    for k, v in props.items():
        m.set_editor_property(k, v)
    return m


def mapping(imc, act, key_name, *modifiers):
    m = unreal.EnhancedActionKeyMapping()
    m.set_editor_property("action", act)
    m.set_editor_property("key", key(key_name))
    if modifiers:
        m.set_editor_property("modifiers", list(modifiers))
    return m


def write_mappings(imc, maps):
    """UE 5.6 keeps mappings in DefaultKeyMappings.Mappings; older versions use Mappings directly."""
    try:
        data = imc.get_editor_property("default_key_mappings")
        data.set_editor_property("mappings", maps)
        imc.set_editor_property("default_key_mappings", data)
    except Exception:
        imc.set_editor_property("mappings", maps)
    save(imc)


def run():
    # ---------------- Desktop ----------------
    move = action("IA_Move", VT.AXIS2D)
    look = action("IA_Look", VT.AXIS2D)
    orbit = action("IA_OrbitHold", VT.BOOLEAN)
    zoom = action("IA_Zoom", VT.AXIS1D)
    click = action("IA_Click", VT.BOOLEAN)
    interact = action("IA_Interact", VT.BOOLEAN)
    jump = action("IA_Jump", VT.BOOLEAN)
    inv = action("IA_ToggleInventory", VT.BOOLEAN)

    imc = load_or_create(f"{P}/IMC_Desktop", unreal.InputMappingContext, unreal.InputMappingContext_Factory())
    S = unreal.InputAxisSwizzle
    swz = lambda: mod(unreal.InputModifierSwizzleAxis, imc, order=S.YXZ)
    neg = lambda: mod(unreal.InputModifierNegate, imc)
    dead = lambda: mod(unreal.InputModifierDeadZone, imc, lower_threshold=0.2)
    scale = lambda s: mod(unreal.InputModifierScalar, imc, scalar=unreal.Vector(s, s, s))
    maps = [
        # Gamepad rows live in C++ (UGielinorGamepadComponent, runtime IMC above this one).
        # WASD -> 2D (X = right, Y = forward)
        mapping(imc, move, "W", swz()),
        mapping(imc, move, "S", swz(), neg()),
        mapping(imc, move, "D"),
        mapping(imc, move, "A", neg()),
        mapping(imc, move, "Up", swz()),
        mapping(imc, move, "Down", swz(), neg()),
        mapping(imc, look, "Mouse2D", scale(0.4)),
        mapping(imc, orbit, "RightMouseButton"),
        mapping(imc, orbit, "MiddleMouseButton"),
        mapping(imc, zoom, "MouseWheelAxis"),
        mapping(imc, click, "LeftMouseButton"),
        mapping(imc, interact, "E"),
        mapping(imc, jump, "SpaceBar"),
        mapping(imc, inv, "Tab"),
    ]
    write_mappings(imc, maps)

    # ---------------- XR ----------------
    xmove = action("IA_XR_Move", VT.AXIS2D)
    xturn = action("IA_XR_Turn", VT.AXIS1D)
    xtele = action("IA_XR_Teleport", VT.BOOLEAN)
    gl = action("IA_XR_GrabLeft", VT.BOOLEAN)
    gr = action("IA_XR_GrabRight", VT.BOOLEAN)
    tl = action("IA_XR_TriggerLeft", VT.BOOLEAN)
    tr = action("IA_XR_TriggerRight", VT.BOOLEAN)
    menu = action("IA_XR_Menu", VT.BOOLEAN)

    xmc = load_or_create(f"{P}/IMC_XR", unreal.InputMappingContext, unreal.InputMappingContext_Factory())
    xdead = lambda: mod(unreal.InputModifierDeadZone, xmc, lower_threshold=0.15)
    xmaps = []
    for prefix in ("OculusTouch", "ValveIndex"):
        xmaps += [
            mapping(xmc, xmove, f"{prefix}_Left_Thumbstick_2D", xdead()),
            mapping(xmc, xturn, f"{prefix}_Right_Thumbstick_X", xdead()),
            mapping(xmc, xtele, f"{prefix}_Right_Thumbstick_Up"),
            mapping(xmc, gl, f"{prefix}_Left_Grip_Click" if prefix == "OculusTouch" else f"{prefix}_Left_Grip_Axis"),
            mapping(xmc, gr, f"{prefix}_Right_Grip_Click" if prefix == "OculusTouch" else f"{prefix}_Right_Grip_Axis"),
            mapping(xmc, tl, f"{prefix}_Left_Trigger_Click"),
            mapping(xmc, tr, f"{prefix}_Right_Trigger_Click"),
        ]
    xmaps.append(mapping(xmc, menu, "OculusTouch_Left_Menu_Click"))
    xmaps.append(mapping(xmc, menu, "ValveIndex_Left_B_Click"))
    write_mappings(xmc, xmaps)

    unreal.log(f"[Gielinor] Input assets created in {P}: {len(maps)} desktop + {len(xmaps)} XR mappings.")


if __name__ == "__main__":
    run()
