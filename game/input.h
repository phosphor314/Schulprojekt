#pragma once

#include <array>
#include <glm/glm.hpp>

struct InputData{
public: 
	/**
 * @brief Checks if a specific key is currently being held down.
 * 
 * @param key The virtual key code to check.
 * @return true If the key is pressed.
 * @return false Otherwise.
 */
bool getKeyDown(int key) const;
	/**
 * @brief Sets the state of a specific key (pressed or released).
 * 
 * @param key The virtual key code.
 * @param state The new state (true for down, false for up).
 */
void setKeyDown(int key, bool state);
	
	/**
 * @brief Checks if the left mouse button was pressed during the current frame.
 * 
 * @return true If the button was newly pressed.
 * @return false Otherwise.
 */
bool getMouseButtonDown() const;
	/**
 * @brief Sets the state of the left mouse button.
 * 
 * @param state The new state (true for down, false for up).
 */
void setMouseButtonDown(bool state);
	
	/**
 * @brief Retrieves the current position of the mouse cursor.
 * 
 * @return glm::vec2 Current screen coordinates.
 */
glm::vec2 getMousePos() const;
	/**
 * @brief Updates the stored position of the mouse cursor.
 * 
 * @param newPos The new coordinates.
 */
void setMousePos(glm::vec2 newPos);
	
private:
	static constexpr float MOUSE_SENSITIVITY = 0.01f;

	std::array<char, 512> keymap;
	glm::vec2 mousePos;
  bool leftMouseButtonDown = false;
};
