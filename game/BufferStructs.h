#pragma once
#include <glm/glm.hpp>

struct Player {
  static constexpr float HURTBOX_RADIUS = 0.8f;

  glm::vec3 position;
  glm::vec3 forward;
  glm::vec3 up;

  float health;
};

struct Enemy {
  static constexpr float HURTBOX_RADIUS = 1.2f;

  glm::vec3 position;
  glm::vec3 forward;

  float eyelidAngle;

  float health;
};

struct EnemyStorageBufferStruct {
  glm::mat4 transformMat;
  glm::mat4 lowerEyelidTransform;
  glm::mat4 upperEyelidTransform;
};

struct Projectile {
  glm::vec3 position;
  glm::vec3 forward;
};

struct ParticleUBO {
  glm::vec4 color;
  float size;
};
