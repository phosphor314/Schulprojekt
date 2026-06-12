#pragma once

#include <glm/detail/qualifier.hpp>
#include <glm/ext/vector_float3.hpp>
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vulkan/vulkan_core.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <optional>
#include <string>
#include <vector>
#include "constants.h"

struct QueueIndices {
  std::optional<uint32_t> graphicsQueue;
  std::optional<uint32_t> presentQueue;
  std::optional<uint32_t> computeQueue;

  bool complete() const;
};

struct SwapChainSupportDetails {
  VkSurfaceCapabilitiesKHR capabilities;
  std::vector<VkSurfaceFormatKHR> formats;
  std::vector<VkPresentModeKHR> presentModes;

  bool isComplete() const;
};

struct UniformBufferObject {
  glm::mat4 camera;
  glm::vec4 view_pos;
  glm::vec4 light_pos;
  float aspectRatio;
};

struct ShaderBuffer {
  VkDeviceSize size = 0;
  VkDeviceSize offset;
  VkBuffer buffer = VK_NULL_HANDLE;
  VkDeviceMemory bufferMemory = VK_NULL_HANDLE;
};

struct Vertex {
  Vertex() = default;
  Vertex(glm::vec3 pos, glm::vec3 norm, glm::vec3 col)
      : position(pos), normal(norm), color(col) {}

  glm::vec3 position;
  glm::vec3 normal;
  glm::vec3 color;

  bool operator==(const Vertex &other) const;
};

struct Camera {
  glm::vec3 eye = {0.0f, 4.0f, 4.0f};
  glm::vec3 forward = {0.0f, -1.0f, -1.0f};
  glm::vec3 up = {0.0f, 1.0f, 0.0f};
  float fov = 3.1415f;

  glm::mat4 getTransformationMat(float aspectRatio);
};

struct RenderEngine {
public:
  Camera cam;
  GLFWwindow *window = nullptr;
  VkRenderPass renderPass;
  VkDevice device = VK_NULL_HANDLE;
  uint32_t currentFrame = 0;

  RenderEngine() = default;
  ~RenderEngine();

  void init();

  // application draw calls are sandwiched between these two
  VkCommandBuffer beginRendering();
  void endRendering();

  bool running() const;

  void cleanup();

  void framebufferSizeCallback(int nWidth, int nHeight);

  VkShaderModule createShaderModule(const std::vector<char> &code) const;
  VkResult allocateMemory(VkMemoryPropertyFlags properties,
                          ShaderBuffer *pBuffers, size_t bufferCount,
                          VkDeviceMemory &memory);
  VkResult createBuffer(VkBufferUsageFlags usage, ShaderBuffer &buffer);
  VkImageView createImageView(VkImage image, VkFormat format,
                              VkImageAspectFlags aspectFlags, VkDevice device,
                              uint32_t mipLevels);
  void createImage(uint32_t width, uint32_t height, uint32_t mipLevels,
                   VkFormat format, VkImageTiling tiling,
                   VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
                   VkImage &image, VkDeviceMemory &imageMemory);
  std::vector<char> readFile(const std::string &filename);
  VkResult copyBuffer(const ShaderBuffer &srcBuffer, const ShaderBuffer &dstBuffer,
                      VkBufferCopy copyRegion);
  VkResult createStagingBuffer(ShaderBuffer &buffer, void **ppData);
  void freeStagingBuffer(ShaderBuffer &buffer);
  void destroyBuffer(ShaderBuffer &buffer);
  void freeMemory(VkDeviceMemory memory);

private:
  const std::vector<const char *> deviceExtensions = {
      VK_KHR_SWAPCHAIN_EXTENSION_NAME};

  uint32_t imageIndex = 0;

  VkDebugUtilsMessengerEXT debugMessenger;
  VkInstance vkInstance;
  VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
  VkSurfaceKHR surface;
  QueueIndices queueIndices;
  VkQueue graphicsQueue;
  VkQueue presentQueue;
  VkQueue computeQueue;
  VkCommandPool pool;

  VkSwapchainKHR swapchain;
  std::vector<VkImage> swapchainImages;
  std::vector<VkImageView> swapchainImageViews;
  std::vector<VkFramebuffer> swapchainFramebuffers;
  VkFormat swapchainImageFormat;
  VkExtent2D swapchainExtent;

  VkFormat depthFormat;
  VkImage zImage;
  VkImageView zImageView;
  VkDeviceMemory zImageMemory;

  std::vector<VkSemaphore> imageAvailableSemaphores;
  std::vector<VkSemaphore> renderFinishedSemaphores;
  std::vector<VkFence> inFlightFences;
  std::vector<VkCommandBuffer> commandBuffers;

  bool framebufferResized = false;

  VkResult swapchainImageResult;

  void initGLFW();
  void initVulkan();
  void createVkInstance();
  void choosePhysicalDevice();
  void createSurface();
  void createLogicalDevice();
  void getRequiredExtensions(std::vector<const char *> &extensions);
  void setupDebugMessenger();
  void createCommandPool();
  void createSwapchain();
  void initializeSwapchain();
  void createRenderPass();
  void createCommandBuffers();

  void recreateSwapchain();
  void beginRecordingCommandBuffer(VkCommandBuffer commandBuffer,
                                   uint32_t index);
  void endRecordingCommandBuffer(VkCommandBuffer commandBuffer);

  QueueIndices findGraphicsDeviceIndices(VkPhysicalDevice device);
  SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
  VkSurfaceFormatKHR
  chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats);
  VkPresentModeKHR
  choosePresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes);
  VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capablities);
  bool checkDeviceExtensionSupport(VkPhysicalDevice device);
  bool isDeviceSuitable(VkPhysicalDevice device);
  static VKAPI_ATTR VkBool32 VKAPI_CALL
  debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                VkDebugUtilsMessageTypeFlagsEXT messageType,
                const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
                void *pUserData);
  VkFormat findDepthFormat();
  VkFormat findSuppertedFormat(const std::vector<VkFormat> &candidates,
                               VkImageTiling tiling,
                               VkFormatFeatureFlags features,
                               VkPhysicalDevice device);
  uint32_t findMemoryType(uint32_t typeFilter,
                          VkMemoryPropertyFlags properties);
  void
  transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout,
                        VkImageLayout newLayout, uint32_t mipLevels,
                        VkImageAspectFlags flags = VK_IMAGE_ASPECT_COLOR_BIT);
  VkCommandBuffer beginSingleTimeCommands();
  VkResult endSingleTimeCommands(VkCommandBuffer commandBuffer);
  bool hasStencilComponent(VkFormat format);
};
