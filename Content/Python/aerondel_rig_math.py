"""Bake Lite glTFs into one Interchange-legal bind space and compute runtime alignment.

Quaternius / Kenney adventurer (and similar) files ship several skinned meshes that each have
their own inverse-bind matrices and a non-joint CharacterArmature parent at -90 X * scale 100.
Interchange will ignore every IBM when multiple skins share joints with different matrices, then
drop parent transforms of any skinned mesh that is not a scene root — leaving a lying bind pose
and split feet. This baker:

  * is pure Python (no numpy) so it runs inside Unreal Editor's Python as well as offline tests;
  * merges every skinned primitive into ONE mesh with ONE skin and ONE IBM set (inv of each
    joint's world bind);
  * places the skinned mesh node at the scene root with identity TRS;
  * flattens CharacterArmature into Root and bakes the armature scale into every descendant's
    local translation (and translation animation keys) so joint rest TRS itself stands upright
    in glTF Y-up (head above feet) even if an importer ignores the IBMs.

Alignment (align_bind_pose) maps feet->head to +Z and facing to +X. Offsets in rigs.json default
to 0; the old yaw=-90 default is not used.
"""
from __future__ import annotations

import json
import math
import struct
from pathlib import Path


# glTF Y-up, right-handed -> UE Z-up, left-handed: (x, y, z) -> (x, -z, y).
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


# ----- 4x4 matrices as row-major nested lists (no numpy) -----

def mat4_identity():
    return [[1.0, 0.0, 0.0, 0.0],
            [0.0, 1.0, 0.0, 0.0],
            [0.0, 0.0, 1.0, 0.0],
            [0.0, 0.0, 0.0, 1.0]]


def mat4_mul(A, B):
    C = [[0.0, 0.0, 0.0, 0.0] for _ in range(4)]
    for i in range(4):
        Ai = A[i]
        for j in range(4):
            C[i][j] = Ai[0] * B[0][j] + Ai[1] * B[1][j] + Ai[2] * B[2][j] + Ai[3] * B[3][j]
    return C


def mat4_mul_vec4(M, v):
    x, y, z, w = v
    return (
        M[0][0] * x + M[0][1] * y + M[0][2] * z + M[0][3] * w,
        M[1][0] * x + M[1][1] * y + M[1][2] * z + M[1][3] * w,
        M[2][0] * x + M[2][1] * y + M[2][2] * z + M[2][3] * w,
        M[3][0] * x + M[3][1] * y + M[3][2] * z + M[3][3] * w,
    )


def mat4_translation(M):
    return (float(M[0][3]), float(M[1][3]), float(M[2][3]))


def mat4_from_colmajor(m16):
    M = mat4_identity()
    for c in range(4):
        for r in range(4):
            M[r][c] = float(m16[c * 4 + r])
    return M


def mat4_to_colmajor(M):
    out = []
    for c in range(4):
        for r in range(4):
            out.append(float(M[r][c]))
    return out


def mat4_close(A, B, eps=1e-3):
    for i in range(4):
        for j in range(4):
            if abs(A[i][j] - B[i][j]) > eps:
                return False
    return True


def mat4_inv(M):
    """Invert a 4x4 via Gauss-Jordan. Raises if the matrix is singular."""
    A = [M[i][:] + [1.0 if i == j else 0.0 for j in range(4)] for i in range(4)]
    for col in range(4):
        piv = col
        best = abs(A[col][col])
        for r in range(col + 1, 4):
            v = abs(A[r][col])
            if v > best:
                best, piv = v, r
        if best < 1e-18:
            raise ValueError("singular 4x4")
        if piv != col:
            A[col], A[piv] = A[piv], A[col]
        div = A[col][col]
        row = A[col]
        invd = 1.0 / div
        for j in range(8):
            row[j] *= invd
        for r in range(4):
            if r == col:
                continue
            f = A[r][col]
            if f == 0.0:
                continue
            src = A[r]
            for j in range(8):
                src[j] -= f * row[j]
    return [A[i][4:] for i in range(4)]


