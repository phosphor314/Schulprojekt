#pragma once

#include <array>
#include <glm/glm.hpp>

struct InputData{
public: 
	bool getKeyDown(int key) const;
	void setKeyDown(int key, bool state);
	
	bool getMouseButtonDown() const;
	void setMouseButtonDown(bool state);
	
	glm::vec2 getMousePos() const;
	void setMousePos(glm::vec2 newPos);
	
private:
	static constexpr float MOUSE_SENSITIVITY = 0.01f;

	std::array<char, 512> keymap;
	glm::vec2 mousePos;
  bool leftMouseButtonDown = false;
};
