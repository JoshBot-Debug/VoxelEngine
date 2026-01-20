#include "CameraBuffer.h"

#include "Application.h"

namespace Kitagawa {
namespace Render {

CameraBuffer::CameraBuffer() {
  VkDeviceSize bufferSize = sizeof(Camera);

  const uint32_t framesInFlight = Akari::Application::GetMaxFramesInFlight();

  m_Mapped.resize(framesInFlight);
  m_Buffers.resize(framesInFlight);
  m_Allocations.resize(framesInFlight);

  for (size_t i = 0; i < framesInFlight; i++) {

    VkBufferCreateInfo bufferInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = bufferSize,
        .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
    };

    VmaAllocationCreateInfo allocCreateInfo = {
        .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                 VMA_ALLOCATION_CREATE_MAPPED_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO,
    };

    VmaAllocationInfo allocInfo;

    vmaCreateBuffer(Akari::Application::GetVmaAllocator(), &bufferInfo,
                    &allocCreateInfo, &m_Buffers[i], &m_Allocations[i],
                    &allocInfo);

    memset(allocInfo.pMappedData, 0, bufferSize);

    m_Mapped[i] = allocInfo.pMappedData;
  }
}

CameraBuffer::~CameraBuffer() {
  VmaAllocator allocator = Akari::Application::GetVmaAllocator();
  const uint32_t framesInFlight = Akari::Application::GetMaxFramesInFlight();

  for (size_t i = 0; i < framesInFlight; i++) {
    vmaDestroyBuffer(allocator, m_Buffers[i], m_Allocations[i]);
  }
}

void CameraBuffer::Render(PerspectiveCamera *camera) {

  Camera data{
      .view = camera->GetViewMatrix(),
      .projection = camera->GetProjectionMatrix(),
      .viewProjection = camera->GetViewProjectionMatrix(),
      .inverseView = camera->GetInverseViewMatrix(),
      .inverseProjection = camera->GetInverseProjectionMatrix(),
      .inverseViewProjection = camera->GetInverseViewProjectionMatrix(),

      .pView = camera->GetPreviousViewMatrix(),
      .pProjection = camera->GetPreviousProjectionMatrix(),
      .pViewProjection = camera->GetPreviousViewProjectionMatrix(),
      .pInverseView = camera->GetPreviousInverseViewMatrix(),
      .pInverseProjection = camera->GetPreviousInverseProjectionMatrix(),
      .pInverseViewProjection = camera->GetPreviousInverseViewProjectionMatrix(),

      .position = camera->Position,
      .farPlane = camera->FarPlane,
  };

  memcpy(m_Mapped[Akari::Application::GetCurrentFrameIndex()], &data,
         sizeof(Camera));
}

} // namespace Render
} // namespace Kitagawa
