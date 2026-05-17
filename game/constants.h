#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>

static constexpr std::string SHADER_ROOT = "shaders/";
constexpr int WIDTH = 800;
constexpr int HEIGHT = 600;
constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 3;

#define VkVerify(expr)                                                         \
  {                                                                            \
    VkResult tmp = expr;                                                       \
    if (tmp != VK_SUCCESS) {                                                   \
      throw std::runtime_error(string_VkResult(tmp));                          \
    }                                                                          \
  }
