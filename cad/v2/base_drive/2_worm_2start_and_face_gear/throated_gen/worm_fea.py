import sys, pickle, math, numpy as np, meshio, shapely, shapely.affinity, time
sys.path.insert(0,sys.argv[1]); from fea_common import *
S=sys.argv[1]+"/"; t0=time.time()
GX,GY,WZ=16.5,2.2,6.0; WY=GY+11.55
m=meshio.read(S+"worm.msh"); tet=m.cells_dict["tetra10"][:,:4]
used=np.unique(tet); remap=-np.ones(m.points.shape[0],int); remap[used]=np.arange(len(used))
mesh=MeshTet(m.points[used].T.copy(), remap[tet].T.copy())
e=ElementVector(ElementTetP2()); C=C_transiso(axis=0)   # worm printed axis-vertical: layer normal = worm axis (x)
K=assemble_chunked(mesh,e,make_form(C)); basis=Basis(mesh,e,intorder=2)
print("assembled dofs %d (%.0fs)"%(K.shape[0],time.time()-t0),flush=True)
X=basis.doflocs; Xn=X[:,0::3]
D_clamp=basis.get_dofs(mesh.facets_satisfying(lambda x: x[0]<-5.99)).all()
bore=mesh.facets_satisfying(lambda x: np.hypot(x[1]-WY,x[2]-WZ)<2.75)
D_bore=np.concatenate([basis.get_dofs(bore).all("u^2"),basis.get_dofs(bore).all("u^3")])
D=np.unique(np.concatenate([D_clamp,D_bore]))
P=pickle.load(open(S+"patches.pkl","rb"))
bf=mesh.boundary_facets(); mid=mesh.p[:,mesh.facets[:,bf]].mean(axis=1)
res={}
for name in ("old","new"):
    sh=-6.36*5*P[name]["phi"]/360.0; pt=[(z,shapely.affinity.translate(o,sh,0)) for z,o in P[name]["patch"]]; zs=np.array([z for z,_ in pt]); areas=np.array([o.area for _,o in pt])
    sel=[]; pz={}
    for (z,o),A in zip(pt,areas):
        poly=o.buffer(0.12)
        k=np.where((np.abs(mid[2]-z)<=0.026)&shapely.contains_xy(poly,mid[0],mid[1]))[0]
        if len(k): sel.append(bf[k]); pz[z]=A
    sel=np.unique(np.concatenate(sel)) if sel else np.array([],int)
    fb=FacetBasis(mesh,e,facets=sel,intorder=3)
    zk=np.array(sorted(pz)); Ak=np.array([pz[z] for z in zk])
    # per-slice pressure so each slice's force is proportional to its overlap; normalise later
    fa=fb.mesh.p[:,fb.mesh.facets[:,sel]]; 
    @LinearForm
    def load(v,w):
        z=w.x[2]; i=np.clip(np.searchsorted(zk,z-0.025),0,len(zk)-1)
        p=Ak[i]
        return -p*(v[0]*w.n[0]+v[1]*w.n[1]+v[2]*w.n[2])
    f=load.assemble(fb)
    F=np.array([f[0::3].sum(),f[1::3].sum(),f[2::3].sum()])
    f=f*(100.0/abs(F[0])); F=F*(100.0/abs(F[0]))   # normalise: 100 N gear-tangential (worm axial) load
    u,it,rr=solve(K,f,D,Xn)
    s=stresses(basis,u,C)                    # (6,nel,nqp)
    cen=mesh.p[:,mesh.t].mean(axis=1)
    zone=(cen[0]>9)&(cen[0]<24)&(np.hypot(cen[1]-WY,cen[2]-WZ)>3.2)  # thread zone, away from bore BC
    sxx=s[0][zone].max(axis=1); tau=np.sqrt(s[4]**2+s[5]**2)[zone].max(axis=1)
    sv=np.sqrt(0.5*((s[0]-s[1])**2+(s[1]-s[2])**2+(s[2]-s[0])**2)+3*(s[3]**2+s[4]**2+s[5]**2))[zone].max(axis=1)
    from scipy.spatial import cKDTree
    lm=fb.mesh.p[:,fb.mesh.facets[:,sel]].mean(axis=1).T
    dist=cKDTree(lm).query(cen.T[zone])[0]
    far=dist>0.4
    a=s[0][zone].max(axis=1)[far]; t=np.sqrt(s[4]**2+s[5]**2)[zone].max(axis=1)[far]
    print("   BULK (>0.4 mm from contact) across-layer tension p99.9 %.1f max %.1f | layer shear p99.9 %.1f max %.1f MPa per 100 N"%(np.percentile(a,99.9),a.max(),np.percentile(t,99.9),t.max()),flush=True)
    res.setdefault(name+"_bulk",(np.percentile(a,99.9),np.percentile(t,99.9),a.max(),t.max()))
    rr_=np.hypot(cen[1]-WY,cen[2]-WZ)[zone]
    sxa=s[0][zone]; ta=np.sqrt(s[4]**2+s[5]**2)[zone]; smin=s[:3][:,zone].min(axis=0)
    for lab,msk in (("ROOT r5.2-6.0",(rr_>5.2)&(rr_<6.0)),("FLANK/contact r>6.0",rr_>=6.0)):
        a=sxa[msk].max(axis=1); t=ta[msk].max(axis=1)
        print("   %-20s across-layer tension p99.9 %.1f p99 %.1f | interlayer shear p99.9 %.1f p99 %.1f MPa per 100 N"%(lab,np.percentile(a,99.9),np.percentile(a,99),np.percentile(t,99.9),np.percentile(t,99)),flush=True)
        res.setdefault(name+"_zones",{})[lab]=(np.percentile(a,99.9),np.percentile(t,99.9))
    def pct(a): return "max %.1f / p99.9 %.1f / p99 %.1f"%(a.max(),np.percentile(a,99.9),np.percentile(a,99))
    umax=np.abs(u).reshape(-1,3).max(axis=0)
    print("%s patch: %d facets, z %.2f..%.2f | applied F = (%.0f, %.0f, %.0f) N | CG %d it rel %.1e"%(name,len(sel),zk.min(),zk.max(),*F,it,rr))
    print("   per 100 N gear-tangential load:  sigma_across_layers(tension) %s MPa"%pct(sxx))
    print("                                    interlayer shear            %s MPa"%pct(tau))
    print("                                    von Mises                   %s MPa | max disp %s mm"%(pct(sv),np.round(umax,4)),flush=True)
    ld=np.unique(fb.mesh.facets[:,sel]); U=u.reshape(-1,3)[:0]
    nod=np.where(np.abs(f[0::3])+np.abs(f[1::3])+np.abs(f[2::3])>0)[0]; Un=np.c_[u[0::3][nod],u[1::3][nod],u[2::3][nod]]; wts=np.abs(f[0::3][nod])+np.abs(f[1::3][nod])+np.abs(f[2::3][nod]); um=(Un*wts[:,None]).sum(0)/wts.sum()
    print("   loaded-face mean displacement per 100 N: %s mm (y = away from gear)"%np.round(um,4),flush=True)
    res[name]=dict(um=um,sxx=np.percentile(sxx,99.9),tau=np.percentile(tau,99.9),sv=np.percentile(sv,99.9),sxxmax=sxx.max(),F=F)
pickle.dump(res,open(S+"worm_res.pkl","wb")); print("done %.0fs"%(time.time()-t0))
