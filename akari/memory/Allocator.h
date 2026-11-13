#pragma once

#include <cstdint>
#include <functional>
#include <vector>

namespace akari::memory {

struct Partition {
  uint32_t Id;
  size_t   Size;
};

struct Allocation {
  uint32_t Id {0};
  uint32_t Size {0};
  uint32_t Offset {0};
  bool     Resized {false};
};

class Allocator {
private:
  std::vector<uint32_t> m_Id {};
  std::vector<uint32_t> m_Size {};
  std::vector<uint32_t> m_Offset {};
  std::vector<bool>     m_Free {};

  uint32_t m_TotalMemory {0};

public:
  Allocation Allocate(uint32_t id, uint32_t bytes);

  Allocation GetAllocation(uint32_t id) const;

  std::vector<Allocation> GetAllocations() const;

  std::vector<Allocation> GetAllocations(const std::function<bool(uint32_t id)>& resized) const;

  uint32_t TotalMemory() const;

  Allocation Check(uint32_t id, uint32_t bytes) const;

  void Clear();
};

} // namespace akari::memory
