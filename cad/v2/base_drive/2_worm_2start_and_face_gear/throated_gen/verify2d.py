import sys, time, pickle; sys.path.insert(0,sys.argv[1])
from common2d import *
NEW=pickle.load(open(S+"new_slices.pkl","rb"))["slices"]
DZ=0.05
ks=[k for k in D["W"]]                       # planes where the worm exists
Wr={k:rack(D["W"][k]).intersection(box(GX-12,-50,GX+12,50)) for k in ks}
OLD={k:rings_to_geom(D["G"][k]) for k in ks}
NEWk={k:NEW[float(k)] for k in ks}
def wpose(k,phi,ph,eps=0,dzoff=0,dcd=0):
    kk=k
    if dzoff:
        kk="%g"%round(float(k)+dzoff,4)
        if kk not in Wr: return None
    g=Wr[kk]
    if dcd: g=affinity.translate(g,0,-dcd)
    return affinity.rotate(affinity.translate(g,L*5*phi/360,0),phi-ph-eps,origin=(GX,GY))
def overlap(gear,phi,ph,eps=0,dzoff=0,dcd=0,detail=False):
    vol=0; gap=9; per=[]
    for k in ks:
        w=wpose(k,phi,ph,eps,dzoff,dcd)
        if w is None: continue
        o=w.intersection(gear[k]); a=o.area; vol+=a*DZ
        if detail: per.append((float(k),o))
        elif a<1e-9: gap=min(gap,w.distance(gear[k]))
    return (vol,gap,per)
def edge(gear,phi,ph,sgn):
    lo,hi=0.0,3.0*sgn
    for _ in range(12):
        m=(lo+hi)/2
        if overlap(gear,phi,ph,m)[0]<1e-7: lo=m
        else: hi=m
    return lo
t0=time.time()
print("== 1. full tooth period, new gear vs real worm (gear step 1.5 deg)")
worst=9; badv=0
for i in range(25):
    phi=i*1.5; v,g,_=overlap(NEWk,phi,0); badv=max(badv,v); worst=min(worst,g)
print("   max overlap %.5f mm3, min in-plane gap %.3f mm  (%.0fs)"%(badv,worst,time.time()-t0),flush=True)
print("== 2. backlash window")
for name,gear,ph in (("old",OLD,25.0),("new",NEWk,0.0)):
    for phi in (0,12,24):
        up=edge(gear,phi,ph,1); lo=edge(gear,phi,ph,-1)
        print("   %s phi %2d: [%+.2f,%+.2f] = %.2f deg = %.3f mm at pitch"%(name,phi,lo,up,up-lo,math.radians(up-lo)*5.06),flush=True)
print("== 3. misalignment, new gear (sweep of a tooth period)")
for lab,kw in (("gear 0.3 mm high",dict(dzoff=0.3)),("gear 0.3 mm low",dict(dzoff=-0.3)),("centre dist -0.15",dict(dcd=0.15)),("centre dist +0.3",dict(dcd=-0.3))):
    vs=[overlap(NEWk,i*3.0,0,**kw)[:2] for i in range(12)]
    print("   %-18s max overlap %.4f mm3, min gap %.3f"%(lab,max(v[0] for v in vs),min(v[1] for v in vs)),flush=True)
print("== 4. contact at equal penetration (0.03 mm at pitch, both drive directions)")
pen=math.degrees(0.03/5.06)
res={}
for name,gear,ph in (("old",OLD,25.0),("new",NEWk,0.0)):
    tot=[]
    for phi in (0,6,12,18,24,30):
        for sgn in (1,-1):
            e=edge(gear,phi,ph,sgn)+sgn*pen
            v,_,per=overlap(gear,phi,ph,e,detail=True)
            teeth=set(); zr=[]
            for z,o in per:
                if o.area>1e-6:
                    zr.append(z)
                    for p in getattr(o,"geoms",[o]):
                        if p.area>1e-7:
                            c=p.centroid; teeth.add(round(math.degrees(math.atan2(c.y-GY,c.x-GX))/36))
            tot.append((v,len(teeth),(min(zr),max(zr)) if zr else (0,0)))
    vs=[t[0] for t in tot]; res[name]=sum(vs)/len(vs)
    print("   %s: overlap vol avg %.4f mm3 (min %.4f max %.4f), teeth in contact %s, contact z-span %s"%(name,res[name],min(vs),max(vs),sorted(set(t[1] for t in tot)),sorted(set("%.1f..%.1f"%t[2] for t in tot))[:4]),flush=True)
print("   => contact area ratio new/old ~ %.1fx"%(res["new"]/res["old"]))
print("== 5. tooth land 0.15-0.35 mm below local tip")
for z in (-2.5,0,2,2.5,3,4,5,6,7,8,9,9.5,10,12,12.8):
    g=NEW[min(NEW,key=lambda q:abs(q-z))]
    rt=max(math.hypot(x-GX,y-GY) for p in getattr(g,"geoms",[g]) for x,y in p.exterior.coords)
    ring=Point(GX,GY).buffer(rt-0.15,resolution=512).difference(Point(GX,GY).buffer(rt-0.35,resolution=512))
    sl=g.intersection(ring); ws=sorted(p.area/0.2 for p in getattr(sl,"geoms",[sl]) if p.area>1e-6)
    print("   z %5.1f tip r %.2f: %d teeth, land %.2f-%.2f mm"%(z,rt,len(ws),ws[0] if ws else 0,ws[-1] if ws else 0))
print("done %.0fs"%(time.time()-t0))
