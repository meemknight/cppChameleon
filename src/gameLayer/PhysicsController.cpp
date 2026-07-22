#include "PhysicsController.h"

#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Geometry/Triangle.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayerInterfaceTable.h>
#include <Jolt/Physics/Collision/BroadPhase/ObjectVsBroadPhaseLayerFilterTable.h>
#include <Jolt/Physics/Collision/ObjectLayerPairFilterTable.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include <gl3d.h>

#include <glm/geometric.hpp>
#include <glm/mat4x4.hpp>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <thread>

using namespace JPH;
using namespace JPH::literals;

namespace
{
	constexpr ObjectLayer cNonMovingLayer = 0;
	constexpr ObjectLayer cMovingLayer = 1;
	constexpr uint cObjectLayerCount = 2;

	constexpr BroadPhaseLayer cNonMovingBroadPhaseLayer(0);
	constexpr BroadPhaseLayer cMovingBroadPhaseLayer(1);
	constexpr uint cBroadPhaseLayerCount = 2;

	constexpr uint cMaxBodies = 1024;
	constexpr uint cNumBodyMutexes = 0;
	constexpr uint cMaxBodyPairs = 1024;
	constexpr uint cMaxContactConstraints = 1024;
	constexpr size_t cTempAllocatorSize = 10 * 1024 * 1024;

	constexpr float cFloorHalfExtent = 2000.0f;
	constexpr float cFloorHalfHeight = 1.0f;

	constexpr float cCharacterHeightStanding = 1.35f;
	constexpr float cCharacterRadiusStanding = 0.30f;
	constexpr float cWalkSpeed = 9.0f;
	constexpr float cRunSpeed = 17.0f;
	constexpr float cJumpSpeed = 5.5f;
	constexpr float cSpawnHeightOffset = 0.15f;

	void TraceImpl(const char *inFMT, ...)
	{
		va_list list;
		va_start(list, inFMT);

		char buffer[1024] = {};
		vsnprintf(buffer, sizeof(buffer), inFMT, list);

		va_end(list);
	}

#ifdef JPH_ENABLE_ASSERTS
	bool AssertFailedImpl(const char *inExpression, const char *inMessage, const char *inFile, uint inLine)
	{
		(void)inExpression;
		(void)inMessage;
		(void)inFile;
		(void)inLine;
		return true;
	}
#endif

	Vec3 toJolt(glm::vec3 value)
	{
		return Vec3(value.x, value.y, value.z);
	}

	template <typename TVector>
	glm::vec3 toGlm(const TVector &value)
	{
		return glm::vec3(
			static_cast<float>(value.GetX()),
			static_cast<float>(value.GetY()),
			static_cast<float>(value.GetZ()));
	}

	int getPhysicsWorkerCount()
	{
		const unsigned int hardwareThreadCount = std::max(1u, std::thread::hardware_concurrency());
		return static_cast<int>(std::max(1u, hardwareThreadCount - 1u));
	}

	Float3 toFloat3(const glm::vec3 &value)
	{
		return Float3(value.x, value.y, value.z);
	}

	glm::vec3 transformPoint(const glm::mat4 &transform, const glm::vec3 &point)
	{
		return glm::vec3(transform * glm::vec4(point, 1.0f));
	}

	glm::vec3 toGlm(const objl::Vector3 &value)
	{
		return glm::vec3(value.X, value.Y, value.Z);
	}

	glm::vec3 toGlm(const Float3 &value)
	{
		return glm::vec3(value.x, value.y, value.z);
	}

	void appendTriangle(
		TriangleList &triangles,
		const glm::mat4 &transform,
		bool reverseWinding,
		const glm::vec3 &a,
		const glm::vec3 &b,
		const glm::vec3 &c)
	{
		const Float3 transformedA = toFloat3(transformPoint(transform, a));
		const Float3 transformedB = toFloat3(transformPoint(transform, b));
		const Float3 transformedC = toFloat3(transformPoint(transform, c));

		if (reverseWinding)
		{
			triangles.emplace_back(transformedA, transformedC, transformedB);
		}
		else
		{
			triangles.emplace_back(transformedA, transformedB, transformedC);
		}
	}

