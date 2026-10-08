# union of the extruded slices -> one manifold mesh (manifold3d), written as binary STL
# argv: output path, optional "print" = shift so the bottom face sits on z = 0
import sys, pickle, numpy as np, manifold3d as mf, trimesh
from common_face import *
from shapely.geometry.polygon import orient
OUT = sys.argv[1]; PRINT = len(sys.argv) > 2 and sys.argv[2] == "print"
sl = pickle.load(open(S + "face_slices_N%d.pkl" % N, "rb"))["slices"]
zs = sorted(sl)
bnd = [Z_HUB_BOT] + [(zs[i] + zs[i + 1]) / 2 for i in range(len(zs) - 1)] + [Z_GEAR_TOP]
parts = []
for i, z in enumerate(zs):
    g = make_valid(sl[z].simplify(0.006, preserve_topology=True)).buffer(0)
    loops = []
    for p in getattr(g, "geoms", [g]):
        if p.geom_type != "Polygon" or p.area < 1e-9: continue
        p = orient(p, 1.0)
        loops += [np.array(p.exterior.coords[:-1])] + [np.array(h.coords[:-1]) for h in p.interiors]
    cs = mf.CrossSection(loops, mf.FillRule.EvenOdd)
    parts.append(cs.extrude(bnd[i + 1] - bnd[i]).translate((0, 0, bnd[i])))
m = mf.Manifold.batch_boolean(parts, mf.OpType.Add)
mesh = m.to_mesh(); V = np.array(mesh.vert_properties)[:, :3]; F = np.array(mesh.tri_verts)
tm = trimesh.Trimesh(V, F, process=True)
if PRINT: tm.apply_translation((0, 0, -Z_HUB_BOT))
tm.export(OUT)
print("%s: %d tris, volume %.1f mm3, watertight %s, winding %s, bodies %d, genus-ish euler %d" % (
    OUT, len(tm.faces), tm.volume, tm.is_watertight, tm.is_winding_consistent, m.num_prop() if False else len(tm.split(only_watertight=True)), tm.euler_number))
