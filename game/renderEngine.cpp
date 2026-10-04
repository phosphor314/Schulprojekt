#include "renderEngine.h"
#include "../tiny_obj_loader.h"
#include "GLFW/glfw3.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include <vector>
#include <vulkan/vulkan_core.h>

// Defines validation layers to use for Vulkan debugging.
#ifndef NDEBUG
std::array<const char *const, 1> VK_VALIDATION_LAYERS = {
    "VK_LAYER_KHRONOS_validation"};
#else
std::array<const char *const, 0> VK_VALIDATION_LAYERS = {};
#endif

// Implements the equality operator for the Vertex struct.
bool Vertex::operator==(const Vertex &other) const {
  return position == other.position && normal == other.normal &&
         color == other.color;
}

// Calculates the model-view-projection transformation matrix for the camera.
glm::mat4 Camera::getTransformationMat(float aspectRatio) {
  glm::mat4 proj = glm::perspective(fov, aspectRatio, 0.05f, 100.0f);
  proj[1][1] *= -1;
  return proj * glm::lookAt(eye, eye + forward, up);
}

// Destructor for RenderEngine.
RenderEngine::~RenderEngine() {}

// Initializes the rendering engine, setting up GLFW and Vulkan components.
void RenderEngine::init() {
  cam.fov = glm::half_pi<float>();
  initGLFW();
  initVulkan();
}

// Begins the rendering process for the current frame. Handles image acquisition and command buffer recording start.
VkCommandBuffer RenderEngine::beginRendering() {
  glfwPollEvents();

  // Wait for the previous frame's fence to be complete.
  VkVerify(vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_FALSE, UINT64_MAX));

  // Acquire the next image from the swap chain.
  swapchainImageResult = vkAcquireNextImageKHR(
      device, swapchain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);
  if (swapchainImageResult == VK_ERROR_OUT_OF_DATE_KHR) {
    recreateSwapchain();
    return VK_NULL_HANDLE;
  } else if (swapchainImageResult != VK_SUBOPTIMAL_KHR &&
             swapchainImageResult != VK_SUCCESS) {
    throw std::runtime_error("Failed to acquire swap chain image!");
  }

  // Reset the fence for the current frame.
  VkVerify(vkResetFences(device, 1, &inFlightFences[currentFrame]));

  // Reset the command buffer.
  VkVerify(vkResetCommandBuffer(commandBuffers[currentFrame], 0));
  // Begin recording commands for this image.
  beginRecordingCommandBuffer(commandBuffers[currentFrame], imageIndex);
  return commandBuffers[currentFrame];
}

// Ends the rendering process for the current frame. Submits and presents the rendered image.
void RenderEngine::endRendering() {
  endRecordingCommandBuffer(commandBuffers[currentFrame]);

  VkSubmitInfo submitInfo{.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO};
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffers[currentFrame];

  // Semaphore to signal that rendering is finished for the current image.
  std::array<VkSemaphore, 1> waitSemaphores = {
      imageAvailableSemaphores[currentFrame]};
  std::array<VkPipelineStageFlags, 1> waitStages = {
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
  submitInfo.waitSemaphoreCount = waitSemaphores.size();
  submitInfo.pWaitSemaphores = waitSemaphores.data();
  submitInfo.pWaitDstStageMask = waitStages.data();

  // Semaphore to signal that the image is ready for presentation.
  std::array<VkSemaphore, 1> signalSemaphores = {
      renderFinishedSemaphores[imageIndex]};
  submitInfo.signalSemaphoreCount = signalSemaphores.size();
  submitInfo.pSignalSemaphores = signalSemaphores.data();

  // Submit the command buffer to the graphics queue.
  VkVerify(vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]))

  // Present the image to the swap chain.
  VkPresentInfoKHR presentInfo{.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
  presentInfo.waitSemaphoreCount = 1;
  presentInfo.pWaitSemaphores = &renderFinishedSemaphores[imageIndex];
  presentInfo.swapchainCount = 1;
  presentInfo.pSwapchains = &swapchain;
  presentInfo.pImageIndices = &imageIndex;

  VkVerify(vkQueuePresentKHR(presentQueue, &presentInfo))

      if (swapchainImageResult == VK_ERROR_OUT_OF_DATE_KHR ||
          swapchainImageResult == VK_SUBOPTIMAL_KHR || framebufferResized) {
    framebufferResized = false;
    recreateSwapchain();
  }
  else if (swapchainImageResult != VK_SUCCESS) {
    throw std::runtime_error("failed to present swap chain image!");
  }

  currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

// Checks if the rendering loop should continue based on window state.
bool RenderEngine::running() const { return !glfwWindowShouldClose(window); }

// Performs necessary cleanup of all Vulkan and GLFW resources.
void RenderEngine::cleanup() {
  // Wait for the GPU to finish all pending operations.
  vkDeviceWaitIdle(device);

  // Destroy the render pass.
  vkDestroyRenderPass(device, renderPass, nullptr);

  // Destroy the debug messenger if it was created.
  auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
      vkInstance, "vkDestroyDebugUtilsMessengerEXT");
  if (func != nullptr) {
    func(vkInstance, debugMessenger, nullptr);
  }

  // Destroy the depth/stencil image and its view.
  vkDestroyImageView(device, zImageView, nullptr);
  vkDestroyImage(device, zImage, nullptr);
  vkFreeMemory(device, zImageMemory, nullptr);

  // Destroy the swap chain.
  vkDestroySwapchainKHR(device, swapchain, nullptr);

  // Free semaphores, framebuffers, and image views for the swap chain.
  for (size_t i = 0; i < renderFinishedSemaphores.size(); ++i) {
    vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
    vkDestroyFramebuffer(device, swapchainFramebuffers[i], nullptr);
    vkDestroyImageView(device, swapchainImageViewS[i], nullptr);
    vkDestroyFence(device, inFlightFences[i], nullptr);
    vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
  }

  // Free command buffers associated with the command pool.
  vkFreeCommandBuffers(device, pool, MAX_FRAMES_IN_FLIGHT, commandBuffers.data());

  // Destroy the command pool.
  vkDestroyCommandPool(device, pool, nullptr);

  // Destroy the logical device.
  vkDestroyDevice(device, nullptr);

  // Destroy the Vulkan instance.
  vkDestroyInstance(vkInstance, nullptr);

  // Destroy the GLFW window.
  glfwDestroyWindow(window);

  // Terminate GLFW.
  glfwTerminate();
}

