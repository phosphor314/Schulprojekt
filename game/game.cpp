#include "game.h"
#include "../tiny_obj_loader.h"
#include "GLFW/glfw3.h"
#include "constants.h"
#include "renderEngine.h"
#include "vulkan/vulkan_core.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/fwd.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <random>

namespace std {
template <> struct hash<Vertex> {
  size_t operator()(const Vertex &v) const noexcept {
    static_assert(sizeof(Vertex) == 4 * sizeof(size_t) + sizeof(int),
                  "Vertex does not have the right size!");
    const size_t *A = reinterpret_cast<const size_t *>(&v);
    const int *B = reinterpret_cast<const int *>(&v) + 8;
    return A[0] ^ (A[1] << 1) ^ (A[2] << 2) ^ (A[3] << 3) ^ (B[0] << 8);
  }
};
} // namespace std

Game::Game() {
  std::fill_n(keymap.data(), keymap.size(), 0);
  engine.init();
  createUniformBuffers();
  createSSBOs();
  createDescriptorPool();
  createDescriptorSetLayouts();
  system((SHADER_ROOT + "compile").c_str());
  materials = MaterialLoader(engine, globalDescriptorSetLayout);
  createDescriptorSets();
  loadModelData();

  enemies.resize(3);
  enemies[0].position = {0.0f, 0.0f, 0.0f};
  enemies[1].position = {2.0f, 0.0f, 1.0f};
  enemies[2].position = {0.0f, 1.0f, 2.0f};

  glfwSetWindowUserPointer(engine.window, this);

  glfwSetFramebufferSizeCallback(
      engine.window, [](GLFWwindow *win, int nWidth, int nHeight) {
        reinterpret_cast<Game *>(glfwGetWindowUserPointer(win))
            ->engine.framebufferSizeCallback(nWidth, nHeight);
      });

  glfwSetCursorPosCallback(
      engine.window, [](GLFWwindow *win, double _x, double _y) {
        Game *game = reinterpret_cast<Game *>(glfwGetWindowUserPointer(win));
        constexpr float SENS = 0.01f;

        float x = _x * SENS;
        float y = _y * SENS;

        game->player.up = {0, 0, 1.0f};
        game->player.forward = {sin(y) * sin(x), sin(y) * cos(x), cos(y)};
      });

  glfwSetKeyCallback(engine.window, [](GLFWwindow *win, int key, int scancode,
                                       int action, int mods) {
    Game *game = reinterpret_cast<Game *>(glfwGetWindowUserPointer(win));

    if (key < game->keymap.size()) {
      if (action == GLFW_PRESS) {
        game->keymap[key] = 1;
      } else if (action == GLFW_RELEASE) {
        game->keymap[key] = 0;
      }
    }
  });

  glfwSetMouseButtonCallback(
      engine.window, [](GLFWwindow *win, int button, int action, int mods) {
        Game *game = reinterpret_cast<Game *>(glfwGetWindowUserPointer(win));

        if (button == GLFW_MOUSE_BUTTON_LEFT) {
          if (action == GLFW_PRESS) {
            game->leftMouseButtonDown = true;
          } else if (action == GLFW_RELEASE) {
            game->leftMouseButtonDown = false;
          }
        }
      });
}

Game::~Game() {
  vkDeviceWaitIdle(device);

  engine.destroyBuffer(vertexBuffer);
  engine.destroyBuffer(indexBuffer);
  engine.destroyBuffer(bulletVertexBuffer);
  engine.destroyBuffer(enemyStorageBuffer);
  engine.freeMemory(bufferMemory);
  engine.freeMemory(ssboMemory);

  vkUnmapMemory(device, uniformBuffersMemory);
  for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
    vkDestroyBuffer(device, uniformBuffers[i].buffer, nullptr);
  }
  vkFreeMemory(device, uniformBuffersMemory, nullptr);

  vkDestroyDescriptorSetLayout(device, globalDescriptorSetLayout, nullptr);

  vkDestroyDescriptorPool(device, descriptorPool, nullptr);

  engine.cleanup();
}

