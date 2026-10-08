# check the hobbed face gear against the REAL (unscaled) worm, slice by slice
import time, pickle
from common_face import *
G = pickle.load(open(S + "face_slices_N%d.pkl" % N, "rb"))["slices"]
gz = sorted(G)
def gat(z):
    k = min(gz, key=lambda q: abs(q - z)); return G[k] if abs(k - z) < 0.03 else None
WK = sorted(float(k) for k in D["W"])
WR = {z: worm_parts(D["W"]["%g" % z if "%g" % z in D["W"] else str(z)]) for z in WK}
def overlap(phi, eps=0.0, dz=0.0, dcd=0.0, detail=False):
    """gear lowered by dz (+ = away from the worm): gear slice z meets worm section z + dz"""
    vol = 0; gap = 9; per = []
    for z in WK:
        g = gat(z + dz)
        if g is None or g.is_empty: continue
        rk, body = WR[z]
        w = wpose(rk, body, phi, eps, dcd)
        o = w.intersection(g); a = o.area; vol += a * DZ
        if detail: per.append((z, o))
        elif a < 1e-9: gap = min(gap, w.distance(g))
    return vol, gap, per
def edge(phi, sgn, **kw):
    lo, hi = 0.0, 4.0 * sgn
    for _ in range(13):
        m = (lo + hi) / 2
        if overlap(phi, m, **kw)[0] < 1e-7: lo = m
        else: hi = m
    return lo
t0 = time.time()
print("N=%d  r_p %.3f  ratio %.2f  Z_TOP %.2f" % (N, RP, N / STARTS, Z_TOP))
worst = 9; badv = 0
for i in range(13):
    v, g, _ = overlap(i * TAU / 12); badv = max(badv, v); worst = min(worst, g)
print("1. full tooth period vs real worm: max overlap %.5f mm3, min gap %.3f mm  (%.0fs)" % (badv, worst, time.time() - t0), flush=True)
bl = []
for phi in (0, TAU / 3, 2 * TAU / 3):
    up = edge(phi, 1); lo = edge(phi, -1); bl.append(up - lo)
print("2. backlash at gear: %s deg = %s mm at r_p" % (" ".join("%.2f" % b for b in bl), " ".join("%.3f" % math.radians(b * RP) if False else "%.3f" % (math.radians(b) * RP) for b in bl)), flush=True)
for lab, kw in (("gear 0.3 high", dict(dz=-0.3)), ("gear 0.3 low", dict(dz=0.3)), ("gear 0.6 low", dict(dz=0.6)),
                ("worm 0.2 closer (cd)", dict(dcd=-0.2)), ("worm 0.3 further (cd)", dict(dcd=0.3))):
    vs = [overlap(i * TAU / 8, **kw)[:2] for i in range(8)]
    e = [edge(0, s_, **kw) for s_ in (1, -1)]
    print("3. %-22s max overlap %.4f mm3, min gap %.3f, backlash %.2f deg" % (lab, max(v[0] for v in vs), min(v[1] for v in vs), e[0] - e[1]), flush=True)
pen = math.degrees(0.03 / RP)
tot = []
for phi in [i * TAU / 6 for i in range(6)]:
    for sgn in (1, -1):
        e = edge(phi, sgn) + sgn * pen
        v, _, per = overlap(phi, e, detail=True)
        teeth = set(); zr = []; rr = []; yy = []
        for z, o in per:
            for p in getattr(o, "geoms", [o]):
                if p.area > 1e-7:
                    zr.append(z); c = p.centroid
                    rr.append(math.hypot(c.x - GX, c.y - GY)); yy.append(c.x - GX)
                    teeth.add(round(math.degrees(math.atan2(c.y - GY, c.x - GX) - (phi - e) * 0) / TAU))
        tot.append((v, len(teeth), min(zr), max(zr), min(rr), max(rr), min(yy), max(yy)))
vs = [t[0] for t in tot]
print("4. contact at 0.03 mm penetration: overlap vol avg %.4f (min %.4f) mm3; teeth %s; z %.2f..%.2f; r %.1f..%.1f; along worm %.1f..%.1f" % (
    sum(vs) / len(vs), min(vs), sorted(set(t[1] for t in tot)), min(t[2] for t in tot), max(t[3] for t in tot),
    min(t[4] for t in tot), max(t[5] for t in tot), min(t[6] for t in tot), max(t[7] for t in tot)))
for z in (-1.2, -0.9, -0.6, -0.3, 0.0):
    g = gat(z)
    if g is None: continue
    lands = []
    for rr in (RP - 1, RP, RP + 1):
        ring = disc(rr + 0.1).difference(disc(rr - 0.1)); sl = g.intersection(ring)
        ws = [p.area / 0.2 for p in getattr(sl, "geoms", [sl]) if p.area > 1e-6]
        lands.append("r%.1f:%d teeth %.2f-%.2f" % (rr, len(ws), min(ws) if ws else 0, max(ws) if ws else 0))
    print("5. tooth thickness z %+.1f  %s" % (z, "  ".join(lands)))
print("done %.0fs" % (time.time() - t0))
