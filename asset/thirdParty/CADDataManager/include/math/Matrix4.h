#pragma once
#include <vector>

struct Matrix4 {
public:
	Matrix4() = default;

	Matrix4(const Matrix4& matrix4);

	~Matrix4() = default;

	Matrix4& operator = (const Matrix4& matrix4);

	Matrix4& set(float n11, float n12, float n13, float n14,
		float n21, float n22, float n23, float n24,
		float n31, float n32, float n33, float n34,
		float n41, float n42, float n43, float n44);

	Matrix4& identity();

	Matrix4& copy(const Matrix4& matrix4);

	Matrix4& copyPosition(const Matrix4& matrix4);

	Matrix4& multiply(const Matrix4& matrix4);

	Matrix4& premultiply(const Matrix4& matrix4); //左乘

	Matrix4& multiplyMatrices(const Matrix4& matrix4a, const Matrix4& matrix4b);

	Matrix4& multiplyScalar(float s);

	float determinant();

	Matrix4& transpose();

	Matrix4& setPosition(float x, float y, float z);

	Matrix4& invert();

	float getMaxScaleOnAxis();

	Matrix4& makeTranslation(float x, float y, float z);

	Matrix4& makeRotationX(float theta);

	Matrix4& makeRotationY(float theta);

	Matrix4& makeRotationZ(float theta);

	Matrix4& makeScale(float x, float y, float z);

	Matrix4& makeShear(float xy, float xz, float yx, float yz, float zx, float zy);

	Matrix4& makePerspective(float left, float right, float top, float bottom, float zNear, float zFar);

	Matrix4& makeOrthographic(float left, float right, float top, float bottom, float zNear, float zFar);

	bool equals(const Matrix4& matrix4);

	float* getElements() const;

	std::vector<float> toVector(std::vector<float>& vector, int offset);

	std::vector<float> toVector();

protected:			 
	float mElements[16] = {
				1.f, 0.f, 0.f, 0.f,
				0.f, 1.f, 0.f, 0.f,
				0.f, 0.f, 1.f, 0.f,
				0.f, 0.f, 0.f, 1.f
	};
};