// Callback executed when the window size changes. Sets the framebufferResized flag.
void RenderEngine::framebufferSizeCallback(int nWidth, int nHeight) {
  framebufferResized = true;
}

// Initializes GLFW, setting up the window and input modes.
void RenderEngine::initGLFW() {
  // Hint to use X11 platform backend.
  glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);

  if (!glfwInit()) {
    throw std::runtime_error("failed to initialize GLFW");
  }

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

  window = glfwCreateWindow(WIDTH, HEIGHT, "LOL", NULL, NULL);

  if (!window) {
    glfwTerminate();
    throw std::runtime_error("failed to create window");
  }

  // Disable cursor blinking.
  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
}

// Initializes all Vulkan components: instance, device, surface, swapchain, etc.
void RenderEngine::initVulkan() {
  // Create the Vulkan instance.
  createVkInstance();

  #ifndef NDEBUG
  // Setup debugging tools.
  setupDebugMessenger();
  #endif

  // Create the surface for the window.
  createSurface();
  // Select a suitable physical GPU device.
  choosePhysicalDevice();
  // Create the logical device.
  createLogicalDevice();
  // Create the command pool.
  createCommandPool();
  // Create the swap chain.
  createSwapchain();
  // Create the render pass definition.
  createRenderPass();
  // Initialize swap chain resources (framebuffers, semaphores, etc.).
  initializeSwapchain();
  // Create command buffers for frame recording.
  createCommandBuffers();
}

// Creates the Vulkan instance and sets up required extensions.
void RenderEngine::createVkInstance() {
  VkApplicationInfo appInfo{};
  appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  appInfo.apiVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
  appInfo.applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
  appInfo.engineVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
  appInfo.pApplicationName = "Homebrew game";
  appInfo.pEngineName = "Homebrew engine";

  VkInstanceCreateInfo cInfo{};
  cInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  cInfo.pApplicationInfo = &appInfo;
  cInfo.ppEnabledLayerNames = VK_VALIDATION_LAYERS.data();
  cInfo.enabledLayerCount = VK_VALIDATION_LAYERS.size();
  std::vector<const char *> extensions;
  getRequiredExtensions(extensions);
  cInfo.ppEnabledExtensionNames = extensions.data();
  cInfo.enabledExtensionCount = extensions.size();

  VkVerify(vkCreateInstance(&cInfo, nullptr, &vkInstance));
}

// Searches for a suitable physical device that supports necessary features.
void RenderEngine::choosePhysicalDevice() {
  uint32_t physicalDeviceCount;
  vkEnumeratePhysicalDevices(vkInstance, &physicalDeviceCount, nullptr);
  std::vector<VkPhysicalDevice> devices;
  devices.resize(physicalDeviceCount);
  vkEnumeratePhysicalDevices(vkInstance, &physicalDeviceCount, devices.data());

  for (const VkPhysicalDevice &dev : devices) {
    if (isDeviceSuitable(dev)) {
      physicalDevice = dev;
      break;
    }
  }

  if (physicalDevice == VK_NULL_HANDLE) {
    throw std::runtime_error("could not find suitable physical device");
  }
}

