#pragma once
#include "global/baseDef.h"
#include "math/Ray.h"
#include "math/Layers.h"
#include "manager/assemblyManager.h"

namespace cadDataManager {
	struct ThresholdParams {
		float Mesh_threshold = 0.f;
		float Line_threshold = 1.f;
		float LOD_threshold = 0.f;
		float Points_threshold = 1.f;
		float Sprite_threshold = 0.f;
	};

	struct IntersectionFace {
		using Ptr = std::shared_ptr<IntersectionFace>;
		static Ptr create() { return std::make_shared<IntersectionFace>(); }

		float mA{ 0.0f };
		float mB{ 0.0f };
		float mC{ 0.0f };
		Vector3 mNormal;
	};

	struct Intersection {
		using Ptr = std::shared_ptr<Intersection>;
		static Ptr create() { return std::make_shared<Intersection>(); }

		float mDistance;
		Vector3 mPoint;
		int mIndex;
		int mFaceIndex;
		InstanceInfo::Ptr mInstanceInfo { nullptr };
		ElementInfo::Ptr mElementInfo { nullptr };
		Instance::Ptr mInstance { nullptr };
		IntersectionFace::Ptr mFace { nullptr };
	};

	class Raycaster : public std::enable_shared_from_this<Raycaster> {
	public:
		using Ptr = std::shared_ptr<Raycaster>;
		static Ptr create() { return std::make_shared<Raycaster>(); }

		Raycaster() = default;

		Raycaster(Vector3 origin, Vector3 direction, float near = 0.f, float far = MAX1);

		Raycaster(const Ray::Ptr r);

		~Raycaster() = default;

		void setRay(Ray::Ptr ray) { mRay = ray; };

		Ray::Ptr getRay() const;

		Vector3& getOrigin() const;

		Vector3& getDirection() const;

		float getNear() const;

		float getFar() const;

		Layers& getLayers() { return mLayers; };

		volatile ThresholdParams& getThresholdParams() { return mThresholdParams; };

		Raycaster& set(Vector3 origin, Vector3 direction);

		Intersection::Ptr pickMesh(bool needPickElement = false);

		Intersection::Ptr checkBufferGeometryIntersection(std::vector<float> position, int a, int b, int c, InstanceInfo::Ptr instanceInfo, ElementInfo::Ptr elementInfo);

		Intersection::Ptr checkIntersection(Vector3 pA, Vector3 pB, Vector3 pC, Vector3 point, InstanceInfo::Ptr instanceInfo, ElementInfo::Ptr elementInfo);

	public:
		Ray::Ptr mRay = Ray::create();
		float mNear = 0;
		//float mFar = std::numeric_limits<float>::max();
		float mFar = 10e9;
		Layers mLayers;
		volatile ThresholdParams mThresholdParams;
	};
}