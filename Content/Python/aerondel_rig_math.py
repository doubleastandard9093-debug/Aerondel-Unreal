"""Bake Lite glTFs into one bind space and compute the runtime mesh alignment.

Quaternius / Kenney adventurer (and similar) files ship several skinned meshes that each have
their own inverse-bind matrices and a node-level -90 X * 100 transform. Combining them on import
as-is leaves the feet in a different space from the body (lying torso, upright oversized boots).
bake_glb_for_unreal() skins every mesh into the armature's world bind pose, writes one IBM set
(inv of each joint world), identity mesh-node TRS, and flattens CharacterArmature into Root so an
importer that drops non-joint parents still matches.

Alignment (align_bind_pose) maps feet->head to +Z and the character's facing to +X. Offsets in
rigs.json are applied on top and default to 0; the old yaw=-90 default is not used, so a baked
(already upright) mesh is not rotated onto its back.

This module is pure Python (numpy) so Tools/test_import_lite_models.py can run the glb through the
same math the importer and FitRig use, without Unreal.
"""
from __future__ import annotations

import json
import math
import os
import struct
from pathlib import Path

import numpy as np

# glTF Y-up, right-handed -> UE Z-up, left-handed: rotate +90 about X, then flip the new Y
# so (x, y, z) -> (x, -z, y). This is the conversion that stands a baked adventurer (up +Y) onto +Z.
def gltf_to_ue(p):
    x, y, z = p
    return (float(x), float(-z), float(y))


def load_glb(path):
    data = Path(path).read_bytes()
    if data[:4] != b"glTF":
        raise ValueError("not a glb: %s" % path)
    off, length = 12, struct.unpack_from("<I", data, 8)[0]
    gltf = blob = None
    while off < length:
        clen, ctype = struct.unpack_from("<I4s", data, off)
        chunk = data[off + 8 : off + 8 + clen]
        off += 8 + clen
        if ctype == b"JSON":
            gltf = json.loads(chunk)
        elif ctype == b"BIN\x00":
            blob = chunk
    if gltf is None or blob is None:
        raise ValueError("glb missing JSON or BIN: %s" % path)
    return gltf, bytearray(blob)


def write_glb(gltf, blob, path):
    blob = bytes(blob)
    pad = (4 - len(blob) % 4) % 4
    blob += b"\x00" * pad
    gltf = dict(gltf)
    bufs = list(gltf.get("buffers") or [{"byteLength": 0}])
    bufs[0] = dict(bufs[0], byteLength=len(blob))
    gltf["buffers"] = bufs
    js = json.dumps(gltf, separators=(",", ":")).encode("utf-8")
    js += b" " * ((4 - len(js) % 4) % 4)
    total = 12 + 8 + len(js) + 8 + len(blob)
    out = Path(path)
    out.parent.mkdir(parents=True, exist_ok=True)
    with out.open("wb") as fh:
        fh.write(struct.pack("<III", 0x46546C67, 2, total))
        fh.write(struct.pack("<I4s", len(js), b"JSON"))
        fh.write(js)
        fh.write(struct.pack("<I4s", len(blob), b"BIN\x00"))
        fh.write(blob)


def _mat4_trs(t, r, s):
    t, r, s = np.asarray(t, float), np.asarray(r, float), np.asarray(s, float)
    x, y, z, w = r
    sx, sy, sz = s
    xx, yy, zz, xy, xz, yz, wx, wy, wz = x * x, y * y, z * z, x * y, x * z, y * z, w * x, w * y, w * z
    M = np.eye(4)
    M[0, 0], M[0, 1], M[0, 2] = sx * (1 - 2 * (yy + zz)), sy * (2 * (xy - wz)), sz * (2 * (xz + wy))
    M[1, 0], M[1, 1], M[1, 2] = sx * (2 * (xy + wz)), sy * (1 - 2 * (xx + zz)), sz * (2 * (yz - wx))
    M[2, 0], M[2, 1], M[2, 2] = sx * (2 * (xz - wy)), sy * (2 * (yz + wx)), sz * (1 - 2 * (xx + yy))
    M[0:3, 3] = t
    return M