// Creates a surface object for the GLFW window using the Vulkan instance.
void RenderEngine::createSurface() {
  VkResult res = glfwCreateWindowSurface(vkInstance, window, nullptr, &surface);
  VkVerify(res);
}

// Creates the logical device based on the physical device, setting up queue families.
void RenderEngine::createLogicalDevice() {
  QueueIndices indices = findGraphicsDeviceIndices(physicalDevice);

  std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
  std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsQueue.value(),
                                             indices.presentQueue.value()};

  float queuePriority = 1.0f;
  for (uint32_t queueFamily : uniqueQueueFamilies) {
    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = queueFamily;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;
    queueCreateInfos.push_back(queueCreateInfo);
  }

  VkPhysicalDeviceFeatures deviceFeatures{};
  deviceFeatures.samplerAnisotropy = VK_TRUE;

  VkDeviceCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  createInfo.pQueueCreateInfos = queueCreateInfos.data();
  createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
  createInfo.pEnabledFeatures = &deviceFeatures;

  createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
  createInfo.ppEnabledExtensionNames = deviceExtensions.data();

  // Enable validation layers if present.
  if (VK_VALIDATION_LAYERS.size() != 0) {
    createInfo.enabledLayerCount = static_cast<uint32_t>(VK_VALIDATION_LAYERS.size());
    createInfo.ppEnabledLayerNames = VK_VALIDATION_LAYERS.data();
  } else {
    createInfo.enabledLayerCount = 0;
  }

  if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
    throw std::runtime_error("failed to create logical device!");
  }

  // Store the queue indices found for graphics and presentation.
  queueIndices = indices;

  vkGetDeviceQueue(device, indices.graphicsQueue.value(), 0, &graphicsQueue);
  vkGetDeviceQueue(device, indices.presentQueue.value(), 0, &presentQueue);
  vkGetDeviceQueue(device, indices.computeQueue.value(), 0, &computeQueue);
}

// Retrieves necessary Vulkan instance extensions from GLFW.
void RenderEngine::getRequiredExtensions(std::vector<const char *> &extensions) {
  uint32_t count;
  const char **ext = glfwGetRequiredInstanceExtensions(&count);
  for (int i = 0; i < count; ++i) {
    extensions.push_back(ext[i]);
  }
  // Always enable the debug utils extension.
  extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
}

// Sets up the debug messenger for logging Vulkan errors and warnings.
void RenderEngine::setupDebugMessenger() {
  VkDebugUtilsMessengerCreateInfoEXT createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
  createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
  createInfo.messageType =
      VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT;
  createInfo.pfnUserCallback = debugCallback;

  // Create the messenger using the vkCreateDebugUtilsMessengerEXT function pointer.
  auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
      vkInstance, "vkCreateDebugUtilsMessengerEXT");
  if (func) {
    VkVerify(func(vkInstance, &createInfo, nullptr, &debugMessenger));
  } else {
    throw std::runtime_error(
        "failed to retrieve debug messenger creation function");
  }
}

// Creates the command pool for allocating command buffers.
void RenderEngine::createCommandPool() {
  VkCommandPoolCreateInfo cInfo{};
  cInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  // Allows command buffers to be reset without destroying and recreating them.
  cInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  cInfo.queueFamilyIndex = queueIndices.graphicsQueue.value();

  VkVerify(vkCreateCommandPool(device, &cInfo, nullptr, &pool));
}

