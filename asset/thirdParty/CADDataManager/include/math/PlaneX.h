#pragma once
#include "parser/serialization_generated.h"
#include "Vector3.h"
#include "Line3.h"
#include "Matrix4.h"

class PlaneX
{
public:
	PlaneX() = default;

	PlaneX(Vector3 normal, float constant);

	~PlaneX() = default;

	Vector3 getNormal() const;
	
	float getConstant() const {
		return mConstant;
	};

	PlaneX& set(Vector3 normal, float constant);

	PlaneX& setFromNormalAndCoplanarPoint(Vector3 normal, Vector3 point);

	float distanceToPoint(Vector3 point);

	Vector3 projectPoint(Vector3 point, Vector3 target);
private:
	Vector3					mNormal{ 1.0f, 0.0f, 0.0f };
	float					mConstant{ 0.0f };
	bool					mIsPlaneX{ true };
};

