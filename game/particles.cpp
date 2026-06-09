#include "BufferStructs.h"
#include "particles.h"
#include <memory.h>


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

void Particles::render(VkCommandBuffer commandBuffer, const Material& mat, RenderEngine& eng){
  updateUniformBuffers(eng);
  
  VkDeviceSize ZERO = 0;
  vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mat.getPipelineLayout(), 1, 1, &descriptorSets[eng.currentFrame], 0, nullptr);
  vkCmdBindVertexBuffers(commandBuffer, 0, 1, &selfMemory.buffer, &ZERO);
  vkCmdDraw(commandBuffer, particlesPos.size(), 1, 0, 0);  
}

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

void Particles::updateUniformBuffers(RenderEngine& eng){
  {
    ParticleUBO newData{};
    newData.color = glm::vec4(1.0, 0.0, 0.0, 1.0);
    newData.size = 20.0f;

    memcpy((char*)uniformBuffersMapped + uniformBuffers[eng.currentFrame].offset, &newData, sizeof(ParticleUBO));
  }
}

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

uint32_t Particles::getMaxPaticleCount() const{
  return maxParticlecCount;
}
	
ShaderBuffer Particles::getParticleBuffer() const{
  ShaderBuffer out = selfMemory;
  out.size = getMaxPaticleCount()*sizeof(glm::vec3);
  return out;
}

void Particles::free(RenderEngine& eng){
  vkUnmapMemory(eng.device, selfMemory.bufferMemory);
  eng.destroyBuffer(selfMemory);
  for (ShaderBuffer& buff : uniformBuffers){eng.destroyBuffer(buff);}
  eng.freeMemory(selfMemory.bufferMemory);
  eng.freeMemory(uniformBuffers[0].bufferMemory);
  vkDestroyDescriptorSetLayout(eng.device, descriptorSetLayout, nullptr);
}