def _node_local(n):
    if "matrix" in n:
        return np.array(n["matrix"], float).reshape(4, 4, order="F")
    return _mat4_trs(
        n.get("translation", [0, 0, 0]),
        n.get("rotation", [0, 0, 0, 1]),
        n.get("scale", [1, 1, 1]),
    )


def node_parents(nodes):
    parent = [-1] * len(nodes)
    for i, n in enumerate(nodes):
        for c in n.get("children") or []:
            parent[c] = i
    return parent


def node_worlds(nodes):
    parent = node_parents(nodes)
    world = [None] * len(nodes)

    def W(i):
        if world[i] is None:
            L = _node_local(nodes[i])
            world[i] = L if parent[i] < 0 else W(parent[i]) @ L
        return world[i]

    for i in range(len(nodes)):
        W(i)
    return world, parent


def _read_accessor(gltf, blob, acc_i):
    a = gltf["accessors"][acc_i]
    bv = gltf["bufferViews"][a["bufferView"]]
    off = (bv.get("byteOffset") or 0) + (a.get("byteOffset") or 0)
    typ = a["type"]
    cols = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT4": 16}[typ]
    fmt, sz, nd = {
        5126: ("f", 4, np.float32),
        5123: ("H", 2, np.uint16),
        5121: ("B", 1, np.uint8),
        5125: ("I", 4, np.uint32),
        5120: ("b", 1, np.int8),
        5122: ("h", 2, np.int16),
    }[a["componentType"]]
    stride = bv.get("byteStride") or cols * sz
    n = a["count"]
    out = np.empty((n, cols), dtype=nd)
    for i in range(n):
        out[i] = struct.unpack_from("<" + fmt * cols, blob, off + i * stride)
    if a.get("normalized") and nd in (np.uint16, np.uint8, np.int8, np.int16):
        den = {np.uint16: 65535.0, np.uint8: 255.0, np.int16: 32767.0, np.int8: 127.0}[type(out[0, 0])]
        # dtype type() of numpy scalar is the numpy type
        den = {np.uint16: 65535.0, np.uint8: 255.0, np.int16: 32767.0, np.int8: 127.0}[out.dtype.type]
        out = out.astype(np.float32) / den
    return out


def _append_f32(blob, data, cols):
    raw = np.asarray(data, dtype=np.float32).reshape(-1, cols).tobytes()
    # 4-byte align
    if len(blob) % 4:
        blob.extend(b"\x00" * (4 - len(blob) % 4))
    off = len(blob)
    blob.extend(raw)
    return off, len(raw)


def _add_view_acc(gltf, offset, length, count, typ, component=5126, target=None):
    cols = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT4": 16}[typ]
    view = {"buffer": 0, "byteOffset": offset, "byteLength": length}
    if target is not None:
        view["target"] = target
    gltf.setdefault("bufferViews", []).append(view)
    vi = len(gltf["bufferViews"]) - 1
    acc = {"bufferView": vi, "componentType": component, "count": count, "type": typ}
    gltf.setdefault("accessors", []).append(acc)
    return len(gltf["accessors"]) - 1


def _read_ibm(gltf, blob, acc_i):
    raw = _read_accessor(gltf, blob, acc_i).reshape(-1, 16)
    return np.array([m.reshape(4, 4, order="F") for m in raw])


def _skin_positions(gltf, blob, prim, Jbind, ibm):
    P = _read_accessor(gltf, blob, prim["attributes"]["POSITION"])[:, :3].astype(np.float64)
    J = _read_accessor(gltf, blob, prim["attributes"]["JOINTS_0"])
    Wt = _read_accessor(gltf, blob, prim["attributes"]["WEIGHTS_0"]).astype(np.float64)
    s = Wt.sum(axis=1, keepdims=True)
    s[s == 0] = 1.0
    Wt = Wt / s
    P4 = np.hstack([P, np.ones((len(P), 1))])
    out = np.zeros((len(P), 3), np.float64)
    nrm_out = None
    N = None
    if "NORMAL" in prim["attributes"]:
        N = _read_accessor(gltf, blob, prim["attributes"]["NORMAL"])[:, :3].astype(np.float64)
        nrm_out = np.zeros_like(N)
    for k in range(min(4, J.shape[1])):
        js = J[:, k].astype(int)
        for ji in np.unique(js):
            mask = (js == ji) & (Wt[:, k] > 0)
            if not np.any(mask):
                continue
            M = Jbind[int(ji)] @ ibm[int(ji)]
            out[mask] += Wt[mask, k : k + 1] * (M @ P4[mask].T).T[:, :3]
            if nrm_out is not None:
                R = np.linalg.inv(M[:3, :3]).T
                nrm_out[mask] += Wt[mask, k : k + 1] * (R @ N[mask].T).T
    if nrm_out is not None:
        ln = np.linalg.norm(nrm_out, axis=1, keepdims=True)
        ln[ln == 0] = 1.0
        nrm_out = nrm_out / ln
    return out, nrm_out


