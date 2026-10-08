# One-piece double eccentric for the base cycloid: replaces CAM1 + CAM 2 of Base.step.
# Assembly frame of Base.step, cycloid axis at (0, 50). Run: freecadcmd -c "exec(open('make_double_cam.py').read())"
import FreeCAD as App, Part, Mesh, MeshPart, math, os
from FreeCAD import Vector as V
OUT = os.environ.get("CAM_OUT", ".") + "/"
AX = V(0, 50, 0)
E = 1.2            # eccentricity (CAD: CAM1 centre +1.20, CAM 2 centre -1.19 in x)
R_LOBE = 5.0       # bearing seat, same as the CAD cams (Ø10)
R_BORE = 2.575     # Ø5.15: prints ~Ø5.0, a snug slide fit on the Ø5 rod with no reaming
Z1, Z2 = 20.0, 25.0     # cam 1 (cycloid 1)
Z3, Z4 = 26.2, 31.2     # cam 2 (cycloid 2)
CH = 0.3           # lead-in chamfer on the two outer seat edges
W_IN, W_OUT, SLOT_H = 1.0, 1.6, 3.2   # tapered nib window from +y: 1.0 wide at the rod, 1.6 at the cam surface; 3.2 tall, centred on the old pin heights
lobe1 = Part.makeCylinder(R_LOBE, Z2 - Z1, AX + V(E, 0, Z1))
lobe2 = Part.makeCylinder(R_LOBE, Z4 - Z3, AX + V(-E, 0, Z3))
lens = Part.makeCylinder(R_LOBE, Z3 - Z2, AX + V(E, 0, Z2)).common(Part.makeCylinder(R_LOBE, Z3 - Z2, AX + V(-E, 0, Z2)))
cam = lobe1.fuse([lobe2, lens]).removeSplitter()
# chamfers on the bottom edge of cam 1 and the top edge of cam 2 (bearing lead-in)
for (cx, z, up) in ((E, Z1, True), (-E, Z4, False)):
    ring = Part.makeCone(R_LOBE - CH, R_LOBE, CH, AX + V(cx, 0, z), V(0, 0, 1)) if up else Part.makeCone(R_LOBE, R_LOBE - CH, CH, AX + V(cx, 0, z - CH), V(0, 0, 1))
    shell = Part.makeCylinder(R_LOBE + 1, CH, AX + V(cx, 0, z if up else z - CH)).cut(ring)
    cam = cam.cut(shell)
cam = cam.cut(Part.makeCylinder(R_BORE, Z4 - Z1 + 2, AX + V(0, 0, Z1 - 1)))
for zc in ((Z1 + Z2) / 2, (Z3 + Z4) / 2):
    # wedge-shaped window (one side only, +y): the tapered pen nib wedges in it when the bearing's inner race
    # rolls over the cam and pushes it toward the rod, so it has no play and can't back out
    k = (W_OUT - W_IN) / 2 / (4.85 - R_BORE)            # half-width growth per mm of radius
    r0, r1 = 1.5, 7.0
    pts = [V(-(W_IN / 2 + k * (r0 - R_BORE)), r0, 0), V(W_IN / 2 + k * (r0 - R_BORE), r0, 0),
           V(W_IN / 2 + k * (r1 - R_BORE), r1, 0), V(-(W_IN / 2 + k * (r1 - R_BORE)), r1, 0)]
    pts = [p + AX + V(0, 0, zc - SLOT_H / 2) for p in pts]
    wedge = Part.Face(Part.makePolygon(pts + [pts[0]])).extrude(V(0, 0, SLOT_H))
    cam = cam.cut(wedge)
cam = cam.removeSplitter()
assert cam.isValid() and len(cam.Solids) == 1, "bad solid"
cam.exportStep(OUT + "Double_cam.step")
pr = cam.copy(); pr.translate(V(0, -50, -Z1))          # print orientation: cam 1 face on the bed
def stl(shape, path):
    m = MeshPart.meshFromShape(Shape=shape, LinearDeflection=0.005, AngularDeflection=0.05, Relative=False); m.write(path); return m
m = stl(pr, OUT + "Double_cam_print.stl")
print("volume %.1f mm3, solids %d, valid %s, mesh solid %s, non-manifold %s" % (cam.Volume, len(cam.Solids), cam.isValid(), m.isSolid(), m.hasNonManifolds()))
