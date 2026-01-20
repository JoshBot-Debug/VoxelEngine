#include "IVecMutex.h"

std::shared_mutex &IVecMutex::Get(const glm::ivec3 &key) {
  {
    std::shared_lock<std::shared_mutex> readLock(m_Mutex);
    auto it = m_Pool.find(key);
    if (it != m_Pool.end())
      return *it->second;
  }

  std::unique_lock<std::shared_mutex> writeLock(m_Mutex);
  auto &mtxPtr = m_Pool[key];
  if (!mtxPtr) {
    mtxPtr = std::make_unique<std::shared_mutex>();
  }
  return *mtxPtr;
}

void IVecMutex::Remove(const glm::ivec3 &key) {
  std::shared_lock<std::shared_mutex> readLock(m_Mutex);
  auto it = m_Pool.find(key);
  if (it != m_Pool.end())
    if (it->second.use_count() == 1)
      m_Pool.erase(key);
}