def _mat4_trs(t, r, s):
    tx, ty, tz = (t or [0, 0, 0])
    x, y, z, w = (r or [0, 0, 0, 1])
    sx, sy, sz = (s or [1, 1, 1])
    xx, yy, zz = x * x, y * y, z * z
    xy, xz, yz = x * y, x * z, y * z
    wx, wy, wz = w * x, w * y, w * z
    M = mat4_identity()
    M[0][0], M[0][1], M[0][2], M[0][3] = sx * (1 - 2 * (yy + zz)), sy * (2 * (xy - wz)), sz * (2 * (xz + wy)), float(tx)
    M[1][0], M[1][1], M[1][2], M[1][3] = sx * (2 * (xy + wz)), sy * (1 - 2 * (xx + zz)), sz * (2 * (yz - wx)), float(ty)
    M[2][0], M[2][1], M[2][2], M[2][3] = sx * (2 * (xz - wy)), sy * (2 * (yz + wx)), sz * (1 - 2 * (xx + yy)), float(tz)
    return M


def _mat4_to_trs(M):
    t = [float(M[0][3]), float(M[1][3]), float(M[2][3])]
    col = lambda c: (M[0][c], M[1][c], M[2][c])
    def nrm(v):
        L = math.sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2])
        return (0.0, 0.0, 0.0) if L < 1e-18 else (v[0] / L, v[1] / L, v[2] / L), L
    cx, sx = nrm(col(0))
    cy, sy = nrm(col(1))
    cz, sz = nrm(col(2))
    if min(sx, sy, sz) < 1e-12:
        sx = sy = sz = 1.0
        cx, cy, cz = (1, 0, 0), (0, 1, 0), (0, 0, 1)
    # If the basis is a reflection, flip one axis so the quat is a rotation.
    det = (cx[0] * (cy[1] * cz[2] - cy[2] * cz[1])
           - cx[1] * (cy[0] * cz[2] - cy[2] * cz[0])
           + cx[2] * (cy[0] * cz[1] - cy[1] * cz[0]))
    if det < 0:
        sz = -sz
        cz = (-cz[0], -cz[1], -cz[2])
    Rn = (cx, cy, cz)  # columns
    # Matrix-to-quat (xyzw). Columns are the rotated basis.
    r00, r10, r20 = cx
    r01, r11, r21 = cy
    r02, r12, r22 = cz
    tr = r00 + r11 + r22
    if tr > 0:
        s = math.sqrt(tr + 1.0) * 2.0
        qw, qx, qy, qz = 0.25 * s, (r21 - r12) / s, (r02 - r20) / s, (r10 - r01) / s
    elif r00 > r11 and r00 > r22:
        s = math.sqrt(1.0 + r00 - r11 - r22) * 2.0
        qw, qx, qy, qz = (r21 - r12) / s, 0.25 * s, (r01 + r10) / s, (r02 + r20) / s
    elif r11 > r22:
        s = math.sqrt(1.0 + r11 - r00 - r22) * 2.0
        qw, qx, qy, qz = (r02 - r20) / s, (r01 + r10) / s, 0.25 * s, (r12 + r21) / s
    else:
        s = math.sqrt(1.0 + r22 - r00 - r11) * 2.0
        qw, qx, qy, qz = (r10 - r01) / s, (r02 + r20) / s, (r12 + r21) / s, 0.25 * s
    return t, [float(qx), float(qy), float(qz), float(qw)], [float(sx), float(sy), float(sz)]


def _node_trs_components(n):
    if "matrix" in n:
        return _mat4_to_trs(mat4_from_colmajor(n["matrix"]))
    return (
        list(n.get("translation") or [0, 0, 0]),
        list(n.get("rotation") or [0, 0, 0, 1]),
        list(n.get("scale") or [1, 1, 1]),
    )


def _write_trs(n, t, r, s):
    n.pop("matrix", None)
    n["translation"] = [float(t[0]), float(t[1]), float(t[2])]
    n["rotation"] = [float(r[0]), float(r[1]), float(r[2]), float(r[3])]
    n["scale"] = [float(s[0]), float(s[1]), float(s[2])]


def _clear_trs_identity(n):
    n.pop("matrix", None)
    n.pop("translation", None)
    n.pop("rotation", None)
    n.pop("scale", None)


def is_identity_node(n, eps=1e-4):
    if "matrix" in n:
        return mat4_close(mat4_from_colmajor(n["matrix"]), mat4_identity(), eps)
    t, r, s = _node_trs_components(n)
    if any(abs(x) > eps for x in t):
        return False
    if any(abs(x - 1.0) > eps for x in s):
        return False
    # q and -q are the same rotation
    if r[3] < 0:
        r = [-r[0], -r[1], -r[2], -r[3]]
    return abs(r[0]) <= eps and abs(r[1]) <= eps and abs(r[2]) <= eps and abs(r[3] - 1.0) <= 1e-3


