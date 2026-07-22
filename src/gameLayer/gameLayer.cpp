#define GLM_ENABLE_EXPERIMENTAL
#include "gameLayer.h"
#include "PhysicsController.h"
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtx/transform.hpp>
#include "platformInput.h"
#include "imgui.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <sstream>
#include "imfilebrowser.h"
#include <gl2d/gl2d.h>
#include <platformTools.h>
#include <IconsForkAwesome.h>
#include <imguiTools.h>
#include <gl3d.h>
#include <imgui_internal.h>


gl2d::Renderer2D renderer;
gl3d::Renderer3D renderer3D;
gl3d::Model playerModel;
gl3d::Model mapModel;

#define PLAYER_ANIMATIONS 18
const float PLAYER_MODEL_YAW_OFFSET = 3.141592;
const float THIRD_PERSON_CAMERA_DISTANCE_DEFAULT = 7.5f;
const float THIRD_PERSON_CAMERA_DISTANCE_MIN = 2.5f;
const float THIRD_PERSON_CAMERA_DISTANCE_MAX = 12.0f;
const float THIRD_PERSON_CAMERA_ZOOM_STEP = 0.85f;
const float THIRD_PERSON_CAMERA_TARGET_HEIGHT = 1.45f;
const float THIRD_PERSON_PITCH_MIN = glm::radians(-70.0f);
const float THIRD_PERSON_PITCH_MAX = glm::radians(35.0f);

gl3d::Entity playerEntity;
gl3d::Entity mapEntity;
PhysicsController playerPhysics;

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

CameraMode cameraMode = CameraMode::Free;
float thirdPersonYaw = 0.0f;
float thirdPersonPitch = glm::radians(-18.0f);
float thirdPersonCameraDistance = THIRD_PERSON_CAMERA_DISTANCE_DEFAULT;
bool paintModeActive = false;
std::vector<PlayerPaintTexture> playerPaintTextures;
constexpr int PLAYER_PAINT_BRUSH_RADIUS_DEFAULT = 6;
constexpr int PLAYER_PAINT_BRUSH_RADIUS_MIN = 1;
constexpr int PLAYER_PAINT_BRUSH_RADIUS_MAX = 96;
constexpr float PLAYER_PAINT_BRUSH_RESIZE_SPEED = 0.12f;
constexpr float PLAYER_PAINT_BRUSH_PREVIEW_SCALE = 2.4f;
int playerPaintBrushRadius = PLAYER_PAINT_BRUSH_RADIUS_DEFAULT;
float playerPaintBrushRadiusPrecise = static_cast<float>(PLAYER_PAINT_BRUSH_RADIUS_DEFAULT);

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

PaintDebugState paintDebugState;
bool hasLastPaintStrokeScreenPosition = false;
glm::ivec2 lastPaintStrokeScreenPosition = {};

#define USE_GPU 1

#pragma region gpu
extern "C"
{
	__declspec(dllexport) unsigned long NvOptimusEnablement = USE_GPU;
	__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = USE_GPU;
}
#pragma endregion



glm::vec2 consumeLookDelta(glm::vec2 &lastMousePos, bool captureMouse)
{
	glm::vec2 delta = {};

	if (platform::hasFocused() && captureMouse)
	{
		platform::showMouse(false);

		glm::vec2 currentMousePos = platform::getRelMousePosition();
		constexpr float lookSpeed = 0.2f * (1.0f / 60.0f);
		delta = (lastMousePos - currentMousePos) * lookSpeed;
		lastMousePos = currentMousePos;
	}
	else
	{
		platform::showMouse(true);
		lastMousePos = platform::getRelMousePosition();
	}

	if (platform::isButtonPressed(platform::Button::Escape))
	{
		platform::showMouse(true);
		platform::loseFocus();
	}

	return delta;
}

glm::vec2 consumeMouseDragDelta(glm::vec2 &lastMousePos, bool dragActive)
{
	glm::vec2 currentMousePos = platform::getRelMousePosition();
	glm::vec2 delta = {};

	if (platform::hasFocused() && dragActive)
	{
		constexpr float lookSpeed = 0.2f * (1.0f / 60.0f);
		delta = (lastMousePos - currentMousePos) * lookSpeed;
	}

	lastMousePos = currentMousePos;
	return delta;
}

