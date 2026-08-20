// This file is part of Noggit3, licensed under GNU General Public License (version 3).
#include <ClientFile.hpp>
#include <external/tracy/Tracy.hpp>
#include <math/frustum.hpp>
#include <noggit/Action.hpp>
#include <noggit/ActionManager.hpp>
#include <noggit/Alphamap.hpp>
#include <noggit/Brush.h>
#include <noggit/ChunkWater.hpp>
#include <noggit/Log.h>
#include <noggit/MapChunk.h>
#include <noggit/MapHeaders.h>
#include <noggit/MapTile.h> // MapTile
#include <noggit/Misc.h>
#include <noggit/ModelInstance.h>
#include <noggit/texture_set.hpp>
#include <noggit/TileWater.hpp>
#include <noggit/tool_enums.hpp>
#include <noggit/WMOInstance.h>
#include <noggit/World.h>
#include <util/sExtendableArray.hpp>

#include <algorithm>
#include <list>
#include <cstring>
#include <string>
#include <cstdint>
#include <cmath>
#include <limits>
#include <map>
#include <QImage>

MapChunk::MapChunk(MapTile* maintile, BlizzardArchive::ClientFile* f, bool bigAlpha,tile_mode mode
                    , Noggit::NoggitRenderContext context, bool init_empty, int chunk_idx, bool load_textures
                    , BlizzardArchive::ClientFile* modern_tex_file, size_t modern_tex_chunk_offset)
  : _mode(mode)
  , mt(maintile)
  , use_big_alphamap(bigAlpha)
  , _context(context)
  , vmin(std::numeric_limits<float>::max())
  , vmax(std::numeric_limits<float>::lowest())
  , _chunk_update_flags(ChunkUpdateFlags::VERTEX | ChunkUpdateFlags::ALPHAMAP
                        | ChunkUpdateFlags::SHADOW | ChunkUpdateFlags::MCCV
                        | ChunkUpdateFlags::NORMALS| ChunkUpdateFlags::HOLES
                        | ChunkUpdateFlags::AREA_ID| ChunkUpdateFlags::FLAGS
                        | ChunkUpdateFlags::GROUND_EFFECT | ChunkUpdateFlags::DETAILDOODADS_EXCLUSION)
{


  if (init_empty)
  {

    //  header.flags = 0;
    px = chunk_idx / 16;
    py = chunk_idx % 16;

    zbase = ZEROPOINT - (maintile->zbase + py * CHUNKSIZE);
    xbase = ZEROPOINT - (maintile->xbase + px * CHUNKSIZE);
    ybase = 0.0f;

    areaID = 0;

    // zbase = header.zpos;
    // xbase = header.xpos;
    // ybase = header.ypos;

    texture_set.reset();

    auto& tile_buffer = mt->getChunkHeightmapBuffer();
    int chunk_start = (px * 16 + py) * mapbufsize * 4;

    // Generate normals
    for (int i = 0; i < mapbufsize; ++i)
    {
      int pixel_start = chunk_start + i * 4;
      tile_buffer[pixel_start] = 0.0f;
      tile_buffer[pixel_start + 1] = 1.0f;
      tile_buffer[pixel_start + 2] = 0.0f;
    }

    // Clear shadows
    memset(_shadow_map, 0, 64 * 64);

    // Do not write MCCV
    hasMCCV = false;

    holes = 0;

    zbase = zbase*-1.0f + ZEROPOINT;
    xbase = xbase*-1.0f + ZEROPOINT;

    glm::vec3 *ttv = mVertices;

    for (int j = 0; j < 17; ++j) {
      for (int i = 0; i < ((j % 2) ? 8 : 9); ++i) {
        float xpos, zpos;
        xpos = i * UNITSIZE;
        zpos = j * 0.5f * UNITSIZE;
        if (j % 2) {
          xpos += UNITSIZE*0.5f;
        }
        glm::vec3 v = glm::vec3(xbase + xpos, ybase + 0.0f, zbase + zpos);
        *ttv++ = v;
        vmin.y = std::min(vmin.y, v.y);
        vmax.y = std::max(vmax.y, v.y);
      }
    }

    vmin.x = xbase;
    vmin.z = zbase;
    vmax.x = xbase + 8 * UNITSIZE;
    vmax.z = zbase + 8 * UNITSIZE;

    update_intersect_points();

    // use absolute y pos in vertices
    ybase = 0.0f;

    vcenter = (vmin + vmax) * 0.5f;

    return;
  }


  uint32_t fourcc;
  uint32_t size;

  size_t base = f->getPos();

  hasMCCV = false;

  MapChunkHeader tmp_chunk_header;

  // - MCNK ----------------------------------------------
  {
    f->read(&fourcc, 4);
    f->read(&size, 4);

    assert(fourcc == 'MCNK');



    f->read(&tmp_chunk_header, sizeof(MapChunkHeader));

    header_flags.value = tmp_chunk_header.flags.value;
    areaID = tmp_chunk_header.areaid;

    zbase = tmp_chunk_header.zpos;
    xbase = tmp_chunk_header.xpos;
    ybase = tmp_chunk_header.ypos;

    px = tmp_chunk_header.ix;
    py = tmp_chunk_header.iy;

    holes = tmp_chunk_header.holes;

    // correct the x and z values ^_^
    zbase = zbase*-1.0f + ZEROPOINT;
    xbase = xbase*-1.0f + ZEROPOINT;

    // Fix for issue #47: validate the file-stored chunk coordinates against
    // the position implied by the tile index and the chunk's index inside
    // the file. ADTs that were moved or renamed keep stale offsets, and the
    // cursor raycast (which uses the heightmap data) then disagrees with the
    // rendering, making the tile unselectable.
    {
      int const expected_px = chunk_idx / 16;
      int const expected_py = chunk_idx % 16;
      float const expected_xbase = maintile->xbase + expected_px * CHUNKSIZE;
      float const expected_zbase = maintile->zbase + expected_py * CHUNKSIZE;

      if (px != expected_px || py != expected_py
          || std::fabs(xbase - expected_xbase) > 0.5f
          || std::fabs(zbase - expected_zbase) > 0.5f)
      {
        LogDebug << "Chunk " << px << "_" << py << " of tile " << maintile->index.x << "_"
                 << maintile->index.z << " had invalid stored coordinates ("
                 << xbase << ", " << zbase << "), expected (" << expected_xbase
                 << ", " << expected_zbase << "). Fixing them." << std::endl;

        px = expected_px;
        py = expected_py;
        xbase = expected_xbase;
        zbase = expected_zbase;
      }
    }

  }

  // 9.1.5x (Shadowlands): modern split ADT. The geometry (MCVT/MCNR/MCCV/MCSE)
  // comes from the root file's MCNKs (parsed by scanning the sub-chunks, the
  // header offsets can't be trusted there: high res holes share the offset
  // fields), textures/layers (MCLY/MCAL), shadows (MCSH) and materials (MCMT)
  // come from the header-less MCNKs of the _tex0 sibling file. Models are
  // placed from MDDF/MODF of the _obj0 sibling on tile level.
  // See docs/MODERN_ADT.md.
  if (maintile->isModernFormat())
  {
    readModernSplit(f, base, tmp_chunk_header, chunk_idx
      , modern_tex_file, modern_tex_chunk_offset, load_textures);
    return;
  }

  if (!load_textures)
  {
      tmp_chunk_header.nLayers = 0;
  }

  texture_set = std::make_unique<TextureSet>(this, f, base, bigAlpha,
     !!header_flags.flags.do_not_fix_alpha_map, mode == tile_mode::uid_fix_all, _context, tmp_chunk_header);

  // - MCVT ----------------------------------------------
  {
    f->seek(base + tmp_chunk_header.ofsHeight);
    f->read(&fourcc, 4);
    f->read(&size, 4);

    assert(fourcc == 'MCVT');

    glm::vec3 *ttv = mVertices;

    // vertices
    for (int j = 0; j < 17; ++j) {
      for (int i = 0; i < ((j % 2) ? 8 : 9); ++i) {
        float h, xpos, zpos;
        f->read(&h, 4);
        xpos = i * UNITSIZE;
        zpos = j * 0.5f * UNITSIZE;
        if (j % 2) {
          xpos += UNITSIZE*0.5f;
        }
        glm::vec3 v = glm::vec3(xbase + xpos, ybase + h, zbase + zpos);
        *ttv++ = v;
        vmin.y = std::min(vmin.y, v.y);
        vmax.y = std::max(vmax.y, v.y);
      }
    }

    vmin.x = xbase;
    vmin.z = zbase;
    vmax.x = xbase + 8 * UNITSIZE;
    vmax.z = zbase + 8 * UNITSIZE;

    update_intersect_points();

    // use absolute y pos in vertices
    ybase = 0.0f;
    tmp_chunk_header.ypos = 0.0f;
  }
  // - MCNR ----------------------------------------------
  {
    f->seek(base + tmp_chunk_header.ofsNormal);
    f->read(&fourcc, 4);
    f->read(&size, 4);

    assert(fourcc == 'MCNR');

    auto& tile_buffer = mt->getChunkHeightmapBuffer();
    int chunk_start = (px * 16 + py) * mapbufsize * 4;

    char nor[3];
    for (int i = 0; i < mapbufsize; ++i)
    {
      f->read(nor, 3);
      int pixel_start = chunk_start + i * 4;
      tile_buffer[pixel_start] = nor[0] / 127.0f;
      tile_buffer[pixel_start + 1] = nor[2] / 127.0f;
      tile_buffer[pixel_start + 2] = nor[1] / 127.0f;
    }
  }
  // - MCSH ----------------------------------------------
  if((header_flags.flags.has_mcsh) && tmp_chunk_header.ofsShadow && tmp_chunk_header.sizeShadow)
  {
    f->seek(base + tmp_chunk_header.ofsShadow);
    f->read(&fourcc, 4);
    f->read(&size, 4);

    assert(fourcc == 'MCSH');

    char compressed_shadow_map[64 * 64 / 8];

    // shadow map 64 x 64
    f->read(&compressed_shadow_map, 0x200);
    f->seekRelative(-0x200);

    uint8_t *p;
    char *c;
    p = _shadow_map;
    c = compressed_shadow_map;
    for (int i = 0; i<64 * 8; ++i)
    {
      for (int b = 0x01; b != 0x100; b <<= 1)
      {
        *p++ = ((*c) & b) ? 85 : 0;
      }
      c++;
    }

    if (!header_flags.flags.do_not_fix_alpha_map)
    {
      for (std::size_t i(0); i < 64; ++i)
      {
        _shadow_map[i * 64 + 63] = _shadow_map[i * 64 + 62];
        _shadow_map[63 * 64 + i] = _shadow_map[62 * 64 + i];
      }
      _shadow_map[63 * 64 + 63] = _shadow_map[62 * 64 + 62];
    }
  }
  else
  {
    /** We have no shadow map (MCSH), so we got no shadows at all!  **
    ** This results in everything being black.. Yay. Lets fake it! **/
    memset(_shadow_map, 0, 64 * 64);
  }
  // - MCCV ----------------------------------------------
  if(tmp_chunk_header.ofsMCCV)
  {
    f->seek(base + tmp_chunk_header.ofsMCCV);
    f->read(&fourcc, 4);
    f->read(&size, 4);

    assert(fourcc == 'MCCV');

    if (!(header_flags.flags.has_mccv))
    {
      header_flags.flags.has_mccv = 1;
    }

    hasMCCV = true;

    unsigned char t[4];
    for (int i = 0; i < mapbufsize; ++i)
    {
      f->read(t, 4);
      mccv[i] = glm::vec3((float)t[2] / 127.0f, (float)t[1] / 127.0f, (float)t[0] / 127.0f);
    }
  }
  else
  {
    glm::vec3 mccv_default(1.f, 1.f, 1.f);
    for (int i = 0; i < mapbufsize; ++i)
    {
      mccv[i] = mccv_default;
    }
  }

  if (tmp_chunk_header.sizeLiquid > 8)
  {
    f->seek(base + tmp_chunk_header.ofsLiquid);

    f->read(&fourcc, 4);
    f->seekRelative(4); // ignore the size here, the valid size is in the header

    assert(fourcc == 'MCLQ');

    int layer_count = (tmp_chunk_header.sizeLiquid - 8) / sizeof(mclq);
    std::vector<mclq> layers(layer_count);
    f->read(layers.data(), sizeof(mclq)*layer_count);

    mt->Water.getChunk(px, py)->from_mclq(layers);
    // remove the liquid flags as it'll be saved as MH2O
    header_flags.value &= ~(0xF << 2);
  }

  vcenter = (vmin + vmax) * 0.5f;

  if (tmp_chunk_header.nSndEmitters)
  {
      f->seek(base + tmp_chunk_header.ofsSndEmitters);
      f->read(&fourcc, 4);
      f->read(&size, 4);

      assert(fourcc == 'MCSE');

      for (unsigned int i = 0; i < tmp_chunk_header.nSndEmitters; i++)
      {
          ENTRY_MCSE sound_emitter;
          f->read(&sound_emitter, sizeof(ENTRY_MCSE));

          float pos_x = sound_emitter.pos[1] * -1.0f + ZEROPOINT;
          float pos_y = sound_emitter.pos[2];
          float pos_z = sound_emitter.pos[0] * -1.0f + ZEROPOINT;

          sound_emitter.pos[0] = pos_x;
          sound_emitter.pos[1] = pos_y;
          sound_emitter.pos[2] = pos_z;

          sound_emitters.emplace_back(sound_emitter);
      }
  }
}

