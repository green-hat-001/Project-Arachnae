# Linear-static FEA of a face-gear sector under one worst-phase contact patch (per 100 N tangential).
# Voxel hex8 from the hob slices; printed PETG, transversely isotropic, layer normal = gear axis (printed flat).
# run: uv run --with scikit-fem --with pyamg --with scipy --with numpy --with shapely python fea_face.py
import sys, os, time, pickle, math, numpy as np, shapely
here = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, here + "/../throated_gen")
from fea_common import *
from common_face import *
exec(open(here + "/verify_face.py").read().split("t0 = time.time()")[0])     # overlap()/edge()
t0 = time.time(); h = 0.15
pen = math.degrees(float(os.environ.get("FACE_PEN", 0.03)) / RP); best = None
for phi in [i * TAU / 6 for i in range(6)]:
    for sgn in (1, -1):
        e = edge(phi, sgn) + sgn * pen
        v, _, per = overlap(phi, e, detail=True)
        if best is None or v < best[0]: best = (v, phi, sgn, e, per)
v, phi, sgn, e, per = best
patch = [(z, o) for z, o in per if o.area > 1e-8]
cx = np.average([o.centroid.x for _, o in patch], weights=[o.area for _, o in patch])
cy = np.average([o.centroid.y for _, o in patch], weights=[o.area for _, o in patch])
a0 = math.atan2(cy - GY, cx - GX)
ntooth = len(set(round((math.degrees(math.atan2(p.centroid.y - GY, p.centroid.x - GX))) / TAU) for _, o in patch for p in getattr(o, "geoms", [o]) if p.area > 1e-7))
print("worst phase phi %.2f dir %+d: overlap %.4f mm3, %d slices, %d teeth, patch centre r %.2f" % (phi, sgn, v, len(patch), ntooth, math.hypot(cx - GX, cy - GY)), flush=True)
R0, R1, HALF = 6.0, 13.2, 38.0
sector = shapely.Polygon([(GX + R0 * math.cos(a0 + math.radians(t)), GY + R0 * math.sin(a0 + math.radians(t))) for t in np.linspace(HALF, -HALF, 31)] +
                         [(GX + R1 * math.cos(a0 + math.radians(t)), GY + R1 * math.sin(a0 + math.radians(t))) for t in np.linspace(-HALF, HALF, 31)])
zs = np.arange(Z_HUB_BOT + h / 2, Z_TOP + 0.3, h)
xs = np.arange(GX - R1, GX + R1, h); ys = np.arange(GY - R1, GY + R1, h)
XX, YY = np.meshgrid(xs + h / 2, ys + h / 2, indexing="ij")
occ = np.zeros((len(xs), len(ys), len(zs)), bool)
for k, z in enumerate(zs):
    g = G[min(gz, key=lambda q: abs(q - z))].intersection(sector)
    occ[:, :, k] = shapely.contains_xy(g, XX, YY)
idx = np.argwhere(occ); nx, ny, nz = len(xs) + 1, len(ys) + 1, len(zs) + 1
nid = lambda i, j, k: (i * ny + j) * nz + k
i, j, k = idx.T
corners = [(0, 0, 0), (1, 0, 0), (1, 1, 0), (0, 1, 0), (0, 0, 1), (1, 0, 1), (1, 1, 1), (0, 1, 1)]
T = np.array([nid(i + a, j + b, k + c) for a, b, c in corners])
used, inv = np.unique(T, return_inverse=True); T = inv.reshape(T.shape)
I_ = used // (ny * nz); J_ = (used // nz) % ny; K_ = used % nz
Pn = np.array([xs[0] + I_ * h, ys[0] + J_ * h, Z_HUB_BOT + K_ * h])
T = T[[0, 1, 3, 4, 2, 5, 7, 6]]
mesh = MeshHex(Pn, T); el = ElementVector(ElementHex1())
C = C_transiso(axis=2)
K = assemble_chunked(mesh, el, make_form(C), chunk=10000, intorder=2)
print("%d voxels, %d dofs (%.0fs)" % (T.shape[1], K.shape[0], time.time() - t0), flush=True)
X = mesh.p; r = np.hypot(X[0] - GX, X[1] - GY)
ang = (np.degrees(np.arctan2(X[1] - GY, X[0] - GX)) - math.degrees(a0) + 180) % 360 - 180
fixed = np.where((r < R0 + h) | (np.abs(ang) > HALF - 1.0))[0]          # web continues inward + sector cuts
Dd = np.concatenate([3 * fixed, 3 * fixed + 1, 3 * fixed + 2])
# load on the gear: tangential, resisting the probe rotation, plus the separating force pushing the gear down
t_dir = np.array([-math.sin(a0), math.cos(a0), 0.0]) * (-sgn)
Fg = 100.0 * t_dir + np.array([0, 0, -100.0 * math.tan(math.radians(25))])
f = np.zeros(3 * X.shape[1]); Atot = sum(o.area for _, o in patch)
bnd = np.unique(mesh.facets[:, mesh.boundary_facets()])
for z, o in patch:
    nn = bnd[(np.abs(X[2, bnd] - z) <= h) & shapely.contains_xy(o.buffer(0.2), X[0, bnd], X[1, bnd])]
    if len(nn) == 0: continue
    for d in range(3): np.add.at(f, 3 * nn + d, Fg[d] * o.area / Atot / len(nn))
Fapp = np.array([f[0::3].sum(), f[1::3].sum(), f[2::3].sum()])
u, it, rr = solve(K, f, Dd, X)
s = stresses(Basis(mesh, el, intorder=1), u, C)[:, :, 0]
cen = mesh.p[:, mesh.t].mean(axis=1); rc = np.hypot(cen[0] - GX, cen[1] - GY)
ac = (np.degrees(np.arctan2(cen[1] - GY, cen[0] - GX)) - math.degrees(a0) + 180) % 360 - 180
zone = (rc > R0 + 0.6) & (np.abs(ac) < HALF - 4)
szz = s[2][zone]; tau = np.sqrt(s[3] ** 2 + s[4] ** 2)[zone]
sxx, syy, sxy = s[0][zone], s[1][zone], s[5][zone]
p1 = (sxx + syy) / 2 + np.sqrt(((sxx - syy) / 2) ** 2 + sxy ** 2)
from scipy.spatial import cKDTree
nodl = np.where(np.abs(f[0::3]) + np.abs(f[1::3]) + np.abs(f[2::3]) > 0)[0]
far = cKDTree(X[:, nodl].T).query(cen.T[zone])[0] > 0.45
print("applied F %s N; CG %d it rel %.1e; max |u| %.4f mm" % (np.round(Fapp, 1), it, rr, np.abs(u).max()))
for lab, m in (("all", np.ones_like(far)), ("bulk >0.45 mm from contact", far)):
    print("%-28s in-layer %.1f | across-layer tension %.1f | interlayer shear %.1f  MPa per 100 N (p99.9)" % (
        lab, np.percentile(p1[m], 99.9), np.percentile(szz[m], 99.9), np.percentile(tau[m], 99.9)))
um = (np.c_[u[0::3][nodl], u[1::3][nodl], u[2::3][nodl]]).mean(0)
print("mean loaded-face displacement per 100 N: %s mm (tangential %.4f, axial %.4f)" % (np.round(um, 4), um @ t_dir, um[2]))
print("done %.0fs" % (time.time() - t0))
