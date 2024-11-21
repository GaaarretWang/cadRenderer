#pragma once
#include "serialization_generated.h"
#include "../model/instance/minstance.h"

class InstanceParser
{
public:
	static void parseInstance(const FlatBufferDocSpace::CompData* instanceObj);
};