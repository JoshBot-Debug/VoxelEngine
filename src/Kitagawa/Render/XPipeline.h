#pragma once

#include <functional>
#include <vector>
#include <vulkan/vulkan.h>

namespace Kitagawa {
namespace Render {

class XPipeline {
private:
  std::vector<std::vector<VkDescriptorSet>> m_DescriptorSets       = {};
  std::vector<VkDescriptorSetLayout>        m_DescriptorSetLayouts = {};

  struct DescriptorSet {
    VkDescriptorPool                                                descriptorPool;
    std::vector<VkDescriptorSetLayoutBinding>                       layoutBindings;
    uint32_t                                                        descriptorSetCount;
    std::function<VkDescriptorBufferInfo(int descriptorCountIndex)> pBufferInfo;
    std::vector<VkDescriptorPoolSize>&                              poolSize;
  };

public:
  ~XPipeline();

  void CreateDescriptorSetLayout(const std::vector<std::vector<VkDescriptorSetLayoutBinding>>& bindings);

  void CreateDescriptorSets(VkDescriptorPool descriptorPool);

  void CreateDescriptorSet(const DescriptorSet& set);
};

} // namespace Render
} // namespace Kitagawa
