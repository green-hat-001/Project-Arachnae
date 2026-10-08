# virtual hobbing of the face gear, slice by slice (planes perpendicular to the gear axis)
import time, pickle
from common_face import *
s = D["s"]; STEP = 0.25
def gsec(z): return rings_to_geom(D["G"][z])
dbore = disc(3.95).difference(gsec("6.0").intersection(disc(3.95)))
pinh = disc(3.0).difference(gsec("-2.0").intersection(disc(3.0)))
def bores(g, z):
    if z < PIN_TOP: g = g.difference(pinh)
    if z > BORE_FLOOR: g = g.difference(dbore)
    return g
def disc_r(z):        # v1 web: flat bottom out to r 10, then 45 deg up to the case limit (curled when printed)
    if V1: return min(r_out(z), 10.0 + (z - Z_HUB_BOT))
    return r_out(z)   # v2: straight up from the bed at r 14.5, then stepping in with the case; no overhang

t0 = time.time(); out = {}
zs = [round(Z_HUB_BOT + DZ / 2 + i * DZ, 4) for i in range(int(round((Z_GEAR_TOP - Z_HUB_BOT) / DZ)))]
for z in zs:
    if z <= Z_TOP:
        g = disc(disc_r(z))
        k = min(D["Wp"], key=lambda q: abs(float(q) - z))
        if abs(float(k) - z) < 0.03:
            rk, body = worm_parts(D["Wp"][k], scale=s)
            U = unary_union([wpose(rk, body, i * STEP).intersection(disc(disc_r(z) + 0.3))
                             for i in range(int(math.ceil(TAU / STEP)) + 1)])
            cut = unary_union([affinity.rotate(U, TAU * m, origin=(GX, GY)) for m in range(N)])
            g = g.difference(cut).buffer(-OPEN, resolution=8).buffer(OPEN, resolution=8)
    else:
        r = HUB_TOP_R if z > Z_GEAR_TOP - 0.8 else r_hub(z)
        g = disc(r)
    out[z] = make_valid(bores(g, z))
    if int(round(z * 20)) % 10 == 0: print("z %.3f area %.2f (%.0fs)" % (z, out[z].area, time.time() - t0), flush=True)
pickle.dump({"slices": out, "dz": DZ, "N": N}, open(S + "face_slices_N%d.pkl" % N, "wb"))
print("done N=%d rp %.3f, %d slices, volume %.1f mm3, %.0fs" % (N, RP, len(out), sum(g.area for g in out.values()) * DZ, time.time() - t0))
