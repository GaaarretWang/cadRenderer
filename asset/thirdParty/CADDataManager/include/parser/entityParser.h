#pragma once
#include "serialization_generated.h"
#include "../manager/appearanceDataManager.h"
#include "../model/entity/entity.h"
#include "../global/baseDef.h"

class EntityParser
{
public:
	static Entity::Ptr parseEntity(const FlatBufferDocSpace::EntityData* entityObj);
	
};