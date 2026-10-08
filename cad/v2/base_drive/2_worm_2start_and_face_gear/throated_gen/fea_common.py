import numpy as np, scipy.sparse as sp, time
from skfem import *
from skfem.helpers import sym_grad
def C_transiso(axis, Ein=2000., Eax=1700., nu_in=0.38, nu_ax=0.35, Gax=600.):
    """transversely isotropic stiffness (Voigt xx,yy,zz,yz,xz,xy); 'axis' = build direction (layer normal)"""
    Gin=Ein/(2*(1+nu_in))
    E=[Ein,Ein,Ein]; E[axis]=Eax
    Sm=np.zeros((6,6))
    for i in range(3):
        for j in range(3):
            if i==j: Sm[i,j]=1/E[i]
            else:
                nu = nu_ax*E[i]/Eax if (i==axis or j==axis) else nu_in
                if j==axis: nu = nu_ax*E[i]/E[j] if False else nu_ax*Ein/Eax*(E[i]/Ein) if i!=axis else nu
                Sm[i,j]=-(nu_ax/Eax if (i==axis or j==axis) else nu_in/Ein)
    shear_axes={3:(1,2),4:(0,2),5:(0,1)}
    for k,(a,b) in shear_axes.items():
        Sm[k,k]=1/(Gax if axis in (a,b) else Gin)
    return np.linalg.inv(Sm)
def voigt(e): return [e[0,0],e[1,1],e[2,2],2*e[1,2],2*e[0,2],2*e[0,1]]
def make_form(C):
    nz=[(i,j,C[i,j]) for i in range(6) for j in range(6) if abs(C[i,j])>1e-9]
    @BilinearForm
    def a(u,v,w):
        Eu=voigt(sym_grad(u)); Ev=voigt(sym_grad(v)); s=0
        for i,j,c in nz: s=s+c*Eu[j]*Ev[i]
        return s
    return a
def assemble_chunked(mesh,elem,form,chunk=8000,intorder=None):
    K=None; n=mesh.t.shape[1]
    for s in range(0,n,chunk):
        b=Basis(mesh,elem,elements=np.arange(s,min(n,s+chunk)),**({"intorder":intorder} if intorder else {}))
        k=form.assemble(b).tocsr()
        K=k if K is None else K+k
    return K
def rigid_modes(X):
    x,y,z=X; n=X.shape[1]; B=np.zeros((3*n,6))
    B[0::3,0]=1; B[1::3,1]=1; B[2::3,2]=1
    B[0::3,3]=-y; B[1::3,3]=x
    B[1::3,4]=-z; B[2::3,4]=y
    B[0::3,5]=z;  B[2::3,5]=-x
    return B
def solve(K,f,D,X):
    import pyamg
    I=np.setdiff1d(np.arange(K.shape[0]),D)
    Kii=K[I][:,I].tocsr(); fi=f[I]
    B=rigid_modes(X)[I] if X is not None else None
    ml=pyamg.smoothed_aggregation_solver(Kii,B=B,max_coarse=400,max_levels=12,coarse_solver="splu")
    res=[]; ui=ml.solve(fi,tol=1e-8,accel="cg",maxiter=400,residuals=res)
    u=np.zeros(K.shape[0]); u[I]=ui
    return u,len(res),res[-1]/res[0]
def stresses(basis,u,C):
    g=basis.interpolate(u).grad           # (3,3,nel,nqp)
    e=0.5*(g+np.transpose(g,(1,0,2,3)))
    E=np.array(voigt(e))                  # (6,nel,nqp)
    return np.einsum('ij,jeq->ieq',C,E)   # Voigt stress
