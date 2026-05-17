#pragma once

#include "renderEngine.h"
#include <vector>

enum MaterialType {
  ENEMIES,
  PARTICLES,
  BULLETS,
  MAX_MAT // used to iterate, not an actual material
};

struct Material {
  Material(MaterialType type, RenderEngine &engine,
           VkDescriptorSetLayout perFrameSet);

  MaterialType getType() const;
  VkPipelineLayout getPipelineLayout() const;

  void beginMaterialPass(VkCommandBuffer commandBuffer) const;

  void free(VkDevice);

private:
  MaterialType material;
  VkPipeline pipeline;
  VkPipelineLayout pipelineLayout;
  std::vector<VkDescriptorSetLayout> selfLayouts;

  void createEnemiesMaterial(RenderEngine &, VkDescriptorSetLayout);
  void createEnemiesSetLayouts(RenderEngine &);

  void createParticlesMaterial(RenderEngine &, VkDescriptorSetLayout);
  void createParticleSetLayouts(RenderEngine &);

  void createBulletsMaterial(RenderEngine &, VkDescriptorSetLayout);
  void createBulletsSetLayouts(RenderEngine &);
};

struct MaterialLoader {
  MaterialLoader() = default;
  MaterialLoader(RenderEngine &, VkDescriptorSetLayout perFrameSet);

  const Material &beginMaterialPass(MaterialType type, VkCommandBuffer);

  void free(VkDevice dev);

private:
  std::vector<Material> materials;
};