	void appendStaticMeshTriangles(const objl::Mesh &mesh, const glm::mat4 &transform, bool reverseWinding, TriangleList &triangles)
	{
		const auto appendVertexTriangle = [&](const auto &vertices, unsigned int i0, unsigned int i1, unsigned int i2)
		{
			appendTriangle(
				triangles,
				transform,
				reverseWinding,
				toGlm(vertices[i0].Position),
				toGlm(vertices[i1].Position),
				toGlm(vertices[i2].Position));
		};

		if (!mesh.Vertices.empty())
		{
			if (!mesh.Indices.empty())
			{
				for (size_t index = 0; index + 2 < mesh.Indices.size(); index += 3)
				{
					appendVertexTriangle(mesh.Vertices, mesh.Indices[index], mesh.Indices[index + 1], mesh.Indices[index + 2]);
				}
			}
			else
			{
				for (size_t index = 0; index + 2 < mesh.Vertices.size(); index += 3)
				{
					appendVertexTriangle(mesh.Vertices, static_cast<unsigned int>(index), static_cast<unsigned int>(index + 1), static_cast<unsigned int>(index + 2));
				}
			}
		}
		else if (!mesh.VerticesAnimations.empty())
		{
			if (!mesh.Indices.empty())
			{
				for (size_t index = 0; index + 2 < mesh.Indices.size(); index += 3)
				{
					appendVertexTriangle(mesh.VerticesAnimations, mesh.Indices[index], mesh.Indices[index + 1], mesh.Indices[index + 2]);
				}
			}
			else
			{
				for (size_t index = 0; index + 2 < mesh.VerticesAnimations.size(); index += 3)
				{
					appendVertexTriangle(mesh.VerticesAnimations, static_cast<unsigned int>(index), static_cast<unsigned int>(index + 1), static_cast<unsigned int>(index + 2));
				}
			}
		}
	}

	glm::vec3 chooseSpawnPosition(const TriangleList &triangles)
	{
		bool foundWalkableTriangle = false;
		float bestTriangleArea = -1.0f;
		float bestTriangleHeight = -FLT_MAX;
		glm::vec3 bestSpawnPosition = {};

		glm::vec3 minBounds(FLT_MAX);
		glm::vec3 maxBounds(-FLT_MAX);

		for (const Triangle &triangle : triangles)
		{
			const glm::vec3 a = toGlm(triangle.mV[0]);
			const glm::vec3 b = toGlm(triangle.mV[1]);
			const glm::vec3 c = toGlm(triangle.mV[2]);

			minBounds = glm::min(minBounds, glm::min(a, glm::min(b, c)));
			maxBounds = glm::max(maxBounds, glm::max(a, glm::max(b, c)));

			const glm::vec3 edge1 = b - a;
			const glm::vec3 edge2 = c - a;
			const glm::vec3 crossProduct = glm::cross(edge1, edge2);
			const float areaTwice = glm::length(crossProduct);
			if (areaTwice < 0.0001f)
			{
				continue;
			}

			const glm::vec3 normal = crossProduct / areaTwice;
			if (normal.y < 0.55f)
			{
				continue;
			}

			const glm::vec3 center = (a + b + c) / 3.0f;
			const float topY = std::max(a.y, std::max(b.y, c.y));
			const bool isBetterTriangle = !foundWalkableTriangle
				|| areaTwice > bestTriangleArea
				|| (std::abs(areaTwice - bestTriangleArea) < 0.0001f && topY > bestTriangleHeight);

			if (isBetterTriangle)
			{
				foundWalkableTriangle = true;
				bestTriangleArea = areaTwice;
				bestTriangleHeight = topY;
				bestSpawnPosition = {center.x, topY + cSpawnHeightOffset, center.z};
			}
		}

		if (foundWalkableTriangle)
		{
			return bestSpawnPosition;
		}

		const glm::vec3 boundsCenter = (minBounds + maxBounds) * 0.5f;
		return {boundsCenter.x, maxBounds.y + cSpawnHeightOffset, boundsCenter.z};
	}
}