void Game::update(float deltaTime) {
  updatePlayer(deltaTime);
  updateBullets(deltaTime);
  updateEnemies(deltaTime);

  updateUniformBuffers();

  VkCommandBuffer commandBuffer = engine.beginRendering();

  const Material *mat = &materials.beginMaterialPass(ENEMIES, commandBuffer);

  vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          mat->getPipelineLayout(), 0, 1,
                          &descriptorSets[engine.currentFrame], 0, nullptr);

  VkDeviceSize ZERO = 0;
  vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer.buffer, &ZERO);
  vkCmdBindIndexBuffer(commandBuffer, indexBuffer.buffer, 0,
                       VK_INDEX_TYPE_UINT32);
  vkCmdDrawIndexed(commandBuffer, indexCount, enemies.size(), 0, 0, 0);

  vkCmdNextSubpass(commandBuffer, VK_SUBPASS_CONTENTS_INLINE);

  mat = &materials.beginMaterialPass(BULLETS, commandBuffer);

  vkCmdBindVertexBuffers(commandBuffer, 0, 1, &bulletVertexBuffer.buffer,
                         &ZERO);
  vkCmdDraw(commandBuffer, 3, projectiles.size(), 0, 0);

  engine.endRendering();
}

void Game::updatePlayer(float deltaTime) {
  constexpr float SPEED = 16.0f;
  constexpr float BULLET_SPEED = 64.0f;
  constexpr float BULLET_RANDOMNESS = 8.0f;
  constexpr float GROUND_PLANE = 0.0f;

  if (keymap[GLFW_KEY_W]) {
    player.position += player.forward * SPEED * deltaTime;
  }
  if (keymap[GLFW_KEY_S]) {
    player.position -= player.forward * SPEED * deltaTime;
  }
  if (keymap[GLFW_KEY_D]) {
    player.position +=
        glm::cross(player.forward, player.up) * SPEED * deltaTime;
  }
  if (keymap[GLFW_KEY_A]) {
    player.position -=
        glm::cross(player.forward, player.up) * SPEED * deltaTime;
  }
  player.position.z = GROUND_PLANE;

  if (leftMouseButtonDown) {
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

  engine.cam.fov = glm::half_pi<float>();
  engine.cam.eye = player.position;
  engine.cam.forward = player.forward;
  engine.cam.up = player.up;
}

void Game::updateEnemies(float deltaTime) {
  constexpr float DESIRED_DISTANCE = 3.0f;
  constexpr float DESIRED_HEIGHT = 1.2f;
  constexpr float SPEED = 4.0f;

  std::uniform_real_distribution<float> distRad(20.0f, 50.0f);
  std::uniform_real_distribution<float> distAng(glm::half_pi<float>(),
                                                3 * glm::half_pi<float>());
  glm::vec3 newEnemy =
      glm::rotate(glm::identity<glm::mat4>(), distAng(randomState), player.up) *
      glm::vec4(distRad(randomState) * player.forward, 1.0f);
  Enemy toPush;
  toPush.health = 20.0f;
  toPush.position = newEnemy;
  if (enemies.size() < MAX_ENEMY_COUNT) {
    enemies.push_back(toPush);
  }

  enemies.erase(std::remove_if(enemies.begin(), enemies.end(),
                               [](const Enemy &e) { return e.health < 0.0f; }),
                enemies.end());

  for (Enemy &e : enemies) {
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
  }

  ShaderBuffer stagingBuffer;
  stagingBuffer.size = sizeof(EnemyStorageBufferStruct) * enemies.size();
  void *pData;
  VkVerify(engine.createStagingBuffer(stagingBuffer, &pData))

      std::vector<EnemyStorageBufferStruct>
          enemyData;
  for (const Enemy &e : enemies) {
    EnemyStorageBufferStruct toPush;
    glm::vec3 rotationAxis =
        glm::normalize(glm::cross({0.0f, 0.0f, -1.0f}, e.forward));
    float angle = glm::acos(glm::dot(e.forward, glm::vec3(0.0f, 0.0f, -1.0f)));
    toPush.transformMat =
        glm::rotate(glm::translate(glm::identity<glm::mat4>(), e.position),
                    angle, rotationAxis);

    enemyData.push_back(toPush);
  }

  memcpy(pData, enemyData.data(), stagingBuffer.size);

  VkBufferCopy bufferCopy;
  bufferCopy.srcOffset = 0;
  bufferCopy.dstOffset =
      engine.currentFrame * sizeof(EnemyStorageBufferStruct) * MAX_ENEMY_COUNT;
  bufferCopy.size = stagingBuffer.size;
  VkVerify(engine.copyBuffer(stagingBuffer, enemyStorageBuffer, bufferCopy))

      engine.freeStagingBuffer(stagingBuffer);
}

void Game::updateBullets(float deltaTime) {
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
  VkVerify(engine.createStagingBuffer(stagingBuffer, &pData))

      memcpy(pData, projectiles.data(), stagingBuffer.size);
  VkBufferCopy bufferCopy{};
  bufferCopy.size = stagingBuffer.size;
  VkVerify(engine.copyBuffer(stagingBuffer, bulletVertexBuffer, bufferCopy))

      engine.freeStagingBuffer(stagingBuffer);
}

void Game::loadModelData() {
  constexpr uint32_t MAX_INSTANCES = 10000;

  std::vector<Vertex> eyeballVertices;
  std::vector<uint32_t> eyeballIndices;
  loadModel("floating eyeball.obj", eyeballVertices, eyeballIndices);
  indexCount = eyeballIndices.size();

  vertexBuffer.size = eyeballVertices.size() * sizeof(Vertex);
  VkVerify(engine.createBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                   VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                               vertexBuffer)) indexBuffer.size =
      eyeballIndices.size() * sizeof(eyeballIndices[0]);
  VkVerify(engine.createBuffer(VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                                   VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                               indexBuffer))

      bulletVertexBuffer.size = sizeof(Projectile) * MAX_INSTANCES;
  VkVerify(engine.createBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                   VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                               bulletVertexBuffer))

      std::array<ShaderBuffer, 3>
          buffers = {vertexBuffer, indexBuffer, bulletVertexBuffer};
  VkVerify(engine.allocateMemory(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                 buffers.data(), buffers.size(), bufferMemory))

      ShaderBuffer stagingBuffer;
  stagingBuffer.size =
      std::max_element(buffers.begin(), buffers.end(),
                       [](const ShaderBuffer &a, const ShaderBuffer &b) {
                         return a.size < b.size;
                       })
          ->size;
  void *pData;
  VkVerify(engine.createStagingBuffer(stagingBuffer, &pData))

      memcpy(pData, eyeballVertices.data(), vertexBuffer.size);
  VkBufferCopy copyRegion{};
  copyRegion.srcOffset = 0;
  copyRegion.dstOffset = 0;
  copyRegion.size = vertexBuffer.size;
  VkVerify(engine.copyBuffer(stagingBuffer, vertexBuffer, copyRegion))

      memcpy(pData, eyeballIndices.data(), indexBuffer.size);
  copyRegion.size = indexBuffer.size;
  VkVerify(engine.copyBuffer(stagingBuffer, indexBuffer, copyRegion))

      engine.freeStagingBuffer(stagingBuffer);
}

