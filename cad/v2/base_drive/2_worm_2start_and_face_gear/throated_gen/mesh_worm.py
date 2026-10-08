import os
import gmsh, sys, numpy as np
H=float(sys.argv[1]) if len(sys.argv)>1 else 0.35
gmsh.initialize(); gmsh.option.setNumber("General.Terminal",0); gmsh.option.setNumber("General.NumThreads",2)
gmsh.model.add("worm")
v=gmsh.model.occ.importShapes(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Worm_2start_v2.step"))
b=gmsh.model.occ.addBox(-6,0,-8,35,28,28)
out,_=gmsh.model.occ.intersect(v,[(3,b)]); gmsh.model.occ.synchronize()
# fine near the mesh zone (x 8..25), coarse elsewhere
f=gmsh.model.mesh.field
bx=f.add("Box"); f.setNumber(bx,"VIn",H); f.setNumber(bx,"VOut",1.2)
for k,val in (("XMin",8),("XMax",25.5),("YMin",0),("YMax",14.5),("ZMin",-2),("ZMax",14),("Thickness",3)): f.setNumber(bx,k,val)
f.setAsBackgroundMesh(bx)
gmsh.option.setNumber("Mesh.MeshSizeExtendFromBoundary",0); gmsh.option.setNumber("Mesh.MeshSizeFromPoints",0); gmsh.option.setNumber("Mesh.MeshSizeFromCurvature",0)
gmsh.option.setNumber("Mesh.ElementOrder",2); gmsh.option.setNumber("Mesh.SecondOrderLinear",0)
gmsh.model.mesh.generate(3)
et,tags,nodes=gmsh.model.mesh.getElements(3)
nt,xyz,_=gmsh.model.mesh.getNodes()
print("tets",len(tags[0]),"nodes",len(nt))
gmsh.write(sys.argv[2] if len(sys.argv)>2 else "/tmp/w.msh"); gmsh.finalize()
