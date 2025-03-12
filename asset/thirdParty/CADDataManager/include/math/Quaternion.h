#pragma once
#include "Vector3.h"
#include <string>
#include <vector>
#include <functional>
namespace cadDataManager {
	class Quaternion {
	public:
		Quaternion() = default;

		Quaternion(float x, float y, float z, float w);

		~Quaternion() = default;

		float getX() const;

		Quaternion& setX(float value);

		float getY() const;

		Quaternion& setY(float value);

		float getZ() const;

		Quaternion& setZ(float value);

		float getW() const;

		Quaternion& setW(float value);

		Quaternion& set(float x, float y, float z, float w);

		Quaternion& copy(Quaternion quaternion);

		Quaternion& identity();

		Quaternion& invert();

		Quaternion& conjugate();

		float dot(Quaternion v);

		float lengthSq();

		float length();

		Quaternion& normalize();

		Quaternion& multiply(Quaternion q);

		Quaternion& multiply(Quaternion q, Quaternion p);

		Quaternion& premultiply(Quaternion q);

		Quaternion& multiplyQuaternions(Quaternion a, Quaternion b);

		Quaternion& slerp(Quaternion qb, float t);

		Quaternion& slerpQuaternions(Quaternion qa, Quaternion qb, float t);

		bool equals(Quaternion quaternion);

		Quaternion& fromArray(std::vector<float> arr, long offset = 0);

		std::vector<float> toArray(std::vector<float> arr, long offset = 0);

		std::string toString();

	private:
		float mX{ 0.0f };
		float mY{ 0.0f };
		float mZ{ 0.0f };
		float mW{ 0.0f };
		std::function<void()> mChangeCallback{ nullptr };
	};
}