def _node_local(n):
    if "matrix" in n:
        return mat4_from_colmajor(n["matrix"])
    t, r, s = _node_trs_components(n)
    return _mat4_trs(t, r, s)


def node_parents(nodes):
    parent = [-1] * len(nodes)
    for i, n in enumerate(nodes):
        for c in n.get("children") or []:
            parent[int(c)] = i
    return parent


def node_worlds(nodes):
    parent = node_parents(nodes)
    world = [None] * len(nodes)

    def W(i):
        if world[i] is None:
            L = _node_local(nodes[i])
            world[i] = L if parent[i] < 0 else mat4_mul(W(parent[i]), L)
        return world[i]

    for i in range(len(nodes)):
        W(i)
    return world, parent


# ----- accessors -----

_COMPONENT = {
    5126: ("f", 4),
    5123: ("H", 2),
    5121: ("B", 1),
    5125: ("I", 4),
    5120: ("b", 1),
    5122: ("h", 2),
}
_COLS = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT4": 16}
_NORM_DEN = {5123: 65535.0, 5121: 255.0, 5122: 32767.0, 5120: 127.0}


def _read_accessor(gltf, blob, acc_i):
    a = gltf["accessors"][acc_i]
    bv = gltf["bufferViews"][a["bufferView"]]
    off = (bv.get("byteOffset") or 0) + (a.get("byteOffset") or 0)
    typ = a["type"]
    cols = _COLS[typ]
    fmt, sz = _COMPONENT[a["componentType"]]
    stride = bv.get("byteStride") or cols * sz
    n = a["count"]
    out = []
    pack = "<" + fmt * cols
    for i in range(n):
        out.append(list(struct.unpack_from(pack, blob, off + i * stride)))
    if a.get("normalized") and a["componentType"] in _NORM_DEN:
        den = _NORM_DEN[a["componentType"]]
        out = [[c / den for c in row] for row in out]
    return out


def _append_f32(blob, rows, cols):
    if len(blob) % 4:
        blob.extend(b"\x00" * (4 - len(blob) % 4))
    off = len(blob)
    flat = []
    if rows and not isinstance(rows[0], (list, tuple)):
        flat = [float(x) for x in rows]
    else:
        for row in rows:
            if cols == 1 and not isinstance(row, (list, tuple)):
                flat.append(float(row))
            else:
                flat.extend(float(x) for x in row[:cols])
    raw = struct.pack("<" + "f" * len(flat), *flat)
    blob.extend(raw)
    return off, len(raw)


def _append_ints(blob, rows, cols, component=5121):
    fmt, sz = _COMPONENT[component]
    if len(blob) % 4:
        blob.extend(b"\x00" * (4 - len(blob) % 4))
    off = len(blob)
    flat = []
    for row in rows:
        if cols == 1 and not isinstance(row, (list, tuple)):
            flat.append(int(row))
        else:
            flat.extend(int(x) for x in row[:cols])
    raw = struct.pack("<" + fmt * len(flat), *flat)
    blob.extend(raw)
    # pad to 4
    if len(blob) % 4:
        blob.extend(b"\x00" * (4 - len(blob) % 4))
    return off, len(raw)  # length before pad is what views should use? Use actual written unpadded.
    # glTF byteLength can include padding; use unpadded length.


def _add_view_acc(gltf, offset, length, count, typ, component=5126, target=None):
    view = {"buffer": 0, "byteOffset": offset, "byteLength": length}
    if target is not None:
        view["target"] = target
    gltf.setdefault("bufferViews", []).append(view)
    vi = len(gltf["bufferViews"]) - 1
    acc = {"bufferView": vi, "componentType": component, "count": count, "type": typ}
    gltf.setdefault("accessors", []).append(acc)
    return len(gltf["accessors"]) - 1


def _read_ibm(gltf, blob, acc_i):
    raw = _read_accessor(gltf, blob, acc_i)
    out = []
    for row in raw:
        if len(row) != 16:
            # already a MAT4 flattened per accessor type
            raise ValueError("IBM accessor is not MAT4")
        out.append(mat4_from_colmajor(row))
    return out


def _aabb(points):
    xs, ys, zs = zip(*[(p[0], p[1], p[2]) for p in points])
    return [min(xs), min(ys), min(zs)], [max(xs), max(ys), max(zs)]


