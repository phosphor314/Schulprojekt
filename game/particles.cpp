#include "BufferStructs.h"
#include "particles.h"
#include <memory.h>


/**
 * @brief Constructs a new Particles object.
 * 
 * @param maxParticleCount The maximum number of particles the system can handle.
 * @param eng Reference to the RenderEngine instance.
 * @param pool The descriptor pool to be used.
 */
Particles::Particles(uint32_t maxParticleCount, RenderEngine& eng, VkDescriptorPool pool) {
  this->maxParticlecCount = maxParticleCount;
  
  selfMemory.size = sizeof(glm::vec3)*maxParticleCount;
  eng.createBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, selfMemory);
  eng.allocateMemory(
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
    &selfMemory, 
    1, 
    selfMemory.bufferMemory);
	vkMapMemory(eng.device, selfMemory.bufferMemory, 0, selfMemory.size, 0, &selfMemoryMapped);

  createUniformBuffers(eng);
  createDescriptorSetLayout(eng);
  createDescritptorSets(eng, pool);
}

/**
 * @brief Renders the particle system.
 * 
 * @param commandBuffer The Vulkan command buffer to record rendering commands into.
 * @param mat The material to use for rendering.
 * @param eng Reference to the RenderEngine instance.
 */
void Particles::render(VkCommandBuffer commandBuffer, const Material& mat, RenderEngine& eng){
  updateUniformBuffers(eng);
  
  VkDeviceSize ZERO = 0;
  vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mat.getPipelineLayout(), 1, 1, &descriptorSets[eng.currentFrame], 0, nullptr);
  vkCmdBindVertexBuffers(commandBuffer, 0, 1, &selfMemory.buffer, &ZERO);
  vkCmdDraw(commandBuffer, particlesPos.size(), 1, 0, 0);  
}

/**
 * @brief Updates the state of all particles (position, velocity, lifetime).
 * 
 * @param deltaTime Time elapsed since the last frame in seconds.
 */
void Particles::update(float deltaTime) {
  assert(particlesPos.size() == particlesVel.size());
  assert(particlesPos.size() == timeToLive.size());
  
	int last = particlesPos.size() - 1;
  for (int i = 0; i <= last;) {
    particlesVel[i] += glm::vec3{0.0f, 0.0f, -30.00f} * deltaTime;
    particlesPos[i] += particlesVel[i] * deltaTime;
    timeToLive[i] -= deltaTime;
    if (timeToLive[i] < 0){
      particlesVel[i] = particlesVel[last];
      particlesPos[i] = particlesPos[last];
      timeToLive[i] = timeToLive[last];
      --last;
      continue;
    }
    ++i;
  }
  particlesVel.resize(last + 1);
  particlesPos.resize(last + 1);
  timeToLive.resize(last + 1);

  memcpy(selfMemoryMapped, particlesPos.data(), particlesPos.size() * sizeof(particlesPos[0]));
}

/**
 * @brief Updates the uniform buffer data for the particle system.
 * 
 * @param eng Reference to the RenderEngine instance.
 */
void Particles::updateUniformBuffers(RenderEngine& eng){
  {
    ParticleUBO newData{};
    newData.color = glm::vec4(1.0, 0.0, 0.0, 1.0);
    newData.size = 20.0f;

    memcpy((char*)uniformBuffersMapped + uniformBuffers[eng.currentFrame].offset, &newData, sizeof(ParticleUBO));
  }
}

/**
 * @brief Creates the descriptor set layout for the particle system.
 * 
 * @param eng Reference to the RenderEngine instance.
 */
void Particles::createDescriptorSetLayout(RenderEngine& eng){
  {
    VkDescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.stageFlags =
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &uboLayoutBinding;

    VkVerify(vkCreateDescriptorSetLayout(eng.device, &layoutInfo, nullptr,
                                         &descriptorSetLayout))
  }
}

/**
 * @brief Creates descriptor sets for the particle system.
 * 
 * @param eng Reference to the RenderEngine instance.
 * @param pool The descriptor pool to use.
 */
void Particles::createDescritptorSets(RenderEngine& eng, VkDescriptorPool pool){
  VkDescriptorSetAllocateInfo allocInfo{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
  allocInfo.descriptorPool = pool;
  allocInfo.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
  std::array<VkDescriptorSetLayout, MAX_FRAMES_IN_FLIGHT> tmp;
  std::fill_n(tmp.data(), MAX_FRAMES_IN_FLIGHT, descriptorSetLayout);
  allocInfo.pSetLayouts = tmp.data();
  VkVerify(vkAllocateDescriptorSets(eng.device, &allocInfo, descriptorSets.data()));

	for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
    {
      VkDescriptorBufferInfo buffInfo{
        .buffer = uniformBuffers[i].buffer,
        .offset = 0,
        .range = uniformBuffers[i].size
      };
      std::array<VkWriteDescriptorSet, 1> descriptorWrites;
      descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      descriptorWrites[0].pNext = nullptr;
      descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
      descriptorWrites[0].pBufferInfo = &buffInfo;
      descriptorWrites[0].descriptorCount = 1;
      descriptorWrites[0].dstArrayElement = 0;
      descriptorWrites[0].dstBinding = 0;
      descriptorWrites[0].dstSet = descriptorSets[i];

      vkUpdateDescriptorSets(eng.device, descriptorWrites.size(),
                             descriptorWrites.data(), 0, nullptr);
    }
  }
}

/**
 * @brief Creates the uniform buffers for the particle system.
 * 
 * @param eng Reference to the RenderEngine instance.
 */
void Particles::createUniformBuffers(RenderEngine& eng){
  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
    uniformBuffers[i].size = sizeof(ParticleUBO);
    VkVerify(eng.createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, uniformBuffers[i]))
  }
  eng.allocateMemory(
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
    uniformBuffers.data(), 
    MAX_FRAMES_IN_FLIGHT, 
    uniformBuffers[0].bufferMemory);

  VkVerify(vkMapMemory(
    eng.device, 
    uniformBuffers[0].bufferMemory, 
    0,
		sizeof(ParticleUBO) * MAX_FRAMES_IN_FLIGHT,
    0, 
    &uniformBuffersMapped));
}

/**
 * @brief Gets the maximum number of particles this system supports.
 * 
 * @return uint32_t The maximum particle capacity.
 */
uint32_t Particles::getMaxPaticleCount() const{
  return maxParticlecCount;
}
	
/**
 * @brief Gets the buffer structure containing particle data.
 * 
 * @return ShaderBuffer The buffer definition for particle data.
 */
ShaderBuffer Particles::getParticleBuffer() const{
  ShaderBuffer out = selfMemory;
  out.size = getMaxPaticleCount()*sizeof(glm::vec3);
  return out;
}

/**
 * @brief Frees all allocated resources for the particle system.
 * 
 * @param eng Reference to the RenderEngine instance.
 */
void Particles::free(RenderEngine& eng){
  vkUnmapMemory(eng.device, selfMemory.bufferMemory);
  eng.destroyBuffer(selfMemory);
  for (ShaderBuffer& buff : uniformBuffers){eng.destroyBuffer(buff);}
  eng.freeMemory(selfMemory.bufferMemory);
  eng.freeMemory(uniformBuffers[0].bufferMemory);
  vkDestroyDescriptorSetLayout(eng.device, descriptorSetLayout, nullptr);
}