// Creates the swap chain interface for the window, defining supported image formats and presentation modes.
void RenderEngine::createSwapchain() {
  // Query the physical device for supported surface capabilities.
  SwapChainSupportDetails supportDetails =
      querySwapChainSupport(physicalDevice);

  // Select the best format, present mode, and extent from the available options.
  VkSurfaceFormatKHR surfaceFormat =
      chooseSurfaceFormat(supportDetails.formats);
  VkPresentModeKHR presentMode = choosePresentMode(supportDetails.presentModes);
  VkExtent2D extent = chooseSwapExtent(supportDetails.capabilities);
  uint32_t imageCount = supportDetails.capabilities.minImageCount + 1;
  if (imageCount > supportDetails.capabilities.maxImageCount &&
      supportDetails.capabilities.maxImageCount > 0) {
    imageCount = supportDetails.capabilities.maxImageCount;
  }

  // Setup the depth/stencil image and view.
  depthFormat = findDepthFormat();
  createImage(extent.width, extent.height, 1, depthFormat,
              VK_IMAGE_TILING_OPTIMAL,
              VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, zImage, zImageMemory);
  zImageView = createImageView(zImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT,
                               device, 1);

  // Determine queue types for sharing.
  std::array<uint32_t, 2> queueFamilyIndices = {
      queueIndices.graphicsQueue.value(), queueIndices.presentQueue.value()};

  // Create the swap chain object.
  VkSwapchainCreateInfoKHR createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  createInfo.surface = surface;
  createInfo.minImageCount = imageCount;
  createInfo.imageFormat = surfaceFormat.format;
  createInfo.imageColorSpace = surfaceFormat.colorSpace;
  createInfo.imageExtent = extent;
  createInfo.imageArrayLayers = 1;
  createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

  // Configure image sharing mode based on queue types.
  if (queueIndices.graphicsQueue != queueIndices.presentQueue) {
    createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    createInfo.queueFamilyIndexCount = 2;
    createInfo.pQueueFamilyIndices = queueFamilyIndices.data();
  } else {
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  }

  createInfo.preTransform = supportDetails.capabilities.currentTransform;
  createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  createInfo.presentMode = presentMode;
  createInfo.clipped = VK_TRUE;
  createInfo.oldSwapchain = VK_NULL_HANDLE;

  VkVerify(vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapchain));

  // Retrieve swap chain images.
  VkResult result = vkGetSwapchainImagesKHR(
      device, swapchain, &imageCount, nullptr);
  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
    // If acquisition fails, we proceed assuming the resize handler will catch it.
  } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
    throw std::runtime_error("Failed to retrieve sufficciently many swap chain images!");
  }

  swapchainImages.resize(imageCount);
  // Get the list of swap chain images.
  VkVerify(vkGetSwapchainImagesKHR(device, swapchain, &imageCount,
                                    swapchainImages.data()));
  swapchainImageFormat = surfaceFormat.format;
  swapchainExtent = extent;

  // Create image views for swap chain images.
  swapchainImageViewS.resize(imageCount);
  for (uint32_t i = 0; i < swapchainImages.size(); i++) {
    swapchainImageViewS[i] =
        createImageView(swapchainImages[i], swapchainImageFormat,
                        VK_IMAGE_ASPECT_COLOR_BIT, device, 1);
  }

  // Create semaphores and fences for synchronization.
  VkSemaphoreCreateInfo semaphoreInfo{};
  semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  VkFenceCreateInfo fenceInfo{};
  fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  renderFinishedSemaphores.resize(imageCount);
  imageAvailableSemaphores.resize(imageCount);
  inFlightFences.resize(imageCount);
  for (size_t i = 0; i < imageCount; ++i) {
    VkVerify(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]))
        VkVerify(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]))
        VkVerify(
            vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]));
  }
}

// Sets up the framebuffers, binding the swap chain images to the render pass.
void RenderEngine::initializeSwapchain() {
  swapchainFramebuffers.resize(swapchainImages.size());
  for (int i = 0; i < swapchainFramebuffers.size(); ++i) {
    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = renderPass;
    // Attach the swap chain image and the depth stencil image.
    std::array<VkImageView, 2> attachments = {swapchainImageViewS[i], zImageView};
    framebufferInfo.attachmentCount = attachments.size();
    framebufferInfo.pAttachments = attachments.data();
    framebufferInfo.width = swapchainExtent.width;
    framebufferInfo.height = swapchainExtent.height;
    framebufferInfo.layers = 1;

    VkVerify(vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapchainFramebuffers[i]));
  }
}

