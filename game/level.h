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
  /**
 * @brief Virtual destructor for the Level base class.
 */
virtual ~Level();
  /**
 * @brief Updates the level state.
 * 
 * @param data Input data from the user (keys, mouse, etc.).
 * @param deltaTime Time elapsed since the last frame in seconds.
 */
virtual void update(const InputData &data, float deltaTime) = 0;
  /**
 * @brief Renders the level content.
 * 
 * @param commandBuffer The Vulkan command buffer to record rendering commands into.
 */
virtual void render(VkCommandBuffer) = 0;

protected:
  Player player;
};

class Level0 : public Level {
public:
  /**
 * @brief Constructor for Level0.
 * 
 * @param eng Pointer to the RenderEngine instance.
 * @param game Pointer to the Game instance.
 */
Level0(RenderEngine &eng, Game &game);
  /**
 * @brief Destructor for Level0.
 */
~Level0() override;

  /**
 * @brief Implementation of the level update logic.
 * 
 * @param data Input data from the user (keys, mouse, etc.).
 * @param deltaTime Time elapsed since the last frame in seconds.
 */
virtual void update(const InputData &data, float deltaTime) override;
  /**
 * @brief Implementation of the level rendering logic.
 * 
 * @param commandBuffer The Vulkan command buffer to record rendering commands into.
 */
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

  /**
 * @brief Loads all necessary assets (models, textures, etc.) for the level.
 */
void loadModelData();
  /**
 * @brief Creates and binds all necessary descriptor set layouts.
 */
void createDescriptorSetLayouts();
  /**
 * @brief Initializes the descriptor pool for Vulkan.
 */
void createDescriptorPool();
  /**
 * @brief Creates descriptor sets used across different pipeline stages.
 */
void createDescriptorSets();
  /**
 * @brief Creates uniform buffers for storing per-frame data.
 */
void createUniformBuffers();
  /**
 * @brief Creates Shader Storage Buffer Objects (SSBOs) for large data sets.
 */
void createSSBOs();

  /**
 * @brief Updates the state of all projectiles.
 * 
 * @param deltaTime Time elapsed since the last frame in seconds.
 */
void updateBullets(float deltaTime);
  /**
 * @brief Updates the state of all enemies.
 * 
 * @param deltaTime Time elapsed since the last frame in seconds.
 */
void updateEnemies(float deltaTime);
  /**
 * @brief Updates the player state based on user input.
 * 
 * @param data Input data from the user (keys, mouse, etc.).
 * @param deltaTime Time elapsed since the last frame in seconds.
 */
void updatePlayer(const InputData &data, float deltaTime);
  /**
 * @brief Updates the uniform buffer data before rendering.
 */
void updateUniformBuffer();
};
