#pragma once

#include <vk_types.h>
#include <vector>
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>
#include <vector>
#include <glm/gtx/hash.hpp>

struct VertexInputDescription {
	std::vector<VkVertexInputBindingDescription> bindings;
	std::vector<VkVertexInputAttributeDescription> attributes;
	VkPipelineVertexInputStateCreateFlags flags = 0;
};

struct Vertex {
    glm::vec3 position;
	glm::vec3 normal; 
    glm::vec3 color;
	glm::vec2 texCoord; 

    static VertexInputDescription getVertexDescription();
	
	bool operator==(const Vertex& other) const {
        return position == other.position 
		&& normal == other.normal
		&& color == other.color 
		&& texCoord == other.texCoord;
    }
};

template<> struct std::hash<Vertex> {
	size_t operator()(Vertex const& vertex) const {
		return ((std::hash<glm::vec3>()(vertex.position) ^ (std::hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^ (std::hash<glm::vec2>()(vertex.texCoord) << 1);
	}
};

struct Mesh {
	std::vector<Vertex> _vertices;
	std::vector<uint32_t> _indices; 

	AllocatedBuffer _vertexBuffer; 
	AllocatedBuffer _indexBuffer; 

	bool load_from_obj(const char* filename);
};
