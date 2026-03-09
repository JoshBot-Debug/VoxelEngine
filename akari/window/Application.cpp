#include "window/Application.h"
#include "window/Input.h"

#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"
#include <stdio.h>
#include <stdlib.h>
#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <vulkan/vk_layer.h>
#include <vulkan/vulkan.h>

#include <iostream>

// Emedded font
#include "imgui/RobotoBold.embed"
#include "imgui/RobotoLight.embed"
#include "imgui/RobotoLightItalic.embed"
#include "imgui/RobotoMedium.embed"
#include "imgui/RobotoRegular.embed"

// Vulkan Memory Allocator
#define VMA_IMPLEMENTATION
#define VMA_VULKAN_VERSION 1003000 // Vulkan 1.3
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#include "Application.h"
#include "vk_mem_alloc.h"

extern bool g_ApplicationRunning;

#define IMGUI_UNLIMITED_FRAME_RATE

static uint32_t VULKAN_API_VERSION = VK_API_VERSION_1_3;

static VkSurfaceKHR           g_Surface        = NULL;
static VkAllocationCallbacks* g_Allocator      = NULL;
static VkInstance             g_Instance       = VK_NULL_HANDLE;
static VkPhysicalDevice       g_PhysicalDevice = VK_NULL_HANDLE;
static VkDevice               g_Device         = VK_NULL_HANDLE;
static uint32_t               g_QueueFamily    = (uint32_t)-1;
static VkQueue                g_Queue          = VK_NULL_HANDLE;
static VkPipelineCache        g_PipelineCache  = VK_NULL_HANDLE;
static VkDescriptorPool       g_DescriptorPool = VK_NULL_HANDLE;
static VmaAllocator           g_VmaAllocator;

static ImGui_ImplVulkanH_Window g_MainWindowData;
static uint32_t                 g_MinImageCount    = 2;
static bool                     g_SwapChainRebuild = false;

// Per-frame-in-flight
static std::vector<std::vector<VkCommandBuffer>> s_AllocatedCommandBuffers;
static Defer                                     s_Defer;

static uint32_t g_FrameIndex = 0;

static akari::window::Application* s_Instance = nullptr;
static std::vector<ImFont*>        s_Fonts;

#ifdef DEBUG

static VkDebugUtilsMessengerEXT g_DebugMessenger;

inline VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT      severity,
    VkDebugUtilsMessageTypeFlagsEXT             type,
    const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
    void*                                       userData) {

  if (strstr(
          callbackData->pMessageIdName,
          "UNASSIGNED-BestPractices-vkBindMemory-small-dedicated-allocation") ||
      strstr(callbackData->pMessageIdName,
             "UNASSIGNED-BestPractices-vkAllocateMemory-small-allocation") ||
      strstr(callbackData->pMessageIdName,
             "UNASSIGNED-BestPractices-Verbose-Success-Logging")) {
    return VK_FALSE; // Ignore this message
  }

  std::cerr << callbackData->pMessage << std::endl;
  return VK_FALSE;
}

inline void SetupDebugMessenger(VkInstance instance) {
  VkDebugUtilsMessengerCreateInfoEXT createInfo {};
  createInfo.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
  createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
  createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  createInfo.pfnUserCallback = DebugCallback;

  auto vkCreateDebugUtilsMessengerEXT =
      (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
          instance,
          "vkCreateDebugUtilsMessengerEXT");

  if (!vkCreateDebugUtilsMessengerEXT)
    throw std::runtime_error("Failed to load vkCreateDebugUtilsMessengerEXT");

  if (vkCreateDebugUtilsMessengerEXT(instance, &createInfo, g_Allocator, &g_DebugMessenger) != VK_SUCCESS)
    throw std::runtime_error("Failed to create debug messenger!");
}
#endif

