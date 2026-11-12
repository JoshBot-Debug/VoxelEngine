#include "VoxLoader.h"

#include <algorithm>
#include <array>
#include <climits>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
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

enum class NodeType { Transform,
                      Group,
                      Shape };

struct SceneNode {
  NodeType             type    = NodeType::Transform;
  int32_t              childId = -1;
  glm::ivec3           translation {0, 0, 0};
  std::vector<int32_t> childrenIds;
  std::vector<int32_t> modelIds;
};

struct ModelInstance {
  int32_t    modelId;
  glm::ivec3 worldTranslation;
};

struct Lab {
  float L, a, b;
};
} // namespace vox_loader

namespace {

vox_loader::Lab RGBToLab(uint8_t r, uint8_t g, uint8_t b) {
  auto Linearize = [](float c) {
    c /= 255.0f;
    return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);
  };

  float R = Linearize(r);
  float G = Linearize(g);
  float B = Linearize(b);

  float X = (R * 0.4124564f + G * 0.3575761f + B * 0.1804375f) / 0.95047f;
  float Y = (R * 0.2126729f + G * 0.7151522f + B * 0.0721750f);
  float Z = (R * 0.0193339f + G * 0.1191920f + B * 0.9503041f) / 1.08883f;

  auto F = [](float t) {
    constexpr float delta = 6.0f / 29.0f;
    return t > delta * delta * delta ? std::cbrt(t) : t / (3.0f * delta * delta) + 4.0f / 29.0f;
  };

  float fx = F(X);
  float fy = F(Y);
  float fz = F(Z);

  return {
      116.0f * fy - 16.0f,
      500.0f * (fx - fy),
      200.0f * (fy - fz)};
}

float DeltaE(const vox_loader::Lab& a, const vox_loader::Lab& b) {
  float dL = a.L - b.L;
  float da = a.a - b.a;
  float db = a.b - b.b;

  return std::sqrt(dL * dL + da * da + db * db);
}

uint32_t ReadU32(std::istream& stream) {
  uint32_t value = 0;
  stream.read(reinterpret_cast<char*>(&value), 4);
  return value;
}

int32_t ReadI32(std::istream& stream) {
  int32_t value = 0;
  stream.read(reinterpret_cast<char*>(&value), 4);
  return value;
}

std::string ReadString(std::istream& stream) {
  uint32_t    len = ReadU32(stream);
  std::string str(len, '\0');
  if (len > 0) {
    stream.read(str.data(), len);
  }
  return str;
}

std::unordered_map<std::string, std::string> ReadDict(std::istream& stream) {
  uint32_t                                     count = ReadU32(stream);
  std::unordered_map<std::string, std::string> dict;
  for (uint32_t i = 0; i < count; ++i) {
    std::string key = ReadString(stream);
    std::string val = ReadString(stream);
    dict.emplace(std::move(key), std::move(val));
  }
  return dict;
}

glm::ivec3 ParseTranslation(const std::string& str) {
  glm::ivec3         t {0, 0, 0};
  std::istringstream iss(str);
  iss >> t.x >> t.y >> t.z;
  return t;
}

void ReadChunkHeader(std::istream& stream, vox_loader::Chunk& chunk) {
  stream.read(chunk.id.data(), 4);
  chunk.contentSize  = ReadU32(stream);
  chunk.childrenSize = ReadU32(stream);
}