glm::vec2 consumeRawMouseDragDelta(glm::vec2 &lastMousePos, bool dragActive)
{
	glm::vec2 currentMousePos = platform::getRelMousePosition();
	glm::vec2 delta = {};

	if (platform::hasFocused() && dragActive)
	{
		delta = currentMousePos - lastMousePos;
	}

	lastMousePos = currentMousePos;
	return delta;
}

glm::vec3 buildThirdPersonForward(float yaw, float pitch)
{
	const float cosPitch = std::cos(pitch);
	return glm::normalize(glm::vec3(
		std::sin(yaw) * cosPitch,
		std::sin(pitch),
		-std::cos(yaw) * cosPitch));
}

glm::vec3 getPlanarForward(glm::vec3 forward, glm::vec3 fallback = {0.0f, 0.0f, -1.0f})
{
	forward.y = 0.0f;
	const float forwardLength = glm::length(forward);

	if (forwardLength < 0.0001f)
	{
		return fallback;
	}

	return forward / forwardLength;
}

glm::vec3 getRightFromForward(glm::vec3 forward)
{
	glm::vec3 right = glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f));
	const float rightLength = glm::length(right);

	if (rightLength < 0.0001f)
	{
		return {1.0f, 0.0f, 0.0f};
	}

	return right / rightLength;
}

void syncThirdPersonOrbitToCamera()
{
	const glm::vec3 viewDirection = glm::normalize(renderer3D.camera.viewDirection);
	thirdPersonPitch = std::asin(std::clamp(viewDirection.y, -1.0f, 1.0f));
	thirdPersonYaw = std::atan2(viewDirection.x, -viewDirection.z);
}

void setCameraMode(CameraMode newMode)
{
	if (cameraMode == newMode)
	{
		return;
	}

	if (newMode == CameraMode::ThirdPerson)
	{
		syncThirdPersonOrbitToCamera();
	}
	else
	{
		paintModeActive = false;
	}

	cameraMode = newMode;
}

void toggleCameraMode()
{
	setCameraMode(cameraMode == CameraMode::Free ? CameraMode::ThirdPerson : CameraMode::Free);
}

void applyFreeCameraInput(::gl3d::Renderer3D &renderer, float speed, float deltaTime, const glm::vec2 &lookDelta)
{
	glm::vec3 dir = {};
	if (platform::isButtonHeld(platform::Button::W))
	{
		dir.z -= speed * deltaTime;
	}
	if (platform::isButtonHeld(platform::Button::S))
	{
		dir.z += speed * deltaTime;
	}

	if (platform::isButtonHeld(platform::Button::A))
	{
		dir.x -= speed * deltaTime;
	}
	if (platform::isButtonHeld(platform::Button::D))
	{
		dir.x += speed * deltaTime;
	}

	if (platform::isButtonHeld(platform::Button::Q))
	{
		dir.y -= speed * deltaTime;
	}
	if (platform::isButtonHeld(platform::Button::E))
	{
		dir.y += speed * deltaTime;
	}

	renderer.camera.moveFPS(dir);
	renderer.camera.rotateCamera(lookDelta);
}

PhysicsControllerInput buildPlayerInput()
{
	PhysicsControllerInput result = {};

	const glm::vec3 planarForward = getPlanarForward(
		buildThirdPersonForward(thirdPersonYaw, 0.0f));
	const glm::vec3 right = getRightFromForward(planarForward);

	if (platform::isButtonHeld(platform::Button::W))
	{
		result.moveDirection += planarForward;
	}
	if (platform::isButtonHeld(platform::Button::S))
	{
		result.moveDirection -= planarForward;
	}
	if (platform::isButtonHeld(platform::Button::D))
	{
		result.moveDirection += right;
	}
	if (platform::isButtonHeld(platform::Button::A))
	{
		result.moveDirection -= right;
	}

	if (glm::length(result.moveDirection) > 0.0001f)
	{
		result.moveDirection = glm::normalize(result.moveDirection);
	}

	result.wantsToRun = platform::isButtonHeld(platform::Button::LeftShift);
	result.wantsToJump = platform::isButtonPressed(platform::Button::Space);
	return result;
}

void syncPlayerEntityToPhysics()
{
	gl3d::Transform playerTransform = renderer3D.getEntityTransform(playerEntity);
	playerTransform.position = playerPhysics.getPlayerPosition();
	playerTransform.rotation = {};
	playerTransform.rotation.y = playerPhysics.getPlayerYaw() + PLAYER_MODEL_YAW_OFFSET;
	renderer3D.setEntityTransform(playerEntity, playerTransform);
}

