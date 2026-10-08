import os
import Part, FreeCAD as App, json, math, time
V=App.Vector
S=os.environ.get("THROAT_SCRATCH", "/tmp/throated_gen/")  # intermediate files
GX,GY,WZ=16.5,2.2,6.0; WY=GY+11.55; s=(7.19+0.35)/7.19
t0=time.time()
w=Part.read("base_worm_2start/Worm_2start_v2.step").common(Part.makeBox(26,40,40,V(4,WY-20,WZ-20)))
g=Part.read("base_worm_2start/Gear_10T_2start_stub.step")
def sec(shape,z):
    out=[]
    for wi in shape.slice(V(0,0,1),z):
        pts=[]
        for e in wi.OrderedEdges:
            d=e.discretize(Deflection=0.002)
            if e.Orientation=="Reversed": pass
            if pts and (pts[-1]-d[0]).Length>(pts[-1]-d[-1]).Length: d=d[::-1]
            pts+= d if not pts else d[1:]
        out.append([(round(p.x,5),round(p.y,5)) for p in pts])
    return out
dz=0.05
zs=[round(-3.0+i*dz,4) for i in range(int(16/dz)+1)]
W={}; Wp={}; G={}
for z in zs:
    if abs(z-WZ)<7.19: W[z]=sec(w,z)
    zp=WZ+(z-WZ)/s
    if abs(zp-WZ)<7.19: Wp[z]=sec(w,zp)
for z in [round(-4.5+i*dz,4) for i in range(int(17.5/dz)+1)]:
    G[z]=sec(g,z)
# screw-sign check: worm rotated +30 deg about +x vs translated
wr=w.copy(); wr.rotate(V(0,WY,WZ),V(1,0,0),30)
chk={"rot30":sec(wr,WZ+2.5),"base":sec(w,WZ+2.5)}
json.dump({"W":{str(k):v for k,v in W.items()},"Wp":{str(k):v for k,v in Wp.items()},"G":{str(k):v for k,v in G.items()},"chk":chk,"s":s},open(S+"sections.json","w"))
print("exported W %d Wp %d G %d in %.0fs"%(len(W),len(Wp),len(G),time.time()-t0))