// Defines and creates the render pass, specifying attachment formats and subpasses.
void RenderEngine::createRenderPass() {
  VkAttachmentDescription colorAttachment{};
  colorAttachment.format = swapchainImageFormat;
  colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
  colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  VkAttachmentReference colorAttachmentRef{};
  colorAttachmentRef.attachment = 0;
  colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkAttachmentDescription depthAttachment{};
  depthAttachment.format = depthFormat;
  depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
  depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkAttachmentReference depthAttachmentRef{};
  depthAttachmentRef.attachment = 1;
  depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  std::array<VkSubpassDescription, 2> subpasses{};
  subpasses[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpasses[0].colorAttachmentCount = 1;
  subpasses[0].pColorAttachments = &colorAttachmentRef;
  subpasses[0].pDepthStencilAttachment = &depthAttachmentRef;

  subpasses[1].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpasses[1].colorAttachmentCount = 1;
  subpasses[1].pColorAttachments = &colorAttachmentRef;
  subpasses[1].pDepthStencilAttachment = &depthAttachmentRef;

  std::array<VkSubpassDependency, 1> subpassDependencies;
  subpassDependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
  subpassDependencies[0].srcSubpass = 0;
  subpassDependencies[0].dstSubpass = 1;
  subpassDependencies[0].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  subpassDependencies[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
                                  VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
  subpassDependencies[0].srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  subpassDependencies[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;

  VkRenderPassCreateInfo renderPassInfo{
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
  std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};
  renderPassInfo.attachmentCount = attachments.size();
  renderPassInfo.pAttachments = attachments.data();
  renderPassInfo.subpassCount = subpasses.size();
  renderPassInfo.pSubpasses = subpasses.data();
  renderPassInfo.dependencyCount = subpassDependencies.size();
  renderPassInfo.pDependencies = subpassDependencies.data();

  VkVerify(vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass));
}

// Allocates and manages command buffers for frame recording.
void RenderEngine::createCommandBuffers() {
  commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
  VkCommandBufferAllocateInfo allocInfo{
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
  allocInfo.commandBufferCount = commandBuffers.size();
  allocInfo.commandPool = pool;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data());
}

// Recreates the swap chain when the window is resized or parameters change.
void RenderEngine::recreateSwapchain() {
  int width = 0, height = 0;
  // Wait for window resize event to get new dimensions.
  while (width == 0 || height == 0) {
    glfwGetFramebufferSize(window, &width, &height);
    glfwWaitEvents();
  }

  // Wait for the GPU to idle before destruction.
  vkDeviceWaitIdle(device);

  // Destroy old swap chain and associated resources.
  vkDestroySwapchainKHR(device, swapchain, nullptr);

  for (size_t i = 0; i < renderFinishedSemaphores.size(); ++i) {
    vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
    vkDestroyFramebuffer(device, swapchainFramebuffers[i], nullptr);
    vkDestroyImageView(device, swapchainImageViewS[i], nullptr);
    vkDestroyFence(device, inFlightFences[i], nullptr);
    vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
  }

  // Recreate the swap chain and re-initialize its components.
  createSwapchain();
  initializeSwapchain();
}

// Begins the command buffer recording process for a given swap chain image index.
void RenderEngine::beginRecordingCommandBuffer(VkCommandBuffer commandBuffer,
                                            uint32_t imageIndex) {
  VkCommandBufferBeginInfo beginInfo{
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  VkVerify(vkBeginCommandBuffer(commandBuffer, &beginInfo));

  // Set the viewport.
  VkViewport viewport{};
  viewport.height = swapchainExtent.height;
  viewport.width = swapchainExtent.width;
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  viewport.maxDepth = 1.0f;
  viewport.minDepth = 0.0f;
  vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

  // Set the scissor rectangle to match the swap chain extent.
  VkRect2D scissor{};
  scissor.extent = swapchainExtent;
  vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

  // Start the render pass definition.
  VkRenderPassBeginInfo renderPassInfo{
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
  renderPassInfo.renderPass = renderPass;
  renderPassInfo.renderArea.extent = swapchainExtent;
  std::array<VkClearValue, 2> clearValues;
  clearValues[0].color = {0.0f, 0.0f, 0.0f, 0.0f};
  clearValues[1].depthStencil.depth = 1.0f;
  renderPassInfo.clearValueCount = clearValues.size();
  renderPassInfo.pClearValues = clearValues.data();
  renderPassInfo.framebuffer = swapchainFramebuffers[imageIndex];

  vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
}

// Ends the command buffer recording process.
void RenderEngine::endRecordingCommandBuffer(VkCommandBuffer commandBuffer) {
  vkCmdEndRenderPass(commandBuffer);
  VkVerify(vkEndCommandBuffer(commandBuffer));
}

// Finds and returns the queue indices for graphics, present, and compute devices.
QueueIndices RenderEngine::findGraphicsDeviceIndices(VkPhysicalDevice device) {
  QueueIndices indices;

  uint32_t queueFamilyCount = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
  std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

  int i = 0;
  for (const auto &queueFamily : queueFamilies) {
    // Check for graphics capabilities.
    if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
      indices.graphicsQueue = i;
    }

    // Check for presentation capabilities.
    VkBool32 presentSupport = false;
    vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
    if (presentSupport) {
      indices.presentQueue = i;
    }

    // Check for compute capabilities.
    if (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT) {
      indices.computeQueue = i;
    }

    // Stop searching once all required indices are found.
    if (indices.complete()) {
      break;
    }

    i++;
  }

  return indices;
}

// Queries the physical device for swap chain support details (formats, present modes).
SwapChainSupportDetails RenderEngine::querySwapChainSupport(VkPhysicalDevice device) {
  SwapChainSupportDetails details;

  // Get supported surface capabilities.
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

  // Get supported surface formats.
  uint32_t formatCount;
  vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
  if (formatCount) {
    details.formats.resize(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
  }

  // Get supported present modes.
  uint32_t presentModeCount;
  vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
  if (presentModeCount) {
    details.presentModes.resize(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(
        device, surface, &presentModeCount, details.presentModes.data());
  }

  return details;
}

// Chooses the best surface format, preferring nonlinear 32bit RGBA formats.
VkSurfaceFormatKHR RenderEngine::chooseSurfaceFormat(
    const std::vector<VkSurfaceFormatKHR> &availableFormats) {
  // try to find a format with nonlinear 32bit RGBA
  for (const auto &availableFormat : availableFormats) {
    if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB &&
        availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      return availableFormat;
    }
  }

  // Fallback to the first available format.
  return availableFormats[0];
}

// Chooses the best present mode, prioritizing Mailbox for low latency.
VkPresentModeKHR RenderEngine::choosePresentMode(
    const std::vector<VkPresentModeKHR> &availablePresentModes) {
  // try to find MAILBOX_KHR
  for (const auto &availablePresentMode : availablePresentModes) {
    if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
      return availablePresentMode;
    }
  }

  // fall back to FIFO_KHR if MAILBOX_KHR is unavailable.
  return VK_PRESENT_MODE_FIFO_KHR;
}

// Calculates the optimal swap chain extent based on surface capabilities.
VkExtent2D RenderEngine::chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capablities) {
  // If current extent is already set, use it.
  if (capablities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
    return capablities.currentExtent;
  } else {
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);

    VkExtent2D actualExtent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};

    // Clamp the dimensions within the min/max allowed by the surface.
    actualExtent.width =
        std::clamp(actualExtent.width, capablities.minImageExtent.width, capablities.maxImageExtent.width);
    actualExtent.height =
        std::clamp(actualExtent.height, capablities.minImageExtent.height, capablities.maxImageExtent.height);

    return actualExtent;
  }
}