uint16_t MapChunk::foldHighResHoles(uint64_t holes_high_res)
{
  // 9.1.5x: the 8x8 high res hole map (1 bit per cell, (Holes[row] >> col) & 1)
  // is folded down to Noggit's 4x4 low res hole map used for editing: any of the
  // four high res cells of a quadrant being a hole makes the quadrant a hole.
  uint16_t low_res = 0;

  for (int row = 0; row < 4; ++row)
  {
    for (int col = 0; col < 4; ++col)
    {
      bool any_hole = false;

      for (int dy = 0; dy < 2 && !any_hole; ++dy)
      {
        for (int dx = 0; dx < 2 && !any_hole; ++dx)
        {
          any_hole = (holes_high_res >> ((row * 2 + dy) * 8 + (col * 2 + dx))) & 1ULL;
        }
      }

      if (any_hole)
      {
        low_res |= static_cast<uint16_t>(1u << (row * 4 + col));
      }
    }
  }

  return low_res;
}

void MapChunk::readModernSplit(BlizzardArchive::ClientFile* f, size_t base, MapChunkHeader& header
  , int /*chunk_idx*/, BlizzardArchive::ClientFile* tex_file, size_t tex_chunk_offset, bool load_textures)
{
  // 9.1.5x (Shadowlands) modern split ADT reading, see docs/MODERN_ADT.md.
  //
  // root file MCNK: has the regular 0x80 header but the sub-chunks are located
  // by scanning (offsets in the header share bytes with the high res hole map).
  uint32_t fourcc;
  uint32_t size;
  uint32_t mcnk_size;

  hasMCCV = false;
  memset(_shadow_map, 0, 64 * 64);

  // holes: modern files can carry a 8x8 (64 bit) hole map, fold it down to
  // Noggit's 4x4 (16 bit) editor resolution. The MCNK flag and the hole data
  // slot (0x14) replace the ofsHeight/ofsNormal header fields.
  if (header_flags.flags.high_res_holes)
  {
    uint64_t holes_high_res = 0;
    f->seek(base + 8 + 0x14);
    f->read(&holes_high_res, 8);
    holes = foldHighResHoles(holes_high_res);
  }
  else
  {
    holes &= 0xFFFF;
  }

  f->seek(base + 4);
  f->read(&mcnk_size, 4);

  size_t const sub_begin = base + 8 + 0x80;
  size_t const sub_end = base + 8 + mcnk_size;

  bool got_heights = false;

  for (size_t pos = sub_begin; pos + 8 <= sub_end; )
  {
    f->seek(pos);
    f->read(&fourcc, 4);
    f->read(&size, 4);

    // corrupt data guard: stop scanning when a bogus size is found
    if (pos + 8 + size > sub_end)
    {
      break;
    }

    switch (fourcc)
    {
    case 'MCVT':
      {
        glm::vec3* ttv = mVertices;

        for (int j = 0; j < 17; ++j)
        {
          for (int i = 0; i < ((j % 2) ? 8 : 9); ++i)
          {
            float h;
            f->read(&h, 4);

            float xpos = i * UNITSIZE;
            float zpos = j * 0.5f * UNITSIZE;
            if (j % 2)
            {
              xpos += UNITSIZE * 0.5f;
            }
            glm::vec3 v = glm::vec3(xbase + xpos, ybase + h, zbase + zpos);
            *ttv++ = v;
            vmin.y = std::min(vmin.y, v.y);
            vmax.y = std::max(vmax.y, v.y);
          }
        }
        got_heights = true;
      }
      break;

    case 'MCNR':
      {
        auto& tile_buffer = mt->getChunkHeightmapBuffer();
        int chunk_start = (px * 16 + py) * mapbufsize * 4;

        char nor[3];
        for (int i = 0; i < mapbufsize; ++i)
        {
          f->read(nor, 3);
          int pixel_start = chunk_start + i * 4;
          tile_buffer[pixel_start] = nor[0] / 127.0f;
          tile_buffer[pixel_start + 1] = nor[2] / 127.0f;
          tile_buffer[pixel_start + 2] = nor[1] / 127.0f;
        }
      }
      break;

    case 'MCCV':
      header_flags.flags.has_mccv = 1;
      hasMCCV = true;
      {
        unsigned char t[4];
        for (int i = 0; i < mapbufsize; ++i)
        {
          f->read(t, 4);
          mccv[i] = glm::vec3((float)t[2] / 127.0f, (float)t[1] / 127.0f, (float)t[0] / 127.0f);
        }
      }
      break;

    case 'MCSE':
      {
        int const emitter_count = static_cast<int>(size) / 0x1C;

        for (int i = 0; i < emitter_count; i++)
        {
          ENTRY_MCSE sound_emitter;
          f->read(&sound_emitter, sizeof(ENTRY_MCSE));

          float pos_x = sound_emitter.pos[1] * -1.0f + ZEROPOINT;
          float pos_y = sound_emitter.pos[2];
          float pos_z = sound_emitter.pos[0] * -1.0f + ZEROPOINT;

          sound_emitter.pos[0] = pos_x;
          sound_emitter.pos[1] = pos_y;
          sound_emitter.pos[2] = pos_z;

          sound_emitters.emplace_back(sound_emitter);
        }
      }
      break;

    case 'MCDD':
      // detail doodad disable map, preserved verbatim
      if (size >= 8)
      {
        f->read(_modern_preserved.mcdd.data(), 8);
        _modern_preserved.has_mcdd = true;
      }
      break;

    default:
      // MCLV (baked vertex lighting, post Cataclysm) and everything else is
      // skipped; MCLV is not preserved on save (documented limitation)
      break;
    }

    pos += 8 + size;
  }

  if (!got_heights)
  {
    // no heights found (should never happen in valid files): flat fallback
    glm::vec3* ttv = mVertices;
    for (int j = 0; j < 17; ++j)
    {
      for (int i = 0; i < ((j % 2) ? 8 : 9); ++i)
      {
        *ttv++ = glm::vec3(xbase + i * UNITSIZE + ((j % 2) ? UNITSIZE * 0.5f : 0.0f)
          , ybase, zbase + j * 0.5f * UNITSIZE);
      }
    }
  }

  vmin.x = xbase;
  vmin.z = zbase;
  vmax.x = xbase + 8 * UNITSIZE;
  vmax.z = zbase + 8 * UNITSIZE;

  update_intersect_points();

  vcenter = (vmin + vmax) * 0.5f;

  if (!hasMCCV)
  {
    glm::vec3 const mccv_default(1.f, 1.f, 1.f);
    for (int i = 0; i < mapbufsize; ++i)
    {
      mccv[i] = mccv_default;
    }
  }

  // ------- _tex0 file: texture layers, alphamaps, shadows, materials -------

  size_t mcly_chunk_pos = 0;   // position of the MCLY chunk header
  size_t mcly_size = 0;
  size_t mcal_chunk_pos = 0;

  if (tex_file)
  {
    tex_file->seek(tex_chunk_offset);
    tex_file->read(&fourcc, 4);
    tex_file->read(&mcnk_size, 4);

    if (fourcc != 'MCNK')
    {
      LogError << "MapChunk::readModernSplit: expected MCNK at offset " << tex_chunk_offset
               << " of the _tex0 file (found \"" << std::string(reinterpret_cast<char const*>(&fourcc), 4)
               << "\" instead)" << std::endl;
      tex_file = nullptr;
    }
  }

  if (tex_file)
  {
    // the tex MCNKs have NO 0x80 header, sub-chunks start right after the chunk header
    size_t const tex_sub_begin = tex_chunk_offset + 8;
    size_t const tex_sub_end = tex_chunk_offset + 8 + mcnk_size;

    bool has_mcsh_chunk = false;
    size_t mcsh_data_pos = 0;

    for (size_t pos = tex_sub_begin; pos + 8 <= tex_sub_end; )
    {
      tex_file->seek(pos);
      tex_file->read(&fourcc, 4);
      tex_file->read(&size, 4);

      if (pos + 8 + size > tex_sub_end)
      {
        break;
      }

      switch (fourcc)
      {
      case 'MCLY':
        mcly_chunk_pos = pos;
        mcly_size = size;
        break;

      case 'MCAL':
        mcal_chunk_pos = pos;
        break;

      case 'MCSH':
        has_mcsh_chunk = true;
        mcsh_data_pos = pos + 8;
        break;

      case 'MCMT':
        // terrain material ids per layer, preserved verbatim
        if (size >= 4)
        {
          tex_file->read(_modern_preserved.mcmt.data(), 4);
          _modern_preserved.has_mcmt = true;
        }
        break;

      default:
        break;
      }

      pos += 8 + size;
    }

    // shadows (they live in the _tex0 file in the split format)
    if (has_mcsh_chunk && header_flags.flags.has_mcsh)
    {
      char compressed_shadow_map[64 * 64 / 8];

      tex_file->seek(mcsh_data_pos);
      tex_file->read(&compressed_shadow_map, 0x200);

      uint8_t* p = _shadow_map;
      char* c = compressed_shadow_map;
      for (int i = 0; i < 64 * 8; ++i)
      {
        for (int b = 0x01; b != 0x100; b <<= 1)
        {
          *p++ = ((*c) & b) ? 85 : 0;
        }
        c++;
      }

      if (!header_flags.flags.do_not_fix_alpha_map)
      {
        for (std::size_t i(0); i < 64; ++i)
        {
          _shadow_map[i * 64 + 63] = _shadow_map[i * 64 + 62];
          _shadow_map[63 * 64 + i] = _shadow_map[62 * 64 + i];
        }
        _shadow_map[63 * 64 + 63] = _shadow_map[62 * 64 + 62];
      }
    }
  }

  // hand the texture machinery a synthetic header pointing into the _tex0
  // file: the offsets are relative to the tex MCNK of this chunk, matching
  // the semantics TextureSet expects from the legacy single-file layout.
  uint32_t n_layers = 0;

  if (tex_file && load_textures && mcly_chunk_pos && mcly_size)
  {
    n_layers = std::min<uint32_t>(mcly_size / sizeof(ENTRY_MCLY), 4u);

    // validate the layer texture ids before handing them to the texture set,
    // corrupt/out of range ids otherwise read past the texture filename table
    tex_file->seek(mcly_chunk_pos + 8);

    for (uint32_t i = 0; i < n_layers; ++i)
    {
      ENTRY_MCLY entry;
      tex_file->read(&entry, sizeof(ENTRY_MCLY));

      if (entry.textureID >= mt->mTextureFilenames.size())
      {
        LogError << "MapChunk::readModernSplit: out of range textureID " << entry.textureID
                 << " in layer " << i << " (" << mt->mTextureFilenames.size()
                 << " textures), dropping the remaining layers" << std::endl;
        n_layers = i;
        break;
      }
    }

    if (n_layers > 1 && !mcal_chunk_pos)
    {
      LogError << "MapChunk::readModernSplit: " << n_layers << " layers but no MCAL chunk "
               << "in the _tex0 file, dropping the additional layers" << std::endl;
      n_layers = 1;
    }
  }

  MapChunkHeader tex_chunk_header = {};
  tex_chunk_header.flags.value = header_flags.value;
  tex_chunk_header.nLayers = n_layers;

  if (mcly_chunk_pos)
  {
    tex_chunk_header.ofsLayer = static_cast<uint32_t>(mcly_chunk_pos - tex_chunk_offset);
  }

  if (mcal_chunk_pos)
  {
    tex_chunk_header.ofsAlpha = static_cast<uint32_t>(mcal_chunk_pos - tex_chunk_offset);
  }

  std::copy(header.doodadMapping, header.doodadMapping + 8, tex_chunk_header.doodadMapping);
  std::copy(header.doodadStencil, header.doodadStencil + 8, tex_chunk_header.doodadStencil);

  // modern clients always use the 4096 byte (8 bit, bigalpha) MCAL mode;
  // the Blizzard RLE compression is still applied per layer flag
  use_big_alphamap = true;

  // the texture set is always created (possibly with 0 layers), other code
  // paths rely on it being present
  texture_set = std::make_unique<TextureSet>(this, tex_file ? tex_file : f
    , tex_file ? tex_chunk_offset : base, true
    , !!header_flags.flags.do_not_fix_alpha_map
    , _mode == tile_mode::uid_fix_all, _context, tex_chunk_header);
}

auto MapChunk::getHoleMask(void) const -> unsigned
{
  return static_cast<unsigned>(holes);
}

int MapChunk::indexLoD(int x, int y)
{
  return (x + 1) * 9 + x * 8 + y;
}

int MapChunk::indexNoLoD(int x, int y)
{
  return x * 8 + x * 9 + y;
}

void MapChunk::update_intersect_points()
{
  // update the center of the chunk and visibility when the vertices changed
  vcenter = (vmin + vmax) * 0.5f;
}


std::vector<uint8_t> MapChunk::compressed_shadow_map() const
{
  std::vector<uint8_t> shadow_map(64 * 64 / 8);

  for (int i = 0; i < 64 * 64; ++i)
  {
    if (_shadow_map[i])
    {
      shadow_map[i / 8] |= 1 << i % 8;
    }
  }

  return shadow_map;
}

bool MapChunk::has_shadows() const
{
  for (int i = 0; i < 64 * 64; ++i)
  {
    if (_shadow_map[i])
    {
      return true;
    }
  }

  return false;
}

