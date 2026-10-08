import json, math, sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from params import *
from shapely.geometry import Polygon, box, Point
from shapely.ops import unary_union
from shapely import affinity
from shapely.validation import make_valid
D = json.load(open(S + "face_sections.json"))
P = LEAD / STARTS
TAU = 360.0 / N                          # one gear tooth period, deg
def rings_to_geom(rings):
    polys = [make_valid(Polygon(r)) for r in rings if len(r) >= 4]
    if not polys: return Polygon()
    polys.sort(key=lambda p: -p.area); g = Polygon()
    for p in polys: g = g.symmetric_difference(p)
    return make_valid(g)
def worm_parts(rings, scale=1.0, x0=11.0):
    """(tiled thread rack, static body) of one worm section. The rack is one clean axial pitch tiled;
    it is clipped to the thread zone after it moves. The body (x < zone) is a solid of revolution: it never moves."""
    g = rings_to_geom(rings)
    if scale != 1.0: g = affinity.scale(g, 1.0, scale, origin=(0, WY))
    band = box(-50, WY - 8, 80, WY + 8)
    cell = g.intersection(box(x0, WY - 8, x0 + P, WY + 8))
    rk = make_valid(unary_union([affinity.translate(cell, k * P, 0) for k in range(-10, 14)])).buffer(0)
    body = g.intersection(box(-50, WY - 8, ZONE[0], WY + 8)).buffer(0)
    return rk, body
ZONEBOX = box(ZONE[0], WY - 8, ZONE[1], WY + 8)
def trans(phi): return (N / STARTS) * LEAD * phi / 360.0
def wpose(rk, body, phi, eps=0.0, dcd=0.0):
    """worm at gear angle phi (deg) in the gear body frame; eps = extra gear rotation (backlash probe),
    dcd = centre-distance error (+ = worm further away)"""
    w = unary_union([affinity.translate(rk, trans(phi), 0).intersection(ZONEBOX), body])
    if dcd: w = affinity.translate(w, 0, dcd)
    return affinity.rotate(w, phi - eps, origin=(GX, GY))
disc = lambda r: Point(GX, GY).buffer(r, resolution=256)
