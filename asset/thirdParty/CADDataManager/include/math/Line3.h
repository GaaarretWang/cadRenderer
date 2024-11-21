#pragma once
#include "parser/serialization_generated.h"
#include "Vector3.h"
#include <algorithm>

class Line3 {
public:
	using Ptr = std::shared_ptr<Line3>;

	static Ptr create() {
		return std::make_shared<Line3>();
	}

	Line3() = default;

	Line3(Vector3 start, Vector3 end);

	~Line3() = default;

	Vector3 getStart() const;

	Vector3 getEnd() const;

	Line3& set(Vector3 start, Vector3 end);

	void copy(Line3::Ptr line3);

	Line3& copy(Line3 line3);

	Vector3 delta(Vector3 target);

	Vector3 closestPointToPoint(Vector3 point, bool clampToLine, Vector3 target);

	double closestPointToPointParameter(Vector3 point, bool clampToLine);
private:
	Vector3				mStart{ 0.0f, 0.0f, 0.0f };
	Vector3				mEnd{ 0.0f, 0.0f, 0.0f };
};