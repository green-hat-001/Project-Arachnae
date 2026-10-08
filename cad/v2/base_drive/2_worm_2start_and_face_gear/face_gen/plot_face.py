import pickle, sys, matplotlib; matplotlib.use("Agg"); import matplotlib.pyplot as plt
from common_face import *
sl = pickle.load(open(S + "face_slices_N%d.pkl" % N, "rb"))["slices"]
zs = [float(a) for a in sys.argv[1].split(",")]
fig, ax = plt.subplots(1, len(zs), figsize=(5 * len(zs), 5.5))
for a, z in zip(ax, zs):
    g = sl[min(sl, key=lambda q: abs(q - z))]
    for p in getattr(g, "geoms", [g]):
        if p.geom_type != "Polygon": continue
        x, y = p.exterior.xy; a.fill([v - GX for v in x], [v - GY for v in y], color="tab:blue", alpha=.6)
        for h in p.interiors: x, y = h.xy; a.fill([v - GX for v in x], [v - GY for v in y], color="white")
    a.axhline(CD, color="r", lw=.5); a.axhline(RP, color="g", lw=.5, ls=":")
    a.set_aspect("equal"); a.set_title("N=%d z %.2f" % (N, z)); a.set_xlim(-14, 14); a.set_ylim(-14, 14)
plt.tight_layout(); plt.savefig(S + "face_N%d.png" % N, dpi=55)