void Game::createDescriptorSetLayouts() {
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

    VkVerify(vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr,
                                         &globalDescriptorSetLayout))
  }
}

void Game::createUniformBuffers() {
  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
    uniformBuffers[i].size = sizeof(UniformBufferObject);
    VkVerify(engine.createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                 uniformBuffers[i]))
  }

  VkVerify(engine.allocateMemory(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                 uniformBuffers.data(), uniformBuffers.size(),
                                 uniformBuffersMemory))

      VkVerify(vkMapMemory(device, uniformBuffersMemory, 0,
                           sizeof(UniformBufferObject) * uniformBuffers.size(),
                           0, &uniformBuffersMapped))
}

void Game::createSSBOs() {
  enemyStorageBuffer.size =
      MAX_FRAMES_IN_FLIGHT * sizeof(EnemyStorageBufferStruct) * MAX_ENEMY_COUNT;

  VkVerify(engine.createBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                   VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                               enemyStorageBuffer))
      VkVerify(engine.allocateMemory(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                     &enemyStorageBuffer, 1, ssboMemory))
}

void Game::createDescriptorPool() {
  std::array<VkDescriptorPoolSize, 2> poolSizes;
  poolSizes[0].descriptorCount = MAX_FRAMES_IN_FLIGHT;
  poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  poolSizes[1].descriptorCount = MAX_FRAMES_IN_FLIGHT;
  poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;

  VkDescriptorPoolCreateInfo cInfo{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
  cInfo.maxSets = MAX_FRAMES_IN_FLIGHT;
  cInfo.pPoolSizes = poolSizes.data();
  cInfo.poolSizeCount = poolSizes.size();

  VkVerify(vkCreateDescriptorPool(device, &cInfo, nullptr, &descriptorPool))
}

void Game::createDescriptorSets() {
  VkDescriptorSetAllocateInfo allocInfo{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
  allocInfo.descriptorPool = descriptorPool;
  allocInfo.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
  std::array<VkDescriptorSetLayout, MAX_FRAMES_IN_FLIGHT> tmp;
  std::fill_n(tmp.data(), MAX_FRAMES_IN_FLIGHT, globalDescriptorSetLayout);
  allocInfo.pSetLayouts = tmp.data();
  VkVerify(vkAllocateDescriptorSets(device, &allocInfo, descriptorSets.data()))

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

      vkUpdateDescriptorSets(device, descriptorWrites.size(),
                             descriptorWrites.data(), 0, nullptr);
    }
  }
}