bool MapChunk::GetVertex(float x, float z, glm::vec3 *V)
{
  float xdiff, zdiff;

  xdiff = x - xbase;
  zdiff = z - zbase;

  const int row = static_cast<int>(zdiff / (UNITSIZE * 0.5f) + 0.5f);
  const int column = static_cast<int>((xdiff - UNITSIZE * 0.5f * (row % 2)) / UNITSIZE + 0.5f);
  if ((row < 0) || (column < 0) || (row > 16) || (column >((row % 2) ? 8 : 9)))
    return false;

  *V = mVertices[17 * (row / 2) + ((row % 2) ? 9 : 0) + column];
  return true;
}

void MapChunk::getVertexInternal(float x, float z, glm::vec3* v)
{
  float xdiff, zdiff;

  xdiff = x - xbase;
  zdiff = z - zbase;

  const int row = static_cast<int>(zdiff / (UNITSIZE * 0.5f) + 0.5f);
  const int column = static_cast<int>((xdiff - UNITSIZE * 0.5f * (row % 2)) / UNITSIZE + 0.5f);

  *v = mVertices[17 * (row / 2) + ((row % 2) ? 9 : 0) + column];
}

float MapChunk::getHeight(int x, int z)
{
  if (x > 9 || z > 9 || x < 0 || z < 0) return 0.0f;
  return mVertices[indexNoLoD(x, z)].y;
}

float MapChunk::getMinHeight() const
{
  return vmin.y;
}

float MapChunk::getMaxHeight() const
{
  return vmax.y;
}

glm::vec3 MapChunk::getCenter() const
{
  return vcenter;
}

void MapChunk::clearHeight()
{
  for (int i = 0; i < mapbufsize; ++i)
  {
    mVertices[i].y = 0.0f;
  }

  vmin.y = 0.0f;
  vmax.y = 0.0f;

  update_intersect_points();

  registerChunkUpdate(ChunkUpdateFlags::VERTEX);
}


TextureSet* MapChunk::getTextureSet() const
{
  return texture_set.get();
}

void MapChunk::draw ( math::frustum const& frustum
                    , OpenGL::Scoped::use_program& mcnk_shader
                    , const float& cull_distance
                    , const glm::vec3& camera
                    , bool need_visibility_update
                    , bool show_unpaintable_chunks
                    , bool draw_paintability_overlay
                    , bool draw_chunk_flag_overlay
                    , bool draw_areaid_overlay
                    , std::map<int, misc::random_color>& area_id_colors
                    , int animtime
                    , display_mode display
                    , std::array<int, 4>& textures_bound
                    )
{
  ZoneScopedN("MapChunk::draw()");

/*
  bool cantPaint = show_unpaintable_chunks
                    && draw_paintability_overlay
                    && Noggit::Ui::selected_texture::get()
                    && !canPaintTexture(*Noggit::Ui::selected_texture::get());

  {
    ZoneScopedN("MapChunk::draw() : Binding textures");

    if (texture_set->num())
    {

      texture_set->bind_alpha(0);

      for (int i = 0; i < texture_set->num(); ++i)
      {
        //texture_set->bindTexture(i, i + 1, textures_bound);
        //mcnk_shader.uniform("tex_temp_" + std::to_string(i), (*texture_set->getTextures())[i]->array_index());

        if (texture_set->is_animated(i))
        {
          mcnk_shader.uniform("tex_anim_"+ std::to_string(i), texture_set->anim_uv_offset(i, animtime));
        }
      }
    }

    OpenGL::texture::set_active_texture(5);
    shadow.bind();
  }

  {
    ZoneScopedN("MapChunk::draw() : Setting uniforms");

    mcnk_shader.uniform("layer_count", (int)texture_set->num());
    mcnk_shader.uniform("cant_paint", (int)cantPaint);

    if (draw_chunk_flag_overlay)
    {
      mcnk_shader.uniform ("draw_impassible_flag", (int)header_flags.flags.impass);
    }

    if (draw_areaid_overlay)
    {
      mcnk_shader.uniform("areaid_color", (math::vector_4d)area_id_colors[areaID]);
    }
  }

  {
    ZoneScopedN("MapChunk::draw() : VAO/Buffer bindings");


  }

  {
    ZoneScopedN("MapChunk::draw() : Draw call");
    gl.drawElements(GL_TRIANGLES, _lod_level_indice_count, GL_UNSIGNED_SHORT, nullptr);
  }

  {
    ZoneScopedN("MapChunk::draw() : Defaulting tex anim uniforms");

    for (int i = 0; i < texture_set->num(); ++i)
    {
      if (texture_set->is_animated(i))
      {
        mcnk_shader.uniform("tex_anim_" + std::to_string(i), math::vector_2d());
      }
    }
  }

  */

}

bool MapChunk::intersect (math::ray const& ray, selection_result* results, bool first_result)
{
  if (!ray.intersect_bounds (vmin, vmax))
  {
    return false;
  }

  static constexpr std::array<std::uint16_t, 768> indices =
  {
    9, 0, 17, 9, 17, 18, 9, 18, 1, 9, 1, 0, 26, 17, 34, 26,
    34, 35, 26, 35, 18, 26, 18, 17, 43, 34, 51, 43, 51, 52, 43, 52,
    35, 43, 35, 34, 60, 51, 68, 60, 68, 69, 60, 69, 52, 60, 52, 51,
    77, 68, 85, 77, 85, 86, 77, 86, 69, 77, 69, 68, 94, 85, 102, 94,
    102, 103, 94, 103, 86, 94, 86, 85, 111, 102, 119, 111, 119, 120, 111, 120,
    103, 111, 103, 102, 128, 119, 136, 128, 136, 137, 128, 137, 120, 128, 120, 119,
    10, 1, 18, 10, 18, 19, 10, 19, 2, 10, 2, 1, 27, 18, 35, 27,
    35, 36, 27, 36, 19, 27, 19, 18, 44, 35, 52, 44, 52, 53, 44, 53,
    36, 44, 36, 35, 61, 52, 69, 61, 69, 70, 61, 70, 53, 61, 53, 52,
    78, 69, 86, 78, 86, 87, 78, 87, 70, 78, 70, 69, 95, 86, 103, 95,
    103, 104, 95, 104, 87, 95, 87, 86, 112, 103, 120, 112, 120, 121, 112, 121,
    104, 112, 104, 103, 129, 120, 137, 129, 137, 138, 129, 138, 121, 129, 121, 120,
    11, 2, 19, 11, 19, 20, 11, 20, 3, 11, 3, 2, 28, 19, 36, 28,
    36, 37, 28, 37, 20, 28, 20, 19, 45, 36, 53, 45, 53, 54, 45, 54,
    37, 45, 37, 36, 62, 53, 70, 62, 70, 71, 62, 71, 54, 62, 54, 53,
    79, 70, 87, 79, 87, 88, 79, 88, 71, 79, 71, 70, 96, 87, 104, 96,
    104, 105, 96, 105, 88, 96, 88, 87, 113, 104, 121, 113, 121, 122, 113, 122,
    105, 113, 105, 104, 130, 121, 138, 130, 138, 139, 130, 139, 122, 130, 122, 121,
    12, 3, 20, 12, 20, 21, 12, 21, 4, 12, 4, 3, 29, 20, 37, 29,
    37, 38, 29, 38, 21, 29, 21, 20, 46, 37, 54, 46, 54, 55, 46, 55,
    38, 46, 38, 37, 63, 54, 71, 63, 71, 72, 63, 72, 55, 63, 55, 54,
    80, 71, 88, 80, 88, 89, 80, 89, 72, 80, 72, 71, 97, 88, 105, 97,
    105, 106, 97, 106, 89, 97, 89, 88, 114, 105, 122, 114, 122, 123, 114, 123,
    106, 114, 106, 105, 131, 122, 139, 131, 139, 140, 131, 140, 123, 131, 123, 122,
    13, 4, 21, 13, 21, 22, 13, 22, 5, 13, 5, 4, 30, 21, 38, 30,
    38, 39, 30, 39, 22, 30, 22, 21, 47, 38, 55, 47, 55, 56, 47, 56,
    39, 47, 39, 38, 64, 55, 72, 64, 72, 73, 64, 73, 56, 64, 56, 55,
    81, 72, 89, 81, 89, 90, 81, 90, 73, 81, 73, 72, 98, 89, 106, 98,
    106, 107, 98, 107, 90, 98, 90, 89, 115, 106, 123, 115, 123, 124, 115, 124,
    107, 115, 107, 106, 132, 123, 140, 132, 140, 141, 132, 141, 124, 132, 124, 123,
    14, 5, 22, 14, 22, 23, 14, 23, 6, 14, 6, 5, 31, 22, 39, 31,
    39, 40, 31, 40, 23, 31, 23, 22, 48, 39, 56, 48, 56, 57, 48, 57,
    40, 48, 40, 39, 65, 56, 73, 65, 73, 74, 65, 74, 57, 65, 57, 56,
    82, 73, 90, 82, 90, 91, 82, 91, 74, 82, 74, 73, 99, 90, 107, 99,
    107, 108, 99, 108, 91, 99, 91, 90, 116, 107, 124, 116, 124, 125, 116, 125,
    108, 116, 108, 107, 133, 124, 141, 133, 141, 142, 133, 142, 125, 133, 125, 124,
    15, 6, 23, 15, 23, 24, 15, 24, 7, 15, 7, 6, 32, 23, 40, 32,
    40, 41, 32, 41, 24, 32, 24, 23, 49, 40, 57, 49, 57, 58, 49, 58,
    41, 49, 41, 40, 66, 57, 74, 66, 74, 75, 66, 75, 58, 66, 58, 57,
    83, 74, 91, 83, 91, 92, 83, 92, 75, 83, 75, 74, 100, 91, 108, 100,
    108, 109, 100, 109, 92, 100, 92, 91, 117, 108, 125, 117, 125, 126, 117, 126,
    109, 117, 109, 108, 134, 125, 142, 134, 142, 143, 134, 143, 126, 134, 126, 125,
    16, 7, 24, 16, 24, 25, 16, 25, 8, 16, 8, 7, 33, 24, 41, 33,
    41, 42, 33, 42, 25, 33, 25, 24, 50, 41, 58, 50, 58, 59, 50, 59,
    42, 50, 42, 41, 67, 58, 75, 67, 75, 76, 67, 76, 59, 67, 59, 58,
    84, 75, 92, 84, 92, 93, 84, 93, 76, 84, 76, 75, 101, 92, 109, 101,
    109, 110, 101, 110, 93, 101, 93, 92, 118, 109, 126, 118, 126, 127, 118, 127,
    110, 118, 110, 109, 135, 126, 143, 135, 143, 144, 135, 144, 127, 135, 127, 126
  };

  bool intersection_found = false;
  for (int i (0); i < indices.size(); i += 3)
  {
    if ( auto distance = ray.intersect_triangle ( mVertices[indices[i + 0]]
                                                , mVertices[indices[i + 1]]
                                                , mVertices[indices[i + 2]]
                                                )
       )
    {
      results->emplace_back
          (*distance, selected_chunk_type (this, std::make_tuple(indices[i], indices[i + 1],
                                                                 indices[i + 2]), ray.position (*distance)));
      intersection_found = true;

      if (first_result)
        return intersection_found;
    }
  }

  return intersection_found;
}

void MapChunk::updateVerticesData()
{
  // This method assumes tile's heightmap texture is currently bound to the active tex unit

  if (!(_chunk_update_flags & ChunkUpdateFlags::VERTEX))
    return;

  vmin.y = std::numeric_limits<float>::max();
  vmax.y = std::numeric_limits<float>::lowest();

  auto& tile_buffer = mt->getChunkHeightmapBuffer();

  int chunk_start = (px * 16 + py) * mapbufsize * 4;

  for (int i(0); i < mapbufsize; ++i)
  {
    vmin.y = std::min(vmin.y, mVertices[i].y);
    vmax.y = std::max(vmax.y, mVertices[i].y);
    tile_buffer[chunk_start + i * 4 + 3] = mVertices[i].y;
  }

  mt->markExtentsDirty();

  update_intersect_points();
}

void MapChunk::recalcExtents()
{
  for (int i(0); i < mapbufsize; ++i)
  {
    vmin.y = std::min(vmin.y, mVertices[i].y);
    vmax.y = std::max(vmax.y, mVertices[i].y);
  }
}