def _flatten_armature(gltf, joint_set, parent):
    """Compose each non-joint ancestor of the first joint into that joint and reparent it up."""
    nodes = gltf["nodes"]
    skins = gltf.get("skins") or []
    if not skins or not skins[0].get("joints"):
        return
    first = skins[0]["joints"][0]
    # Collect non-joint ancestors, nearest first.
    anc = []
    i = parent[first]
    while i >= 0:
        if i not in joint_set:
            anc.append(i)
        i = parent[i]
    if not anc:
        return
    # Product of ancestor locals (rootward last) * first.local
    M = _node_local(nodes[first])
    for a in anc:  # nearest parent first; we want farthest * ... * nearest * first
        pass
    M = np.eye(4)
    for a in reversed(anc):
        M = M @ _node_local(nodes[a])
    M = M @ _node_local(nodes[first])
    # Write TRS onto the first joint (column-major decompose: assume no shear)
    t = M[0:3, 3]
    R = M[0:3, 0:3]
    sx, sy, sz = [np.linalg.norm(R[:, c]) for c in range(3)]
    if min(sx, sy, sz) < 1e-12:
        sx = sy = sz = 1.0
    Rn = np.column_stack([R[:, c] / (sx, sy, sz)[c] for c in range(3)])
    # rotation matrix to quat (xyzw)
    tr = Rn[0, 0] + Rn[1, 1] + Rn[2, 2]
    if tr > 0:
        s = math.sqrt(tr + 1.0) * 2
        qw, qx, qy, qz = 0.25 * s, (Rn[2, 1] - Rn[1, 2]) / s, (Rn[0, 2] - Rn[2, 0]) / s, (Rn[1, 0] - Rn[0, 1]) / s
    else:
        # pick largest diag
        if Rn[0, 0] > Rn[1, 1] and Rn[0, 0] > Rn[2, 2]:
            s = math.sqrt(1.0 + Rn[0, 0] - Rn[1, 1] - Rn[2, 2]) * 2
            qw, qx, qy, qz = (Rn[2, 1] - Rn[1, 2]) / s, 0.25 * s, (Rn[0, 1] + Rn[1, 0]) / s, (Rn[0, 2] + Rn[2, 0]) / s
        elif Rn[1, 1] > Rn[2, 2]:
            s = math.sqrt(1.0 + Rn[1, 1] - Rn[0, 0] - Rn[2, 2]) * 2
            qw, qx, qy, qz = (Rn[0, 2] - Rn[2, 0]) / s, (Rn[0, 1] + Rn[1, 0]) / s, 0.25 * s, (Rn[1, 2] + Rn[2, 1]) / s
        else:
            s = math.sqrt(1.0 + Rn[2, 2] - Rn[0, 0] - Rn[1, 1]) * 2
            qw, qx, qy, qz = (Rn[1, 0] - Rn[0, 1]) / s, (Rn[0, 2] + Rn[2, 0]) / s, (Rn[1, 2] + Rn[2, 1]) / s, 0.25 * s
    n = nodes[first]
    n.pop("matrix", None)
    n["translation"] = [float(t[0]), float(t[1]), float(t[2])]
    n["rotation"] = [float(qx), float(qy), float(qz), float(qw)]
    n["scale"] = [float(sx), float(sy), float(sz)]
    # Reparent first joint to the top ancestor's parent (usually the scene root).
    top = anc[-1]
    new_parent = parent[top]
    # Remove first from its current parent's children.
    cur = parent[first]
    if cur >= 0 and first in (nodes[cur].get("children") or []):
        nodes[cur]["children"] = [c for c in nodes[cur]["children"] if c != first]
    if new_parent >= 0:
        ch = list(nodes[new_parent].get("children") or [])
        if first not in ch:
            ch.append(first)
        nodes[new_parent]["children"] = ch
    else:
        # attach to every scene root list
        for sc in gltf.get("scenes") or []:
            roots = list(sc.get("nodes") or [])
            if first not in roots:
                roots.append(first)
            sc["nodes"] = roots
    # Clear ancestor TRS so leftover empty nodes don't move anything if kept.
    for a in anc:
        an = nodes[a]
        an.pop("matrix", None)
        an["translation"] = [0, 0, 0]
        an["rotation"] = [0, 0, 0, 1]
        an["scale"] = [1, 1, 1]


