#include "level.h"
#include "game.h"
#include <algorithm>
#include <cstring>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <iostream>
#include <memory.h>

Level::~Level() {}

Level0::Level0(RenderEngine &eng, Game &game) {
  this->game = &game;
  engine = &eng;
  loadModelData();
  createDescriptorSetLayouts();
  createDescriptorPool();
  materialLoader = MaterialLoader(eng, globalDescriptorSetLayout);
  createUniformBuffers();
  createSSBOs();
  createDescriptorSets();
  enemyDeathEffect = Particles(MAX_PARTICLE_COUNT, eng, descriptorPool);
}

Level0::~Level0() {
  VkDevice &device = engine->device;

  vkDestroyDescriptorSetLayout(device, globalDescriptorSetLayout, nullptr);
  vkDestroyDescriptorPool(device, descriptorPool, nullptr);

  for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
    vkDestroyBuffer(device, uniformBuffers[i].buffer, nullptr);
  }
  vkUnmapMemory(device, uniformBuffersMemory);
  engine->freeMemory(uniformBuffersMemory);

  vkDestroyBuffer(device, enemyStorageBuffer.buffer, nullptr);
  vkFreeMemory(device, enemyStorageBuffer.bufferMemory, nullptr);

  vkDestroyBuffer(device, vertexBuffer.buffer, nullptr);
  vkDestroyBuffer(device, indexBuffer.buffer, nullptr);
  vkDestroyBuffer(device, bulletVertexBuffer.buffer, nullptr);
  vkFreeMemory(device, geometryBufferMemory, nullptr);

  enemyDeathEffect.free(*engine);
  materialLoader.free(device);
}

void Level0::loadModelData() {
  constexpr uint32_t MAX_INSTANCES = 10000;

  std::vector<Vertex> eyeballVertices;
  std::vector<uint32_t> eyeballIndices;
  loadModel("floating eyeball.obj", eyeballVertices, eyeballIndices);
  indexCount = eyeballIndices.size();

  vertexBuffer.size = eyeballVertices.size() * sizeof(Vertex);
  VkVerify(engine->createBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                    VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                vertexBuffer));
  indexBuffer.size = eyeballIndices.size() * sizeof(eyeballIndices[0]);
  VkVerify(engine->createBuffer(VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                                    VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                indexBuffer));

  bulletVertexBuffer.size = sizeof(Projectile) * MAX_INSTANCES;
  VkVerify(engine->createBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                    VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                bulletVertexBuffer));

  std::array<ShaderBuffer, 3> buffers = {vertexBuffer, indexBuffer,
                                         bulletVertexBuffer};
  VkVerify(engine->allocateMemory(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                  buffers.data(), buffers.size(),
                                  geometryBufferMemory));

  ShaderBuffer stagingBuffer;
  stagingBuffer.size =
      std::max_element(buffers.begin(), buffers.end(),
                       [](const ShaderBuffer &a, const ShaderBuffer &b) {
                         return a.size < b.size;
                       })
          ->size;
  void *pData;
  VkVerify(engine->createStagingBuffer(stagingBuffer, &pData));

  memcpy(pData, eyeballVertices.data(), vertexBuffer.size);
  VkBufferCopy copyRegion{};
  copyRegion.srcOffset = 0;
  copyRegion.dstOffset = 0;
  copyRegion.size = vertexBuffer.size;
  VkVerify(engine->copyBuffer(stagingBuffer, vertexBuffer, copyRegion))

      memcpy(pData, eyeballIndices.data(), indexBuffer.size);
  copyRegion.size = indexBuffer.size;
  VkVerify(engine->copyBuffer(stagingBuffer, indexBuffer, copyRegion))

      engine->freeStagingBuffer(stagingBuffer);
}

void Level0::createDescriptorSetLayouts() {
  // create the per frame set layout
  {
    VkDescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.stageFlags =
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding ssboLayoutBinding{};
    ssboLayoutBinding.binding = 1;
    ssboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    ssboLayoutBinding.descriptorCount = 1;
    ssboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    std::array<VkDescriptorSetLayoutBinding, 2> bindings = {uboLayoutBinding,
                                                            ssboLayoutBinding};
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    VkVerify(vkCreateDescriptorSetLayout(engine->device, &layoutInfo, nullptr,
                                         &globalDescriptorSetLayout))
  }
}