glm::vec3 MapChunk::getNeighborVertex(int i, unsigned dir)
{
  // i - vertex index
  // 0 - up_left
  // 1 - up_right
  // 2 - down_left
  // 3 - down_right

  // Returns a neighboring vertex,
  // if one does not exist,
  // returns a virtual vertex with the height that equals to the height of MapChunks's vertex i.

  constexpr float half_unit = UNITSIZE / 2.f;
  static constexpr std::array xdiff{-half_unit, half_unit, half_unit, -half_unit};
  static constexpr std::array zdiff{-half_unit, -half_unit, half_unit, half_unit};

  static constexpr std::array idiff{-9, -8, 9, 8};

  float vertex_x = mVertices[i].x + xdiff[dir];
  float vertex_z = mVertices[i].z + zdiff[dir];

  TileIndex tile({vertex_x, 0, vertex_z});
  glm::vec3 result{};

  if (tile.x == mt->index.x && tile.z == mt->index.z)
  {
    mt->getVertexInternal(vertex_x, vertex_z, &result);
  }
  else if (mt->_world->mapIndex.tileLoaded(tile))
  {
    mt->_world->mapIndex.getTile(tile)->getVertexInternal(vertex_x, vertex_z, &result);
  }
  else
  {
    result = {vertex_x, mVertices[i].y,  vertex_z};
  }

  return result;

  /*
  if ((i >= 1 && i <= 7 && dir < 2 && !py)
      || (i >= 137 && i <= 143 && (dir == 2 || dir == 3) && py == 15)
      || (!i && (dir < 2 && !py || (!dir || dir == 3) && !px))
      || (i == 8 && ((dir == 1 || dir == 2) && px == 15) || dir < 2 && !py)
      || (i == 136 && ((dir == 3 || dir == 2) && py == 15) || (!dir || dir == 3) && !px)
      || (i == 144 && ((dir == 3 || dir == 2) && py == 15) || ((dir == 1 || dir == 2) && px == 15))
      || (!(i % 17) && (!dir || dir == 3) && !px)
      || (!(i % 25) && (dir == 1 || dir == 2) && px == 15))
  {
    float vertex_x = mVertices[i].x + xdiff[dir];
    float vertex_z = mVertices[i].z + zdiff[dir];

    tile_index tile({vertex_x, 0, vertex_z});

    if (!mt->_world->mapIndex.tileLoaded(tile))
    {
      return glm::vec3( mVertices[i].x + xdiff[dir], mVertices[i].y, mVertices[i].z + zdiff[dir]);
    }

    glm::vec3 vertex{};
    mt->_world->mapIndex.getTile(tile)->getVertexInternal(mVertices[i].x + xdiff[dir], mVertices[i].z + zdiff[dir], &vertex);

    return glm::vec3( mVertices[i].x + xdiff[dir], vertex.y, mVertices[i].z + zdiff[dir]);

  }

  switch (dir)
  {
    case 0:
    {
      if (!i)
        return mt->getChunk(px - 1, py - 1)->mVertices[135];
      else if (i == 136)
        return mt->getChunk(px - 1, py)->mVertices[135];
      else if (i == 8)
        return mt->getChunk(px, py - 1)->mVertices[135];
      else if (i >= 1 && i <= 7)
        return mt->getChunk(px, py - 1)->mVertices[127 + i];
      else if (!(i % 17))
        return mt->getChunk(px - 1, py)->mVertices[i - 1];
      break;
    }
    case 1:
    {
      if (!i)
        return mt->getChunk(px, py - 1)->mVertices[128];
      else if (i == 144)
        return mt->getChunk(px + 1, py)->mVertices[128];
      else if (i == 8)
        return mt->getChunk(px + 1, py - 1)->mVertices[128];
      else if (i >= 1 && i <= 7)
        return mt->getChunk(px, py - 1)->mVertices[128 + i];
      else if (!(i % 17 % 8) && i % 17 != 16 && i % 17)
        return mt->getChunk(px + 1, py)->mVertices[i - 8];
      break;
    }
    case 2:
    {
      if (i == 136)
        return mt->getChunk(px, py + 1)->mVertices[9];
      else if (i == 144)
        return mt->getChunk(px + 1, py + 1)->mVertices[9];
      else if (i == 8)
        return mt->getChunk(px + 1, py)->mVertices[9];
      else if (!(i % 17 % 8) && i % 17 != 16 && i && i % 17)
        return mt->getChunk(px + 1, py)->mVertices[i + 9];
      else if (i >= 137 && i <= 143)
        return mt->getChunk(px, py + 1)->mVertices[i - 127];

      break;
    }
    case 3:
    {
      if (!i)
        return mt->getChunk(px - 1, py)->mVertices[16];
      else if (i == 136)
        return mt->getChunk(px - 1, py + 1)->mVertices[16];
      else if (i == 144)
        return mt->getChunk(px, py + 1)->mVertices[16];
      else if (!(i % 17))
        return mt->getChunk(px - 1, py)->mVertices[i + 16];
      else if (i >= 137 && i <= 143)
        return mt->getChunk(px, py + 1)->mVertices[i - 128];
      break;
    }
  }

  return mVertices[i + idiff[dir]];

  */
}

glm::uvec2 MapChunk::getUnitIndextAt(glm::vec3 pos)
{
    glm::uvec2 unit_index = glm::uvec2(floor((pos.x - xbase) / UNITSIZE), floor((pos.z - zbase) / UNITSIZE));

    if (unit_index.x > 8 || unit_index.y > 8)
        return glm::uvec2(8, 8);

    return unit_index;
}

void MapChunk::recalcNorms()
{
  // 0 - up_left
  // 1 - up_right
  // 2 - down_left
  // 3 - down_right

  auto& tile_buffer = mt->getChunkHeightmapBuffer();
  int chunk_start = (px * 16 + py) * mapbufsize * 4;

  for (int i = 0; i < mapbufsize; ++i)
  {
    glm::vec3 const P1 (getNeighborVertex(i, 0)); // up_left
    glm::vec3 const P2 (getNeighborVertex(i, 1)); // up_right
    glm::vec3 const P3 (getNeighborVertex(i, 2)); // down_left
    glm::vec3 const P4 (getNeighborVertex(i, 3)); // down_right

    glm::vec3 const N1 = glm::cross((P2 - mVertices[i]) , (P1 - mVertices[i]));
    glm::vec3 const N2 = glm::cross((P3 - mVertices[i]) , (P2 - mVertices[i]));
    glm::vec3 const N3 = glm::cross((P4 - mVertices[i]) , (P3 - mVertices[i]));
    glm::vec3 const N4 = glm::cross((P1 - mVertices[i]) , (P4 - mVertices[i]));

    glm::vec3 Norm (N1 + N2 + N3 + N4);
    Norm = glm::normalize(Norm);

    Norm.x = std::floor(Norm.x * 127) / 127;
    Norm.y = std::floor(Norm.y * 127) / 127;
    Norm.z = std::floor(Norm.z * 127) / 127;

    //! \todo: find out why recalculating normals without changing the terrain result in slightly different normals

    int pixel_start = chunk_start + i * 4;
    tile_buffer[pixel_start] = -Norm.z;
    tile_buffer[pixel_start + 1] = Norm.y;
    tile_buffer[pixel_start + 2] = -Norm.x;

  }

  registerChunkUpdate(ChunkUpdateFlags::NORMALS);

}

void MapChunk::updateNormalsData()
{
  //gl.texSubImage2D(GL_TEXTURE_2D, 0, 0, px * 16 + py, mapbufsize, 1, GL_RGB, GL_FLOAT, mNormals);
}

bool MapChunk::changeTerrain(glm::vec3 const& pos, float change, float radius, int BrushType, float inner_radius
                           , float min_height, float max_height)
{
  //float dist, xdiff, zdiff;
  bool changed = false;

  for (int i = 0; i < mapbufsize; ++i)
  {
    float dt = change;
    if (changeTerrainProcessVertex(pos, mVertices[i], dt, radius, inner_radius, BrushType))
    {
      changed = true;
      mVertices[i].y += dt;

      // Min/Max blending (issue #30): raising blends the terrain towards
      // the max height, lowering blends it towards the min height
      if (change > 0.f)
      {
        mVertices[i].y = std::min(mVertices[i].y, max_height);
      }
      else if (change < 0.f)
      {
        mVertices[i].y = std::max(mVertices[i].y, min_height);
      }
    }
  }
  if (changed)
  {
    registerChunkUpdate(ChunkUpdateFlags::VERTEX);
  }
  return changed;
}

bool MapChunk::ChangeMCCV(glm::vec3 const& pos, glm::vec4 const& color, float change, float radius, bool editMode)
{
  float dist;
  bool changed = false;

  if (!hasMCCV)
  {
    initMCCV();
    changed = true;
  }

  for (int i = 0; i < mapbufsize; ++i)
  {
    dist = misc::dist(mVertices[i], pos);
    if (dist <= radius)
    {
      float edit = change * (1.0f - dist / radius);
      if (editMode)
      {
        mccv[i].x += (color.x / 0.5f - mccv[i].x)* edit;
        mccv[i].y += (color.y / 0.5f - mccv[i].y)* edit;
        mccv[i].z += (color.z / 0.5f - mccv[i].z)* edit;
      }
      else
      {
        mccv[i].x += (1.0f - mccv[i].x) * edit;
        mccv[i].y += (1.0f - mccv[i].y) * edit;
        mccv[i].z += (1.0f - mccv[i].z) * edit;
      }

      mccv[i].x = std::min(std::max(mccv[i].x, 0.0f), 2.0f);
      mccv[i].y = std::min(std::max(mccv[i].y, 0.0f), 2.0f);
      mccv[i].z = std::min(std::max(mccv[i].z, 0.0f), 2.0f);

      changed = true;
    }
  }
  if (changed)
  {
    registerChunkUpdate(ChunkUpdateFlags::MCCV);
  }

  return changed;
}

bool MapChunk::stampMCCV(glm::vec3 const& pos, glm::vec4 const& color, float change, float radius, bool editMode, QImage* img, bool paint, bool use_image_colors)
{
  float dist;
  bool changed = false;

  if (!hasMCCV)
  {
    initMCCV();
    changed = true;
  }

  for (int i = 0; i < mapbufsize; ++i)
  {
    dist = misc::dist(mVertices[i], pos);

    if(std::abs(pos.x - mVertices[i].x) > radius || std::abs(pos.z - mVertices[i].z) > radius)
      continue;

    glm::vec3 const diff{mVertices[i] - pos};

    int pixel_x = std::round(((diff.x + radius) / (2.f * radius)) * img->width());
    int pixel_y =  std::round(((diff.z + radius) / (2.f * radius)) * img->height());

    if (use_image_colors)
    {
      if (pixel_x >= 0 && pixel_x < img->width() && pixel_y >= 0 && pixel_y < img->height())
      {
        QColor const image_color = img->pixelColor(pixel_x, pixel_y);
        float const alpha = image_color.alphaF();

        // Fix for issue #29: transparent mask pixels used to be read as
        // opaque black (#000000), overwriting the vertex paint data near
        // the cursor. Blend with the existing color by the pixel alpha so
        // transparent mask areas leave the vertex color untouched.
        if (alpha > 0.0f)
        {
          glm::vec3 const image_rgb
          {
            std::min(std::max(image_color.redF() / 0.5f, 0.0f), 2.0f),
            std::min(std::max(image_color.greenF() / 0.5f, 0.0f), 2.0f),
            std::min(std::max(image_color.blueF() / 0.5f, 0.0f), 2.0f)
          };

          mccv[i].x += (image_rgb.x - mccv[i].x) * alpha;
          mccv[i].y += (image_rgb.y - mccv[i].y) * alpha;
          mccv[i].z += (image_rgb.z - mccv[i].z) * alpha;
        }
      }
      // out of bounds mask pixels: leave the vertex color untouched
    }
    else
    {
      float image_factor;
      if (pixel_x >= 0 && pixel_x < img->width() && pixel_y >= 0 && pixel_y < img->height())
      {
        auto mask_color = img->pixelColor(pixel_x, pixel_y);
        // Fix for issues #10/#29: take the mask alpha into account so
        // fully transparent areas of the mask don't apply the brush and
        // feathered edges scale the effect smoothly
        image_factor = (mask_color.redF() + mask_color.greenF() + mask_color.blueF()) / 3.0f
            * mask_color.alphaF();
      }
      else
      {
        image_factor = 0.f;
      }

      float edit = image_factor * (paint ? ((change * (1.0f - dist / radius))) : (change * 20.f));
      if (editMode)
      {
        mccv[i].x += (color.x / 0.5f - mccv[i].x)* edit;
        mccv[i].y += (color.y / 0.5f - mccv[i].y)* edit;
        mccv[i].z += (color.z / 0.5f - mccv[i].z)* edit;
      }
      else
      {
        mccv[i].x += (1.0f - mccv[i].x) * edit;
        mccv[i].y += (1.0f - mccv[i].y) * edit;
        mccv[i].z += (1.0f - mccv[i].z) * edit;
      }

      mccv[i].x = std::min(std::max(mccv[i].x, 0.0f), 2.0f);
      mccv[i].y = std::min(std::max(mccv[i].y, 0.0f), 2.0f);
      mccv[i].z = std::min(std::max(mccv[i].z, 0.0f), 2.0f);
    }

    changed = true;

  }
  if (changed)
  {
    registerChunkUpdate(ChunkUpdateFlags::MCCV);
  }

  return changed;
}

void MapChunk::update_vertex_colors()
{
  if (_chunk_update_flags & ChunkUpdateFlags::MCCV)

    gl.texSubImage2D(GL_TEXTURE_2D, 0, 0, px * 16 + py, mapbufsize, 1, GL_RGB, GL_FLOAT, mccv);
}

glm::vec3 MapChunk::pickMCCV(glm::vec3 const& pos)
{
  float dist;
  float cur_dist = UNITSIZE;

  if (!hasMCCV)
  {
    return glm::vec3(1.0f, 1.0f, 1.0f);
  }

  int v_index = 0;
  for (int i = 0; i < mapbufsize; ++i)
  {
    dist = misc::dist(mVertices[i], pos);
    if (dist <= cur_dist)
    {
      cur_dist = dist;
      v_index = i;
    }
  }

  return mccv[v_index];

}

