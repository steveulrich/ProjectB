# Dorado reference analysis

Status: visual interpretation updated with recovered source placements, October 30, 2026. Source coordinates are recovered data; camera fits and some bounds remain estimates. This document does not certify Unreal assets or resume the paused gameplay queue.

## Image identity and inspection

| Reference | Supplied attachment | Native size | Primary evidence |
| --- | --- | --- | --- |
| REF_01 | `codex-clipboard-692f632a-02a6-46c4-b44f-99f43eab5843.png` | 2048 × 918 | Court, stair construction, sun façade, foreground serpent profile |
| REF_02 | `codex-clipboard-16afbd05-0e5e-4270-b1ab-77d22607e169.png` | 1920 × 862 | Long-axis composition, narrow shrine, ring interruption, pylon scale |
| REF_03 | `codex-clipboard-5dcfd47a-5999-42d1-89ac-ba85151f0772.png` | 1920 × 862 | Sun façade correspondence, cliff depth, floating island, waterfall, sky |

All three originals were inspected and are retained under `SourceArt/Dorado/References/`. Detail crops remain local under ignored `Saved/Logs/dorado-reconstruction-20261030/reference-crops/`: `REF01_temple_pads.png`, `REF02_ring_shrine.png`, and `REF03_island.png`. They are inspection evidence rather than texture inputs and are not needed to view the committed references.

## Landmark correspondence

| Feature | Observed | Inferred | Unknown |
| --- | --- | --- | --- |
| Broad sun façade | REF_01 and REF_03 share the concentric luminous disc, pointed gold surround, horizontal terrace, and tall sloping ribs. | Both images show the same end from nearby oblique directions. | Rear construction and total hidden height. |
| Narrow shrine | REF_02 shows a slender pointed structure, vertical violet accent, long gold ribs, and no broad luminous sun disc. | This is the opposite end. It must retain distinct geometry. | Exact relationship to unseen back faces and supporting mass. |
| Lateral pads | REF_01 has blue-white at image-left and green at image-right; REF_02 reverses those colors. | This supports opposite viewing directions. It does not establish gameplay functions. | Whether pad colors vary with gameplay state. |
| Central floor medallion | Both arena views show a small center disc, concentric slate bands, and repeated gold stepped motifs. | This is a common midfield landmark. | Fine center icon and exact motif count where obscured. |
| Larger snake circles | A tan center with a drawn snake-like emblem and violet-blue outer band appears toward the camera in both arena views. | Similar circles may repeat on the two court halves. Motif similarity alone cannot identify the same physical circle. | Full placement and symmetry of unseen counterparts. |
| Broken ring | REF_02 shows an upper gap left of the head terminal, two unequal upper arcs, violet bridging, and a large right-side interruption. | Begin with one transverse assembly and inspect existing meshes before adding another. | Exact ring-plane location, hidden segment count, and any additional assemblies. |
| Giant serpent pylons | REF_01 exposes a long open-mouth profile; REF_02 frames the court with massive supports and differently foreshortened heads. | Profiles can belong to matching end-frame modules. Preserve their mouth depth and long projecting gold curls. | Number and placement of unseen pylons. |
| Floating waterfall island | REF_03 clearly shows a wide, thin canopy shelf above deeply tapered lobes, a smaller right shelf, roots, and a cyan fall. | REF_01's distant right-side island may belong to the same composition. | Exact correspondence and distance. |

**Resolved project convention:** preserve Y as arena length, X as width, and Z as up. The recovered source court center is `(513.97998, 666.29779, 73.674896)` meters. Existing imported floor bounds support mirroring source Y during conversion. ProjectB's existing art uses a uniform scale of 0.5. Thus the proposed source-to-project transform is `50 × (sourceX-centerX, -(sourceY-centerY), sourceZ-centerZ)` centimeters. Confirm it with the recovered asymmetric calibration object before full import.

The recovered `rel_el_arenatemple_group1` anchor is at source Y=479.10562. The tall narrow shrine is `rel_el_arenastatue_b_group`, whose target apex is approximately Z=3578 cm at the negative-Y end. `arenastatue_a` is a smaller foreground ornament, not the tall shrine. The sun disc's visible center is approximately target `(3, 5950, 961)` cm; an actor pivot is not a reliable image landmark.

The giant framing heads belong to the `rel_ed_serpent_wall_a_group`/`a_clump` and `b_group`/`b_clump` families, forming one pair at each end. At the negative-Y end, upright body centers are X=-677.628 and +678.968 cm, a separation of 1356.595 cm; each body and its matching clump must move together. The eight `snakeheadpillar_b` placements are smaller edge braces in four pairs with 1900 cm spacing. They must not drive the giant-pylon control. Two complementary `ring_a`/`ring_b` entities share midfield XY and form one assembly.

## Provisional layout and scale

The [provisional top-down layout](ProvisionalLayout.svg) records relationships rather than a surveyed floor plan. Solid shapes represent directly visible feature types; dashed shapes or labels mark inferred connections. Do not mirror the sun façade to make the narrow end.