def _skin_positions(gltf, blob, prim, Jbind, ibm, joint_remap=None):
    P = _read_accessor(gltf, blob, prim["attributes"]["POSITION"])
    J = _read_accessor(gltf, blob, prim["attributes"]["JOINTS_0"])
    Wt = _read_accessor(gltf, blob, prim["attributes"]["WEIGHTS_0"])
    n = len(P)
    # Precompute bind-skin matrices per joint: Jbind * ibm
    nJ = len(Jbind)
    Ms = [mat4_mul(Jbind[i], ibm[i]) for i in range(nJ)]
    Nsrc = None
    Tsrc = None
    if "NORMAL" in prim["attributes"]:
        Nsrc = _read_accessor(gltf, blob, prim["attributes"]["NORMAL"])
    if "TANGENT" in prim["attributes"]:
        Tsrc = _read_accessor(gltf, blob, prim["attributes"]["TANGENT"])
    out = [[0.0, 0.0, 0.0] for _ in range(n)]
    nrm = [[0.0, 0.0, 0.0] for _ in range(n)] if Nsrc is not None else None
    tan = [[0.0, 0.0, 0.0, 0.0] for _ in range(n)] if Tsrc is not None else None
    influences = min(4, len(J[0]) if J else 0)
    for i in range(n):
        wsum = 0.0
        wi = []
        for k in range(influences):
            w = float(Wt[i][k])
            if w == 0.0:
                continue
            ji = int(J[i][k])
            if joint_remap is not None:
                ji = joint_remap[ji]
            wi.append((ji, w))
            wsum += w
        if wsum <= 0:
            out[i] = [float(P[i][0]), float(P[i][1]), float(P[i][2])]
            if nrm is not None:
                nrm[i] = [float(Nsrc[i][0]), float(Nsrc[i][1]), float(Nsrc[i][2])]
            if tan is not None:
                tan[i] = [float(x) for x in Tsrc[i][:4]]
            continue
        invw = 1.0 / wsum
        px, py, pz = float(P[i][0]), float(P[i][1]), float(P[i][2])
        ox = oy = oz = 0.0
        nx = ny = nz = 0.0
        tx = ty = tz = 0.0
        tw = float(Tsrc[i][3]) if tan is not None and len(Tsrc[i]) > 3 else 1.0
        for ji, w in wi:
            w *= invw
            M = Ms[ji]
            ox += w * (M[0][0] * px + M[0][1] * py + M[0][2] * pz + M[0][3])
            oy += w * (M[1][0] * px + M[1][1] * py + M[1][2] * pz + M[1][3])
            oz += w * (M[2][0] * px + M[2][1] * py + M[2][2] * pz + M[2][3])
            if nrm is not None or tan is not None:
                # Inverse-transpose of the linear part ≈ cofactor / det, but for
                # skinning normals we use the 3x3 of M (uniform scale is fine).
                # For general M, R = inv(M3).T; use inv of the 3x3.
                # Cheap path: if M is nearly rigid*uniform, rotating by M3 is OK after normalize.
                if nrm is not None:
                    nx0, ny0, nz0 = float(Nsrc[i][0]), float(Nsrc[i][1]), float(Nsrc[i][2])
                    nx += w * (M[0][0] * nx0 + M[0][1] * ny0 + M[0][2] * nz0)
                    ny += w * (M[1][0] * nx0 + M[1][1] * ny0 + M[1][2] * nz0)
                    nz += w * (M[2][0] * nx0 + M[2][1] * ny0 + M[2][2] * nz0)
                if tan is not None:
                    tx0, ty0, tz0 = float(Tsrc[i][0]), float(Tsrc[i][1]), float(Tsrc[i][2])
                    tx += w * (M[0][0] * tx0 + M[0][1] * ty0 + M[0][2] * tz0)
                    ty += w * (M[1][0] * tx0 + M[1][1] * ty0 + M[1][2] * tz0)
                    tz += w * (M[2][0] * tx0 + M[2][1] * ty0 + M[2][2] * tz0)
        out[i] = [ox, oy, oz]
        if nrm is not None:
            L = math.sqrt(nx * nx + ny * ny + nz * nz) or 1.0
            nrm[i] = [nx / L, ny / L, nz / L]
        if tan is not None:
            L = math.sqrt(tx * tx + ty * ty + tz * tz) or 1.0
            tan[i] = [tx / L, ty / L, tz / L, tw]
    return out, nrm, tan, J, joint_remap


