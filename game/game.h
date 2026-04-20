#pragma once

#include "renderEngine.h"
#include <glm/ext/vector_float3.hpp>
#include <vector>
#include <array>
#include <random>


struct Player{
    static constexpr float HURTBOX_RADIUS = 0.8f;
    
    glm::vec3 position;
    glm::vec3 forward;
    glm::vec3 up;
    
    float health;
};

struct Enemy{
    static constexpr float HURTBOX_RADIUS = 1.2f;
    
    glm::vec3 position;
    glm::vec3 forward;
    
    float eyelidAngle;
    
    float health;
};

struct EnemyStorageBufferStruct{
	glm::mat4 transformMat;
	glm::mat4 lowerEyelidTransform;
	glm::mat4 upperEyelidTransform;
};

// hitscan
struct Projectile{
	glm::vec3 position;
	glm::vec3 forward;
};

struct Game{
public:
    Game();
    ~Game();
    
    // does both game update and rendering
    void update(float deltaTime);
    
    bool running() const;

private:
	static constexpr std::string SHADER_ROOT = "shaders/";
	static constexpr uint32_t MAX_ENEMY_COUNT = 200;

    std::array<char, 512> keymap;
    bool leftMouseButtonDown = false;
    RenderEngine engine;
    Player player;
    std::vector<Enemy> enemies;
    std::vector<Projectile> projectiles;
    
	VkPipelineLayout pipelineLayout;
	VkPipeline graphicsPipeline;
	
	VkPipeline bulletPipeline;

	VkDescriptorSetLayout globalDescriptorSetLayout;
	
	VkDescriptorPool descriptorPool;

	std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> descriptorSets;

	VkDeviceMemory uniformBuffersMemory;
	std::array<ShaderBuffer, MAX_FRAMES_IN_FLIGHT> uniformBuffers;
	void* uniformBuffersMapped;
    
    VkDeviceMemory ssboMemory;
    ShaderBuffer enemyStorageBuffer;
    
    VkDeviceMemory bufferMemory;
    
    ShaderBuffer vertexBuffer;
    ShaderBuffer indexBuffer;
    uint32_t indexCount;
    
    ShaderBuffer bulletVertexBuffer;
    
    std::mt19937 randomState;
    
    VkDevice& device = engine.device;
    
    void updatePlayer(float deltaTime);
    void updateEnemies(float deltaTime);
    void updateBullets(float deltaTime);
    
    void createGraphicsPipeline();
    void loadModelData();
    void createDescriptorSetLayouts();
    void createUniformBuffers();
    void createSSBOs();
    void createDescriptorPool();
    void createDescriptorSets();
    
    void updateUniformBuffers();
    
    void loadModel(const char* modelPath, std::vector<Vertex>& vertices, std::vector<uint32_t>& indices);
};
