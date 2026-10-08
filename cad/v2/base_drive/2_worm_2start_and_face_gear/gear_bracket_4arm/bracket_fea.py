# Gear support bracket flex: old V (2 arms) vs new X (4 arms), linear static, voxel hex8.
# Run:  uv run --with scikit-fem --with pyamg --with scipy --with numpy --with shapely \
#         python bracket_fea.py <slices.json> <dir with fea_common.py>
# slices.json: per-layer section polygons of both brackets in the Base.step assembly frame
# (made by FreeCAD from the two STEPs; z levels are in the file).
import sys, json, math, time, numpy as np, shapely
sys.path.insert(0, sys.argv[2]); from fea_common import *
S = json.load(open(sys.argv[1])); Z = np.array(S["z"]); h = 0.5
C0 = np.array([0.0, 50.0])                                   # gear / base axis
CORNERS = {"A": (33.5, 13.82), "B": (-36.18, 16.5), "C": (36.18, 83.5), "D": (-33.5, 86.18)}
FRONT, FAR = ("A", "B"), ("C", "D")
CM = C_transiso(axis=2)                                      # printed flat: layer normal = z
NUT_R = float(__import__("os").environ.get("NUT_R", "4.5"))                                                 # clamped footprint around each rod
POCKET_R = 7.85                                              # Ø15.7 bearing seat, z -12.5 .. -7.5
# Far rods (new bracket only): M5, 21.3 mm free between the nut under the case ear and the bracket,
# both ends clamped -> fixed-guided beam, 12EI/L^3 per in-plane direction. Axially a rod in tension: rigid.
E_STEEL, D_MINOR, L_ROD = 200000.0, 4.2, 21.3
K_ROD = 12 * E_STEEL * (math.pi * D_MINOR**4 / 64) / L_ROD**3

def polys(layer):
    g = shapely.Polygon()
    for w in layer:
        if len(w) >= 3: g = g.symmetric_difference(shapely.Polygon(w).buffer(0))
    return g

def build(name):
    L = S[name]; nz = len(Z) - 1
    xs = np.arange(-60, 60 + 1e-9, h); ys = np.arange(-10, 100 + 1e-9, h)
    XX, YY = np.meshgrid(xs + h / 2, ys + h / 2, indexing="ij")
    occ = np.zeros((len(xs), len(ys), nz), bool)
    for k in range(nz): occ[:, :, k] = shapely.contains_xy(polys(L[k]), XX, YY)
    idx = np.argwhere(occ); ny, nzz = len(ys) + 1, nz + 1
    nid = lambda i, j, k: (i * ny + j) * nzz + k
    i, j, k = idx.T
    corners = [(0,0,0),(1,0,0),(1,1,0),(0,1,0),(0,0,1),(1,0,1),(1,1,1),(0,1,1)]
    T = np.array([nid(i + a, j + b, k + c) for a, b, c in corners])
    used, inv = np.unique(T, return_inverse=True); T = inv.reshape(T.shape)
    I_ = used // (ny * nzz); J_ = (used // nzz) % ny; K_ = used % nzz
    P = np.array([xs[0] + I_ * h, ys[0] + J_ * h, Z[K_]])
    T = T[[0, 1, 3, 4, 2, 5, 7, 6]]                         # skfem hex node order
    return MeshHex(P, T)

