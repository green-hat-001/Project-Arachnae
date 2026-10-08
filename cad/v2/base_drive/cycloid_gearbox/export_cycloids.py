# Export the two cycloids (Cycloidal 1 / Cycloidal 2 of Base.step) as STEP (assembly frame) + print STL (flat on the bed).
# Run from this folder: freecadcmd -c "exec(open('export_cycloids.py').read())"
import FreeCAD as App, Import, Part, MeshPart
from FreeCAD import Vector as V
doc = App.newDocument("b"); Import.insert("../Base.step", "b")
found = {}
for o in doc.Objects:
    if o.TypeId != "Part::Feature" or not o.Shape.Solids: continue
    s = o.Shape.copy(); s.Placement = o.getGlobalPlacement(); s = Part.Shape(s); b = s.BoundBox
    if 18000 < s.Volume < 20000 and b.ZLength < 5.1 and abs(b.ZMin - 20.0) < 0.1 and "c1" not in found: found["c1"] = s
    if 18000 < s.Volume < 20000 and b.ZLength < 5.1 and abs(b.ZMin - 26.2) < 0.1 and "c2" not in found: found["c2"] = s
for k, name in (("c1", "Cycloid_1"), ("c2", "Cycloid_2")):
    s = found[k]
    s.exportStep(name + ".step")
    p = s.copy(); p.translate(V(0, -50, -s.BoundBox.ZMin))
    m = MeshPart.meshFromShape(Shape=p, LinearDeflection=0.01, AngularDeflection=0.08, Relative=False); m.write(name + "_print.stl")
    print("%s: valid %s, solids %d, volume %.0f mm3, z %.1f..%.1f, eccentric centre x %.2f, mesh solid %s, non-manifold %s" % (
        name, s.isValid(), len(s.Solids), s.Volume, s.BoundBox.ZMin, s.BoundBox.ZMax, s.BoundBox.Center.x, m.isSolid(), m.hasNonManifolds()))
