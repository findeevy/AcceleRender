#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <vector>
#include <vulkan/vk_platform.h>
#include <vulkan/vulkan_raii.hpp>

const uint32_t WIDTH = 1280;
const uint32_t HEIGHT = 720;

const std::vector validationLayers = {"VK_LAYER_KHRONOS_validation"};

#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif

class AcceleRender {
public:
  void run() {
    initWindow();
    initVulkan();
    mainLoop();
    cleanup();
  }

private:
  vk::raii::Context context;
  // Our global Vulkan instance.
  vk::raii::Instance instance = nullptr;

  vk::raii::DebugUtilsMessengerEXT debugMessenger = nullptr;

  vk::raii::PhysicalDevice physicalGPU = nullptr;

  vk::raii::Device GPU = nullptr;

  vk::raii::Queue graphicsQueue = nullptr;

  vk::PhysicalDeviceFeatures GPUFeatures;

  vk::raii::SurfaceKHR surface = nullptr;

  GLFWwindow *window = nullptr;

  vk::raii::Queue presentQueue = nullptr;

  std::vector<const char *> gpuExtensions = {
      vk::KHRSwapchainExtensionName, vk::KHRSpirv14ExtensionName,
      vk::KHRSynchronization2ExtensionName,
      vk::KHRCreateRenderpass2ExtensionName};

  void createSurface() {
    VkSurfaceKHR _surface;
    if (glfwCreateWindowSurface(*instance, window, nullptr, &_surface) != 0) {
      throw std::runtime_error("failed to create window surface!");
    }
    surface = vk::raii::SurfaceKHR(instance, _surface);
  }

  void pickLogicalGPU() {
    std::vector<vk::QueueFamilyProperties> queueFamilyProperties =
        physicalGPU.getQueueFamilyProperties();

    uint32_t graphicsIndex =
        static_cast<uint32_t>(queueFamilyProperties.size());
    uint32_t presentIndex = static_cast<uint32_t>(queueFamilyProperties.size());

    // Find a queue family that supports both graphics and present
    for (uint32_t i = 0; i < queueFamilyProperties.size(); i++) {
      if (queueFamilyProperties[i].queueFlags & vk::QueueFlagBits::eGraphics) {
        if (graphicsIndex == queueFamilyProperties.size()) {
          graphicsIndex = i;
        }
        if (physicalGPU.getSurfaceSupportKHR(i, *surface)) {
          graphicsIndex = i;
          presentIndex = i;
          break;
        }
      }
    }

    // Find separate present queue if needed
    if (presentIndex == queueFamilyProperties.size()) {
      for (uint32_t i = 0; i < queueFamilyProperties.size(); i++) {
        if (physicalGPU.getSurfaceSupportKHR(i, *surface)) {
          presentIndex = i;
          break;
        }
      }
    }

    // Fallback to any graphics queue
    if (graphicsIndex == queueFamilyProperties.size()) {
      for (uint32_t i = 0; i < queueFamilyProperties.size(); i++) {
        if (queueFamilyProperties[i].queueFlags &
            vk::QueueFlagBits::eGraphics) {
          graphicsIndex = i;
          break;
        }
      }
    }

    if ((graphicsIndex == queueFamilyProperties.size()) ||
        (presentIndex == queueFamilyProperties.size())) {
      throw std::runtime_error(
          "No graphics and/or present queue family found!");
    }

    // Collect unique queue families
    std::set<uint32_t> uniqueQueueFamilies = {graphicsIndex, presentIndex};
    std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies) {
      queueCreateInfos.push_back(
          vk::DeviceQueueCreateInfo({}, queueFamily, 1, &queuePriority));
    }

    vk::StructureChain<vk::PhysicalDeviceFeatures2,
                       vk::PhysicalDeviceVulkan13Features,
                       vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>
        featureChain;

    featureChain.get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering =
        true;
    featureChain.get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>()
        .extendedDynamicState = true;

    vk::DeviceCreateInfo deviceCreateInfo{};
    deviceCreateInfo.pNext = &featureChain.get<vk::PhysicalDeviceFeatures2>();
    deviceCreateInfo.queueCreateInfoCount =
        static_cast<uint32_t>(queueCreateInfos.size());
    deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
    deviceCreateInfo.enabledExtensionCount =
        static_cast<uint32_t>(gpuExtensions.size());
    deviceCreateInfo.ppEnabledExtensionNames = gpuExtensions.data();

