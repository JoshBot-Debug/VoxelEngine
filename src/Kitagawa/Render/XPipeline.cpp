#include "XPipeline.h"

#include "Application.h"

namespace Kitagawa {
namespace Render {

XPipeline::~XPipeline() {
}

void XPipeline::CreateDescriptorSetLayout(const std::vector<std::vector<VkDescriptorSetLayoutBinding>>& bindings) {
  VkDevice device = Akari::Application::GetDevice();

  m_DescriptorSetLayouts.resize(bindings.size());

  for (size_t i = 0; i < bindings.size(); i++) {

    const std::vector<VkDescriptorSetLayoutBinding>& binding = bindings[i];

    VkDescriptorSetLayoutCreateInfo layoutInfo{
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<uint32_t>(binding.size()),
        .pBindings    = binding.data(),
    };

    VkResult result = vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &m_DescriptorSetLayouts[i]);
    if (result != VK_SUCCESS)
      throw std::runtime_error("Failed to create descriptor set layout " + std::to_string(i));
  }
}

void XPipeline::CreateDescriptorSets(VkDescriptorPool descriptorPool) {

  m_DescriptorSets.resize(m_DescriptorSetLayouts.size());
}

void XPipeline::CreateDescriptorSet(const DescriptorSet& set) {
  VkDevice device = Akari::Application::GetDevice();

  VkDescriptorSetLayout&       layout = m_DescriptorSetLayouts.emplace_back();
  std::vector<VkDescriptorSet> descriptorSets(set.descriptorSetCount);

  // Create the descriptor set layout
  {
    VkDescriptorSetLayoutCreateInfo layoutInfo{
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<uint32_t>(set.layoutBindings.size()),
        .pBindings    = set.layoutBindings.data(),
    };

    VkResult result = vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &layout);
    if (result != VK_SUCCESS)
      throw std::runtime_error("Failed to create descriptor set layout");
  }

  // Allocate the descriptor set
  {
    std::vector<VkDescriptorSetLayout> layouts(set.descriptorSetCount, layout);

    VkDescriptorSetAllocateInfo allocInfo{
        .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool     = set.descriptorPool,
        .descriptorSetCount = set.descriptorSetCount,
        .pSetLayouts        = layouts.data(),
    };

    VkResult result = vkAllocateDescriptorSets(device, &allocInfo, descriptorSets.data());

    if (result != VK_SUCCESS)
      throw std::runtime_error("Failed to allocate descriptor sets 1");
  }

  // Update descriptor sets
  {
    for (size_t i = 0; i < set.descriptorSetCount; i++) {
      std::vector<VkWriteDescriptorSet> writes;

      for (size_t j = 0; j < set.layoutBindings.size(); j++) {
        VkDescriptorBufferInfo pBufferInfo = set.pBufferInfo(i);

        writes.emplace_back(VkWriteDescriptorSet{
            .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet          = descriptorSets[i],
            .dstBinding      = set.layoutBindings[j].binding,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType  = set.layoutBindings[j].descriptorType,
            .pBufferInfo     = &pBufferInfo,
        });
      }

      vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }
  }

  // Capture the descriptor set pool size
  {
    for (size_t i = 0; i < set.layoutBindings.size(); i++)
      set.poolSize.push_back(VkDescriptorPoolSize{.type = set.layoutBindings[i].descriptorType, .descriptorCount = set.descriptorSetCount});
  }
}

} // namespace Render
} // namespace Kitagawa