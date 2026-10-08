import FreeCAD as App, Part, math, json
from FreeCAD import Vector as V
import os
D = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..") + "/"  # cad/v2/base_drive/
new = Part.read(D + "2_worm_2start_and_face_gear/gear_bracket_4arm/Gear_support_bracket_4arm.step")
old = Part.read(D + "1_worm_4start/Gear_support_bracket.step")
m = App.Matrix(); m.rotateY(math.pi); old = old.transformGeometry(m)
m = App.Matrix(); m.rotateZ(math.radians(-2.2)); old = old.transformGeometry(m)
old.translate(V(0, 50, -6.3 - old.BoundBox.ZMax))
def levels():
    zs = [-12.5]
    for a, b, n in [(-12.5, -11.3, 3), (-11.3, -7.5, 9), (-7.5, -6.3, 3)]:
        zs += [a + (b - a) * (i + 1) / n for i in range(n)]
    return zs
Z = levels(); out = {"z": Z}
for nm, s in [("old", old), ("new", new)]:
    L = []
    for k in range(len(Z) - 1):
        zm = 0.5 * (Z[k] + Z[k + 1])
        L.append([[(p.x, p.y) for p in w.discretize(Deflection=0.02)] for w in s.slice(V(0, 0, 1), zm)])
    out[nm] = L
    print(nm, "vol", round(s.Volume), "bb", s.BoundBox)
json.dump(out, open("/tmp/bracket_fea/slices.json", "w"))