bool copyTextureToCpu(gl3d::Texture texture, std::vector<unsigned char> &pixels, glm::ivec2 &size, int &quality)
{
	gl3d::GpuTexture *gpuTexture = renderer3D.getTextureData(texture);
	if (gpuTexture == nullptr || gpuTexture->id == 0)
	{
		return false;
	}

	size = gpuTexture->getTextureSize();
	if (size.x <= 0 || size.y <= 0)
	{
		return false;
	}

	quality = gpuTexture->getTextureQuality();
	pixels.assign(size.x * size.y * 4, 255);

	glBindTexture(GL_TEXTURE_2D, gpuTexture->id);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
	return true;
}

void computeTextureAlphaFlags(const std::vector<unsigned char> &pixels, bool &alphaExists, bool &alphaHasData)
{
	alphaExists = false;
	alphaHasData = false;

	for (size_t i = 3; i < pixels.size(); i += 4)
	{
		if (pixels[i] != 255)
		{
			alphaExists = true;
		}

		if (pixels[i] != 0 && pixels[i] != 255)
		{
			alphaHasData = true;
		}

		if (alphaExists && alphaHasData)
		{
			return;
		}
	}
}

gl3d::Texture createPaintTexture(const PlayerPaintTexture &paintTexture)
{
	bool alphaExists = false;
	bool alphaHasData = false;
	computeTextureAlphaFlags(paintTexture.pixels, alphaExists, alphaHasData);

	gl3d::GpuTexture gpuTexture;
	gpuTexture.loadTextureFromMemory(
		const_cast<unsigned char *>(paintTexture.pixels.data()),
		paintTexture.size.x,
		paintTexture.size.y,
		4,
		paintTexture.quality);

	return renderer3D.createIntenralTexture(
		gpuTexture,
		alphaExists ? 1 : 0,
		alphaHasData ? 1 : 0,
		"playerPaintMesh" + std::to_string(paintTexture.meshIndex));
}

PlayerPaintTexture *getPlayerPaintTexture(int meshIndex)
{
	for (auto &paintTexture : playerPaintTextures)
	{
		if (paintTexture.meshIndex == meshIndex)
		{
			return &paintTexture;
		}
	}

	return nullptr;
}

bool isPaintBrushResizeActive(const platform::Input &input)
{
	return cameraMode == CameraMode::ThirdPerson
		&& paintModeActive
		&& input.rMouse.held;
}

glm::ivec2 getMouseFramebufferPosition(const platform::Input &input)
{
	const glm::ivec2 windowSize = platform::getWindowSize();
	const glm::ivec2 framebufferSize = platform::getFrameBufferSize();

	paintDebugState.windowMousePosition = {input.mouseX, input.mouseY};

	if (windowSize.x <= 0 || windowSize.y <= 0)
	{
		paintDebugState.framebufferMousePosition = paintDebugState.windowMousePosition;
		return paintDebugState.framebufferMousePosition;
	}

	paintDebugState.framebufferMousePosition = {
		input.mouseX * framebufferSize.x / windowSize.x,
		input.mouseY * framebufferSize.y / windowSize.y
	};

	return paintDebugState.framebufferMousePosition;
}

void uploadPlayerPaintTexture(PlayerPaintTexture &paintTexture)
{
	gl3d::GpuTexture *gpuTexture = renderer3D.getTextureData(paintTexture.texture);
	if (gpuTexture == nullptr || gpuTexture->id == 0)
	{
		return;
	}

	glBindTexture(GL_TEXTURE_2D, gpuTexture->id);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexSubImage2D(
		GL_TEXTURE_2D,
		0,
		0,
		0,
		paintTexture.size.x,
		paintTexture.size.y,
		GL_RGBA,
		GL_UNSIGNED_BYTE,
		paintTexture.pixels.data());

	if (paintTexture.quality > gl3d::linearNoMipmap)
	{
		glGenerateMipmap(GL_TEXTURE_2D);
	}
}

