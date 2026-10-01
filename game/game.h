#pragma once

#include "renderEngine.h"
#include "input.h"
#include "level.h"
#include <memory>
#include <vector>


void loadModel(const char *modelPath, std::vector<Vertex> &vertices, std::vector<uint32_t> &indices);

struct Game {
public:
  Game();
  ~Game();

  // does both game update and rendering
  void update(float deltaTime);

  bool running() const;

private:
  InputData inputs;
  RenderEngine engine;
  std::unique_ptr<Level> currentLevel;
  
  VkDevice& device = engine.device;

	void render();

  void compileShaders();
  void initializeWindow();
  void initializeUserInput();
};
