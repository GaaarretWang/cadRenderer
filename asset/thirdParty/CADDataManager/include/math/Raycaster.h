#pragma once
#include "global/baseDef.h"
#include "math/Ray.h"
#include "math/Layers.h"
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
		IntersectionFace::Ptr mFace;
	};

	class Raycaster : public std::enable_shared_from_this<Raycaster> {
	public:
		using Ptr = std::shared_ptr<Raycaster>;
		static Ptr create() { return std::make_shared<Raycaster>(); }

		Raycaster() = default;

		Raycaster(Vector3 origin, Vector3 direction, float near = 0.f, float far = MAX1);

		Raycaster(const Ray::Ptr r);

		~Raycaster() = default;

		Ray::Ptr getRay() const;

		Vector3& getOrigin() const;

		Vector3& getDirection() const;

		float getNear() const;

		float getFar() const;

		Layers& getLayers() { return mLayers; };

		volatile ThresholdParams& getThresholdParams() { return mThresholdParams; };

		Raycaster& set(Vector3 origin, Vector3 direction);

		std::vector<Intersection::Ptr> intersectObject();

	private:
		Ray::Ptr mRay = Ray::create();
		float mNear = 0;
		float mFar = std::numeric_limits<float>::max();
		Layers mLayers;

		volatile ThresholdParams mThresholdParams;
	};
}