void paintTextureBlack(PlayerPaintTexture &paintTexture, glm::ivec2 center)
{
	for (int y = center.y - playerPaintBrushRadius; y <= center.y + playerPaintBrushRadius; ++y)
	{
		for (int x = center.x - playerPaintBrushRadius; x <= center.x + playerPaintBrushRadius; ++x)
		{
			if (x < 0 || y < 0 || x >= paintTexture.size.x || y >= paintTexture.size.y)
			{
				continue;
			}

			const glm::ivec2 delta = glm::ivec2(x, y) - center;
			if (delta.x * delta.x + delta.y * delta.y > playerPaintBrushRadius * playerPaintBrushRadius)
			{
				continue;
			}

			const size_t pixelIndex = static_cast<size_t>(x + y * paintTexture.size.x) * 4;
			paintTexture.pixels[pixelIndex + 0] = 0;
			paintTexture.pixels[pixelIndex + 1] = 0;
			paintTexture.pixels[pixelIndex + 2] = 0;
		}
	}
}

void resetPaintStrokeState()
{
	hasLastPaintStrokeScreenPosition = false;
	lastPaintStrokeScreenPosition = {};
}

bool paintInterpolatedStrokeScreenSpace(glm::ivec2 fromScreenPosition,
	glm::ivec2 toScreenPosition,
	gl3d::PaintTargetSample &lastSuccessfulSample)
{
	lastSuccessfulSample = {};

	const float previewRadius = (std::max)(
		1.0f,
		static_cast<float>(playerPaintBrushRadius) * PLAYER_PAINT_BRUSH_PREVIEW_SCALE);
	const float stepDistance = (std::max)(1.0f, previewRadius);
	const glm::vec2 from = glm::vec2(fromScreenPosition);
	const glm::vec2 to = glm::vec2(toScreenPosition);
	const float distance = glm::distance(from, to);
	const int interpolationSteps = (std::max)(1, static_cast<int>(std::ceil(distance / stepDistance)));
	std::vector<PlayerPaintTexture *> touchedTextures;
	bool paintedAny = false;

	for (int step = 0; step <= interpolationSteps; ++step)
	{
		const float t = static_cast<float>(step) / static_cast<float>(interpolationSteps);
		const glm::ivec2 interpolatedScreenPosition = glm::ivec2(glm::round(glm::mix(from, to, t)));
		gl3d::PaintTargetSample sample = {};
		if (!renderer3D.sampleEntityPaintTarget(interpolatedScreenPosition, sample))
		{
			continue;
		}

		PlayerPaintTexture *paintTexture = getPlayerPaintTexture(sample.meshIndex);
		if (paintTexture == nullptr)
		{
			continue;
		}

		paintTextureBlack(*paintTexture, sample.texturePixel);
		if (std::find(touchedTextures.begin(), touchedTextures.end(), paintTexture) == touchedTextures.end())
		{
			touchedTextures.push_back(paintTexture);
		}

		paintedAny = true;
		lastSuccessfulSample = sample;
	}

	for (PlayerPaintTexture *paintTexture : touchedTextures)
	{
		uploadPlayerPaintTexture(*paintTexture);
	}

	return paintedAny;
}

void setupPlayerPaintTextures()
{
	playerPaintTextures.clear();

	const int meshCount = renderer3D.getEntityMeshesCount(playerEntity);
	for (int meshIndex = 0; meshIndex < meshCount; ++meshIndex)
	{
		gl3d::TextureDataForMaterial materialTextures = renderer3D.getEntityMeshMaterialTextures(playerEntity, meshIndex);
		if (!renderer3D.isTexture(materialTextures.albedoTexture))
		{
			continue;
		}

		PlayerPaintTexture paintTexture = {};
		paintTexture.meshIndex = meshIndex;
		if (!copyTextureToCpu(materialTextures.albedoTexture, paintTexture.pixels, paintTexture.size, paintTexture.quality))
		{
			continue;
		}

		paintTexture.texture = createPaintTexture(paintTexture);
		materialTextures.albedoTexture = paintTexture.texture;
		renderer3D.setEntityMeshMaterialTextures(playerEntity, meshIndex, materialTextures);
		playerPaintTextures.push_back(std::move(paintTexture));
	}

	renderer3D.setEntityPaintTarget(playerEntity);
}

void updatePaintBrushSize(const platform::Input &input)
{
	paintDebugState.brushResizeActive = isPaintBrushResizeActive(input);

	static glm::vec2 lastPaintBrushResizeMousePosition = {};
	const glm::vec2 resizeDelta = consumeRawMouseDragDelta(
		lastPaintBrushResizeMousePosition,
		paintDebugState.brushResizeActive);

	if (!paintDebugState.brushResizeActive)
	{
		return;
	}

	playerPaintBrushRadiusPrecise = std::clamp(
		playerPaintBrushRadiusPrecise + resizeDelta.x * PLAYER_PAINT_BRUSH_RESIZE_SPEED,
		static_cast<float>(PLAYER_PAINT_BRUSH_RADIUS_MIN),
		static_cast<float>(PLAYER_PAINT_BRUSH_RADIUS_MAX));
	playerPaintBrushRadius = static_cast<int>(std::round(playerPaintBrushRadiusPrecise));
}