void Level0::createDescriptorPool() {
  std::array<VkDescriptorPoolSize, 3> poolSizes;
  poolSizes[0].descriptorCount = MAX_FRAMES_IN_FLIGHT;
  poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

  poolSizes[1].descriptorCount = MAX_FRAMES_IN_FLIGHT;
  poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;

  poolSizes[2].descriptorCount = MAX_FRAMES_IN_FLIGHT;
  poolSizes[2].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

  VkDescriptorPoolCreateInfo cInfo{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
  cInfo.maxSets = MAX_FRAMES_IN_FLIGHT * 2;
  cInfo.pPoolSizes = poolSizes.data();
  cInfo.poolSizeCount = poolSizes.size();

  VkVerify(
      vkCreateDescriptorPool(engine->device, &cInfo, nullptr, &descriptorPool))
}

void Level0::createDescriptorSets() {
  VkDescriptorSetAllocateInfo allocInfo{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
  allocInfo.descriptorPool = descriptorPool;
  allocInfo.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
  std::array<VkDescriptorSetLayout, MAX_FRAMES_IN_FLIGHT> tmp;
  std::fill_n(tmp.data(), MAX_FRAMES_IN_FLIGHT, globalDescriptorSetLayout);
  allocInfo.pSetLayouts = tmp.data();
  VkVerify(vkAllocateDescriptorSets(engine->device, &allocInfo,
                                    descriptorSets.data()));

  for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
    {
      std::array<VkWriteDescriptorSet, 2> descriptorWrites;
      descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      descriptorWrites[0].pNext = nullptr;
      descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
      descriptorWrites[0].descriptorCount = 1;
      descriptorWrites[0].dstArrayElement = 0;
      descriptorWrites[0].dstBinding = 0;
      descriptorWrites[0].dstSet = descriptorSets[i];

      VkDescriptorBufferInfo bufferInfo;
      bufferInfo.buffer = uniformBuffers[i].buffer;
      bufferInfo.offset = 0;
      bufferInfo.range = sizeof(UniformBufferObject);
      descriptorWrites[0].pBufferInfo = &bufferInfo;

      descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      descriptorWrites[1].pNext = nullptr;
      descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
      descriptorWrites[1].descriptorCount = 1;
      descriptorWrites[1].dstArrayElement = 0;
      descriptorWrites[1].dstBinding = 1;
      descriptorWrites[1].dstSet = descriptorSets[i];

      VkDescriptorBufferInfo ssbo;
      ssbo.buffer = enemyStorageBuffer.buffer;
      ssbo.offset = i * sizeof(EnemyStorageBufferStruct) * MAX_ENEMY_COUNT;
      ssbo.range = sizeof(EnemyStorageBufferStruct) * MAX_ENEMY_COUNT;
      descriptorWrites[1].pBufferInfo = &ssbo;

      vkUpdateDescriptorSets(engine->device, descriptorWrites.size(),
                             descriptorWrites.data(), 0, nullptr);
    }
  }
}

void Level0::createUniformBuffers() {
  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
    uniformBuffers[i].size = sizeof(UniformBufferObject);
    VkVerify(engine->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                  uniformBuffers[i]))
  }

  VkVerify(engine->allocateMemory(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                  uniformBuffers.data(), uniformBuffers.size(),
                                  uniformBuffersMemory));

  VkVerify(vkMapMemory(engine->device, uniformBuffersMemory, 0,
                       sizeof(UniformBufferObject) * uniformBuffers.size(), 0,
                       &uniformBuffersMapped));
}

void Level0::createSSBOs() {
  enemyStorageBuffer.size =
      MAX_FRAMES_IN_FLIGHT * sizeof(EnemyStorageBufferStruct) * MAX_ENEMY_COUNT;

  VkVerify(engine->createBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                    VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                enemyStorageBuffer));

  VkVerify(engine->allocateMemory(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                  &enemyStorageBuffer, 1,
                                  enemyStorageBuffer.bufferMemory));
}

void Level0::update(const InputData &data, float deltaTime) {
  updatePlayer(data, deltaTime);
  updateEnemies(deltaTime);
  updateBullets(deltaTime);
  enemyDeathEffect.update(deltaTime);
  updateUniformBuffer();
}

