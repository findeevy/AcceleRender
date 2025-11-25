/**
 * @file VulkanUtils.cpp
 * @brief Utility helper functions for common Vulkan operations.
 *
 * This file contains reusable Vulkan helper functions:
 * - Reading SPIR-V shader binaries from disk
 * - Creating Vulkan image views
 *
 * @note Part of the vkutils module.
 */
#include "../include/VulkanUtils.hpp"
#include <fstream>
#include <stdexcept>
#include <vector>
#include <vulkan/vulkan_raii.hpp>

/**
 * @namespace vkutils
 * @brief Collection of utility functions for Vulkan resource management.
 */
namespace vkutils {

/**
 * @brief Reads the entire contents of a binary file.
 *
 * This utility function is typically used to load SPIR-V shader bytecode or
 * other binary assets into memory. The file is opened in binary mode and read
 * into a contiguous buffer of bytes.
 *
 * @param filename Path to the file to read (e.g., "shaders/vert.spv")
 * @return A std::vector<char> containing the raw file bytes
 * @throws std::runtime_error if the file cannot be opened
 *
 * @note Uses std::ios::ate to determine the file size efficiently.
 * @example
 * @code
 * auto vertShaderCode = vkutils::readFile("shaders/vert.spv");
 * @endcode
 */
std::vector<char> readFile(const std::string &filename) {
  std::ifstream file(filename, std::ios::ate | std::ios::binary);
  if (!file.is_open()) {
    throw std::runtime_error("Failed to open file: " + filename);
  }

  size_t fileSize = static_cast<size_t>(file.tellg());
  std::vector<char> buffer(fileSize);

  file.seekg(0);
  file.read(buffer.data(), fileSize);
  file.close();

  return buffer;
}

/**
 * @brief Creates a 2D Vulkan image view from a given image.
 *
 * This helper constructs a vk::raii::ImageView object for the specified image,
 * which allows the image to be accessed by shaders. The view type is 2D by
 * default, and the caller can specify the format, aspect mask, and mip level
 * count.
 *
 * @param device Logical Vulkan device used to create the image view
 * @param image Vulkan image handle wrapped in vk::raii::Image
 * @param format Format of the image (e.g., vk::Format::eR8G8B8A8Srgb)
 * @param aspectFlags Aspect mask specifying which parts of the image are
 *        accessible (e.g., vk::ImageAspectFlagBits::eColor)
 * @param mipLevels Number of mipmap levels in the image (default is 1)
 * @return A vk::raii::ImageView object managing the created view
 *
 * @throws vk::SystemError if Vulkan creation fails
 *
 * @example
 * @code
 * auto textureView = vkutils::createImageView(device, textureImage,
 *                                             vk::Format::eR8G8B8A8Srgb,
 *                                             vk::ImageAspectFlagBits::eColor);
 * @endcode
 */
vk::raii::ImageView createImageView(const vk::raii::Device &device,
                                    const vk::raii::Image &image,
                                    vk::Format format,
                                    vk::ImageAspectFlags aspectFlags,
                                    uint32_t mipLevels) {
  vk::ImageViewCreateInfo viewInfo{};
  viewInfo.image = *image;
  viewInfo.viewType = vk::ImageViewType::e2D;
  viewInfo.format = format;
  viewInfo.components.r = vk::ComponentSwizzle::eIdentity;
  viewInfo.components.g = vk::ComponentSwizzle::eIdentity;
  viewInfo.components.b = vk::ComponentSwizzle::eIdentity;
  viewInfo.components.a = vk::ComponentSwizzle::eIdentity;
  viewInfo.subresourceRange.aspectMask = aspectFlags;
  viewInfo.subresourceRange.baseMipLevel = 0;
  viewInfo.subresourceRange.levelCount = mipLevels;
  viewInfo.subresourceRange.baseArrayLayer = 0;
  viewInfo.subresourceRange.layerCount = 1;

  return vk::raii::ImageView(device, viewInfo);
}

} // end of namespace vkutils
