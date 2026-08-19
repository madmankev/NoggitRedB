# Noggit RED — Major Update Work Plan (Arena session)

Two tracks executed in parallel:

## Track A — Infrastructure & 9.1.5.x (modern client) support
1. Vendor `blizzard-archive-library` (CASC/MPQ/Directory archives, listfile, FileDataID keys)
   into `src/external/blizzard-archive-library` (gitlab submodules unreachable from CI-less
   environments; vendored = always buildable).
2. Vendor `blizzard-database-library` (DBC + DB2 WDC3 readers, WoWDBDefs definitions)
   into `src/external/blizzard-database-library`.
3. Vendor required `cmake/` helper scripts (build-dependencies: cmake_function, cmake_macro,
   FetchContentFast, revision scripts, Find modules).
4. Modern client (9.1.5x) functionality on top:
   - client version / game path plumbing (project + settings UI)
   - FileDataID-aware asset pipeline verification (listfile bootstrap, no listfile => graceful degradation)
   - modern ADT handling audit (split ADTs, MH2O etc.)

## Track B — Fix open GitLab work items (41)
### Bugs
- [ ] #1  Undo stack corrupts (stamp) — harden ActionManager
- [ ] #3  Asset browser / object palette MIME format
- [ ] #4  World loaded multiple times in map selector
- [ ] #9  Normal map export wrong resolution
- [ ] #10 Vertex paint image mask colors wrong
- [ ] #16 Gizmo not updating properly / colors
- [ ] #17 Multi-selection editing ignores selection center
- [ ] #23 "Mouse move follow cursor" swaps axes
- [ ] #24 Pressure slider wheel sensitivity
- [ ] #25 Flatten/Blur lock-mode square not transparent
- [ ] #26 Custom stamp terrain flicker
- [ ] #27 ModelRender _transparency_lookup out of range
- [ ] #28 Light flicker in map corner
- [ ] #29 VertexPainter image mask alpha black
- [ ] #32 Some downported M2 do not render
- [ ] #33 Shading doesn't update after heightmap import
- [ ] #35 Massive useless memory allocation
- [ ] #43 Object rotation speed framerate dependent
- [ ] #47 Invalid ADT offsets => tile not selectable
- [ ] #53 QImage rescale broken (tiled edges import)
- [ ] #55 Saving erases undo history
- [ ] #57 md5translate.trs formatting for X < 10
- [ ] #60 Rotation gizmo pivot inaccuracy
- [ ] #64 Can't remove sound entry rows
- [ ] #65 Waterfall/animated textures show red placeholder

### Features
- [ ] #2  Render batch sorting
- [ ] #7  Optional runtime asset test coverage
- [ ] #30 Raise&Lower min/max blending
- [ ] #34 Standard rendering & tile loading option
- [ ] #36 Object palette save/load
- [ ] #37 Copy asset path in Object Editor
- [ ] #44 Patch client with project folder
- [ ] #46 Camera collision
- [ ] #50 Error panel/console
- [ ] #51 Game mode
- [ ] #52 WorldSafeLocs editor
- [ ] #54 Render missing models as error cubes
- [ ] #56 Update key bindings help
- [ ] #61 Missing rendering features
- [ ] #62 Minimap liquid rendering
- [ ] #63 Shader hot reloading

Status legend: [ ] todo · [x] done · [~] partial
