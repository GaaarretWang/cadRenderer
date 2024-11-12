#pragma once 
#include <cmath>
#include "Matrix4.h"


struct Vector3 {
public:
	Vector3() {};

	Vector3(float x, float y, float z) : mX(x), mY(y), mZ(z) {};

	~Vector3() = default;

	float getX() const {
		return mX;
	};

	float getY() const {
		return mY;
	};

	float getZ() const {
		return mZ;
	};

	Vector3& copy(const Vector3& v);

	Vector3 clone();

	Vector3& addScaledVector(Vector3 v, float s);

	Vector3& subVectors(Vector3 a, Vector3 b) {
		mX = a.getX() - b.getX();
		mY = a.getY() - b.getY();
		mZ = a.getZ() - b.getZ();

		return *this;
	};

	Vector3& crossVectors(Vector3 a, Vector3 b) {
		float ax = a.getX(), ay = a.getY(), az = a.getZ();
		float bx = b.getX(), by = b.getY(), bz = b.getZ();

		mX = ay * bz - az * by;
		mY = az * bx - ax * bz;
		mZ = ax * by - ay * bx;

		return *this;
	};

	Vector3& cross(Vector3 v) {
		return this->crossVectors(*this, v);
	}
	;

	Vector3& divideScalar(float scalar) {
		return this->multiplyScalar(1 / scalar);
	};

	Vector3& multiplyScalar(float scalar) {
		mX *= scalar;
		mY *= scalar;
		mZ *= scalar;

		return *this;
	}

	float length() {
		return std::sqrt(mX * mX + mY * mY + mZ * mZ);
	};

	Vector3& normalize() {
		if (this->length()) {
			return this->divideScalar(this->length());
		}
		else {
			return this->divideScalar(1);
		}
	};

	Vector3& set(float x, float y, float z) {

		mX = x;
		mY = y;
		mZ = z;

		return *this;
	};
	Vector3& sub(Vector3 v);
	Vector3& add(Vector3 v);
	Vector3& minn(Vector3 v);
	Vector3& maxx(Vector3 v);
	Vector3& addScalar(float s);
	Vector3& clamp(Vector3 min, Vector3 max);
	Vector3& applyMatrix4(Matrix4& m);
	float dot(Vector3 v);
	float distanceTo(Vector3& v);
	float distanceToSquared(Vector3 v);
	Vector3& setFromMatrixColumn(Matrix4 m, float index);
	Vector3& addVectors(Vector3 a, Vector3 b);
	Vector3& setLength(float l);
	bool equals(Vector3 v);
	double lengthSq();
	template<typename T>
	Vector3& fromArray(const T* arr, int offset = 0);
	std::vector<float> buildArray();
	
public:
	union {
		float mXYZ[3] = { 0.0f };
		struct { float mX, mY, mZ; };
	};
};

template<typename T>
Vector3& Vector3::fromArray(const T* arr, int offset) {

	mX = static_cast<float>(arr[offset]);
	mY = static_cast<float>(arr[offset + 1]);
	mZ = static_cast<float>(arr[offset + 2]);

	return *this;
}