    GPU = vk::raii::Device(physicalGPU, deviceCreateInfo);
    graphicsQueue = vk::raii::Queue(GPU, graphicsIndex, 0);
    presentQueue = vk::raii::Queue(GPU, presentIndex, 0);
  }

  void pickPhysicalGPU() {
    std::vector<vk::raii::PhysicalDevice> gpus =
        instance.enumeratePhysicalDevices();
    const auto devIter = std::ranges::find_if(gpus, [&](auto const &gpu) {
      auto queueFamilies = gpu.getQueueFamilyProperties();
      bool isSuitable = gpu.getProperties().apiVersion >= VK_API_VERSION_1_3;
      const auto qfpIter = std::ranges::find_if(
          queueFamilies, [](vk::QueueFamilyProperties const &qfp) {
            return (qfp.queueFlags & vk::QueueFlagBits::eGraphics) !=
                   static_cast<vk::QueueFlags>(0);
          });
      isSuitable = isSuitable && (qfpIter != queueFamilies.end());
      auto extensions = gpu.enumerateDeviceExtensionProperties();
      bool found = true;
      for (auto const &extension : gpuExtensions) {
        auto extensionIter =
            std::ranges::find_if(extensions, [extension](auto const &ext) {
              return strcmp(ext.extensionName, extension) == 0;
            });
        found = found && extensionIter != extensions.end();
      }
      isSuitable = isSuitable && found;
      printf("\n");
      if (isSuitable) {
        physicalGPU = gpu;
      }
      return isSuitable;
    });
    if (devIter == gpus.end()) {
      throw std::runtime_error(
          "Failed to find a GPU that supports Vulkan 1.3!");
    }
  }

  void setupDebugMessenger() {
    if (!enableValidationLayers) {
      return;
    }

    vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
    vk::DebugUtilsMessageTypeFlagsEXT messageTypeFlags(
        vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
        vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
        vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);
    vk::DebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfoEXT{
        {}, severityFlags, messageTypeFlags, &debugCallback};
    instance.createDebugUtilsMessengerEXT(debugUtilsMessengerCreateInfoEXT);
  }

  static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(
      vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
      vk::DebugUtilsMessageTypeFlagsEXT type,
      const vk::DebugUtilsMessengerCallbackDataEXT *pCallbackData, void *) {
    if (severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eError ||
        severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning) {
      std::cerr << "validation layer: type " << to_string(type)
                << " msg: " << pCallbackData->pMessage << std::endl;
    }

    return vk::False;
  }

  // Retrieves a list of extensions that are required (if in debug mode).
  std::vector<const char *> getRequiredExtensions() {
    uint32_t glfwExtensionCount = 0;
    auto glfwExtensions =
        glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
    if (enableValidationLayers) {
      extensions.push_back(vk::EXTDebugUtilsExtensionName);
    }

    extensions.push_back(vk::KHRGetPhysicalDeviceProperties2ExtensionName);

    return extensions;
  }

  void createInstance() {
    vk::ApplicationInfo appInfo("AcceleRender", VK_MAKE_VERSION(1, 0, 0),
                                "No Engine", VK_MAKE_VERSION(1, 0, 0),
                                VK_API_VERSION_1_0);

    std::vector<char const *> requiredLayers;
    if (enableValidationLayers) {
      requiredLayers.assign(validationLayers.begin(), validationLayers.end());
    }

    auto layerProperties = context.enumerateInstanceLayerProperties();
    for (auto const &requiredLayer : requiredLayers) {
      if (std::ranges::none_of(
              layerProperties, [requiredLayer](auto const &layerProperty) {
                return strcmp(layerProperty.layerName, requiredLayer) == 0;
              })) {
        throw std::runtime_error("Required layer not supported: " +
                                 std::string(requiredLayer));
      }
    }

    auto requiredExtensions = getRequiredExtensions();

    auto extensionProperties = context.enumerateInstanceExtensionProperties();
    for (auto const &requiredExtension : requiredExtensions) {
      if (std::ranges::none_of(
              extensionProperties,
              [requiredExtension](auto const &extensionProperty) {
                return strcmp(extensionProperty.extensionName,
                              requiredExtension) == 0;
              })) {
        throw std::runtime_error("Required extension not supported: " +
                                 std::string(requiredExtension));
      }
    }

    vk::InstanceCreateInfo createInfo{
        {},
        &appInfo,
        static_cast<uint32_t>(requiredLayers.size()),
        requiredLayers.data(),
        static_cast<uint32_t>(requiredExtensions.size()),
        requiredExtensions.data()};
    instance = vk::raii::Instance(context, createInfo);
  }

  void initWindow() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
  }

  void initVulkan() {
    createInstance();
    setupDebugMessenger();
    createSurface();
    pickPhysicalGPU();
    pickLogicalGPU();
  }

  void mainLoop() {
    while (!glfwWindowShouldClose(window)) {
      glfwPollEvents();
    }
  }

  void cleanup() {
    glfwDestroyWindow(window);
    glfwTerminate();
  }
};

int main() {
  try {
    AcceleRender app;
    app.run();
  } catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
