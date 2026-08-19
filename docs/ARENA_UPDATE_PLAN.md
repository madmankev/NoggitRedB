# Arena update plan — issues & 9.1.5x support

Working branch: `arena/01a018d4-noggitredb`

## All 41 open issues (GitLab prophecy-rp/noggit-red)

| # | Title | Status |
|---|-------|--------|
| 1 | Undo Stack corrupts sometimes (stamp brush) | fixed (63803f8) |
| 2 | Batch sorting for rendering (transparency) | fixed (0d446da) |
| 3 | Asset browser / Object palette MIME data format | fixed (63803f8) |
| 4 | World loaded multiple times in map selector | already fixed in baseline |
| 7 | Optional runtime test coverage for assets | fixed (33fd44a) |
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
| 34 | Standard rendering and tile loading (frustum) | fixed (e6fe1d4) |
| 35 | Massive useless memory allocation (texture_set) | already fixed in baseline |
| 36 | Object Palette Save/Load | fixed (04f8b85) |
| 37 | Copy asset path in Object Editor | already fixed in baseline |
| 43 | Object rotation speed framerate dependent | already fixed in baseline |
| 44 | Patch client with project folder | fixed (839e1cd) |
| 46 | Camera collision | fixed (aa226ca) |
| 47 | Invalid ADT offsets not selectable | already fixed in baseline |
| 50 | Error panel/console | fixed (c2c7b18) |
| 51 | Game mode | fixed (aa226ca) |
| 52 | WorldSafeLocs.dbc editor | fixed (d56340e) |
| 53 | QImage rescale broken (tiled edges import) | fixed (3c48bb6) |
| 54 | Render missing models as error cubes | fixed (63803f8) |
| 55 | Saving erases undo history | already fixed in baseline |
| 56 | Update Key Bindings help | fixed (63803f8) |
| 57 | md5translate.trs wrong formatting for X < 10 | already fixed in baseline |
| 60 | Rotation gizmo pivot inaccuracy | already fixed in baseline |
| 61 | Missing rendering features | fixed (64f0123): blended-batch sort vs water (beams). Skybox, flight bounds, tex transforms, unlit already implemented. Remaining wishlist: particles/ribbons, WMO liquid |
| 62 | Minimap generation: missing liquid lighting | fixed (d44f211) |
| 63 | Shader hot reloading | fixed (3d4f4dd) |
| 64 | Can't remove sound entry files rows | already fixed in baseline |
| 65 | Waterfalls/animated textures get red placeholder box | already fixed in baseline |

## 9.1.5x (Shadowlands) support gaps to audit
- [x] Project creation/version plumbing — Shadowlands expansion selectable on project creation (`ProjectVersion::SL`, build `9.1.0.39584`, enUS locale), client path validated on creation (`.build.info` / `_retail_\.build.info` detected, `_retail_` hint shown)
- [x] CASC archive reading — local client storage opened through CascLib (`.build.info`), online mode optional
- [x] DB2 (WDC3) database reading — `BlizzardDatabase` no longer hardcodes `.dbc`; projects on modern clients resolve `DBFilesClient\<Table>.db2`, format auto-detected from file magic (WDC3 vs WDBC). This unbroke the map list on 9.1.5 projects
- [ ] Modern ADT (split objects/LOD) reading + saving — **not supported**; modern ADTs cannot be opened yet (saving is blocked on SL projects to avoid data corruption, see below)
- [~] Modern M2 (skin/.anim) reading — M2 version gate recognizes modern M2s (`m2_version_legion_bfa_sl`), BLP2/FDID chunk handling partial; full .skin/.anim pipeline not validated
- [ ] Modern WMO reading — not validated on 9.1.5 data
- [x] BLP2 / FileDataID handling / listfile — project creation offers to copy a user-selected `listfile.csv` into the project; project load fails early with a helpful message when it is missing instead of an obscure error; downloads from wago.tools / wow.tools referenced
- [ ] Modern water/liquid — not validated on 9.1.5 data

### 9.1.5x project requirements (user facing)
1. Point the *game client path* at a folder containing `.build.info` (a Battle.net install root or its `_retail_` sub folder).
2. Place the community `listfile.csv` (FileDataID → path mapping, from e.g. wago.tools or wow.tools) into the *project folder* — the creation dialog can do this for you.
3. Reading client data (DB2 tables incl. the map list, models, textures) works from the CASC storage; files overridden by the project's own folder take priority.

Current limitation: maps (ADT tiles) and their models render fine for 3.3.5a; opening/saving map tiles of the modern split-ADT format is not implemented — saving is disabled for Shadowlands projects so no client data is corrupted. All editor tooling (brushes, books/palettes, DB editors) works on data that reads successfully.