static void SetupVulkan(std::vector<const char*>& instanceExtensions) {
  VkResult err = VK_SUCCESS;

  // Create Vulkan Instance
  {
    VkApplicationInfo appInfo {};
    appInfo.sType         = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pEngineName   = "akari";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion    = VULKAN_API_VERSION;

    VkInstanceCreateInfo createInfo {};
    createInfo.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo        = &appInfo;
    createInfo.enabledExtensionCount   = static_cast<uint32_t>(instanceExtensions.size());
    createInfo.ppEnabledExtensionNames = instanceExtensions.data();

#ifdef DEBUG
#ifdef ENABLE_VULKAN_VALIDATION
    std::vector<VkValidationFeatureEnableEXT> enables {
        VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT,
        VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT,
        VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_RESERVE_BINDING_SLOT_EXT,
        VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT,
    };

    VkValidationFeaturesEXT validationFeatures {};
    validationFeatures.sType                          = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
    validationFeatures.enabledValidationFeatureCount  = (uint32_t)enables.size();
    validationFeatures.pEnabledValidationFeatures     = enables.data();
    validationFeatures.disabledValidationFeatureCount = 0;
    validationFeatures.pDisabledValidationFeatures    = nullptr;

    createInfo.pNext = &validationFeatures;

    // Enabling validation layers
    const char* layers[]           = {"VK_LAYER_KHRONOS_validation"};
    createInfo.enabledLayerCount   = 1;
    createInfo.ppEnabledLayerNames = layers;

    // Enable debug utils extension
    instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    createInfo.enabledExtensionCount   = static_cast<uint32_t>(instanceExtensions.size()),
    createInfo.ppEnabledExtensionNames = instanceExtensions.data();

    // Create Vulkan Instance
    err = vkCreateInstance(&createInfo, g_Allocator, &g_Instance);
    CheckVkResult(err, "Failed to at vkCreateInstance");

    SetupDebugMessenger(g_Instance);
#else
    // Create Vulkan Instance without any debug feature
    err = vkCreateInstance(&createInfo, g_Allocator, &g_Instance);
    CheckVkResult(err, "Failed to at vkCreateInstance");
#endif
#endif
  }

  // Select GPU
  {
    uint32_t GPUCount = 0;
    err               = vkEnumeratePhysicalDevices(g_Instance, &GPUCount, NULL);
    CheckVkResult(err, "Failed to at vkEnumeratePhysicalDevices");
    IM_ASSERT(GPUCount > 0);

    VkPhysicalDevice* gpus =
        (VkPhysicalDevice*)malloc(sizeof(VkPhysicalDevice) * GPUCount);
    err = vkEnumeratePhysicalDevices(g_Instance, &GPUCount, gpus);
    CheckVkResult(err, "Failed to at vkEnumeratePhysicalDevices");

    // If a number >1 of GPUs got reported, find discrete GPU if present, or use
    // first one available. This covers most common cases
    // (multi-gpu/integrated+dedicated graphics).
    int useGPU = 0;
    for (int i = 0; i < (int)GPUCount; i++) {
      VkPhysicalDeviceProperties properties;
      vkGetPhysicalDeviceProperties(gpus[i], &properties);

#ifdef DEBUG
      printf("GPU %d:\n", i);
      printf("  Name: %s\n", properties.deviceName);
      printf("  Vendor ID: 0x%X\n", properties.vendorID);
      printf("  Device ID: 0x%X\n", properties.deviceID);
      std::cout << "  Driver Version: "
                << VK_VERSION_MAJOR(properties.driverVersion) << "."
                << VK_VERSION_MINOR(properties.driverVersion) << "."
                << VK_VERSION_PATCH(properties.driverVersion) << "\n";
      std::cout << "  Vulkan API Version: "
                << VK_VERSION_MAJOR(properties.apiVersion) << "."
                << VK_VERSION_MINOR(properties.apiVersion) << "."
                << VK_VERSION_PATCH(properties.apiVersion) << "\n\n";
#endif

      if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        useGPU = i;
        break;
      }
    }

    g_PhysicalDevice = gpus[useGPU];

    free(gpus);
  }

  // Select graphics queue family
  {
    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(g_PhysicalDevice, &count, NULL);
    VkQueueFamilyProperties* queues = (VkQueueFamilyProperties*)malloc(sizeof(VkQueueFamilyProperties) * count);
    vkGetPhysicalDeviceQueueFamilyProperties(g_PhysicalDevice, &count, queues);
    for (uint32_t i = 0; i < count; i++)
      if (queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
        g_QueueFamily = i;
        break;
      }
    free(queues);
    IM_ASSERT(g_QueueFamily != (uint32_t)-1);
  }

  // Create Logical Device (with 1 queue)
  {
    std::vector<const char*> deviceExtensions {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };

    // Enable features
    VkPhysicalDeviceBufferDeviceAddressFeatures bufferDevAddrFeat {};
    bufferDevAddrFeat.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;

    VkPhysicalDeviceSynchronization2FeaturesKHR sync2 {};
    sync2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;
    sync2.pNext = &bufferDevAddrFeat;

    VkPhysicalDeviceFeatures2 features2 {};
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features2.pNext = &sync2;

    vkGetPhysicalDeviceFeatures2(g_PhysicalDevice, &features2);

    features2.features.fillModeNonSolid = VK_TRUE;
    features2.features.wideLines        = VK_TRUE;
    features2.features.independentBlend = VK_TRUE;

    const float pQueuePriorities[] = {1.0f};

    VkDeviceQueueCreateInfo qci {};
    qci.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    qci.queueFamilyIndex = g_QueueFamily;
    qci.queueCount       = 1;
    qci.pQueuePriorities = pQueuePriorities;

    std::vector<VkDeviceQueueCreateInfo> queueInfo = {qci};

    VkDeviceCreateInfo deviceInfo {};
    deviceInfo.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.pNext                   = &features2;
    deviceInfo.queueCreateInfoCount    = (uint32_t)queueInfo.size();
    deviceInfo.pQueueCreateInfos       = queueInfo.data();
    deviceInfo.enabledExtensionCount   = (uint32_t)deviceExtensions.size();
    deviceInfo.ppEnabledExtensionNames = deviceExtensions.data();

    err = vkCreateDevice(g_PhysicalDevice, &deviceInfo, g_Allocator, &g_Device);
    CheckVkResult(err, "Failed at vkCreateDevice");

    vkGetDeviceQueue(g_Device, g_QueueFamily, 0, &g_Queue);
  }

  // Create Descriptor Pool
  {
    VkDescriptorPoolSize poolSize[] = {
        {VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000},
    };
    VkDescriptorPoolCreateInfo poolInfo {};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets       = 1000 * IM_ARRAYSIZE(poolSize);
    poolInfo.poolSizeCount = (uint32_t)IM_ARRAYSIZE(poolSize);
    poolInfo.pPoolSizes    = poolSize;

    err = vkCreateDescriptorPool(g_Device, &poolInfo, g_Allocator, &g_DescriptorPool);
    CheckVkResult(err, "Failed to at vkCreateDescriptorPool");
  }
}