void paintPlayerFromCursor(platform::Input &input)
{
	paintDebugState.hoverValid = false;
	paintDebugState.clickValid = false;

	if (!paintModeActive || cameraMode != CameraMode::ThirdPerson)
	{
		resetPaintStrokeState();
		return;
	}

	const glm::ivec2 currentScreenPosition = getMouseFramebufferPosition(input);
	gl3d::PaintTargetSample hoverSample = {};
	if (!renderer3D.sampleEntityPaintTarget(currentScreenPosition, hoverSample))
	{
		paintDebugState.hoverValid = false;
	}
	else
	{
		paintDebugState.hoverValid = true;
		paintDebugState.hoverSample = hoverSample;
	}

	if (paintDebugState.brushResizeActive)
	{
		resetPaintStrokeState();
		return;
	}

	if (!input.isLMouseHeld())
	{
		resetPaintStrokeState();
		return;
	}

	const glm::ivec2 strokeStartPosition = hasLastPaintStrokeScreenPosition
		? lastPaintStrokeScreenPosition
		: currentScreenPosition;

	gl3d::PaintTargetSample lastSuccessfulSample = {};
	paintDebugState.clickValid = paintInterpolatedStrokeScreenSpace(
		strokeStartPosition,
		currentScreenPosition,
		lastSuccessfulSample);
	if (paintDebugState.clickValid)
	{
		paintDebugState.clickSample = lastSuccessfulSample;
	}

	hasLastPaintStrokeScreenPosition = true;
	lastPaintStrokeScreenPosition = currentScreenPosition;
}

void renderPaintBrushOverlay(const platform::Input &input)
{
	if (!paintModeActive || cameraMode != CameraMode::ThirdPerson)
	{
		return;
	}

	const glm::vec2 mousePosition = glm::vec2(getMouseFramebufferPosition(input));
	const float previewRadius = (std::max)(
		8.0f,
		static_cast<float>(playerPaintBrushRadius) * PLAYER_PAINT_BRUSH_PREVIEW_SCALE);

	gl2d::Color4f brushColor = paintDebugState.hoverValid
		? gl2d::Color4f{1.0f, 1.0f, 1.0f, 0.95f}
		: gl2d::Color4f{1.0f, 0.35f, 0.35f, 0.95f};

	if (paintDebugState.brushResizeActive)
	{
		brushColor = {1.0f, 0.85f, 0.2f, 0.98f};
	}

	renderer.renderCircleOutline(mousePosition, previewRadius + 1.0f, {0.0f, 0.0f, 0.0f, 0.9f}, 3.0f, 40);
	renderer.renderCircleOutline(mousePosition, previewRadius, brushColor, 2.0f, 40);
}

void updateThirdPersonCameraZoom(bool ignoreImguiCapture = false)
{
	ImGuiIO &io = ImGui::GetIO();
	if ((!ignoreImguiCapture && io.WantCaptureMouse) || std::abs(io.MouseWheel) < 0.001f)
	{
		return;
	}

	thirdPersonCameraDistance = std::clamp(
		thirdPersonCameraDistance - io.MouseWheel * THIRD_PERSON_CAMERA_ZOOM_STEP,
		THIRD_PERSON_CAMERA_DISTANCE_MIN,
		THIRD_PERSON_CAMERA_DISTANCE_MAX);
}

void updateThirdPersonCamera()
{
	const glm::vec3 playerPosition = playerPhysics.getPlayerPosition();
	const glm::vec3 cameraForward = buildThirdPersonForward(thirdPersonYaw, thirdPersonPitch);
	const glm::vec3 target = playerPosition + glm::vec3(0.0f, THIRD_PERSON_CAMERA_TARGET_HEIGHT, 0.0f);

	renderer3D.camera.viewDirection = cameraForward;
	renderer3D.camera.position = target - cameraForward * thirdPersonCameraDistance;
}