def _descendants(nodes, root):
    """All nodes under root, not including root itself."""
    kids = []
    stack = list(nodes[root].get("children") or [])
    while stack:
        i = int(stack.pop())
        kids.append(i)
        stack.extend(nodes[i].get("children") or [])
    return kids


def _detach(nodes, parent, i, scenes=None):
    p = parent[i] if i < len(parent) else -1
    if p >= 0:
        ch = list(nodes[p].get("children") or [])
        if i in ch:
            nodes[p]["children"] = [c for c in ch if c != i]
    if scenes:
        for sc in scenes:
            roots = list(sc.get("nodes") or [])
            if i in roots:
                sc["nodes"] = [r for r in roots if r != i]


def _flatten_armature(gltf, joint_set, parent):
    """Compose each non-joint ancestor of the first joint into that joint and reparent it up."""
    nodes = gltf["nodes"]
    skins = gltf.get("skins") or []
    if not skins or not skins[0].get("joints"):
        return
    first = skins[0]["joints"][0]
    anc = []
    i = parent[first]
    while i >= 0:
        if i not in joint_set:
            anc.append(i)
        i = parent[i]
    if not anc:
        return
    M = mat4_identity()
    for a in reversed(anc):
        M = mat4_mul(M, _node_local(nodes[a]))
    M = mat4_mul(M, _node_local(nodes[first]))
    t, r, s = _mat4_to_trs(M)
    _write_trs(nodes[first], t, r, s)
    top = anc[-1]
    new_parent = parent[top]
    _detach(nodes, parent, first, gltf.get("scenes"))
    if new_parent >= 0:
        ch = list(nodes[new_parent].get("children") or [])
        if first not in ch:
            ch.append(first)
        nodes[new_parent]["children"] = ch
    else:
        for sc in gltf.get("scenes") or []:
            roots = list(sc.get("nodes") or [])
            if first not in roots:
                roots.append(first)
            sc["nodes"] = roots
    for a in anc:
        _write_trs(nodes[a], [0, 0, 0], [0, 0, 0, 1], [1, 1, 1])


def _scale_translation_accessors(gltf, blob, node_ids, k):
    """Multiply translation keyframes of the given nodes by k (copy-on-write if shared)."""
    want = set(node_ids)
    scaled_acc = set()
    skip_acc = set()
    # Accessors also used by nodes we must not scale: copy rather than mutate.
    shared_elsewhere = set()
    for anim in gltf.get("animations") or []:
        for ch in anim.get("channels") or []:
            if (ch.get("target") or {}).get("path") != "translation":
                continue
            ni = ch["target"].get("node")
            samp = anim["samplers"][ch["sampler"]]
            acc = samp["output"]
            if ni not in want:
                shared_elsewhere.add(acc)
    for anim in gltf.get("animations") or []:
        for ch in anim.get("channels") or []:
            tgt = ch.get("target") or {}
            if tgt.get("path") != "translation":
                continue
            ni = tgt.get("node")
            if ni not in want:
                continue
            samp = anim["samplers"][ch["sampler"]]
            acc_i = samp["output"]
            if acc_i in scaled_acc:
                continue
            data = _read_accessor(gltf, blob, acc_i)
            scaled = [[float(p[0]) * k, float(p[1]) * k, float(p[2]) * k] for p in data]
            if acc_i in shared_elsewhere or acc_i in skip_acc:
                off, length = _append_f32(blob, scaled, 3)
                new_i = _add_view_acc(gltf, off, length, len(scaled), "VEC3")
                mn, mx = _aabb(scaled)
                gltf["accessors"][new_i]["min"] = mn
                gltf["accessors"][new_i]["max"] = mx
                samp["output"] = new_i
            else:
                off, length = _append_f32(blob, scaled, 3)
                # Retarget the existing accessor to the new view (keeps index stable).
                view = {"buffer": 0, "byteOffset": off, "byteLength": length}
                gltf.setdefault("bufferViews", []).append(view)
                acc = gltf["accessors"][acc_i]
                acc["bufferView"] = len(gltf["bufferViews"]) - 1
                acc["byteOffset"] = 0
                acc["componentType"] = 5126
                acc["count"] = len(scaled)
                acc["type"] = "VEC3"
                mn, mx = _aabb(scaled)
                acc["min"] = mn
                acc["max"] = mx
                scaled_acc.add(acc_i)


