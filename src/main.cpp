/**
 * @file main.cpp
 * @brief Entry point for Accelerender.
 *
 * Handles platform-specific Vulkan loader setup (macOS), initializes
 * the VulkanRenderer, runs the rendering loop, and ensures proper
 * cleanup in case of exceptions.
 *
 * @authors Finley Deevy, Eric Newton
 */

#include "../include/render.hpp"

/**
 * @brief Entry point for the Accelerender application.
 *
 * @return EXIT_SUCCESS if the application runs successfully,
 *         otherwise EXIT_FAILURE.
 */
int main() {

// macOS specific initialization
#ifdef __APPLE__
#include <cstdlib>
#include <iostream>

  // on macOS, manually set the dynamic loader path to the Vulkan SDK dylib
  const char *sdkPath = std::getenv("VULKAN_SDK");
  if (sdkPath) {
    // construct full path to Vulkan loader dynamic library
    std::string dylibPath =
        std::string(sdkPath) + "/macOS/lib/libvulkan.1.dylib";

    // override DYLD_LIBRARY_PATH to point to Vulkan loader
    setenv("DYLD_LIBRARY_PATH", (std::string(sdkPath) + "/macOS/lib").c_str(),
           1);

    std::cout << "Using Vulkan loader from: " << dylibPath << std::endl;
  } else {
    // warn user if VULKAN_SDK environment variable is missing
    std::cerr << "Warning: VULKAN_SDK not set. Vulkan may fail to load."
              << std::endl;
  }
#endif

  // create Accelerender application object
  VulkanRenderer app;

  try {
    // run Accelerender initialization, main loop & rendering
    app.run();
  } catch (const std::exception &e) {
    // catch any exceptions thrown during either setup or rendering
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
  }

  // normal exit if everything ran successfully
  return EXIT_SUCCESS;
}