def run(name, far_springs):
    t0 = time.time()
    mesh = build(name); e = ElementVector(ElementHex1())
    K = assemble_chunked(mesh, e, make_form(CM), chunk=20000, intorder=2).tolil()
    X = mesh.p; n = X.shape[1]
    vol = mesh.t.shape[1] * 0  # (voxel volume not needed)
    top = np.isclose(X[2], Z[-1]);
    # clamp: nodes on any face of the boss within the nut footprint, top and bottom of the arm/boss
    fixed = []; springs = []
    arms = FRONT if name == "old" else FRONT + FAR
    zfaces = np.isclose(X[2], -6.3) | np.isclose(X[2], -12.5 if name == "old" else -11.3)
    for c in arms:
        cx, cy = CORNERS[c]
        nn = np.where(zfaces & (np.hypot(X[0] - cx, X[1] - cy) <= NUT_R))[0]
        if far_springs and c in FAR:
            springs.append((c, nn))
            fixed += list(3 * nn + 2)                        # axially held by the nut (rod in tension)
        else:
            fixed += list(3 * nn) + list(3 * nn + 1) + list(3 * nn + 2)
    for c, nn in springs:                                    # rod bending shared by the clamped nodes
        for d in (0, 1):
            for q in nn: K[3 * q + d, 3 * q + d] += K_ROD / len(nn)
    K = K.tocsr(); Dd = np.array(sorted(set(fixed)))
    # bearing-seat nodes: pocket wall
    r = np.hypot(X[0] - C0[0], X[1] - C0[1])
    seat = np.where((r >= POCKET_R - 0.05) & (r <= POCKET_R + 0.75) & (X[2] <= -7.5 + 1e-6))[0]
    print("%s%s: %d el, %d dof, %d seat nodes, rod k %.0f N/mm (%.0fs)" % (name, " +far-rod springs" if far_springs else "",
          mesh.t.shape[1], 3 * n, len(seat), K_ROD, time.time() - t0), flush=True)
    res = {}
    for lab, F in loadcases():
        f = np.zeros(3 * n)
        fr = np.array([F[0], F[1]]); frm = np.linalg.norm(fr)
        if frm > 0:                                          # radial: cosine-weighted on the half the bearing pushes into
            dirn = fr / frm
            c = ((X[0, seat] - C0[0]) * dirn[0] + (X[1, seat] - C0[1]) * dirn[1]) / r[seat]
            w = np.clip(c, 0, None); w = w / w.sum()
            f[3 * seat] += F[0] * w; f[3 * seat + 1] += F[1] * w
        if F[2] != 0:                                        # axial: shear into the pocket wall (glued / press fit)
            f[3 * seat + 2] += F[2] / len(seat)
        u, it, rr = solve(K, f, Dd, X)
        Us = np.c_[u[0::3][seat], u[1::3][seat], u[2::3][seat]].mean(0)
        s = stresses(Basis(mesh, e, intorder=1), u, CM)[:, :, 0]
        vm = np.sqrt(0.5 * ((s[0] - s[1])**2 + (s[1] - s[2])**2 + (s[2] - s[0])**2) + 3 * (s[3]**2 + s[4]**2 + s[5]**2))
        cen = mesh.p[:, mesh.t].mean(axis=1)
        nearbc = np.zeros(cen.shape[1], bool)
        for cc in arms: nearbc |= np.hypot(cen[0] - CORNERS[cc][0], cen[1] - CORNERS[cc][1]) < NUT_R + 1.0
        res[lab] = (Us, np.abs(u).reshape(-1, 3).max(0) if False else np.sqrt(u[0::3]**2 + u[1::3]**2 + u[2::3]**2).max(),
                    np.percentile(vm[~nearbc], 99.9), vm[~nearbc].max())
        print("   %-26s seat moves (%+.4f, %+.4f, %+.4f) mm | in-plane %.4f | axial %.4f | max|u| %.4f | von Mises p99.9 %.1f max %.1f MPa  [CG %d it]"
              % (lab, Us[0], Us[1], Us[2], math.hypot(Us[0], Us[1]), abs(Us[2]), res[lab][1], res[lab][2], res[lab][3], it), flush=True)
    return res

def loadcases():
    # Case B (arm straight out, cycloid 10:1): gear tangential Ft2 = 190 N (along the worm axis, +-y),
    # separating Fr = Ft2 tan(25 deg)/cos(8.86 deg) (pushes the gear away from the worm, +x),
    # axial Fa = 0.35 Ft2 (pushes the gear onto the bracket bearing, -z, one rotation direction).
    # The bracket bearing takes 11/25 of the radial load (case bearing z 15, mesh z 4, this bearing z -10).
    Ft2 = 190.0; Fr = Ft2 * math.tan(math.radians(25)) / math.cos(math.radians(8.86)); Fa = 0.35 * Ft2; sh = 11 / 25
    return [("B radial, +y", (sh * Fr, sh * Ft2, 0.0)),
            ("B radial, -y", (sh * Fr, -sh * Ft2, 0.0)),
            ("B axial only (-z)", (0.0, 0.0, -Fa)),
            ("B combined +y & axial", (sh * Fr, sh * Ft2, -Fa)),
            ("B combined -y & axial", (sh * Fr, -sh * Ft2, -Fa))]

R = {}
RUNS = __import__("os").environ.get("RUNS", "old,new_rigid,new_rods").split(",")
if "old" in RUNS: R["old"] = run("old", False)
if "new_rigid" in RUNS: R["new_rigid"] = run("new", False)
if "new_rods" in RUNS: R["new_rods"] = run("new", True)
print("\nScale linearly: case E (437 N) = x2.30, case F jam (961 N) = x5.06.")
json.dump({k: {l: [list(map(float, v[0])), float(v[1]), float(v[2]), float(v[3])] for l, v in r.items()} for k, r in R.items()},
          open(sys.argv[1].replace("slices.json", "fea_results.json"), "w"), indent=1)
