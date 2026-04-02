#pragma once 

#include "global/baseDef.h"
#include "math.h"
#include "math/Vector3.h"
#include "math/Plane.h"
#include "math/Box3.h"

namespace cadDataManager {

	class Triangle {

	public:

		Triangle() = default;

		Triangle(Vector3 a, Vector3 b, Vector3 c);

		~Triangle() = default;

		Vector3 getA() const;

		Vector3 getB() const;

		Vector3 getC() const;

		Vector3& getNormal(Vector3& a, Vector3& b, Vector3& c, Vector3& target);

		Vector3& getBarycoord(Vector3 point, Vector3 a, Vector3 b, Vector3 c, Vector3 target);

		bool containsPoint(Vector3 point, Vector3 a, Vector3 b, Vector3 c);


		bool isFrontFacing(Vector3 a, Vector3 b, Vector3 c, Vector3  direction);

		Triangle& set(Vector3 a, Vector3 b, Vector3 c);

		Triangle& setFromPointsAndIndices(Vector3 points[], int i0, int i1, int i2);

		Triangle& copy(Triangle triangle);

		float getArea();

		Vector3& getMidpoint(Vector3 target);

		Vector3& getNormal(Vector3 target);

		Vector3& getBarycoord(Vector3 point, Vector3 target);

		bool containsPoint(Vector3 point);

		bool isFrontFacing(Vector3 direction);

		Vector3& closestPointToPoint(Vector3 p, Vector3 target);

		bool equalTriangle(Triangle triangle);


	private:

		Vector3 mA{ 0.f, 0.f, 0.f };
		Vector3 mB{ 0.f, 0.f, 0.f };
		Vector3 mC{ 0.f, 0.f, 0.f };

	};
}