#pragma once

#include <functional>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "imgui.h"
#include "vulkan/vulkan.h"

#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "Debug.h"
#include "Layer.h"

#include "vk_mem_alloc.h"

namespace akari::window {

struct ApplicationSpecification {
  std::string Name   = "akari";
  uint32_t    Width  = 1600;
  uint32_t    Height = 900;

  bool EnableKeyboardNavigation = true;
  bool EnableDocking            = false;

  bool DarkMode  = true;
  bool Maximized = false;
  bool Centered  = true;

  float FontSize = 20.0f;
};

class Application {

private:
  struct CommandBufferSpecification {
    bool     Begin = true;
    uint32_t Count = 1;
  };

private:
  ApplicationSpecification m_Specification;
  GLFWwindow*              m_WindowHandle = nullptr;
  bool                     m_Running      = false;

  float m_TimeStep      = 0.0f;
  float m_FrameTime     = 0.0f;
  float m_LastFrameTime = 0.0f;

  std::vector<std::shared_ptr<Layer>> m_LayerStack;
  std::function<void()>               m_MenubarCallback;

private:
  void Init();
  void Shutdown();

public:
  enum FontType {
    Font_Regular = 0,
    Font_Bold,
    Font_Medium,
    Font_Light,
    Font_LightItalic
  };

public:
  Application(const ApplicationSpecification& applicationSpecification =
                  ApplicationSpecification());
  ~Application();

  static Application& Get();

  void Run();

  void SetMenubarCallback(const std::function<void()>& menubarCallback) {
    m_MenubarCallback = menubarCallback;
  }

  template <typename T>
  void PushLayer() {
    static_assert(std::is_base_of<Layer, T>::value,
                  "Pushed type is not subclass of Layer!");
    m_LayerStack.emplace_back(std::make_shared<T>())->OnAttach();
  }

  void PushLayer(const std::shared_ptr<Layer>& layer) {
    m_LayerStack.emplace_back(layer)->OnAttach();
  }

  void Close();

  float       GetTime();
  GLFWwindow* GetWindowHandle() const { return m_WindowHandle; }

  static VkInstance       GetInstance();
  static VkPhysicalDevice GetPhysicalDevice();
  static VkDevice         GetDevice();
  static VkQueue          GetGraphicsQueue();

  static VkCommandBuffer
                       GetCommandBuffer(const CommandBufferSpecification& specification);
  static VkCommandPool GetCommandPool();
  static void          FlushCommandBuffer(VkCommandBuffer commandBuffer);

  static void SubmitResourceFree(std::function<void()>&& func);

  static VkSampleCountFlagBits GetMaxUsableSampleCount();
  static uint32_t              GetGraphicsQueueFamilyIndex();
  static uint32_t              GetCurrentFrameIndex();
  static uint32_t              GetMaxFramesInFlight();
  static uint32_t              GetFrameCount();
  static VmaAllocator          GetVmaAllocator();

  static ImFont*                            GetFont(FontType type);
  static std::vector<VkExtensionProperties> GetSupportedDeviceExtensions();
  static std::vector<VkExtensionProperties> GetSupportedInstanceExtensions();
  static uint32_t                           FindMemoryType(uint32_t              typeFilter,
                                                           VkMemoryPropertyFlags properties);
};

// Implemented by CLIENT
Application* CreateApplication(int argc, char** argv);

} // namespace akari::window

template <typename... Args>
inline void CheckVkResult(VkResult err, const Args&... args) {
  if (err == 0)
    return;

  std::ostringstream oss;
  oss << "VkResult " << VkResultToString(err);
  ((oss << " " << args), ...);

  if (err < 0)
    throw std::runtime_error(oss.str());
}