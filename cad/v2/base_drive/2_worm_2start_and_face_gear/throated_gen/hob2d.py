import sys, time, pickle; sys.path.insert(0,sys.argv[1])
from common2d import *
H=3.8; RT=5.39+0.35; RIM=CD-math.sqrt(RT**2-H**2); T=RIM-5.91
s=D["s"]; STEP=0.25
# cutter scaled radially by s already thickens the thread by ~0.206 mm axial at pitch = the backlash; no extra widening
d=0.0
def rb(z):
    a=abs(z-WZ)
    r=RIM+0.05 if a<=H else (RIM+0.05-(a-H) if a<=H+T else 5.91)
    if a<RT: r=min(r, CD-math.sqrt(RT**2-a**2))
    return r
disc=lambda r: Point(GX,GY).buffer(r,resolution=256)
clipbox=box(GX-12,-50,GX+12,50)
t0=time.time(); out={}
zs=sorted(float(k) for k in D["G"])
for k in D["G"]:
    z=float(k)
    G=rings_to_geom(D["G"][k])
    if z< -3.0+1e-9:
        out[z]=G; continue
    blank=disc(rb(z)).difference(disc(3.9))
    if k in D["Wp"]:
        r=rack(D["Wp"][k],scale=s).intersection(clipbox)
        rt=r
        U=unary_union([pose(rt,i*STEP,1).intersection(disc(RIM+0.4)) for i in range(int(36/STEP))])
        cut=unary_union([affinity.rotate(U,36*m,origin=(GX,GY)) for m in range(10)])
        blank=blank.difference(cut)
    out[z]=make_valid(unary_union([blank,G.intersection(disc(3.95))]))
    if int(round(z*20))%40==0: print("z %.2f  area %.2f  (%.0fs)"%(z,out[z].area,time.time()-t0),flush=True)
pickle.dump({"slices":out,"dz":0.05,"RIM":RIM},open(S+"new_slices.pkl","wb"))
print("done, %d slices, volume %.1f mm3, %.0fs"%(len(out),sum(g.area for g in out.values())*0.05,time.time()-t0))
