#pragma once

#include <memory>
#include <typeindex>
#include <unordered_map>

namespace ECS {
class System {
public:
  virtual ~System()               = default;
  virtual void Update(void* data) = 0;
  virtual void Free() {};
};

class SystemManager {
public:
  std::unordered_map<std::type_index, std::shared_ptr<System>> m_Systems;

  template <typename T, typename... Args>
  std::shared_ptr<T> Register(Args&&... args) {
    auto system = std::make_shared<T>(std::forward<Args>(args)...);
    m_Systems.emplace(typeid(T), system);
    return system;
  }

  template <typename T>
  void Update(void* data = nullptr) {
    m_Systems.at(typeid(T))->Update(data);
  }

  void Free(void* data = nullptr) {
    for (auto [_, system] : m_Systems)
      system->Free();
  }
};
} // namespace ECS