PhysicsController::PhysicsController() = default;

PhysicsController::~PhysicsController()
{
	shutdown();
}

bool PhysicsController::init()
{
	if (initialized_)
	{
		return true;
	}

	RegisterDefaultAllocator();
	Trace = TraceImpl;
	JPH_IF_ENABLE_ASSERTS(AssertFailed = AssertFailedImpl;)

	Factory::sInstance = new Factory();
	RegisterTypes();

	broadPhaseLayerInterface_.emplace(cObjectLayerCount, cBroadPhaseLayerCount);
	objectVsObjectLayerFilter_.emplace(cObjectLayerCount);
	broadPhaseLayerInterface_->MapObjectToBroadPhaseLayer(cNonMovingLayer, cNonMovingBroadPhaseLayer);
	broadPhaseLayerInterface_->MapObjectToBroadPhaseLayer(cMovingLayer, cMovingBroadPhaseLayer);
	objectVsObjectLayerFilter_->EnableCollision(cNonMovingLayer, cMovingLayer);
	objectVsObjectLayerFilter_->EnableCollision(cMovingLayer, cMovingLayer);
	objectVsBroadPhaseLayerFilter_.emplace(
		*broadPhaseLayerInterface_,
		cBroadPhaseLayerCount,
		*objectVsObjectLayerFilter_,
		cObjectLayerCount);

	physicsSystem_.emplace();
	tempAllocator_.emplace(cTempAllocatorSize);
	jobSystem_.emplace(
		cMaxPhysicsJobs,
		cMaxPhysicsBarriers,
		getPhysicsWorkerCount());

	physicsSystem_->Init(
		cMaxBodies,
		cNumBodyMutexes,
		cMaxBodyPairs,
		cMaxContactConstraints,
		*broadPhaseLayerInterface_,
		*objectVsBroadPhaseLayerFilter_,
		*objectVsObjectLayerFilter_);

	BodyInterface &bodyInterface = physicsSystem_->GetBodyInterface();

	BodyCreationSettings floorSettings(
		new BoxShape(Vec3(cFloorHalfExtent, cFloorHalfHeight, cFloorHalfExtent)),
		RVec3(0.0_r, -cFloorHalfHeight, 0.0_r),
		Quat::sIdentity(),
		EMotionType::Static,
		cNonMovingLayer);
	groundBodyId_ = bodyInterface.CreateAndAddBody(floorSettings, EActivation::DontActivate);

	standingShape_ = RotatedTranslatedShapeSettings(
		Vec3(0.0f, 0.5f * cCharacterHeightStanding + cCharacterRadiusStanding, 0.0f),
		Quat::sIdentity(),
		new CapsuleShape(0.5f * cCharacterHeightStanding, cCharacterRadiusStanding)).Create().Get();

	Ref<CharacterVirtualSettings> settings = new CharacterVirtualSettings();
	settings->mShape = standingShape_;
	settings->mMass = 80.0f;
	settings->mMaxSlopeAngle = DegreesToRadians(50.0f);
	settings->mMaxStrength = 100.0f;
	settings->mCharacterPadding = 0.02f;
	settings->mPenetrationRecoverySpeed = 1.0f;
	settings->mPredictiveContactDistance = 0.1f;
	settings->mSupportingVolume = Plane(Vec3::sAxisY(), -cCharacterRadiusStanding);
	settings->mEnhancedInternalEdgeRemoval = true;

	character_.emplace(
		settings,
		RVec3(0.0_r, 0.0_r, 0.0_r),
		Quat::sIdentity(),
		&*physicsSystem_);
	character_->RefreshContacts(
		physicsSystem_->GetDefaultBroadPhaseLayerFilter(cMovingLayer),
		physicsSystem_->GetDefaultLayerFilter(cMovingLayer),
		{},
		{},
		*tempAllocator_);

	physicsSystem_->OptimizeBroadPhase();

	initialized_ = true;
	return true;
}

