#include <GLFW/glfw3.h>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vulkan/vk_platform.h>
#include <vulkan/vulkan_raii.hpp>

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

class HelloTriangleApplication {
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

  GLFWwindow *window = nullptr;

  void createInstance() {
    vk::ApplicationInfo appInfo("Hello Triangle", VK_MAKE_VERSION(1, 0, 0),
                                "No Engine", VK_MAKE_VERSION(1, 0, 0),
                                VK_API_VERSION_1_0);

    uint32_t glfwExtensionCount = 0;
    const char **glfwExtensions =
        glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    auto extensionProperties = context.enumerateInstanceExtensionProperties();

    for (uint32_t i = 0; i < glfwExtensionCount; ++i) {
      bool found = false;
      for (const auto &prop : extensionProperties) {
        if (strcmp(prop.extensionName, glfwExtensions[i]) == 0) {
          found = true;
          break;
        }
      }
      if (!found) {
        throw std::runtime_error("Required GLFW extension not supported: " +
                                 std::string(glfwExtensions[i]));
      }
    }

    vk::InstanceCreateInfo createInfo({}, &appInfo, 0, nullptr,
                                      glfwExtensionCount, glfwExtensions);

    instance = vk::raii::Instance(context, createInfo);
  }

  void initWindow() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
  }

  void initVulkan() { createInstance(); }

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
    HelloTriangleApplication app;
    app.run();
  } catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
