#pragma once

#include <gl3d.h>
#include <glm/vec4.hpp>
#include <string>

namespace platform
{
	struct GridModelResources
	{
		gl3d::Model model = {};
		gl3d::Material material = {};
	};

	// Builds a square grid on the XZ plane out of thin quads so it can be rendered as a regular model.
	GridModelResources createGridModel(
		gl3d::Renderer3D &renderer,
		float size,
		unsigned int lineCountPerAxis,
		glm::vec4 color = {0.45f, 0.45f, 0.45f, 1.f},
		float lineThickness = 0.f,
		float y = 0.f,
		const std::string &name = "worldGrid");

	gl3d::Model createGridModel(
		gl3d::Renderer3D &renderer,
		gl3d::Material material,
		float size,
		unsigned int lineCountPerAxis,
		float lineThickness = 0.f,
		float y = 0.f,
		const std::string &name = "worldGrid");
}
