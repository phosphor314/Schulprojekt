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

  /**
 * @brief Constructs a new Particles object.
 * 
 * @param maxParticleCount The maximum number of particles the system can handle.
 * @param eng Reference to the RenderEngine instance.
 * @param pool The descriptor pool to be used.
 */
Particles(uint32_t maxParticleCount, RenderEngine& eng, VkDescriptorPool pool);
  /**
 * @brief Default constructor for Particles.
 */
Particles() = default;

	/**
 * @brief Gets the maximum number of particles this system supports.
 * 
 * @return uint32_t The maximum particle capacity.
 */
uint32_t getMaxPaticleCount() const;
	
	/**
 * @brief Gets the buffer structure containing particle data.
 * 
 * @return ShaderBuffer The buffer definition for particle data.
 */
ShaderBuffer getParticleBuffer() const;

  /**
 * @brief Updates the state of all particles.
 * 
 * @param deltaTime Time elapsed since the last frame in seconds.
 */
void update(float deltaTime);
	/**
 * @brief Renders the particle system.
 * 
 * @param commandBuffer The Vulkan command buffer to record rendering commands into.
 * @param mat The material to use for rendering.
 * @param eng Reference to the RenderEngine instance.
 */
void render(VkCommandBuffer commandBuffer, const Material& mat, RenderEngine& eng);
	/**
 * @brief Frees all allocated resources for the particle system.
 * 
 * @param eng Reference to the RenderEngine instance.
 */
void free(RenderEngine&);
	
private:
	uint32_t maxParticlecCount;
	void* selfMemoryMapped;
	std::array<ShaderBuffer, MAX_FRAMES_IN_FLIGHT> uniformBuffers;
  void *uniformBuffersMapped;
  VkDescriptorSetLayout descriptorSetLayout;
  std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> descriptorSets;
  
  /**
 * @brief Updates the uniform buffer data for the particle system.
 * 
 * @param eng Reference to the RenderEngine instance.
 */
void updateUniformBuffers(RenderEngine& eng);
  
  /**
 * @brief Creates the descriptor set layout for the particle system.
 * 
 * @param eng Reference to the RenderEngine instance.
 */
void createDescriptorSetLayout(RenderEngine& eng);
  /**
 * @brief Creates descriptor sets for the particle system.
 * 
 * @param eng Reference to the RenderEngine instance.
 * @param pool The descriptor pool to use.
 */
void createDescritptorSets(RenderEngine& eng, VkDescriptorPool pool);
  /**
 * @brief Creates the uniform buffers for the particle system.
 * 
 * @param eng Reference to the RenderEngine instance.
 */
void createUniformBuffers(RenderEngine& eng);
};