def _unscale_root(gltf, blob, root_i):
    """Bake a non-1 uniform scale on root_i into every descendant's local translation + keys."""
    nodes = gltf["nodes"]
    t, r, s = _node_trs_components(nodes[root_i])
    # Require a (nearly) uniform scale; do nothing if already identity.
    if all(abs(x - 1.0) < 1e-4 for x in s):
        return 1.0
    k = (abs(s[0]) + abs(s[1]) + abs(s[2])) / 3.0
    if abs(k - 1.0) < 1e-4:
        _write_trs(nodes[root_i], t, r, [1, 1, 1])
        return 1.0
    if max(abs(s[0] - k), abs(s[1] - k), abs(s[2] - k)) > 0.05 * max(abs(k), 1.0):
        # Non-uniform: still apply the average so rest pose stays roughly right.
        pass
    _write_trs(nodes[root_i], t, r, [1, 1, 1])
    kids = _descendants(nodes, root_i)
    for i in kids:
        ct, cr, cs = _node_trs_components(nodes[i])
        _write_trs(nodes[i], [ct[0] * k, ct[1] * k, ct[2] * k], cr, cs)
    _scale_translation_accessors(gltf, blob, kids, k)
    return k


def bake_glb_for_unreal(src, dst):
    """Rewrite a Lite rigged glb into one Interchange-legal bind space. Returns dst."""
    gltf, blob = load_glb(src)
    nodes = gltf["nodes"]
    worlds, parent = node_worlds(nodes)
    skins = gltf.get("skins") or []
    if not skins:
        write_glb(gltf, blob, dst)
        return dst

    # Canonical joint list: first skin, then any extras from other skins (stable order).
    joints = list(skins[0]["joints"])
    seen = set(joints)
    for skin in skins[1:]:
        for j in skin.get("joints") or []:
            if j not in seen:
                joints.append(j)
                seen.add(j)
    joint_set = set(joints)
    Jbind = [worlds[j] for j in joints]

    # Bake every skinned primitive into armature world, remapping joints into `joints`.
    merged_prims = []
    default_name = Path(src).stem.replace(" ", "_") or "Rigged"
    src_skins = skins
    for si, skin in enumerate(src_skins):
        src_joints = list(skin["joints"])
        ibm_old = _read_ibm(gltf, blob, skin["inverseBindMatrices"]) if "inverseBindMatrices" in skin else [mat4_inv(worlds[j]) for j in src_joints]
        if src_joints != joints:
            by_node = {src_joints[k]: ibm_old[k] for k in range(len(src_joints))}
            # Positions are skinned with this skin's joint list / IBM order.
            ibm_for_src = ibm_old
            Jbind_src = [worlds[j] for j in src_joints]
            remap = {old_i: joints.index(src_joints[old_i]) for old_i in range(len(src_joints))}
        else:
            ibm_for_src = ibm_old
            Jbind_src = Jbind
            remap = None
        for ni, n in enumerate(nodes):
            if n.get("skin") != si or "mesh" not in n:
                continue
            mesh = gltf["meshes"][n["mesh"]]
            for prim in mesh.get("primitives") or []:
                if "JOINTS_0" not in (prim.get("attributes") or {}) or "WEIGHTS_0" not in prim["attributes"]:
                    # Keep as a static-looking primitive still referenced by the merged mesh.
                    merged_prims.append(dict(prim))
                    continue
                P, N, T, Jrows, _ = _skin_positions(gltf, blob, prim, Jbind_src, ibm_for_src, joint_remap=None)
                new_prim = {"attributes": dict(prim["attributes"]), "mode": prim.get("mode", 4)}
                if "indices" in prim:
                    new_prim["indices"] = prim["indices"]
                if "material" in prim:
                    new_prim["material"] = prim["material"]
                if "targets" in prim:
                    new_prim["targets"] = prim["targets"]
                off, length = _append_f32(blob, P, 3)
                acc_i = _add_view_acc(gltf, off, length, len(P), "VEC3", target=34962)
                mn, mx = _aabb(P)
                gltf["accessors"][acc_i]["min"] = mn
                gltf["accessors"][acc_i]["max"] = mx
                new_prim["attributes"]["POSITION"] = acc_i
                if N is not None:
                    off, length = _append_f32(blob, N, 3)
                    new_prim["attributes"]["NORMAL"] = _add_view_acc(gltf, off, length, len(N), "VEC3", target=34962)
                if T is not None:
                    off, length = _append_f32(blob, T, 4)
                    new_prim["attributes"]["TANGENT"] = _add_view_acc(gltf, off, length, len(T), "VEC4", target=34962)
                if remap is not None:
                    mapped = []
                    for row in Jrows:
                        mapped.append([remap[int(row[k])] if k < len(row) else 0 for k in range(4)])
                    # JOINTS_0 as uint8 if possible
                    component = 5121 if max(joints) < 256 else 5123
                    cols = 4
                    off, length = _append_ints(blob, mapped, cols, component)
                    new_prim["attributes"]["JOINTS_0"] = _add_view_acc(
                        gltf, off, length, len(mapped), "VEC4", component=component, target=34962
                    )
                merged_prims.append(new_prim)

    if not merged_prims:
        write_glb(gltf, blob, dst)
        return dst

    # Flatten armature into the first joint, then bake its scale into descendant TRS + keys.
    # Do this BEFORE writing IBMs so IBMs match the joint rest that Interchange will see.
    _flatten_armature(gltf, joint_set, parent)
    parent = node_parents(nodes)
    first = joints[0]
    _unscale_root(gltf, blob, first)

    worlds, parent = node_worlds(nodes)
    Jbind = [worlds[j] for j in joints]
    new_ibm = [mat4_inv(Jw) for Jw in Jbind]
    ibm_flat = [mat4_to_colmajor(m) for m in new_ibm]
    off, length = _append_f32(blob, ibm_flat, 16)
    ibm_acc = _add_view_acc(gltf, off, length, len(joints), "MAT4")

    # Replace the mesh list so Interchange cannot also import the leftover multi-skin meshes.
    gltf["meshes"] = [{"name": default_name, "primitives": merged_prims}]
    mesh_i = 0
    gltf["skins"] = [{
        "name": nodes[first].get("name") or "Armature",
        "joints": list(joints),
        "inverseBindMatrices": ibm_acc,
        "skeleton": first,
    }]

    # Strip skin/mesh from old nodes so Interchange does not import leftover multi-skins.
    scenes = gltf.setdefault("scenes", [{"nodes": list(gltf.get("scene") and [] or [])}])
    if not scenes:
        scenes = [{"nodes": []}]
        gltf["scenes"] = scenes
    for ni, n in enumerate(list(nodes)):
        if "skin" in n or ("mesh" in n and n.get("skin") is not None):
            _detach(nodes, parent, ni, scenes)
            n.pop("skin", None)
            n.pop("mesh", None)
        elif "mesh" in n and "skin" not in n:
            # Unskinned mesh: leave it, but it must not stay parented under a leftover armature.
            pass

    mesh_node = {"name": default_name, "mesh": mesh_i, "skin": 0}
    # Identity TRS: omit the properties entirely.
    nodes.append(mesh_node)
    mesh_ni = len(nodes) - 1

    # Scene roots = identity skinned mesh + the skeleton root (first joint).
    # Drop leftover RootNode / CharacterArmature empties so they cannot sneak a -90 X back in.
    for sc in scenes:
        roots = []
        for r in sc.get("nodes") or []:
            if r == first or r == mesh_ni:
                continue
            # Keep only unskinned mesh nodes (props).
            rn = nodes[r] if r < len(nodes) else None
            if rn and "mesh" in rn and "skin" not in rn:
                roots.append(r)
        sc["nodes"] = [mesh_ni, first] + roots
    gltf["scene"] = gltf.get("scene", 0)
    if gltf["scene"] >= len(scenes):
        gltf["scene"] = 0

    # An empty "children": [] array is a glTF error (EMPTY_ENTITY), e.g. CharacterArmature
    # after Root is reparented to the scene.
    for n in nodes:
        ch = n.get("children")
        if isinstance(ch, list) and len(ch) == 0:
            n.pop("children", None)

    write_glb(gltf, blob, dst)
    problems = validate_baked_for_interchange(dst)
    if problems:
        raise RuntimeError("bake produced a file Interchange will reject: " + "; ".join(problems))
    return dst


