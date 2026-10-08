import sys, pickle; sys.path.insert(0,sys.argv[1])
from common2d import S
exec(open(S+"verify2d.py").read().split('t0=time.time()')[0])   # reuse setup + overlap/edge
pen=math.degrees(0.03/5.06)
out={}
for name,gear,ph in (("old",OLD,25.0),("new",NEWk,0.0)):
    best=None
    for phi in (0,6,12,18,24,30):
        for sgn in (1,-1):
            e=edge(gear,phi,ph,sgn)+sgn*pen
            v,_,per=overlap(gear,phi,ph,e,detail=True)
            if best is None or v<best[0]: best=(v,phi,sgn,e,per)
    v,phi,sgn,e,per=best
    # body frame -> assembly frame (worm at rotation theta=5*phi): rotate by ph+e-phi about gear axis
    rot=ph+e-phi
    pts=[(z,affinity.rotate(o,rot,origin=(GX,GY))) for z,o in per if o.area>1e-8]
    out[name]=dict(vol=v,phi=phi,sgn=sgn,eps=e,rot=rot,patch=pts)
    zz=[z for z,_ in pts]
    print("%s worst phase phi=%d dir %+d: overlap %.4f mm3, z %.2f..%.2f, %d slices"%(name,phi,sgn,v,min(zz),max(zz),len(pts)))
pickle.dump(out,open(S+"patches.pkl","wb"))