void Level0::updateBullets(float deltaTime) {
  constexpr float CUTOFF_DIST = 32.0f;

  const auto nBegin = std::remove_if(
      projectiles.begin(), projectiles.end(), [this](const Projectile &p) {
        return glm::dot(player.position - p.position,
                        player.position - p.position) >
               CUTOFF_DIST * CUTOFF_DIST;
      });
  projectiles.erase(nBegin, projectiles.end());

  for (Projectile &p : projectiles) {
    for (Enemy &e : enemies) {
      glm::vec3 perp = -glm::normalize(glm::cross(
          p.forward, glm::cross(p.forward, p.position - e.position)));
      if (perp == glm::vec3(0.0f, 0.0f, 0.0f)) {
        continue;
      }
      float distSq = glm::dot(perp, p.position - e.position);
      if (distSq < e.HURTBOX_RADIUS * e.HURTBOX_RADIUS) {
        p.forward = glm::vec3(0.0f, 0.0f, 0.0f);
        e.health -= 1.0f;
      }
    }
  }

  projectiles.erase(std::remove_if(projectiles.begin(), projectiles.end(),
                                   [](const Projectile &p) {
                                     return p.forward ==
                                            glm::vec3(0.0f, 0.0f, 0.0f);
                                   }),
                    projectiles.end());

  if (projectiles.size() == 0) {
    return;
  }

  for (Projectile &p : projectiles) {
    p.position += p.forward * deltaTime;
  }

  ShaderBuffer stagingBuffer;
  void *pData;
  stagingBuffer.size = sizeof(Projectile) * projectiles.size();
  VkVerify(engine->createStagingBuffer(stagingBuffer, &pData));

  memcpy(pData, projectiles.data(), stagingBuffer.size);
  VkBufferCopy bufferCopy{};
  bufferCopy.srcOffset = 0;
  bufferCopy.dstOffset = 0;
  bufferCopy.size = stagingBuffer.size;
  VkVerify(engine->copyBuffer(stagingBuffer, bulletVertexBuffer, bufferCopy));

  engine->freeStagingBuffer(stagingBuffer);
}

void Level0::updateEnemies(float deltaTime) {
  constexpr float DESIRED_DISTANCE = 3.0f;
  constexpr float DESIRED_HEIGHT = 1.2f;
  constexpr float SPEED = 8.0f;

  std::uniform_real_distribution<float> distRad(50.0f, 100.0f);
  std::uniform_real_distribution<float> distAng(glm::half_pi<float>(),
                                                3 * glm::half_pi<float>());
  glm::vec3 newEnemy =
      glm::vec3(glm::rotate(glm::identity<glm::mat4>(), distAng(randomState),
                            player.up) *
                glm::vec4(distRad(randomState) * player.forward, 1.0f)) +
      player.position;
  Enemy toPush;
  toPush.health = 10.0f;
  toPush.position = newEnemy;
  if (enemies.size() < MAX_ENEMY_COUNT) {
    enemies.push_back(toPush);
  }

  int last = enemies.size() - 1;
  for (int i = 0; i <= last;) {
    Enemy &e = enemies[i];

    if (e.health <= 0.0f) {
      for (int i = 0; i < 1000; ++i) {
        std::uniform_real_distribution velDist(-20.0f, 20.0f);
        std::uniform_real_distribution posDist(-Enemy::HURTBOX_RADIUS,
                                               Enemy::HURTBOX_RADIUS);
        if (enemyDeathEffect.particlesPos.size() <
            enemyDeathEffect.getMaxPaticleCount()) {
          enemyDeathEffect.particlesPos.emplace_back(
              e.position + glm::vec3{posDist(randomState), posDist(randomState),
                                     posDist(randomState)});
          enemyDeathEffect.particlesVel.emplace_back(
              velDist(randomState), velDist(randomState), velDist(randomState));
          enemyDeathEffect.timeToLive.emplace_back(2.0f);
        }
      }
      enemies[i] = enemies[last];
      --last;
      continue;
    }

    e.forward = glm::normalize(player.position - e.position);
    e.position += SPEED * e.forward * deltaTime;

    // HORRENDOUS
    for (Enemy &other : enemies) {
      if (other.position == e.position) {
        continue;
      }
      float distSq =
          glm::dot(other.position - e.position, other.position - e.position);
      if (distSq < Enemy::HURTBOX_RADIUS * Enemy::HURTBOX_RADIUS) {
        float dist = std::sqrtf(distSq);
        glm::vec3 norm = (other.position - e.position) / dist;
        other.position += (Enemy::HURTBOX_RADIUS - dist) * norm;
        e.position -= (Enemy::HURTBOX_RADIUS - dist) * norm;
      }
    }

    ++i;
  }
  enemies.resize(last + 1);

  ShaderBuffer stagingBuffer;
  stagingBuffer.size = sizeof(EnemyStorageBufferStruct) * enemies.size();
  void *pData;
  VkVerify(engine->createStagingBuffer(stagingBuffer, &pData));

  std::vector<EnemyStorageBufferStruct> enemyData;
  for (const Enemy &e : enemies) {
    EnemyStorageBufferStruct toPush;
    glm::vec3 rotationAxis =
        glm::normalize(glm::cross({0.0f, 0.0f, -1.0f}, e.forward));
    float angle = glm::acos(glm::dot(e.forward, glm::vec3(0.0f, 0.0f, -1.0f)));
    toPush.transformMat =
        glm::rotate(glm::translate(glm::identity<glm::mat4>(), e.position),
                    angle, -rotationAxis);

    enemyData.push_back(toPush);
  }

  memcpy(pData, enemyData.data(), stagingBuffer.size);

  VkBufferCopy bufferCopy;
  bufferCopy.srcOffset = 0;
  bufferCopy.dstOffset =
      engine->currentFrame * sizeof(EnemyStorageBufferStruct) * MAX_ENEMY_COUNT;
  bufferCopy.size = stagingBuffer.size;
  VkVerify(engine->copyBuffer(stagingBuffer, enemyStorageBuffer, bufferCopy));

  engine->freeStagingBuffer(stagingBuffer);
}

