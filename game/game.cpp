#include "game.h"
#include "../tiny_obj_loader.h"
#include "GLFW/glfw3.h"
#include "constants.h"
#include "renderEngine.h"
#include "vulkan/vulkan_core.h"
#include <cstring>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/fwd.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>

namespace std {
template <> struct hash<Vertex> {
  size_t operator()(const Vertex &v) const noexcept {
    static_assert(sizeof(Vertex) == 4 * sizeof(size_t) + sizeof(int), "Vertex does not have the right size!");
    const size_t *A = reinterpret_cast<const size_t *>(&v);
    const int *B = reinterpret_cast<const int *>(&v) + 8;
    return A[0] ^ (A[1] << 1) ^ (A[2] << 2) ^ (A[3] << 3) ^ (B[0] << 8);
  }
};
} // namespace std

/**
 * @brief Constructs a new Game object.
 */
Game::Game() {
  engine.init();
  compileShaders();
  initializeWindow();
  initializeUserInput();
  currentLevel = std::make_unique<Level0>(engine, *this);
}

/**
 * @brief Compiles necessary shaders for the rendering pipeline.
 */
void Game::compileShaders(){
	system((SHADER_ROOT + "compile").c_str());
}

/**
 * @brief Sets up the game window and graphics context.
 */
void Game::initializeWindow(){
  glfwSetWindowUserPointer(engine.window, this);

  glfwSetFramebufferSizeCallback(
      engine.window, [](GLFWwindow *win, int nWidth, int nHeight) {
        reinterpret_cast<Game *>(glfwGetWindowUserPointer(win))
            ->engine.framebufferSizeCallback(nWidth, nHeight);
      });
}

/**
 * @brief Sets up input handling mechanisms.
 */
void Game::initializeUserInput(){
  glfwSetCursorPosCallback(
      engine.window, [](GLFWwindow *win, double x, double y) {
        Game *game = reinterpret_cast<Game *>(glfwGetWindowUserPointer(win));
        game->inputs.setMousePos(glm::vec2(x, y));
  });

  glfwSetKeyCallback(engine.window, [](GLFWwindow *win, int key, int scancode,
                                       int action, int mods) {
    Game *game = reinterpret_cast<Game *>(glfwGetWindowUserPointer(win));

    if (action == GLFW_PRESS){
      game->inputs.setKeyDown(key, true);
    }
    else if (action == GLFW_RELEASE){
      game->inputs.setKeyDown(key, false);
    }
  });

  glfwSetMouseButtonCallback(
      engine.window, [](GLFWwindow *win, int button, int action, int mods) {
        Game *game = reinterpret_cast<Game *>(glfwGetWindowUserPointer(win));

        if (button == GLFW_MOUSE_BUTTON_LEFT) {
          if (action == GLFW_PRESS) {
            game->inputs.setMouseButtonDown(true);
          } else if (action == GLFW_RELEASE) {
            game->inputs.setMouseButtonDown(false);
          }
        }
  });
}

/**
 * @brief Destructor for the Game class.
 */
Game::~Game() {
  vkDeviceWaitIdle(device);
  currentLevel.reset();
  engine.cleanup();
}

/**
 * @brief Updates the state of the game.
 * 
 * @param deltaTime Time elapsed since the last frame in seconds.
 */
void Game::update(float deltaTime) {
  currentLevel->update(inputs, deltaTime);
  render();
}

/**
 * @brief Renders the current state of the game.
 */
void Game::render(){
  VkCommandBuffer commandBuffer = engine.beginRendering();
  
  currentLevel->render(commandBuffer);
  
  engine.endRendering();
}

/**
 * @brief Loads a 3D model from the specified path.
 * 
 * @param modelPath Path to the model file.
 * @param vertices Output vector to store model vertices.
 * @param indices Output vector to store model indices.
 */
void loadModel(const char *modelPath, std::vector<Vertex> &vertices, std::vector<uint32_t> &indices) {
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