def skinned_mesh_nodes(gltf):
    """Indices of nodes that have both a mesh and a skin."""
    return [i for i, n in enumerate(gltf.get("nodes") or []) if "mesh" in n and "skin" in n]


def scene_root_set(gltf):
    scenes = gltf.get("scenes") or []
    si = gltf.get("scene", 0) or 0
    if si >= len(scenes):
        si = 0
    return set(scenes[si].get("nodes") or []) if scenes else set()


def ibm_matrices(gltf, blob, skin):
    if "inverseBindMatrices" not in skin:
        return None
    return _read_ibm(gltf, blob, skin["inverseBindMatrices"])


def validate_baked_for_interchange(path, src=None, require_upright=False):
    """Simulate Interchange rules. Return a list of problem strings (empty = OK).

    require_upright: also demand head/feet landmarks with an upright joint rest
    (glTF Y-up and UE Z-up). Off by default because many Lite rigs (cows, folk,
    goblins) have no foot bones by name; the bake still has to be Interchange-legal.
    """
    gltf, blob = load_glb(path)
    problems = []
    skins = gltf.get("skins") or []
    if not skins:
        return problems
    if len(skins) != 1:
        problems.append("expected one skin, found %d" % len(skins))
    nodes = gltf["nodes"]
    worlds, parent = node_worlds(nodes)
    roots = scene_root_set(gltf)
    for i in skinned_mesh_nodes(gltf):
        if i not in roots or parent[i] >= 0:
            problems.append("skinned mesh node %s is not a scene root (parent=%s)" % (
                nodes[i].get("name") or i, parent[i]))
        if not is_identity_node(nodes[i]):
            t, r, s = _node_trs_components(nodes[i])
            problems.append("skinned mesh node %s is not identity TRS (T=%s R=%s S=%s)" % (
                nodes[i].get("name") or i, t, r, s))
    # One shared IBM per joint, consistent with inv(joint world).
    per_joint = {}
    for si, skin in enumerate(skins):
        ibms = ibm_matrices(gltf, blob, skin)
        if ibms is None:
            problems.append("skin %d has no inverseBindMatrices" % si)
            continue
        sj = skin.get("joints") or []
        if len(ibms) != len(sj):
            problems.append("skin %d IBM count %d != joints %d" % (si, len(ibms), len(sj)))
        for k, j in enumerate(sj):
            if k >= len(ibms):
                break
            ibm = ibms[k]
            if j in per_joint and not mat4_close(per_joint[j], ibm, 1e-4):
                problems.append("joint %s has multiple different InverseBindMatrices" % (
                    nodes[j].get("name") if j < len(nodes) else j))
            per_joint[j] = ibm
            try:
                expected = mat4_inv(worlds[j])
            except Exception:
                problems.append("joint %s world is singular" % (nodes[j].get("name") if j < len(nodes) else j))
                continue
            if not mat4_close(ibm, expected, 1e-3):
                problems.append("joint %s IBM is not inv(world bind)" % (
                    nodes[j].get("name") if j < len(nodes) else j))
    pos = joint_world_positions(gltf)
    lm = landmarks(pos)
    if require_upright:
        if lm is None:
            problems.append("could not find head/feet landmarks in joint rest pose")
        else:
            if not (lm["head"][1] > lm["feet"][1] + 0.5):
                problems.append("joint rest pose is not upright in glTF Y-up (head Y=%.3f, feet Y=%.3f)" % (
                    lm["head"][1], lm["feet"][1]))
            ue_head, ue_feet = gltf_to_ue(lm["head"]), gltf_to_ue(lm["feet"])
            if not (ue_head[2] > ue_feet[2] + 0.5):
                problems.append("joint rest is not upright after glTF->UE (head Z=%.3f, feet Z=%.3f)" % (
                    ue_head[2], ue_feet[2]))
    anims = gltf.get("animations") or []
    if not anims:
        problems.append("no animations on the baked file")
    return problems


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
        out[n.get("name") or ("node_%d" % i)] = mat4_translation(worlds[i])
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
        if ("shoulder" in n) or ("upperarm" in n) or ("arm" in n and "fore" not in n and "lower" not in n):
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
            attrs = prim.get("attributes") or {}
            if "POSITION" not in attrs:
                continue
            P = _read_accessor(gltf, blob, attrs["POSITION"])
            if not P:
                continue
            mn, mx = _aabb(P)
            mins.append(mn)
            maxs.append(mx)
    if not mins:
        return None
    mn = [min(c[j] for c in mins) for j in range(3)]
    mx = [max(c[j] for c in maxs) for j in range(3)]
    return mn, mx
