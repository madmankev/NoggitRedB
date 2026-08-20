# Modern (split) ADT support — Battle for Azeroth 8.1+ / Shadowlands 9.1.5x

This document describes how Noggit RED reads and writes the modern World of
Warcraft map tile format used by Battle for Azeroth (8.1+) and Shadowlands
(9.1.5x) clients, the design decisions behind the implementation, and the
known limitations.

References: <https://wowdev.wiki/ADT> (split files / v18 sections),
<https://wowdev.wiki/ADT/v18>, <https://wowdev.wiki/ADT/ADTLodImplementation>.

## The split files

Unlike the single-file format of pre-Cataclysm clients, a modern tile is a
set of files next to each other in `World/Maps/<map>/`:

| file                 | role |
| -------------------- | ---- |
| `<map>_<x>_<y>.adt`      | **root**: MVER, MHDR, MFBO (flight bounds), MH2O (liquids), terrain geometry MCNKs, optional blend meshes and other chunks |
| `<map>_<x>_<y>_tex0.adt` | **tex**: MDID texture FileDataID table (+ MHID/MTXF/MTXP/MTCG/MAMP), one header-less MCNK per chunk with MCLY/MCSH/MCAL/MCMT |
| `<map>_<x>_<y>_obj0.adt` | **obj**: MMDX/MMID, MWMO/MWID, MDDF/MODF placements, MWDR/MWDS, one header-less MCNK per chunk with MCRD/MCRW references |
| `<map>_<x>_<y>_lod.adt`  | far-view LOD level data (Legion+). Not read or written by this implementation, see limitations |
| `<map>_<x>_<y>_obj1.adt` | high-LOD object data. Parsed implicitly never; left untouched on disk |

There is **no MCIN** offset table in modern files. Chunks appear in file
order (chunk 0 = MCNK at grid position 0/0 … chunk 255 = 15/15) and the
parsers of all three files match them by **index**, not by offset. The MCNKs
in the tex and obj files have **no 0x80 header**: their fourcc and size are
immediately followed by sub-chunks.

## Reading (MapTile::finishLoadingModern)

All three files are opened independently; a missing or corrupt `_tex0` /
`_obj0` file downgrades gracefully (the geometry still loads). Each file is
walked sequentially with a chunk-frame bounds check, corrupt trailing data
only aborts the scan instead of the tile.

* **Root file**: MHDR is kept verbatim (`MapTile::_original_mhdr`) so fields
  the editor doesn't model (the MAMP byte, padding) survive a round trip.
  MFBO and MH2O reuse the legacy readers. Unknown tile-level chunks (the
  blend mesh chunks MBMH/MBBB/MBNV/MBMI and anything else) are **preserved
  verbatim** as raw byte blobs and re-emitted on save.
* **Tex file**: the MDID table of terrain texture FileDataIDs is resolved to
  paths through the project listfile:
  * resolvable fdid → its path (normalized),
  * unresolvable fdid → a stable placeholder name `tileset/fdid_<id>.blp`
    the id is recovered from on save (surviving even without a listfile),
  * fdid 0 → a custom texture, its path is restored from the editor sidecar
    (see below).

  The aligned metadata arrays MHID (height texture fdids), MTXF (per texture
  flags), MTXP (height scale/offset) and MTCG (color grading) as well as
  MAMP are collected per texture path so they realign themselves when the
  texture table changes between saves.
* **Obj file**: MMDX/MWMO name tables (kept in load order), the MMID/MWID
  FileDataID companions, and the MDDF/MODF placements. Modern flags are
  honored:
  * MDDF flag `0x40` / MODF flag `0x8`: `nameID` **is** a FileDataID,
  * MODF flag `0x4`: the per-entry scale is `scale / 1024` (the entry's
    flags are remembered per instance so they round trip),
  * the name tables keep their order for writing (see below),
  * MWDR/MWDS and unknown obj chunks are preserved verbatim.

  The MCRD/MCRW per-chunk model reference chunks are skipped — the client
  uses them as a culling acceleration, the editor draws everything; they are
  regenerated on save.

### Chunks (MapChunk::readModernSplit)

The root MCNKs keep the classic 0x80 header. Since ~5.3 the header fields
`ofsHeight`/`ofsNormal` overlap the 8-byte high-res (8x8) hole map, so the
sub-chunks are **located by scanning** from the MCNK end, never through the
header offsets; only MCVT/MCNR/MCCV/MCSE/MCDD are read, MCLV (baked vertex
lighting, post-Cataclysm) is skipped.

* High-res holes (flag `high_res_holes`) are folded down to the 4x4
  low-res hole map the editor works with (a quadrant is a hole when any of
  its four high-res cells is a hole). On save the classic 4x4 map is
  written (`high_res_holes` cleared) — this is a deliberate, documented
  resolution downgrade.
* Texture layers: the tex MCNK of the same chunk index is located, and a
  **synthetic `MapChunkHeader`** is built that points `ofsLayer`/`ofsAlpha`
  into that file, so the existing `TextureSet` machinery (which already
  understands the 4096-byte big-alpha mode and Blizzard's RLE compressed
  MCAL) can be reused unchanged. The modern writer always emits the 8-bit
  4096 byte alphamap mode, so reading forces it as well.
* MCSH shadow data lives in the tex file in the split format (the
  `has_mcsh` flag stays in the root MCNK header). The 0x200 bit-packed map
  is decoded exactly like the legacy path.
* MCMT (terrain material ids) and MCDD (detail doodad disable map) are kept
  per chunk and re-emitted verbatim.

## Writing (MapTile::saveModernADT + MapChunk::saveModern)

