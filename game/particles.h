#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "renderEngine.h"
#include "constants.h"
#include "material.h"


struct Particles {
  std::vector<glm::vec3> particlesPos;
  std::vector<glm::vec3> particlesVel;
  std::vector<float> timeToLive;
  ShaderBuffer selfMemory;

  Particles(uint32_t maxParticleCount, RenderEngine& eng, VkDescriptorPool pool);
  Particles() = default;

	uint32_t getMaxPaticleCount() const;
	
	ShaderBuffer getParticleBuffer() const;

  void update(VkCommandBuffer commandBuffer, float deltaTime,
              ShaderBuffer &stagingBuffer, void *stagingBufferMem);
	void render(VkCommandBuffer commandBuffer, const Material& mat, RenderEngine& eng);
	void free(RenderEngine&);
	
private:
	uint32_t maxParticlecCount;
	std::array<ShaderBuffer, MAX_FRAMES_IN_FLIGHT> uniformBuffers;
  void *uniformBuffersMapped;
  VkDescriptorSetLayout descriptorSetLayout;
  std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> descriptorSets;
  
  void createDescriptorSetLayout(RenderEngine& eng);
  void createDescritptorSets(RenderEngine& eng, VkDescriptorPool pool);
  void createUniformBuffers(RenderEngine& eng);
};
