#include "input.h"

/**
 * @brief Checks if a specific key is currently being held down.
 * 
 * @param key The virtual key code to check.
 * @return true If the key is pressed.
 * @return false Otherwise.
 */
bool InputData::getKeyDown(int key) const{
	return keymap[key];
}

/**
 * @brief Sets the state of a specific key (pressed or released).
 * 
 * @param key The virtual key code.
 * @param state The new state (true for down, false for up).
 */
void InputData::setKeyDown(int key, bool state){
	keymap[key] = state;
}
	
/**
 * @brief Checks if the left mouse button was pressed during the current frame.
 * 
 * @return true If the button was newly pressed.
 * @return false Otherwise.
 */
bool InputData::getMouseButtonDown() const{
	return leftMouseButtonDown;
}

/**
 * @brief Sets the state of the left mouse button.
 * 
 * @param state The new state (true for down, false for up).
 */
void InputData::setMouseButtonDown(bool state){
	leftMouseButtonDown = state;
}

/**
 * @brief Retrieves the current position of the mouse cursor.
 * 
 * @return glm::vec2 Current screen coordinates.
 */
glm::vec2 InputData::getMousePos() const{
	return mousePos;
}

/**
 * @brief Updates the stored position of the mouse cursor.
 * 
 * @param newPos The new coordinates.
 */
void InputData::setMousePos(glm::vec2 newPos){
	mousePos = newPos * MOUSE_SENSITIVITY;
}