// All the ImGui_ImplVulkanH_XXX structures/functions are optional helpers used
// by the demo. Your real engine/app may not use them.
static void SetupVulkanWindow(ImGui_ImplVulkanH_Window* wd,
                              VkSurfaceKHR              surface,
                              int                       width,
                              int                       height) {
  wd->Surface = surface;

  // Check for WSI support
  VkBool32 res;
  vkGetPhysicalDeviceSurfaceSupportKHR(g_PhysicalDevice, g_QueueFamily, wd->Surface, &res);
  if (res != VK_TRUE) {
    fprintf(stderr, "Error no WSI support on physical device 0\n");
    exit(-1);
  }

  // Select Surface Format
  const VkFormat requestSurfaceImageFormat[] = {
      VK_FORMAT_B8G8R8A8_UNORM,
      VK_FORMAT_R8G8B8A8_UNORM,
      VK_FORMAT_B8G8R8_UNORM,
      VK_FORMAT_R8G8B8_UNORM,
  };

  const VkColorSpaceKHR requestSurfaceColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;

  wd->SurfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat(
      g_PhysicalDevice,
      wd->Surface,
      requestSurfaceImageFormat,
      (size_t)IM_ARRAYSIZE(requestSurfaceImageFormat),
      requestSurfaceColorSpace);

  // Get the number of presentation modes of the surface
  uint32_t presentModeCount = 0;
  CheckVkResult(vkGetPhysicalDeviceSurfacePresentModesKHR(
      g_PhysicalDevice,
      wd->Surface,
      &presentModeCount,
      nullptr));

  // Get the list of presentation modes of the surface
  std::vector<VkPresentModeKHR> presentModes(presentModeCount);
  CheckVkResult(vkGetPhysicalDeviceSurfacePresentModesKHR(
      g_PhysicalDevice,
      wd->Surface,
      &presentModeCount,
      presentModes.data()));

  VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;

  for (auto mode : presentModes)
    if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
      presentMode = mode;

  wd->PresentMode = ImGui_ImplVulkanH_SelectPresentMode(
      g_PhysicalDevice,
      wd->Surface,
      &presentMode,
      1);

  // Create SwapChain, RenderPass, Framebuffer, etc.
  IM_ASSERT(g_MinImageCount >= 2);
  ImGui_ImplVulkanH_CreateOrResizeWindow(
      g_Instance,
      g_PhysicalDevice,
      g_Device,
      wd,
      g_QueueFamily,
      g_Allocator,
      width,
      height,
      g_MinImageCount,
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
}

static void CleanupVulkan() {
  vkDestroyDescriptorPool(g_Device, g_DescriptorPool, g_Allocator);

#ifdef DEBUG
#ifdef ENABLE_VULKAN_VALIDATION
  auto vkDestroyDebugUtilsMessengerEXT =
      (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
          g_Instance,
          "vkDestroyDebugUtilsMessengerEXT");
  vkDestroyDebugUtilsMessengerEXT(g_Instance, g_DebugMessenger, g_Allocator);
#endif

#ifdef ENABLE_VMA_STATS_LOG
  char* statsString = nullptr;
  vmaBuildStatsString(g_VmaAllocator, &statsString, VK_TRUE);
  printf("VMA Stats:\n%s\n", statsString);
  vmaFreeStatsString(g_VmaAllocator, statsString);
#endif
#endif

  vmaDestroyAllocator(g_VmaAllocator);
  vkDestroySurfaceKHR(g_Instance, g_Surface, g_Allocator);
  vkDestroyDevice(g_Device, g_Allocator);
  vkDestroyInstance(g_Instance, g_Allocator);
}

static void CleanupVulkanWindow() {
  ImGui_ImplVulkanH_DestroyWindow(g_Instance, g_Device, &g_MainWindowData, g_Allocator);
}

static void FrameRender(ImGui_ImplVulkanH_Window* wd, ImDrawData* drawData) {
  VkResult err;

  ImGui_ImplVulkanH_FrameSemaphores* fs                      = &wd->FrameSemaphores[wd->SemaphoreIndex];
  VkSemaphore                        imageAcquiredSemaphore  = fs->ImageAcquiredSemaphore;
  VkSemaphore                        renderCompleteSemaphore = fs->RenderCompleteSemaphore;

  err = vkAcquireNextImageKHR(g_Device, wd->Swapchain, UINT64_MAX, imageAcquiredSemaphore, VK_NULL_HANDLE, &wd->FrameIndex);

  g_FrameIndex = wd->FrameIndex;

  if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR) {
    g_SwapChainRebuild = true;
    return;
  }

  CheckVkResult(err, "Failed to at vkAcquireNextImageKHR");

  ImGui_ImplVulkanH_Frame* fd = &wd->Frames[wd->FrameIndex];
  {
    err = vkWaitForFences(g_Device, 1, &fd->Fence, VK_TRUE, UINT64_MAX);
    CheckVkResult(err, "Failed to at vkWaitForFences");

    err = vkResetFences(g_Device, 1, &fd->Fence);
    CheckVkResult(err, "Failed to at vkResetFences");
  }

  // Free buffers after waiting on frame
  {
    for (auto& b : s_Defer.FreeBuffers[g_FrameIndex])
      vmaDestroyBuffer(g_VmaAllocator, b.buffer, b.allocation);
    s_Defer.FreeBuffers[g_FrameIndex].clear();
  }

  // Free resources in queue after waiting on frame
  {
    for (auto& func : s_Defer.FreeResources[g_FrameIndex])
      func();
    s_Defer.FreeResources[g_FrameIndex].clear();
  }

  {
    auto& allocatedCommandBuffers = s_AllocatedCommandBuffers[wd->FrameIndex];
    if (allocatedCommandBuffers.size() > 0) {
      vkFreeCommandBuffers(g_Device, fd->CommandPool, (uint32_t)allocatedCommandBuffers.size(), allocatedCommandBuffers.data());
      allocatedCommandBuffers.clear();
    }

    err = vkResetCommandPool(g_Device, fd->CommandPool, 0);
    CheckVkResult(err, "Failed to at vkResetCommandPool");
    VkCommandBufferBeginInfo info {};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    err = vkBeginCommandBuffer(fd->CommandBuffer, &info);
    CheckVkResult(err, "Failed to at vkBeginCommandBuffer");
  }

  {
    VkRenderPassBeginInfo info {};
    info.sType                    = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    info.renderPass               = wd->RenderPass;
    info.framebuffer              = fd->Framebuffer;
    info.renderArea.extent.width  = wd->Width;
    info.renderArea.extent.height = wd->Height;
    info.clearValueCount          = 1;
    info.pClearValues             = &wd->ClearValue;
    vkCmdBeginRenderPass(fd->CommandBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);
  }

  // Record dear imgui primitives into command buffer
  ImGui_ImplVulkan_RenderDrawData(drawData, fd->CommandBuffer);

  // Submit command buffer
  vkCmdEndRenderPass(fd->CommandBuffer);
  {
    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo info {};
    info.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    info.waitSemaphoreCount   = 1;
    info.pWaitSemaphores      = &imageAcquiredSemaphore;
    info.pWaitDstStageMask    = &waitStage;
    info.commandBufferCount   = 1;
    info.pCommandBuffers      = &fd->CommandBuffer;
    info.signalSemaphoreCount = 1;
    info.pSignalSemaphores    = &renderCompleteSemaphore;

    err = vkEndCommandBuffer(fd->CommandBuffer);
    CheckVkResult(err, "Failed to at vkEndCommandBuffer");

    err = vkQueueSubmit(g_Queue, 1, &info, fd->Fence);
    CheckVkResult(err, "Failed to at vkQueueSubmit");
  }
}