bool MapChunk::flattenTerrain ( glm::vec3 const& pos
                              , float remain
                              , float radius
                              , int BrushType
                              , flatten_mode const& mode
                              , glm::vec3 const& origin
                              , math::degrees angle
                              , math::degrees orientation
                              )
{
  bool changed (false);

  for (int i(0); i < mapbufsize; ++i)
  {
	  float const dist(misc::dist(mVertices[i], pos));

	  if (dist >= radius)
	  {
		  continue;
	  }

	  float const ah(origin.y
		  + ((mVertices[i].x - origin.x) * glm::cos(math::radians(orientation)._)
			  + (mVertices[i].z - origin.z) * glm::sin(math::radians(orientation)._)
			  ) * glm::tan(math::radians(angle)._)
	  );

	  if ((!mode.lower && ah < mVertices[i].y)
		  || (!mode.raise && ah > mVertices[i].y)
		  )
	  {
		  continue;
	  }

	  if (BrushType == eFlattenType_Origin)
	  {
		  mVertices[i].y = origin.y;
		  changed = true;
		  continue;
	  }

    mVertices[i].y = glm::mix
      ( 
        mVertices[i].y
      , ah
      , BrushType == eFlattenType_Flat ? remain
      : BrushType == eFlattenType_Linear ? remain * (1.f - dist / radius)
      : BrushType == eFlattenType_Smooth ? pow (remain, 1.f + dist / radius)
      : throw std::logic_error ("bad brush type")

      );

    changed = true;
  }

  if (changed)
  {
    registerChunkUpdate(ChunkUpdateFlags::VERTEX);
  }

  return changed;
}

bool MapChunk::blurTerrain ( glm::vec3 const& pos
                           , float remain
                           , float radius
                           , int BrushType
                           , flatten_mode const& mode
                           /*, std::function<std::optional<float>(float, float)> height*/
                           )
{
  bool changed = false;

  if (BrushType == eFlattenType_Origin)
  {
    return false;
  }

  for (int i (0); i < mapbufsize; ++i)
  {
    float const dist = misc::dist(mVertices[i], pos);

    if (dist >= radius)
    {
      continue;
    }

    int Rad = (int)(radius / UNITSIZE);
    float TotalHeight = 0;
    float TotalWeight = 0;
    for (int j = -Rad * 2; j <= Rad * 2; ++j)
    {
      float tz = pos.z + j * UNITSIZE / 2;
      for (int k = -Rad; k <= Rad; ++k)
      {
        float const tx = pos.x + k*UNITSIZE + (j % 2) * UNITSIZE / 2.0f;
        float const dist2 = misc::dist (tx, tz, mVertices[i].x, mVertices[i].z);
        if (dist2 > radius)
          continue;

        glm::vec3 vec;
        bool res = mt->getWorld()->GetVertex(tx, tz, &vec);
        if (res)
        {
            // float h = vec.y;
            TotalHeight += (1.0f - dist2 / radius) * vec.y;
            TotalWeight += (1.0f - dist2 / radius);
        }

        /*
        // auto h (height (tx, tz));
        auto h = height(tx, tz);
        if (h)
        {
          TotalHeight += (1.0f - dist2 / radius) * h.value();
          TotalWeight += (1.0f - dist2 / radius);
        }*/
      }
    }

    float target = TotalHeight / TotalWeight;
    float& y = mVertices[i].y;

    if ((target > y && !mode.raise) || (target < y && !mode.lower))
    {
      continue;
    }

    y = glm::mix
      ( 
        y,
        target,
        BrushType == eFlattenType_Flat ? remain
      : BrushType == eFlattenType_Linear ? remain * (1.f - dist / radius)
      : BrushType == eFlattenType_Smooth ? pow (remain, 1.f + dist / radius)
      : throw std::logic_error ("bad brush type")
      );

    changed = true;
  }

  if (changed)
  {
    registerChunkUpdate(ChunkUpdateFlags::VERTEX);
  }

  return changed;
}

bool MapChunk::changeTerrainProcessVertex(glm::vec3 const& pos, glm::vec3 const& vertex, float& dt,
                                          float radiusOuter, float radiusInner, int brushType)
{
  float dist, xdiff, zdiff;
  bool changed = false;

  xdiff = vertex.x - pos.x;
  zdiff = vertex.z - pos.z;

  if (brushType == eTerrainType_Quadra)
  {
    if ((std::abs(xdiff) < std::abs(radiusOuter / 2)) && (std::abs(zdiff) < std::abs(radiusOuter / 2)))
    {
      dist = std::sqrt(xdiff*xdiff + zdiff*zdiff);
      dt = dt * (1.0f - dist * radiusInner / radiusOuter);
      changed = true;
    }
  }
  else
  {
    dist = std::sqrt(xdiff*xdiff + zdiff*zdiff);
    if (dist < radiusOuter)
    {
      changed = true;

      switch (brushType)
      {
        case eTerrainType_Flat:
          break;
        case eTerrainType_Linear:
          dt = dt * (1.0f - dist * (1.0f - radiusInner) / radiusOuter);
          break;
        case eTerrainType_Smooth:
          dt = dt / (1.0f + dist / radiusOuter);
          break;
        case eTerrainType_Polynom:
          dt = dt * ((dist / radiusOuter)*(dist / radiusOuter) + dist / radiusOuter + 1.0f);
          break;
        case eTerrainType_Trigo:
          dt = dt * cos(dist / radiusOuter);
          break;
        case eTerrainType_Gaussian:
          dt = dist < radiusOuter * radiusInner ? dt * std::exp(-(std::pow(radiusOuter * radiusInner / radiusOuter, 2) / (2 * std::pow(0.39f, 2)))) : dt * std::exp(-(std::pow(dist / radiusOuter, 2) / (2 * std::pow(0.39f, 2))));

          break;
        default:
          LogError << "Invalid terrain edit type (" << brushType << ")" << std::endl;
          changed = false;
          break;
      }
    }
  }

  return changed;
}

auto MapChunk::stamp(glm::vec3 const& pos, float dt, QImage const* img, float radiusOuter
, float radiusInner, int brushType, bool sculpt
, float min_height, float max_height) -> void
{
  if (sculpt)
  {
    for(int i{}; i < mapbufsize; ++i)
    {
      if(std::abs(pos.x - mVertices[i].x) > radiusOuter || std::abs(pos.z - mVertices[i].z) > radiusOuter)
        continue;

      float delta = dt;
      changeTerrainProcessVertex(pos, mVertices[i], delta, radiusOuter, radiusInner, brushType);

      glm::vec3 const diff{mVertices[i] - pos};

      int pixel_x = std::round(((diff.x + radiusOuter) / (2.f * radiusOuter)) * img->width());
      int pixel_y =  std::round(((diff.z + radiusOuter) / (2.f * radiusOuter)) * img->height());

      float image_factor;

      if (pixel_x >= 0 && pixel_x < img->width() && pixel_y >= 0 && pixel_y < img->height())
      {
        auto color = img->pixelColor(pixel_x, pixel_y);
        image_factor = (color.redF() + color.greenF() + color.blueF()) / 3.0f;
      }
      else
      {
        image_factor = 0;
      }

      mVertices[i].y += delta * image_factor;

      // Min/Max blending (issue #30)
      if (dt > 0.f)
      {
        mVertices[i].y = std::min(mVertices[i].y, max_height);
      }
      else if (dt < 0.f)
      {
        mVertices[i].y = std::max(mVertices[i].y, min_height);
      }
    }
  }
  else
  {
    auto cur_action = NOGGIT_CUR_ACTION;
    float* original_heightmap = cur_action->getChunkTerrainOriginalData(this);

    if (!original_heightmap)
      return;

    // Fix for issue #26: the stamp used to recompute every vertex from the
    // action-start snapshot on each event (y = original + accumulated * f).
    // That re-basing discarded the edits of any other terrain tool stacked
    // after it (e.g. blur in a custom stamp), and the two tools kept
    // overwriting each other every frame, making the terrain flicker.
    //
    // Every brush falloff applied by changeTerrainProcessVertex() is a
    // multiplicative factor of dt, so applying the per-event delta to the
    // current height is mathematically identical to re-basing from the
    // snapshot as long as the stamp is the only tool editing the terrain,
    // and it composes correctly when other tools run in the same pass.
    for(int i{}; i < mapbufsize; ++i)
    {
      if(std::abs(pos.x - mVertices[i].x) > radiusOuter || std::abs(pos.z - mVertices[i].z) > radiusOuter)
        continue;

      float delta = dt;

      changeTerrainProcessVertex(pos, mVertices[i], delta, radiusOuter, radiusInner, brushType);

      glm::vec3 const diff{mVertices[i] - pos};

      int pixel_x = std::round(((diff.x + radiusOuter) / (2.f * radiusOuter)) * img->width());
      int pixel_y =  std::round(((diff.z + radiusOuter) / (2.f * radiusOuter)) * img->height());

      float image_factor;

      if (pixel_x >= 0 && pixel_x < img->width() && pixel_y >= 0 && pixel_y < img->height())
      {
        auto color = img->pixelColor(pixel_x, pixel_y);
        image_factor = (color.redF() + color.greenF() + color.blueF()) / 3.0f;
      }
      else
      {
        image_factor = 0;
      }

      mVertices[i].y += delta * image_factor;

      // Min/Max blending (issue #30)
      if (dt > 0.f)
      {
        mVertices[i].y = std::min(mVertices[i].y, max_height);
      }
      else if (dt < 0.f)
      {
        mVertices[i].y = std::max(mVertices[i].y, min_height);
      }
    }
  }

  registerChunkUpdate(ChunkUpdateFlags::VERTEX);
}

void MapChunk::eraseTextures()
{
  texture_set->eraseTextures();
}

void MapChunk::eraseTexture(scoped_blp_texture_reference const& tex)
{

    int textureindex = texture_set->get_texture_index_or_add(tex, 0);

    if (textureindex != -1)
    {
        texture_set->eraseTexture(textureindex);
    }
}

void MapChunk::change_texture_flags(scoped_blp_texture_reference const& tex, std::size_t flags)
{
  texture_set->change_texture_flags(tex, flags);
}

int MapChunk::addTexture(scoped_blp_texture_reference texture)
{
  return texture_set->addTexture(std::move (texture));
}

bool MapChunk::switchTexture(scoped_blp_texture_reference const& oldTexture, scoped_blp_texture_reference newTexture)
{
  bool changed = texture_set->replace_texture(oldTexture, std::move (newTexture));
  if (changed)
    registerChunkUpdate(ChunkUpdateFlags::ALPHAMAP);

  return changed;
}

bool MapChunk::paintTexture(glm::vec3 const& pos, Brush* brush, float strength, float pressure, scoped_blp_texture_reference texture)
{
  return texture_set->paintTexture(xbase, zbase, pos.x, pos.z, brush, strength, pressure, std::move (texture));
}

bool MapChunk::stampTexture(glm::vec3 const& pos, Brush *brush, float strength, float pressure, scoped_blp_texture_reference texture, QImage* img, bool paint)
{
  return texture_set->stampTexture(xbase, zbase, pos.x, pos.z, brush, strength, pressure, std::move (texture), img, paint);
}

bool MapChunk::replaceTexture(glm::vec3 const& pos, float radius, scoped_blp_texture_reference const& old_texture, scoped_blp_texture_reference new_texture, bool entire_chunk)
{
  return texture_set->replace_texture(xbase, zbase, pos.x, pos.z, radius, old_texture, std::move (new_texture), entire_chunk);
}

bool MapChunk::canPaintTexture(scoped_blp_texture_reference texture)
{
  return texture_set->canPaintTexture(texture);
}

void MapChunk::update_shadows()
{
  if (_chunk_update_flags & ChunkUpdateFlags::SHADOW)
    gl.texSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, px * 16 + py, 64, 64, 1, GL_RED, GL_UNSIGNED_BYTE, _shadow_map);
}

void MapChunk::clear_shadows()
{
  memset(_shadow_map, 0, 64 * 64);

  registerChunkUpdate(ChunkUpdateFlags::SHADOW);
}

void MapChunk::paintDetailDoodadsExclusion(glm::vec3 const& pos, float radius, bool exclusion)
{
    texture_set->setDetailDoodadsExclusion(xbase, zbase, pos, radius, false, exclusion);
}

bool MapChunk::isHole(int i, int j)
{
  return (holes & ((1 << ((j * 4) + i)))) != 0;
}

void MapChunk::setHole(glm::vec3 const& pos, float radius, bool big, bool add)
{
  if (big)
  {
    holes = add ? 0xFFFFFFFF : 0x0;
  }
  else
  {
    for (int x = 0; x < 4; ++x)
    {
      for (int y = 0; y < 4; ++y)
      {
        if (misc::getShortestDist(pos.x, pos.z, xbase + (MINICHUNKSIZE * x),
                          zbase + (MINICHUNKSIZE * y), MINICHUNKSIZE) <= radius)
        {
          int v = 1 << (y * 4 + x);
          holes = add ? (holes | v) : (holes & ~v);
        }

      }
    }
  }

  registerChunkUpdate(ChunkUpdateFlags::HOLES);
}

void MapChunk::setAreaID(int ID)
{
  areaID = ID;

  registerChunkUpdate(ChunkUpdateFlags::AREA_ID);
}

int MapChunk::getAreaID()
{
  return areaID;
}