void PhysicsController::shutdown()
{
	if (!initialized_)
	{
		return;
	}

	character_.reset();

	if (physicsSystem_ && !groundBodyId_.IsInvalid())
	{
		BodyInterface &bodyInterface = physicsSystem_->GetBodyInterface();

		if (!mapBodyId_.IsInvalid())
		{
			bodyInterface.RemoveBody(mapBodyId_);
			bodyInterface.DestroyBody(mapBodyId_);
			mapBodyId_ = BodyID();
		}

		bodyInterface.RemoveBody(groundBodyId_);
		bodyInterface.DestroyBody(groundBodyId_);
		groundBodyId_ = BodyID();
	}

	jobSystem_.reset();
	tempAllocator_.reset();
	physicsSystem_.reset();
	objectVsBroadPhaseLayerFilter_.reset();
	objectVsObjectLayerFilter_.reset();
	broadPhaseLayerInterface_.reset();
	standingShape_ = nullptr;
	facingYaw_ = 0.0f;
	initialized_ = false;

	if (Factory::sInstance != nullptr)
	{
		UnregisterTypes();
		delete Factory::sInstance;
		Factory::sInstance = nullptr;
	}
}

bool PhysicsController::setStaticMapCollision(const char *modelPath, float importScale, const gl3d::Transform &transform)
{
	if (!initialized_ || !physicsSystem_ || modelPath == nullptr || modelPath[0] == '\0')
	{
		return false;
	}

	gl3d::ErrorReporter errorReporter;
	gl3d::FileOpener fileOpener;
	gl3d::LoadedModelData loadedModel(modelPath, errorReporter, fileOpener, importScale);

	if (loadedModel.loader.LoadedMeshes.empty())
	{
		return false;
	}

	const glm::mat4 worldTransform = gl3d::getTransformMatrix(transform);
	const bool reverseWinding = glm::determinant(glm::mat3(worldTransform)) < 0.0f;

	TriangleList triangles;
	for (const auto &mesh : loadedModel.loader.LoadedMeshes)
	{
		const size_t triangleCount = !mesh.Indices.empty()
			? mesh.Indices.size() / 3
			: (!mesh.Vertices.empty() ? mesh.Vertices.size() / 3 : mesh.VerticesAnimations.size() / 3);
		triangles.reserve(triangles.size() + triangleCount);
		appendStaticMeshTriangles(mesh, worldTransform, reverseWinding, triangles);
	}

	if (triangles.empty())
	{
		return false;
	}

	const glm::vec3 spawnPosition = chooseSpawnPosition(triangles);

	BodyInterface &bodyInterface = physicsSystem_->GetBodyInterface();
	if (!mapBodyId_.IsInvalid())
	{
		bodyInterface.RemoveBody(mapBodyId_);
		bodyInterface.DestroyBody(mapBodyId_);
		mapBodyId_ = BodyID();
	}

	MeshShapeSettings meshSettings(std::move(triangles));
	meshSettings.mBuildQuality = MeshShapeSettings::EBuildQuality::FavorBuildSpeed;

	BodyCreationSettings mapSettings(
		new MeshShapeSettings(std::move(meshSettings)),
		RVec3::sZero(),
		Quat::sIdentity(),
		EMotionType::Static,
		cNonMovingLayer);

	mapBodyId_ = bodyInterface.CreateAndAddBody(mapSettings, EActivation::DontActivate);
	physicsSystem_->OptimizeBroadPhase();

	if (character_ && tempAllocator_)
	{
		character_->SetPosition(RVec3(spawnPosition.x, spawnPosition.y, spawnPosition.z));
		character_->SetLinearVelocity(Vec3::sZero());

		character_->RefreshContacts(
			physicsSystem_->GetDefaultBroadPhaseLayerFilter(cMovingLayer),
			physicsSystem_->GetDefaultLayerFilter(cMovingLayer),
			{},
			{},
			*tempAllocator_);
	}

	return !mapBodyId_.IsInvalid();
}

