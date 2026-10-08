# Pencil tool module + twist-lock (bayonet) tool interface for the Arachnae forearm.
# Run: freecadcmd pencil_gen.py   (writes STEP + STL next to this file)
#
# Local frame: +Z along the arm toward the tool, origin on the interface plane
# (top face of the receiver). In Main.stp the axis sits at CAD (x -26.5, y 55.5)
# and the plane at z 1022 (forearm end z 1014 + 8 mm of bayonet cavity).
import math, os, FreeCAD as App, Part, MeshPart, Mesh
V = App.Vector
OUT = os.path.dirname(os.path.abspath(__file__)) if '__file__' in dir() else os.getcwd()

# ---- interface dimensions (mm) ----
FORE_W, FORE_H = 61.0, 59.0      # forearm end section (Main.stp: x -57..4, y 26..85)
CLR = 0.7                        # printed slip fit per side
SPIGOT_D, LUG_D = 45.5, 51.0     # male spigot and lug tip diameters
LIP_D, CAV_D = 46.5, 52.5        # receiver lip opening and lug cavity
LUG_W, NLUG = 11.0, 3            # lug width (mm along circumference) and count
TWIST = 40.0                     # degrees from entry to lock

def rrect(w, h, r, z0, dz):
    b = Part.makeBox(w, h, dz, V(-w / 2, -h / 2, z0))
    return b.makeFillet(r, [e for e in b.Edges if abs(e.Vertexes[0].Point.z - e.Vertexes[1].Point.z) > 1e-6])

def ring_sectors(r_in, r_out, z0, dz, width, n, phase=0.0):
    """n radial blocks of the given width between two radii (lugs / notches)."""
    out = None
    for i in range(n):
        b = Part.makeBox(r_out - r_in + 2, width, dz, V(r_in - 1, -width / 2, z0))
        b.rotate(V(0, 0, 0), V(0, 0, 1), phase + i * 360.0 / n)
        out = b if out is None else out.fuse(b)
    return out

# ---- receiver: clamps over the forearm end, takes any tool ----
def receiver():
    body = rrect(FORE_W + 2 * CLR + 7.6, FORE_H + 2 * CLR + 7.6, 8.0, -24.0, 24.0)
    body = body.cut(Part.makeBox(FORE_W + 2 * CLR, FORE_H + 2 * CLR, 16.01, V(-(FORE_W + 2 * CLR) / 2, -(FORE_H + 2 * CLR) / 2, -24.01)))
    body = body.cut(Part.makeCylinder(CAV_D / 2, 5.0, V(0, 0, -8.0)))            # lug cavity
    body = body.cut(Part.makeCylinder(LIP_D / 2, 3.2, V(0, 0, -3.1)))            # lip opening
    body = body.cut(ring_sectors(LIP_D / 2 - 1, CAV_D / 2, -3.1, 3.2, LUG_W + 1.0, NLUG))   # entry notches
    for ang in (0, 90, 180, 270):                                               # M3 clamp screws
        h = Part.makeCylinder(1.7, 20, V(0, 0, -16), V(1, 0, 0)); h.translate(V(-10, 0, 0)); h.rotate(V(0, 0, 0), V(0, 0, 1), ang)
        body = body.cut(h)
    # twist arrow / index mark recessed 0.6 mm into the top
    tri = Part.Face(Part.makePolygon([V(30.5, -3, 0), V(30.5, 3, 0), V(34.5, 0, 0), V(30.5, -3, 0)]))
    body = body.cut(tri.extrude(V(0, 0, -0.6)))
    return body

# ---- male half shared by every tool (sits inside the receiver) ----
def spigot():
    s = Part.makeCylinder(SPIGOT_D / 2, 8.0, V(0, 0, -8.0))
    lugs = ring_sectors(SPIGOT_D / 2 - 1, LUG_D / 2, -8.0, 4.0, LUG_W, NLUG, phase=TWIST)   # locked position
    return s.fuse(lugs)

