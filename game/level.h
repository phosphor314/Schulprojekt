#pragma once

#define GLFW_INCLUDE_VULKAN
#include "renderEngine.h"
#include <GLFW/glfw3.h>

struct ParticleUBO {
  glm::vec4 color;
  float size;
};

struct Particles {
  std::vector<glm::vec3> particlesPos;
  std::vector<glm::vec3> particlesVel;

  Particles(ShaderBuffer &&particleMem);

  void update(VkCommandBuffer commandBuffer, float deltaTime,
              ShaderBuffer &stagingBuffer, void *stagingBufferMem);

private:
  ShaderBuffer buff;
  std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> setLayouts;
  VkDeviceMemory uniformBuffersMemory;
  std::array<ShaderBuffer, MAX_FRAMES_IN_FLIGHT> uniformBuffers;
  void *uniformBuffersMapped;
};

struct LevelLoader {};