void Level0::updatePlayer(const InputData &inputs, float deltaTime) {
  constexpr float SPEED = 64.0f;
  constexpr float BULLET_SPEED = 200.0f;
  constexpr float BULLET_RANDOMNESS = 8.0f;
  constexpr float GROUND_PLANE = 0.0f;

  player.up = {0, 0, 1.0f};
  player.forward = {sin(inputs.getMousePos().y) * sin(inputs.getMousePos().x),
                    sin(inputs.getMousePos().y) * cos(inputs.getMousePos().x),
                    cos(inputs.getMousePos().y)};

  glm::vec3 forward = glm::vec3(player.forward.x, player.forward.y, 0);
  if (forward.length() != 0) {
    forward /= forward.length();
  }

  if (inputs.getKeyDown(GLFW_KEY_W)) {
    player.position += forward * SPEED * deltaTime;
  }
  if (inputs.getKeyDown(GLFW_KEY_S)) {
    player.position -= forward * SPEED * deltaTime;
  }
  if (inputs.getKeyDown(GLFW_KEY_D)) {
    player.position += glm::cross(forward, player.up) * SPEED * deltaTime;
  }
  if (inputs.getKeyDown(GLFW_KEY_A)) {
    player.position -= glm::cross(forward, player.up) * SPEED * deltaTime;
  }
  player.position.z = GROUND_PLANE;

  if (inputs.getMouseButtonDown()) {
    std::uniform_real_distribution<float> dist(-BULLET_RANDOMNESS,
                                               BULLET_RANDOMNESS);

    Projectile toPush;
    toPush.forward =
        player.forward * BULLET_SPEED +
        glm::vec3(dist(randomState), dist(randomState), dist(randomState));
    toPush.position =
        player.position + player.forward * (player.HURTBOX_RADIUS + 0.2f);
    projectiles.push_back(toPush);
  }

  for (const Enemy &e : enemies) {
    float sqDist = glm::dot((e.position - player.position),
                            (e.position - player.position));
    if (sqDist < player.HURTBOX_RADIUS * player.HURTBOX_RADIUS) {
      glfwSetWindowShouldClose(engine->window, GLFW_TRUE);
    }
  }

  engine->cam.fov = glm::half_pi<float>();
  engine->cam.eye = player.position;
  engine->cam.forward = player.forward;
  engine->cam.up = player.up;
}

void Level0::updateUniformBuffer() {
  UniformBufferObject ubo;
  ubo.aspectRatio = (float)WIDTH / HEIGHT;
  ubo.camera =
      glm::perspective(FOV, ubo.aspectRatio, 0.1f, 200.0f) *
      glm::lookAt(player.position, player.position + player.forward, player.up);
  ubo.light_pos = {0.0f, 0.0f, 0.0f, 0.0f};
  ubo.view_pos = {player.position, 0.0f};

  memcpy((char *)uniformBuffersMapped +
             uniformBuffers[engine->currentFrame].offset,
         &ubo, sizeof(UniformBufferObject));
}

void Level0::render(VkCommandBuffer commandBuffer) {
  Material mat = materialLoader.beginMaterialPass(ENEMIES, commandBuffer);
  VkDeviceSize ZERO = 0;

  vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer.buffer, &ZERO);
  vkCmdBindIndexBuffer(commandBuffer, indexBuffer.buffer, 0,
                       VK_INDEX_TYPE_UINT32);
  vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          mat.getPipelineLayout(), 0, 1,
                          &descriptorSets[engine->currentFrame], 0, nullptr);
  vkCmdDrawIndexed(commandBuffer, indexCount, enemies.size(), 0, 0, 0);

  mat = materialLoader.beginMaterialPass(PARTICLES, commandBuffer);
  enemyDeathEffect.render(commandBuffer, mat, *engine);
  vkCmdNextSubpass(commandBuffer, VK_SUBPASS_CONTENTS_INLINE);

  mat = materialLoader.beginMaterialPass(BULLETS, commandBuffer);

  vkCmdBindVertexBuffers(commandBuffer, 0, 1, &bulletVertexBuffer.buffer,
                         &ZERO);
  vkCmdDraw(commandBuffer, 3, projectiles.size(), 0, 0);
}
