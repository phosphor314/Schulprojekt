#pragma once

#include "renderEngine.h"
#include "input.h"
#include "level.h"
#include <memory>
#include <vector>


/**
 * @brief Loads a 3D model from the specified path.
 * 
 * @param modelPath Path to the model file.
 * @param vertices Output vector to store model vertices.
 * @param indices Output vector to store model indices.
 */
void loadModel(const char *modelPath, std::vector<Vertex> &vertices, std::vector<uint32_t> &indices);

struct Game {
public:
  /**
 * @brief Constructs a new Game object.
 */
Game();
  ~/**
 * @brief Constructs a new Game object.
 */
Game();

  // does both game update and rendering
  /**
 * @brief Updates the state of the game.
 * 
 * @param deltaTime Time elapsed since the last frame in seconds.
 */
void update(float deltaTime);

  /**
 * @brief Checks if the game is currently running.
 * 
 * @return true If the game is still active.
 * @return false If the game has stopped.
 */
bool running() const;

private:
  InputData inputs;
  RenderEngine engine;
  std::unique_ptr<Level> currentLevel;
  
  VkDevice& device = engine.device;

	/**
 * @brief Renders the current state of the game.
 */
void render();

  /**
 * @brief Compiles necessary shaders for the rendering pipeline.
 */
void compileShaders();
  /**
 * @brief Sets up the game window and graphics context.
 */
void initializeWindow();
  /**
 * @brief Sets up input handling mechanisms.
 */
void initializeUserInput();
};