// Checks if the physical device supports all required Vulkan extensions.
bool RenderEngine::checkDeviceExtensionSupport(VkPhysicalDevice device) {
  uint32_t deviceExtensionCount;
  vkEnumerateDeviceExtensionProperties(device, nullptr, &deviceExtensionCount, nullptr);
  std::vector<VkExtensionProperties> extensionProperties(deviceExtensionCount);
  vkEnumerateDeviceExtensionProperties(device, nullptr, &deviceExtensionCount, extensionProperties.data());

  std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

  for (const VkExtensionProperties &extension : extensionProperties) {
    requiredExtensions.erase(extension.extensionName);
  }

  return requiredExtensions.empty();
}

// Checks if a physical device is suitable for rendering tasks.
bool RenderEngine::isDeviceSuitable(VkPhysicalDevice device) {
  QueueIndices indices = findGraphicsDeviceIndices(device);
  bool extensionsSupported = checkDeviceExtensionSupport(device);
  bool swapChainAdequate = false;

  VkPhysicalDeviceFeatures supportedFeatures;
  vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

  if (extensionsSupported) {
    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
    swapChainAdequate = swapChainSupport.isComplete();
  }

  // Check for complete queue support, extensions, swapchain capability, anisotropic filtering, and 64-bit shader support.
  return indices.complete() && extensionsSupported && swapChainAdequate &&
         supportedFeatures.samplerAnisotropy && supportedFeatures.shaderInt64;
}

// Vulkan debug callback function to print messages to stderr.
VKAPI_ATTR VkBool32 VKAPI_CALL RenderEngine::debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
    void *pUserData) {
  std::cerr << "\n" << pCallbackData->pMessage << "\n";
  return VK_FALSE;
}

// Finds a suitable depth/stencil format for rendering.
VkFormat RenderEngine::findDepthFormat() {
  return findSupportedFormat(
      {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT,
       VK_FORMAT_D24_UNORM_S8_UINT},
      VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT,
      physicalDevice);
}

// Searches for a format that satisfies tiling and feature requirements.
VkFormat RenderEngine::findSupportedFormat(
    const std::vector<VkFormat> &candidates, VkImageTiling tiling,
    VkFormatFeatureFlags features, VkPhysicalDevice device) {
  for (VkFormat format : candidates) {
    VkFormatProperties props;
    vkGetPhysicalDeviceFormatProperties(device, format, &props);

    // Check if the format supports the required tiling and features.
    if (tiling == VK_IMAGE_TILING_LINEAR &&
        (props.linearTilingFeatures & features) == features) {
      return format;
    } else if (tiling == VK_IMAGE_TILING_OPTIMAL &&
               (props.optimalTilingFeatures & features) == features) {
      return format;
    }
  }

  throw std::runtime_error("failed to find supported format!");
}

