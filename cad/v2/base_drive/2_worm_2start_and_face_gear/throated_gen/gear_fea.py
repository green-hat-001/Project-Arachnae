import sys, pickle, math, numpy as np, shapely, shapely.affinity, time
sys.path.insert(0,sys.argv[1]); from fea_common import *; from common2d import D, rings_to_geom, GX, GY
S=sys.argv[1]+"/"; t0=time.time(); h=0.15
P=pickle.load(open(S+"patches.pkl","rb")); W=pickle.load(open(S+"worm_res.pkl","rb"))
NEW=pickle.load(open(S+"new_slices.pkl","rb"))["slices"]
C=C_transiso(axis=2)                           # gear printed axis-vertical: layer normal = z
res={}
for name in ("old","new"):
    pp=P[name]; rot=pp["rot"]
    patch=[(z,shapely.affinity.rotate(o,-rot,origin=(GX,GY))) for z,o in pp["patch"]]   # into gear body frame
    cx=np.mean([o.centroid.x for _,o in patch]); cy=np.mean([o.centroid.y for _,o in patch])
    a0=math.atan2(cy-GY,cx-GX)
    sector=shapely.Polygon([(GX,GY)]+[(GX+12*math.cos(a0+math.radians(t)),GY+12*math.sin(a0+math.radians(t))) for t in np.linspace(-60,60,25)])
    zs=np.arange(-4.5+h/2,13.0,h)
    xs=np.arange(GX-8,GX+8+1e-9,h); ys=np.arange(GY-8,GY+8+1e-9,h)
    XX,YY=np.meshgrid(xs+h/2,ys+h/2,indexing="ij")
    occ=np.zeros((len(xs),len(ys),len(zs)),bool)
    for k,z in enumerate(zs):
        g=(NEW[min(NEW,key=lambda q:abs(q-z))] if name=="new" else rings_to_geom(D["G"][min(D["G"],key=lambda q:abs(float(q)-z))]))
        g=g.intersection(sector)
        occ[:,:,k]=shapely.contains_xy(g,XX,YY)
    # hex mesh from occupied voxels
    idx=np.argwhere(occ); nx,ny,nz=len(xs)+1,len(ys)+1,len(zs)+1
    nid=lambda i,j,k:(i*ny+j)*nz+k
    i,j,k=idx.T
    corners=[(0,0,0),(1,0,0),(1,1,0),(0,1,0),(0,0,1),(1,0,1),(1,1,1),(0,1,1)]
    T=np.array([nid(i+a,j+b,k+c) for a,b,c in corners])
    used,inv=np.unique(T,return_inverse=True); T=inv.reshape(T.shape)
    I_=used//(ny*nz); J_=(used//nz)%ny; K_=used%nz
    Pn=np.array([xs[0]+I_*h, ys[0]+J_*h, -4.5+K_*h])
    # skfem hex node order: (0,0,0),(1,0,0),(0,1,0),(0,0,1),(1,1,0),(1,0,1),(0,1,1),(1,1,1)
    T=T[[0,1,3,4,2,5,7,6]]
    mesh=MeshHex(Pn,T); e=ElementVector(ElementHex1())
    K=assemble_chunked(mesh,e,make_form(C),chunk=10000,intorder=2)
    print("%s: %d voxels, %d dofs (%.0fs)"%(name,T.shape[1],K.shape[0],time.time()-t0),flush=True)
    X=mesh.p; r=np.hypot(X[0]-GX,X[1]-GY); ang=np.degrees(np.arctan2(X[1]-GY,X[0]-GX))-math.degrees(a0); ang=(ang+180)%360-180
    fixed_nodes=np.where((r<2.75)|(np.abs(ang)>58.5))[0]       # rod in bore/pin hole + sector cut faces
    Dd=np.concatenate([3*fixed_nodes,3*fixed_nodes+1,3*fixed_nodes+2])
    # load: opposite of the force on the worm, rotated into the body frame, per slice weighted by overlap area
    Fw=W[name]["F"]; ca,sa=math.cos(math.radians(-rot)),math.sin(math.radians(-rot))
    Fg=-np.array([ca*Fw[0]-sa*Fw[1], sa*Fw[0]+ca*Fw[1], Fw[2]])
    f=np.zeros(3*X.shape[1]); Atot=sum(o.area for _,o in patch)
    bnd=np.unique(mesh.facets[:,mesh.boundary_facets()])
    for z,o in patch:
        nn=bnd[(np.abs(X[2,bnd]-z)<=h)&shapely.contains_xy(o.buffer(0.2),X[0,bnd],X[1,bnd])]
        if len(nn)==0: continue
        w=o.area/Atot/len(nn)
        for d in range(3): np.add.at(f,3*nn+d,Fg[d]*w)
    Fapp=np.array([f[0::3].sum(),f[1::3].sum(),f[2::3].sum()])
    u,it,rr=solve(K,f,Dd,X)
    s=stresses(basis,u,C)[:,:,0] if False else stresses(Basis(mesh,e,intorder=1),u,C)[:,:,0]
    cen=mesh.p[:,mesh.t].mean(axis=1); rc=np.hypot(cen[0]-GX,cen[1]-GY)
    ac=np.degrees(np.arctan2(cen[1]-GY,cen[0]-GX))-math.degrees(a0); ac=(ac+180)%360-180
    zone=(rc>3.6)&(np.abs(ac)<45)                       # teeth + rim, away from fixed faces
    szz=s[2][zone]; tau=np.sqrt(s[3]**2+s[4]**2)[zone]
    sxx,syy,sxy=s[0][zone],s[1][zone],s[5][zone]
    p1=(sxx+syy)/2+np.sqrt(((sxx-syy)/2)**2+sxy**2)      # max in-plane principal (tooth bending, along layers)
    from scipy.spatial import cKDTree
    nodl=np.where(np.abs(f[0::3])+np.abs(f[1::3])+np.abs(f[2::3])>0)[0]
    dist=cKDTree(X[:,nodl].T).query(cen.T[zone])[0]; far=dist>0.45
    print("   BULK (>0.45 mm from contact) in-layer %.1f (max %.1f) | across-layer %.1f (max %.1f) | layer shear %.1f (max %.1f) MPa per 100 N, p99.9"%(np.percentile(p1[far],99.9),p1[far].max(),np.percentile(szz[far],99.9),szz[far].max(),np.percentile(tau[far],99.9),tau[far].max()),flush=True)
    res.setdefault(name+"_bulk",(np.percentile(p1[far],99.9),np.percentile(szz[far],99.9),np.percentile(tau[far],99.9)))
    zc=cen[2][zone]; rcz=rc[zone]; dz_=zc-6.0
    rroot=np.where(np.abs(dz_)<7.54,11.55-np.sqrt(np.clip(7.54**2-dz_**2,0,None)),9)
    rroot=np.minimum(rroot,4.11+0*rroot) if name=="old" else rroot
    for lab,msk in (("ROOT",(rcz>rroot-0.2)&(rcz<rroot+0.6)),("TOOTH/contact",rcz>=rroot+0.6)):
        pa=p1[msk]; za=szz[msk]; ta=tau[msk]
        if msk.sum()==0: continue
        print("   %-14s in-layer %.1f | across-layer %.1f | interlayer shear %.1f MPa per 100 N (p99.9, %d el)"%(lab,np.percentile(pa,99.9),np.percentile(za,99.9),np.percentile(ta,99.9),msk.sum()),flush=True)
        res.setdefault(name+"_zones",{})[lab]=(np.percentile(pa,99.9),np.percentile(za,99.9),np.percentile(ta,99.9))
    def pct(a): return "p99.9 %.1f / p99 %.1f"%(np.percentile(a,99.9),np.percentile(a,99))
    print("   applied F %s N, CG %d it rel %.1e, max |u| %.4f mm"%(np.round(Fapp,1),it,rr,np.abs(u).max()))
    print("   per 100 N: in-layer tension (tooth bending) %s | across-layer tension %s | interlayer shear %s MPa"%(pct(p1),pct(szz),pct(tau)),flush=True)
    nod=np.where(np.abs(f[0::3])+np.abs(f[1::3])+np.abs(f[2::3])>0)[0]; Un=np.c_[u[0::3][nod],u[1::3][nod],u[2::3][nod]]; wts=np.abs(f[0::3][nod])+np.abs(f[1::3][nod])+np.abs(f[2::3][nod]); um=(Un*wts[:,None]).sum(0)/wts.sum()
    rdir=np.array([math.cos(a0),math.sin(a0),0]); print("   loaded-face mean displacement per 100 N: %s mm, radial (toward gear centre = away from worm) %.4f"%(np.round(um,4),-um@rdir),flush=True)
    res[name]=dict(um=um,urad=-um@rdir,p1=np.percentile(p1,99.9),szz=np.percentile(szz,99.9),tau=np.percentile(tau,99.9),umax=np.abs(u).max())
pickle.dump(res,open(S+"gear_res.pkl","wb")); print("done %.0fs"%(time.time()-t0))