void MapChunk::setFlag(bool changeto, uint32_t flag)
{
  if (changeto)
  {
    header_flags.value |= flag;
  }
  else
  {
    header_flags.value &= ~flag;
  }

  registerChunkUpdate(ChunkUpdateFlags::FLAGS);
}

void MapChunk::save(util::sExtendableArray& lADTFile
                    , int& lCurrentPosition
                    , int& lMCIN_Position
                    , std::map<std::string, int> &lTextures
                    , std::vector<WMOInstance*> &lObjectInstances
                    , std::vector<ModelInstance*>& lModelInstances
                    , bool use_mclq_liquids)
{
  int lID;
  int lMCNK_Size = 0x80;
  int lMCNK_Position = lCurrentPosition;
  lADTFile.Extend(8 + 0x80);  // This is only the size of the header. More chunks will increase the size.
  SetChunkHeader(lADTFile, lCurrentPosition, 'MCNK', lMCNK_Size);
  lADTFile.GetPointer<MCIN>(lMCIN_Position + 8)->mEntries[py * 16 + px].offset = lCurrentPosition; // check this

                                                                                                   // MCNK data
  // lADTFile.Insert(lCurrentPosition + 8, 0x80, reinterpret_cast<char*>(&(header)));
  auto const lMCNK_header = lADTFile.GetPointer<MapChunkHeader>(lCurrentPosition + 8);

  header_flags.flags.do_not_fix_alpha_map = use_mclq_liquids ? 0 : 1;

  lMCNK_header->flags = header_flags;
  lMCNK_header->ix = px;
  lMCNK_header->iy = py;
  lMCNK_header->zpos = zbase * -1.0f + ZEROPOINT;
  lMCNK_header->xpos = xbase * -1.0f + ZEROPOINT;
  lMCNK_header->ypos = ybase;

  lMCNK_header->holes = holes;
  lMCNK_header->areaid = areaID;

  lMCNK_header->nLayers = 0;
  lMCNK_header->nDoodadRefs = 0;
  lMCNK_header->ofsHeight = 0;
  lMCNK_header->ofsNormal = 0;
  lMCNK_header->ofsLayer = 0;
  lMCNK_header->ofsRefs = 0;
  lMCNK_header->ofsAlpha = 0;
  lMCNK_header->sizeAlpha = 0;
  lMCNK_header->ofsShadow = 0;
  lMCNK_header->sizeShadow = 0;
  lMCNK_header->nMapObjRefs = 0;
  lMCNK_header->ofsMCCV = 0;

  //! \todo  Implement sound emitter support. Or not.
  lMCNK_header->ofsSndEmitters = 0;
  lMCNK_header->nSndEmitters = 0;

  lMCNK_header->ofsLiquid = 0;
  //! \todo Is this still 8 if no chunk is present? Or did they correct that?
  lMCNK_header->sizeLiquid = 8;

  lMCNK_header->ypos = mVertices[0].y;

  if(texture_set)
  {
    // hackfix -- temp hackfix to bruteforce update + save
    texture_set->apply_alpha_changes();
    texture_set->updateDoodadMapping();

    std::copy(texture_set->getDoodadMappingBase(), texture_set->getDoodadMappingBase() + 8
    , lMCNK_header->doodadMapping);
    *reinterpret_cast<std::uint64_t*>(lMCNK_header->doodadStencil)
    = *reinterpret_cast<std::uint64_t const*>(texture_set->getDoodadStencilBase());
  }
  else
  {
    std::fill(lMCNK_header->doodadMapping, lMCNK_header->doodadMapping + 8, 0);
    *reinterpret_cast<std::uint64_t*>(lMCNK_header->doodadStencil) = 0;
  }

  lCurrentPosition += 8 + 0x80;

  // MCVT
  int lMCVT_Size = mapbufsize * 4;

  lADTFile.Extend(8 + lMCVT_Size);
  SetChunkHeader(lADTFile, lCurrentPosition, 'MCVT', lMCVT_Size);

  // lADTFile.GetPointer<MapChunkHeader>(lMCNK_Position + 8)->ofsHeight = lCurrentPosition - lMCNK_Position;

  auto header_ptr = lADTFile.GetPointer<MapChunkHeader>(lMCNK_Position + 8);

  header_ptr->ofsHeight = lCurrentPosition - lMCNK_Position;

  auto const lHeightmap = lADTFile.GetPointer<float>(lCurrentPosition + 8);

  for (int i = 0; i < mapbufsize; ++i)
    lHeightmap[i] = mVertices[i].y - mVertices[0].y;

  lCurrentPosition += 8 + lMCVT_Size;
  lMCNK_Size += 8 + lMCVT_Size;

  // MCCV
  int lMCCV_Size = 0;
  if (hasMCCV)
  {
    lMCCV_Size = mapbufsize * sizeof(unsigned int);
    lADTFile.Extend(8 + lMCCV_Size);
    SetChunkHeader(lADTFile, lCurrentPosition, 'MCCV', lMCCV_Size);
    header_ptr->ofsMCCV = lCurrentPosition - lMCNK_Position;

    auto const lmccv = lADTFile.GetPointer<unsigned int>(lCurrentPosition + 8);

    for (int i = 0; i < mapbufsize; ++i)
    {
      lmccv[i] = (((unsigned char)(mccv[i].z * 127.0f) & 0xFF) <<  0)
               + (((unsigned char)(mccv[i].y * 127.0f) & 0xFF) <<  8)
               + (((unsigned char)(mccv[i].x * 127.0f) & 0xFF) << 16);
    }

    lCurrentPosition += 8 + lMCCV_Size;
    lMCNK_Size += 8 + lMCCV_Size;
  }
  else
  {
    header_ptr->ofsMCCV = 0;
  }

  // MCNR
  int lMCNR_Size = mapbufsize * 3;

  lADTFile.Extend(8 + lMCNR_Size);
  SetChunkHeader(lADTFile, lCurrentPosition, 'MCNR', lMCNR_Size);

  header_ptr->ofsNormal = lCurrentPosition - lMCNK_Position;

  auto const lNormals = lADTFile.GetPointer<char>(lCurrentPosition + 8);

  auto& tile_buffer = mt->getChunkHeightmapBuffer();
  int chunk_start = (px * 16 + py) * mapbufsize * 4;

  for (int i = 0; i < mapbufsize; ++i)
  {
    int pixel_start = chunk_start + i * 4;

    lNormals[i * 3 + 0] = static_cast<char>(tile_buffer[pixel_start] * 127);
    lNormals[i * 3 + 1] = static_cast<char>(tile_buffer[pixel_start + 2] * 127);
    lNormals[i * 3 + 2] = static_cast<char>(tile_buffer[pixel_start + 1] * 127);
  }

  lCurrentPosition += 8 + lMCNR_Size;
  lMCNK_Size += 8 + lMCNR_Size;
  //        }

  // Unknown MCNR bytes
  // These are not in as we have data or something but just to make the files more blizzlike.
  //        {
  lADTFile.Extend(13);
  lCurrentPosition += 13;
  lMCNK_Size += 13;
  //        }

  // MCLY
  //        {
  size_t lMCLY_Size = texture_set ? texture_set->num() * 0x10 : 0;

  lADTFile.Extend(static_cast<long>(8 + lMCLY_Size));
  SetChunkHeader(lADTFile, lCurrentPosition, 'MCLY', static_cast<int>(lMCLY_Size));

  header_ptr->ofsLayer = lCurrentPosition - lMCNK_Position;
  header_ptr->nLayers = texture_set ? static_cast<std::uint32_t>(texture_set->num()) : 0;

  std::vector<std::vector<uint8_t>> alphamaps;

  int lMCAL_Size = 0;

  if (texture_set)
  {
    alphamaps = texture_set->save_alpha(use_big_alphamap);

    // MCLY data
    for (size_t j = 0; j < texture_set->num(); ++j)
    {
      auto const lLayer = lADTFile.GetPointer<ENTRY_MCLY>(lCurrentPosition + 8 + 0x10 * static_cast<unsigned long>(j));

      lLayer->textureID = lTextures.find(texture_set->filename(j))->second;
      lLayer->flags = texture_set->flag(j);
      lLayer->ofsAlpha = lMCAL_Size;
      lLayer->effectID = texture_set->effect(j);

      if (j == 0)
      {
        lLayer->flags &= ~(FLAG_USE_ALPHA | FLAG_ALPHA_COMPRESSED);
      }
      else
      {
        lLayer->flags |= FLAG_USE_ALPHA;
        //! \todo find out why compression fuck up textures ingame
        lLayer->flags &= ~FLAG_ALPHA_COMPRESSED;

        lMCAL_Size += static_cast<int>(alphamaps[j - 1].size());
      }
    }

  }

  lCurrentPosition += 8 + static_cast<int>(lMCLY_Size);
  lMCNK_Size += 8 + static_cast<int>(lMCLY_Size);
  //        }

  // MCRF
  //        {
  std::list<int> lDoodadIDs;
  std::list<int> lObjectIDs;

  std::array<glm::vec3, 2> lChunkExtents;
  lChunkExtents[0] = glm::vec3(xbase, 0.0f, zbase);
  lChunkExtents[1] = glm::vec3(xbase + CHUNKSIZE, 0.0f, zbase + CHUNKSIZE);

  // search all wmos that are inside this chunk
  lID = 0;
  for(auto const& wmo : lObjectInstances)
  {
    if (wmo->isInsideRect(&lChunkExtents))
    {
      lObjectIDs.push_back(lID);
    }

    lID++;
  }

  // search all models that are inside this chunk
  lID = 0;
  for(auto const& model : lModelInstances)
  {
    if (model->isInsideRect(&lChunkExtents))
    {
      lDoodadIDs.push_back(lID);
    }
    lID++;
  }

  int lMCRF_Size = static_cast<int>(4 * (lDoodadIDs.size() + lObjectIDs.size()));
  lADTFile.Extend(8 + lMCRF_Size);
  SetChunkHeader(lADTFile, lCurrentPosition, 'MCRF', lMCRF_Size);

  header_ptr->ofsRefs = lCurrentPosition - lMCNK_Position;
  header_ptr->nDoodadRefs = static_cast<std::uint32_t>(lDoodadIDs.size());
  header_ptr->nMapObjRefs = static_cast<std::uint32_t>(lObjectIDs.size());

  // MCRF data
  auto const lReferences = lADTFile.GetPointer<int>(lCurrentPosition + 8);

  lID = 0;
  for (std::list<int>::iterator it = lDoodadIDs.begin(); it != lDoodadIDs.end(); ++it)
  {
    lReferences[lID] = *it;
    lID++;
  }

  for (std::list<int>::iterator it = lObjectIDs.begin(); it != lObjectIDs.end(); ++it)
  {
    lReferences[lID] = *it;
    lID++;
  }

  lCurrentPosition += 8 + lMCRF_Size;
  lMCNK_Size += 8 + lMCRF_Size;
  //        }

  // MCSH
  if (has_shadows())
  {
    // header_flags.flags.has_mcsh = 1;
    lMCNK_header->flags.flags.has_mcsh = 1;

    int lMCSH_Size = 0x200;
    lADTFile.Extend(8 + lMCSH_Size);
    SetChunkHeader(lADTFile, lCurrentPosition, 'MCSH', lMCSH_Size);

    header_ptr->ofsShadow = lCurrentPosition - lMCNK_Position;
    header_ptr->sizeShadow = 0x200;

    auto const lLayer = lADTFile.GetPointer<char>(lCurrentPosition + 8);

    auto shadow_map = compressed_shadow_map();
    memcpy(lLayer.get(), shadow_map.data(), 0x200);

    lCurrentPosition += 8 + lMCSH_Size;
    lMCNK_Size += 8 + lMCSH_Size;
  }
  else
  {
    lMCNK_header->flags.flags.has_mcsh = 0;
    // header_flags.flags.has_mcsh = 0;
    header_ptr->ofsShadow = 0;
    header_ptr->sizeShadow = 0;
  }

  // MCAL
  lADTFile.Extend(8 + lMCAL_Size);
  SetChunkHeader(lADTFile, lCurrentPosition, 'MCAL', lMCAL_Size);

  header_ptr->ofsAlpha = lCurrentPosition - lMCNK_Position;
  header_ptr->sizeAlpha = 8 + lMCAL_Size;

  auto lAlphaMaps = lADTFile.GetPointer<char>(lCurrentPosition + 8);

  for (auto& alpha : alphamaps)
  {
    memcpy(lAlphaMaps.get(), alpha.data(), alpha.size());
    lAlphaMaps += alpha.size();
  }

  lCurrentPosition += 8 + lMCAL_Size;
  lMCNK_Size += 8 + lMCAL_Size;

  if (use_mclq_liquids)
  {
    auto liquids = liquid_chunk();

    if (liquids && liquids->layer_count() > 0)
    {
      int liquids_size = 8 + liquids->layer_count() * sizeof(mclq);

      lMCNK_Size += liquids_size;

      header_ptr->sizeLiquid = liquids_size;
      header_ptr->ofsLiquid = lCurrentPosition - lMCNK_Position;

      // current position updated inside
      liquids->save_mclq(lADTFile, lMCNK_Position, lCurrentPosition);
    }
    // no liquid, vanilla adt still have an empty chunk
    else
    {
      lADTFile.Extend(8);
      // size seems to be 0 in vanilla adts in the mclq chunk's header and set right in the mcnk header (layer_size * n_layer + 8)
      SetChunkHeader(lADTFile, lCurrentPosition, 'MCLQ', 0);

      header_ptr->sizeLiquid = 8;
      header_ptr->ofsLiquid = lCurrentPosition - lMCNK_Position;

      // When saving a tile that had MLCQ, also remove flags in MCNK
      // Client sees the flag and loads random data as if it were MCLQ, which we dont save.
      // clear MCLQ liquid flags (0x4, 0x8, 0x10, 0x20)
      header_ptr->flags.value &= 0xFFFFFFC3;

      lCurrentPosition += 8;
      lMCNK_Size += 8;
    }
  }

  // MCSE
  int lMCSE_Size = sizeof(ENTRY_MCSE) * sound_emitters.size();
  lADTFile.Extend(8 + lMCSE_Size);
  SetChunkHeader(lADTFile, lCurrentPosition, 'MCSE', lMCSE_Size);

  header_ptr->ofsSndEmitters = lCurrentPosition - lMCNK_Position;
  header_ptr->nSndEmitters = lMCSE_Size / 0x1C;

  lCurrentPosition += 8;

  for (auto& sound_emitter : sound_emitters)
  {
      auto const MCSE = lADTFile.GetPointer<ENTRY_MCSE>(lCurrentPosition);

      MCSE->soundId = sound_emitter.soundId;

      MCSE->pos[0] = ZEROPOINT - sound_emitter.pos[2];
      MCSE->pos[1] = ZEROPOINT - sound_emitter.pos[0];
      MCSE->pos[2] = sound_emitter.pos[1];

      MCSE->size[0] = sound_emitter.size[0];
      MCSE->size[1] = sound_emitter.size[1];
      MCSE->size[2] = sound_emitter.size[2];

      lCurrentPosition += 0x1C;
  }

  // lCurrentPosition += 8 + lMCSE_Size;
  lMCNK_Size += 8 + lMCSE_Size;

  lADTFile.GetPointer<sChunkHeader>(lMCNK_Position)->mSize = lMCNK_Size;
  lADTFile.GetPointer<MCIN>(lMCIN_Position + 8)->mEntries[py * 16 + px].size = lMCNK_Size + sizeof (sChunkHeader);
}

