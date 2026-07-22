#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayerInterfaceTable.h>
#include <Jolt/Physics/Collision/BroadPhase/ObjectVsBroadPhaseLayerFilterTable.h>
#include <Jolt/Physics/Collision/ObjectLayerPairFilterTable.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include <glm/vec3.hpp>
#include <optional>

namespace gl3d
{
	struct Transform;
}

struct PhysicsControllerInput
{
	glm::vec3 moveDirection = {};
	bool wantsToRun = false;
	bool wantsToJump = false;
	bool wantsToClimb = false;
};

class PhysicsController
{
public:
	PhysicsController();
	~PhysicsController();

	PhysicsController(const PhysicsController &) = delete;
	PhysicsController &operator=(const PhysicsController &) = delete;

	bool init();
	void shutdown();
	bool setStaticMapCollision(const char *modelPath, float importScale, const gl3d::Transform &transform);

	void update(float deltaTime, const PhysicsControllerInput &input);

	glm::vec3 getPlayerPosition() const;
	glm::vec3 getPlayerVelocity() const;
	float getPlayerYaw() const;
	bool isGrounded() const;
	bool isWallAttached() const;

private:
	bool initialized_ = false;

	std::optional<JPH::BroadPhaseLayerInterfaceTable> broadPhaseLayerInterface_;
	std::optional<JPH::ObjectLayerPairFilterTable> objectVsObjectLayerFilter_;
	std::optional<JPH::ObjectVsBroadPhaseLayerFilterTable> objectVsBroadPhaseLayerFilter_;
	std::optional<JPH::PhysicsSystem> physicsSystem_;
	std::optional<JPH::TempAllocatorImpl> tempAllocator_;
	std::optional<JPH::JobSystemThreadPool> jobSystem_;
	std::optional<JPH::CharacterVirtual> character_;

	JPH::BodyID groundBodyId_;
	JPH::BodyID mapBodyId_;
	JPH::RefConst<JPH::Shape> standingShape_;
	float facingYaw_ = 0.0f;
	bool wallAttached_ = false;
};
