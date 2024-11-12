#pragma once
#include "parser/serialization_generated.h"
#include "Vector3.h"
#include "Line3.h"
#include "Matrix4.h"

class Plane
{
public:
	Plane() = default;

	Plane(Vector3 normal, float constant);

	~Plane() = default;

	Vector3 getNormal() const;
	
	float getConstant() const {
		return mConstant;
	};

	Plane& set(Vector3 normal, float constant);

	Plane& setFromNormalAndCoplanarPoint(Vector3 normal, Vector3 point);

	float distanceToPoint(Vector3 point);

	Vector3 projectPoint(Vector3 point, Vector3 target);
private:
	Vector3					mNormal{ 1.0f, 0.0f, 0.0f };
	float					mConstant{ 0.0f };
	bool					mIsPlane{ true };
};

