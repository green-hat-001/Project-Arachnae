# Pencil module + quarter-turn tool mount (2026-10-06)

Made for the film's repairability shot (rendered in Blender). **Designed, not
printed or fitted.** The real gripper is already **twist-locked** and twists off and on by hand. This folder's own receiver/spigot geometry is a CAD redesign of that
mount, and the **pencil module is the part that doesn't exist yet**.

`freecadcmd pencil_gen.py` regenerates everything (STEP + STL, mm, +Z = toward the tool, origin on
the interface plane; in `Full_arm_v2.stp` that is axis x −26.5, y 55.5, plane z 1022).

| Part | What | Print |
|---|---|---|
| `tool_receiver` | collar that slips over the forearm end (61×59 mm, 0.7 mm clearance per side), 4× M3 clamp holes, bayonet lip with 3 entry notches + an index arrow | open pocket up, no supports |
| `gripper_spigot` | the male half for the existing gripper: Ø45.5 spigot, 3 lugs to Ø51 | flat side down |
| `pencil_module` | same spigot + lugs, 45° chamfer into a knurled twist grip, cone body, Ø8.2 bore for a standard hex pencil, side window, slotted collet nose | spigot down, no supports |
| `pencil_collet_ring` | knurled ring with a tapered bore; pushed onto the nose it squeezes the collet | flat |
| `pencil_*` (STL only) | the pencil, for rendering | — |

Bayonet: lugs drop through the notches, then turn **40°** under a 3 mm lip (1 mm axial
clearance). Not yet in it: a detent to stop it unscrewing, and an electrical pass-through
for the gripper servo (its bus cable would need a slip-free connector or a slack loop).
