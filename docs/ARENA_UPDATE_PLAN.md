# Arena update plan — issues & 9.1.5x support

Working branch: `arena/01a018d4-noggitredb`

## All 41 open issues (GitLab prophecy-rp/noggit-red)

| # | Title | Status |
|---|-------|--------|
| 1 | Undo Stack corrupts sometimes (stamp brush) | fixed (63803f8) |
| 2 | Batch sorting for rendering (transparency) | |
| 3 | Asset browser / Object palette MIME data format | fixed (63803f8) |
| 4 | World loaded multiple times in map selector | already fixed in baseline |
| 7 | Optional runtime test coverage for assets | |
| 9 | Exported Normal Map wrong resolution (256x256) | fixed (3c48bb6) |
| 10 | Vertex paint image mask broken | already fixed in baseline |
| 16 | Viewport gizmo not updating properly | already fixed in baseline |
| 17 | Multi-selection editing ignores object centers | fixed (63803f8) |
| 23 | "Mouse move follow cursor" swaps x/y axes | already fixed in baseline |
| 24 | Pressure wheel sensitivity too high | already fixed in baseline |
| 25 | White square in Flatten/Blur lock mode opaque | already fixed in baseline |
| 26 | Custom stamp makes terrain flicker | fixed (63803f8) |
| 27 | ModelRender _transparency_lookup out of range | already fixed in baseline |
| 28 | Light flickering in left corner of map | fixed (63803f8) |
| 29 | VertexPainter image mask alpha #000000 | already fixed in baseline |
| 30 | Raise&Lower Min/Max blending | already fixed in baseline |
| 32 | Some downported M2 models do not render | fixed (63803f8) |
| 33 | Shading does not update when importing heightmaps | already fixed in baseline |
| 34 | Standard rendering and tile loading (frustum) | fixed |
| 35 | Massive useless memory allocation (texture_set) | already fixed in baseline |
| 36 | Object Palette Save/Load | fixed |
| 37 | Copy asset path in Object Editor | already fixed in baseline |
| 43 | Object rotation speed framerate dependent | already fixed in baseline |
| 44 | Patch client with project folder | fixed |
| 46 | Camera collision | fixed |
| 47 | Invalid ADT offsets not selectable | already fixed in baseline |
| 50 | Error panel/console | fixed (c2c7b18) |
| 51 | Game mode | fixed |
| 52 | WorldSafeLocs.dbc editor | fixed (d56340e) |
| 53 | QImage rescale broken (tiled edges import) | fixed (3c48bb6) |
| 54 | Render missing models as error cubes | fixed (63803f8) |
| 55 | Saving erases undo history | already fixed in baseline |
| 56 | Update Key Bindings help | fixed (63803f8) |
| 57 | md5translate.trs wrong formatting for X < 10 | already fixed in baseline |
| 60 | Rotation gizmo pivot inaccuracy | already fixed in baseline |
| 61 | Missing rendering features | fixed: blended-batch sort vs water (beams). Skybox, flight bounds, tex transforms, unlit already implemented. Remaining wishlist: particles/ribbons, WMO liquid |
| 62 | Minimap generation: missing liquid lighting | fixed |
| 63 | Shader hot reloading | fixed (3d4f4dd) |
| 64 | Can't remove sound entry files rows | already fixed in baseline |
| 65 | Waterfalls/animated textures get red placeholder box | already fixed in baseline |

## 9.1.5x (Shadowlands) support gaps to audit
- [ ] Project creation/version plumbing
- [ ] CASC archive reading
- [ ] DB2 (WDC3) database reading
- [ ] Modern ADT (split objects/LOD) reading + saving
- [ ] Modern M2 (skin/.anim) reading
- [ ] Modern WMO reading
- [ ] BLP2 / FileDataID handling / listfile
- [ ] Modern water/liquid
