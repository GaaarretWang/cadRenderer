#pragma  once
#include "global/baseDef.h"
#include "serialization_generated.h"
#include "model/geometry/geometry.h"
#include "math/Vector3.h"
#include "global/typeDefine.h"

class GeometryParser
{
public:
	Geometry::Ptr parseGeometry(const FlatBufferDocSpace::DiscreteInforData* discreteObj, ElementType type);

	std::vector<float> getV(const FlatBufferDocSpace::DiscreteInforData* discreteObj);

	std::vector<int> getI(const FlatBufferDocSpace::DiscreteInforData* discreteObj);

	std::vector<int> generateFaceIndex1(int arrayLength);

	std::vector<int> generateFaceIndex2(int arrayLength);

	std::vector<float> getN(const FlatBufferDocSpace::DiscreteInforData* discreteObj, const std::vector<float>& position, const std::vector<int>& index);
};