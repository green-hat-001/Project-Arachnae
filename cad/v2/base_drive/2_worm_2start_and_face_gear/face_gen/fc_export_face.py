# freecadcmd: export worm sections (real + scaled cutter) and the stub gear's bore shapes
import sys, os, json, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)) if "__file__" in dir() else os.environ["FACE_GEN"])
from params import *
import Part, FreeCAD as App
V = App.Vector
os.makedirs(S, exist_ok=True)
t0 = time.time()
w = Part.read(WORM_FILE).common(Part.makeBox(40, 40, 40, V(ZONE[0] - 20, WY - 20, WZ - 20)))
g = Part.read(CAD + "base_worm_2start/Gear_10T_2start_stub.step")
def sec(shape, z):
    out = []
    for wi in shape.slice(V(0, 0, 1), z):
        pts = []
        for e in wi.OrderedEdges:
            d = e.discretize(Deflection=0.002)
            if pts and (pts[-1] - d[0]).Length > (pts[-1] - d[-1]).Length: d = d[::-1]
            pts += d if not pts else d[1:]
        out.append([(round(p.x, 5), round(p.y, 5)) for p in pts])
    return out
zs = [round(-2.0 + i * DZ, 4) for i in range(int(3.6 / DZ) + 1)]      # -2.0 .. 1.6
W = {}; Wp = {}
for z in zs:
    if abs(z - WZ) < WORM_TIP: W[z] = sec(w, z)
    zp = WZ + (z - WZ) / SCALE
    if abs(zp - WZ) < WORM_TIP: Wp[z] = sec(w, zp)
G = {str(z): sec(g, z) for z in (-2.0, 6.0)}     # pin hole, D-bore
json.dump({"W": {str(k): v for k, v in W.items()}, "Wp": {str(k): v for k, v in Wp.items()}, "G": G, "s": SCALE},
          open(S + "face_sections.json", "w"))
print("exported W %d Wp %d in %.0fs" % (len(W), len(Wp), time.time() - t0))
