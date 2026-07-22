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
const float THIRD_PERSON_CAMERA_DISTANCE = 5.0f;
const float THIRD_PERSON_CAMERA_TARGET_HEIGHT = 1.45f;
const float THIRD_PERSON_PITCH_MIN = glm::radians(-70.0f);
const float THIRD_PERSON_PITCH_MAX = glm::radians(35.0f);

gl3d::Entity playerEntity;
gl3d::Entity mapEntity;
PhysicsController playerPhysics;

enum class CameraMode
{
	Free,
	ThirdPerson
};

CameraMode cameraMode = CameraMode::Free;
float thirdPersonYaw = 0.0f;
float thirdPersonPitch = glm::radians(-18.0f);

#define USE_GPU 1

#pragma region gpu
extern "C"
{
	__declspec(dllexport) unsigned long NvOptimusEnablement = USE_GPU;
	__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = USE_GPU;
}
#pragma endregion



glm::vec2 consumeLookDelta(glm::vec2 &lastMousePos)
{
	glm::vec2 delta = {};

	if (platform::hasFocused())
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

void updateThirdPersonCamera()
{
	const glm::vec3 playerPosition = playerPhysics.getPlayerPosition();
	const glm::vec3 cameraForward = buildThirdPersonForward(thirdPersonYaw, thirdPersonPitch);
	const glm::vec3 target = playerPosition + glm::vec3(0.0f, THIRD_PERSON_CAMERA_TARGET_HEIGHT, 0.0f);

	renderer3D.camera.viewDirection = cameraForward;
	renderer3D.camera.position = target - cameraForward * THIRD_PERSON_CAMERA_DISTANCE;
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

	playerModel = renderer3D.loadModel(RESOURCES_PATH "player2.glb", gl3d::maxQuality, 1);


	playerEntity = renderer3D.createEntity(playerModel, {}, false, true, false);
	renderer3D.setEntityAnimate(playerEntity, true);

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

	static glm::vec2 lastMousePosition = {};
	const glm::vec2 lookDelta = consumeLookDelta(lastMousePosition);

	if (platform::isButtonPressed(platform::Button::Tab))
	{
		toggleCameraMode();
	}

	PhysicsControllerInput playerInput = {};
	if (cameraMode == CameraMode::Free)
	{
		applyFreeCameraInput(renderer3D, 20.0f, deltaTime, lookDelta);
	}
	else
	{
		thirdPersonYaw -= lookDelta.x;
		thirdPersonPitch = std::clamp(thirdPersonPitch + lookDelta.y, THIRD_PERSON_PITCH_MIN, THIRD_PERSON_PITCH_MAX);
		playerInput = buildPlayerInput();
	}

	playerPhysics.update(deltaTime, playerInput);
	syncPlayerEntityToPhysics();

	if (cameraMode == CameraMode::ThirdPerson)
	{
		updateThirdPersonCamera();
	}


	renderer3D.render(deltaTime);


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


		static int animation = 0;
		ImGui::SliderInt("Animation", &animation, 0, PLAYER_ANIMATIONS - 1);
		renderer3D.setEntityAnimationIndex(playerEntity, animation);
		ImGui::Text("Camera: %s", cameraMode == CameraMode::Free ? "Free" : "Third-Person");
		if (ImGui::Button(cameraMode == CameraMode::Free ? "Switch to Third-Person (Tab)" : "Switch to Free Camera (Tab)"))
		{
			toggleCameraMode();
		}
		ImGui::Text("Free camera: WASD + Q/E");
		ImGui::Text("Player: WASD move, Shift run, Space jump");
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