void MapChunk::saveModern(util::sExtendableArray& root_file
                          , int& root_position
                          , util::sExtendableArray& tex_file
                          , int& tex_position
                          , util::sExtendableArray& obj_file
                          , int& obj_position
                          , std::map<std::string, int>& texture_ids
                          , std::vector<WMOInstance*>& object_instances
                          , std::vector<ModelInstance*>& model_instances)
{
  // 9.1.5x (Shadowlands) modern split ADT writing, see docs/MODERN_ADT.md:
  //  - root file: MCNK with 0x80 header + geometry (MCVT/MCCV/MCNR/MCSE/MCDD)
  //  - tex file:  header-less MCNK with MCLY/MCSH/MCAL/MCMT
  //  - obj file:  header-less MCNK with MCRD/MCRW

  int lID;

  // modern files always use the 4096 byte (8 bit) alpha maps, never "fixed"
  // 63x63 maps, and Noggit only writes 16 bit (4x4) low res holes
  header_flags.flags.do_not_fix_alpha_map = 1;
  header_flags.flags.high_res_holes = 0;

  // ---------------------------------------------------------------------
  // per-chunk model references (MCRD/MCRW): which placements overlap this
  // chunk (the modern equivalent of the pre-Cataclysm MCRF computation)
  // ---------------------------------------------------------------------
  std::list<int> doodad_ids;
  std::list<int> object_ids;

  std::array<glm::vec3, 2> chunk_extents;
  chunk_extents[0] = glm::vec3(xbase, 0.0f, zbase);
  chunk_extents[1] = glm::vec3(xbase + CHUNKSIZE, 0.0f, zbase + CHUNKSIZE);

  lID = 0;
  for (auto const& wmo : object_instances)
  {
    if (wmo->isInsideRect(&chunk_extents))
    {
      object_ids.push_back(lID);
    }
    lID++;
  }

  lID = 0;
  for (auto const& model : model_instances)
  {
    if (model->isInsideRect(&chunk_extents))
    {
      doodad_ids.push_back(lID);
    }
    lID++;
  }

  // ---------------------------------------------------------------------
  // root file: MCNK with 0x80 header + terrain geometry
  // ---------------------------------------------------------------------
  int root_mcnk_size = 0x80;
  int const root_mcnk_position = root_position;

  root_file.Extend(8 + 0x80);
  SetChunkHeader(root_file, root_position, 'MCNK', root_mcnk_size);

  auto const root_header = root_file.GetPointer<MapChunkHeader>(root_position + 8);

  root_header->flags = header_flags;
  // keep the flags in sync with the chunks actually written below
  root_header->flags.flags.has_mccv = hasMCCV ? 1 : 0;
  root_header->ix = px;
  root_header->iy = py;
  root_header->zpos = zbase * -1.0f + ZEROPOINT;
  root_header->xpos = xbase * -1.0f + ZEROPOINT;
  root_header->ypos = mVertices[0].y;
  root_header->holes = holes & 0xFFFF;
  root_header->areaid = areaID;
  root_header->nLayers = texture_set ? static_cast<std::uint32_t>(texture_set->num()) : 0;
  root_header->nDoodadRefs = static_cast<std::uint32_t>(doodad_ids.size());
  root_header->nMapObjRefs = static_cast<std::uint32_t>(object_ids.size());

  root_header->ofsHeight = 0;
  root_header->ofsNormal = 0;
  root_header->ofsLayer = 0;
  root_header->ofsRefs = 0;
  root_header->ofsAlpha = 0;
  root_header->sizeAlpha = 0;
  root_header->ofsShadow = 0;
  root_header->sizeShadow = 0;
  root_header->ofsSndEmitters = 0;
  root_header->nSndEmitters = 0;
  // no MCLQ in modern files, liquids live in MH2O of the root file
  root_header->ofsLiquid = 0;
  root_header->sizeLiquid = 0;
  root_header->ofsMCCV = 0;

  if (texture_set)
  {
    // hackfix -- temp hackfix to bruteforce update + save
    texture_set->apply_alpha_changes();
    texture_set->updateDoodadMapping();

    std::copy(texture_set->getDoodadMappingBase(), texture_set->getDoodadMappingBase() + 8
      , root_header->doodadMapping);
    *reinterpret_cast<std::uint64_t*>(root_header->doodadStencil)
      = *reinterpret_cast<std::uint64_t const*>(texture_set->getDoodadStencilBase());
  }
  else
  {
    std::fill(root_header->doodadMapping, root_header->doodadMapping + 8, 0);
    *reinterpret_cast<std::uint64_t*>(root_header->doodadStencil) = 0;
  }

  root_position += 8 + 0x80;

  // MCVT
  {
    int const mcvt_size = mapbufsize * 4;

    root_file.Extend(8 + mcvt_size);
    SetChunkHeader(root_file, root_position, 'MCVT', mcvt_size);

    root_header->ofsHeight = root_position - root_mcnk_position;

    auto const heightmap = root_file.GetPointer<float>(root_position + 8);

    for (int i = 0; i < mapbufsize; ++i)
    {
      heightmap[i] = mVertices[i].y - mVertices[0].y;
    }

    root_position += 8 + mcvt_size;
    root_mcnk_size += 8 + mcvt_size;
  }

  // MCCV
  if (hasMCCV)
  {
    int const mccv_size = mapbufsize * sizeof(unsigned int);

    root_file.Extend(8 + mccv_size);
    SetChunkHeader(root_file, root_position, 'MCCV', mccv_size);

    root_header->ofsMCCV = root_position - root_mcnk_position;

    auto const mccv_data = root_file.GetPointer<unsigned int>(root_position + 8);

    for (int i = 0; i < mapbufsize; ++i)
    {
      mccv_data[i] = (((unsigned char)(mccv[i].z * 127.0f) & 0xFF) <<  0)
                   + (((unsigned char)(mccv[i].y * 127.0f) & 0xFF) <<  8)
                   + (((unsigned char)(mccv[i].x * 127.0f) & 0xFF) << 16);
    }

    root_position += 8 + mccv_size;
    root_mcnk_size += 8 + mccv_size;
  }

  // MCNR: modern (Cata+) files include the 13 padding bytes in the chunk size
  {
    int const mcnr_size = mapbufsize * 3 + 13;

    root_file.Extend(8 + mcnr_size);
    SetChunkHeader(root_file, root_position, 'MCNR', mcnr_size);

    root_header->ofsNormal = root_position - root_mcnk_position;

    auto const normals = root_file.GetPointer<char>(root_position + 8);

    auto& tile_buffer = mt->getChunkHeightmapBuffer();
    int const chunk_start = (px * 16 + py) * mapbufsize * 4;

    for (int i = 0; i < mapbufsize; ++i)
    {
      int const pixel_start = chunk_start + i * 4;

      normals[i * 3 + 0] = static_cast<char>(tile_buffer[pixel_start] * 127);
      normals[i * 3 + 1] = static_cast<char>(tile_buffer[pixel_start + 2] * 127);
      normals[i * 3 + 2] = static_cast<char>(tile_buffer[pixel_start + 1] * 127);
    }

    // the padding bytes stay zeroed
    std::fill(normals.get() + mapbufsize * 3, normals.get() + mcnr_size, 0);

    root_position += 8 + mcnr_size;
    root_mcnk_size += 8 + mcnr_size;
  }

  // MCSE
  if (!sound_emitters.empty())
  {
    int const mcse_size = static_cast<int>(sizeof(ENTRY_MCSE) * sound_emitters.size());

    root_file.Extend(8 + mcse_size);
    SetChunkHeader(root_file, root_position, 'MCSE', mcse_size);

    root_header->ofsSndEmitters = root_position - root_mcnk_position;
    root_header->nSndEmitters = mcse_size / 0x1C;

    root_position += 8;
    root_mcnk_size += 8 + mcse_size;

    for (auto& sound_emitter : sound_emitters)
    {
      auto const mcse = root_file.GetPointer<ENTRY_MCSE>(root_position);

      mcse->soundId = sound_emitter.soundId;

      mcse->pos[0] = ZEROPOINT - sound_emitter.pos[2];
      mcse->pos[1] = ZEROPOINT - sound_emitter.pos[0];
      mcse->pos[2] = sound_emitter.pos[1];

      mcse->size[0] = sound_emitter.size[0];
      mcse->size[1] = sound_emitter.size[1];
      mcse->size[2] = sound_emitter.size[2];

      root_position += 0x1C;
    }
  }

  // MCDD: preserved detail doodad disable map
  if (_modern_preserved.has_mcdd)
  {
    root_file.Extend(8 + 8);
    SetChunkHeader(root_file, root_position, 'MCDD', 8);

    auto const mcdd_data = root_file.GetPointer<std::uint8_t>(root_position + 8);
    std::copy(_modern_preserved.mcdd.begin(), _modern_preserved.mcdd.end(), mcdd_data.get());

    root_position += 16;
    root_mcnk_size += 16;
  }

  root_file.GetPointer<sChunkHeader>(root_mcnk_position)->mSize = root_mcnk_size;

  // ---------------------------------------------------------------------
  // tex file: header-less MCNK with MCLY / MCSH / MCAL / MCMT
  // ---------------------------------------------------------------------
  std::vector<std::vector<uint8_t>> alphamaps;

  if (texture_set)
  {
    alphamaps = texture_set->save_alpha(true);
  }

  int mcal_size = 0;
  for (auto const& amap : alphamaps)
  {
    mcal_size += static_cast<int>(amap.size());
  }

  bool const write_shadows = has_shadows();
  std::size_t const num_layers = texture_set ? texture_set->num() : 0;

  int tex_mcnk_size = 0;
  tex_mcnk_size += 8 + static_cast<int>(num_layers * sizeof(ENTRY_MCLY));   // MCLY
  if (write_shadows)
  {
    tex_mcnk_size += 8 + 0x200;                                            // MCSH
  }
  tex_mcnk_size += 8 + mcal_size;                                          // MCAL
  if (_modern_preserved.has_mcmt)
  {
    tex_mcnk_size += 8 + 4;                                                // MCMT
  }

  tex_file.Extend(8 + tex_mcnk_size);
  SetChunkHeader(tex_file, tex_position, 'MCNK', tex_mcnk_size);
  tex_position += 8;

  // MCLY
  {
    int const mcly_size = static_cast<int>(num_layers * sizeof(ENTRY_MCLY));

    SetChunkHeader(tex_file, tex_position, 'MCLY', mcly_size);

    int mcum_mcal_offset = 0;

    for (size_t j = 0; j < num_layers; ++j)
    {
      auto const layer = tex_file.GetPointer<ENTRY_MCLY>(tex_position + 8 + sizeof(ENTRY_MCLY) * j);

      auto const texture_it = texture_ids.find(texture_set->filename(j));

      if (texture_it == texture_ids.end())
      {
        // can only happen when a texture appeared between the table build and
        // this write; fall back to the base layer texture
        LogError << "MapChunk::saveModern: texture \"" << texture_set->filename(j)
                 << "\" missing from the MDID table, using slot 0 instead" << std::endl;
        layer->textureID = 0;
      }
      else
      {
        layer->textureID = static_cast<uint32_t>(texture_it->second);
      }

      layer->flags = texture_set->flag(j);
      layer->ofsAlpha = mcum_mcal_offset;
      layer->effectID = texture_set->effect(j);

      if (j == 0)
      {
        layer->flags &= ~(FLAG_USE_ALPHA | FLAG_ALPHA_COMPRESSED);
      }
      else
      {
        layer->flags |= FLAG_USE_ALPHA;
        // always written uncompressed (4096 bytes per layer map)
        layer->flags &= ~FLAG_ALPHA_COMPRESSED;

        mcum_mcal_offset += static_cast<int>(alphamaps[j - 1].size());
      }
    }

    tex_position += 8 + mcly_size;
  }

  // MCSH: shadow data lives in the tex file for the split format, the flag
  // stays in the root MCNK header
  root_header->flags.flags.has_mcsh = write_shadows ? 1 : 0;

  if (write_shadows)
  {
    SetChunkHeader(tex_file, tex_position, 'MCSH', 0x200);

    auto const shadow_data = tex_file.GetPointer<char>(tex_position + 8);
    auto const shadow_map = compressed_shadow_map();
    memcpy(shadow_data.get(), shadow_map.data(), 0x200);

    tex_position += 8 + 0x200;
  }

  // MCAL (always a single chunk holding every layer map, uncompressed)
  {
    SetChunkHeader(tex_file, tex_position, 'MCAL', mcal_size);

    auto alpha_data = tex_file.GetPointer<char>(tex_position + 8);

    for (auto& amap : alphamaps)
    {
      memcpy(alpha_data.get(), amap.data(), amap.size());
      alpha_data += amap.size();
    }

    tex_position += 8 + mcal_size;
  }

  // MCMT: preserved terrain material ids
  if (_modern_preserved.has_mcmt)
  {
    SetChunkHeader(tex_file, tex_position, 'MCMT', 4);

    auto const mcmt_data = tex_file.GetPointer<std::uint8_t>(tex_position + 8);
    std::copy(_modern_preserved.mcmt.begin(), _modern_preserved.mcmt.end(), mcmt_data.get());

    tex_position += 8 + 4;
  }

  // ---------------------------------------------------------------------
  // obj file: header-less MCNK with MCRD (doodad refs) / MCRW (wmo refs)
  // ---------------------------------------------------------------------
  int const obj_mcnk_size = (8 + 4 * static_cast<int>(doodad_ids.size()))
                          + (8 + 4 * static_cast<int>(object_ids.size()));

  obj_file.Extend(8 + obj_mcnk_size);
  SetChunkHeader(obj_file, obj_position, 'MCNK', obj_mcnk_size);
  obj_position += 8;

  SetChunkHeader(obj_file, obj_position, 'MCRD', 4 * static_cast<int>(doodad_ids.size()));
  {
    auto const refs = obj_file.GetPointer<int>(obj_position + 8);

    lID = 0;
    for (int const doodad : doodad_ids)
    {
      refs[lID++] = doodad;
    }

    obj_position += 8 + 4 * static_cast<int>(doodad_ids.size());
  }

  SetChunkHeader(obj_file, obj_position, 'MCRW', 4 * static_cast<int>(object_ids.size()));
  {
    auto const refs = obj_file.GetPointer<int>(obj_position + 8);

    lID = 0;
    for (int const object : object_ids)
    {
      refs[lID++] = object;
    }

    obj_position += 8 + 4 * static_cast<int>(object_ids.size());
  }
}


