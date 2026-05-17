#include "level.h"
#include <memory.h>

Particles::Particles(ShaderBuffer &&mem) { buff = std::move(mem); }

void Particles::update(VkCommandBuffer buffer, float deltaTime,
                       ShaderBuffer &stagingBuffer, void *stagingBufferMem) {
  assert(particlesPos.size() == particlesVel.size());
  vkCmdBindVertexBuffers(buffer, 0, 1, &buff.buffer, &buff.offset);
  vkCmdDraw(buffer, particlesPos.size(), 1, 0, 0);

  for (size_t i = 0; i < particlesPos.size(); ++i) {
    particlesVel[i] += glm::vec3{0.0f, 2.0f, 0.0f} * deltaTime;
    particlesPos[i] += particlesVel[i] * deltaTime;
  }

  memcpy(stagingBufferMem, particlesPos.data(),
         particlesPos.size() * sizeof(particlesPos[0]));

  VkBufferCopy copyRegion{
      .srcOffset = 0, .dstOffset = buff.offset, .size = buff.size};
  vkCmdCopyBuffer(buffer, stagingBuffer.buffer, buff.buffer, 1, &copyRegion);
}
