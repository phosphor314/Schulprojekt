#pragma once

#include "renderEngine.h"
#include "material.h"
#include "BufferStructs.h"
#include "input.h"
#include <array>


struct CompositeObject{
public:
	CompositeObject() = default;
	
	void render(MaterialType currentType, VkCommandBuffer commandBuffer);

	void setGeometry(MaterialType which, ShaderBuffer geometry, uint32_t vertexCount, uint32_t instanceCount);
	
private:
	std::array<ShaderBuffer, MAX_MAT> geometry;
	std::array<uint32_t, MAX_MAT> vertexCounts;
	std::array<uint32_t, MAX_MAT> instanceCounts;
};

class ILevel{
	virtual CompositeObject getStaticGeometry() = 0;
	virtual Player initializePlayer() = 0;
	virtual void update(const InputData& data) = 0;
};

class Level0 : ILevel{
	virtual CompositeObject getStaticGeometry();
	virtual Player initializePlayer();
	virtual void update(const InputData& data);
};
