#pragma once

#include "global/baseDef.h"
#include "vector3.h"
#include "math/Box3.h"
#include "math/PlaneX.h"

class Sphere {
public:
	using Ptr = std::shared_ptr<Sphere>;

	static Ptr create() {
		return std::make_shared<Sphere>();
	}

	Sphere() = default;

	Sphere(Vector3 center, float radius);

	~Sphere() = default;

	Vector3 getCenter() const { return mCenter; }

	float getRadius() const { return mRadius; }

	Sphere& set(Vector3 center, float radius);

	Sphere& setFromPoints(std::vector<Vector3>& points);

	Sphere& setFromPoints(std::vector<Vector3>& points, Vector3 optionalCenter);

	Sphere& copy(Sphere sphere);

	Sphere& copy(Sphere::Ptr sphere);

	bool isEmpty();

	Sphere& makeEmpty();

	bool containsPoint(Vector3 point);

	float distanceToPoint(Vector3 point);

	bool intersectsSphere(Sphere sphere);

	//bool intersectsBox(Box3 box);

	bool intersectsPlaneX(PlaneX planeX);

	Vector3& clampPoint(Vector3 point, Vector3 target);

	Box3& getBoundingBox(Box3 target);

	Sphere& applyMatrix4(Matrix4 matrix);

	Sphere& translate(Vector3 offset);

	Sphere& expandByPoint(Vector3 point);

	Sphere& unionSphere(Sphere sphere);

	bool equals(Sphere sphere);

	Sphere& clone();

private:
	Vector3 mCenter{ 0.0f, 0.0f, 0.0f };
	float mRadius{ -1.0f };

};