bool initGame()
{
#pragma region init stuff
	//initializing stuff for the renderer
	gl2d::init();
	renderer.create();

	platform::showMouse(false);

	renderer3D.init(1, 1, RESOURCES_PATH "BRDFintegrationMap.png");

	renderer3D.skyBox = renderer3D.loadHDRSkyBox(RESOURCES_PATH "sky.hdr");
	//renderer3D.skyBox.color = glm::vec4{0.8,0.8, 0.8,1};

	playerModel = renderer3D.loadModel(RESOURCES_PATH "player3.glb", gl3d::maxQuality, 1);


	playerEntity = renderer3D.createEntity(playerModel, {}, false, true, false);
	renderer3D.setEntityAnimate(playerEntity, true);
	setupPlayerPaintTextures();

	if (!playerPhysics.init())
	{
		return false;
	}

	syncPlayerEntityToPhysics();



#pragma region map

	if (1)
	{

		gl3d::Transform mapTransform;
		mapTransform.rotation.x = glm::radians(-90.f);
		mapTransform.position.x = -90;
		mapTransform.position.z = 120;

		mapModel = renderer3D.loadModel(RESOURCES_PATH "amongusMap.glb", gl3d::maxQuality, 0.1);

		mapEntity = renderer3D.createEntity(mapModel, mapTransform, true, true, false);
		playerPhysics.setStaticMapCollision(RESOURCES_PATH "amongusMap.glb", 0.1f, mapTransform);

	};

#pragma endregion


	auto grid = platform::createGridModel(
		renderer3D,
		200.f,
		41,
		{0.35f, 0.35f, 0.38f, 1.f},
		0.f,
		0.f,
		"worldGrid");
	renderer3D.createEntity(grid.model, {}, true, true, false);

	renderer3D.createDirectionalLight({0,-1,0.3}, glm::vec3(1, 1, 1), 1, false);
	renderer3D.createDirectionalLight({0,-1,-0.3}, glm::vec3(1, 1, 1), 1, false);
	renderer3D.createDirectionalLight({0.3,-1,0.0}, glm::vec3(0.4, 0.4, 0.4), 1, false);
	renderer3D.createDirectionalLight({-0.3,1,0.0}, glm::vec3(0.4, 0.4, 0.4), 1, false);

	renderer3D.camera.position = {0.0f, 2.2f, 6.0f};
	renderer3D.camera.viewDirection = glm::normalize(
		glm::vec3(0.0f, THIRD_PERSON_CAMERA_TARGET_HEIGHT, 0.0f) - renderer3D.camera.position);
	syncThirdPersonOrbitToCamera();
#pragma endregion



	return true;
}


