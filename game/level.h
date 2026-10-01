#pragma once

#include "BufferStructs.h"
#include "input.h"
#include "material.h"
#include "particles.h"
#include "renderEngine.h"
#include <array>
#include <random>

struct Game;

class Level {
public:
  virtual ~Level();
  virtual void update(const InputData &data, float deltaTime) = 0;
  virtual void render(VkCommandBuffer) = 0;

protected:
  Player player;
};

class Level0 : public Level {
public:
  Level0(RenderEngine &eng, Game &game);
  ~Level0() override;

  virtual void update(const InputData &data, float deltaTime) override;
  virtual void render(VkCommandBuffer) override;

private:
  static constexpr uint32_t MAX_ENEMY_COUNT = 200;
  static constexpr uint32_t MAX_PARTICLE_COUNT = 20000;

  RenderEngine *engine;
  Game *game;

  std::vector<Enemy> enemies;
  std::vector<Projectile> projectiles;

  VkDescriptorPool descriptorPool;
  VkDescriptorSetLayout globalDescriptorSetLayout;
  std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> descriptorSets;

  VkDeviceMemory uniformBuffersMemory;
  std::array<ShaderBuffer, MAX_FRAMES_IN_FLIGHT> uniformBuffers;
  void *uniformBuffersMapped;

  ShaderBuffer enemyStorageBuffer;

  VkDeviceMemory geometryBufferMemory;
  ShaderBuffer vertexBuffer;
  ShaderBuffer indexBuffer;
  uint32_t indexCount;

  ShaderBuffer bulletVertexBuffer;

  std::mt19937 randomState;

  Particles enemyDeathEffect;

  MaterialLoader materialLoader;

  void loadModelData();
  void createDescriptorSetLayouts();
  void createDescriptorPool();
  void createDescriptorSets();
  void createUniformBuffers();
  void createSSBOs();

  void updateBullets(float deltaTime);
  void updateEnemies(float deltaTime);
  void updatePlayer(const InputData &, float deltaTime);
  void updateUniformBuffer();
};
