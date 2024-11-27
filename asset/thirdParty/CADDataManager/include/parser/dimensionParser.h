#pragma once
#include "serialization_generated.h"
#include "model/dimension/modelBasedDefinition.h"
#include "global/baseDef.h"

class DimensionParser
{
public:
	static ModelBasedDefinition::Ptr parseDimension(const FlatBufferDocSpace::ElementData* elementObj);
};