# Arena update plan — issues & 9.1.5x support

Working branch: `arena/01a018d4-noggitredb`

## All 41 open issues (GitLab prophecy-rp/noggit-red)

| # | Title | Status |
|---|-------|--------|
| 1 | Undo Stack corrupts sometimes (stamp brush) | |
| 2 | Batch sorting for rendering (transparency) | |
| 3 | Asset browser / Object palette MIME data format | |
| 4 | World loaded multiple times in map selector | |
| 7 | Optional runtime test coverage for assets | |
| 9 | Exported Normal Map wrong resolution (256x256) | |
| 10 | Vertex paint image mask broken | |
| 16 | Viewport gizmo not updating properly | |
| 17 | Multi-selection editing ignores object centers | |
| 23 | "Mouse move follow cursor" swaps x/y axes | |
| 24 | Pressure wheel sensitivity too high | |
| 25 | White square in Flatten/Blur lock mode opaque | |
| 26 | Custom stamp makes terrain flicker | |
| 27 | ModelRender _transparency_lookup out of range | |
| 28 | Light flickering in left corner of map | |
| 29 | VertexPainter image mask alpha #000000 | |
| 30 | Raise&Lower Min/Max blending | |
| 32 | Some downported M2 models do not render | |
| 33 | Shading does not update when importing heightmaps | |
| 34 | Standard rendering and tile loading (frustum) | |
| 35 | Massive useless memory allocation (texture_set) | |
| 36 | Object Palette Save/Load | |
| 37 | Copy asset path in Object Editor | |
| 43 | Object rotation speed framerate dependent | |
| 44 | Patch client with project folder | |
| 46 | Camera collision | |
| 47 | Invalid ADT offsets not selectable | |
| 50 | Error panel/console | |
| 51 | Game mode | |
| 52 | WorldSafeLocs.dbc editor | |
| 53 | QImage rescale broken (tiled edges import) | |
| 54 | Render missing models as error cubes | |
| 55 | Saving erases undo history | |
| 56 | Update Key Bindings help | |
| 57 | md5translate.trs wrong formatting for X < 10 | |
| 60 | Rotation gizmo pivot inaccuracy | |
| 61 | Missing rendering features | |
| 62 | Minimap generation: missing liquid lighting | |
| 63 | Shader hot reloading | |
| 64 | Can't remove sound entry files rows | |
| 65 | Waterfalls/animated textures get red placeholder box | |

## 9.1.5x (Shadowlands) support gaps to audit
- [ ] Project creation/version plumbing
- [ ] CASC archive reading
- [ ] DB2 (WDC3) database reading
- [ ] Modern ADT (split objects/LOD) reading + saving
- [ ] Modern M2 (skin/.anim) reading
- [ ] Modern WMO reading
- [ ] BLP2 / FileDataID handling / listfile
- [ ] Modern water/liquid
