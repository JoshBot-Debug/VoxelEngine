#pragma once

#include <mutex>
#include <memory>
#include <unordered_map>
#include <shared_mutex>
#include <glm/glm.hpp>

// #define GLM_ENABLE_EXPERIMENTAL
// #include <glm/gtx/hash.hpp>

namespace std {
template <>
struct hash<glm::ivec3> {
    std::size_t operator()(const glm::ivec3& v) const noexcept {
        size_t h1 = std::hash<int>()(v.x);
        size_t h2 = std::hash<int>()(v.y);
        size_t h3 = std::hash<int>()(v.z);
        return h1 ^ (h2 << 1) ^ (h3 << 2);
    }
};
}

class IVecMutex {
private:
  std::unordered_map<glm::ivec3, std::shared_ptr<std::shared_mutex>> m_Pool;
  std::shared_mutex m_Mutex;

public:
  std::shared_mutex &Get(const glm::ivec3 &key);
  void Remove(const glm::ivec3 &key);
};