def bake_glb_for_unreal(src, dst):
    """Rewrite a Lite rigged glb into one bind space. Returns dst."""
    gltf, blob = load_glb(src)
    nodes = gltf["nodes"]
    worlds, parent = node_worlds(nodes)
    skins = gltf.get("skins") or []
    if not skins:
        write_glb(gltf, blob, dst)
        return dst
    joints = skins[0]["joints"]
    joint_set = set(joints)
    Jbind = [worlds[j] for j in joints]
    new_ibm = np.stack([np.linalg.inv(Jw) for Jw in Jbind], axis=0)

    # Append identical IBM accessor used by every skin.
    ibm_flat = np.array([m.reshape(16, order="F") for m in new_ibm], dtype=np.float32)
    off, length = _append_f32(blob, ibm_flat, 16)
    ibm_acc = _add_view_acc(gltf, off, length, len(joints), "MAT4")
    for skin in skins:
        skin["inverseBindMatrices"] = ibm_acc
        skin["joints"] = list(joints)  # share the same joint list

    # Re-read originals from src, not the mutated gltf (IBM index already changed).
        src_gltf, src_blob = load_glb(src)
    for si, skin in enumerate(src_gltf.get("skins") or []):
        ibm_old = _read_ibm(src_gltf, src_blob, skin["inverseBindMatrices"])
        # Map this skin's joint indices to the shared joints list if they differ.
        src_joints = skin["joints"]
        # We assume the same 62 joints in the same order (true for adventurer). If not, permute.
        if src_joints != joints:
            # build IBM in shared order: look up by node index
            by_node = {src_joints[k]: ibm_old[k] for k in range(len(src_joints))}
            ibm_old = np.stack([by_node[j] for j in joints])
            # And JOINTS_0 on the primitive index into src_joints — must remap.
            remap = {old_i: joints.index(src_joints[old_i]) for old_i in range(len(src_joints))}
        else:
            remap = None
        for ni, n in enumerate(nodes):
            if n.get("skin") != si or "mesh" not in n:
                continue
            mesh = gltf["meshes"][n["mesh"]]
            for pi, prim in enumerate(mesh["primitives"]):
                # Joints on this prim index the skin's joint array (src_joints). If we remapped
                # IBM to shared order, also remap JOINTS_0 — do that inside a copy of prim attrs.
                prim_work = prim
                P, N = _skin_positions(src_gltf, src_blob, prim, Jbind, ibm_old)
                off, length = _append_f32(blob, P, 3)
                acc_i = _add_view_acc(gltf, off, length, len(P), "VEC3", target=34962)
                # min/max
                gltf["accessors"][acc_i]["min"] = [float(x) for x in P.min(0)]
                gltf["accessors"][acc_i]["max"] = [float(x) for x in P.max(0)]
                prim["attributes"]["POSITION"] = acc_i
                if N is not None:
                    off, length = _append_f32(blob, N, 3)
                    prim["attributes"]["NORMAL"] = _add_view_acc(gltf, off, length, len(N), "VEC3", target=34962)
            # Mesh node becomes identity — vertices are already in world / bind space.
            n.pop("matrix", None)
            n["translation"] = [0, 0, 0]
            n["rotation"] = [0, 0, 0, 1]
            n["scale"] = [1, 1, 1]

    _flatten_armature(gltf, joint_set, parent)
    write_glb(gltf, blob, dst)
    return dst