The project has an actual character scale cue: `Source/LyraGame/Character/LyraCharacter.cpp:41` initializes the base Lyra capsule with radius 40 cm and half-height 90 cm. Blueprint or runtime overrides remain to be checked. Do not treat the capsule height as the artwork's exact anatomical height.

The following image-only ranges preceded source recovery and are retained as inference history. Use the exact source anchors and current adjustable parameters in `SourceArt/Dorado/dimensions.json` for assembly instead. Do not combine these obsolete rough ranges with the recovered geometry:

| Adjustable dimension | Initial estimate | Visual basis |
| --- | --- | --- |
| Main court length, excluding temple mass | 5600–7200 cm | Approximately two longer decorated halves around midfield |
| Main court width, excluding pad overhang | 2600–3400 cm | Open lanes, central disc, and sparse cover blocks |
| Side-platform rise | 160–220 cm | Roughly a dozen shallow stair risers |
| Side-platform outer radius | 250–340 cm | Pad relative to stairs and court width |
| Cover-block height | 80–110 cm | Approximately waist-height relative to the 180 cm scale cue |
| Ring outer diameter | 4200–6000 cm | Ring spans beyond the lateral court edges |
| Ring section radial thickness | 250–420 cm | Heavy angular border visible against the sky |
| Giant pylon height above court | 1000–1500 cm | Strong foreground framing in REF_02 |

The central configuration separates unit conversion from ProjectB's 0.5 integration scale and records provenance for every adjustable dimension. Recovered source data does not automatically establish screenshot-version parity or successful Unreal validation.

## Forms that determine the match

- Keep the court elongated, with circular ornament embedded in a continuous tan floor. Avoid turning the floor into disconnected round platforms.
- Preserve separate broad shallow stair runs and chunky stepped cheeks at each lateral platform. Gold follows edges and changes in elevation.
- Use deep retaining walls with block courses and gold corner strips. The arena is a substantial elevated structure, not a thin floating slab.
- Build serpent mouths as open volumes. The projecting snout, curled gold elements, teeth, jaw thickness, and recessed vertical support channels determine their silhouette.
- Give ring segments thick side walls, angular panel steps, gold borders, and violet inserts. Preserve the broken upper gap and hanging lower portions.
- Keep the sun disc cream-yellow and bright; surrounding gold remains reflective. The narrow shrine uses a vertical violet accent instead.
- Use rounded, spreading canopy clumps in several green values. Concentrate them on cliffs and island tops; leave court lanes open.
- The REF_03 island has a broad overhanging roof, two downward lobes, a smaller right shelf, and sparse roots. Its cyan fall flows down the face into haze.
- Distant scenery should occupy multiple depth bands: nearby cliffs, isolated floating shelves, far green mountains, and pale temple silhouettes.

## Camera targets

All source images are approximately 2.23:1. Preserve that aspect ratio for direct comparisons.

| Camera | Target composition | Important anchors |
| --- | --- | --- |
| `CAM_Ref_01` | Elevated oblique view toward the sun end; substantial downward pitch. | Sun disc near (0.23, 0.20), midfield near (0.43, 0.52), green pad near (0.54, 0.41), foreground giant head dominating right side. |
| `CAM_Ref_02` | Long-axis view from the sun-end direction toward the narrow shrine, framed by pylons. | Shrine tip near (0.58, 0.20), midfield near (0.56, 0.67), top ring gap near (0.54, 0.18), distant pyramid near left quarter. |
| `CAM_Ref_03` | Exterior view toward the sun temple and right-side gorge; near-level camera rather than a floor survey. | Sun disc cropped at left near (0.06, 0.65), green pad near (0.21, 0.89), large island top near (0.61, 0.60), pale distant temple at upper right. |

Coordinates are approximate fractions of image width and height, with origin at the upper-left. Lens and exposure are unknown. Start with a moderate perspective lens and fit landmark relationships; do not assert original FOV values.

Recovered LOD0 surface picks and manual image correspondences produced the camera suggestions in `camera_ref01_head_landmarks.json`, `camera_ref02_landmarks.json`, and `camera_ref03_surface_landmarks.json` under `Config/`. Their solver residuals are approximately 4.10, 2.68, and 4.13 pixels respectively. These are manual feature-fit residuals, not image-similarity scores. Actual native renders still determine acceptance. REF_01's near and far serpent-head features fit without changing source geometry scale.

## Review priorities and unresolved evidence

First compare court proportions, distinct temple ends, ring silhouette, and serpent profiles. Then compare floor bands, side stairs, retaining walls, and background depth. Material microdetail follows those corrections.

The screenshots cannot establish scoring zones, healing pads, spawning, destructibility, navigation, out-of-bounds behavior, or ball collision. Preserve existing systems and validate these separately. No traversal, material correctness, scene performance, or target-editor success is claimed by this reference analysis.