void TraverseSceneGraph(
    int32_t                                                   nodeId,
    glm::ivec3                                                accumulatedTranslation,
    const std::unordered_map<int32_t, vox_loader::SceneNode>& nodes,
    std::vector<vox_loader::ModelInstance>&                   instances) {
  auto it = nodes.find(nodeId);
  if (it == nodes.end()) {
    return;
  }

  const auto& node = it->second;
  if (node.type == vox_loader::NodeType::Transform) {
    TraverseSceneGraph(
        node.childId,
        accumulatedTranslation + node.translation,
        nodes,
        instances);
  } else if (node.type == vox_loader::NodeType::Group) {
    for (int32_t child : node.childrenIds) {
      TraverseSceneGraph(child, accumulatedTranslation, nodes, instances);
    }
  } else if (node.type == vox_loader::NodeType::Shape) {
    for (int32_t modelId : node.modelIds) {
      instances.push_back({modelId, accumulatedTranslation});
    }
  }
}

} // namespace

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

  vox_loader::Chunk mainChunk;
  ReadChunkHeader(file, mainChunk);

  std::vector<vox_loader::Model> models;
  vox_loader::Model              currentModel {};
  bool                           hasSize   = false;
  bool                           hasVoxels = false;

  std::unordered_map<int32_t, vox_loader::SceneNode> nodes;
  std::array<uint8_t, 256 * 4>                       paletteRGBA {};

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
    } else if (std::memcmp(chunk.id.data(), "nTRN", 4) == 0) {
      int32_t nodeId = ReadI32(file);
      ReadDict(file); // node attributes
      int32_t  childId    = ReadI32(file);
      int32_t  reservedId = ReadI32(file);
      int32_t  layerId    = ReadI32(file);
      uint32_t numFrames  = ReadU32(file);
      (void)reservedId;
      (void)layerId;

      glm::ivec3 translation {0, 0, 0};
      for (uint32_t f = 0; f < numFrames; ++f) {
        auto frameDict = ReadDict(file);
        if (f == 0) {
          auto it = frameDict.find("_t");
          if (it != frameDict.end()) {
            translation = ParseTranslation(it->second);
          }
        }
      }

      vox_loader::SceneNode node;
      node.type        = vox_loader::NodeType::Transform;
      node.childId     = childId;
      node.translation = translation;
      nodes[nodeId]    = std::move(node);
    } else if (std::memcmp(chunk.id.data(), "nGRP", 4) == 0) {
      int32_t nodeId = ReadI32(file);
      ReadDict(file); // node attributes
      uint32_t numChildren = ReadU32(file);

      vox_loader::SceneNode node;
      node.type = vox_loader::NodeType::Group;
      node.childrenIds.resize(numChildren);
      for (uint32_t i = 0; i < numChildren; ++i) {
        node.childrenIds[i] = ReadI32(file);
      }
      nodes[nodeId] = std::move(node);
    } else if (std::memcmp(chunk.id.data(), "nSHP", 4) == 0) {
      int32_t nodeId = ReadI32(file);
      ReadDict(file); // node attributes
      uint32_t numModels = ReadU32(file);

      vox_loader::SceneNode node;
      node.type = vox_loader::NodeType::Shape;
      for (uint32_t i = 0; i < numModels; ++i) {
        int32_t modelId = ReadI32(file);
        ReadDict(file); // model attributes
        node.modelIds.push_back(modelId);
      }
      nodes[nodeId] = std::move(node);
    }

    file.seekg(
        chunkStart +
        static_cast<std::streamoff>(chunk.contentSize));

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

  // Resolve scene graph instances (fallback to single model at origin if no scene graph)
  std::vector<vox_loader::ModelInstance> instances;
  if (!nodes.empty()) {
    TraverseSceneGraph(0, {0, 0, 0}, nodes, instances);
  } else {
    for (int32_t i = 0; i < static_cast<int32_t>(models.size()); ++i) {
      instances.push_back({i, {0, 0, 0}});
    }
  }

  // Compute model corner origins and normalize minimum bounds to (0, 0, 0)
  struct PlacedInstance {
    int32_t    modelId;
    glm::ivec3 origin;
  };
  std::vector<PlacedInstance> placed;
  placed.reserve(instances.size());

  glm::ivec3 minBounds {INT_MAX, INT_MAX, INT_MAX};
  glm::ivec3 maxBounds {INT_MIN, INT_MIN, INT_MIN};

  for (const auto& inst : instances) {
    if (inst.modelId < 0 || inst.modelId >= static_cast<int32_t>(models.size())) {
      continue;
    }
    const auto& model = models[inst.modelId];
    glm::ivec3  halfSize(model.x / 2, model.y / 2, model.z / 2);
    glm::ivec3  origin = inst.worldTranslation - halfSize;
    glm::ivec3  extent = origin + glm::ivec3(model.x, model.y, model.z);

    minBounds = glm::min(minBounds, origin);
    maxBounds = glm::max(maxBounds, extent);
    placed.push_back({inst.modelId, origin});
  }

  if (placed.empty()) {
    minBounds = {0, 0, 0};
    maxBounds = {0, 0, 0};
  }

  std::array<vox_loader::Lab, 256> paletteLab {};

  for (uint32_t i = 0; i < 256; ++i) {
    paletteLab[i] = RGBToLab(
        paletteRGBA[i * 4 + 0],
        paletteRGBA[i * 4 + 1],
        paletteRGBA[i * 4 + 2]);
  }

  std::array<uint8_t, 256> paletteRemap {};

  for (uint32_t i = 0; i < 256; ++i)
    paletteRemap[i] = static_cast<uint8_t>(i);

  if (m_ColorMergeThreshold > 0.0f) {
    for (uint32_t i = 0; i < 256; ++i) {
      for (uint32_t j = 0; j < i; ++j) {

        if (DeltaE(paletteLab[i], paletteLab[j]) <=
            m_ColorMergeThreshold) {

          paletteRemap[i] = paletteRemap[j];
          break;
        }
      }
    }
  }

  // Create palette materials
  std::array<uint32_t, 256> materialIDs {};
  for (uint32_t i = 0; i < 256; ++i) {
    uint32_t representative = paletteRemap[i];

    if (representative != i) {
      materialIDs[i] = materialIDs[representative];
      continue;
    }

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

    materialIDs[i] = m_Palette->Create(
        Palette::Item {
            .Name = "VOX_" + std::to_string(i + 1),
            .Mat  = material});
  }

  // Populate chunk manager with shifted positive coordinates
  for (const auto& inst : placed) {
    const auto& model  = models[inst.modelId];
    glm::ivec3  offset = inst.origin - minBounds;

    for (const vox_loader::Voxel& voxel : model.voxels) {
      if (voxel.color == 0) {
        continue;
      }

      uint32_t materialID = materialIDs[voxel.color - 1];
      m_ChunkManager->Set(
          offset.x + voxel.x,
          offset.y + voxel.y,
          offset.z + voxel.z,
          materialID);
    }
  }

  glm::ivec3 worldSize = maxBounds - minBounds;
  std::cout << "VoxLoader: World size "
            << worldSize.x << "x"
            << worldSize.y << "x"
            << worldSize.z << '\n';
  std::cout << "VoxLoader: " << m_Palette->GetMaterials().size() << " Materials created\n";
  std::cout << "VoxLoader: Loaded " << placed.size() << " model instance(s)\n";

  return true;
}

} // namespace vxen