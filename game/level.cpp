#include "level.h"

void CompositeObject::render(MaterialType currentType, VkCommandBuffer commandBuffer){
	vkCmdBindVertexBuffers(commandBuffer, 0, 1, &geometry[(int)currentType].buffer, &geometry[(int)currentType].offset);
	vkCmdDraw(commandBuffer, vertexCounts[(int)currentType], instanceCounts[(int)currentType], 0, 0);
}

void CompositeObject::setGeometry(MaterialType which, ShaderBuffer geometry, uint32_t vertexCount, uint32_t instanceCount){
	this->geometry[(int)which] = geometry;
	this->vertexCounts[(int)which] = vertexCount;
	this->instanceCounts[(int)which] = instanceCount;
}

CompositeObject Level0::getStaticGeometry(){
	return CompositeObject();
}

Player Level0::initializePlayer(){
	Player playerState;
	playerState.position = glm::vec3(0.0f, 0.0f, 0.0f);
	playerState.health = 20.0f;
	
	return playerState;
}
