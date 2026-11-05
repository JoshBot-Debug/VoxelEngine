#include "VoxLoader.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

#include "voxel/Type.h"

namespace vxen {

namespace vox_loader {
struct Voxel {
  uint8_t x, y, z, color;
};

struct Model {
  uint32_t           x, y, z;
  std::vector<Voxel> voxels;
};

struct Chunk {
  std::array<char, 4> id;
  uint32_t            contentSize;
  uint32_t            childrenSize;
};
} // namespace vox_loader

uint32_t ReadU32(std::istream& stream) {
  uint32_t value;
  stream.read(reinterpret_cast<char*>(&value), 4);
  return value;
}

void ReadChunkHeader(std::istream& stream, vox_loader::Chunk& chunk) {
  stream.read(chunk.id.data(), 4);
  chunk.contentSize  = ReadU32(stream);
  chunk.childrenSize = ReadU32(stream);
}

void VoxLoader::SetPalette(Palette* palette) {
  m_Palette = palette;
}

void VoxLoader::SetChunkManager(ChunkManager* chunkManager) {
  m_ChunkManager = chunkManager;
}

bool VoxLoader::Load(const std::string& path) {
  if (!m_Palette || !m_ChunkManager) {
    std::cerr << "VoxLoader: Palette or ChunkManager not set\n";
    return false;
  }

  std::ifstream file(path, std::ios::binary);

  if (!file) {
    std::cerr << "VoxLoader: Failed to open " << path << '\n';
    return false;
  }

  char magic[4];
  file.read(magic, 4);

  if (std::memcmp(magic, "VOX ", 4) != 0) {
    std::cerr << "VoxLoader: Invalid VOX file\n";
    return false;
  }

  uint32_t version = ReadU32(file);

  if (version < 150) {
    std::cerr << "VoxLoader: Unsupported VOX version\n";
    return false;
  }

  // MAIN chunk
  vox_loader::Chunk mainChunk;
  ReadChunkHeader(file, mainChunk);

  std::vector<vox_loader::Model> models;
  vox_loader::Model              currentModel {};

  bool hasSize   = false;
  bool hasVoxels = false;

  // Default MagicaVoxel palette
  std::array<uint8_t, 256 * 4> paletteRGBA {};

  while (file && file.peek() != EOF) {
    vox_loader::Chunk chunk;
    ReadChunkHeader(file, chunk);

    const auto chunkStart = file.tellg();

    if (std::memcmp(chunk.id.data(), "SIZE", 4) == 0) {
      currentModel.x = ReadU32(file);
      currentModel.y = ReadU32(file);
      currentModel.z = ReadU32(file);
      hasSize        = true;
    } else if (std::memcmp(chunk.id.data(), "XYZI", 4) == 0) {
      uint32_t count = ReadU32(file);

      currentModel.voxels.resize(count);

      file.read(
          reinterpret_cast<char*>(currentModel.voxels.data()),
          count * sizeof(vox_loader::Voxel));

      hasVoxels = true;
    } else if (std::memcmp(chunk.id.data(), "RGBA", 4) == 0) {
      file.read(
          reinterpret_cast<char*>(paletteRGBA.data()),
          paletteRGBA.size());
    }

    // Advance to next chunk
    file.seekg(
        chunkStart +
        static_cast<std::streamoff>(chunk.contentSize));

    // Preserve model when SIZE + XYZI are complete
    if (hasSize && hasVoxels) {
      models.emplace_back(std::move(currentModel));
      currentModel = {};
      hasSize      = false;
      hasVoxels    = false;
    }
  }

  if (models.empty()) {
    std::cerr << "VoxLoader: No voxel models found\n";
    return false;
  }

  // Create palette materials
  std::array<uint32_t, 256> materialIDs {};

  for (uint32_t i = 0; i < 256; ++i) {
    uint8_t r = paletteRGBA[i * 4 + 0];
    uint8_t g = paletteRGBA[i * 4 + 1];
    uint8_t b = paletteRGBA[i * 4 + 2];
    uint8_t a = paletteRGBA[i * 4 + 3];

    auto material = std::make_shared<Material>(
        Material {
            .Albedo = glm::vec4(
                r / 255.0f,
                g / 255.0f,
                b / 255.0f,
                a / 255.0f)});

    auto itemId = m_Palette->Create(
        Palette::Item {
            .Name = "VOX_" + std::to_string(i + 1),
            .Mat  = material});

    materialIDs[i] = itemId;
  }

  // Populate chunk manager
  for (const vox_loader::Model& model : models) {
    for (const vox_loader::Voxel& voxel : model.voxels) {
      if (voxel.color == 0)
        continue;

      uint32_t materialID = materialIDs[voxel.color - 1];

      m_ChunkManager->Set(
          voxel.x,
          voxel.z,
          voxel.y,
          materialID);
    }
  }

  uint32_t worldX = 0;
  uint32_t worldY = 0;
  uint32_t worldZ = 0;

  for (const auto& model : models) {
    worldX = std::max(worldX, model.x);
    worldY = std::max(worldY, model.y);
    worldZ = std::max(worldZ, model.z);
  }

  std::cout << "VoxLoader: World size "
            << worldX << "x"
            << worldY << "x"
            << worldZ << '\n';

  std::cout << "VoxLoader: " << m_Palette->GetMaterials().size() << " Materials created\n";

  std::cout << "VoxLoader: Loaded " << models.size()
            << " model(s)\n";

  return true;
}

} // namespace vxen
