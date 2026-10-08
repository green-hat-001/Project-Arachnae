import sys, pickle, struct, numpy as np; sys.path.insert(0,sys.argv[1])
from common2d import *
from shapely import constrained_delaunay_triangles
from shapely.geometry.polygon import orient
OUT=sys.argv[2]
sl=pickle.load(open(S+"new_slices.pkl","rb"))["slices"]
zs=sorted(z for z in sl if -4.5+1e-6<z<13.0-1e-6)
P=[make_valid(sl[z].simplify(0.008,preserve_topology=True)).buffer(0) for z in zs]
bnd=[-4.5]+[(zs[i]+zs[i+1])/2 for i in range(len(zs)-1)]+[13.0]
tris=[]
def polys(g): return [p for p in getattr(g,"geoms",[g]) if p.geom_type=="Polygon" and p.area>1e-9]
def flat(g,z,up):
    for p in polys(g):
        for t in getattr(constrained_delaunay_triangles(p),"geoms",[]):
            c=list(orient(t,1.0).exterior.coords)[:3]
            if not up: c=c[::-1]
            tris.append([(x,y,z) for x,y in c])
for i,g in enumerate(P):
    z0,z1=bnd[i],bnd[i+1]
    for p in polys(g):
        p=orient(p,1.0)
        for ring in [p.exterior]+list(p.interiors):
            c=list(ring.coords)
            for a,b in zip(c[:-1],c[1:]):
                tris.append([(a[0],a[1],z0),(b[0],b[1],z0),(b[0],b[1],z1)])
                tris.append([(a[0],a[1],z0),(b[0],b[1],z1),(a[0],a[1],z1)])
flat(P[0],bnd[0],False); flat(P[-1],bnd[-1],True)
for i in range(len(P)-1):
    z=bnd[i+1]
    flat(P[i].difference(P[i+1]),z,True); flat(P[i+1].difference(P[i]),z,False)
with open(OUT,"wb") as f:
    f.write(b"Arachnae throated worm wheel 10T 2-start".ljust(80,b" ")); f.write(struct.pack("<I",len(tris)))
    for t in tris:
        a,b,c=(np.array(v) for v in t); n=np.cross(b-a,c-a); l=np.linalg.norm(n); n=n/l if l>0 else n
        f.write(struct.pack("<12fH",*n,*a,*b,*c,0))
print("triangles",len(tris),"vol(slices) %.1f"%sum(p.area*(bnd[i+1]-bnd[i]) for i,p in enumerate(P)))