// Creates a Vulkan image and allocates necessary device memory for it.
void RenderEngine::createImage(uint32_t width, uint32_t height,
                                uint32_t mipLevels, VkFormat format,
                                VkImageTiling tiling, VkImageUsageFlags usage,
                                VkMemoryPropertyFlags properties, VkImage &image,
                                VkDeviceMemory &imageMemory) {
  // Create the image handle.
  VkImageCreateInfo imageInfo{};
  imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imageInfo.imageType = VK_IMAGE_TYPE_2D;
  imageInfo.extent.width = width;
  imageInfo.extent.height = height;
  imageInfo.extent.depth = 1;
  imageInfo.mipLevels = mipLevels;
  imageInfo.arrayLayers = 1;
  imageInfo.format = format;
  imageInfo.tiling = tiling;
  imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  imageInfo.usage = usage;
  imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
  imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
    throw std::runtime_error("failed to create image!");
  }

  // Get memory requirements for the image.
  VkMemoryRequirements memRequirements;
  vkGetImageMemoryRequirements(device, image, &memRequirements);

  // Allocate device memory for the image.
  VkMemoryAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocInfo.allocationSize = memRequirements.size;
  allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

  if (vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
    throw std::runtime_error("failed to allocate image memory!");
  }

  // Bind the allocated memory to the image.
  if (vkBindImageMemory(device, image, imageMemory, 0) != VK_SUCCESS) {
    throw std::runtime_error("failed to bind image memory!");
  }
}

// Finds a suitable memory type index based on required properties.
uint32_t RenderEngine::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
  VkPhysicalDeviceMemoryProperties memProperties;
  vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

  for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
    // Check if the memory type matches the requested filter and properties.
    if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags &
                                     properties) == properties) {
      return i;
    }
  }

  throw std::runtime_error("failed to find suitable memory type!");
}

// Transitions an image layout from one state to another, ensuring proper synchronization.
void RenderEngine::transitionImageLayout(VkImage image, VkFormat format,
                                      VkImageLayout oldLayout,
                                      VkImageLayout newLayout,
                                      uint32_t mipLevels,
                                      VkImageAspectFlags flags) {
  VkCommandBuffer commandBuffer = beginSingleTimeCommands();

  // Define the memory barrier.
  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = oldLayout;
  barrier.newLayout = newLayout;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = image;
  barrier.subresourceRange.aspectMask = flags;
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = mipLevels;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;
  barrier.srcAccessMask = 0; // TODO
  barrier.dstAccessMask = 0; // TODO

  // Adjust aspect mask if stencil component is present.
  if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL ||
      newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL) {
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

    if (hasStencilComponent(format)) {
      barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }
  } else {
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  }

  // Set access masks and pipeline stages based on layout transition types.
  if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
      newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
  } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
             newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  } else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
             newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  } else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
             newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL) {
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  } else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
             newLayout == VK_IMAGE_LAYOUT_GENERAL) {
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  } else {
    throw std::invalid_argument("unsupported layout transition!");
  }

  vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0,
                       nullptr, 0, nullptr, 1, &barrier);

  endSingleTimeCommands(commandBuffer);
}

// Creates a single-time command buffer for one-off submission tasks.
VkCommandBuffer RenderEngine::beginSingleTimeCommands() {
  VkCommandBufferAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandPool = pool;
  allocInfo.commandBufferCount = 1;

  VkCommandBuffer commandBuffer;
  vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  vkBeginCommandBuffer(commandBuffer, &beginInfo);

  return commandBuffer;
}

// Ends single-time command buffer recording and submits it to the queue for immediate execution.
VkResult RenderEngine::endSingleTimeCommands(VkCommandBuffer commandBuffer) {
  VkResult result = vkEndCommandBuffer(commandBuffer);
  if (result != VK_SUCCESS) {
    return result;
  }

  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer;

  result = vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
  if (result != VK_SUCCESS) {
    return result;
  }

  // Wait for the command to finish execution.
  result = vkQueueWaitIdle(graphicsQueue);

  // Free the single-use command buffer.
  vkFreeCommandBuffers(device, pool, 1, &commandBuffer);

  return result;
}

// Checks if the given format supports stencil components.
bool RenderEngine::hasStencilComponent(VkFormat format) {
  return format == VK_FORMAT_D32_SFLOAT_S8_UINT ||
         format == VK_FORMAT_D24_UNORM_S8_UINT;
}

// Loads file contents from disk into a vector of characters.
std::vector<char> RenderEngine::readFile(const std::string &filename) {
  std::ifstream file(filename, std::ios_base::binary | std::ios_base::ate);
  if (!file.is_open()) {
    throw std::runtime_error(strerror(errno));
  }

  size_t fileSize = (size_t)file.tellg();
  std::vector<char> buffer(fileSize);
  file.seekg(0);
  file.read(buffer.data(), fileSize);
  file.close();

  return buffer;
}