# ---- pencil module ----
PEN_BORE = 8.2   # standard hex pencil, 7 mm across flats (7.6 over corners)
def pencil_module():
    m = spigot()
    m = m.fuse(Part.makeCone(SPIGOT_D / 2 + 0.5, 32.0, 9.0, V(0, 0, 0)))       # 45 deg chamfer: prints without support
    grip = Part.makeCylinder(32.0, 5.0, V(0, 0, 9.0))
    for i in range(30):                                                         # knurled twist grip
        c = Part.makeCylinder(1.6, 5.2, V(32.6, 0, 8.9)); c.rotate(V(0, 0, 0), V(0, 0, 1), i * 12.0)
        grip = grip.cut(c)
    m = m.fuse(grip)
    m = m.fuse(Part.makeCone(24.0, 8.5, 62.0, V(0, 0, 14.0)))                    # body
    m = m.fuse(Part.makeCylinder(8.5, 14.0, V(0, 0, 76.0)))                     # collet nose
    m = m.cut(Part.makeCylinder(PEN_BORE / 2, 120.0, V(0, 0, -10.0)))
    for a in (0, 90):                                                           # 4 collet slots
        b = Part.makeBox(1.6, 20.0, 9.0, V(-0.8, -10.0, 81.01)); b.rotate(V(0, 0, 0), V(0, 0, 1), a)
        m = m.cut(b)
    # side window shows the pencil seated (and saves plastic)
    w = Part.makeBox(60, 7.0, 26.0, V(0, -3.5, 36.0))
    m = m.cut(w)
    return m

def collet_ring():   # slides over the slotted nose; tapered bore squeezes the collet
    r = Part.makeCylinder(11.5, 7.0, V(0, 0, 83.0))
    r = r.cut(Part.makeCone(8.75, 8.35, 7.02, V(0, 0, 82.99)))
    for i in range(18):
        c = Part.makeCylinder(1.0, 7.2, V(12.0, 0, 82.9)); c.rotate(V(0, 0, 0), V(0, 0, 1), i * 20.0)
        r = r.cut(c)
    return r

def pencil(stick_out=68.0, inside=70.0):
    af = 7.0; rc = af / math.sqrt(3)
    pts = [V(rc * math.cos(math.radians(30 + 60 * i)), rc * math.sin(math.radians(30 + 60 * i)), 0) for i in range(6)]
    hexf = Part.Face(Part.makePolygon(pts + [pts[0]]))
    z0 = 90.0 - inside + 6.0
    L = inside + stick_out
    tip = 18.0
    body = hexf.extrude(V(0, 0, L - tip)); body.translate(V(0, 0, z0))
    cone = Part.makeCone(rc + 0.3, 0.0, tip, V(0, 0, z0 + L - tip))          # sharpened to a point
    wood = hexf.extrude(V(0, 0, tip)); wood.translate(V(0, 0, z0 + L - tip)); wood = wood.common(cone)
    graphite = wood.common(Part.makeCylinder(1.05, tip + 5, V(0, 0, z0 + L - tip)))
    wood = wood.cut(graphite)
    return body, wood, graphite

def gripper_spigot():   # bolts under the existing gripper base plate
    return spigot().fuse(Part.makeCylinder(SPIGOT_D / 2, 2.0, V(0, 0, 0)))

parts = {
    'tool_receiver': receiver(),
    'pencil_module': pencil_module(),
    'pencil_collet_ring': collet_ring(),
    'gripper_spigot': gripper_spigot(),
}
pb, pw, pg = pencil()
vis = {'pencil_paint': pb, 'pencil_wood': pw, 'pencil_graphite': pg}
for name, shp in list(parts.items()) + list(vis.items()):
    shp = shp.removeSplitter() if hasattr(shp, 'removeSplitter') else shp
    if name in parts: shp.exportStep(os.path.join(OUT, name + '.step'))
    mesh = MeshPart.meshFromShape(Shape=shp, LinearDeflection=0.04, AngularDeflection=0.2, Relative=False)
    mesh.write(os.path.join(OUT, name + '.stl'))
    bb = shp.BoundBox
    print(f"PART {name:20s} vol {shp.Volume/1000:7.1f} cm3  valid {shp.isValid()}  "
          f"bb x {bb.XMin:.1f}..{bb.XMax:.1f} y {bb.YMin:.1f}..{bb.YMax:.1f} z {bb.ZMin:.1f}..{bb.ZMax:.1f}  tris {mesh.CountFacets}")