void Game::updateUniformBuffers() {
  {
    UniformBufferObject newData{};
    newData.camera =
        engine.cam.getTransformationMat((float)WIDTH / (float)HEIGHT);
    newData.aspectRatio = (float)WIDTH / (float)HEIGHT;
    newData.light_pos = glm::vec4(4.0f, 1.0f, 0.0f, 0.0f);
    newData.view_pos = glm::vec4(engine.cam.eye, 0.0f);

    memcpy((char *)uniformBuffersMapped +
               uniformBuffers[engine.currentFrame].offset,
           &newData, sizeof(UniformBufferObject));
  }
}

void Game::loadModel(const char *modelPath, std::vector<Vertex> &vertices,
                     std::vector<uint32_t> &indices) {
  tinyobj::ObjReader reader;
  reader.ParseFromFile(modelPath);
  tinyobj::attrib_t attrib = reader.GetAttrib();
  std::vector<tinyobj::shape_t> shapes = reader.GetShapes();
  std::unordered_map<Vertex, uint32_t> uniqueVertices;
  uint32_t vertexCount = 0;

  // Loop over shapes
  for (size_t s = 0; s < shapes.size(); s++) {
    // Loop over faces(polygon)
    size_t index_offset = 0;
    for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++) {
      size_t fv = size_t(shapes[s].mesh.num_face_vertices[f]);

      // Loop over vertices in the face.
      for (size_t v = 0; v < fv; v++) {
        // access to vertex
        tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];

        Vertex vert;

        vert.position.x = attrib.vertices[3 * size_t(idx.vertex_index) + 0];
        vert.position.y = attrib.vertices[3 * size_t(idx.vertex_index) + 1];
        vert.position.z = attrib.vertices[3 * size_t(idx.vertex_index) + 2];

        // Check if `normal_index` is zero or positive. negative = no normal
        // data
        if (idx.normal_index >= 0) {
          vert.normal.x = attrib.normals[3 * size_t(idx.normal_index) + 0];
          vert.normal.y = attrib.normals[3 * size_t(idx.normal_index) + 1];
          vert.normal.z = attrib.normals[3 * size_t(idx.normal_index) + 2];
        }

        if (!uniqueVertices.contains(vert)) {
          uniqueVertices[vert] = vertexCount;
          ++vertexCount;
        }
        indices.push_back(uniqueVertices[vert]);
      }
      index_offset += fv;
    }
  }

  vertices.resize(vertexCount);
  for (const auto &kv : uniqueVertices) {
    vertices[kv.second] = kv.first;
  }
}

bool Game::running() const { return engine.running(); }
