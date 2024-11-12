#pragma once
#include "global/typeDefine.h"
#include "model/geometry/geometry.h"

class GeometryMergeUtil
{
public:
	Geometry::Ptr mergeGeometries(std::vector<Geometry::Ptr> geometries);
};