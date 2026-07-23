#pragma once

#include "PhysicsController.h"
#include "platformInput.h"

#include <gl2d/gl2d.h>
#include <gl3d.h>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <vector>

struct ClientGameplay
{
	struct PlayerPaintTexture
	{
		int meshIndex = -1;
		gl3d::Texture texture = {};
		glm::ivec2 size = {};
		int quality = gl3d::maxQuality;
		std::vector<unsigned char> pixels;
	};

	enum class CameraMode
	{
		Free,
		ThirdPerson
	};

	enum class PaintColorSlider
	{
		None,
		Hue,
		Saturation,
		Value
	};

	struct PaintDebugState
	{
		bool hoverValid = false;
		gl3d::PaintTargetSample hoverSample = {};
		bool clickValid = false;
		gl3d::PaintTargetSample clickSample = {};
		bool brushResizeActive = false;
		glm::ivec2 windowMousePosition = {};
		glm::ivec2 framebufferMousePosition = {};
	};

	bool init();
	bool update(float deltaTime, platform::Input &input, gl2d::Renderer2D &renderer, gl2d::Font &paintUiFont);
	void shutdown();

	gl3d::Renderer3D renderer3D;
	gl3d::Model playerModel;
	gl3d::Model mapModel;
	gl3d::Entity playerEntity;
	gl3d::Entity mapEntity;
	PhysicsController playerPhysics;

	CameraMode cameraMode = CameraMode::Free;
	float thirdPersonYaw = 0.0f;
	float thirdPersonPitch = -0.31415927f;
	float thirdPersonCameraDistance = 7.5f;
	bool paintModeActive = false;
	std::vector<PlayerPaintTexture> playerPaintTextures;
	int playerPaintBrushRadius = 6;
	float playerPaintBrushRadiusPrecise = 6.0f;
	glm::vec3 playerPaintColorHsv = {0.0f, 1.0f, 0.0f};
	bool paintColorPickModeActive = false;
	bool paintColorUiHovered = false;
	bool paintColorUiCapturingMouse = false;
	PaintColorSlider activePaintColorSlider = PaintColorSlider::None;
	PaintDebugState paintDebugState;
	bool hasLastPaintStrokeScreenPosition = false;
	glm::ivec2 lastPaintStrokeScreenPosition = {};
};
