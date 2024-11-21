#pragma once
#include "Vector3.h"
#include <limits>
#include <memory>

class Box3 {
public:
	static std::shared_ptr<Box3> create() {
		return std::make_shared<Box3>();
	}

	Box3() {};

	Box3(const Vector3& min, const Vector3& max);

	Box3(const Box3& box3) = default;

	~Box3() = default;

	void set(const Vector3& min, const Vector3& max);

	Box3& setFromVector(const std::vector<float>& vector);

	Box3& setFromPoints(const std::vector<Vector3>& vec3);

	Box3& setFromCenterAndSize(const Vector3& center, const Vector3& size);

	void copy(const Box3& box);

	Box3& makeEmpty();

	bool isEmpty();

	Box3& expandByPoint(const Vector3& point);

	Box3& expandByVector(const Vector3& ve3);

	Box3& expandByScalar(float scalar);

	bool containsPoint(const Vector3& point);

	bool containBox(const Box3& box3);

	bool intersectsBox(const Box3& box3);

	float distanceToPoint(const Vector3& point);

	Box3& intersect(const Box3& box3);

	Box3& unionBox3(Box3 box3);

	Box3& applyMatrix4(Matrix4 matrix4);

	Box3& translate(const Vector3& offset);

	Vector3 getMin() const;
	Vector3 getMax() const;

	Vector3 getCenter(Vector3 target = Vector3());

	Vector3& clampPoint(const Vector3& point, Vector3& target);

	//bool intersectsSphere(const Sphere& sphere);


private:
	Vector3 mMin = { std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity() };
	Vector3 mMax = { -std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity() };
};