# ----- bind-pose landmarks and alignment (UE-space math, used by tests and documented for C++) -----

def q_mul(a, b):
    ax, ay, az, aw = a
    bx, by, bz, bw = b
    return (
        aw * bx + ax * bw + ay * bz - az * by,
        aw * by - ax * bz + ay * bw + az * bx,
        aw * bz + ax * by - ay * bx + az * bw,
        aw * bw - ax * bx - ay * by - az * bz,
    )


def q_rot(q, v):
    x, y, z, w = q
    vx, vy, vz = v
    tx, ty, tz = 2 * (y * vz - z * vy), 2 * (z * vx - x * vz), 2 * (x * vy - y * vx)
    return (
        vx + w * tx + (y * tz - z * ty),
        vy + w * ty + (z * tx - x * tz),
        vz + w * tz + (x * ty - y * tx),
    )


def q_normalize(q):
    x, y, z, w = q
    n = math.sqrt(x * x + y * y + z * z + w * w) or 1.0
    return (x / n, y / n, z / n, w / n)


def find_between(frm, to):
    """FQuat::FindBetweenNormals — rotation taking frm to to. Both should be unit."""
    fx, fy, fz = frm
    tx, ty, tz = to
    cx, cy, cz = fy * tz - fz * ty, fz * tx - fx * tz, fx * ty - fy * tx
    d = fx * tx + fy * ty + fz * tz
    if d < -0.999999:
        ax, ay, az = (0, 1, 0) if abs(fx) > 0.9 else (1, 0, 0)
        ix, iy, iz = fy * az - fz * ay, fz * ax - fx * az, fx * ay - fy * ax
        L = math.sqrt(ix * ix + iy * iy + iz * iz) or 1.0
        return (ix / L, iy / L, iz / L, 0.0)
    s = math.sqrt((1.0 + d) * 2.0)
    if s < 1e-8:
        return (0.0, 0.0, 0.0, 1.0)
    inv = 1.0 / s
    return q_normalize((cx * inv, cy * inv, cz * inv, s * 0.5))


def _vsub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def _vadd(a, b):
    return (a[0] + b[0], a[1] + b[1], a[2] + b[2])


def _vscale(a, s):
    return (a[0] * s, a[1] * s, a[2] * s)


def _vdot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def _vcross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def _vnorm(a):
    L = math.sqrt(_vdot(a, a))
    if L < 1e-12:
        return (0.0, 0.0, 0.0), 0.0
    return _vscale(a, 1.0 / L), L


def align_bind_pose(head, feet, left=None, right=None, extra_pitch=0.0, extra_yaw=0.0, extra_roll=0.0):
    """Return a quaternion that maps feet->head to +Z and (optional) left->right so facing is +X.

    Positions are in UE component space (Z up). extra_* are additional UE rotator degrees applied
    after the automatic align (defaults 0 — do not pass the legacy yaw=-90).
    """
    up, _ = _vnorm(_vsub(head, feet))
    if _vdot(up, up) < 0.5:
        return (0.0, 0.0, 0.0, 1.0)
    q_up = find_between(up, (0.0, 0.0, 1.0))
    q = q_up
    if left is not None and right is not None:
        across, L = _vnorm(_vsub(right, left))  # character's right
        if L > 1e-8:
            # UE left-handed: right × up = forward (Y × Z = X).
            fwd, fl = _vnorm(_vcross(across, up))
            if fl > 1e-8:
                fwd1 = q_rot(q_up, fwd)
                horiz, hl = _vnorm((fwd1[0], fwd1[1], 0.0))
                if hl > 1e-8:
                    q = q_mul(find_between(horiz, (1.0, 0.0, 0.0)), q_up)
    if extra_pitch or extra_yaw or extra_roll:
        q = q_mul(_rotator_quat(extra_pitch, extra_yaw, extra_roll), q)
    return q_normalize(q)


