#pragma once

#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.h>

namespace Akari {
namespace Render {

class Pipeline {
public:
  struct DescriptorWriteInfo {
    VkDescriptorType                      type;
    uint32_t                              binding;
    std::optional<VkDescriptorBufferInfo> buffer;
    std::optional<VkDescriptorImageInfo>  image;
  };

  struct DescriptorSetInfo {
    uint32_t                         id;
    uint32_t                         layoutIndex;
    VkDescriptorPool                 descriptorPool;
    uint32_t                         descriptorSetCount;
    std::vector<DescriptorWriteInfo> writes;
  };

  struct DescriptorSetLayoutInfo {
    uint32_t                                  index;
    std::vector<VkDescriptorSetLayoutBinding> layoutBindings;
  };

  struct PipelineInfo {
    VkPrimitiveTopology                              topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    uint32_t                                         vertexStride;
    std::vector<VkVertexInputAttributeDescription>   attribs;
    VkRenderPass                                     renderPass;
    std::string                                      vertexShaderFile;
    std::string                                      fragmentShaderFile;
    std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments;
    VkBool32                                         depthTestEnable;
    VkBool32                                         depthWriteEnable;
  };

  struct ComputePipelineInfo {
    std::string computeShaderFile;
  };

  struct DrawInfo {
    VkCommandBuffer              commandBuffer;
    VkBuffer                     vertexBuffer;
    std::vector<VkDeviceSize>    offsets;
    std::vector<VkDescriptorSet> descriptorSets;
    uint32_t                     vertexCount;
  };

  struct DispatchComputeInfo {
    VkCommandBuffer              commandBuffer;
    uint32_t                     groupCountX = 1;
    uint32_t                     groupCountY = 1;
    uint32_t                     groupCountZ = 1;
    std::vector<VkDescriptorSet> descriptorSets;
  };

private:
  std::vector<VkDescriptorSetLayout> m_DescriptorSetLayouts = {};
  VkPipelineLayout                   m_PipelineLayout       = nullptr;
  VkPipeline                         m_Pipeline             = nullptr;

  std::unordered_map<uint32_t, std::vector<VkDescriptorSet>> m_DescriptorSets = {};

  VkShaderModule CreateShaderModule(const std::string& filename);

public:
  ~Pipeline();

  void CreateDescriptorSetLayout(const DescriptorSetLayoutInfo& info);

  void CreateDescriptorSet(const DescriptorSetInfo& info);

  void CreatePipeline(const PipelineInfo& info);

  void CreateComputePipeline(const ComputePipelineInfo& info);

  void DispatchCompute(const DispatchComputeInfo& info);

  void Draw(const DrawInfo& info);

  VkDescriptorSet GetDescriptorSet(uint32_t id, uint32_t index);
};

} // namespace Render
} // namespace Akari
