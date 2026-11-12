
#include "VL32Loader.h"

#include "ChunkManager.h"

#include <array>
#include <fstream>
#include <stdexcept>
#include <unordered_map>

namespace vxen {

VL32Loader::VL32Loader(MaterialResolver materialResolver, CreateVoxel createVoxel)
    : m_MaterialResolver(std::move(materialResolver))
    , m_CreateVoxel(std::move(createVoxel)) {
  if (!m_MaterialResolver || !m_CreateVoxel)
    throw std::invalid_argument("VL32Loader: invalid dependencies");
}

uint32_t VL32Loader::ReadU32BE(const uint8_t* data) {
  return (uint32_t(data[0]) << 24) |
         (uint32_t(data[1]) << 16) |
         (uint32_t(data[2]) << 8) |
         uint32_t(data[3]);
}

int32_t VL32Loader::ReadI32BE(const uint8_t* data) {
  return static_cast<int32_t>(ReadU32BE(data));
}

VL32Loader::Result VL32Loader::Load(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);

  if (!file)
    throw std::runtime_error("VL32Loader: failed to open file");

  Result result;

  std::unordered_map<uint32_t, uint32_t> materialCache;

  std::array<uint8_t, 16> record {};

  bool firstVoxel = true;

  while (file.read(reinterpret_cast<char*>(record.data()), record.size())) {
    const int32_t x = ReadI32BE(record.data() + 0);
    const int32_t y = ReadI32BE(record.data() + 4);
    const int32_t z = ReadI32BE(record.data() + 8);

    RGBA color {
        record[13],
        record[14],
        record[15],
        record[12]};

    // Alpha zero means empty voxel.
    if (color.A == 0)
      continue;

    const uint32_t rgba =
        (uint32_t(color.R) << 24) |
        (uint32_t(color.G) << 16) |
        (uint32_t(color.B) << 8) |
        uint32_t(color.A);

    auto it = materialCache.find(rgba);

    uint32_t materialID;

    if (it == materialCache.end()) {
      materialID = m_MaterialResolver(color);
      materialCache.emplace(rgba, materialID);
      ++result.MaterialCount;
    } else {
      materialID = it->second;
    }

    m_CreateVoxel(x, y, z, materialID);

    const glm::ivec3 position(x, y, z);

    if (firstVoxel) {
      result.Min = position;
      result.Max = position;
      firstVoxel = false;
    } else {
      result.Min = glm::min(result.Min, position);
      result.Max = glm::max(result.Max, position);
    }

    ++result.VoxelCount;
  }

  if (!file.eof())
    throw std::runtime_error("VL32Loader: failed reading voxel record");

  if (result.VoxelCount > 0) {
    result.Size = glm::uvec3(result.Max - result.Min + 1);
  }

  return result;
}

} // namespace vxen
