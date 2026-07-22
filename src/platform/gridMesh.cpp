#include <platform/gridMesh.h>

#include <platform/platformTools.h>

#include <algorithm>
#include <vector>

namespace
{
	void appendGridQuad(std::vector<float> &vertices, std::vector<unsigned int> &indices,
		float minX, float maxX, float minZ, float maxZ, float y)
	{
		const unsigned int startIndex = static_cast<unsigned int>(vertices.size() / 6);

		const float quadVertices[] =
		{
			minX, y, minZ, 0.f, 1.f, 0.f,
			maxX, y, minZ, 0.f, 1.f, 0.f,
			maxX, y, maxZ, 0.f, 1.f, 0.f,
			minX, y, maxZ, 0.f, 1.f, 0.f,
		};

		vertices.insert(vertices.end(), std::begin(quadVertices), std::end(quadVertices));

		const unsigned int quadIndices[] =
		{
			startIndex + 0, startIndex + 2, startIndex + 1,
			startIndex + 0, startIndex + 3, startIndex + 2,
		};

		indices.insert(indices.end(), std::begin(quadIndices), std::end(quadIndices));
	}

	gl3d::Model createGridModelWithMaterial(gl3d::Renderer3D &renderer, gl3d::Material material,
		float size, unsigned int lineCountPerAxis, float lineThickness, float y,
		const std::string &name)
	{
		permaAssertComment(size > 0.f, "Grid size must be positive.");
		permaAssertComment(lineCountPerAxis >= 2, "Grid needs at least 2 lines per axis.");

		size = std::max(size, 0.001f);
		lineCountPerAxis = std::max(lineCountPerAxis, 2u);

		const float halfSize = size * 0.5f;
		const float step = size / static_cast<float>(lineCountPerAxis - 1);
		const float safeThickness = (lineThickness > 0.f) ? lineThickness : (step * 0.025f);
		const float halfThickness = std::min(safeThickness * 0.5f, step * 0.49f);
		const float perpendicularLift = 0.0005f;

		const unsigned int totalLines = lineCountPerAxis * 2;

		std::vector<float> vertices;
		vertices.reserve(static_cast<size_t>(totalLines) * 4u * 6u);

		std::vector<unsigned int> indices;
		indices.reserve(static_cast<size_t>(totalLines) * 6u);

		for (unsigned int i = 0; i < lineCountPerAxis; ++i)
		{
			const float offset = -halfSize + (step * static_cast<float>(i));

			appendGridQuad(
				vertices,
				indices,
				offset - halfThickness,
				offset + halfThickness,
				-halfSize,
				halfSize,
				y);

			appendGridQuad(
				vertices,
				indices,
				-halfSize,
				halfSize,
				offset - halfThickness,
				offset + halfThickness,
				y + perpendicularLift);
		}

		return renderer.createModelFromData(
			material,
			name,
			vertices.size(),
			vertices.data(),
			indices.size(),
			indices.data(),
			true);
	}
}

namespace platform
{
	GridModelResources createGridModel(gl3d::Renderer3D &renderer, float size,
		unsigned int lineCountPerAxis, glm::vec4 color, float lineThickness, float y,
		const std::string &name)
	{
		GridModelResources result;
		result.material = renderer.createMaterial(gl3d::maxQuality, color, 1.f, 0.f, 1.f, 0.f, name + "_material");
		result.model = createGridModelWithMaterial(renderer, result.material, size,
			lineCountPerAxis, lineThickness, y, name);
		return result;
	}

	gl3d::Model createGridModel(gl3d::Renderer3D &renderer, gl3d::Material material,
		float size, unsigned int lineCountPerAxis, float lineThickness, float y,
		const std::string &name)
	{
		return createGridModelWithMaterial(renderer, material, size,
			lineCountPerAxis, lineThickness, y, name);
	}
}