static void FramePresent(ImGui_ImplVulkanH_Window* wd) {
  if (g_SwapChainRebuild)
    return;

  VkSemaphore renderCompleteSemaphore = wd->FrameSemaphores[wd->SemaphoreIndex].RenderCompleteSemaphore;

  VkPresentInfoKHR info {};
  info.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  info.waitSemaphoreCount = 1;
  info.pWaitSemaphores    = &renderCompleteSemaphore;
  info.swapchainCount     = 1;
  info.pSwapchains        = &wd->Swapchain;
  info.pImageIndices      = &wd->FrameIndex;

  VkResult err = vkQueuePresentKHR(g_Queue, &info);
  if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR) {
    g_SwapChainRebuild = true;
    return;
  }

  CheckVkResult(err, "Failed to at vkQueuePresentKHR");

  wd->SemaphoreIndex = (wd->SemaphoreIndex + 1) % wd->SemaphoreCount;
}

static void GLFWCallbackError(int error, const char* description) {
  fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

namespace akari::window {

Application::Application(const ApplicationSpecification& specification)
    : m_Specification(specification) {
  s_Instance = this;

  Init();
}

Application::~Application() {
  Shutdown();

  s_Instance = nullptr;
}

Application& Application::Get() { return *s_Instance; }

void Application::Init() {
  // Setup GLFW window
  glfwSetErrorCallback(GLFWCallbackError);
  if (!glfwInit()) {
    std::cerr << "Could not initalize GLFW!\n";
    return;
  }

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

  m_WindowHandle = glfwCreateWindow(m_Specification.Width, m_Specification.Height, m_Specification.Name.c_str(), NULL, NULL);

  // Center the window
  if (m_Specification.Centered) {
    GLFWmonitor*       primary = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode    = glfwGetVideoMode(primary);
    int                windowWidth, windowHeight;
    glfwGetWindowSize(m_WindowHandle, &windowWidth, &windowHeight);
    glfwSetWindowPos(m_WindowHandle, (mode->width - windowWidth) / 2, (mode->height - windowHeight) / 2);
  }

  // Setup Vulkan
  if (!glfwVulkanSupported()) {
    std::cerr << "GLFW: Vulkan not supported!\n";
    return;
  }
  uint32_t     glfwExtensionCount = 0;
  const char** glfwExtensions =
      glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

  std::vector<const char*> instanceExtensions(
      glfwExtensions,
      glfwExtensions + glfwExtensionCount);

  SetupVulkan(instanceExtensions);

  // Create Window Surface
  VkResult err = glfwCreateWindowSurface(g_Instance, m_WindowHandle, g_Allocator, &g_Surface);
  CheckVkResult(err, "Failed to at glfwCreateWindowSurface");

  // Create Framebuffers
  int w, h;
  glfwGetFramebufferSize(m_WindowHandle, &w, &h);
  ImGui_ImplVulkanH_Window* wd = &g_MainWindowData;
  SetupVulkanWindow(wd, g_Surface, w, h);

  s_AllocatedCommandBuffers.resize(wd->ImageCount);
  s_Defer.FreeBuffers.resize(wd->ImageCount);
  s_Defer.FreeResources.resize(wd->ImageCount);

  // Setup Dear ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  (void)io;
  if (m_Specification.EnableKeyboardNavigation)
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  if (m_Specification.EnableDocking)
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

  // Setup Dear ImGui style
  if (m_Specification.DarkMode)
    ImGui::StyleColorsDark();
  else
    ImGui::StyleColorsLight();

  // When viewports are enabled we tweak WindowRounding/WindowBg so platform
  // windows can look identical to regular ones.
  ImGuiStyle& style = ImGui::GetStyle();
  if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
    style.WindowRounding              = 0.0f;
    style.Colors[ImGuiCol_WindowBg].w = 1.0f;
  }

  // Setup Platform/Renderer backends
  ImGui_ImplGlfw_InitForVulkan(m_WindowHandle, true);
  ImGui_ImplVulkan_InitInfo initInfo {};
  initInfo.Instance        = g_Instance;
  initInfo.PhysicalDevice  = g_PhysicalDevice;
  initInfo.Device          = g_Device;
  initInfo.QueueFamily     = g_QueueFamily;
  initInfo.Queue           = g_Queue;
  initInfo.PipelineCache   = g_PipelineCache;
  initInfo.DescriptorPool  = g_DescriptorPool;
  initInfo.MinImageCount   = g_MinImageCount;
  initInfo.ImageCount      = wd->ImageCount;
  initInfo.Allocator       = g_Allocator;
  initInfo.CheckVkResultFn = CheckVkResult;

  ImGui_ImplVulkan_PipelineInfo pipelineInfoMain {};
  pipelineInfoMain.Subpass     = 0;
  pipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
  pipelineInfoMain.RenderPass  = wd->RenderPass;

  initInfo.PipelineInfoMain = pipelineInfoMain;

  ImGui_ImplVulkan_Init(&initInfo);

  // Load default font
  ImFontConfig fontConfig;
  fontConfig.FontDataOwnedByAtlas = false;

  s_Fonts.push_back(io.Fonts->AddFontFromMemoryTTF(
      (void*)g_RobotoRegular,
      sizeof(g_RobotoRegular),
      m_Specification.FontSize,
      &fontConfig));
  s_Fonts.push_back(
      io.Fonts->AddFontFromMemoryTTF((void*)g_RobotoBold, sizeof(g_RobotoBold), m_Specification.FontSize, &fontConfig));
  s_Fonts.push_back(io.Fonts->AddFontFromMemoryTTF(
      (void*)g_RobotoMedium,
      sizeof(g_RobotoMedium),
      m_Specification.FontSize,
      &fontConfig));
  s_Fonts.push_back(io.Fonts->AddFontFromMemoryTTF(
      (void*)g_RobotoLight,
      sizeof(g_RobotoLight),
      m_Specification.FontSize,
      &fontConfig));
  s_Fonts.push_back(io.Fonts->AddFontFromMemoryTTF(
      (void*)g_RobotoLightItalic,
      sizeof(g_RobotoLightItalic),
      m_Specification.FontSize,
      &fontConfig));

  io.FontDefault = s_Fonts[0];

  // Upload Fonts
  {
    // Use any command queue
    VkCommandPool   command_pool   = wd->Frames[wd->FrameIndex].CommandPool;
    VkCommandBuffer command_buffer = wd->Frames[wd->FrameIndex].CommandBuffer;

    err = vkResetCommandPool(g_Device, command_pool, 0);
    CheckVkResult(err, "Failed to at vkResetCommandPool");
    VkCommandBufferBeginInfo beginInfo {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    err = vkBeginCommandBuffer(command_buffer, &beginInfo);
    CheckVkResult(err, "Failed to at vkBeginCommandBuffer");

    VkSubmitInfo endInfo {};
    endInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    endInfo.commandBufferCount = 1;
    endInfo.pCommandBuffers    = &command_buffer;
    err                        = vkEndCommandBuffer(command_buffer);
    CheckVkResult(err, "Failed to at vkEndCommandBuffer");
    err = vkQueueSubmit(g_Queue, 1, &endInfo, VK_NULL_HANDLE);
    CheckVkResult(err, "Failed to at vkQueueSubmit");

    err = vkDeviceWaitIdle(g_Device);
    CheckVkResult(err, "Failed to at vkDeviceWaitIdle");
  }

  // Setup Vulkan Memory Allocator (vma/VulkanMemoryAllocator)
  {
    VmaVulkanFunctions vulkanFunctions {};
    vulkanFunctions.vkGetInstanceProcAddr = &vkGetInstanceProcAddr;
    vulkanFunctions.vkGetDeviceProcAddr   = &vkGetDeviceProcAddr;

    VmaAllocatorCreateInfo allocatorCreateInfo {};
    allocatorCreateInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT |
                                VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
    allocatorCreateInfo.vulkanApiVersion = VULKAN_API_VERSION;
    allocatorCreateInfo.physicalDevice   = g_PhysicalDevice;
    allocatorCreateInfo.device           = g_Device;
    allocatorCreateInfo.instance         = g_Instance;
    allocatorCreateInfo.pVulkanFunctions = &vulkanFunctions;

    vmaCreateAllocator(&allocatorCreateInfo, &g_VmaAllocator);
  }
}

void Application::Shutdown() {
  for (auto& layer : m_LayerStack)
    layer->OnDetach();

  m_MenubarCallback = nullptr;

  m_LayerStack.clear();

  // Cleanup
  VkResult err = vkDeviceWaitIdle(g_Device);
  CheckVkResult(err, "Failed to at vkDeviceWaitIdle");

  // Free buffers in queue
  for (auto& queue : s_Defer.FreeBuffers)
    for (auto& b : queue)
      vmaDestroyBuffer(g_VmaAllocator, b.buffer, b.allocation);
  s_Defer.FreeBuffers.clear();

  // Free resources in queue
  for (auto& queue : s_Defer.FreeResources)
    for (auto& func : queue)
      func();
  s_Defer.FreeResources.clear();

  ImGui_ImplVulkan_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  CleanupVulkanWindow();
  CleanupVulkan();

  glfwDestroyWindow(m_WindowHandle);
  glfwTerminate();

  g_ApplicationRunning = false;
}

void Application::Run() {
  m_Running = true;

  glfwShowWindow(m_WindowHandle);

  if (m_Specification.Maximized)
    glfwMaximizeWindow(m_WindowHandle);

  ImGui_ImplVulkanH_Window* wd         = &g_MainWindowData;
  ImVec4                    clearColor = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
  ImGuiIO&                  io         = ImGui::GetIO();

  while (!glfwWindowShouldClose(m_WindowHandle) && m_Running) {

    glfwPollEvents();

    for (auto& layer : m_LayerStack)
      layer->OnUpdate(m_TimeStep);

    // Resize swap chain?
    if (g_SwapChainRebuild) {
      int width, height;
      glfwGetFramebufferSize(m_WindowHandle, &width, &height);
      if (width > 0 && height > 0) {
        ImGui_ImplVulkan_SetMinImageCount(g_MinImageCount);
        ImGui_ImplVulkanH_CreateOrResizeWindow(
            g_Instance,
            g_PhysicalDevice,
            g_Device,
            &g_MainWindowData,
            g_QueueFamily,
            g_Allocator,
            width,
            height,
            g_MinImageCount,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
        g_MainWindowData.FrameIndex = 0;

        // Clear allocated command buffers from here since entire pool is
        // destroyed
        s_AllocatedCommandBuffers.clear();
        s_AllocatedCommandBuffers.resize(g_MainWindowData.ImageCount);

        g_SwapChainRebuild = false;
      }
    }

    // Start the Dear ImGui frame
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    {
      static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

      // We are using the ImGuiWindowFlags_NoDocking flag to make the parent
      // window not dockable into, because it would be confusing to have two
      // docking targets within each others.
      ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDocking;
      if (m_MenubarCallback)
        windowFlags |= ImGuiWindowFlags_MenuBar;

      const ImGuiViewport* viewport = ImGui::GetMainViewport();
      ImGui::SetNextWindowPos(viewport->WorkPos);
      ImGui::SetNextWindowSize(viewport->WorkSize);
      ImGui::SetNextWindowViewport(viewport->ID);
      ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
      ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
      windowFlags |= ImGuiWindowFlags_NoTitleBar |
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove;
      windowFlags |=
          ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

      // When using ImGuiDockNodeFlags_PassthruCentralNode, DockSpace() will
      // render our background and handle the pass-thru hole, so we ask Begin()
      // to not render a background.
      if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
        windowFlags |= ImGuiWindowFlags_NoBackground;

      // Important: note that we proceed even if Begin() returns false (aka
      // window is collapsed). This is because we want to keep our DockSpace()
      // active. If a DockSpace() is inactive, all active windows docked into it
      // will lose their parent and become undocked. We cannot preserve the
      // docking relationship between an active window and an inactive docking,
      // otherwise any change of dockspace/settings would lead to windows being
      // stuck in limbo and never being visible.
      ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
      ImGui::Begin("Application", nullptr, windowFlags);

      ImGui::PopStyleVar(3);

      // Submit the DockSpace
      ImGuiIO& io = ImGui::GetIO();
      if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
        ImGuiID dockspace_id = ImGui::GetID("VulkanAppDockspace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
      }

      if (m_MenubarCallback) {
        if (ImGui::BeginMenuBar()) {
          m_MenubarCallback();
          ImGui::EndMenuBar();
        }
      }

      for (auto& layer : m_LayerStack)
        layer->OnRender();

      ImGui::End();
    }

    // Rendering
    ImGui::Render();
    ImDrawData* drawData    = ImGui::GetDrawData();
    const bool  isMinimized = (drawData->DisplaySize.x <= 0.0f || drawData->DisplaySize.y <= 0.0f);

    if (!isMinimized) {
      wd->ClearValue.color.float32[0] = clearColor.x * clearColor.w;
      wd->ClearValue.color.float32[1] = clearColor.y * clearColor.w;
      wd->ClearValue.color.float32[2] = clearColor.z * clearColor.w;
      wd->ClearValue.color.float32[3] = clearColor.w;

      FrameRender(wd, drawData);
      FramePresent(wd);
    }

    float time      = GetTime();
    m_FrameTime     = time - m_LastFrameTime;
    m_TimeStep      = glm::min<float>(m_FrameTime, 0.0333f);
    m_LastFrameTime = time;
    akari::window::Input::ResetScroll();
  }
}

void Application::Close() { m_Running = false; }

float Application::GetTime() { return (float)glfwGetTime(); }

VkInstance Application::GetInstance() { return g_Instance; }

VkPhysicalDevice Application::GetPhysicalDevice() { return g_PhysicalDevice; }

VkDevice Application::GetDevice() { return g_Device; }

VkCommandBuffer
Application::GetCommandBuffer(const CommandBufferSpecification& specification) {
  ImGui_ImplVulkanH_Window* wd = &g_MainWindowData;

  // Use any command queue
  VkCommandPool command_pool = wd->Frames[wd->FrameIndex].CommandPool;

  VkCommandBufferAllocateInfo cmdBufAllocateInfo {};
  cmdBufAllocateInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  cmdBufAllocateInfo.commandPool        = command_pool;
  cmdBufAllocateInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  cmdBufAllocateInfo.commandBufferCount = specification.Count;

  VkCommandBuffer& command_buffer = s_AllocatedCommandBuffers[wd->FrameIndex].emplace_back();
  auto             err            = vkAllocateCommandBuffers(g_Device, &cmdBufAllocateInfo, &command_buffer);
  CheckVkResult(err, "Failed to at vkAllocateCommandBuffers");

  if (specification.Begin) {
    VkCommandBufferBeginInfo beginInfo {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    err = vkBeginCommandBuffer(command_buffer, &beginInfo);
    CheckVkResult(err, "Failed to at vkBeginCommandBuffer");
  }

  return command_buffer;
}

VkCommandPool Application::GetCommandPool() {
  ImGui_ImplVulkanH_Window* wd = &g_MainWindowData;
  return wd->Frames[wd->FrameIndex].CommandPool;
}

void Application::FlushCommandBuffer(VkCommandBuffer commandBuffer) {
  const uint64_t DEFAULT_FENCE_TIMEOUT = 100000000000;

  VkSubmitInfo endInfo {};
  endInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  endInfo.commandBufferCount = 1;
  endInfo.pCommandBuffers    = &commandBuffer;
  auto err                   = vkEndCommandBuffer(commandBuffer);
  CheckVkResult(err, "Failed to at vkEndCommandBuffer");

  // Create fence to ensure that the command buffer has finished executing
  VkFenceCreateInfo fenceCreateInfo {};
  fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceCreateInfo.flags = 0;
  VkFence fence;
  err = vkCreateFence(g_Device, &fenceCreateInfo, nullptr, &fence);
  CheckVkResult(err, "Failed to at vkCreateFence");

  err = vkQueueSubmit(g_Queue, 1, &endInfo, fence);
  CheckVkResult(err, "Failed to at vkQueueSubmit");

  err = vkWaitForFences(g_Device, 1, &fence, VK_TRUE, DEFAULT_FENCE_TIMEOUT);
  CheckVkResult(err, "Failed to at vkWaitForFences");

  vkDestroyFence(g_Device, fence, nullptr);
}

void Application::FreeResource(std::function<void()>&& func) {
  s_Defer.FreeResources[g_FrameIndex].emplace_back(func);
}

void Application::FreeBuffer(const FreeBufferInfo& info) {
  s_Defer.FreeBuffers[g_FrameIndex].emplace_back(info);
}

VkSampleCountFlagBits Application::GetMaxUsableSampleCount() {
  VkPhysicalDeviceProperties props;
  vkGetPhysicalDeviceProperties(GetPhysicalDevice(), &props);

  VkSampleCountFlags counts = props.limits.framebufferColorSampleCounts &
                              props.limits.framebufferDepthSampleCounts;

  if (counts & VK_SAMPLE_COUNT_64_BIT)
    return VK_SAMPLE_COUNT_64_BIT;
  if (counts & VK_SAMPLE_COUNT_32_BIT)
    return VK_SAMPLE_COUNT_32_BIT;
  if (counts & VK_SAMPLE_COUNT_16_BIT)
    return VK_SAMPLE_COUNT_16_BIT;
  if (counts & VK_SAMPLE_COUNT_8_BIT)
    return VK_SAMPLE_COUNT_8_BIT;
  if (counts & VK_SAMPLE_COUNT_4_BIT)
    return VK_SAMPLE_COUNT_4_BIT;
  if (counts & VK_SAMPLE_COUNT_2_BIT)
    return VK_SAMPLE_COUNT_2_BIT;

  return VK_SAMPLE_COUNT_1_BIT;
}

uint32_t Application::GetCurrentFrameIndex() {
  return g_MainWindowData.FrameIndex;
}

uint32_t Application::GetFrameCount() { return ImGui::GetFrameCount(); }

uint32_t Application::GetMaxFramesInFlight() {
  return g_MainWindowData.ImageCount;
}

VmaAllocator Application::GetVmaAllocator() { return g_VmaAllocator; }

VkQueue Application::GetGraphicsQueue() { return g_Queue; }

uint32_t Application::GetGraphicsQueueFamilyIndex() { return g_QueueFamily; }

ImFont* Application::GetFont(FontType type) { return s_Fonts[type]; }

std::vector<VkExtensionProperties> Application::GetSupportedDeviceExtensions() {
  VkResult result = (VkResult)0;

  uint32_t count = 0;
  result         = vkEnumerateDeviceExtensionProperties(g_PhysicalDevice, nullptr, &count, nullptr);
  CheckVkResult(result, "Failed to at vkEnumerateDeviceExtensionProperties");

  std::vector<VkExtensionProperties> extensions(count);

  result = vkEnumerateDeviceExtensionProperties(g_PhysicalDevice, nullptr, &count, extensions.data());
  CheckVkResult(result, "Failed to at vkEnumerateDeviceExtensionProperties");

  return extensions;
}

std::vector<VkExtensionProperties>
Application::GetSupportedInstanceExtensions() {
  VkResult result = (VkResult)0;

  uint32_t count = 0;
  result         = vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);
  CheckVkResult(result, "Failed to at vkEnumerateInstanceExtensionProperties");

  std::vector<VkExtensionProperties> extensions(count);

  result = vkEnumerateInstanceExtensionProperties(nullptr, &count, extensions.data());
  CheckVkResult(result, "Failed to at vkEnumerateInstanceExtensionProperties");

  return extensions;
}

uint32_t Application::FindMemoryType(uint32_t              typeFilter,
                                     VkMemoryPropertyFlags properties) {
  VkPhysicalDeviceMemoryProperties memProperties;
  vkGetPhysicalDeviceMemoryProperties(g_PhysicalDevice, &memProperties);

  for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
    if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags &
                                    properties) == properties) {
      return i;
    }
  }

  throw std::runtime_error("Failed to find suitable memory type!");
}

} // namespace akari::window