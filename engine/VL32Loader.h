
#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <glm/glm.hpp>

namespace vxen {

class VL32Loader {
public:
  struct RGBA {
    uint8_t R, G, B, A;

    bool operator==(const RGBA&) const = default;
  };

  struct Result {
    uint64_t   VoxelCount    = 0;
    uint64_t   MaterialCount = 0;
    glm::ivec3 Min {};
    glm::ivec3 Max {};
    glm::uvec3 Size {};
  };

  // Converts an RGBA color to your engine's material ID.
  using MaterialResolver = std::function<uint32_t(const RGBA&)>;

  using CreateVoxel = std::function<void(uint32_t x, uint32_t y, uint32_t z, uint32_t id)>;

  VL32Loader(uint8_t colorBits, MaterialResolver materialResolver, CreateVoxel createVoxel);

  Result Load(const std::filesystem::path& path);

private:
  MaterialResolver m_MaterialResolver;
  CreateVoxel      m_CreateVoxel;
  uint8_t          m_ColorBits {8};

  static uint32_t ReadU32BE(const uint8_t* data);
  static int32_t  ReadI32BE(const uint8_t* data);
};

} // namespace vxen
