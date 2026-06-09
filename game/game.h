#pragma once

#include "BufferStructs.h"
#include "material.h"
#include "renderEngine.h"
#include "particles.h"
#include "constants.h"
#include <array>
#include <glm/ext/vector_float3.hpp>
#include <random>
#include <vector>

struct Game {
public:
  Game();
  ~Game();

  // does both game update and rendering
  void update(float deltaTime);

  bool running() const;

private:
  static constexpr uint32_t MAX_ENEMY_COUNT = 200;
  static constexpr uint32_t MAX_PARTICLE_COUNT = 20000;

  std::array<char, 512> keymap;
  bool leftMouseButtonDown = false;
  RenderEngine engine;
  Player player;
  std::vector<Enemy> enemies;
  std::vector<Projectile> projectiles;

  VkDescriptorSetLayout globalDescriptorSetLayout;

  VkDescriptorPool descriptorPool;

  std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> descriptorSets;

  VkDeviceMemory uniformBuffersMemory;
  std::array<ShaderBuffer, MAX_FRAMES_IN_FLIGHT> uniformBuffers;
  void *uniformBuffersMapped;

  VkDeviceMemory ssboMemory;
  ShaderBuffer enemyStorageBuffer;

  VkDeviceMemory bufferMemory;

  ShaderBuffer vertexBuffer;
  ShaderBuffer indexBuffer;
  uint32_t indexCount;

  ShaderBuffer bulletVertexBuffer;

  std::mt19937 randomState;

  MaterialLoader materials;
  
  Particles enemyDeathEffect;

  VkDevice &device = engine.device;

  void updatePlayer(float deltaTime);
  void updateEnemies(float deltaTime);
  void updateBullets(float deltaTime);

	void render();

  void loadModelData();
  void createDescriptorSetLayouts();
  void createUniformBuffers();
  void createSSBOs();
  void createDescriptorPool();
  void createDescriptorSets();
  void compileShaders();
  void createMaterialLoader();
  void createParticleSystems();
  void initializeWindow();
  void initializeUserInput();

  void updateUniformBuffers();

  void loadModel(const char *modelPath, std::vector<Vertex> &vertices,
                 std::vector<uint32_t> &indices);
};