void PhysicsController::update(float deltaTime, const PhysicsControllerInput &input)
{
	if (!initialized_ || !character_ || !physicsSystem_ || !tempAllocator_ || !jobSystem_ || deltaTime <= 0.0f)
	{
		return;
	}

	CharacterVirtual *character = &*character_;
	character->UpdateGroundVelocity();

	const Vec3 up = character->GetUp();
	const Vec3 currentVelocity = character->GetLinearVelocity();
	const Vec3 currentVerticalVelocity = currentVelocity.Dot(up) * up;
	const Vec3 groundVelocity = character->GetGroundVelocity();
	const Vec3 desiredHorizontalVelocity = character->CancelVelocityTowardsSteepSlopes(
		toJolt(input.moveDirection) * (input.wantsToRun ? cRunSpeed : cWalkSpeed));

	Vec3 newVelocity = currentVerticalVelocity;
	const bool isOnGround = character->GetGroundState() == CharacterBase::EGroundState::OnGround;
	const bool movingTowardsGround = (currentVerticalVelocity.GetY() - groundVelocity.GetY()) < 0.1f;

	if (isOnGround && movingTowardsGround)
	{
		newVelocity = groundVelocity;

		if (input.wantsToJump)
		{
			newVelocity += up * cJumpSpeed;
		}
	}

	newVelocity += physicsSystem_->GetGravity() * deltaTime;
	newVelocity += desiredHorizontalVelocity;

	character->SetLinearVelocity(newVelocity);

	if (glm::length(input.moveDirection) > 0.001f)
	{
		facingYaw_ = std::atan2(input.moveDirection.x, input.moveDirection.z);
	}
	else
	{
		const Vec3 horizontalVelocity = character->GetLinearVelocity() - character->GetLinearVelocity().Dot(up) * up;
		if (horizontalVelocity.LengthSq() > 0.0001f)
		{
			facingYaw_ = std::atan2(horizontalVelocity.GetX(), horizontalVelocity.GetZ());
		}
	}

	character->SetRotation(Quat::sRotation(Vec3::sAxisY(), facingYaw_));

	CharacterVirtual::ExtendedUpdateSettings updateSettings;
	updateSettings.mStickToFloorStepDown = Vec3(0.0f, -0.5f, 0.0f);
	updateSettings.mWalkStairsStepUp = Vec3(0.0f, 0.4f, 0.0f);
	updateSettings.mWalkStairsMinStepForward = 0.02f;
	updateSettings.mWalkStairsStepForwardTest = 0.15f;

	character->ExtendedUpdate(
		deltaTime,
		-up * physicsSystem_->GetGravity().Length(),
		updateSettings,
		physicsSystem_->GetDefaultBroadPhaseLayerFilter(cMovingLayer),
		physicsSystem_->GetDefaultLayerFilter(cMovingLayer),
		{},
		{},
		*tempAllocator_);

	const int collisionSteps = std::max(1, static_cast<int>(std::ceil(deltaTime * 60.0f)));
	physicsSystem_->Update(deltaTime, collisionSteps, &*tempAllocator_, &*jobSystem_);
}

glm::vec3 PhysicsController::getPlayerPosition() const
{
	if (!character_)
	{
		return {};
	}

	return toGlm(character_->GetPosition());
}

glm::vec3 PhysicsController::getPlayerVelocity() const
{
	if (!character_)
	{
		return {};
	}

	return toGlm(character_->GetLinearVelocity());
}

float PhysicsController::getPlayerYaw() const
{
	if (!initialized_)
	{
		return 0.0f;
	}

	return facingYaw_;
}

bool PhysicsController::isGrounded() const
{
	if (!character_)
	{
		return false;
	}

	return character_->GetGroundState() == CharacterBase::EGroundState::OnGround;
}
