import os
import json, math, numpy as np
from shapely.geometry import Polygon, MultiPolygon, box, Point
from shapely.ops import unary_union
from shapely import affinity
from shapely.validation import make_valid
S=os.environ.get("THROAT_SCRATCH", "/tmp/throated_gen/")  # intermediate files
D=json.load(open(S+"sections.json"))
GX,GY,WZ,CD=16.5,2.2,6.0,11.55; WY=GY+CD; L=6.36; P=L/2
def rings_to_geom(rings):
    """even-odd fill of slice rings (outer + holes)"""
    polys=[make_valid(Polygon(r)) for r in rings if len(r)>=4]
    if not polys: return Polygon()
    polys.sort(key=lambda p:-p.area); g=Polygon()
    for p in polys: g=g.symmetric_difference(p)
    return make_valid(g)
def rack(rings, scale=1.0, x0=11.0):
    """periodic worm section: take one axial pitch from the clean middle, tile it"""
    g=rings_to_geom(rings)
    if scale!=1.0: g=affinity.scale(g,1.0,scale,origin=(0,WY))
    cell=g.intersection(box(x0,-50,x0+P,50))
    parts=[affinity.translate(cell,k*P,0) for k in range(-6,11)]
    return make_valid(unary_union(parts)).buffer(0)
def pose(g,phi,sigma):
    """worm geometry (in assembly coords) -> gear body frame at gear angle phi (deg): R(phi) T(sigma*L*5phi/360)"""
    t=affinity.translate(g,sigma*L*5*phi/360.0,0)
    return affinity.rotate(t,phi,origin=(GX,GY))
