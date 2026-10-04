#pragma once

#include <glm/detail/qualifier.hpp>
#include <glm/ext/vector_float3.hpp>
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vulkan/vulkan_core.h>
#define GLFW_INCLUDE_VULKAN
#include "constants.h"
#include <GLFW/glfw3.h>
#include <optional>
#include <string>
#include <vector>

struct QueueIndices {
  std::optional<uint32_t> graphicsQueue;
  std::optional<uint32_t> presentQueue;
  std::optional<uint32_t> computeQueue;

  bool complete() const; // Checks if all queue indices are set.
};

struct SwapChainSupportDetails {
  VkSurfaceCapabilitiesKHR capabilities;
  std::vector<VkSurfaceFormatKHR> formats;
  std::vector<VkPresentModeKHR> presentModes;

  bool isComplete() const; // Checks if all necessary swapchain support details are present.
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
  // Default constructor for convenience
  Vertex() = default;
  // Constructor taking position, normal, and color
  Vertex(glm::vec3 pos, glm::vec3 norm, glm::vec3 col)
      : position(pos), normal(norm), color(col) {}

  glm::vec3 position;
  glm::vec3 normal;
  glm::vec3 color;

  // Equality operator for comparing vertices
  bool operator==(const Vertex &other) const;
};

struct Camera {
  glm::vec3 eye = {0.0f, 4.0f, 4.0f};
  glm::vec3 forward = {0.0f, -1.0f, -1.0f};
  glm::vec3 up = {0.0f, 1.0f, 0.0f};
  float fov = 3.1415f;

  // Calculates the transformation matrix for the camera view
  glm::mat4 getTransformationMat(float aspectRatio);
};

struct RenderEngine {
public:
  Camera cam;
  GLFWwindow *window = nullptr;
  VkRenderPass renderPass;
  VkDevice device = VK_NULL_HANDLE;
  uint32_t currentFrame = 0;

  // Default constructor
  RenderEngine() = default;
  // Destructor to clean up resources
  ~RenderEngine();

  // Initializes the rendering pipeline and GLFW
  void init();

  // Begins the rendering process for the current frame
  VkCommandBuffer beginRendering();

  // Ends the rendering process for the current frame
  void endRendering();

  // Checks if the rendering loop is still active
  bool running() const;

  // Cleans up all allocated Vulkan and GLFW resources
  void cleanup();

  // Callback for handling window resize events
  void framebufferSizeCallback(int nWidth, int nHeight);

  // Creates a Vulkan shader module from raw code
  VkShaderModule createShaderModule(const std::vector<char> &code) const;

  // Allocates device memory for a buffer and populates the buffer structure
  VkResult allocateMemory(VkMemoryPropertyFlags properties,
                           ShaderBuffer *pBuffers, size_t bufferCount,
                           VkDeviceMemory &memory);

  // Creates a Vulkan buffer object
  VkResult createBuffer(VkBufferUsageFlags usage, ShaderBuffer &buffer);

  // Creates a Vulkan image view from an image
  VkImageView createImageView(VkImage image, VkFormat format,
                               VkImageAspectFlags aspectFlags, VkDevice device,
                               uint32_t mipLevels);

  // Creates a Vulkan image and allocates device memory for it
  void createImage(uint32_t width, uint32_t height, uint32_t mipLevels,
                   VkFormat format, VkImageTiling tiling,
                   VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
                   VkImage &image, VkDeviceMemory &imageMemory);

  // Reads file contents into a character vector
  std::vector<char> readFile(const std::string &filename);

  // Copies data from a source buffer to a destination buffer
  VkResult copyBuffer(const ShaderBuffer &srcBuffer,
                       const ShaderBuffer &dstBuffer, VkBufferCopy copyRegion);

  // Creates a staging buffer for data transfer
  VkResult createStagingBuffer(ShaderBuffer &buffer, void **ppData);

  // Frees the staging buffer
  void freeStagingBuffer(ShaderBuffer &buffer);

  // Destroys a buffer object
  void destroyBuffer(ShaderBuffer &buffer);

  // Frees allocated device memory
  void freeMemory(VkDeviceMemory memory);

private:
  // List of required Vulkan device extensions
  const std::vector<const char *> deviceExtensions = {
      VK_KHR_SWAPCHAIN_EXTENSION_NAME};

  // Index of the current swapchain image being rendered to
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
  std::vector<VkImageView> swapchainImageViewS;
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

  // Initializes GLFW
  void initGLFW();
  // Initializes Vulkan instance and device
  void initVulkan();
  // Creates the Vulkan instance
  void createVkInstance();
  // Select the physical device to use
  void choosePhysicalDevice();
  // Creates the surface associated with the GLFW window
  void createSurface();
  // Creates the logical device
  void createLogicalDevice();
  // Gathers necessary Vulkan extensions
  void getRequiredExtensions(std::vector<const char *> &extensions);
  // Sets up the debug messenger for logging
  void setupDebugMessenger();
  // Creates the command pool
  void createCommandPool();
  // Creates the swapchain
  void createSwapchain();
  // Initializes the swapchain resources
  void initializeSwapchain();
  // Creates the render pass definition
  void createRenderPass();
  // Creates the command buffers for all swapchain images
  void createCommandBuffers();

  // Recreates the swapchain when the window is resized
  void recreateSwapchain();

  // Begins recording commands for a specific swapchain image index
  void beginRecordingCommandBuffer(VkCommandBuffer commandBuffer,
                                uint32_t index);

  // Ends the command buffer recording
  void endRecordingCommandBuffer(VkCommandBuffer commandBuffer);

  // Finds indices for graphics and present queues
  QueueIndices findGraphicsDeviceIndices(VkPhysicalDevice device);

  // Queries and returns swap chain support details for the physical device
  SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);

  // Chooses the best surface format based on available formats
  VkSurfaceFormatKHR
  chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats);

  // Chooses the best present mode based on available modes
  VkPresentModeKHR
  choosePresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes);

  // Chooses the best swap extent based on surface capabilities
  VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capablities);

  // Checks if the physical device supports necessary extensions
  bool checkDeviceExtensionSupport(VkPhysicalDevice device);

  // Checks if the physical device is suitable for rendering
  bool isDeviceSuitable(VkPhysicalDevice device);

  // Vulkan debug callback function
  static VKAPI_ATTR VkBool32 VKAPI_CALL
  debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                VkDebugUtilsMessageTypeFlagsEXT messageType,
                const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
                void *pUserData);

  // Finds the appropriate depth format for rendering
  VkFormat findDepthFormat();

  // Finds a compatible format given rendering features and tiling requirements
  VkFormat findSupportedFormat(const std::vector<VkFormat> &candidates,
                                VkImageTiling tiling,
                                VkFormatFeatureFlags features,
                                VkPhysicalDevice device);

  // Finds a valid memory type index based on required properties
  uint32_t findMemoryType(uint32_t typeFilter,
                          VkMemoryPropertyFlags properties);

  // Transitions an image layout from one state to another
  void
  transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout,
                        VkImageLayout newLayout, uint32_t mipLevels,
                        VkImageAspectFlags flags = VK_IMAGE_ASPECT_COLOR_BIT);

  // Gets a command buffer for single-time rendering commands
  VkCommandBuffer beginSingleTimeCommands();

  // Ends single-time command buffer recording
  VkResult endSingleTimeCommands(VkCommandBuffer commandBuffer);

  // Checks if the given format supports stencil components
  bool hasStencilComponent(VkFormat format);
};