`MapTile::saveTile` routes Shadowlands projects to the modern writer, legacy
projects keep using the old single-file writer. The gather phase (models on
the tile, texture table, WDT size-class pre-sort) is shared with the legacy
writer.

* **Model tables are stable**: MMDX/MWMO first contain all entries the
  loaded file had (in that order), so indices referenced by preserved raw
  chunks (e.g. blend meshes referencing WMO table entries) stay valid; new
  models are appended. The MMID/MWID FileDataID companions are rebuilt from
  the listfile (0 when a path can't be resolved — loose-file custom models).
* **Placements**: path-referenced models write `nameID` = table index, fdid-
  only models write `nameID` = FileDataID with flags `0x40`/`0x8`. WMO
  entries always get the `0x4` scale flag (the editor always writes a
  scale), the remaining MODF flags (`use_lod`, nameset, doodadset, ...)
  and the MDDF flags round trip unchanged.
* **Textures**: MDID is rebuilt in alphabetical path order (the texture set
  of every chunk resolves through the table written in the same pass), a
  path with no FileDataID yields a 0 entry — see the custom texture
  paragraph below. MHID/MTXF/MTXP/MTCG/MAMP are only written when the loaded
  file had them and stay aligned with the (possibly re-ordered) texture
  table.
* **Chunks** write their part into all three files in one pass:
  * root: regular 0x80 MCNK header (legacy-compatible offsets re-computed,
    high-res hole flag cleared, 4x4 hole map) + MCVT/MCCV/MCNR (448 bytes,
    the Cata+ variant with the 13 padding bytes **inside** the chunk),
    MCCV/MCSE/preserved MCDD,
  * tex: header-less MCNK with MCLY (flags fixed up for the uncompressed
    8-bit alpha mode), MCSH iff the chunk has shadows (`has_mcsh` in the
    root header follows), a single MCAL with all layer maps uncompressed,
    preserved MCMT,
  * obj: header-less MCNK with MCRD/MCRW computed via the same rectangle
    overlap test the legacy MCRF used.

All three files, and the sidecar when needed, are written into the project
directory through the regular `ClientFile` mechanism (project files shadow
CASC content on read, so the files loaded from CASC are overridden by saved
ones in the project folder — same behavior as legacy MPQ projects).

## Custom content and FileDataIDs

A modern client loads *everything* by FileDataID. That has two visible
consequences:

* **Custom models/WMOS (loose files, not in CASC)**: they are written into
  MMDX/MWMO with a 0 fdid in MMID/MWID. The patched 3.3.5-era assumption
  "file on disk exists" doesn't hold for a CASC-only client — the retail
  client will not see those models until they exist in its storage.
  Editor-side they keep working (the editor loads loose files first).
* **Custom terrain textures**: same story, with the addition that MDID 0 is
  unusable by the client. Noggit persists the texture paths of all fdid-0
  MDID entries per tile in a JSON sidecar
  `World/Maps/<map>/<map>_<x>_<y>.noggit-tex.json`:

  ```json
  {
    "textures": ["TILESET\\MyPatch\\moss_custom.blp", "", ...]
  }
  ```

  (array aligned to the MDID table, entries for resolvable textures stay
  empty). On load the sidecar restores those slots so a save/reload cycle in
  the editor never loses custom texturing. The file is only written when at
  least one unresolvable texture exists.

## Known limitations (honest list)

* **Far-view LOD is not regenerated.** `_lod.adt` (Legion) and `_obj1.adt`
  are not read or written. After editing, a tile's far view in the client
  still shows the *old* silhouette until the LOD is regenerated with an
  external tool (see the `ADTLodImplementation` article on wowdev.wiki).
  Stale LOD files are harmless to the editor itself. Consider deleting the
  stale siblings alongside the saved tile if you can't regenerate them.
* **High-res (8x8) holes are folded to 4x4** on load and written back as
  4x4 (`high_res_holes` cleared). Blizzard maps hardly use high-res holes;
  if you need them untouched, don't save those tiles yet.
* **MCLV (baked vertex light) is dropped** on save. Noggit recomputes
  vertex colors (MCCV) for lighting anyway.
* **Liquids (MH2O) round trip through the existing writer.** Per-chunk
  liquid edits work as before; there is no editing UI for the modern-only
  MH2O additions.
* **`uid_fix_all`-mode (no textures) re-saves drop tex-file metadata**,
  because the metadata is keyed per texture and no texture table exists in
  that mode. The same tradeoff already existed for MTXF in the legacy
  writer. Normal map-editing saves preserve everything.
* Preserved verbatim chunks (blend meshes MBMH/MBBB/MBNV/MBMI, MWDR/MWDS)
  can reference the model name tables. Table order is kept stable precisely
  for this, but if you *remove* models the preserved data may point one
  entry off — in practice Blizzard's own blend meshes pair with removed WMOs
  rarely; this is considered acceptable.

## Where the code lives

| piece | location |
| ----- | -------- |
| format switch | `MapTile::isModernFormat()` (Shadowlands project version) |
| reading | `MapTile::finishLoadingModern`, `MapChunk::readModernSplit` |
| writing | `MapTile::saveModernADT`, `MapChunk::saveModern` (routed by `MapTile::saveTile`) |
| texture sidecar | `MapTile::loadModernTextureSidecar` / `saveModernTextureSidecar` |
| per-instance flag fidelity | `ModelInstance::mddf_flags`, `WMOInstance` scale flag (0x4) handling |
| format structs | `src/noggit/MapHeaders.h` (MHDR, ENTRY_MDDF/ENTRY_MODF, MCNK header, MCLY flags) |