// Creates a Vulkan shader module from raw shader code provided in a character vector.
VkShaderModule RenderEngine::createShaderModule(const std::vector<char> &code) const {
  VkShaderModuleCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  createInfo.codeSize = code.size();
  // Cast vector data to the expected format for Vulkan.
  createInfo.pCode = reinterpret_cast<const uint32_t *>(code.data());

  VkShaderModule shaderModule;
  if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
    throw std::runtime_error("failed to create shader module!");
  }

  return shaderModule;
}

// Creates a generic Vulkan buffer object.
VkResult RenderEngine::createBuffer(VkBufferUsageFlags usage, ShaderBuffer &buffer) {
  VkBufferCreateInfo bufferInfo{};
  bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferInfo.size = buffer.size;
  bufferInfo.usage = usage;
  bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  return vkCreateBuffer(device, &bufferInfo, nullptr, &buffer.buffer);
}

// Allocates device memory for a buffer and binds it.
VkResult RenderEngine::allocateMemory(VkMemoryPropertyFlags properties,
                                     ShaderBuffer *pBuffers,
                                     size_t bufferCount,
                                     VkDeviceMemory &memory) {
  uint32_t memoryTypeBits = UINT32_MAX;
  VkDeviceSize bufferOffsetAccumulator = 0;
  VkMemoryRequirements requirements;
  for (size_t i = 0; i < bufferCount; ++i) {
    // Get memory requirements for the current buffer.
    vkGetBufferMemoryRequirements(device, pBuffers[i].buffer, &requirements);

    // Align the starting offset.
    if (bufferOffsetAccumulator % requirements.alignment != 0) {
      bufferOffsetAccumulator =
          (bufferOffsetAccumulator / requirements.alignment + 1) *
          requirements.alignment;
    }
    pBuffers[i].offset = bufferOffsetAccumulator;
    bufferOffsetAccumulator += requirements.size;

    // Accumulate memory type requirements.
    memoryTypeBits = memoryTypeBits & requirements.memoryTypeBits;
  }

  // Allocate the total memory block needed.
  VkMemoryAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocInfo.allocationSize =
      pBuffers[bufferCount - 1].offset + requirements.size;
  allocInfo.memoryTypeIndex = findMemoryType(memoryTypeBits, properties);

  VkResult result = vkAllocateMemory(device, &allocInfo, nullptr, &memory);
  if (result != VK_SUCCESS) {
    return result;
  }

  // Bind the allocated memory to each buffer.
  for (size_t i = 0; i < bufferCount; ++i) {
    pBuffers[i].bufferMemory = memory;
    result = vkBindBufferMemory(device, pBuffers[i].buffer, memory,
                                 pBuffers[i].offset);
    if (result != VK_SUCCESS) {
      return result;
    }
  }

  return result;
}

// Copies data between two buffers using a single time command submission.
VkResult RenderEngine::copyBuffer(const ShaderBuffer &srcBuffer,
                                   const ShaderBuffer &dstBuffer,
                                   VkBufferCopy copyRegion) {
  VkCommandBuffer commandBuffer = beginSingleTimeCommands();

  vkCmdCopyBuffer(commandBuffer, srcBuffer.buffer, dstBuffer.buffer, 1, &copyRegion);

  return endSingleTimeCommands(commandBuffer);
}

// Creates a host-visible staging buffer and maps it for data transfer.
VkResult RenderEngine::createStagingBuffer(ShaderBuffer &buffer,
                                          void **ppData) {
  // Create the buffer object for the staging area.
  VkResult result = createBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, buffer);
  if (result != VK_SUCCESS) {
    return result;
  }

  // Allocate host-visible memory for the staging buffer.
  result = allocateMemory(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                         VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                         &buffer, 1, buffer.bufferMemory);
  if (result != VK_SUCCESS) {
    return result;
  }

  // Map the memory to get a pointer for CPU access.
  return vkMapMemory(device, buffer.bufferMemory, 0, buffer.size, 0, ppData);
}

// Frees the staging buffer's memory and destroys the buffer object.
void RenderEngine::freeStagingBuffer(ShaderBuffer &buffer) {
  vkUnmapMemory(device, buffer.bufferMemory);
  vkDestroyBuffer(device, buffer.buffer, nullptr);
  vkFreeMemory(device, buffer.bufferMemory, nullptr);
}

// Destroys a buffer object without freeing its memory (used when memory is managed separately).
void RenderEngine::destroyBuffer(ShaderBuffer &buffer) {
  vkDestroyBuffer(device, buffer.buffer, nullptr);
}

// Frees a block of allocated device memory.
void RenderEngine::freeMemory(VkDeviceMemory memory) {
  vkFreeMemory(device, memory, nullptr);
}