def _rotator_quat(pitch, yaw, roll):
    """UE FRotator: Q = Qyaw * Qpitch * Qroll, degrees."""
    def aa(axis, deg):
        h = math.radians(deg) * 0.5
        s, c = math.sin(h), math.cos(h)
        return (axis[0] * s, axis[1] * s, axis[2] * s, c)
    return q_mul(q_mul(aa((0, 0, 1), yaw), aa((0, 1, 0), pitch)), aa((1, 0, 0), roll))


def is_foot_name(name):
    n = name.lower()
    if n.startswith("pt.") or n.startswith("pt_") or n in ("pt.l", "pt.r"):
        return False  # pole targets, not soles
    return any(k in n for k in ("foot", "toe", "ankle")) and "end" not in n


def is_head_name(name):
    n = name.lower()
    return "head" in n and "end" not in n and "top" not in n


def is_left_name(name):
    n = name.lower()
    return n.endswith(".l") or n.endswith("_l") or ".l." in n or "_l_" in n or "left" in n


def is_right_name(name):
    n = name.lower()
    return n.endswith(".r") or n.endswith("_r") or ".r." in n or "_r_" in n or "right" in n


def joint_world_positions(gltf, blob=None):
    """Name -> world position of every node, from the current (possibly baked) TRS."""
    nodes = gltf["nodes"]
    worlds, _ = node_worlds(nodes)
    out = {}
    for i, n in enumerate(nodes):
        M = worlds[i]
        out[n.get("name") or ("node_%d" % i)] = (float(M[0, 3]), float(M[1, 3]), float(M[2, 3]))
    return out


def landmarks(positions):
    """From a name->pos map, pick head, feet average, left/right (shoulders, else hips, else feet)."""
    head = None
    feet, lefts, rights = [], [], []
    shoulders_l, shoulders_r = [], []
    hips_l, hips_r = [], []
    for name, p in positions.items():
        if is_head_name(name) and head is None:
            head = p
        if is_foot_name(name):
            feet.append(p)
        n = name.lower()
        if "shoulder" in n or "upperarm" in n or "arm" in n and "fore" not in n and "lower" not in n:
            if is_left_name(name):
                shoulders_l.append(p)
            elif is_right_name(name):
                shoulders_r.append(p)
        if "upperleg" in n or "thigh" in n or "upleg" in n or n.startswith("hip"):
            if is_left_name(name):
                hips_l.append(p)
            elif is_right_name(name):
                hips_r.append(p)
    if head is None or not feet:
        return None
    def avg(pts):
        s = [0.0, 0.0, 0.0]
        for p in pts:
            s[0] += p[0]; s[1] += p[1]; s[2] += p[2]
        k = float(len(pts))
        return (s[0] / k, s[1] / k, s[2] / k)
    feet_p = avg(feet)
    left = right = None
    if shoulders_l and shoulders_r:
        left, right = avg(shoulders_l), avg(shoulders_r)
    elif hips_l and hips_r:
        left, right = avg(hips_l), avg(hips_r)
    else:
        fl = [p for name, p in positions.items() if is_foot_name(name) and is_left_name(name)]
        fr = [p for name, p in positions.items() if is_foot_name(name) and is_right_name(name)]
        if fl and fr:
            left, right = avg(fl), avg(fr)
    return {"head": head, "feet": feet_p, "left": left, "right": right, "foot_points": feet}


def mesh_vertex_aabb(gltf, blob, name=None):
    """AABB of POSITION accessors (after bake, this is bind-world). Optional mesh node name filter."""
    nodes = gltf["nodes"]
    ids = None
    if name:
        ids = {n["mesh"] for n in nodes if n.get("name") == name and "mesh" in n}
    mins, maxs = [], []
    for i, mesh in enumerate(gltf.get("meshes") or []):
        if ids is not None and i not in ids:
            continue
        for prim in mesh.get("primitives") or []:
            P = _read_accessor(gltf, blob, prim["attributes"]["POSITION"])[:, :3]
            mins.append(P.min(0)); maxs.append(P.max(0))
    if not mins:
        return None
    mn = np.min(np.stack(mins), 0); mx = np.max(np.stack(maxs), 0)
    return mn, mx
