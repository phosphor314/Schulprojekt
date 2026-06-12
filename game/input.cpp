#include "input.h"

bool InputData::getKeyDown(int key) const{
	return keymap[key];
}

void InputData::setKeyDown(int key, bool state){
	keymap[key] = state;
}
	
bool InputData::getMouseButtonDown() const{
	return leftMouseButtonDown;
}

void InputData::setMouseButtonDown(bool state){
	leftMouseButtonDown = state;
}

glm::vec2 InputData::getMousePos() const{
	return mousePos;
}

void InputData::setMousePos(glm::vec2 newPos){
	mousePos = newPos * MOUSE_SENSITIVITY;
}
