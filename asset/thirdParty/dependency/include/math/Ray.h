#pragma once
#include "Vector3.h"
#include "global/baseDef.h"
#include "Plane.h"
#include "Box3.h"

class Ray {
public:
	using Ptr = std::shared_ptr<Ray>;
	static Ptr create() { return std::make_shared<Ray>(); }

	Ray() = default;

	Ray(Vector3 origin, Vector3 direction);

	Ray(const Ray::Ptr r);

	~Ray() = default;

	Vector3& getOrigin();

	Vector3& getDirection();

	Ray& set(Vector3 origin, Vector3 direction);

	Ray& copy(Ray::Ptr ray);

	Vector3& at(float t, Vector3 target);

	Ray& lookAt(Vector3 v);

	Ray& recast(float t);

	Vector3& closestPointToPoint(Vector3 point, Vector3 target);

	float distanceToPoint(Vector3 point);

	float distanceSqToPoint(Vector3 point);

	float distanceSqToSegment(Vector3 v0, Vector3 v1);

	float distanceSqToSegment(Vector3 v0, Vector3 v1, Vector3 optionalPointOnRay);

	float distanceSqToSegment(Vector3 v0, Vector3 v1, Vector3 optionalPointOnRay, Vector3 optionalPointOnSegment);

	float distanceToPlane(Plane plane);

	Vector3& intersectPlane(Plane plane, Vector3 target);

	bool intersectsPlane(Plane plane);

	Vector3& intersectBox(Box3 box, Vector3 target);

	bool intersectsBox(Box3 box);

	Ray& applyMatrix(Matrix4 matrix4);

	bool equals(Ray::Ptr ray);

	Ray& clone();



private:
	Vector3 mOrigin{ 0.0f, 0.0f, 0.0f };
	Vector3 mDirection{ 0.0f, 0.0f, -1.0f };
};