bool MapChunk::fixGapLeft(const MapChunk* chunk)
{
  if (!chunk)
    return false;

  bool changed = false;

  for (size_t i = 0; i <= 136; i+= 17)
  {
    float h = chunk->mVertices[i + 8].y;
    if (mVertices[i].y != h)
    {
      mVertices[i].y = h;
      changed = true;
    }
  }

  if (changed)
  {
    registerChunkUpdate(ChunkUpdateFlags::VERTEX);
  }

  return changed;
}

bool MapChunk::fixGapAbove(const MapChunk* chunk)
{
  if (!chunk)
    return false;

  bool changed = false;

  for (size_t i = 0; i < 9; i++)
  {
    float h = chunk->mVertices[i + 136].y;
    if (mVertices[i].y != h)
    {
      mVertices[i].y = h;
      changed = true;
    }
  }

  if (changed)
  {
    registerChunkUpdate(ChunkUpdateFlags::VERTEX);
  }

  return changed;
}

glm::vec3* MapChunk::getHeightmap()
{
  return &mVertices[0];
}

glm::vec3 const* MapChunk::getNormals() const
{
  return &mNormals[0];
}

glm::vec3* MapChunk::getVertexColors()
{
  return &mccv[0];
}


void MapChunk::selectVertex(glm::vec3 const& pos, float radius, std::unordered_set<glm::vec3*>& vertices)
{
  if (misc::getShortestDist(pos.x, pos.z, xbase, zbase, CHUNKSIZE) > radius)
  {
    return;
  }

  for (int i = 0; i < mapbufsize; ++i)
  {
    if (misc::dist(pos.x, pos.z, mVertices[i].x, mVertices[i].z) <= radius)
    {
      vertices.emplace(&mVertices[i]);
    }
  }
}

void MapChunk::fixVertices(std::unordered_set<glm::vec3*>& selected)
{
  std::vector<int> ids ={ 0, 1, 17, 18 };
  // iterate through each "square" of vertices
  for (int i = 0; i < 64; ++i)
  {
    int not_selected = 0, count = 0, mid_vertex = ids[0] + 9;
    float h = 0.0f;

    for (int& index : ids)
    {
      if (selected.find(&mVertices[index]) == selected.end())
      {
        not_selected = index;
      }
      else
      {
        count++;
      }
      h += mVertices[index].y;
      index += (((i+1) % 8) == 0) ? 10 : 1;
    }

    if (count == 2)
    {
      mVertices[mid_vertex].y = h * 0.25f;
    }
    else if (count == 3)
    {
      mVertices[mid_vertex].y = (h - mVertices[not_selected].y) / 3.0f;
    }
  }
}

bool MapChunk::isBorderChunk(std::unordered_set<glm::vec3*>& selected)
{
  for (int i = 0; i < mapbufsize; ++i)
  {
    // border chunk if at least a vertex isn't selected
    if (selected.find(&mVertices[i]) == selected.end())
    {
      return true;
    }
  }

  return false;
}

QImage MapChunk::getHeightmapImage(float min_height, float max_height)
{
  glm::vec3* heightmap = getHeightmap();

  QImage image(17, 17, QImage::Format_RGBA64);

  unsigned const LONG{9}, SHORT{8}, SUM{LONG + SHORT}, DSUM{SUM * 2};

  for (unsigned y = 0; y < SUM; ++y)
    for (unsigned x = 0; x < SUM; ++x)
    {
      unsigned const plain {y * SUM + x};
      bool const is_virtual {static_cast<bool>(plain % 2)};
      bool const erp = plain % DSUM / SUM;
      unsigned const idx {(plain - (is_virtual ? (erp ? SUM : 1) : 0)) / 2};
      float value = is_virtual ? (heightmap[idx].y + heightmap[idx + (erp ? SUM : 1)].y) / 2.f : heightmap[idx].y;
      value = std::min(1.0f, std::max(0.0f, ((value - min_height) / (max_height - min_height))));
      image.setPixelColor(x, y, QColor::fromRgbF(value, value, value, 1.0));
    }

  return std::move(image);
}

QImage MapChunk::getAlphamapImage(unsigned layer)
{
  texture_set->apply_alpha_changes();
  auto& alphamaps = *texture_set->getAlphamaps();

  auto& alpha_layer = *alphamaps.at(layer - 1);

  QImage image(64, 64, QImage::Format_RGBA8888);

  for (int i = 0; i < 64; ++i)
  {
    for (int j = 0; j < 64; ++j)
    {
      int value = alpha_layer.getAlpha(64 * j + i);
      image.setPixelColor(i, j, QColor(value, value, value, 255));
    }
  }

  return std::move(image);
}

void MapChunk::setHeightmapImage(QImage const& image, float multiplier, int mode)
{
  glm::vec3* heightmap = getHeightmap();

  unsigned const LONG{9}, SHORT{8}, SUM{LONG + SHORT}, DSUM{SUM * 2};

  for (unsigned y = 0; y < SUM; ++y)
    for (unsigned x = 0; x < SUM; ++x)
    {
      unsigned const plain {y * SUM + x};
      bool const is_virtual {static_cast<bool>(plain % 2)};

      if (is_virtual)
        continue;

      bool const erp = plain % DSUM / SUM;
      unsigned const idx {(plain - (is_virtual ? (erp ? SUM : 1) : 0)) / 2};

      switch (mode)
      {
        case 0: // Set
          heightmap[idx].y = qGray(image.pixel(x, y)) / 255.0f * multiplier;
          break;

        case 1: // Add
          heightmap[idx].y += qGray(image.pixel(x, y)) / 255.0f * multiplier;
          break;

        case 2: // Subtract
          heightmap[idx].y -= qGray(image.pixel(x, y)) / 255.0f * multiplier;
          break;

        case 3: // Multiply
          heightmap[idx].y *= qGray(image.pixel(x, y)) / 255.0f * multiplier;
          break;
      }
    }
  registerChunkUpdate(ChunkUpdateFlags::VERTEX);
}

ChunkWater const* MapChunk::liquid_chunk() const
{
  return mt->Water.getChunk(px, py);
}

ChunkWater* MapChunk::liquid_chunk()
{
  return mt->Water.getChunk(px, py);
}

bool MapChunk::hasColors() const
{
  return hasMCCV;
}

void MapChunk::unload()
{
  _chunk_update_flags = ChunkUpdateFlags::VERTEX | ChunkUpdateFlags::ALPHAMAP
                        | ChunkUpdateFlags::SHADOW | ChunkUpdateFlags::MCCV
                        | ChunkUpdateFlags::NORMALS| ChunkUpdateFlags::HOLES
                        | ChunkUpdateFlags::AREA_ID| ChunkUpdateFlags::FLAGS
                        | ChunkUpdateFlags::GROUND_EFFECT | ChunkUpdateFlags::DETAILDOODADS_EXCLUSION;
}

void MapChunk::setAlphamapImage(const QImage &image, unsigned int layer)
{
  if (!layer)
    return;

  texture_set->create_temporary_alphamaps_if_needed();
  auto& temp_alphamaps = *texture_set->getTempAlphamaps();

  for (int i = 0; i < 64; ++i)
  {
    for (int j = 0; j < 64; ++j)
    {
      temp_alphamaps[layer][64 * j + i] = static_cast<float>(qGray(image.pixel(i, j))) / 255.0f;
    }
  }

  texture_set->markDirty();
}

QImage MapChunk::getVertexColorImage()
{
  glm::vec3* colors = getVertexColors();

  QImage image(17, 17, QImage::Format_RGBA8888);

  unsigned const LONG{9}, SHORT{8}, SUM{LONG + SHORT}, DSUM{SUM * 2};


  for (unsigned y = 0; y < SUM; ++y)
    for (unsigned x = 0; x < SUM; ++x)
    {
      unsigned const plain {y * SUM + x};
      bool const is_virtual {static_cast<bool>(plain % 2)};
      bool const erp = plain % DSUM / SUM;
      unsigned const idx {(plain - (is_virtual ? (erp ? SUM : 1) : 0)) / 2};
      float r = is_virtual ? (colors[idx].x + colors[idx + (erp ? SUM : 1)].x) / 4.f : colors[idx].x / 2.f;
      float g = is_virtual ? (colors[idx].y + colors[idx + (erp ? SUM : 1)].y) / 4.f : colors[idx].y / 2.f;
      float b = is_virtual ? (colors[idx].z + colors[idx + (erp ? SUM : 1)].z) / 4.f : colors[idx].z / 2.f;
      image.setPixelColor(x, y, QColor::fromRgbF(r, g, b, 1.0));
    }

  return std::move(image);
}

void MapChunk::setVertexColorImage(const QImage &image)
{
  glm::vec3* colors = getVertexColors();

  unsigned const LONG{9}, SHORT{8}, SUM{LONG + SHORT}, DSUM{SUM * 2};

  for (unsigned y = 0; y < SUM; ++y)
    for (unsigned x = 0; x < SUM; ++x)
    {
      unsigned const plain{y * SUM + x};
      bool const is_virtual{static_cast<bool>(plain % 2)};

      if (is_virtual)
        continue;

      bool const erp = plain % DSUM / SUM;
      unsigned const idx{(plain - (is_virtual ? (erp ? SUM : 1) : 0)) / 2};

      QColor color = image.pixelColor(x, y);
      colors[idx].x = color.redF() * 2.f;
      colors[idx].y = color.greenF() * 2.f;
      colors[idx].z = color.blueF() * 2.f;
    }

  registerChunkUpdate(ChunkUpdateFlags::MCCV);
}

void MapChunk::initMCCV()
{
  if (!hasMCCV)
  {
    for (int i = 0; i < mapbufsize; ++i)
    {
      mccv[i].x = 1.0f; // set default shaders
      mccv[i].y = 1.0f;
      mccv[i].z = 1.0f;
    }

    header_flags.flags.has_mccv = 1;
    hasMCCV = true;
  }
}

void MapChunk::registerChunkUpdate(unsigned flags)
{
  _chunk_update_flags |= flags;
  mt->registerChunkUpdate(flags);
}

void MapChunk::endChunkUpdates()
{
  _chunk_update_flags = 0;
}

unsigned MapChunk::getUpdateFlags() const
{
  return _chunk_update_flags;
}