bool gameLogic(float deltaTime, platform::Input &input)
{
#pragma region init stuff
	int w = 0; int h = 0;
	w = platform::getFrameBufferSizeX(); //window w
	h = platform::getFrameBufferSizeY(); //window h
	(void)input;
	
	glViewport(0, 0, w, h);
	glDisable(GL_DITHER);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	renderer.updateWindowMetrics(w, h);

#pragma endregion

	//renderer.renderRectangle({100,100, 100, 100}, Colors_Blue);


	renderer3D.updateWindowMetrics(w, h);

	if (platform::isButtonPressed(platform::Button::Tab))
	{
		toggleCameraMode();
	}

	if (cameraMode == CameraMode::ThirdPerson && platform::isButtonPressed(platform::Button::F))
	{
		paintModeActive = !paintModeActive;
	}

	updatePaintBrushSize(input);

	const bool captureMouseLook = cameraMode == CameraMode::Free || !paintModeActive;
	static glm::vec2 lastMousePosition = {};
	static glm::vec2 lastPaintOrbitMousePosition = {};
	const glm::vec2 lookDelta = consumeLookDelta(lastMousePosition, captureMouseLook);
	const glm::vec2 paintOrbitDelta = consumeMouseDragDelta(
		lastPaintOrbitMousePosition,
		cameraMode == CameraMode::ThirdPerson && paintModeActive && input.isMMouseHeld());

	PhysicsControllerInput playerInput = {};
	if (cameraMode == CameraMode::Free)
	{
		applyFreeCameraInput(renderer3D, 20.0f, deltaTime, lookDelta);
	}
	else
	{
		if (paintModeActive)
		{
			thirdPersonYaw -= paintOrbitDelta.x;
			thirdPersonPitch = std::clamp(
				thirdPersonPitch + paintOrbitDelta.y,
				THIRD_PERSON_PITCH_MIN,
				THIRD_PERSON_PITCH_MAX);
			updateThirdPersonCameraZoom(true);
		}
		else
		{
			thirdPersonYaw -= lookDelta.x;
			thirdPersonPitch = std::clamp(
				thirdPersonPitch + lookDelta.y,
				THIRD_PERSON_PITCH_MIN,
				THIRD_PERSON_PITCH_MAX);
			updateThirdPersonCameraZoom();
			playerInput = buildPlayerInput();
		}
	}

	playerPhysics.update(deltaTime, playerInput);
	syncPlayerEntityToPhysics();

	if (cameraMode == CameraMode::ThirdPerson)
	{
		updateThirdPersonCamera();
	}


	renderer3D.render(deltaTime);
	paintPlayerFromCursor(input);
	renderPaintBrushOverlay(input);


	renderer.flush();


	{
		ImGuiViewport *mainVp = ImGui::GetMainViewport();

		ImGuiWindowClass windowClass;
		windowClass.DockingAllowUnclassed = false;
		windowClass.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoDocking;
		windowClass.ViewportFlagsOverrideSet = ImGuiViewportFlags_NoAutoMerge;
		ImGui::SetNextWindowClass(&windowClass);

		// place it to the right of the main window
		ImGui::SetNextWindowPos(
			ImVec2(mainVp->Pos.x + mainVp->Size.x + 20, mainVp->Pos.y + 50),
			ImGuiCond_FirstUseEver
		);


		ImGui::PushMakeWindowNotTransparent();
		ImGui::Begin("Tweaks");


		static int animation = 9;
		ImGui::SliderInt("Animation", &animation, 0, PLAYER_ANIMATIONS - 1);
		renderer3D.setEntityAnimationIndex(playerEntity, animation);
		ImGui::Text("Camera: %s", cameraMode == CameraMode::Free ? "Free" : "Third-Person");
		if (ImGui::Button(cameraMode == CameraMode::Free ? "Switch to Third-Person (Tab)" : "Switch to Free Camera (Tab)"))
		{
			toggleCameraMode();
		}
		ImGui::Text("Free camera: WASD + Q/E");
		ImGui::Text("Player: WASD move, Shift run, Space jump");
		ImGui::Text("Paint mode: %s", paintModeActive ? "On" : "Off");
		ImGui::Text("Paint: F toggle, Left click paint, Right drag resize, Middle drag orbit, Scroll zoom");
		ImGui::Text("Paint brush radius: %d%s", playerPaintBrushRadius, paintDebugState.brushResizeActive ? " (resizing)" : "");
		ImGui::Text("Paint textures: %d", static_cast<int>(playerPaintTextures.size()));
		ImGui::Text(
			"Mouse window %d,%d framebuffer %d,%d",
			paintDebugState.windowMousePosition.x,
			paintDebugState.windowMousePosition.y,
			paintDebugState.framebufferMousePosition.x,
			paintDebugState.framebufferMousePosition.y);
		ImGui::Text("ImGui wants mouse: %s", ImGui::GetIO().WantCaptureMouse ? "Yes" : "No");
		ImGui::Text("Paint hover hit: %s", paintDebugState.hoverValid ? "Yes" : "No");
		if (paintDebugState.hoverValid)
		{
			ImGui::Text(
				"Hover mesh %d texel %d,%d / %d,%d",
				paintDebugState.hoverSample.meshIndex,
				paintDebugState.hoverSample.texturePixel.x,
				paintDebugState.hoverSample.texturePixel.y,
				paintDebugState.hoverSample.textureSize.x,
				paintDebugState.hoverSample.textureSize.y);
		}
		ImGui::Text("Paint click hit: %s", paintDebugState.clickValid ? "Yes" : "No");
		if (paintDebugState.clickValid)
		{
			ImGui::Text(
				"Last click mesh %d texel %d,%d",
				paintDebugState.clickSample.meshIndex,
				paintDebugState.clickSample.texturePixel.x,
				paintDebugState.clickSample.texturePixel.y);
		}
		ImGui::Text("Third-person zoom: Mouse wheel (%.1f)", thirdPersonCameraDistance);
		ImGui::Text("Grounded: %s", playerPhysics.isGrounded() ? "Yes" : "No");


		ImGui::PopMakeWindowNotTransparent();
		ImGui::End();
	}

	return true;
#pragma endregion

}

//This function might not be be called if the program is forced closed
void closeGame()
{
	playerPhysics.shutdown();
}
